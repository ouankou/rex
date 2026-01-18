#include "sage3basic.h"

#include "rose_attributes_list.h"

#include <errno.h>

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"
// DQ (11/28/2009): I think this is equivalent to "USE_ROSE"
// #if CAN_NOT_COMPILE_WITH_ROSE != true
// #if (CAN_NOT_COMPILE_WITH_ROSE == 0)
// #ifndef USE_ROSE

// DQ (9/30/2013): This global variable is used in only the initial accumulation
// of the CPP directives, comments and tokens by file name in the
// src/frontend/SageIII/preproc-c.ll file.  Later after processin it is an empty
// map (e.g. in the unparsing phase). This is a confusing global variable to
// have and it appears to have be used within a specific phase of the processing
// of CPP directives, comments and tokens. AS(01/04/07) Global map of filenames
// to PreprocessingInfo*'s as it is inefficient to get this by a traversal of
// the AST
std::map<std::string, ROSEAttributesList *> mapFilenameToAttributes;

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

// JH (01/03/2006) methods for packing the PreprocessingInfo data, in order to
// store it into a file and rebuild it!
unsigned int PreprocessingInfo::packed_size() const {
  // This function computes the size of the packed representation of this
  // classes data members.

  ASSERT_this();

  unsigned int packedSize = sizeof(file_info) +
                            /* string size and string */ sizeof(unsigned int) +
                            internalString.size() + sizeof(numberOfLines) +
                            sizeof(whatSortOfDirective) +
                            sizeof(relativePosition) +
                            sizeof(lineNumberForCompilerGeneratedLinemarker) +
                            /* string size and string */ sizeof(unsigned int) +
                            filenameForCompilerGeneratedLinemarker.size() +
                            /* string size and string */ sizeof(unsigned int) +
                            optionalflagsForCompilerGeneratedLinemarker.size();

  // This is part of preprocessing support in ROSE.
  // #ifndef USE_ROSE
  // Add in the four pointers required for preprocessing support.
  // Until we add the support to save all the preprocessing data into
  // the AST file we would have to reprocess the relevant
  // file to store this.
  // #endif

  // Debugging information.  What can we assert about the packedSize vs. the
  // sizeof(PreprocessingInfo)? If there is anything, then it might make for
  // a simple test here.  However, there does not appear to be any
  // relationship since the sizeof(PreprocessingInfo) does not account for
  // the sizes of internal strings used. printf ("In
  // PreprocessingInfo::packed_size(): packedSize = %u
  // sizeof(PreprocessingInfo) = %" PRIuPTR "
  // \n",packedSize,sizeof(PreprocessingInfo));

  // I think that because we have to save additional information the
  // packedSize will be a little larger than the sizeof(PreprocessingInfo).
  // So assert this as a test. Unfortunately it is not always true!
  // ROSE_ASSERT(packedSize >= sizeof(PreprocessingInfo));

  return packedSize;
}

// JH (01/03/2006) This pack methods might cause memory leaks. Think of deleting
// them after stored to file ...
char *PreprocessingInfo::packed() const {
  ASSERT_this();

  // printf ("Inside of PreprocessingInfo::packed() internalString = %s
  // \n",internalString.c_str());

  const char *saveString = internalString.c_str();
  unsigned int stringSize = internalString.size();

  // Wouldn't padding of data cause us to under compute the size of the buffer?
  char *returnData = new char[packed_size()];

  char *storePointer = returnData;

  // printf ("Error, need to get the info out of the SgFileInfo object! \n");
  // ROSE_ABORT();

  // DQ (2/28/2010): We do want to write out the data value for this since it
  // has been converted to a global index value in the AST file I/O.
  memcpy(storePointer, (char *)(&file_info), sizeof(file_info));
  storePointer += sizeof(file_info);

  // memcpy (storePointer , (char*)(&lineNumber), sizeof(lineNumber) );
  // storePointer += sizeof(lineNumber);
  // memcpy (storePointer , (char*)(&columnNumber), sizeof(columnNumber) );
  // storePointer += sizeof(columnNumber);

  memcpy(storePointer, (char *)(&numberOfLines), sizeof(numberOfLines));
  storePointer += sizeof(numberOfLines);
  memcpy(storePointer, (char *)(&whatSortOfDirective),
         sizeof(whatSortOfDirective));
  storePointer += sizeof(DirectiveType);
  memcpy(storePointer, (char *)(&relativePosition), sizeof(relativePosition));
  storePointer += sizeof(RelativePositionType);
  memcpy(storePointer, (char *)(&stringSize), sizeof(stringSize));
  storePointer += sizeof(stringSize);
  memcpy(storePointer, saveString, stringSize);

  // printf ("Inside of PreprocessingInfo::packed(): Note some Fortran
  // specific data members are not packed yet (also all the preprocessing
  // data is not packed). \n");

  // DQ (2/28/2010): Some assertion checking that will be done later in the
  // unparser. printf ("In PreprocessingInfo::packed(): getTypeOfDirective() =
  // %d \n",getTypeOfDirective());
  ROSE_ASSERT(getTypeOfDirective() !=
              PreprocessingInfo::CpreprocessorUnknownDeclaration);

  return returnData;
}

// JH (01/03/2006) This unpack method works complementary to packed ...
void PreprocessingInfo::unpacked(char *storePointer) {
  ASSERT_this();

  // std::cout << " in PreprocessingInfo::unpacked ... " << std::endl;
  // printf ("Error, need to build a new SgFileInfo object! \n");
  // ROSE_ABORT();

  // DQ (2/28/2010): But jump over the file_info data member so that all ther
  // other data members will be unpacked properly.
  storePointer += sizeof(file_info);

  // memcpy ( (char*)(&lineNumber), storePointer, sizeof(lineNumber) );
  // storePointer += sizeof(lineNumber);
  // memcpy ( (char*)(&columnNumber), storePointer, sizeof(columnNumber) );
  // storePointer += sizeof(columnNumber);

  memcpy((char *)(&numberOfLines), storePointer, sizeof(numberOfLines));
  storePointer += sizeof(numberOfLines);
  memcpy((char *)(&whatSortOfDirective), storePointer,
         sizeof(whatSortOfDirective));
  storePointer += sizeof(DirectiveType);
  memcpy((char *)(&relativePosition), storePointer, sizeof(relativePosition));
  storePointer += sizeof(RelativePositionType);
  int stringSize = 0;
  memcpy((char *)(&stringSize), storePointer, sizeof(stringSize));
  storePointer += sizeof(stringSize);
  internalString = string(storePointer, stringSize);

  // DQ (2/28/2010): Some assertion checking that will be done later in the
  // unparser. This test helps debug if any of the data members are set at an
  // offset to there proper positions. printf ("In
  // PreprocessingInfo::unpacked(): getTypeOfDirective() = %d
  // \n",getTypeOfDirective());
  ROSE_ASSERT(getTypeOfDirective() !=
              PreprocessingInfo::CpreprocessorUnknownDeclaration);
}

// ********************************************
// Member functions for class PreprocessingInfo
// ********************************************

// #endif

PreprocessingInfo::PreprocessingInfo() {
  // Set these values so that they are not set to zero (a valid value) if a
  // PreprocessingInfo object is reused

  // DQ (4/22/2006): This default constructor is called by the
  // EasyStorage<PreprocessingInfo*>::rebuildDataStoredInEasyStorageClass()
  // if this constructor builds a Sg_File_Info object during the AST
  // reconstruction phase then errors result in the final AST.
  // Very Strange Errors!!!

  // DQ (4/21/2006): These are illegal values for a Sg_File_Info object
  // file_info = new Sg_File_Info("comment filename",-1,-1);
  // file_info = new Sg_File_Info("comment filename",0,0);
  file_info = NULL;
  // lineNumber          = -1;
  // columnNumber        = -1;

  numberOfLines = -1;
  whatSortOfDirective = CpreprocessorUnknownDeclaration;
  relativePosition = before;

  // DQ (1/15/2015): Adding support for token-based unparsing, initialization of
  // new data member.
  p_isTransformation = false;
}

