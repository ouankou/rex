#include "sage3basic.h"

#include "rose_attributes_list.h"

#include <cctype>
#include <cstring>
#include <errno.h>
#include <limits>

#include "rose_config.h"

using namespace std;
using namespace Rose;

PreprocessingInfo::PreprocessingInfo()
    : outputPlacement(source_position), attachedOwner(nullptr) {
  // Set these values so that they are not set to zero (a valid value) if a
  // PreprocessingInfo object is reused

  file_info = NULL;

  numberOfLines = -1;
  whatSortOfDirective = CpreprocessorUnknownDeclaration;
  relativePosition = before;

  p_isTransformation = false;
}

// Typical constructor used by lex-based code retrieve comments and preprocessor
// control directives
PreprocessingInfo::PreprocessingInfo(DirectiveType dt,
                                     const string &inputString,
                                     const string &inputFileName, int line_no,
                                     int col_no, int nol,
                                     RelativePositionType relPos)
    : PreprocessingInfo(
          dt, inputString,
          std::make_unique<Sg_File_Info>(inputFileName, line_no, col_no), nol,
          relPos) {}

PreprocessingInfo::PreprocessingInfo(DirectiveType dt,
                                     const string &inputString,
                                     std::unique_ptr<Sg_File_Info> position,
                                     int nol, RelativePositionType relPos)
    : file_info(position.get()), numberOfLines(nol), whatSortOfDirective(dt),
      relativePosition(relPos), outputPlacement(source_position),
      attachedOwner(nullptr) {
  ROSE_ASSERT(file_info != nullptr);

  // DQ (12/23/2006): Mark this as a comment or directive (mostly so that we can
  // know that the parent being NULL is not meaningful in the AST consistancy
  // tests).
  file_info->setCommentOrDirective();
  file_info->setOutputInCodeGeneration();

  // The record owns its file info while detached. Attachment publishes the
  // owning located node as its AST parent; no surrogate type node is needed.

  p_isTransformation = false;

  internalString = inputString;

  // DQ (1/13/2014): Added checking for logic to compute macro name for #define
  // macros.
  if (whatSortOfDirective ==
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    (void)getMacroName();
  }
  position.release();
}

PreprocessingInfo::PreprocessingInfo(const PreprocessingInfo &prepInfo)
    : outputPlacement(prepInfo.outputPlacement), attachedOwner(nullptr) {
  ROSE_ASSERT(prepInfo.file_info != NULL);
  auto position = std::make_unique<Sg_File_Info>(*prepInfo.file_info);
  file_info = position.get();
  file_info->set_parent(nullptr);

  file_info->setCommentOrDirective();

  numberOfLines = prepInfo.getNumberOfLines();
  whatSortOfDirective = prepInfo.getTypeOfDirective();
  relativePosition = prepInfo.getRelativePosition();
  internalString = prepInfo.internalString;

  p_isTransformation = prepInfo.p_isTransformation;

  if (whatSortOfDirective ==
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    (void)getMacroName();
  }
  position.release();
}

PreprocessingInfo::~PreprocessingInfo() { delete file_info; }

// Source columns are 1-based.
int PreprocessingInfo::getColumnNumberOfEndOfString() const {
  int col = 1;
  int i = 0;

  // DQ (10/27/2006): the last line has a '\n' so we need the length
  // of the last line before the '\n" triggers the counter to be reset!
  // This fix is required because the strings we have include the final '\n"
  int previousLineLength = col;
  while (internalString[i] != '\0') {
    if (internalString[i] == '\n') {
      previousLineLength = col;
      col = 0;
    } else {
      col++;
      previousLineLength = col;
    }
    i++;
  }

  int endingColumnNumber = previousLineLength;

  // If this is a one line comment then the ending position is the length of the
  // comment PLUS the starting column position
  if (getNumberOfLines() == 1)
    endingColumnNumber += get_file_info()->get_col() - 1;

  return endingColumnNumber;
}

PreprocessingInfo::DirectiveType PreprocessingInfo::getTypeOfDirective() const {
  // Access function for the type of directive
  return whatSortOfDirective;
}

void PreprocessingInfo::setTypeOfDirective(
    PreprocessingInfo::DirectiveType dt) {
  // Access function for the type of directive
  whatSortOfDirective = dt;
}

