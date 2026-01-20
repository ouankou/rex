// -*- c++ -*-

#ifndef ROSE_DOCS_ROSE_TOOLDEV_MAKEFILE_H
#define ROSE_DOCS_ROSE_TOOLDEV_MAKEFILE_H

/** @brief Build systems
 *
 * Using a build system to build tools.
 *
 * Once the ROSE library has been configured, built, and installed it can be
 * used. Any program that uses ROSE will likely need to be compiled with the
 * same switches and software components as ROSE itself. The easiest way to get
 * this information is by running `rose-config --help`.
 *
 * Here's an example makefile:
 *
 * ```makefile
 * # Sample makefile for programs that use the ROSE library.
 * #
 * # ROSE has a number of configuration details that must be present when
 * # compiling and linking a user program with ROSE, and some of these
 * # details are difficult to get right.  The most foolproof way to get
 * # these details into your own makefile is to use the "rose-config"
 * # tool.
 * #
 * #
 * # This makefile assumes:
 * #   1. The ROSE library has been properly installed (refer to the
 * #      documentation for configuring, building, and installing ROSE).
 * #
 * #   2. The top of the installation directory is $(ROSE_PREFIX). This
 * #      is the same directory you specified for "CMAKE_INSTALL_PREFIX".
 * #
 * ##############################################################################
 *
 * # If the ROSE bin directory is in your PATH, rose-config can be found
 * # automatically.
 * ifndef ROSE_PREFIX
 * ROSE_PREFIX = $(shell rose-config prefix)
 * endif
 *
 * ROSE_CONFIG = $(ROSE_PREFIX)/bin/rose-config
 *
 * include $(ROSE_PREFIX)/lib/rose-config.cfg
 *
 * # Standard C++ compiler stuff (see rose-config --help)
 * ROSE_CXX         = $(shell $(ROSE_CONFIG) ROSE_CXX)
 * ROSE_CPPFLAGS    = $(shell $(ROSE_CONFIG) ROSE_CPPFLAGS)
 * ROSE_CXXFLAGS    = $(shell $(ROSE_CONFIG) ROSE_CXXFLAGS)
 * ROSE_LDFLAGS     = $(shell $(ROSE_CONFIG) ROSE_LDFLAGS)
 * ROSE_LIBDIRS     = $(shell $(ROSE_CONFIG) ROSE_LIBDIRS)
 * ROSE_RPATHS      = $(shell $(ROSE_CONFIG) ROSE_RPATHS)
 * ROSE_LINK_RPATHS = $(shell $(ROSE_CONFIG) ROSE_LINK_RPATHS)
 *
 * MOSTLYCLEANFILES =
 *
 * ##############################################################################
 * # Assuming your source code is "demo.C" to build an executable named "demo".
 *
 * all: demo
 *
 * demo.o: demo.C
 * 	$(ROSE_CXX) $(ROSE_CPPFLAGS) $(ROSE_CXXFLAGS) -o $@ -c $^
 *
 * demo: demo.o
 * 	$(ROSE_CXX) $(ROSE_CXXFLAGS) -o $@ $^ $(ROSE_LDFLAGS)
 * $(ROSE_LINK_RPATHS) -Wl,-rpath=$(ROSE_PREFIX)/lib
 *
 * MOSTLYCLEANFILES += demo demo.o
 *
 * ##############################################################################
 * # Standard boilerplate
 *
 * .PHONY: clean
 * clean:
 * 	rm -f $(MOSTLYCLEANFILES)
 * ```
 *
 * See @ref tooldev.
 */
struct tooldev_makefile {};

#endif
