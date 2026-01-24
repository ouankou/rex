// A hand-crafted OpenMP parser for Fortran comments within a SgSourceFile
// It only exposes a few interface functions:
//   omp_fortran_parse()
// void parse_fortran_openmp(SgSourceFile *sageFilePtr) // preferred
//
// All other supporting functions should be declared with "static" (file scope
// only) Liao, 5/24/2009

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sageBuilder.h"

#include "sage3basic.h"

#include "OpenMPIR.h"

#include <cstdio>

#include <cstdlib>

#include <cstring>

#include <string>

using namespace std;
using namespace OmpSupport;

extern std::vector<
    std::tuple<SgLocatedNode *, PreprocessingInfo *, OpenMPDirective *>>
    fortran_omp_pragma_list;
extern OpenMPDirective *ompparser_OpenMPIR;
void parseOpenMPFortran(SgSourceFile *);
bool isFortranPairedDirective(OpenMPDirective *);

// A file scope char* to avoid passing and returning target c string for every
// and each function
static const char *c_char = NULL; // current characters being scanned
static SgNode *c_sgnode =
    NULL; // current SgNode associated with the OpenMP directive

//--------------omp fortran scanner (ofs) functions------------------
//
// note: ofs_skip_xxx() optional skip 0 or more patterns
//       ofs_match_xxx() try to match a pattern, undo side effect if failed.

//! Skip 0 or more whitespace or tabs
static bool ofs_skip_whitespace() {
  bool result = false;
  while ((*c_char) == ' ' || (*c_char) == '\t') {
    c_char++;
    result = true;
  }
  return result;
}

//! Match a given sub c string from the input c string, again skip heading
//! space/tabs if any
//  checkTrail: Check the immediate following character after the match, it must
//  be one of
//      whitespace, end of str, newline, tab, or '!'
//      Set to true by default, used to ensure the matched substr is a full
//      identifier/keywords.
//
//      But Fortran OpenMP allows blanks/tabs to be ignored between certain pair
//      of keywords: e.g: end critical == endcritical  , parallel do ==
//      paralleldo to match the 'end' and 'parallel', we have to skip trail
//      checking.
// return values:
//    true: find a match, the current char is pointed to the next char after the
//    substr false: no match, the current char is intact

static bool ofs_match_substr(const char *substr, bool checkTrail = true) {
  bool result = true;
  const char *old_char = c_char;
  // we skip leading space from the target string
  ofs_skip_whitespace();
  size_t len = strlen(substr);
  for (size_t i = 0; i < len; i++) {
    if (std::tolower(static_cast<unsigned char>(*c_char)) ==
        std::tolower(static_cast<unsigned char>(substr[i]))) {
      c_char++;
    } else {
      result = false;
      c_char = old_char;
      break;
    }
  }
  // handle the next char after the substr match:
  // could only be either space or \n, \0, \t, !comments
  // or the match is revoked, e.g: "parallel1" match sub str "parallel" but
  // the trail is not legal
  // TODO: any other characters?
  if (checkTrail) {
    if (*c_char != ' ' && *c_char != '\0' && *c_char != '\n' &&
        *c_char != '\t' && *c_char != '!') {
      result = false;
      c_char = old_char;
    }
  }
  return result;
}

//! Check if the current Fortran SgFile has fixed source form
static bool isFixedSourceForm() {
  bool result = false;
  SgFile *file = SageInterface::getEnclosingFileNode(c_sgnode);
  ROSE_ASSERT(file != NULL);

  // Only make sense for Fortran files
  ROSE_ASSERT(file->get_Fortran_only());
  if (file->get_inputFormat() ==
      SgFile::e_unknown_output_format) { // default case: only f77 has fixed
                                         // form
    if (file->get_F77_only())
      result = true;
    else
      result = false;
  } else // explicit case: any Fortran could be set to fixed form
  {
    if (file->get_inputFormat() == SgFile::e_fixed_form_output_format)
      result = true;
    else
      result = false;
  }
  return result;
}