string PreprocessingInfo::directiveTypeName(const DirectiveType &directive) {
  string returnString;
  switch (directive) {
  case CpreprocessorUnknownDeclaration:
    returnString = "CpreprocessorUnknownDeclaration";
    break;
  case FortranStyleComment:
    returnString = "FortranStyleComment";
    break;
  case F90StyleComment:
    returnString = "F90StyleComment";
    break;
  case C_StyleComment:
    returnString = "C_StyleComment";
    break;
  case CplusplusStyleComment:
    returnString = "CplusplusStyleComment";
    break;
  case CpreprocessorBlankLine:
    returnString = "CpreprocessorBlankLine";
    break;
  case CpreprocessorIncludeDeclaration:
    returnString = "CpreprocessorIncludeDeclaration";
    break;
  case CpreprocessorIncludeNextDeclaration:
    returnString = "CpreprocessorIncludeNextDeclaration";
    break;
  case CpreprocessorDefineDeclaration:
    returnString = "CpreprocessorDefineDeclaration";
    break;
  case CpreprocessorUndefDeclaration:
    returnString = "CpreprocessorUndefDeclaration";
    break;
  case CpreprocessorIfdefDeclaration:
    returnString = "CpreprocessorIfdefDeclaration";
    break;
  case CpreprocessorElseDeclaration:
    returnString = "CpreprocessorElseDeclaration";
    break;
  case CpreprocessorElifDeclaration:
    returnString = "CpreprocessorElifDeclaration";
    break;
  case CpreprocessorIfndefDeclaration:
    returnString = "CpreprocessorIfndefDeclaration";
    break;
  case CpreprocessorIfDeclaration:
    returnString = "CpreprocessorIfDeclaration";
    break;
  case CpreprocessorDeadIfDeclaration:
    returnString = "CpreprocessorDeadIfDeclaration";
    break;
  case CpreprocessorEndifDeclaration:
    returnString = "CpreprocessorEndifDeclaration";
    break;
  case CpreprocessorEnd_ifDeclaration:
    returnString = "CpreprocessorEnd_ifDeclaration";
    break;
  case CpreprocessorLineDeclaration:
    returnString = "CpreprocessorLineDeclaration";
    break;
  case CpreprocessorPragmaDeclaration:
    returnString = "CpreprocessorPragmaDeclaration";
    break;
  case CpreprocessorErrorDeclaration:
    returnString = "CpreprocessorErrorDeclaration";
    break;
  case CpreprocessorWarningDeclaration:
    returnString = "CpreprocessorWarningDeclaration";
    break;
  case CpreprocessorEmptyDeclaration:
    returnString = "CpreprocessorEmptyDeclaration";
    break;
  case CSkippedToken:
    returnString = "CSkippedToken";
    break;
  case CMacroCall:
    returnString = "CMacroCall";
    break;
  case CMacroCallStatement:
    returnString = "CMacroCallStatement";
    break;
    // DQ (11/17/2008): Added support for #ident
  case CpreprocessorIdentDeclaration:
    returnString = "CpreprocessorIdentDeclaration";
    break;

    // DQ (11/17/2008): Added support for things like:  # 1 "<command line>"
  case CpreprocessorCompilerGeneratedLinemarker:
    returnString = "CpreprocessorCompilerGeneratedLinemarker";
    break;

  default:
    returnString = "ERROR DEFAULT REACHED";
    printf("Default reached in PreprocessingInfo::directiveTypeName() exiting "
           "... (directive = %d) \n",
           directive);
    ROSE_ABORT();
  }

  return returnString;
}

int PreprocessingInfo::getLineNumber() const {
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_line();
}

int PreprocessingInfo::getColumnNumber() const {
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_col();
}

std::string PreprocessingInfo::getFilename() const {
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_filenameString();
}

string PreprocessingInfo::getString() const {
  return internalString;
}

void PreprocessingInfo::setString(const std::string &s) {
  internalString = s;
}

int PreprocessingInfo::getNumberOfLines() const {
  int line = 0;
  int i = 0;
  while (internalString[i] != '\0') {
    if (internalString[i] == '\n') {
      line++;
    }
    i++;
  }

  if (line == 0)
    line = 1;

  ROSE_ASSERT(line > 0);
  return line;
}

void PreprocessingInfo::display(const std::string &label) const {
  ROSE_ASSERT(file_info != nullptr);
  printf("\nInside of PreprocessingInfo display(%s): \n", label.c_str());
  file_info->display(label);
  printf("     numberOfLines  = %d \n", numberOfLines);
  printf("     relativePosition = %s \n",
         relativePositionName(relativePosition).c_str());
  printf("     directiveType  = %s \n",
         directiveTypeName(whatSortOfDirective).c_str());
  printf("     internalString = %s \n\n", internalString.c_str());
}

std::string
PreprocessingInfo::relativePositionName(const RelativePositionType &position) {
  switch (position) {
  case defaultValue:
    return "defaultValue";
  case undef:
    return "undef";
  case before:
    return "before";
  case after:
    return "after";
  case inside:
    return "inside";
  case before_syntax:
    return "before_syntax";
  case after_syntax:
    return "after_syntax";
  default:
    return "";
  }
}

PreprocessingInfo::RelativePositionType
PreprocessingInfo::getRelativePosition(void) const {

  return relativePosition;
}

void PreprocessingInfo::setRelativePosition(RelativePositionType relPos) {

  if (attachedOwner != nullptr && relPos != relativePosition) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-attached-position]: attached "
            "record position cannot change outside its owner transaction\n");
    ROSE_ABORT();
  }

  relativePosition = relPos;
}

PreprocessingInfo::OutputPlacementType
PreprocessingInfo::getOutputPlacement() const {
  if (outputPlacement != source_position &&
      outputPlacement != attached_output_boundary &&
      outputPlacement != attached_output_trailing_line) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-output-placement]: record=%p "
            "has invalid placement=%d\n",
            static_cast<const void *>(this), static_cast<int>(outputPlacement));
    ROSE_ABORT();
  }
  return outputPlacement;
}

