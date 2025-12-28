include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

AM_CPPFLAGS = $(ROSE_INCLUDES)

# The ROSE library should not be linked into Sawyer tools since (1) it's not needed, and (2) it hasn't been built yet at
# the point where Sawyer tools are being linked. In other words, Sawyer tools are not ROSE tools.  Sawyer tools probably
# don't need most of the other ROSE tool dependency libraries either, but at least those are already available.
AM_LDFLAGS = $(ROSE_RPATHS) $(filter-out %/librose.la,$(ROSE_LIBS))

noinst_PROGRAMS = rosebud unitTest01
rosebud_SOURCES =				\
	rosebud.C				\
	Ast.C					\
	Generator.C				\
	RosettaGenerator.C			\
	Utility.C

rosebud_LDADD = ../../util/Sawyer/libroseSawyer.la $(RT_LIBS)

unitTest01_SOURCES =				\
	rosebud.C				\
	Ast.C					\
	Generator.C				\
	RosettaGenerator.C			\
	Utility.C
unitTest01_LDADD = ../../util/Sawyer/libroseSawyer.la $(RT_LIBS)

# Headers are mentioned here so the policy checkers think they're used. There's no reason
# to install them.
headers =					\
	Ast.h					\
	BasicTypes.h				\
	Generator.h				\
	RosettaGenerator.h			\
	Serializer.h				\
	Utility.h

other_files =					\
	README