// Check if the current c string starts with one of the legal OpenMP directive
// sentinels.
//  Two cases:
//     fixed source form: !$omp | c$omp | *$omp , then whitespace, continuation
//     apply to the rest free source form: !$omp only
static bool ofs_match_omp_sentinel(char marker) {
  const char *old_char = c_char;
  ofs_skip_whitespace();
  if (std::tolower(static_cast<unsigned char>(*c_char)) !=
      std::tolower(static_cast<unsigned char>(marker))) {
    c_char = old_char;
    return false;
  }
  ++c_char;
  ofs_skip_whitespace();
  bool matched = ofs_match_substr("$omp");
  if (!matched) {
    c_char = old_char;
  }
  return matched;
}

static bool ofs_is_omp_sentinels() {
  bool result = false;
  // two additional case for fixed form
  if (isFixedSourceForm()) {
    if (ofs_match_omp_sentinel('c') || ofs_match_omp_sentinel('*') ||
        ofs_match_omp_sentinel('d'))
      result = true;
  }
  // a common case for all situations
  if (ofs_match_omp_sentinel('!'))
    result = true;
  return result;
}

static void normalizeFortranOmpSentinel(std::string &buffer) {
  size_t pos = buffer.find_first_not_of(" \t");
  if (pos == std::string::npos) {
    return;
  }
  const char marker = buffer[pos];
  const char marker_lower =
      static_cast<char>(std::tolower(static_cast<unsigned char>(marker)));
  if (marker_lower != '!' && marker_lower != 'c' && marker_lower != '*' &&
      marker_lower != 'd') {
    return;
  }
  size_t next = pos + 1;
  while (next < buffer.size() &&
         (buffer[next] == ' ' || buffer[next] == '\t')) {
    ++next;
  }
  if (next < buffer.size() && buffer[next] == '$') {
    buffer.erase(pos + 1, next - (pos + 1));
  }
}

static size_t find_case_insensitive(const std::string &haystack,
                                    const std::string &needle, size_t pos) {
  if (needle.empty()) {
    return pos <= haystack.size() ? pos : std::string::npos;
  }
  for (size_t i = pos; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      const char a = static_cast<char>(
          std::tolower(static_cast<unsigned char>(haystack[i + j])));
      const char b = static_cast<char>(
          std::tolower(static_cast<unsigned char>(needle[j])));
      if (a != b) {
        match = false;
        break;
      }
    }
    if (match) {
      return i;
    }
  }
  return std::string::npos;
}

static size_t rfind_case_insensitive(const std::string &haystack,
                                     const std::string &needle, size_t pos) {
  if (needle.empty()) {
    return pos <= haystack.size() ? pos : std::string::npos;
  }
  if (pos == std::string::npos || pos > haystack.size()) {
    pos = haystack.size();
  }
  if (needle.size() > haystack.size()) {
    return std::string::npos;
  }
  size_t start = (pos >= needle.size()) ? (pos - needle.size()) : 0;
  for (size_t i = start + 1; i-- > 0;) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      const char a = static_cast<char>(
          std::tolower(static_cast<unsigned char>(haystack[i + j])));
      const char b = static_cast<char>(
          std::tolower(static_cast<unsigned char>(needle[j])));
      if (a != b) {
        match = false;
        break;
      }
    }
    if (match) {
      return i;
    }
    if (i == 0) {
      break;
    }
  }
  return std::string::npos;
}

//! A helper function to remove Fortran '!comments', but not '!$omp ...' from a
//! string
// handle complex cases like:
// ... !... !$omp .. !...
static void removeFortranComments(string &buffer) {
  size_t pos1;
  size_t pos2;
  size_t pos3 = string::npos;

  pos1 = buffer.rfind("!", pos3);
  while (pos1 != string::npos) {
    pos2 = rfind_case_insensitive(buffer, "!$omp", pos3);
    if (pos1 != pos2) // is a real comment if not !$omp
    {
      buffer.erase(pos1);
    } else // find "!$omp", cannot stop here since there might have another '!'
           // before it
           // limit the search range
    {
      if (pos2 >= 1)
        pos3 = pos2 - 1;
      else
        break;
    }
    pos1 = buffer.rfind("!", pos3);
  }
}

