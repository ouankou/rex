#ifndef ROSE_COMMANDLINE_UTILITY_H
#define ROSE_COMMANDLINE_UTILITY_H
//#include "rosedefs.h"
#include "rosedll.h"
#include <list>
#include <vector>
#include <string>
#include <stack>
#include <map>
#define Rose_STL_Container std::vector

//Rama (12/22/2006): changing the class to a namespace and removing the "static"ness of the "member" functions
//! Command line processing utility functions. Functions in this namespace are in the ROSE Utilities library and
//  therefore are unable to call other functions in the ROSE library. See also, Rose::CommandLine for higher-level functions.
namespace CommandlineProcessing
   {
      /** Separate a string into individual parameters.
       *
       * @param commandline Command line string.
       * @return Parsed arguments. */
          ROSE_UTIL_API Rose_STL_Container<std::string> generateArgListFromString ( std::string commandline );

          /** Convert a vector of string to a single string. */
       // std::string generateStringFromArgList( Rose_STL_Container<std::string> & argList);
          ROSE_UTIL_API std::string generateStringFromArgList( const Rose_STL_Container<std::string> & argList);

          /** Convert an argc-argv pair into a string vector.
           *
           * @param argc Argument count.
           * @param argv Argument vector.
           * @return Parsed arguments. */
          ROSE_UTIL_API Rose_STL_Container<std::string> generateArgListFromArgcArgv ( int argc, const char* argv[] );
          
          inline Rose_STL_Container<std::string> generateArgListFromArgcArgv ( int argc, char* argv[] ) 
          {return generateArgListFromArgcArgv(argc, (const char**)argv);}
          
          /** Convert a string vector back to an argc-argv pair.
           *
           * @param argList Argument list.
           * @param argc Receives argument count.
           * @param argv Receives argument vector. */
          ROSE_UTIL_API void generateArgcArgvFromList ( Rose_STL_Container<std::string> argList, int & argc, char** & argv );

          /** Looks for inputPrefix prefixed options.
           *
           * Warning: As opposed to what the former documentation was saying this function doesn't modify argList.
           *
           * @param argList Argument list.
           * @param inputPrefix Option prefix to match.
           * @return List of matched options. */
          ROSE_UTIL_API Rose_STL_Container<std::string> generateOptionList (const Rose_STL_Container<std::string> & argList, std::string inputPrefix );

          /** Looks for inputPrefix-prefixed options.
           *
           * If isOptionTakingSecondParameter of the inputPrefix-prefixed returns true, add the parameter(s) to the result list.
           *
           * @param argList Argument list.
           * @param inputPrefix Option prefix to match.
           * @return List of matched options and parameters. */
          ROSE_UTIL_API Rose_STL_Container<std::string> generateOptionListWithDeclaredParameters (const Rose_STL_Container<std::string> & argList, std::string inputPrefix );

       /** Find all options matching the given prefix.
        *
        * Find all options matching 'inputPrefix:optionName' || 'inputPrefix:optionName optionValue' from argList,
        * strip off 'inputPrefix:' or replace it by 'newPrefix' if provided. Returns a string list of matched options.
        * All matching options and values are removed from argList.
        *
        * @param argList Argument list to scan and update.
        * @param inputPrefix Option prefix to match.
        * @param newPrefix Replacement prefix.
        * @return List of matched options. */
          ROSE_UTIL_API Rose_STL_Container<std::string> generateOptionWithNameParameterList ( Rose_STL_Container<std::string> & argList, std::string inputPrefix, std::string newPrefix = "");

          extern Rose_STL_Container<std::string> extraCppSourceFileSuffixes;

       /** Search 'argv' for an option like optionPrefixOption.
        *
        * The argument 'option' adds () to the actual option, and allows the |(OR) operations.
        *
        * @param argv Argument vector to search.
        * @param optionPrefix Prefix for the option.
        * @param Option Option name or regex.
        * @param removeOption Whether to remove the option from argv.
        * @return True if the option was found. */
          ROSE_UTIL_API bool isOption ( std::vector<std::string> & argv, std::string optionPrefix, std::string Option, bool removeOption );

       /** Search 'argv' for 'optionPrefixOption value' (int).
        *
        * @param argv Argument vector to search.
        * @param optionPrefix Prefix for the option.
        * @param Option Option name or regex.
        * @param optionParameter Receives the parsed value.
        * @param removeOption Whether to remove the option from argv.
        * @return True if the option was found. */
          ROSE_UTIL_API bool isOptionWithParameter ( std::vector<std::string> & argv, std::string optionPrefix, std::string Option, int & optionParameter, bool removeOption );

       /** Search 'argv' for 'optionPrefixOption value' (float).
        *
        * @param argv Argument vector to search.
        * @param optionPrefix Prefix for the option.
        * @param Option Option name or regex.
        * @param optionParameter Receives the parsed value.
        * @param removeOption Whether to remove the option from argv.
        * @return True if the option was found. */
          ROSE_UTIL_API bool isOptionWithParameter ( std::vector<std::string> & argv, std::string optionPrefix, std::string Option, float & optionParameter, bool removeOption );

       /** Search 'argv' for 'optionPrefixOption value' (string).
        *
        * @param argv Argument vector to search.
        * @param optionPrefix Prefix for the option.
        * @param Option Option name or regex.
        * @param optionParameter Receives the parsed value.
        * @param removeOption Whether to remove the option from argv.
        * @return True if the option was found. */
          ROSE_DLL_API bool isOptionWithParameter ( std::vector<std::string> & argv, std::string optionPrefix, std::string Option, std::string & optionParameter, bool removeOption );

      /** Add the strings in argList to the command line.
       *
       * @param argv Argument vector to append to.
       * @param prefix Prefix to prepend to each argument.
       * @param argList Arguments to add. */
          ROSE_UTIL_API void addListToCommandLine ( std::vector<std::string> & argv , std::string prefix, Rose_STL_Container<std::string> argList );
      /** Remove all options matching a specified prefix.
       *
       * @param argv Argument vector to edit.
       * @param prefix Prefix to remove. */
          ROSE_UTIL_API void removeArgs ( std::vector<std::string> & argv, std::string prefix );
      /** Remove all options matching a specified prefix along with their values.
       *
       * @param argv Argument vector to edit.
       * @param prefix Prefix to remove. */
          ROSE_UTIL_API void removeArgsWithParameters ( std::vector<std::string> & argv, std::string prefix );
       /** Remove file names specified in filenameList from argv.
        *
        * @param argv Argument vector to edit.
        * @param filenameList File names to remove.
        * @param exceptFilename File name to keep. */
          ROSE_UTIL_API void removeAllFileNamesExcept ( std::vector<std::string> & argv, Rose_STL_Container<std::string> filenameList, std::string exceptFilename );

      /** Build a string from the argList.
       *
       * @param argList Argument list.
       * @param skipInitialEntry Whether to skip the first entry.
       * @param skipSourceFiles Whether to skip source file arguments.
       * @return Combined command line string. */
          ROSE_UTIL_API std::string generateStringFromArgList ( Rose_STL_Container<std::string> argList, bool skipInitialEntry, bool skipSourceFiles );

      /** Build the list of isolated file names from the command line.
       *
       * @param argList Argument list.
       * @param binaryMode Whether to treat inputs as binaries.
       * @return Source file names. */
          ROSE_DLL_API Rose_STL_Container<std::string>
          generateSourceFilenames(Rose_STL_Container<std::string> argList, bool binaryMode);

          // DQ and PC (6/1/2006): Added Peter's suggested fixes to support
          // auto-documentation.
          /** Add another valid source file suffix.
           *
           * @param suffix Suffix to add. */
          ROSE_UTIL_API void addCppSourceFileSuffix ( const std::string &suffix );

          ROSE_UTIL_API bool isSourceFilename ( std::string name ); 

          ROSE_UTIL_API bool isObjectFilename ( std::string name );
          ROSE_DLL_API bool isExecutableFilename ( std::string name );

       // DQ (8/20/2008): Added test that will allow bogus executable files (marked as .exe but not executable) to pass
          ROSE_DLL_API bool isValidFileWithExecutableFileSuffix ( std::string name );

          ROSE_UTIL_API bool isCFileNameSuffix ( const std::string & suffix );

          ROSE_UTIL_API bool isCppFileNameSuffix(const std::string &suffix);

          // DQ (8/7/2007): Added support for Fortran file suffix names.
          ROSE_UTIL_API bool isFortranFileNameSuffix ( const std::string & suffix );

       // DQ (5/18/2008): Support to marking when C preprocessing is required for 
       // Fortran files, default is true for C and C++.
          ROSE_UTIL_API bool isFortranFileNameSuffixRequiringCPP ( const std::string & suffix );

       // DQ (11/17/2007): Added fortran mode specific suffix checking
          ROSE_UTIL_API bool isFortran77FileNameSuffix   ( const std::string & suffix );
          ROSE_UTIL_API bool isFortran90FileNameSuffix   ( const std::string & suffix );
          ROSE_UTIL_API bool isFortran95FileNameSuffix   ( const std::string & suffix );
          ROSE_UTIL_API bool isFortran2003FileNameSuffix ( const std::string & suffix );
          ROSE_UTIL_API bool isFortran2008FileNameSuffix ( const std::string & suffix );

       // DQ (1/23/2009): Added support for Co-Array Fortran file extension.
          ROSE_UTIL_API bool isCoArrayFortranFileNameSuffix ( const std::string & suffix );

       // TV (05/17/2010) Support for CUDA
          ROSE_UTIL_API bool isCudaFileNameSuffix ( const std::string & suffix );
       // TV (05/17/2010) Support for OpenCL
          ROSE_UTIL_API bool isOpenCLFileNameSuffix ( const std::string & suffix );

          ROSE_UTIL_API void initSourceFileSuffixList();
          static Rose_STL_Container<std::string> validSourceFileSuffixes;
          
          ROSE_UTIL_API void initObjectFileSuffixList();
          static Rose_STL_Container<std::string> validObjectFileSuffixes;

          ROSE_DLL_API void initExecutableFileSuffixList();
          static Rose_STL_Container<std::string> validExecutableFileSuffixes;

       // bool isOptionTakingFileName( std::string argument );
          ROSE_DLL_API bool isOptionTakingSecondParameter( std::string argument );
          ROSE_DLL_API bool isOptionTakingThirdParameter ( std::string argument );
   };

