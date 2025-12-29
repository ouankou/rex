dnl $Id: compiler-id.m4,v 1.2 2008/01/26 00:25:52 dquinlan Exp $

dnl Determines which compiler is being used.
dnl This check uses the compiler behavior when possible.
dnl For some compiler, we resort to a best guess,
dnl because we do not know a foolproof way to get the info.

dnl Much of the information used here came from the very
dnl helpful predef project (http://predef.sourceforge.net/).

dnl Simple wrappers to allow using BTNG_INFO_CXX_ID_NAMES and
dnl BTNG_INFO_CC_ID_NAMES without arguments.
dnl The names CC_ID and CC_VERSION are used for the C compiler id and version.
dnl The names CXX_ID and CXX_VERSION are used for the C++ compiler id and version.
AC_DEFUN([BTNG_INFO_CXX_ID],[
  BTNG_INFO_CXX_ID_NAMES(CXX_ID,CXX_VERSION)
])
AC_DEFUN([BTNG_INFO_CC_ID],[
  BTNG_INFO_CC_ID_NAMES(CC_ID,CC_VERSION)
])
AC_DEFUN([BTNG_INFO_CC_CXX_ID],[
  AC_REQUIRE([BTNG_INFO_CC_ID])
  AC_REQUIRE([BTNG_INFO_CXX_ID])
])


dnl BTNG_INFO_CXX_ID and BTNG_INFO_C_ID determine which C or C++ compiler
dnl is being used.
# Set the variables CXX_ID or C_ID as follows:
# Gnu		-> gnu
# Additional compiler IDs may be added as needed.


AC_DEFUN([BTNG_INFO_CXX_ID_NAMES],
dnl Arguments are:
dnl 1. Name of variable to set to the ID string.
dnl 2. Name of variable to set to the version number.
[
# Start macro BTNG_INFO_CXX_ID_NAMES
  AC_REQUIRE([AC_PROG_CXXCPP])
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  BTNG_AC_LOG(CXXP is $CXX)
  BTNG_AC_LOG(CXXCPP is $CXXCPP)

  $1=unknown
  $2=unknown

dnl Do not change the following chain of if blocks into a case statement.
dnl We may eventually have a compiler that must be tested in a different
dnl method


  # Check if it is a GNU compiler.
  if test $$1 = unknown; then
    BTNG_AC_LOG(checking if $CXX is gnu)
    AC_EGREP_CPP(^yes,
#ifdef __GNUC__
yes;
#endif
,
    $1=gnu
    # GNU compilers output version number with option --version.
    # Alternatively, it also defines __GNUC__, GNUC_MINOR__ and
    # __GNUC_PATCHLEVEL__
    [[$2=`$CXX --version | sed -e 's/[^0-9]\{0,\}\([^ ]\{1,\}\).\{0,\}/\1/' -e 1q`]]
    )
  fi


  AC_LANG_RESTORE
  BTNG_AC_LOG_VAR(CXX_ID CXX_VERSION)
# End macro BTNG_INFO_CXX_ID_NAMES
])





AC_DEFUN([BTNG_INFO_CC_ID_NAMES],
dnl Arguments are:
dnl 1. Name of variable to set to the ID string.
dnl 2. Name of variable to set to the version number.
[
# Start macro BTNG_INFO_CC_ID_NAMES
  AC_REQUIRE([AC_PROG_CPP])
  AC_LANG_SAVE
  AC_LANG_C
  BTNG_AC_LOG(CC is $CC)
  BTNG_AC_LOG(CPP is $CPP)

  $1=unknown
  $2=unknown

dnl Do not change the following chain of if blocks into a case statement.
dnl We may eventually have a compiler that must be tested in a different
dnl method


  # Check if it is a GNU compiler.
  if test $$1 = unknown; then
    BTNG_AC_LOG(checking if $CC is gnu)
    AC_EGREP_CPP(^yes,
#ifdef __GNUC__
yes;
#endif
,
    $1=gnu
    [[$2=`$CC --version | sed -e 's/[^0-9]\{0,\}\([^ ]\{1,\}\).\{0,\}/\1/' -e 1q`]]
    )
  fi


  AC_LANG_RESTORE
  BTNG_AC_LOG_VAR(CC_ID CC_VERSION)
# End macro BTNG_INFO_CC_ID_NAMES
])
