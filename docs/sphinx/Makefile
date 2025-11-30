# Simple Makefile to build the documentation locally

SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
DOXYGEN       ?= doxygen
REPO_ROOT     := $(abspath ..)
DOXYFILE      ?= $(REPO_ROOT)/docs/Doxyfile
SOURCEDIR     = .
BUILDDIR      = ../_build

.PHONY: help html dirhtml clean doxygen

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS)

doxygen:
	cd "$(REPO_ROOT)" && $(DOXYGEN) "$(DOXYFILE)"

html: doxygen
	$(SPHINXBUILD) -b html "$(SOURCEDIR)" "$(BUILDDIR)/html" $(SPHINXOPTS)

dirhtml: doxygen
	$(SPHINXBUILD) -b dirhtml "$(SOURCEDIR)" "$(BUILDDIR)/dirhtml" $(SPHINXOPTS)

clean:
	rm -rf "$(BUILDDIR)"