void PreprocessingInfo::setOutputPlacement(OutputPlacementType placement) {
  if ((placement != source_position && placement != attached_output_boundary &&
       placement != attached_output_trailing_line) ||
      (outputPlacement != source_position && outputPlacement != placement)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-output-placement]: record=%p "
            "cannot transition from placement=%d to placement=%d\n",
            static_cast<void *>(this), static_cast<int>(outputPlacement),
            static_cast<int>(placement));
    ROSE_ABORT();
  }
  outputPlacement = placement;
}

SgLocatedNode *PreprocessingInfo::getAttachedOwner() const {
  return attachedOwner;
}

void PreprocessingInfo::claimAttachedOwner(SgLocatedNode *owner) {
  if (owner == nullptr || attachedOwner != nullptr || file_info == nullptr ||
      file_info->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-attachment-owner]: record is "
            "not uniquely detached\n");
    ROSE_ABORT();
  }
  file_info->set_parent(owner);
  attachedOwner = owner;
}

void PreprocessingInfo::releaseAttachedOwner(SgLocatedNode *owner) {
  if (owner == nullptr || attachedOwner != owner || file_info == nullptr ||
      file_info->get_parent() != owner) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-attachment-owner]: record=%p "
            "current-owner=%p release-owner=%p does not own the record\n",
            static_cast<void *>(this), static_cast<void *>(attachedOwner),
            static_cast<void *>(owner));
    ROSE_ABORT();
  }
  file_info->set_parent(nullptr);
  attachedOwner = nullptr;
}

void PreprocessingInfo::changeAttachedPosition(SgLocatedNode *owner,
                                               RelativePositionType position) {
  if (owner == nullptr || attachedOwner != owner) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-attached-position]: record=%p "
            "current-owner=%p requested-owner=%p cannot change position\n",
            static_cast<void *>(this), static_cast<void *>(attachedOwner),
            static_cast<void *>(owner));
    ROSE_ABORT();
  }
  relativePosition = position;
}

void PreprocessingInfo::changeAttachedOutputPlacement(
    SgLocatedNode *owner, OutputPlacementType placement) {
  if (owner == nullptr || attachedOwner != owner ||
      (placement != attached_output_boundary &&
       placement != attached_output_trailing_line)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-output-placement]: record=%p "
            "current-owner=%p requested-owner=%p cannot publish attached "
            "placement=%d\n",
            static_cast<void *>(this), static_cast<void *>(attachedOwner),
            static_cast<void *>(owner), static_cast<int>(placement));
    ROSE_ABORT();
  }
  outputPlacement = placement;
}

int PreprocessingInfo::getStringLength(void) const {

  return internalString.length();
}

bool PreprocessingInfo::has_file_info() const {
  return file_info != NULL;
}

Sg_File_Info *PreprocessingInfo::get_file_info() const {

  if (file_info == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-source-provenance]: "
            "preprocessing entry has no source or transformation file info\n");
    ROSE_ABORT();
  }
  return file_info;
}

void PreprocessingInfo::set_file_info(Sg_File_Info *info) {
  if (info == nullptr || (info != file_info && info->get_parent() != nullptr) ||
      (file_info != nullptr && file_info->get_parent() != attachedOwner)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-source-owner]: replacement "
            "requires detached file info and an exact existing owner\n");
    ROSE_ABORT();
  }
  if (info == file_info)
    return;
  if (file_info != nullptr)
    file_info->set_parent(nullptr);
  info->set_parent(attachedOwner);
  file_info = info;
}

// DQ (11/28/2008): Support for CPP generated linemarkers
int PreprocessingInfo::get_lineNumberForCompilerGeneratedLinemarker() {
  return lineNumberForCompilerGeneratedLinemarker;
}

std::string PreprocessingInfo::get_filenameForCompilerGeneratedLinemarker() {
  return filenameForCompilerGeneratedLinemarker;
}

std::string
PreprocessingInfo::get_optionalflagsForCompilerGeneratedLinemarker() {
  return optionalflagsForCompilerGeneratedLinemarker;
}

// DQ (11/28/2008): Support for CPP generated linemarkers
void PreprocessingInfo::set_lineNumberForCompilerGeneratedLinemarker(int x) {
  lineNumberForCompilerGeneratedLinemarker = x;
}

void PreprocessingInfo::set_filenameForCompilerGeneratedLinemarker(
    std::string x) {
  filenameForCompilerGeneratedLinemarker = x;
}

void PreprocessingInfo::set_optionalflagsForCompilerGeneratedLinemarker(
    std::string x) {
  optionalflagsForCompilerGeneratedLinemarker = x;
}

namespace {

struct PhaseTwoSpelling {
  std::string logical;
  std::vector<size_t> rawOffsets;
};

std::string diagnosticDirectiveSpelling(const std::string &spelling) {
  std::string result;
  result.reserve(spelling.size());
  for (char character : spelling) {
    switch (character) {
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result += character;
      break;
    }
  }
  return result;
}

[[noreturn]] void failMacroDefinitionSpelling(const char *reason,
                                              const std::string &spelling) {
  ASSERT_not_null(reason);
  const std::string diagnosticSpelling = diagnosticDirectiveSpelling(spelling);
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[macro-definition-spelling]: reason=%s "
          "spelling=%s\n",
          reason, diagnosticSpelling.c_str());
  ROSE_ABORT();
}

bool isHorizontalWhitespace(char character) {
  return character == ' ' || character == '\t' || character == '\f' ||
         character == '\v';
}

