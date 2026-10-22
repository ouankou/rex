#include "rose.h"

#include "FileHelper.h"
#include "Rose/FileSystem.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <string>

namespace fs = std::filesystem;
namespace rfs = Rose::FileSystem;

template <class Error, class Operation> void requireError(Operation operation) {
  bool failed = false;
  try {
    operation();
  } catch (const Error &) {
    failed = true;
  }
  ROSE_ASSERT(failed);
}

int main(int argc, char **argv) {
  ROSE_ASSERT(argc <= 2);
  const fs::path root = rfs::createTemporaryDirectory();
  const std::map<std::string, std::function<void()>> cases{
      {"normalize",
       [&] {
         ROSE_ASSERT(rfs::makeNormal("../../file") == "../../file");
         ROSE_ASSERT(rfs::makeNormal("a/../../file") == "../file");
         ROSE_ASSERT(rfs::makeNormal("/../file") == "/file");
         ROSE_ASSERT(rfs::makeNormal("./a/../file") == "file");
         ROSE_ASSERT(
             FileHelper::normalizePath((root / "new" / "file").string()) ==
             (root / "new" / "file").string());
         fs::create_symlink("loop-b", root / "loop-a");
         fs::create_symlink("loop-a", root / "loop-b");
         requireError<fs::filesystem_error>([&] {
           (void)FileHelper::normalizePath((root / "loop-a").string());
         });
       }},
      {"include-components",
       [&] {
         const std::map<std::string, std::pair<std::string, int>> spellings{
             {"../../rex_header.h", {"rex_header.h", 2}},
             {"./../dir/../rex_header.h", {"rex_header.h", 1}},
             {"../dir../rex_header.h", {"dir../rex_header.h", 1}},
             {"dir./rex_header.h", {"dir./rex_header.h", 0}},
             {"dir/../../rex_header.h", {"rex_header.h", 1}},
             {"rex_header.h", {"rex_header.h", 0}},
             {"/dir/../rex_header.h", {"/rex_header.h", 0}}};
         for (const auto &[spelling, expected] : spellings) {
           ROSE_ASSERT(FileHelper::getTextualPart(spelling) == expected.first);
           ROSE_ASSERT(FileHelper::countUpsToParentFolder(spelling) ==
                       expected.second);
         }
       }},
      {"empty-copy",
       [&] {
         std::ofstream(root / "empty").close();
         rfs::copyFile(root / "empty", root / "empty-copy");
         ROSE_ASSERT(fs::is_regular_file(root / "empty-copy"));
         ROSE_ASSERT(fs::file_size(root / "empty-copy") == 0);
       }},
      {"missing-copy",
       [&] {
         rfs::writeFile(root / "preserved", std::string("preserved"));
         requireError<fs::filesystem_error>(
             [&] { rfs::copyFile(root / "missing", root / "preserved"); });
         ROSE_ASSERT(rfs::readFile<std::string>(root / "preserved") ==
                     "preserved");
         requireError<fs::filesystem_error>(
             [&] { rfs::copyFile(root / "preserved", root / "preserved"); });
         ROSE_ASSERT(rfs::readFile<std::string>(root / "preserved") ==
                     "preserved");
       }},
      {"recursive-copy",
       [&] {
         fs::create_directories(root / "input/a-directory");
         const std::string content("binary\0content\n", 15);
         rfs::writeFile(root / "input/0-first", content);
         rfs::writeFile(root / "input/a-directory/first", content);
         rfs::writeFile(root / "input/z-last", content);
         rfs::copyFilesRecursively(
             root / "input", root / "output",
             [](const fs::path &) { return true; }, rfs::isDirectory);
         ROSE_ASSERT(rfs::readFile<std::string>(
                         root / "output/a-directory/first") == content);
         ROSE_ASSERT(rfs::readFile<std::string>(root / "output/z-last") ==
                     content);
         ROSE_ASSERT(rfs::readFile<std::string>(root / "output/0-first") ==
                     content);
         const auto copied =
             rfs::findNamesRecursively(root / "output", rfs::isFile);
         ROSE_ASSERT(copied.size() == 3);
       }},
      {"read-error",
       [&] {
         requireError<std::ios_base::failure>(
             [&] { rfs::readFile<std::string>(root / "missing"); });
       }},
      {"write-error",
       [&] {
         requireError<std::ios_base::failure>([&] {
           rfs::writeFile(root / "missing-parent/file", std::string("data"));
         });
#if defined(__linux__)
         // /dev/full accepts open() but rejects buffered writes at close().
         requireError<std::ios_base::failure>(
             [&] { rfs::writeFile("/dev/full", std::string("data")); });
#endif
       }},
      {"regex-lifetime",
       [&] {
         auto matcher = [] {
           const std::regex pattern("rex_.*");
           return rfs::baseNameMatches(pattern);
         }();
         ROSE_ASSERT(matcher("dir/rex_file"));
         ROSE_ASSERT(!matcher("dir/other"));
       }},
  };
  for (const auto &[name, run] : cases) {
    if (argc == 1 || name == argv[1]) {
      std::cout << name << std::endl;
      run();
    }
  }
  if (argc == 2)
    ROSE_ASSERT(cases.count(argv[1]) == 1);
  fs::remove_all(root);
}