// DQ (4/5/2010): This are defined in sage_support.C
/** Find the path of a ROSE support file from the source tree.
 *
 * If ROSE is not installed (see roseInstallPrefix()), the top of the source tree plus sourceTreeLocation is
 * used as the location. If the variable is not set, the path in installTreeLocation (with the install prefix added) is
 * used instead.
 *
 * @param sourceTreeLocation Relative path inside the source tree.
 * @param installTreeLocation Relative path inside the install tree.
 * @return Resolved support path. */
ROSE_DLL_API std::string
findRoseSupportPathFromSource(const std::string& sourceTreeLocation,
                              const std::string& installTreeLocation);

// DQ (4/5/2010): This are defined in sage_support.C
/** Find the path of a ROSE support file from the build tree.
 *
 * If ROSE is not installed (see roseInstallPrefix()), the top of the build tree plus buildTreeLocation is
 * used as the location. If the variable is not set, the path in installTreeLocation (with the install prefix added) is
 * used instead.
 *
 * @param buildTreeLocation Relative path inside the build tree.
 * @param installTreeLocation Relative path inside the install tree.
 * @return Resolved support path. */
ROSE_DLL_API std::string
findRoseSupportPathFromBuild(const std::string& buildTreeLocation,
                             const std::string& installTreeLocation);

// DQ (4/5/2010): This are defined in sage_support.C
/** Find the path of the ROSE install prefix.
 *
 * There is an assumption that <directory containing librose>/.. is the prefix, and that other things can
 * be found from that. This may not be true if the various install directories are set by hand (rather than from $prefix).
 * This function either puts the prefix into RESULT and returns true (for an installed copy of ROSE), or returns false (for a
 * build tree).
 *
 * @param result Receives the install prefix.
 * @return True for installed ROSE, false for a build tree. */
ROSE_DLL_API bool
roseInstallPrefix(std::string& result);

// endif associated with define ROSE_COMMANDLINE_UTILITY_H
#endif