// Typical constructor used by lex-based code retrieve comments and preprocessor
// control directives
PreprocessingInfo::PreprocessingInfo(
    DirectiveType dt, const string &inputString, const string &inputFileName,
    int line_no, int col_no, int nol, RelativePositionType relPos
    // DQ (7/19/2008): Removed these: bool copiedFlag, bool unparsedFlag
    )
    : file_info(NULL),
      // lineNumber(line_no), columnNumber (col_no),
      numberOfLines(nol), whatSortOfDirective(dt), relativePosition(relPos) {
  // DQ (10/29/2007): Test the filename is a way similar to how it is failing in
  // lower level code
  if (inputFileName == "NULL_FILE") {
    // printf ("In PreprocessingInfo constructor, inputFileName == "NULL_FILE"
    // \n");
    ROSE_ASSERT(true);
  }

  // printf ("In PreprocessingInfo (constructor): dt = %d line_no = %d col_no =
  // %d nol = %d s = %s \n",dt,line_no,col_no,nol,inputString.c_str());
  file_info = new Sg_File_Info(inputFileName, line_no, col_no);

  // DQ (12/23/2006): Mark this as a comment or directive (mostly so that we can
  // know that the parent being NULL is not meaningful in the AST consistancy
  // tests).
  file_info->setCommentOrDirective();

  // DQ (3/7/2010): Switch this is a SgTypeDefault since one of these are
  // referenced in the generated rose_required_macros_and_functions.h which
  // means that it will always be formally in the AST.  This may fix (or help
  // fix) a bug in the AST file I/O where nodes not properly connected to the
  // AST don't appear to get there global index and freepointer set properly.
  // DQ (6/13/2007): Set the parent to a shared type for now so that it is at
  // least set to a non-null value This can if we like to used as a signature
  // for Sg_File_Info nodes that are associated with comments and directives.
  // file_info->set_parent(file_info);
  // file_info->set_parent(SgTypeShort::get_builtin_type());
  // file_info->set_parent(SgTypeLongLong::createType());
  file_info->set_parent(SgTypeDefault::createType());

  // DQ (1/15/2015): Adding support for token-based unparsing, initialization of
  // new data member.
  p_isTransformation = false;

  // DQ (4/15/2007): Temp code to trace common position in unparsing.
  // internalString = inputString;
  // Normal code
  internalString = inputString;

  // DQ (1/13/2014): Added checking for logic to compute macro name for #define
  // macros.
  if (whatSortOfDirective ==
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    string name = getMacroName();
  }
}

// Copy constructor
PreprocessingInfo::PreprocessingInfo(const PreprocessingInfo &prepInfo) {
  ROSE_ASSERT(prepInfo.file_info != NULL);
  file_info = new Sg_File_Info(*(prepInfo.file_info));

  // DQ (12/23/2006): Mark this as a comment or directive (mostly so that we can
  // know that the parent being NULL is not meaningful.
  file_info->setCommentOrDirective();

  // lineNumber          = prepInfo.getLineNumber();
  // columnNumber        = prepInfo.getColumnNumber();
  numberOfLines = prepInfo.getNumberOfLines();
  whatSortOfDirective = prepInfo.getTypeOfDirective();
  relativePosition = prepInfo.getRelativePosition();
  internalString = prepInfo.internalString;

  // DQ (1/15/2015): Adding support for token-based unparsing, initialization of
  // new data member.
  p_isTransformation = prepInfo.p_isTransformation;

  // DQ (1/13/2014): Added checking for logic to compute macro name for #define
  // macros.
  if (whatSortOfDirective ==
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    string name = getMacroName();
  }
}

PreprocessingInfo::~PreprocessingInfo() {
  ASSERT_this();

  // Reset these values so that they are not set to zero (a valid value) if a
  // PreprocessingInfo object is reused
  delete file_info;
  file_info = NULL;
  // lineNumber          = -1;
  // columnNumber        = -1;
  numberOfLines = -1;
  relativePosition = undef;
  whatSortOfDirective = CpreprocessorUnknownDeclaration;
  internalString = "";

  // DQ (1/15/2015): Adding support for token-based unparsing, initialization of
  // new data member.
  p_isTransformation = false;
}

/* starting column == 1 (DQ (10/27/2006): used to be 0, but changed to 1 for
 * consistancy with legacy frontend) */
int PreprocessingInfo::getColumnNumberOfEndOfString() const {
  ASSERT_this();
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
  ASSERT_this();
  return whatSortOfDirective;
}

void PreprocessingInfo::setTypeOfDirective(
    PreprocessingInfo::DirectiveType dt) {
  // Access function for the type of directive
  ASSERT_this();
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
  case ClinkageSpecificationStart:
    returnString = "ClinkageSpecificationStart";
    break;
  case ClinkageSpecificationEnd:
    returnString = "ClinkageSpecificationEnd";
    break;
  case CpreprocessorErrorDeclaration:
    returnString = "CpreprocessorErrorCDeclaration";
    break;
  case CpreprocessorWarningDeclaration:
    returnString = "CpreprocessorWarningDeclaration";
    break;
  case CpreprocessorEmptyDeclaration:
    returnString = "CpreprocessorEmptyCDeclaration";
    break;
  case CSkippedToken:
    returnString = "CSkippedToken";
    break;
  case CMacroCall:
    returnString = "CMacroCall";
    break;
  case LineReplacement:
    returnString = "LineReplacement";
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
  ASSERT_this();
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_line();
  // return lineNumber;
}

int PreprocessingInfo::getColumnNumber() const {
  ASSERT_this();
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_col();
  // return columnNumber;
}

// DQ (2/27/2019): Adding support for CPP directives and comments to have
// filename information (already present, but we need to access it).
std::string PreprocessingInfo::getFilename() const {
  ASSERT_this();
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_filenameString();
}

// DQ (2/27/2019): Adding support for CPP directives and comments to have
// filename information (already present, but we need to access it).
int PreprocessingInfo::getFileId() const {
  ASSERT_this();
  ROSE_ASSERT(file_info != NULL);
  return file_info->get_file_id();
}

string PreprocessingInfo::getString() const {
  ASSERT_this();
  return internalString;
}

void PreprocessingInfo::setString(const std::string &s) {
  ASSERT_this();
  internalString = s;
}