PhaseTwoSpelling deletePhaseTwoLineSplices(const std::string &spelling) {
  PhaseTwoSpelling result;
  result.logical.reserve(spelling.size());
  result.rawOffsets.reserve(spelling.size());

  for (size_t raw = 0; raw < spelling.size();) {
    if (spelling[raw] == '\\' && raw + 1 < spelling.size()) {
      if (spelling[raw + 1] == '\n') {
        raw += 2;
        continue;
      }
      if (spelling[raw + 1] == '\r' && raw + 2 < spelling.size() &&
          spelling[raw + 2] == '\n') {
        raw += 3;
        continue;
      }
    }

    result.rawOffsets.push_back(raw);
    result.logical += spelling[raw];
    ++raw;
  }

  return result;
}

bool skipMacroDefinitionTrivia(const std::string &spelling, size_t &cursor,
                               const std::string &diagnosticSpelling) {
  bool skipped = false;
  while (cursor < spelling.size()) {
    if (isHorizontalWhitespace(spelling[cursor])) {
      ++cursor;
      skipped = true;
      continue;
    }
    if (spelling.compare(cursor, 2, "/*") == 0) {
      const size_t commentEnd = spelling.find("*/", cursor + 2);
      if (commentEnd == std::string::npos) {
        failMacroDefinitionSpelling("unterminated-comment", diagnosticSpelling);
      }
      cursor = commentEnd + 2;
      skipped = true;
      continue;
    }
    break;
  }
  return skipped;
}

bool isMacroIdentifierStart(unsigned char character) {
  return character == '_' || character == '$' || std::isalpha(character) ||
         character >= 0x80;
}

bool isMacroIdentifierContinuation(unsigned char character) {
  return isMacroIdentifierStart(character) || std::isdigit(character);
}

size_t consumeUniversalCharacterName(const std::string &spelling,
                                     size_t cursor) {
  if (cursor + 1 >= spelling.size() || spelling[cursor] != '\\' ||
      (spelling[cursor + 1] != 'u' && spelling[cursor + 1] != 'U')) {
    return cursor;
  }
  const size_t digitCount = spelling[cursor + 1] == 'u' ? 4 : 8;
  if (cursor + 2 + digitCount > spelling.size()) {
    return cursor;
  }
  for (size_t index = cursor + 2; index < cursor + 2 + digitCount; ++index) {
    if (!std::isxdigit(static_cast<unsigned char>(spelling[index]))) {
      return cursor;
    }
  }
  return cursor + 2 + digitCount;
}

std::string parseMacroDefinitionSpelling(const std::string &spelling) {
  const PhaseTwoSpelling phaseTwo = deletePhaseTwoLineSplices(spelling);
  const std::string &logical = phaseTwo.logical;
  size_t cursor = 0;
  skipMacroDefinitionTrivia(logical, cursor, spelling);
  if (cursor < logical.size() && logical[cursor] == '#') {
    ++cursor;
  } else if (logical.compare(cursor, 2, "%:") == 0) {
    cursor += 2;
  } else {
    failMacroDefinitionSpelling("missing-hash", spelling);
  }
  skipMacroDefinitionTrivia(logical, cursor, spelling);

  constexpr char keyword[] = "define";
  constexpr size_t keywordLength = sizeof(keyword) - 1;
  if (logical.compare(cursor, keywordLength, keyword) != 0) {
    failMacroDefinitionSpelling("missing-define-keyword", spelling);
  }
  cursor += keywordLength;
  if (!skipMacroDefinitionTrivia(logical, cursor, spelling)) {
    failMacroDefinitionSpelling("missing-name-separator", spelling);
  }
  if (cursor >= logical.size()) {
    failMacroDefinitionSpelling("missing-macro-name", spelling);
  }

  const size_t nameBegin = cursor;
  const size_t universalEnd = consumeUniversalCharacterName(logical, cursor);
  if (universalEnd != cursor) {
    cursor = universalEnd;
  } else if (isMacroIdentifierStart(
                 static_cast<unsigned char>(logical[cursor]))) {
    ++cursor;
  } else {
    failMacroDefinitionSpelling("invalid-macro-name", spelling);
  }

  while (cursor < logical.size()) {
    const size_t continuationEnd =
        consumeUniversalCharacterName(logical, cursor);
    if (continuationEnd != cursor) {
      cursor = continuationEnd;
      continue;
    }
    if (!isMacroIdentifierContinuation(
            static_cast<unsigned char>(logical[cursor]))) {
      break;
    }
    ++cursor;
  }

  ASSERT_require(nameBegin < cursor);
  ASSERT_require(cursor <= phaseTwo.rawOffsets.size());
  const size_t rawNameBegin = phaseTwo.rawOffsets[nameBegin];
  const size_t rawNameEnd = phaseTwo.rawOffsets[cursor - 1] + 1;
  const std::string logicalName = logical.substr(nameBegin, cursor - nameBegin);
  const PhaseTwoSpelling exactRawName = deletePhaseTwoLineSplices(
      spelling.substr(rawNameBegin, rawNameEnd - rawNameBegin));
  if (exactRawName.logical != logicalName) {
    failMacroDefinitionSpelling("macro-name-offset-mismatch", spelling);
  }

  return logicalName;
}

} // namespace

