This directory contains the local header fixture for
`tests/nonsmoke/functional/CompileTests/C_tests/test2018_34.c`.

The specimen is derived from an older `sudo_conf.c` revision that expected
generated and project-local headers from sudo's build tree.  The compile test
only needs the declaration surface of those headers, so this fixture keeps the
specimen self-contained without changing the test source.