int PreprocessingInfo::getNumberOfLines() const {
  ASSERT_this();
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

void PreprocessingInfo::display(const string &label) const {
  printf("\n");
  printf("Inside of PreprocessingInfo display(%s): \n", label.c_str());
  ASSERT_this();
  file_info->display(label);
  // printf ("     lineNumber     = %d \n",lineNumber);
  // printf ("     columnNumber   = %d \n",columnNumber);
  printf("     numberOfLines  = %d \n", numberOfLines);
  printf("     relativePosition = %s \n",
         relativePositionName(relativePosition).c_str());
  printf("     directiveType  = %s \n",
         directiveTypeName(whatSortOfDirective).c_str());
  printf("     internalString = %s \n", internalString.c_str());
  printf("\n");
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
  ASSERT_this();

  return relativePosition;
}

void PreprocessingInfo::setRelativePosition(RelativePositionType relPos) {
  ASSERT_this();

  relativePosition = relPos;
}

int PreprocessingInfo::getStringLength(void) const {
  ASSERT_this();

  return internalString.length();
}

Sg_File_Info *PreprocessingInfo::get_file_info() const {
  ASSERT_this();

  ROSE_ASSERT(file_info != NULL);
  return file_info;
}

void PreprocessingInfo::set_file_info(Sg_File_Info *info) {
  ASSERT_this();

  file_info = info;
}

// DQ (8/26/2020): include directive have a filename imbedded inside, and we
// need to extract that for from tools (e.g. the fixup for initializers from
// include files).
std::string PreprocessingInfo::get_filename_from_include_directive() {
  std::string s;

  // s = "#line 12345\"foobar.h\"6789";
  // s = "#line \"foobar.h\"";
  // s = "#line <foobar.h>";
  // s = "#line 12345<foobar.h>6789";

  if (this->getTypeOfDirective() == CpreprocessorIncludeDeclaration) {
    // std::string line = s;
    std::string line = internalString;
    std::string name;

    std::string tester = "\"";

    size_t findPos = line.find(tester); // finds first quote mark

    if (findPos == string::npos) {
      tester = "<";
      findPos = line.find(tester); // finds first quote mark
      tester = ">";
    }

    size_t findPos2 = line.find(tester, findPos + 1); // finds second quote mark
    ROSE_ASSERT(findPos2 > findPos);
    name = line.substr(findPos + 1,
                       (findPos2 - findPos) - 1); // copies the name into name

    // printf ("before adding terminal: name = |%s| \n",name.c_str());
    s = name;
  } else {
    printf(
        "Error: In PreprocessingInfo::get_filename_from_include_directive(): "
        "getTypeOfDirective != CpreprocessorIncludeDeclaration \n");
    ROSE_ABORT();
  }

  return s;
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

// DQ (1/19/2014): List the acceptable leading possible characters to any CPP
// macro only once to aboud errors.
#define CPP_MACRO_ALPHABET                                                     \
  "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

std::string PreprocessingInfo::getMacroName() {
  // This function is only supporting the retrival of the macro name for #define
  // macros (all other cases are an error trapped below).

#define DEBUG_MACRO_NAME 0

  std::string macroName = "unknown CPP directive";
  if (this->getTypeOfDirective() ==
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    string s = internalString;
    string defineSubString = "define";
#if DEBUG_MACRO_NAME
    printf("s = %s \n", s.c_str());
#endif
    size_t lengthOfDefineSubstring = defineSubString.length();
    size_t startOfDefineSubstring = s.find(defineSubString);

    // DQ (1/13/2014): trap out cases where the CPP directive has been marked as
    // a #defin CPP directive, but does not contain the substring "define".
    // ROSE_ASSERT(startOfDefineSubstring != string::npos);
    if (startOfDefineSubstring != string::npos) {
      size_t endOfDefineSubstring =
          startOfDefineSubstring + lengthOfDefineSubstring;
#if DEBUG_MACRO_NAME
      printf("   --- startOfDefineSubstring = %" PRIuPTR
             " endOfDefineSubstring = %" PRIuPTR " \n",
             startOfDefineSubstring, endOfDefineSubstring);
#endif
      string substring = s.substr(endOfDefineSubstring);

      string cpp_macro_alphabet = CPP_MACRO_ALPHABET;
      ROSE_ASSERT(cpp_macro_alphabet.length() == 53);

      // size_t startOfMacroName =
      // s.find_first_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",endOfDefineSubstring);
      size_t startOfMacroName =
          s.find_first_of(cpp_macro_alphabet, endOfDefineSubstring);
      size_t endOfMacroName = s.find_first_of(" (\t", startOfMacroName);
#if DEBUG_MACRO_NAME
      printf("   --- startOfMacroName = %" PRIuPTR " endOfMacroName = %" PRIuPTR
             " \n",
             startOfMacroName, endOfMacroName);
#endif
      // DQ (1/19/2014): Added assertion.
      ROSE_ASSERT(startOfMacroName != string::npos);

      size_t macroNameLength = (endOfMacroName - startOfMacroName);
#if DEBUG_MACRO_NAME
      printf("   --- macroNameLength = %" PRIuPTR " \n", macroNameLength);
#endif
      macroName = s.substr(startOfMacroName, macroNameLength);
#if DEBUG_MACRO_NAME
      printf("   --- macroName = %s \n", macroName.c_str());
#endif
    } else {
      printf("WARNING: In PreprocessingInfo::getMacroName(): 'define' keyword "
             "not identified in CpreprocessorDefineDeclaration type CPP "
             "directive: returning 'unknown' \n");
    }
  } else {
    // DQ (12/30/2013): I think I want this to be an error for now.
    printf("ERROR: In PreprocessingInfo::getMacroName(): "
           "(this->getTypeOfDirective() != "
           "PreprocessingInfo::CpreprocessorDefineDeclaration): returning "
           "error -- %s \n",
           macroName.c_str());
    ROSE_ABORT();
  }

  return macroName;
}

bool PreprocessingInfo::isSelfReferential() {
  // DQ (12/30/2013): Adding support to supress output of macros that are
  // self-referential. e.g. "#define foo X->foo", which would be expanded a
  // second time in the backend processing. Note that if we don't output the
  // #define, then we still might have a problem if there was code that depended
  // upon a "#ifdef foo".  So this handling is not without some risk, but it
  // always better to use the token stream unparsing for these cases.

#define DEBUG_SELF_REFERENTIAL_MACRO 0

  bool result = true;

  if (this->getTypeOfDirective() ==
      PreprocessingInfo::CpreprocessorDefineDeclaration) {
    result = true;
    string macroName = getMacroName();
#if DEBUG_SELF_REFERENTIAL_MACRO
    printf("   --- macroName = %s macroName.length() = %" PRIuPTR " \n",
           macroName.c_str(), macroName.length());
#endif
    string s = internalString;

    // DQ (1/13/2014):if the macro name is "n" then the "n" in "define" will be
    // found by mistake.
    string defineSubstring = "define";
    size_t startOfMacro_define_Substring = s.find(defineSubstring);

    // DQ (1/13/2014): This case could happen if we had a macro marked as
    // #define, but it was not properly formed (getMacroName() returns "unknown
    // CPP directive"). ROSE_ASSERT(startOfMacroSubstring != string::npos);
    if (startOfMacro_define_Substring == string::npos) {
      printf(
          "WARNING: In PreprocessingInfo::isSelfReferential(): (return false): "
          "\"define\" substring not found in CPP #define directitve = %s \n",
          s.c_str());
      return false;
    }

    size_t endOfMacro_define_Substring =
        startOfMacro_define_Substring + defineSubstring.length();
#if DEBUG_SELF_REFERENTIAL_MACRO
    printf("   --- startOfMacro_define_Substring = %" PRIuPTR
           " endOfMacro_define_Substring = %" PRIuPTR " \n",
           startOfMacro_define_Substring, endOfMacro_define_Substring);
#endif

    // DQ (1/13/2014):if the macro name is "n" then the "n" in "define" will be
    // found by mistake. size_t startOfMacroSubstring  = s.find(macroName);
    size_t startOfMacroSubstring =
        s.find(macroName, endOfMacro_define_Substring);

    // DQ (1/13/2014): This case could happen if we had a macro marked as
    // #define, but it was not properly formed (getMacroName() returns "unknown
    // CPP directive"). ROSE_ASSERT(startOfMacroSubstring != string::npos);
    if (startOfMacroSubstring == string::npos) {
      printf(
          "WARNING: In PreprocessingInfo::isSelfReferential(): (return false): "
          "macroName = %s not found in CPP #define directitve = %s \n",
          macroName.c_str(), s.c_str());
      return false;
    }

    size_t endOfMacroSubstring =
        startOfMacroSubstring + (macroName.length() - 1);
#if DEBUG_SELF_REFERENTIAL_MACRO
    printf("   --- startOfMacroSubstring = %" PRIuPTR
           " endOfMacroSubstring = %" PRIuPTR " \n",
           startOfMacroSubstring, endOfMacroSubstring);
#endif
    // size_t secondReferenceToMacroSubstring =
    // s.find(macroName,endOfMacroSubstring);
    size_t secondReferenceToMacroSubstring =
        s.find(macroName, endOfMacroSubstring + 1);
#if DEBUG_SELF_REFERENTIAL_MACRO
    printf("   --- secondReferenceToMacroSubstring = %" PRIuPTR " \n",
           secondReferenceToMacroSubstring);
#endif
    result = (secondReferenceToMacroSubstring != string::npos);
#if DEBUG_SELF_REFERENTIAL_MACRO
    printf("   --- result = %s \n", result ? "true" : "false");
#endif

    if (secondReferenceToMacroSubstring != string::npos) {
      string cpp_macro_alphabet = CPP_MACRO_ALPHABET;
      ROSE_ASSERT(cpp_macro_alphabet.length() == 53);

#if DEBUG_SELF_REFERENTIAL_MACRO
      printf("   --- Double check for self-referencing macro: macroName = %s s "
             "= %s ",
             macroName.c_str(), s.c_str());
#endif
      // DQ (1/9/2014): Detect a prefix at the start of the second referenced
      // string to make sure it is not embedded in another string. e.g. test for
      // cases such as "#define ABC __ABC" which is not a self-referential
      // macro.
      size_t characterBeforeSecondReferenceToMacroSubstring =
          secondReferenceToMacroSubstring - 1;
      ROSE_ASSERT(endOfMacroSubstring <
                  characterBeforeSecondReferenceToMacroSubstring);
      string beforeSecondReferenceToMacroSubstring =
          s.substr(endOfMacroSubstring + 1,
                   (characterBeforeSecondReferenceToMacroSubstring -
                    endOfMacroSubstring));
      // size_t find_last_not_of (const string& str, size_t pos = npos) const;
      // size_t characterBeforeSecondReferenceToMacroSubstring =
      // s.find_last_not_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",characterBeforeSecondReferenceToMacroSubstring);
      // size_t nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring =
      // beforeSecondReferenceToMacroSubstring.find_last_not_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
      size_t nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring =
          beforeSecondReferenceToMacroSubstring.find_last_not_of(
              cpp_macro_alphabet);
#if DEBUG_SELF_REFERENTIAL_MACRO
      printf("   --- characterBeforeSecondReferenceToMacroSubstring            "
             "  = %" PRIuPTR " \n",
             characterBeforeSecondReferenceToMacroSubstring);
      printf("   --- beforeSecondReferenceToMacroSubstring                     "
             "  = %s \n",
             beforeSecondReferenceToMacroSubstring.c_str());
      printf(
          "   --- nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring "
          "= %" PRIuPTR " \n",
          nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring);
#endif
      ROSE_ASSERT(nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring !=
                  string::npos);
      size_t
          nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring_relativeToInternalString =
              nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring +
              endOfMacroSubstring + 1;
#if DEBUG_SELF_REFERENTIAL_MACRO
      printf(
          "   --- "
          "nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring_"
          "relativeToInternalString = %" PRIuPTR " \n",
          nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring_relativeToInternalString);
#endif
      // if (nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring <
      // characterBeforeSecondReferenceToMacroSubstring)
      if (nonWhiteSpaceCharacterBeforeSecondReferenceToMacroSubstring_relativeToInternalString <
          characterBeforeSecondReferenceToMacroSubstring) {
        // This is a case like: "#define ABC __ABC" which is not a
        // self-referential macro.
#if DEBUG_SELF_REFERENTIAL_MACRO
        printf("   --- Detected case of macro renaming: \"#define ABC __ABC\": "
               "not a self-referencing macro (set result = false) \n");
#endif
        result = false;
      }

      // DQ (1/9/2014): Detect a suffix on the macro name such that it would be
      // a renamed macro instead of a self-referential macro.
      ROSE_ASSERT(secondReferenceToMacroSubstring != string::npos);
      size_t endOfSecondReferenceToMacroSubstring =
          secondReferenceToMacroSubstring + macroName.length() - 1;
      ROSE_ASSERT(endOfSecondReferenceToMacroSubstring != string::npos);
      ROSE_ASSERT(endOfSecondReferenceToMacroSubstring <= s.length());
#if DEBUG_SELF_REFERENTIAL_MACRO
      printf("   --- endOfSecondReferenceToMacroSubstring = %" PRIuPTR " \n",
             endOfSecondReferenceToMacroSubstring);
#endif
      string afterSecondReferenceToMacroSubstring =
          s.substr(endOfSecondReferenceToMacroSubstring + 1,
                   (s.length() - endOfSecondReferenceToMacroSubstring));
#if DEBUG_SELF_REFERENTIAL_MACRO
      printf("   --- afterSecondReferenceToMacroSubstring = %s \n",
             afterSecondReferenceToMacroSubstring.c_str());
#endif
      // size_t startOfRemainderSubstring =
      // s.find_first_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",endOfSecondReferenceToMacroSubstring+1);
      size_t startOfRemainderSubstring = s.find_first_of(
          cpp_macro_alphabet, endOfSecondReferenceToMacroSubstring + 1);
      size_t endOfRemainderSubstring =
          s.find_first_of(" (\t\n\0", endOfSecondReferenceToMacroSubstring);
#if DEBUG_SELF_REFERENTIAL_MACRO
      printf("   --- startOfRemainderSubstring = %" PRIuPTR
             " endOfRemainderSubstring = %" PRIuPTR " \n",
             startOfRemainderSubstring, endOfRemainderSubstring);
#endif
      // DQ (1/2/2014): Handle the special case of macro pasting "#define foo(X)
      // foo##X" if (s[endOfSecondReferenceToMacroSubstring+1] == '#' &&
      // s[endOfSecondReferenceToMacroSubstring+2] == '#')
      if (startOfRemainderSubstring < endOfRemainderSubstring) {
        // Detected case of macro pasting.  since the secondary reference to the
        // macro name is modified to be different from the primary macro name
        // this is not a case of self-referencing macro.
#if DEBUG_SELF_REFERENTIAL_MACRO
        printf("   --- Detected case of macro pasting, not a self-referencing "
               "macro (set result = false) \n");
#endif
        result = false;
      } else {
        // Detect second kind of macro pasting.

        // DQ (1/6/2014): Macro pasting is used in libwww application in the
        // forms:
        //    --- #define NS(x) x ## NS
        //    --- #define ns(x) x ## _ns
        // And we have to allow this since it does not appear to build a
        // self-referenced macro name.
        size_t startOfPastingSubstring = s.find("##");
        if (startOfPastingSubstring != string::npos) {
#if DEBUG_SELF_REFERENTIAL_MACRO
          printf("   --- Detected 2nd kind of case of macro pasting, not a "
                 "self-referencing macro (set result = false) \n");
#endif
          result = false;
        }
      }
    }
  } else {
    // We might want test for #ifdef that was associated with an ignored
    // #define...but for now we ignore this case.
    result = false;
  }

  return result;
}

// DQ (1/15/2015): Adding support for token-based unparsing. Access function for
// new data member.
bool PreprocessingInfo::isTransformation() const {
  ASSERT_this();
  return p_isTransformation;
}

void PreprocessingInfo::setAsTransformation() {
  ASSERT_this();
  p_isTransformation = true;
}

void PreprocessingInfo::unsetAsTransformation() {
  ASSERT_this();
  p_isTransformation = false;
}

// *********************************************
// Member functions for class ROSEATTRIBUTESList
// *********************************************

ROSEAttributesList::ROSEAttributesList() {
  index = 0;

  // DQ (9/29/2013): Added initialization of data members (when using
  // token-based preprocessing this data member was not being set before
  // being tested). Note: data members: attributeList, fileName, and
  // filenameIdSet will default to proper values using there default
  // constrcutors.
  rawTokenStream = NULL;

  // DQ (1/15/2015): Adding support for token-based unparsing, initialization of
  // new data member. p_isTransformation = false;
}

ROSEAttributesList::~ROSEAttributesList() { deepClean(); }

void ROSEAttributesList::addElement(PreprocessingInfo::DirectiveType dt,
                                    const std::string &pLine,
                                    const std::string &filename, int lineNumber,
                                    int columnNumber, int numOfLines) {
  ASSERT_this();
  // ROSE_ASSERT(pLine != NULL);

  // DQ (10/28/2007): An empty file name is now allowed (to handle #line 1 ""
  // directives) ROSE_ASSERT(filename.empty() == false);
  ROSE_ASSERT(pLine.empty() == false);
  ROSE_ASSERT(lineNumber > 0);
  ROSE_ASSERT(columnNumber > 0);
  ROSE_ASSERT(numOfLines >= 0); // == 0, if cpp_comment in a single line
  // PreprocessingInfo *pElem = new PreprocessingInfo(dt, pLine, filename,
  // lineNumber, columnNumber, numOfLines, PreprocessingInfo::undef, false,
  // false);
  PreprocessingInfo *pElem =
      new PreprocessingInfo(dt, pLine, filename, lineNumber, columnNumber,
                            numOfLines, PreprocessingInfo::undef);

  // PreprocessingInfo &pRef = *pElem;
  // insertElement(pRef);
  attributeList.push_back(pElem);
}

// DQ (9/29/2013): Added to support adding processed CPP directives and comments
// as tokens to token list.
PreprocessingInfo *ROSEAttributesList::lastElement() {
  ASSERT_this();

  ROSE_ASSERT(attributeList.empty() == false);

  return attributeList.back();
}

void ROSEAttributesList::moveElements(ROSEAttributesList &pList) {
  ASSERT_this();

  int length = pList.size();
  if (length > 0) {
    vector<PreprocessingInfo *>::iterator i = pList.attributeList.begin();
    for (i = pList.attributeList.begin(); i != pList.attributeList.end(); i++) {
      length = pList.getLength();
      // PreprocessingInfo *pElem = new PreprocessingInfo((*i)->stringPointer,
      // (*i)->lineNumber, (*i)->columnNumber); PreprocessingInfo & pRef =
      // *pElem; DQ (4/13/2007): Skip the insertElement() which requires a
      // traversal over the list, we are building this in order so the order is
      // preserved in copying from pList. This is a performance optimization.
      PreprocessingInfo &pRef = *(*i);
      attributeList.push_back(&pRef);
    }

    // empty the STL list
    vector<PreprocessingInfo *>::iterator head = pList.attributeList.begin();
    vector<PreprocessingInfo *>::iterator tail = pList.attributeList.end();
    pList.attributeList.erase(head, tail);
    ROSE_ASSERT(pList.attributeList.size() == 0);
  }
}

void ROSEAttributesList::setFileName(const string &fName) {
  // DQ (10/4/2013): This function is called by the
  // EasyStorage<ROSEAttributesList>::rebuildDataStoredInEasyStorageClass()
  // which is called as part of the AST File I/O (AST serialization).  It was
  // not previously called until more information was added to the AST (likely
  // as part of the new token stream support for parse tree reconstruction in
  // ROSE).

  ASSERT_this();

  // Should have an assert(fName!=NULL) here?
  // strcpy(fileName,fName);
  fileName = fName;

  // TV (11/19/2018): ROSE-1470: with File I/O, SgFile (and contained
  // ROSEAttributesList) are loaded before Sg_File_Info causing issues....
}

string ROSEAttributesList::getFileName() {
  ASSERT_this();
  return fileName;
}

void ROSEAttributesList::setIndex(int i) {
  ASSERT_this();
  index = i;
}

int ROSEAttributesList::getIndex() {
  ASSERT_this();
  return index;
}

int ROSEAttributesList::size(void) {
  ASSERT_this();
  return getLength();
}

int ROSEAttributesList::getLength(void) {
  ASSERT_this();
  return attributeList.size();
}

void ROSEAttributesList::clean(void) {
  ASSERT_this();

  for (PreprocessingInfo *info : attributeList) {
    delete info;
  }
  attributeList.clear();
}

void ROSEAttributesList::deepClean(void) {
  ASSERT_this();

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
  ASSERT_this();
  return attributeList[i];
}

void ROSEAttributesList::display(const string &label) {
  printf("ROSEAttributesList::display (label = %s): size = %zu \n",
         label.c_str(), attributeList.size());
  ASSERT_this();

  // fprintf(outFile,"\n%s: \n", getFileName() );
  vector<PreprocessingInfo *>::iterator j = attributeList.begin();
  for (j = attributeList.begin(); j != attributeList.end(); j++) {
    // printf("  %s\n",( (*j)->stringPointer );

    // DQ (12/19/2008): Modified to report NULL pointers
    // ROSE_ASSERT ( (*j) != NULL );
    // printf("LineNumber: %5d:
    // %s\n",(*j)->getLineNumber(),(*j)->getString().c_str());
    printf("-----------------------\n");
    if (*j != NULL) {
      printf(
          "Directive Type: %s; Relative position: %s; \nLine:%5d; Column:%5d; "
          "String: %s\n",
          PreprocessingInfo::directiveTypeName((*j)->getTypeOfDirective())
              .c_str(),
          PreprocessingInfo::relativePositionName((*j)->getRelativePosition())
              .c_str(),
          (*j)->getLineNumber(), (*j)->getColumnNumber(),
          (*j)->getString().c_str());
    } else {
      printf("Warning: PreprocessingInfo *j == NULL \n");
    }
  }

  printf("END: ROSEAttributesList::display (label = %s) \n", label.c_str());
}

void ROSEAttributesList::set_rawTokenStream(LexTokenStreamTypePointer s) {
  ASSERT_this();
  rawTokenStream = s;
}

LexTokenStreamTypePointer ROSEAttributesList::get_rawTokenStream() {
  ASSERT_this();
  return rawTokenStream;
}

// void ROSEAttributesList::generatePreprocessorDirectivesAndCommentsForAST(
// SgFile* file )
void ROSEAttributesList::generatePreprocessorDirectivesAndCommentsForAST(
    const string &filename) {
  // This function does not work for fixed-format, which is processed
  // separately. This function reads the token stream and extracts out the
  // comments for inclusion into the attributeList.

  ASSERT_this();
  ROSE_ASSERT(filename.empty() == false);

  printf("This is an old version of the function to collect CPP directives and "
         "comments \n");
  ROSE_ABORT();

  ROSE_ASSERT(rawTokenStream != NULL);

  if (attributeList.empty() == false) {
    // Detect where these these have been previously built using a mechanism we
    // are testing. Delete the entries built by the expermiental mechanism and
    // use the previous approach. This allows for the new mechanism to be widely
    // tested in C, C++, and Fortran.

    printf("attributeList has already been build, remove the existing entries "
           "attributeList.size() = %" PRIuPTR " \n",
           attributeList.size());
    std::vector<PreprocessingInfo *>::iterator i = attributeList.begin();
    while (i != attributeList.end()) {
      delete *i;
      i++;
    }
    attributeList.clear();
  }
  ROSE_ASSERT(attributeList.empty() == true);

  LexTokenStreamType::iterator i = rawTokenStream->begin();
  while (i != rawTokenStream->end()) {
    token_element *token = (*i)->p_tok_elem;
    ROSE_ASSERT(token != NULL);
    file_pos_info &start = (*i)->beginning_fpi;

    bool isComment = (token->token_id == SgToken::FORTRAN_COMMENTS);
    if (isComment == true) {
      int numberOfLines = 1;
      PreprocessingInfo *comment = new PreprocessingInfo(
          PreprocessingInfo::FortranStyleComment, token->token_lexeme, filename,
          start.line_num, start.column_num, numberOfLines,
          PreprocessingInfo::before);
      ROSE_ASSERT(comment != NULL);
      attributeList.push_back(comment);
    }

    i++;
  }
}

bool ROSEAttributesList::isFortran90Comment(const string &line) {
  // This refactored code test if a line is a fortran comment.
  // Fortran 90 comments are more complex to recognise than
  // F77.  This function only recognizes F90 comments that have
  // a leading "!".  Other uses of "!" at the end of a valid
  // Fortran statement are not yet captured, but that would be
  // handled by this function (later).

  bool isComment = false;

  char firstNonBlankCharacter = line[0];
  size_t i = 0;
  size_t lineLength = line.length();

  // Loop over any leading blank spaces.
  while (i < lineLength && firstNonBlankCharacter == ' ') {
    firstNonBlankCharacter = line[i];
    i++;
  }

  // The character "!" starts a comment if only blanks are in the leading white
  // space.
  if (firstNonBlankCharacter == '!') {
    // printf ("This is a F90 style comment: line = %s length = %" PRIuPTR "
    // \n",line.c_str(),line.length());
    isComment = true;
  }

  // return isFortran77Comment(line);
  return isComment;
}

bool ROSEAttributesList::isFortran77Comment(const string &line) {
  // This refactored code tests if a line is a fortran fixed format comment (it
  // maybe that it is less specific to F77). It is a very simple test on the
  // character in column zero, but there are a few details...

  // We handle CPP directives first and then comments, Fortran fixed format
  // comments should be easy. if there is a character in the first column, then
  // the whole line is a comment. Also, more subtle, if it is a blank line then
  // it is a comment, so save the blank lines too.

  bool isComment = false;

  char firstCharacter = line[0];
  if (firstCharacter != ' ' /* SPACE */ && firstCharacter != '\n' /* CR  */ &&
      firstCharacter != '\0' /* NUL   */ && firstCharacter != '\t' /* TAB */) {
    // This has something in the first column, so it might be a comment (check
    // further)...

    // Error checking on first character, I believe we can't enforce this, but I
    // would like to have it be a warning.
    if (!(firstCharacter >= ' ') || !(firstCharacter < 126)) {
      printf("Warning: firstCharacter = %d (not an acceptable character value "
             "for Fortran) line.length() = %" PRIuPTR " \n",
             (int)firstCharacter, line.length());
    }

    // Error checking on first character
    // DQ (5/15/2008): The filter is in the conditional above and is not
    // required to be repeated. ROSE_ASSERT(firstCharacter >= ' ' &&
    // firstCharacter < 126);

#define RELAXED_FORTRAN_COMMENT_SPECIFICATION 1
#if RELAXED_FORTRAN_COMMENT_SPECIFICATION
    // Most fortran compilers do not enforce the strinct langauge definition of
    // what a comment is so we have to handle the more relaxed comment
    // specification (which does not appear to be written down anywhere). Make
    // sure it is not part a number (which could be part of a Fortran label)
    if (firstCharacter >= '0' && firstCharacter <= '9') {
      // This is NOT a comment it is part of a label in the first column (see
      // test2008_03.f) Some compilers (gfortran) can interprete a lable even if
      // it starts in the first column (column 1 (fortran perspective) column 0
      // (C perspective)). printf ("This is not a comment, it is part of a label
      // in the first column: line = %s \n",line.c_str());
    } else {
      // DQ (11/19/2008): Commented this out since I can't understand
      // why it was here and it appears to mark everything as a comment!

      // This is position (column) 0 in the line, for F77 this means it is a
      // comment. Note that we check for CPP directives first and only then if
      // the line is not a CPP directive do we test for a F77 style comment, so
      // if the first character of the line is a '#' then it will only be
      // considered a comment if it is not a CPP directive.
      isComment = true;
    }
#else
    // DQ (1/22/2008): Separate from the F77 standard, no compiler is this
    // restrictive (unfortunately)! The Fortran 77 standard says: comments must
    // have a C or * in the first column (check for case)
    if (firstCharacter == 'C' || firstCharacter == 'c' ||
        firstCharacter == '*') {
      isComment = true;
    }
#endif
    // printf ("This is a comment! lineCounter = %d \n",lineCounter);
  }

  return isComment;
}

#define DEBUG_CPP_DIRECTIVE_COLLECTION 0

bool ROSEAttributesList::isCppDirective(
    const string &line, PreprocessingInfo::DirectiveType &cppDeclarationKind,
    std::string &restOfTheLine) {
  // This function tests if a string is a CPP directive (the first line of a CPP
  // directive).

  bool cppDirective = false;
  bool isLikelyCppDirective = false;

  char firstNonBlankCharacter = line[0];
  size_t i = 0;
  size_t lineLength = line.length();

  // Loop through any initial white space.
  while (i < lineLength && firstNonBlankCharacter == ' ') {
    firstNonBlankCharacter = line[i];
    i++;
  }

  // The character "!" starts a comment if only blanks are in the leading white
  // space.

  // DQ (12/9/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable.
#if DEBUG_CPP_DIRECTIVE_COLLECTION
  int positionofHashCharacter = -1;
#endif

  if (firstNonBlankCharacter == '#') {
#if DEBUG_CPP_DIRECTIVE_COLLECTION
    // printf ("This is a CPP directive: i = %d lineCounter = %d line = %s
    // length = %" PRIuPTR " \n",i,lineCounter,line.c_str(),line.length());
#endif
    isLikelyCppDirective = true;

    // DQ (12/9/2016): Eliminating a warning that we want to be an error:
    // -Werror=unused-but-set-variable.
#if DEBUG_CPP_DIRECTIVE_COLLECTION
    positionofHashCharacter = i;
#endif
  }

#if DEBUG_CPP_DIRECTIVE_COLLECTION
  printf("i = %" PRIuPTR " positionofHashCharacter = %d \n", i,
         positionofHashCharacter);
#endif

  // DQ (12/9/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable.
#if DEBUG_CPP_DIRECTIVE_COLLECTION
  bool hasLineContinuation = false;
#endif

  // DQ (4/21/2009): Fixed possible buffer underflow...
  // char lastCharacter = line[lineLength-1];
  char lastCharacter = (lineLength > 0) ? line[lineLength - 1] : '\0';
  if (lastCharacter == '\\') {
#if DEBUG_CPP_DIRECTIVE_COLLECTION
    // DQ (12/9/2016): Eliminating a warning that we want to be an error:
    // -Werror=unused-but-set-variable.
    hasLineContinuation = true;
#endif
  }

#if DEBUG_CPP_DIRECTIVE_COLLECTION
  printf("hasLineContinuation = %s \n", hasLineContinuation ? "true" : "false");
#endif

  // int numberOfLines = 1;

  if (isLikelyCppDirective == true) {
    // PreprocessingInfo(DirectiveType, const std::string & inputString, const
    // std::string & filenameString,
    //      int line_no , int col_no, int nol, RelativePositionType relPos, bool
    //      copiedFlag, bool unparsedFlag);

    // firstNonBlankCharacter = ' ';
    // printf ("firstNonBlankCharacter = %c \n",firstNonBlankCharacter);
    bool spaceAfterHash = false;

    // DQ (12/16/2008): Added support fo tabs between "#" and the directive
    // identifier. Note that Fortran modes of CPP should not allow any
    // whitespace here (at least for gfortran).
    while ((i < lineLength && (firstNonBlankCharacter == ' ' ||
                               firstNonBlankCharacter == '\t')) ||
           firstNonBlankCharacter == '#') {
#if DEBUG_CPP_DIRECTIVE_COLLECTION
      printf("Looping over # or white space between # and CPP directive i = "
             "%" PRIuPTR " \n",
             i);
#endif
      firstNonBlankCharacter = line[i];
      if (spaceAfterHash == false)
        spaceAfterHash = (firstNonBlankCharacter == ' ');

      i++;
    }

    int positionOfFirstCharacterOfCppIdentifier = i - 1;

#if DEBUG_CPP_DIRECTIVE_COLLECTION
    printf(
        "positionOfFirstCharacterOfCppIdentifier = %d spaceAfterHash = %s \n",
        positionOfFirstCharacterOfCppIdentifier,
        spaceAfterHash ? "true" : "false");
#endif
    // Need to back up one!
    i = positionOfFirstCharacterOfCppIdentifier;

    char nonBlankCharacter = line[positionOfFirstCharacterOfCppIdentifier];
    int positionOfLastCharacterOfCppIdentifier =
        positionOfFirstCharacterOfCppIdentifier;
    // while (i < lineLength &&
    // isLegalCharacterForCppIndentifier(nonBlankCharacter) == true))
    while (i <= lineLength &&
           (((nonBlankCharacter >= 'a' && nonBlankCharacter <= 'z') == true) ||
            (nonBlankCharacter >= '0' && nonBlankCharacter <= '9') == true)) {
      nonBlankCharacter = line[i];
#if DEBUG_CPP_DIRECTIVE_COLLECTION
      printf("In loop: i = %" PRIuPTR " lineLength = %" PRIuPTR
             " nonBlankCharacter = %c \n",
             i, lineLength,
             isprint(nonBlankCharacter) ? nonBlankCharacter : '.');
#endif
      i++;
    }

#if DEBUG_CPP_DIRECTIVE_COLLECTION
    printf("i = %" PRIuPTR " \n", i);
#endif

    // Need to backup two (for example if this is the end of the line, as in
    // "#endif")
    positionOfLastCharacterOfCppIdentifier = i - 2;

#if DEBUG_CPP_DIRECTIVE_COLLECTION
    printf("positionOfLastCharacterOfCppIdentifier = %d \n",
           positionOfLastCharacterOfCppIdentifier);
#endif
    int cppIdentifierLength = (positionOfLastCharacterOfCppIdentifier -
                               positionOfFirstCharacterOfCppIdentifier) +
                              1;
    string cppIndentifier = line.substr(positionOfFirstCharacterOfCppIdentifier,
                                        cppIdentifierLength);

    // Some names will convert to integer values
#if DEBUG_CPP_DIRECTIVE_COLLECTION
    long integerValue = -1;
#endif
    if (spaceAfterHash == true) {
      // This is likely going to be a number but test2005_92.C demonstrates a
      // case where this is not true.

      // printf ("firstNonBlankCharacter = %c \n",firstNonBlankCharacter);
      // ROSE_ASSERT(firstNonBlankCharacter == '\"');
      // The modern way to handle conversion of string to integer value is to
      // use strtol(), and not atoi().  But atoi() is simpler.
      const char *str = cppIndentifier.c_str();
      int size = strlen(str) + 1;
      char *buffer = new char[size];

      // Make a copy of the pointer so that we can always delete the memory that
      // was allocated.
      char *original_buffer = buffer;

      // We should initialize "buffer" to all Nul chars (this includes a null
      // terminator and the end of the string).
      for (int j = 0; j < size; j++)
        buffer[j] = '\0';

      // strtol will put the string into buffer if str is not a number and 2nd
      // parameter is not NULL.
      errno = 0;

      // integerValue = strtol(str,NULL,10);

      // DQ (12/9/2016): Eliminating a warning that we want to be an error:
      // -Werror=unused-but-set-variable. integerValue = strtol(str,&buffer,10);
      strtol(str, &buffer, 10);

      // Setting and checking errno does not appear to work for the detection of
      // errors in the use of strtol
      if (errno != 0) {
        printf(
            "Using errno: This was not a valid string (errno = %d returned) \n",
            errno);
      }

      bool isANumber = true;
      if (strcmp(str, buffer) == 0) {
        // printf ("Using strcmp(): This was not a valid string (buffer = %s
        // returned) \n",buffer);
        isANumber = false;
      }
      // printf ("cppIndentifier = %s integerValue = %ld
      // \n",cppIndentifier.c_str(),integerValue);

      // Avoid memory leak!

      // DQ (11/4/2016): This needs to be an array delete (caught by Address
      // Sanitizer). delete original_buffer;
      delete[] original_buffer;

      original_buffer = NULL;
      buffer = NULL;

      // This value will be a constant value used to identify a numerical value.
      // This value should be a macro defined in some centralized location.
      if (isANumber == true) {
        cppIndentifier = "numeric value";

        // Allow the line number to be a part of the restOfTheLine so it can be
        // processed separately. printf ("cppIdentifierLength = %d
        // \n",cppIdentifierLength); printf ("Before being reset:
        // positionOfLastCharacterOfCppIdentifier = %d
        // \n",positionOfLastCharacterOfCppIdentifier);
        positionOfLastCharacterOfCppIdentifier -= cppIdentifierLength;
        // printf ("After being reset: positionOfLastCharacterOfCppIdentifier =
        // %d \n",positionOfLastCharacterOfCppIdentifier);
      } else {
        // printf ("This is not a number: cppIndentifier = %s
        // \n",cppIndentifier.c_str());
      }
    }

#if DEBUG_CPP_DIRECTIVE_COLLECTION
    printf("cppIdentifierLength = %d cppIndentifier = %s integerValue = %ld \n",
           cppIdentifierLength, cppIndentifier.c_str(), integerValue);
#endif

    // classify the CCP directive
    if (cppIndentifier == "include") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorIncludeDeclaration;
    }
    // Is it "includenext" or "include_next", we need more agressive tests!
    else if (cppIndentifier == "includenext") {
      cppDeclarationKind =
          PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    } else if (cppIndentifier == "define") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorDefineDeclaration;
    } else if (cppIndentifier == "undef") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorUndefDeclaration;
    } else if (cppIndentifier == "ifdef") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorIfdefDeclaration;
    } else if (cppIndentifier == "ifndef") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorIfndefDeclaration;
    } else if (cppIndentifier == "if") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorIfDeclaration;
    } else if (cppIndentifier == "else") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorElseDeclaration;
    } else if (cppIndentifier == "elif") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorElifDeclaration;
    } else if (cppIndentifier == "endif") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorEndifDeclaration;
    } else if (cppIndentifier == "end if") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorEnd_ifDeclaration;
    } else if (cppIndentifier == "line") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorLineDeclaration;
    } else if (cppIndentifier == "error") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorErrorDeclaration;
    } else if (cppIndentifier == "warning") {
      cppDeclarationKind = PreprocessingInfo::CpreprocessorWarningDeclaration;
    } else if (cppIndentifier == "pragma") {
      // Ignore case of #pragma, since it is not a CPP directive and is handled
      // by the C language definition only.
      cppDeclarationKind = PreprocessingInfo::CpreprocessorUnknownDeclaration;
    } else if (cppIndentifier == "ident") {
      // Ignore case of #ident
      cppDeclarationKind = PreprocessingInfo::CpreprocessorIdentDeclaration;
    }
    // Recognize the case of a numeric value...set if there was white space
    // following the '#' and then a numeric (integer) value.
    else if (cppIndentifier == "numeric value") {
      // DQ (11/17/2008): This handles the case CPP declarations
      // such as: "# 1 "test2008_05.F90"", "# 1 "<built-in>"",
      // "# 1 "<command line>"" "# 1 "test2008_05.F90""
      cppDeclarationKind =
          PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker;
    } else {
      // This case should be an error...
      // Liao, 5/13/2009
      // This should not be an error. Any weird string can show up in a block of
      // /* */ Check the test input:
      // tests/nonsmoke/functional/CompileTests/C_tests/test2009_01.c
      cppDeclarationKind = PreprocessingInfo::CpreprocessorUnknownDeclaration;
    }

    // Collect the rest of the line: (line length - next character position)
    // + 1.
    int restOfTheLineLength =
        (lineLength - (positionOfLastCharacterOfCppIdentifier + 1)) + 1;
    restOfTheLine = line.substr(positionOfLastCharacterOfCppIdentifier + 1,
                                restOfTheLineLength);

    // Set the return value
    if (cppDeclarationKind !=
        PreprocessingInfo::CpreprocessorUnknownDeclaration) {
      cppDirective = true;
    }
  }

  return cppDirective;
}

