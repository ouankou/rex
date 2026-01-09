# REX Documentation

REX is a modernization of the ROSE compiler infrastructure using the Clang/LLVM
frontend for C/C++ analysis.

## Local documentation build

1. Build REX so the compilation database exists:

```bash
./build-rex.sh $HOME/rex-install Release
```

2. Build the docs (creates `.venv-docs`, downloads the latest MrDocs, and runs MkDocs):

```bash
scripts/build-docs
```

The output site is written to `_site`.
