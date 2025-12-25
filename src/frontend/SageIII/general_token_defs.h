/* DQ (9/24/2007): This is part of Rama's Lexer */
#ifndef ROSE_LEX_TOKEN_DEFINITIONS
#define ROSE_LEX_TOKEN_DEFINITIONS 1

class PreprocessingInfo;

// These have been moved from the preproc.ll file so that they can
// be seen by other parst of ROSE (e.g. in the token processing).
namespace ROSE_token_ids
   {
   // whitespace token IDs
      enum whitespace
         {
        // DQ (9/2/2015): Removed comma to avoid Intel v14 compiler warning.
           T_NOTKNOWN
        // The following are non UNIX whitespace
         };

    enum ROSE_C_CXX_keywords 
    {
        C_CXX_ASM = 0 + 500,
        C_CXX_AUTO = 1 + 500,
        C_CXX_BOOL = 2 + 500,
        C_CXX_BREAK = 3 + 500,
        C_CXX_CASE = 4 + 500,
        C_CXX_CATCH = 5 + 500,
        C_CXX_CHAR = 6 + 500,
        C_CXX_CLASS = 7 + 500,
        C_CXX_CONST = 8 + 500,
        C_CXX_CONSTCAST = 9 + 500,
        C_CXX_CONTINUE = 10 + 500,
        C_CXX_DEFAULT = 11 + 500,
        C_CXX_DEFINED = 12 + 500,
        C_CXX_DELETE = 13 + 500,
        C_CXX_DO = 14 + 500,
        C_CXX_DOUBLE = 15 + 500,
        C_CXX_DYNAMICCAST = 16 + 500,
        C_CXX_ELSE = 17 + 500,
        C_CXX_ENUM = 18 + 500,
        C_CXX_EXPLICIT = 19 + 500,
        C_CXX_EXPORT = 20 + 500,
        C_CXX_EXTERN = 21 + 500,
        C_CXX_FALSE = 22 + 500,
        C_CXX_FLOAT = 23 + 500,
        C_CXX_FOR = 24 + 500,
        C_CXX_FRIEND = 25 + 500,
        C_CXX_GOTO = 26 + 500,
        C_CXX_IF = 27 + 500,
        C_CXX_INLINE = 28 + 500,
        C_CXX_INT = 29 + 500,
        C_CXX_LONG = 30 + 500,
        C_CXX_MUTABLE = 31 + 500,
        C_CXX_NAMESPACE = 32 + 500,
        C_CXX_NEW = 33 + 500,
        C_CXX_OPERATOR = 34 + 500,
        C_CXX_PRIVATE = 35 + 500,
        C_CXX_PROTECTED = 36 + 500,
        C_CXX_PUBLIC = 37 + 500,
        C_CXX_REGISTER = 38 + 500,
        C_CXX_REINTERPRETCAST = 39 + 500,
        C_CXX_RETURN = 40 + 500,
        C_CXX_SHORT = 41 + 500,
        C_CXX_SIGNED = 42 + 500,
        C_CXX_SIZEOF = 43 + 500,
        C_CXX_STATIC = 44 + 500,
        C_CXX_STATICCAST = 45 + 500,
        C_CXX_STRUCT = 46 + 500,
        C_CXX_SWITCH = 47 + 500,
        C_CXX_TEMPLATE = 48 + 500,
        C_CXX_THIS = 49 + 500,
        C_CXX_THROW = 50 + 500,
        C_CXX_TRY = 51 + 500,
        C_CXX_TRUE = 52 + 500,
        C_CXX_TYPEDEF = 53 + 500,
        C_CXX_TYPEID = 54 + 500,
        C_CXX_TYPENAME = 55 + 500,
        C_CXX_UNION = 56 + 500,
        C_CXX_UNSIGNED = 57 + 500,
        C_CXX_USING = 58 + 500,
        C_CXX_VIRTUAL = 59 + 500,
        C_CXX_VOID = 60 + 500,
        C_CXX_VOLATILE = 61 + 500,
        C_CXX_WCHART = 62 + 500,
        C_CXX_WHILE = 63 + 500
   };

      //the following are alternate tokens.
      //Refer to page 12 of C++ STD

enum ROSE_C_CXX_alternate_tok
    {
        C_CXX_LEFT_CURLY_ALT, // The <% which is the same as {
        C_CXX_RIGHT_CURLY_ALT, //The %> which is the same as }
        C_CXX_LEFT_SQUARE_ALT, //The <: which is the same as [
        C_CXX_RIGHT_SQUARE_ALT,//The :> which is the same as ]
        C_CXX_HASH_ALT, // The #
        C_CXX_HASH_HASH_ALT, // The 
     // DQ (9/2/2015): Removed comma to avoid Intel v14 compiler warning.
        C_CXX_and_ALT // The #
    };

enum ROSE_C_CXX_Additional_Info
   {
     C_CXX_COMMENTS           = 0 + 100000,
     C_CXX_STRING_LITERALS    = 1 + 100000,
     C_CXX_IDENTIFIER         = 2 + 100000,
     C_CXX_PREPROCESSING_INFO = 3 + 100000,
     C_CXX_UNIDENTIFIED_TOKEN = 4 + 100000,
  // DQ (10/12/2013): Added C_CXX_SYNTAX, C_CXX_WHITESPACE, and C_CXX_PRAGMA
     C_CXX_SYNTAX             = 5 + 100000,
     C_CXX_WHITESPACE         = 6 + 100000,
     C_CXX_PRAGMA             = 7 + 100000,
     C_CXX_ERROR              = 8 + 100000
   };

};


struct token_element
   {
  // This is the simplist contect of a Token

  // value of the token
     std::string token_lexeme;

  // A token can also be an already processed CPP directive or comment (PreprocessingInfo)
  // PreprocessingInfo* token_preprocessingInfo;

  // The classification for the token (language dependent)
     int token_id;
   };

struct file_pos_info
   {
  // Source code position information for token
     int line_num;
     int column_num;
   };

// At some point I want to replace this with a SgToken 
// IR node until then this is an intermediate step.
struct stream_element
   {
  // This is the element in the token stream.

  // This is the pointer to the token
     struct token_element * p_tok_elem;

  // The token stream can also include references to already processed CPP directives or comments (PreprocessingInfo)
     PreprocessingInfo* p_preprocessingInfo;

  // Positon of the start of the token
     struct file_pos_info beginning_fpi;

  // Position of the end of the token
     struct file_pos_info ending_fpi;
   };

// At present this is an STL list, but I would like it to be a vector at some point
typedef std::list<stream_element*> LexTokenStreamType;
typedef LexTokenStreamType* LexTokenStreamTypePointer;

// endif for ROSE_LEX_TOKEN_DEFINITIONS
#endif