std::string PreprocessingInfo::getMacroName() {
  if (this->getTypeOfDirective() !=
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[macro-definition-kind]: directive=%s "
            "cannot provide a #define macro name\n",
            directiveTypeName(this->getTypeOfDirective()).c_str());
    ROSE_ABORT();
  }

  return parseMacroDefinitionSpelling(internalString);
}

bool PreprocessingInfo::isTransformation() const {
  return p_isTransformation;
}

void PreprocessingInfo::setAsTransformation() {
  p_isTransformation = true;
}

void PreprocessingInfo::unsetAsTransformation() { p_isTransformation = false; }

ROSEAttributesList::ROSEAttributesList() : rawTokenStream(nullptr) {}

ROSEAttributesList::ROSEAttributesList(const ROSEAttributesList &source)
    : rawTokenStream(nullptr), fileName(source.fileName),
      filenameIdSet(source.filenameIdSet) {
  std::map<const PreprocessingInfo *, PreprocessingInfo *> records;
  ROSEAttributesList pending;
  for (const auto *record : source.attributeList) {
    ROSE_ASSERT(record != nullptr);
    auto copy = std::make_unique<PreprocessingInfo>(*record);
    ROSE_ASSERT(records.emplace(record, copy.get()).second);
    pending.attributeList.push_back(copy.get());
    copy.release();
  }
  if (source.rawTokenStream != nullptr) {
    pending.rawTokenStream = new LexTokenStreamType();
    for (const auto *element : *source.rawTokenStream) {
      ROSE_ASSERT(element != nullptr && element->p_tok_elem != nullptr);
      auto copy = std::make_unique<stream_element>();
      auto token = std::make_unique<token_element>(*element->p_tok_elem);
      copy->p_tok_elem = token.get();
      copy->beginning_fpi = element->beginning_fpi;
      copy->ending_fpi = element->ending_fpi;
      if (element->p_preprocessingInfo != nullptr) {
        auto record = records.find(element->p_preprocessingInfo);
        ROSE_ASSERT(record != records.end());
        copy->p_preprocessingInfo = record->second;
      }
      pending.rawTokenStream->push_back(copy.get());
      token.release();
      copy.release();
    }
  }
  attributeList.swap(pending.attributeList);
  std::swap(rawTokenStream, pending.rawTokenStream);
}

ROSEAttributesList::~ROSEAttributesList() { deepClean(); }

void ROSEAttributesList::addElement(PreprocessingInfo::DirectiveType dt,
                                    const std::string &pLine,
                                    const std::string &filename, int lineNumber,
                                    int columnNumber, int numOfLines) {
  ROSE_ASSERT(pLine.empty() == false);
  ROSE_ASSERT(lineNumber > 0);
  ROSE_ASSERT(columnNumber > 0);
  ROSE_ASSERT(numOfLines >= 0); // == 0, if cpp_comment in a single line
  auto element = std::make_unique<PreprocessingInfo>(
      dt, pLine, filename, lineNumber, columnNumber, numOfLines,
      PreprocessingInfo::undef);
  attributeList.push_back(element.get());
  element.release();
}

PreprocessingInfo *ROSEAttributesList::lastElement() {
  ROSE_ASSERT(attributeList.empty() == false);
  if (attributeList.back() == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[preprocessing-list-entry]: lastElement "
                    "contains null entry\n");
    ROSE_ABORT();
  }
  return attributeList.back();
}

void ROSEAttributesList::moveElements(ROSEAttributesList &pList) {
  ASSERT_require(this != &pList);
  attributeList.insert(attributeList.end(), pList.attributeList.begin(),
                       pList.attributeList.end());
  pList.attributeList.clear();
  ROSE_ASSERT(pList.attributeList.empty());
}

void ROSEAttributesList::setFileName(const string &fName) { fileName = fName; }

string ROSEAttributesList::getFileName() { return fileName; }

int ROSEAttributesList::size(void) {
  return getLength();
}

int ROSEAttributesList::getLength(void) {
  return attributeList.size();
}

void ROSEAttributesList::clean(void) {

  for (PreprocessingInfo *info : attributeList) {
    delete info;
  }
  attributeList.clear();
}

void ROSEAttributesList::deepClean(void) {

  clean();

  if (rawTokenStream != NULL) {
    for (stream_element *element : *rawTokenStream) {
      if (element != NULL) {
        delete element->p_tok_elem;
        delete element;
      }
    }
    delete rawTokenStream;
    rawTokenStream = NULL;
  }
}

PreprocessingInfo *ROSEAttributesList::operator[](int i) {
  return attributeList[i];
}

void ROSEAttributesList::set_rawTokenStream(LexTokenStreamTypePointer s) {
  rawTokenStream = s;
}

LexTokenStreamTypePointer ROSEAttributesList::get_rawTokenStream() {
  return rawTokenStream;
}

bool ROSEAttributesList::isFortran90Comment(const string &line) {
  // This refactored code test if a line is a fortran comment.
  // Fortran 90 comments are more complex to recognise than
  // F77.  This function only recognizes F90 comments that have
  // a leading "!".  Other uses of "!" at the end of a valid
  // Fortran statement are not yet captured, but that would be
  // handled by this function (later).

  const size_t firstNonBlank = line.find_first_not_of(" \t\r");
  return firstNonBlank != string::npos && line[firstNonBlank] == '!';
}

