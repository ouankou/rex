AC_DEFUN([ROSE_SUPPORT_JAVA],
[
# Begin macro ROSE_SUPPORT_JAVA.
ROSE_CONFIGURE_SECTION([Checking JVM for OFP])

AC_ARG_WITH([java],
            AS_HELP_STRING([--with-java=PATH],
                           [use a Java JDK for the OFP JVM dependency (default: try)]),
            [javasetting=$withval],
            [javasetting=try])

USE_JAVA=0
JAVA_PATH=""
JAVA_JVM_LINK=""
JAVA_JVM_INCLUDE=""
JAVA=""
JAVAC=""
JAR=""

if test "x$support_fortran_language" != "xyes"; then
  AC_MSG_NOTICE([Fortran disabled; skipping JVM checks])
else
  AC_MSG_CHECKING([for a Java JDK])
  if test "x$javasetting" = xno; then
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([Fortran support requires a Java JDK; remove --with-java=no])
  elif test "x$javasetting" = xyes || test "x$javasetting" = xtry; then
    JAVA_PATH="${JAVA_HOME}"
    if test "x$JAVA_PATH" = x; then
      JAVAC="`which javac`"
      if test $? -eq 0; then
        ROSE_CANON_SYMLINK(JAVAC, "${JAVAC}")
        JAVAC_BASENAME=`basename ${JAVAC}`
        if test x${JAVAC_BASENAME} != "xjavac"; then
          AC_MSG_ERROR([found non-JDK javac; specify a JDK using --with-java=PATH])
        fi
        AS_SET_CATFILE(JAVA_PATH, "`pwd`", "`dirname ${JAVAC}`/..")

      elif test "x$javasetting" = "xyes"; then
        AC_MSG_ERROR([--with-java was given but javac is not in PATH and JAVA_HOME was not set])
      else
        AC_MSG_ERROR([Fortran support requires a Java JDK; set JAVA_HOME or use --with-java=PATH])
      fi
    else
      JAVAC="${JAVA_PATH}/bin/javac"
    fi
  elif test -d "${javasetting}"; then
    if test -x "${javasetting}/bin/javac"; then
      JAVA_PATH="${javasetting}"
      JAVAC="${javasetting}/bin/javac"
    else
      AC_MSG_ERROR([argument to --with-java should be a JDK directory (with bin/javac)])
    fi
  elif test -x "${javasetting}"; then
    AS_SET_CATFILE(JAVA_PATH, "`pwd`", "`dirname ${javasetting}`/..")
    JAVAC="${javasetting}"
  else
    AC_MSG_ERROR([argument to --with-java should be a JDK directory or javac executable])
  fi
  AC_MSG_RESULT([${JAVA_PATH}])

  JAVA_BIN="${JAVA_PATH}/bin"
  JAVA="${JAVA_BIN}/java"
  JAR="${JAVA_BIN}/jar"

  AC_MSG_CHECKING([for java])
  if test -x "${JAVA}"; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_ERROR([java not found in ${JAVA_PATH}])
  fi

  AC_MSG_CHECKING([for jar])
  if test -x "${JAR}"; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_ERROR([jar not found in ${JAVA_PATH}])
  fi

  AC_MSG_CHECKING([for javac])
  if test -x "${JAVAC}"; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_ERROR([javac not found in ${JAVA_PATH}])
  fi

  AC_MSG_CHECKING([for JVM include and link options])
  JAVA_JVM_FULL_PATH="`env _JAVA_LAUNCHER_DEBUG=x ${JAVA} 2>/dev/null | grep '^JVM path is' | cut -c 13-`"
  JAVA_JVM_PATH=`dirname "${JAVA_JVM_FULL_PATH}"`
  if test "x$JAVA_JVM_FULL_PATH" = x; then
    JAVA_JVM_PATH="`env _JAVA_LAUNCHER_DEBUG=x ${JAVA} 2>&1 | grep '^JavaJVMDir  = ' | cut -c 15-`"
    if test "x$JAVA_JVM_PATH" = x; then
      AC_MSG_ERROR([unable to find path to JVM library])
    fi
  fi
  JAVA_JVM_LINK="-L${JAVA_JVM_PATH} -ljvm"
  JAVA_JVM_INCLUDE="-I${JAVA_PATH}/include -I${JAVA_PATH}/include/linux"
  AC_MSG_RESULT([${JAVA_JVM_INCLUDE} and ${JAVA_JVM_LINK}])

  AC_CHECK_HEADERS([jni.h], [have_jni=yes], [have_jni=no])
  if test "x$have_jni" = "xyes"; then
    AC_MSG_WARN([found a default jni.h; ensure it matches the selected JDK if build issues occur])
  fi

  USE_JAVA=1
fi

AC_SUBST(JAVA_PATH)
AC_SUBST(JAVA_JVM_LINK)
AC_SUBST(JAVA_JVM_INCLUDE)
AC_SUBST(JAVA)
AC_SUBST(JAVAC)
AC_SUBST(JAR)

# End macro ROSE_SUPPORT_JAVA.
]
)