//!  A helper function to tell if a line has an ending '&', followed by optional
//!  space , tab , '\n',
// "!comments" should be already removed  (call removeFortranComments()) before
// calling this function
static bool hasFortranLineContinuation(const string &buffer) {
  // search backwards for '&'
  size_t pos = buffer.rfind("&");
  if (pos == string::npos)
    return false;
  else {
    // make sure the characters after & is legal
    for (size_t i = ++pos; i < buffer.length(); i++) {
      char c = buffer[i];
      if ((c != ' ') && (c != '\t')) {
        return false;
      }
    }
  }
  return true;
}

//! Assume two Fortran OpenMP comment lines are just merged, remove" & !$omp
//! [&]" within them
// Be careful about the confusing case
//   the 2nd & and  the end & as another continuation character
static void postProcessingMergedContinuedLine(std::string &buffer) {
  size_t first_pos, second_pos, last_pos, next_cont_pos;
  removeFortranComments(buffer);
  // locate the first &
  first_pos = buffer.find("&");
  assert(first_pos != string::npos);

  // locate the !$omp, must have it for OpenMP directive
  second_pos = find_case_insensitive(buffer, "$omp", first_pos);
  assert(second_pos != string::npos);
  second_pos += 3; // shift to the end 'p' of "$omp"
  // search for the optional next '&'
  last_pos = buffer.find("&", second_pos);

  // locate the possible real cont &
  // If it is also the found optional next '&'
  // discard it as the next '&'
  if (hasFortranLineContinuation(buffer)) {
    next_cont_pos = buffer.rfind("&");
    if (last_pos == next_cont_pos)
      last_pos = string::npos;
  }

  if (last_pos == string::npos)
    last_pos = second_pos;
  // we can now remove from first to last pos from the buffer
  buffer.erase(first_pos, last_pos - first_pos + 1);
}

//-------------- the implementation for the external interface
//-------------------
//! Check if a line is an OpenMP directive,
// the associated node is needed to tell the programming language
bool ofs_is_omp_sentinels(const char *str, SgNode *node) {
  bool result;
  assert(node && str);
  // make sure it is side effect free
  const char *old_char = c_char;
  SgNode *old_node = c_sgnode;

  c_char = str;
  c_sgnode = node;
  result = ofs_is_omp_sentinels();

  c_char = old_char;
  c_sgnode = old_node;

  return result;
}