bool ROSEAttributesList::isFortran77Comment(const string &line) {
  if (line.find_first_not_of(" \t\r") == string::npos) {
    return true;
  }

  const unsigned char firstCharacter = static_cast<unsigned char>(line.front());
  if (firstCharacter != ' ' && firstCharacter != '\t' &&
      (firstCharacter < 0x20 || firstCharacter > 0x7e)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[fortran-fixed-source-byte]: "
            "first-byte=%u is not printable ASCII, space, or tab\n",
            static_cast<unsigned int>(firstCharacter));
    ROSE_ABORT();
  }
  // A digit in column one begins a label; other nonblank characters are
  // treated as fixed-format comments after directives have been classified.
  return firstCharacter != ' ' && firstCharacter != '\t' &&
         (firstCharacter < '0' || firstCharacter > '9');
}

bool ROSEAttributesList::isCppDirective(
    const string &line, PreprocessingInfo::DirectiveType &cppDeclarationKind,
    std::string &restOfTheLine) {
  cppDeclarationKind = PreprocessingInfo::CpreprocessorUnknownDeclaration;
  restOfTheLine.clear();

  const PhaseTwoSpelling phaseTwo = deletePhaseTwoLineSplices(line);
  const string &logicalLine = phaseTwo.logical;
  size_t cursor = logicalLine.find_first_not_of(" \t");
  if (cursor == string::npos || logicalLine[cursor] != '#') {
    return false;
  }
  ++cursor;
  cursor = logicalLine.find_first_not_of(" \t", cursor);
  if (cursor == string::npos) {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorEmptyDeclaration;
    return true;
  }

  if (std::isdigit(static_cast<unsigned char>(logicalLine[cursor]))) {
    cppDeclarationKind =
        PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker;
    restOfTheLine = logicalLine.substr(cursor);
    return true;
  }

  const size_t identifierStart = cursor;
  while (cursor < logicalLine.size()) {
    const unsigned char character =
        static_cast<unsigned char>(logicalLine[cursor]);
    if (!std::isalnum(character) && character != '_') {
      break;
    }
    ++cursor;
  }
  if (cursor == identifierStart) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessing-directive-kind]: hash "
            "directive starts with invalid identifier character\n");
    ROSE_ABORT();
  }

  const string identifier =
      logicalLine.substr(identifierStart, cursor - identifierStart);
  restOfTheLine = logicalLine.substr(cursor);
  if (identifier == "include") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorIncludeDeclaration;
  } else if (identifier == "includenext" || identifier == "include_next") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
  } else if (identifier == "define") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorDefineDeclaration;
  } else if (identifier == "undef") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorUndefDeclaration;
  } else if (identifier == "ifdef") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorIfdefDeclaration;
  } else if (identifier == "ifndef") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorIfndefDeclaration;
  } else if (identifier == "if") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorIfDeclaration;
  } else if (identifier == "else") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorElseDeclaration;
  } else if (identifier == "elif") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorElifDeclaration;
  } else if (identifier == "endif") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorEndifDeclaration;
  } else if (identifier == "end") {
    const size_t suffix = restOfTheLine.find_first_not_of(" \t");
    if (suffix == string::npos || restOfTheLine.compare(suffix, 2, "if") != 0 ||
        (suffix + 2 < restOfTheLine.size() &&
         (std::isalnum(static_cast<unsigned char>(restOfTheLine[suffix + 2])) ||
          restOfTheLine[suffix + 2] == '_'))) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[preprocessing-directive-kind]: "
                      "unsupported hash directive=end\n");
      ROSE_ABORT();
    }
    cppDeclarationKind = PreprocessingInfo::CpreprocessorEnd_ifDeclaration;
  } else if (identifier == "line") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorLineDeclaration;
  } else if (identifier == "error") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorErrorDeclaration;
  } else if (identifier == "warning") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorWarningDeclaration;
  } else if (identifier == "pragma") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorPragmaDeclaration;
  } else if (identifier == "ident") {
    cppDeclarationKind = PreprocessingInfo::CpreprocessorIdentDeclaration;
  } else {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessing-directive-kind]: "
            "unsupported hash directive=%s\n",
            identifier.c_str());
    ROSE_ABORT();
  }
  return true;
}

static bool preprocessingLineHasContinuation(const string &line) {
  size_t end = line.size();
  if (end > 0 && line[end - 1] == '\r') {
    --end;
  }
  return end > 0 && line[end - 1] == '\\';
}

static bool preprocessingLineStartsHashDirective(const string &line) {
  const size_t firstNonBlank = line.find_first_not_of(" \t");
  return firstNonBlank != string::npos && line[firstNonBlank] == '#';
}

