dnl $Id: compiler-ar.m4,v 1.2 2008/01/26 00:25:52 dquinlan Exp $

dnl Define variables that generalizes the command to generate a library.
dnl This is usually done with something like "ar ruv".  But ar does not
dnl work well with C++ because compiling may be done at link time, to
dnl instantiate templates.  In general, a command using the C++ compiler
dnl is preferred over ar.  The variables defined are aimed at literally
dnl replacing the "ar ruv" in the command "ar ruv libxyz.a *.o".
dnl BTNG.

AC_DEFUN([BTNG_CXX_AR],[
dnl Set the variables CXX_ID and CXX_VERSION.

# Determine what compiler we ar using.
# The library-building command depends on the compiler characteristics.
AC_REQUIRE([BTNG_INFO_CXX_ID])


# Create the command syntax such that they can substitute
# the generic case of using ar, i.e. ar ruv.
# This means that the libraries will be generated using
# "$CXX_STATIC_LIB_UPDATE libxyz.a *.o".
case "$CXX_ID" in
  # Each block here defines CXX_STATIC_LIB_UPDATE and CXX_SHARED_LIB_UPDATE
  # for a specific compiler.
  gnu)
    CXX_STATIC_LIB_UPDATE='${AR} ruv'
    # I tried 'gcc -o' for static libraries, but got unresolved symbols
    # and no library.  BTNG.
    CXX_SHARED_LIB_UPDATE='${CXX} -shared -o'
  ;;
  *)
    # Set the default values.
    # (These generally do not work well when templates are involved.)
    CXX_STATIC_LIB_UPDATE='${AR} ruv'
    CXX_SHARED_LIB_UPDATE='${CXX} -o'
  ;;
esac


# Let user override.
AC_ARG_WITH(cxx_static_lib_update,
[  --with-cxx_static_lib_update=COMMAND
			Use COMMAND (not 'ar ruv') to make static C++ library.],
CXX_STATIC_LIB_UPDATE=$with_cxx_static_lib_update)
AC_ARG_WITH(cxx_shared_lib_update,
[  --with-cxx_shared_lib_update=COMMAND
			Use COMMAND (not 'ar ruv') to make shared C++ library.],
CXX_SHARED_LIB_UPDATE=$with_cxx_shared_lib_update)

])