//! Merge clauses from end directives to the corresponding begin directives
// dowait clause:  end do, end sections, end single, end workshare
// copyprivate clause: end single
void mergeEndClausesToBeginDirective(OpenMPDirective *begin_decl,
                                     OpenMPDirective *end_decl,
                                     OpenMPDirective *end_wrapper) {
  ROSE_ASSERT(begin_decl != NULL);
  ROSE_ASSERT(end_decl != NULL);

  // Make sure they match
  OpenMPDirectiveKind begin_type = begin_decl->getKind();
  OpenMPDirectiveKind end_type = end_decl->getKind();
  ROSE_ASSERT(begin_type == end_type);

  map<OpenMPClauseKind, std::vector<OpenMPClause *> *> *begin_all_clauses =
      begin_decl->getAllClauses();
  auto has_clause = [](OpenMPDirective *directive,
                       OpenMPClauseKind kind) -> bool {
    if (directive == nullptr) {
      return false;
    }
    std::vector<OpenMPClause *> *clauses = directive->getClauses(kind);
    return clauses != nullptr && !clauses->empty();
  };
  auto first_clause = [](OpenMPDirective *directive,
                         OpenMPClauseKind kind) -> OpenMPClause * {
    if (directive == nullptr) {
      return nullptr;
    }
    std::vector<OpenMPClause *> *clauses = directive->getClauses(kind);
    if (clauses == nullptr || clauses->empty()) {
      return nullptr;
    }
    return (*clauses)[0];
  };

  // merge end directive's clause to the begin directive.
  // Merge possible nowait clause
  switch (end_type) {
  case OMPD_do:
  case OMPD_sections:
  case OMPD_single:
  case OMPD_workshare: {
    if (begin_all_clauses->find(OMPC_nowait) != begin_all_clauses->end()) {
      break;
    }
    if (has_clause(end_decl, OMPC_nowait) ||
        has_clause(end_wrapper, OMPC_nowait)) {
      begin_decl->addOpenMPClause(OMPC_nowait);
    }
    break;
  }
  default:
    break; // there should be no clause for other cases
  }
  // Merge possible copyrpivate (list) from end single
  if (end_type == OMPD_single) {
    OpenMPClause *end_copyprivate_clause =
        first_clause(end_decl, OMPC_copyprivate);
    if (end_copyprivate_clause == nullptr) {
      end_copyprivate_clause = first_clause(end_wrapper, OMPC_copyprivate);
    }
    if (end_copyprivate_clause != nullptr) {
      auto iter = begin_all_clauses->find(OMPC_copyprivate);
      OpenMPClause *begin_copyprivate_clause = NULL;
      if (iter != begin_all_clauses->end()) {
        begin_copyprivate_clause =
            (*(begin_decl->getClauses(OMPC_copyprivate)))[0];
      } else {
        begin_copyprivate_clause =
            begin_decl->addOpenMPClause(OMPC_copyprivate);
      }
      std::vector<const char *> *expressions =
          end_copyprivate_clause->getExpressions();
      for (auto variable_expression : *expressions) {
        begin_copyprivate_clause->addLangExpr(variable_expression);
      }
    }
  }
}

bool isFortranPairedDirective(OpenMPDirective *node) {
  bool result = false;
  switch (node->getKind()) {
  case OMPD_barrier:
  case OMPD_cancel:
  case OMPD_cancellation_point:
  case OMPD_end:
  case OMPD_flush:
  case OMPD_section:
  case OMPD_taskwait:
  case OMPD_taskyield:
  case OMPD_threadprivate: {
    break;
  }
  default: {
    result = true;
  }
  }
  return result;
}

static bool allowsImplicitFortranEnd(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel:
  case OMPD_do:
  case OMPD_parallel_do:
  case OMPD_parallel_loop:
    return true;
  default:
    return false;
  }
}