void ROSEAttributesList::collectPreprocessorDirectivesAndCommentsForAST(
    const string &filename, ROSEAttributesList::languageTypeEnum languageType) {

  ROSE_ASSERT(filename.empty() == false);

  // Open file for reading line by line!
  const PreprocessingInfo::DirectiveType commentKind =
      PreprocessingInfo::FortranStyleComment;
  string line;
  constexpr int colNumber = 1;

  errno = 0;
  ifstream targetFile(filename.c_str());
  if (targetFile.is_open()) {
    int lineCounter = 1;
    while (getline(targetFile, line)) {
      int numberOfLines = 1;

      if (preprocessingLineStartsHashDirective(line)) {
        string physicalLine = line;
        while (preprocessingLineHasContinuation(physicalLine)) {
          string nextLine;
          if (!getline(targetFile, nextLine)) {
            if (targetFile.bad()) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[preprocessing-source-read]: "
                      "file=%s failed while reading continuation after "
                      "line=%d\n",
                      filename.c_str(), lineCounter + numberOfLines - 1);
            } else {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[preprocessing-continuation]: "
                      "file=%s line=%d ends in a continuation at end of "
                      "file\n",
                      filename.c_str(), lineCounter + numberOfLines - 1);
            }
            ROSE_ABORT();
          }

          // Preserve the physical-line boundary in the directive spelling.
          line += "\n" + nextLine;
          physicalLine = nextLine;
          ++numberOfLines;
        }
      }

      string restOfTheLine;
      PreprocessingInfo::DirectiveType cppDeclarationKind =
          PreprocessingInfo::CpreprocessorUnknownDeclaration;
      bool cppDirective =
          isCppDirective(line, cppDeclarationKind, restOfTheLine);

      if (cppDirective == false) {
        bool isComment = false;

        switch (languageType) {
        case e_C_language:
        case e_Cxx_language:
          break;

        case e_Fortran77_language:
          isComment = isFortran77Comment(line);
          break;

        case e_Fortran9x_language:
          isComment = isFortran90Comment(line);
          break;

        default: {
          printf("Error: default in switch over languageType = %d \n",
                 languageType);
          ROSE_ABORT();
        }
        }

        if (isComment == true) {
          cppDeclarationKind = commentKind;
        }
      }

      // Ordinary source lines are intentionally absent; every classified
      // directive and fixed/free-form comment is preserved exactly once.
      if (cppDeclarationKind !=
          PreprocessingInfo::CpreprocessorUnknownDeclaration) {
        PreprocessingInfo *cppDirective = new PreprocessingInfo(
            cppDeclarationKind, line, filename, lineCounter, colNumber,
            numberOfLines, PreprocessingInfo::before);
        ROSE_ASSERT(cppDirective != NULL);
        attributeList.push_back(cppDirective);
        if (cppDeclarationKind ==
            PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker) {
          size_t cursor = 0;
          int linemarkerLine = 0;
          while (
              cursor < restOfTheLine.size() &&
              std::isdigit(static_cast<unsigned char>(restOfTheLine[cursor]))) {
            const int digit = restOfTheLine[cursor] - '0';
            if (linemarkerLine >
                (std::numeric_limits<int>::max() - digit) / 10) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[preprocessing-linemarker]: "
                      "file=%s line=%d line number is out of range\n",
                      filename.c_str(), lineCounter);
              ROSE_ABORT();
            }
            linemarkerLine = linemarkerLine * 10 + digit;
            ++cursor;
          }

          while (
              cursor < restOfTheLine.size() &&
              (restOfTheLine[cursor] == ' ' || restOfTheLine[cursor] == '\t')) {
            ++cursor;
          }
          if (cursor == restOfTheLine.size() || restOfTheLine[cursor] != '"') {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[preprocessing-linemarker]: "
                    "file=%s line=%d has no quoted filename\n",
                    filename.c_str(), lineCounter);
            ROSE_ABORT();
          }

          const size_t filenameStart = cursor;
          bool escaped = false;
          ++cursor;
          for (; cursor < restOfTheLine.size(); ++cursor) {
            const char character = restOfTheLine[cursor];
            if (character == '"' && !escaped) {
              break;
            }
            if (character == '\\') {
              escaped = !escaped;
            } else {
              escaped = false;
            }
          }
          if (cursor == restOfTheLine.size()) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[preprocessing-linemarker]: "
                    "file=%s line=%d has no closing filename quote\n",
                    filename.c_str(), lineCounter);
            ROSE_ABORT();
          }

          cppDirective->set_lineNumberForCompilerGeneratedLinemarker(
              linemarkerLine);
          cppDirective->set_filenameForCompilerGeneratedLinemarker(
              restOfTheLine.substr(filenameStart, cursor - filenameStart + 1));

          ++cursor;
          while (
              cursor < restOfTheLine.size() &&
              (restOfTheLine[cursor] == ' ' || restOfTheLine[cursor] == '\t')) {
            ++cursor;
          }
          cppDirective->set_optionalflagsForCompilerGeneratedLinemarker(
              restOfTheLine.substr(cursor));
        }
      }

      lineCounter += numberOfLines;
    }

    if (targetFile.bad()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[preprocessing-source-read]: file=%s "
              "failed after line=%d\n",
              filename.c_str(), lineCounter - 1);
      ROSE_ABORT();
    }

    targetFile.close();
  } else {
    const int openError = errno;
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessing-source-open]: file=%s "
            "errno=%d message=%s\n",
            filename.c_str(), openError,
            openError != 0 ? std::strerror(openError)
                           : "input stream did not report an OS error");
    ROSE_ABORT();
  }
}

