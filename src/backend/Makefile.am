# Non-recursive Makefile

include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

###############################################################################
# Subdirectory specifics
# TOO (01/26/11): relative paths must be updated for each propagation upward
# in the flattening of Makefiles
#
# Caution: include path cannot contain variable,
#	e.g. include $(cxxCodeGenerationPath)/Makefile_variables
#
include $(srcdir)/unparser/Makefile_variables
include $(srcdir)/unparser/CxxCodeGeneration/Makefile_variables
include $(srcdir)/unparser/formatSupport/Makefile_variables
include $(srcdir)/unparser/FortranCodeGeneration/Makefile_variables
include $(srcdir)/unparser/languageIndependenceSupport/Makefile_variables

unparserPath=$(srcdir)/unparser#
cxxCodeGenerationPath=$(srcdir)/unparser/CxxCodeGeneration#
formatSupportPath=$(srcdir)/unparser/formatSupport#
fortranCodeGenerationPath=$(srcdir)/unparser/FortranCodeGeneration#
languageIndependenceSupportPath=$(srcdir)/unparser/languageIndependenceSupport#

###############################################################################

AM_CPPFLAGS=\
	$(ROSE_INCLUDES)

noinst_LTLIBRARIES = libbackend.la

##################################################################

libbackend_la_SOURCES=\
	$(unparser_la_sources)


pkginclude_HEADERS=\
	$(unparser_includeHeaders)


dist_pkginclude_HEADERS=


EXTRA_DIST=\
	CMakeLists.txt \
	$(unparser_extraDist)


clean-local:
	$(unparser_cleanLocal)