void parseOpenMPFortran(SgSourceFile *sageFilePtr) {
  struct FortranCommentEntry {
    SgLocatedNode *loc_node;
    PreprocessingInfo *info;
    int file_id;
    int line;
    int column;
    size_t order;
  };

  std::vector<OpenMPDirective *> ompparser_OpenMP_pairing_list;
  std::vector<FortranCommentEntry> comment_entries;
  size_t order = 0;

  std::vector<SgNode *> loc_nodes =
      NodeQuery::querySubTree(sageFilePtr, V_SgLocatedNode);
  for (SgNode *node : loc_nodes) {
    SgLocatedNode *locNode = isSgLocatedNode(node);
    ROSE_ASSERT(locNode);
    AttachedPreprocessingInfoType *comments =
        locNode->getAttachedPreprocessingInfo();
    if (!comments) {
      continue;
    }
    for (PreprocessingInfo *pinfo : *comments) {
      if (pinfo->getTypeOfDirective() ==
              PreprocessingInfo::FortranStyleComment ||
          pinfo->getTypeOfDirective() == PreprocessingInfo::F90StyleComment) {
        comment_entries.push_back({locNode, pinfo, pinfo->getFileId(),
                                   pinfo->getLineNumber(),
                                   pinfo->getColumnNumber(), order++});
      }
    }
  }

  std::stable_sort(
      comment_entries.begin(), comment_entries.end(),
      [](const FortranCommentEntry &lhs, const FortranCommentEntry &rhs) {
        if (lhs.file_id != rhs.file_id) {
          return lhs.file_id < rhs.file_id;
        }
        if (lhs.line != rhs.line) {
          return lhs.line < rhs.line;
        }
        if (lhs.column != rhs.column) {
          return lhs.column < rhs.column;
        }
        return lhs.order < rhs.order;
      });

  PreprocessingInfo *previnfo = nullptr;
  SgLocatedNode *prev_loc_node = nullptr;
  for (const FortranCommentEntry &entry : comment_entries) {
    SgLocatedNode *locNode = entry.loc_node;
    PreprocessingInfo *pinfo = entry.info;

    if (previnfo != nullptr && prev_loc_node != locNode) {
      previnfo = nullptr;
      prev_loc_node = nullptr;
    }

    string buffer = pinfo->getString();
    // We are not interested in other comments
    if (!ofs_is_omp_sentinels(buffer.c_str(), locNode)) {
      if (previnfo != nullptr && prev_loc_node == locNode) {
        printf("error: Found a none-OpenMP comment after a pending OpenMP "
               "comment with a line continuation\n");
        ROSE_ABORT();
      }
      continue;
    }

    normalizeFortranOmpSentinel(buffer);
    // remove possible comments also:
    removeFortranComments(buffer);
    // merge with possible previous line with &
    if (previnfo != nullptr && prev_loc_node == locNode) {
      //            cout<<"previous line:"<<previnfo->getString()<<endl;
      buffer = previnfo->getString() + buffer;
      // remove "& !omp [&]" within the merged line
      postProcessingMergedContinuedLine(buffer);
      //            cout<<"merged line:"<<buffer<<endl;
      previnfo->setString(""); // erase previous line with & at the end
      previnfo = nullptr;
      prev_loc_node = nullptr;
    }

    pinfo->setString(buffer); // save the changed buffer back

    if (hasFortranLineContinuation(buffer)) {
      // delay the handling of the current line to the next line
      previnfo = pinfo;
      prev_loc_node = locNode;
      continue;
    }

    // use ompparser to process Fortran
    ompparser_OpenMPIR = parseOpenMP(pinfo->getString().c_str(), NULL);
    ompparser_OpenMPIR->setLine(pinfo->getLineNumber());

    // set paired directives
    if (isFortranPairedDirective(ompparser_OpenMPIR)) {
      ompparser_OpenMP_pairing_list.push_back(ompparser_OpenMPIR);
    }
    if (ompparser_OpenMPIR->getKind() == OMPD_end) {
      OpenMPDirective *end_directive =
          ((OpenMPEndDirective *)ompparser_OpenMPIR)->getPairedDirective();
      bool matched = false;
      while (!ompparser_OpenMP_pairing_list.empty()) {
        OpenMPDirective *begin_directive = ompparser_OpenMP_pairing_list.back();
        if (end_directive->getKind() == begin_directive->getKind()) {
          mergeEndClausesToBeginDirective(begin_directive, end_directive,
                                          ompparser_OpenMPIR);
          ((OpenMPEndDirective *)ompparser_OpenMPIR)
              ->setPairedDirective(begin_directive);
          ompparser_OpenMP_pairing_list.pop_back();
          matched = true;
          break;
        }
        if (!allowsImplicitFortranEnd(begin_directive->getKind())) {
          ROSE_ASSERT(end_directive->getKind() == begin_directive->getKind());
        }
        ompparser_OpenMP_pairing_list.pop_back();
      }
      if (!matched) {
        cerr << "error: unmatched OpenMP end directive\n";
        ROSE_ABORT();
      }
    }
    fortran_omp_pragma_list.push_back(
        std::make_tuple(locNode, pinfo, ompparser_OpenMPIR));
  }
}