void ROSEAttributesList::collectPreprocessorDirectivesAndCommentsForAST(
    const string &filename, ROSEAttributesList::languageTypeEnum languageType) {
  // This is required for Fortran, but is redundant for C and C++.
  // This is a more direct approach to collecting the CPP directives, where as
  // for C and C++ we have had a solution (using lex) and a second (superior)
  // solution using the token stream pipeline, the Fortran support for CPP is
  // not addressed properly by the existing lex approach (and the token stream
  // pipeline does not apply to Fortran). Thus we have implemented a more direct
  // collection of CPP directives to support the requirements of Fortran CPP
  // handling (such files have a suffix such as: "F", "F90", "F95", "F03",
  // "F08".

  // The lex pass for free-format Fortran collects comments properly, but does
  // not classify CPP directives properly. So maybe we should just extract them
  // separately in an other pass over the file.  Also if we separate out the
  // recognition of CPP directives from comments this function may be useful for
  // the fix format CPP case. CPP directives should also be easier than a lot of
  // other token recognition.

  // printf ("This function
  // ROSEAttributesList::collectFreeFormatPreprocessorDirectivesAndCommentsForAS():
  // is not implemented yet! \n"); ROSE_ABORT();

  ASSERT_this();

  ROSE_ASSERT(filename.empty() == false);

  // Open file for reading line by line!
  const PreprocessingInfo::DirectiveType commentKind =
      PreprocessingInfo::FortranStyleComment;
  string line;
  size_t colNumber = 0;

#if DEBUG_CPP_DIRECTIVE_COLLECTION
  printf(
      "In ROSEAttributesList::collectPreprocessorDirectivesAndCommentsForAST: "
      "Opening file %s for reading comments and CPP directives \n",
      filename.c_str());
#endif

  ifstream targetFile(filename.c_str());
  if (targetFile.is_open()) {
    // The first line is defined to be line 1, line zero does not exist  and is
    // an error value. This synch's the line numbering convention of the OFP
    // with the line numbering convention for CPP directives and comments.
    int lineCounter = 1;
    while (targetFile.eof() == false) {
#if DEBUG_CPP_DIRECTIVE_COLLECTION
      printf("\nAt top of loop over lines in the file ... lineCounter = %d \n",
             lineCounter);
#endif
      getline(targetFile, line);

#if DEBUG_CPP_DIRECTIVE_COLLECTION
      // Debugging output
      cout << "collect CPP directives: " << line << endl;
#endif
      int numberOfLines = 1;

      string restOfTheLine;
      PreprocessingInfo::DirectiveType cppDeclarationKind =
          PreprocessingInfo::CpreprocessorUnknownDeclaration;
      bool cppDirective =
          isCppDirective(line, cppDeclarationKind, restOfTheLine);

#if DEBUG_CPP_DIRECTIVE_COLLECTION
      // printf ("cppDirective = %s \n",cppDirective ? "true" : "false");
      printf("cppDirective = %s cppDeclarationKind = %s \n",
             cppDirective ? "true" : "false",
             PreprocessingInfo::directiveTypeName(cppDeclarationKind).c_str());
#endif

      if (cppDirective == true) {
        if (line[line.length() - 1] == '\\') {
          string nextLine;
          while (line[line.length() - 1] == '\\') {
            getline(targetFile, nextLine);

            // Add linefeed to force nextLine onto the next line when output.
            line += "\n" + nextLine;
          }
        }

        // printf ("After processing continuation lines: line.length() = %"
        // PRIuPTR " line = %s \n",line.length(),line.c_str());
      }

      // DQ (11/17/2008): Refactored the code to make it simpler to add here!
      // If this is not a CPP directive, then check if it is a comment (note
      // that for Fortran (for fixed format), a CPP directive could be
      // identified as a comment so we have to check for CPP directives first.
      if (cppDirective == false) {
        bool isComment = false;

        // Used switch to provide room for additional frontends if
        // needed. Note that C permits multiple comments on a single
        // line, this is not addressed here.
        switch (languageType) {
          // case e_Cxx_language: /* C and C++ cases are already handled via the
          // lex based pass. */

          // For C and C++ ignore the collection of comments for now (this
          // function is defined for Fortran but since C and C++ code is great
          // for testing the CPP we allow it to be used for testing CPP on C and
          // C++ code, but we ignore comments for this case.
        case e_C_language:
          isComment = false;
          break;
        case e_Cxx_language:
          isComment = false;
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

        // bool isComment = isFortran90Comment(line);
        if (isComment == true) {
          // PP was: cppDeclarationKind =
          // PreprocessingInfo::FortranStyleComment;
          cppDeclarationKind = commentKind;
        }
      }

      // Note that #pragma maps to CpreprocessorUnknownDeclaration so ignore
      // that case!
      if (cppDeclarationKind !=
          PreprocessingInfo::CpreprocessorUnknownDeclaration) {
        PreprocessingInfo *cppDirective = new PreprocessingInfo(
            cppDeclarationKind, line, filename, lineCounter, colNumber,
            numberOfLines, PreprocessingInfo::before);
        ROSE_ASSERT(cppDirective != NULL);
        attributeList.push_back(cppDirective);
        // DQ (11/28/2008): Gather additional data for specific directives (CPP
        // generated linemarkers (e.g. "# <line number> <filename> <flags>").
        if (cppDeclarationKind ==
            PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker) {
          // Gather the line number, filename, and any optional flags.
          // printf ("\nProcessing a CpreprocessorCompilerGeneratedLinemarker:
          // restOfTheLine = %s \n",restOfTheLine.c_str());

          // The IR node has not been build yet, we have to save the required
          // information into the PreprocessingInfo object.
          // SgLinemarkerDirectiveStatement* linemarkerDirective =
          // isSgLinemarkerDirectiveStatement(cppDirective);
          // ROSE_ASSERT(linemarkerDirective != NULL);

          size_t i = 0;
          size_t positionOfFirstCharacterOfIntegerValue = 0;
          size_t lineLength = restOfTheLine.length();
          char nonBlankCharacter = restOfTheLine[0];
          while (i <= lineLength && (nonBlankCharacter >= '0' &&
                                     nonBlankCharacter <= '9') == true) {
            nonBlankCharacter = restOfTheLine[i];
            i++;
          }

          // Need to backup two (for example if this is the end of the line, as
          // in "#endif")
          size_t positionOfLastCharacterOfIntegerValue = i - 2;

          int lineNumberLength = (positionOfLastCharacterOfIntegerValue -
                                  positionOfFirstCharacterOfIntegerValue) +
                                 1;
          string cppIndentifier = restOfTheLine.substr(
              positionOfFirstCharacterOfIntegerValue, lineNumberLength);

          // Some names will convert to integer values
          long integerValue = -1;

          // printf ("firstNonBlankCharacter = %c \n",firstNonBlankCharacter);
          // ROSE_ASSERT(firstNonBlankCharacter == '\"');
          // The modern way to handle conversion of string to integer value is
          // to use strtol(), and not atoi().  But atoi() is simpler.
          const char *str = cppIndentifier.c_str();

          // strtol will put the string into buffer if str is not a number and
          // 2nd parameter is not NULL.
          integerValue = strtol(str, NULL, 10);
          cppDirective->set_lineNumberForCompilerGeneratedLinemarker(
              integerValue);

          size_t remainingLineLength =
              (lineLength - positionOfLastCharacterOfIntegerValue) - 1;
          string remainingLine = restOfTheLine.substr(
              positionOfLastCharacterOfIntegerValue + 1, remainingLineLength);
          size_t positionOfFirstQuote = remainingLine.find('"');

          // Liao, 5/13/2009
          // "#  1 2 3" can show up in a comment block /* */,
          // In this case it is not a CPP generated linemarker at all.
          // We should allow to skip this line as tested in
          // tests/nonsmoke/functional/CompileTests/C_tests/test2009_02.c
          if (positionOfFirstQuote == string::npos) {
            // rollback and skip to the next line
            delete cppDirective;
            continue;
          }

          size_t positionOfLastQuote = remainingLine.rfind('"');
          ROSE_ASSERT(positionOfLastQuote != string::npos);
          int filenameLength = (positionOfLastQuote - positionOfFirstQuote) + 1;
          string filename =
              remainingLine.substr(positionOfFirstQuote, filenameLength);

          cppDirective->set_filenameForCompilerGeneratedLinemarker(filename);

          // Add 1 to move past the last quote and 1 more to move beyond any
          // white space.
          string optionalFlags;
          if (positionOfLastQuote + 2 < remainingLineLength) {
            optionalFlags = remainingLine.substr(positionOfLastQuote + 2);
          }

          // printf ("optionalFlags = %s \n",optionalFlags.c_str());
          cppDirective->set_optionalflagsForCompilerGeneratedLinemarker(
              optionalFlags);
        }
      }

      lineCounter++;

      // printf ("increment lineCounter = %d \n",lineCounter);
#if DEBUG_CPP_DIRECTIVE_COLLECTION
      printf("At bottom of loop over lines in the file ... incremented "
             "lineCounter = %d attributeList.size() = %" PRIuPTR " \n\n",
             lineCounter, attributeList.size());
#endif
    }

    // printf ("Closing file \n");
    targetFile.close();
  } else {
    cerr << "Warning: unable to open target source file: " << filename << "\n";
    // ROSE_ABORT();
  }
}

void ROSEAttributesList::generateFileIdListFromLineDirectives() {
  // This function generates a list of fileId numbers associated with each of
  // the different names specified in #line directives.

  // DQ (12/17/2012): Added assertion.
  ROSE_ASSERT(Sg_File_Info::get_nametofileid_map().size() ==
              Sg_File_Info::get_fileidtoname_map().size());

  // DQ (12/17/2012): Added assertion.
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
      directiveString.erase(0, p);
      // string directiveStringWithoutHash = directiveString;
      size_t lengthOfLineKeyword = string("line").length();

      string directiveStringWithoutHashAndKeyword = directiveString.substr(
          lengthOfLineKeyword,
          directiveString.length() - (lengthOfLineKeyword + 1));
      // Remove white space between "#" and "line" keyword.
      p = directiveStringWithoutHashAndKeyword.find_first_not_of(" \t");
      directiveStringWithoutHashAndKeyword.erase(0, p);
      // At this point we have just '2 "toke.l"', and we can strip off the
      // number.
      p = directiveStringWithoutHashAndKeyword.find_first_not_of("0123456789");

      string lineNumberString =
          directiveStringWithoutHashAndKeyword.substr(0, p);
      int line = atoi(lineNumberString.c_str());
      // DQ (1/7/2014): Added handling for case where filename is not present in
      // #line directive.
      if (p != string::npos) {
        // string directiveStringWithoutHashAndKeywordAndLineNumber =
        // directiveStringWithoutHashAndKeyword.substr(p,directiveStringWithoutHashAndKeyword.length()-(p+1));
        string directiveStringWithoutHashAndKeywordAndLineNumber =
            directiveStringWithoutHashAndKeyword.substr(
                p, directiveStringWithoutHashAndKeyword.length());
        // Remove white space between the line number and the filename.
        p = directiveStringWithoutHashAndKeywordAndLineNumber.find_first_not_of(
            " \t");
        directiveStringWithoutHashAndKeywordAndLineNumber.erase(0, p);
        string quotedFilename =
            directiveStringWithoutHashAndKeywordAndLineNumber;
        // DQ (6/1/2016): Fix for case of trailing spaces after the line number
        // (no quoted file name).  See test2016_17.c.
        // ROSE_ASSERT(quotedFilename[0] == '\"');
        if (quotedFilename[0] == '\"') {
          // DQ (8/22/2020): C_tests/test2020_22.c demonstrates that this is not
          // reasonable. ROSE_ASSERT(quotedFilename[quotedFilename.length()-1]
          // == '\"');

          // DQ (8/22/2020): find the closing quote.
          size_t positionOfNextQuote = quotedFilename.find("\"", 1);
          // std::string filename =
          // quotedFilename.substr(1,quotedFilename.length()-2);
          ROSE_ASSERT(positionOfNextQuote <= quotedFilename.length() - 1);
          std::string filename =
              quotedFilename.substr(1, positionOfNextQuote - 1);
          // Add the new filename to the static map stored in the Sg_File_Info
          // (no action if filename is already in the map).
          Sg_File_Info::addFilenameToMap(filename);

          int fileId = Sg_File_Info::getIDFromFilename(filename);

          if (SgProject::get_verbose() > 1) {
            printf(
                "In "
                "ROSEAttributesList::generateFileIdListFromLineDirectives(): "
                "line = %d fileId = %d quotedFilename = %s filename = %s \n",
                line, fileId, quotedFilename.c_str(), filename.c_str());
          }

          if (filenameIdSet.find(fileId) == filenameIdSet.end()) {
            filenameIdSet.insert(fileId);
          }
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

ROSEAttributesListContainer::~ROSEAttributesListContainer() {
  for (auto &entry : attributeListMap) {
    delete entry.second;
  }
  attributeListMap.clear();
}

void ROSEAttributesListContainer::addList(std::string fileName,
                                          ROSEAttributesList *listPointer) {
  // DQ (7/2/2020): Added assertion to catch when this function is called from a
  // NULL pointer.
  ASSERT_this();

  // attributeListList.push_back ( listPointer );
  attributeListMap[fileName] = listPointer;
}

ROSEAttributesList &
ROSEAttributesListContainer::operator[](const string &fName) {
  // return *(findList (fName.c_str()) );
  // return *( findList (fName) );
  return *(attributeListMap[fName]);
}

bool ROSEAttributesListContainer::isInList(const string &fName) {
  bool returnValue = false;

  // printf ("Looking for list for file = %s attributeListList.size() = %d
  // \n",fName,attributeListList.size());

  // Trap out this special case (generated by SAGE/legacy frontend code)
  // if ( Rose::containsString (fName,"NULL_FILE") == true )
  if (fName == "NULL_FILE")
    return false;

  returnValue = (attributeListMap.find(fName) != attributeListMap.end());

  // printf ("In ROSEAttributesListContainer::isInList(): returnValue = %d
  // \n",returnValue);

  return returnValue;
}

void ROSEAttributesListContainer::dumpContents(void) {
  FILE *outFile = NULL;

  // This should be based on the specificed output file name
  outFile = fopen("rose_directives_list.txt", "w");

  printf("ROSEAttributesListContainer::dumpContents() not implemented for new "
         "map datastructure \n");
  ROSE_ABORT();

  fclose(outFile);
}

void ROSEAttributesListContainer::display(const string &label) {
  printf("ROSEAttributesListContainer::display (label = %s) \n", label.c_str());
  // std::map<std::string, ROSEAttributesList*> attributeListMap;

  printf("In ROSEAttributesListContainer::display(): attributeListMap.size() = "
         "%" PRIuPTR " \n",
         attributeListMap.size());
  map<std::string, ROSEAttributesList *>::iterator i = attributeListMap.begin();
  while (i != attributeListMap.end()) {
    string filename = i->first;
    ROSEAttributesList *attributeList = i->second;

    printf("   --- filename = %s \n", filename.c_str());
    ROSE_ASSERT(attributeList != NULL);

    // DQ (9/25/2018): Added output the the list for each file (debugging header
    // file unparsing).
    attributeList->display("In ROSEAttributesListContainer::display(): xxx");

    i++;
  }

  // printf ("ROSEAttributesListContainer::display() not implemented for new map
  // datastructure \n"); ROSE_ABORT();
}

void ROSEAttributesListContainer::deepClean(void) {
  //
  // Call deepClean for each list.
  //

  for (auto &entry : attributeListMap) {
    if (entry.second != NULL) {
      entry.second->deepClean();
    }
  }
}

void ROSEAttributesListContainer::clean(void) {
  //
  // Call clean for each list.
  //

  for (auto &entry : attributeListMap) {
    if (entry.second != NULL) {
      entry.second->clean();
    }
  }
}

// EOF
