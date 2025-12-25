include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

AM_CPPFLAGS = $(ROSE_INCLUDES)

# DQ (10/27/2013): This is a new definition of libastTokenStream
# noinst_LTLIBRARIES = libastTokenStream.la
noinst_LTLIBRARIES = libastTokenStream.la

# DQ (10/27/2013): Support for token stream mapping.
libastTokenStream_la_SOURCES = \
   linearizeAST.C \
   tokenStreamMapping.C frontierDetection.C simpleFrontierDetection.C previousAndNextNode.C maxExtents.C \
   detectMacroOrIncludeFileExpansions.C detectMacroExpansionsToBeUnparsedAsAstTransformations.C

libastTokenStream_la_LIBADD       = 
libastTokenStream_la_DEPENDENCIES = 

# Leave these header file reference in the Makefile.am (so we will pass the policy tests).
pkginclude_HEADERS = linearizeAST.h tokenStreamMapping.h frontierDetection.h simpleFrontierDetection.h previousAndNextNode.h \
   maxExtents.h detectMacroOrIncludeFileExpansions.h detectMacroExpansionsToBeUnparsedAsAstTransformations.h

EXTRA_DIST = CMakeLists.txt
