#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");

const cases = [
  {
    name : "wasm-driver-resource-contract",
    mode : "plain",
    filename : "driver_resource_contract.c",
    source : `#include <stddef.h>

#if !defined(__clang_major__) || __clang_major__ != 23
#error "REX WASM must use the configured LLVM 23 in-process frontend"
#endif

int main(void) {
  size_t value = sizeof(int);
  return value == 0;
}
`,
    expectedFiles : [ "rose_driver_resource_contract.c" ],
  },
  {
    name : "plain-c-round-trip",
    mode : "plain",
    filename : "plain_round_trip.c",
    source : `int square(int x) {
  return x * x;
}

int main(void) {
  return square(3);
}
`,
    expectedFiles : [ "rose_plain_round_trip.c" ],
  },
  {
    name : "plain-cxx-round-trip",
    mode : "plain",
    filename : "plain_class.cpp",
    source : `struct Accumulator {
  int value;

  int add(int x) const {
    return value + x;
  }
};

int main() {
  Accumulator acc{3};
  return acc.add(4);
}
`,
    expectedFiles : [ "rose_plain_class.cpp" ],
  },
  {
    name : "plain-cxx-uppercase-c",
    mode : "plain",
    filename : "plain_caps.C",
    source : `class Counter {
 public:
  int value() const { return 7; }
};

int main() {
  Counter counter;
  return counter.value();
}
`,
    expectedFiles : [ "rose_plain_caps.C" ],
  },
  {
    name : "openmp-ast",
    mode : "omp_ast",
    filename : "omp_parallel.c",
    source : `int main(void) {
  int sum = 0;
#pragma omp parallel reduction(+:sum)
  {
    sum += 1;
  }
  return sum;
}
`,
    expectedFiles : [ "rose_omp_parallel.c" ],
  },
  {
    name : "openmp-lowering",
    mode : "omp_lowering",
    filename : "rodinia_axpy_multi_like.c",
    source : `#define RODINIA_AXPY_SIZE 64
#define RODINIA_AXPY_TEAMS 4
#define RODINIA_AXPY_THREADS 64

static float x[RODINIA_AXPY_SIZE];
static float y[RODINIA_AXPY_SIZE];

static void scale_like(float *xv, float factor, int n) {
  int i;

#pragma omp target teams distribute parallel for map(tofrom : xv[0 : n]) \\
    map(to : factor) num_teams(RODINIA_AXPY_TEAMS) \\
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    xv[i] = xv[i] * factor;
  }
}

static void axpy_like(float *xv, float *yv, float a, int n) {
  int i;

#pragma omp target teams distribute parallel for map(to : xv[0 : n], a) \\
    map(tofrom : yv[0 : n]) num_teams(RODINIA_AXPY_TEAMS) \\
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    yv[i] = a * xv[i] + yv[i];
  }
}

int main(void) {
  int n = RODINIA_AXPY_SIZE;
  axpy_like(x, y, 3.0f, n);
  scale_like(y, 2.0f, n);
  return (int)y[0];
}
`,
    expectedFiles : [
      "rose_rodinia_axpy_multi_like.c",
      "rex_lib_rodinia_axpy_multi_like.cu",
    ],
  },
];

cases.push({
  name : "rex_wasm_repeated_runs",
  sequence : [ cases[1], cases[2], cases[1] ],
});

cases.push({
  name : "rex_wasm_filename_language",
  sequence : [
    ...["c++", "cp", "cc", "cxx", "CPP", "C"].map(
        (extension) => ({
          ...cases[2],
          filename : `rex_wasm_filename_extension.${extension}`,
          expectedFiles : [ `rose_rex_wasm_filename_extension.${extension}` ],
        })),
    {
      ...cases[2],
      filename : "rex wasm filename.c++",
      expectedFiles : [ "rose_rex_wasm_filename.c++" ],
    },
    {
      ...cases[2],
      filename : "-rex_wasm_filename.cpp",
      expectedFiles : [ "rose_-rex_wasm_filename.cpp" ],
    },
  ],
  rejectedFilenames : [
    "rex_wasm_filename.f90",
    "rex_wasm_filename.F90",
    "rex_wasm_filename.txt",
    "rex_wasm_filename",
    "",
    ".",
    "..",
    "rex_wasm\0_filename.c",
  ],
});

function usage() {
  console.error(
      "usage: run_rex_wasm_tests.js [--list] | /path/to/rex_wasm.js [--case NAME]",
  );
}

function parseArgs(argv) {
  const options = {
    list : false,
    caseName : "",
    modulePath : "",
  };
  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--list") {
      options.list = true;
    } else if (arg === "--case") {
      options.caseName = argv[++i] || "";
    } else if (!options.modulePath) {
      options.modulePath = arg;
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }
  return options;
}

async function runCase(createRexModule, wasmDir, testCase) {
  const module = await createRexModule({
    locateFile : (file) => path.join(wasmDir, file),
    print : () => {},
    printErr : () => {},
  });
  for (const input of testCase.sequence || [ testCase ]) {
    console.log(`${testCase.name}: start ${input.mode} ${input.filename}`);
    checkResult(module, input);
  }
  for (const filename of testCase.rejectedFilenames || []) {
    const result = JSON.parse(
        module.runRex("int main(void) { return 0; }", filename, "plain"));
    if (result.ok !== false || result.exitCode !== 2 ||
        result.files.length !== 0 ||
        !result.log.includes("unsupported source filename")) {
      throw new Error(`unsupported filename was not rejected: ${
          JSON.stringify(filename)}: ${JSON.stringify(result)}`);
    }
  }
  if (testCase.rejectedFilenames) {
    checkResult(module, cases[2]);
  }
}

function checkResult(module, testCase) {
  const rawResult = module.runRex(
      testCase.source,
      testCase.filename,
      testCase.mode,
  );
  const result = JSON.parse(rawResult);
  if (!result.ok) {
    throw new Error(
        `${testCase.mode} failed with exit ${result.exitCode}\n${result.log}`,
    );
  }

  const names = new Set(result.files.map((file) => file.name));
  for (const expected of testCase.expectedFiles) {
    if (!names.has(expected)) {
      throw new Error(
          `${testCase.mode} did not generate ${expected}; saw ${
              Array.from(names).join(", ")}`,
      );
    }
  }

  for (const file of result.files) {
    if (typeof file.content !== "string" || file.content.length === 0) {
      throw new Error(`${testCase.mode} generated empty file ${file.name}`);
    }
  }

  console.log(
      `${testCase.name}: ${result.files.map((file) => file.name).join(", ")}`,
  );
}

(async () => {
  const options = parseArgs(process.argv.slice(2));
  if (options.list) {
    for (const testCase of cases) {
      console.log(testCase.name);
    }
    return;
  }

  if (!options.modulePath) {
    usage();
    process.exit(2);
  }

  const selectedCases =
      options.caseName
          ? cases.filter((testCase) => testCase.name === options.caseName)
          : cases;
  if (selectedCases.length === 0) {
    throw new Error(`unknown test case: ${options.caseName}`);
  }

  const resolvedModulePath = path.resolve(options.modulePath);
  if (!fs.existsSync(resolvedModulePath)) {
    throw new Error(`missing module: ${resolvedModulePath}`);
  }
  const wasmDir = path.dirname(resolvedModulePath);
  const createRexModule = require(resolvedModulePath);

  for (const testCase of selectedCases) {
    await runCase(createRexModule, wasmDir, testCase);
  }
})().catch((error) => {
  console.error(error && error.stack ? error.stack : String(error));
  process.exit(1);
});