void ROSEAttributesList::generateFileIdListFromLineDirectives() {
  ROSE_ASSERT(Sg_File_Info::get_nametofileid_map().size() ==
              Sg_File_Info::get_fileidtoname_map().size());

  ROSE_ASSERT(Sg_File_Info::get_nametofileid_map().find("") ==
              Sg_File_Info::get_nametofileid_map().end());

  vector<PreprocessingInfo *>::iterator i = attributeList.begin();
  for (i = attributeList.begin(); i != attributeList.end(); i++) {

    if ((*i)->getTypeOfDirective() ==
        PreprocessingInfo::CpreprocessorLineDeclaration) {
      // This is a CPP line directive
      string directiveString = (*i)->getString();
      // Remove leading white space.
      size_t p = directiveString.find_first_not_of("# \t");
      if (p == string::npos) {
        continue;
      }
      directiveString.erase(0, p);

      // Support both "#line <n> \"file\"" and "# <n> \"file\"" forms.
      string directiveStringWithoutHashAndKeyword = directiveString;
      if (directiveStringWithoutHashAndKeyword.rfind("line", 0) == 0) {
        size_t afterLine = string("line").length();
        if (directiveStringWithoutHashAndKeyword.size() == afterLine ||
            isspace(static_cast<unsigned char>(
                directiveStringWithoutHashAndKeyword[afterLine]))) {
          directiveStringWithoutHashAndKeyword =
              directiveStringWithoutHashAndKeyword.substr(afterLine);
        }
      }

      // Remove white space before the line number.
      p = directiveStringWithoutHashAndKeyword.find_first_not_of(" \t");
      if (p == string::npos) {
        continue;
      }
      directiveStringWithoutHashAndKeyword.erase(0, p);
      // At this point we have just '2 "toke.l"', and we can strip off the
      // number.
      p = directiveStringWithoutHashAndKeyword.find_first_not_of("0123456789");

      if (p == 0) {
        continue;
      }
      if (p != string::npos) {
        string directiveStringWithoutHashAndKeywordAndLineNumber =
            directiveStringWithoutHashAndKeyword.substr(p);
        // Remove white space between the line number and the filename.
        p = directiveStringWithoutHashAndKeywordAndLineNumber.find_first_not_of(
            " \t");
        if (p == string::npos) {
          continue;
        }
        directiveStringWithoutHashAndKeywordAndLineNumber.erase(0, p);
        string quotedFilename =
            directiveStringWithoutHashAndKeywordAndLineNumber;
        if (!quotedFilename.empty() &&
            (quotedFilename[0] == '\"' || quotedFilename[0] == '<' ||
             quotedFilename[0] == '\'')) {
          char opener = quotedFilename[0];
          char closer = (opener == '<') ? '>' : opener;
          size_t positionOfNextQuote = quotedFilename.find(closer, 1);
          if (positionOfNextQuote == string::npos) {
            continue;
          }
          std::string filename =
              quotedFilename.substr(1, positionOfNextQuote - 1);
          // Add the new filename to the static map stored in the Sg_File_Info
          // (no action if filename is already in the map).
          Sg_File_Info::addFilenameToMap(filename);

          filenameIdSet.insert(Sg_File_Info::getIDFromFilename(filename));
        }
      } else {
        printf("NOTE: In "
               "ROSEAttributesList::generateFileIdListFromLineDirectives(): no "
               "filename present in directiveString = %s \n",
               directiveString.c_str());
      }
    }
  }
}

// DQ (12/15/2012): Added access function.
std::set<int> &ROSEAttributesList::get_filenameIdSet() { return filenameIdSet; }

// ##############################################################################
//
//  [DT] 3/15/2000 -- Begin member function definitions for
//       ROSEAttributesListContainer.
//

ROSEAttributesListContainer::ROSEAttributesListContainer() {
  // Nothing to do here?
}

ROSEAttributesListContainer::ROSEAttributesListContainer(
    const ROSEAttributesListContainer &source) {
  ROSEAttributesListContainer pending;
  for (const auto &entry : source.attributeListMap) {
    ROSE_ASSERT(entry.second != nullptr);
    auto list = std::make_unique<ROSEAttributesList>(*entry.second);
    pending.attributeListMap.emplace(entry.first, list.get());
    list.release();
  }
  attributeListMap.swap(pending.attributeListMap);
}

ROSEAttributesListContainer::~ROSEAttributesListContainer() {
  for (auto &entry : attributeListMap) {
    delete entry.second;
  }
  attributeListMap.clear();
}

void ROSEAttributesListContainer::addList(std::string fileName,
                                          ROSEAttributesList *listPointer) {

  ROSEAttributesList *&entry = attributeListMap[fileName];
  if (entry != listPointer) {
    delete entry;
    entry = listPointer;
  }
}

ROSEAttributesList &
ROSEAttributesListContainer::operator[](const string &fName) {
  ROSEAttributesList *list = attributeListMap.at(fName);
  ASSERT_not_null(list);
  return *list;
}

void ROSEAttributesListContainer::deepClean(void) {
  for (auto &entry : attributeListMap) {
    if (entry.second != NULL) {
      entry.second->deepClean();
    }
  }
}

void ROSEAttributesListContainer::clean(void) {
  for (auto &entry : attributeListMap) {
    if (entry.second != NULL) {
      entry.second->clean();
    }
  }
}

// EOF
