#ifndef ROSE_FileSystem_H
#define ROSE_FileSystem_H

#include "mlog.h"
#include "rosedll.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <streambuf>
#include <string>
#include <vector>

namespace Rose {

/** Functions for operating on files in a filesystem. */
namespace FileSystem {

/** Pattern to use when creating temporary files. */
extern const char *tempNamePattern;

/** Name of entities in a filesystem. */
typedef std::filesystem::path Path;

/** Iterate over directory contents non-recursively. */
typedef std::filesystem::directory_iterator DirectoryIterator;

/** Iterate recursively into subdirectories. */
typedef std::filesystem::recursive_directory_iterator
    RecursiveDirectoryIterator;

/** Predicate returning true if path exists.
 *
 * @param path Path to test.
 * @return True if the path exists. */
ROSE_UTIL_API bool isExisting(const Path &path);

/** Predicate returning true for existing regular files.
 *
 * @param path Path to test.
 * @return True if the path exists and is a regular file. */
ROSE_UTIL_API bool isFile(const Path &path);

/** Predicate returning true for existing directories.
 *
 * @param path Path to test.
 * @return True if the path exists and is a directory. */
ROSE_UTIL_API bool isDirectory(const Path &path);

/** Predicate returning true for existing symbolic links.
 *
 * @param path Path to test.
 * @return True if the path exists and is a symbolic link. */
ROSE_UTIL_API bool isSymbolicLink(const Path &path);

/** Predicate returning inverse of @ref isSymbolicLink.
 *
 * @param path Path to test.
 * @return True if the path exists and is not a symbolic link. */
ROSE_UTIL_API bool isNotSymbolicLink(const Path &path);

/** Predicate returning true for matching names.
 *
 *  Returns true if and only if the final component of the path matches the
 * specified regular expression.
 *
 *  For example, to find all files whose base name matches the glob "rose_*" use
 * this (note that the corresponding regular expression is "rose_.*", with a
 * dot):
 *
 * @code
 *  using namespace Rose::FileSystem;
 *  Path top = "/foo/bar"; // where the search starts
 *  std::vector<Path> roseFiles = findAllNames(top,
 * baseNameMatches(std::regex("rose_.*")));
 * @endcode */
class ROSE_UTIL_API baseNameMatches {
  const std::regex &re_;

public:
  baseNameMatches(const std::regex &re) : re_(re) {}
  bool operator()(const Path &path);
};

/** Create a temporary directory.
 *
 *  The temporary directory is created as a subdirectory of the directory which
 * is suitable for temporary files under the conventions of the operating
 * system.  The specifics of how this path is determined are implementation
 * defined (see `std::filesystem::temp_directory_path`).  The created
 * subdirectory has a name of the form "rose-%%%%%%%%-%%%%%%%%" where each "%"
 * is a random hexadecimal digit.  Returns the path to this directory. */
ROSE_UTIL_API Path createTemporaryDirectory();

/** Normalize a path name.
 *
 *  Normalizes a path by removing "." and ".." components to the extent which is
 * possible.
 *
 *  For instance, a name like "/foo/bar/../baz" will become "/foo/baz" and the
 * name "/foo/./baz" will become
 *  "/foo/baz". However, the names "/../foo" and "./foo" cannot be changed
 * because removing the ".." in the first case would place it in a different
 * directory if the name were appended to another name, and in the second case
 * it would convert a relative name to an absolute name. */
ROSE_UTIL_API Path makeNormal(const Path &);

/** Make path relative.
 *
 *  Makes the specified path relative to another path or the current working
 * directory. */
ROSE_UTIL_API Path makeRelative(
    const Path &path, const Path &root = std::filesystem::current_path());

/** Make path absolute.
 *
 *  Makes the specified path an absolute path if it is a relative path.  If
 * relative, then assume `root` is what the path is relative to. */
ROSE_UTIL_API Path makeAbsolute(
    const Path &path, const Path &root = std::filesystem::current_path());

/** Entries within a directory.
 *
 *  Returns a list of entries in a directory--the contents of a
 * directory--without recursing into subdirectories. The return value is a
 * sorted list of paths, each of which contains `root` as a prefix.  If a
 * `select` predicate is supplied then only paths for which the predicate
 * returns true become part of the return value. The predicate is called with
 * the path that would become part of the return value. The `root` itself is
 * never returned and never tested by the predicate.
 *
 *  If `select` is not specified then all entries are returned.
 *
 * @param root Directory to search.
 * @param select Predicate to include paths.
 * @return Sorted list of matching paths.
 *
 * @{ */
template <class Select>
std::vector<Path> findNames(const Path &root, Select select) {
  std::vector<Path> matching;
  if (isDirectory(root)) {
    for (DirectoryIterator iter(root); iter != DirectoryIterator(); ++iter) {
      if (select(iter->path()))
        matching.push_back(iter->path());
    }
  }
  std::sort(matching.begin(), matching.end());
  return matching;
}

ROSE_UTIL_API std::vector<Path> findNames(const Path &root);
/** @} */

/** Recursive list of names satisfying predicate.
 *
 *  Returns a list of entries in a directory and all subdirectories recursively.
 * The return value is a sorted list of paths, each of which contains `root` as
 * a prefix.  If a `select` predicate is supplied then only paths for which the
 *  predicate returns true become part of the return value.  If a `descend`
 * predicate is supplied then this algorithm only recurses into subdirectories
 * for which `descend` returns true.  The predicates are called with the path
 * that would become part of the return value.  The `root` itself is never
 * returned and never tested by the `select` or `descend` predicates.
 *
 *  If `select` is not specified then all entries are returned. If `descend` is
 * not specified then the algorithm traverses into all subdirectories.  Symbolic
 * links to directories are never followed, but are returned if the `select`
 * predicate allows them.
 *
 * @param root Directory to search.
 * @param select Predicate to include paths.
 * @param descend Predicate controlling recursion.
 * @return Sorted list of matching paths.
 *
 * @{ */
template <class Select, class Descend>
std::vector<Path> findNamesRecursively(const Path &root, Select select,
                                       Descend descend) {
  std::vector<Path> matching;
  RecursiveDirectoryIterator end;
  for (RecursiveDirectoryIterator dentry(root); dentry != end; ++dentry) {
    if (select(dentry->path())) {
      matching.push_back(dentry->path());
    }
    if (!descend(dentry->path())) {
      dentry.disable_recursion_pending();
    }
  }
  std::sort(matching.begin(), matching.end());
  return matching;
}

template <class Select>
std::vector<Path> findNamesRecursively(const Path &root, Select select) {
  return findNamesRecursively(root, select, isDirectory);
}

ROSE_UTIL_API std::vector<Path> findNamesRecursively(const Path &root);
/** @} */

/** Copy a file.
 *
 *  Copies the contents of the source file to the destination file, overwriting
 * the destination file if it existed. */
ROSE_UTIL_API void copyFile(const Path &sourceFileName,
                            const Path &destinationFileName);

/** Copy files from one directory to another.
 *
 *  Each of the specified files are copied from their location under `root` to a
 * similar location under `destinationDirectory.` Subdirectories of the
 * destination directory are created as necessary.
 *
 *  Any file whose name is outside the `root` directory will similarly be
 * created outside the `destinationDirectory.` For instance,
 * copyFiles(["bar/baz"], "foo", "frob") will copy "bar/baz" to
 * "frob/../bar/baz" since "bar" is apparently a sibling of "foo", and therefore
 * must be a sibling of "frob".
 *
 *  Throws a `std::filesystem::filesystem_error` on failure.
 *
 * @param files Files to copy, relative to `root`.
 * @param root Root directory for source paths.
 * @param destinationDirectory Destination root directory. */
ROSE_UTIL_API void copyFiles(const std::vector<Path> &files, const Path &root,
                             const Path &destinationDirectory);

/** Recursively copy files.
 *
 *  Get a list of files by recursively matching files under `root` and then copy
 * them to similar locations relative to `destination.` The `root` and
 * `destination` must not overlap.  The `select` and `descend` arguments are the
 * same as for the @ref findNamesRecursively method.
 *
 * @param root Root directory to search.
 * @param destination Destination root directory.
 * @param select Predicate to include paths.
 * @param descend Predicate controlling recursion. */
template <class Select, class Descend>
void copyFilesRecursively(const Path &root, const Path &destination,
                          Select select, Descend descend) {
  std::vector<Path> files = findNamesRecursively(root, select, descend);
  files.erase(files.begin(), std::remove_if(files.begin(), files.end(),
                                            isFile)); // keep only isFile names
  copyFiles(files, root, destination);
}

/** Return a list of all rose_* files */
ROSE_UTIL_API std::vector<Path> findRoseFilesRecursively(const Path &root);

/** Convert a path to a string.
 *
 *  Try not to use this.  Paths contain more information than std::string and
 * the conversion may loose that info.
 *
 * @param path Path to convert.
 * @return String representation of the path. */
ROSE_UTIL_API std::string toString(const Path &path);

/** Load an entire file into an STL container.
 *
 * @param fileName File path to read.
 * @param openMode Stream open mode.
 * @return Container filled with file contents. */
template <class Container>
Container readFile(const std::filesystem::path &fileName,
                   std::ios_base::openmode openMode = std::ios_base::in |
                                                      std::ios_base::binary) {
  using streamIterator = std::istreambuf_iterator<char>;
  std::ifstream stream(fileName.c_str(), openMode);
  if (!stream.good())
    MLOG_ERROR_CXX("UTIL") << "unable to open file " << fileName.string();
  Container container;
  std::copy(streamIterator(stream), streamIterator(),
            std::back_inserter(container));
  if (stream.fail())
    MLOG_ERROR_CXX("UTIL") << "unable to read from file " << fileName.string();
  return container;
}

template <class Container>
void writeFile(const std::filesystem::path &fileName, const Container &data,
               std::ios_base::openmode openMode = std::ios_base::out |
                                                  std::ios_base::binary) {
  std::ofstream stream(fileName.c_str(), openMode);
  if (!stream.good())
    MLOG_ERROR_CXX("UTIL") << "unable to open file " << fileName.string();
  std::ostream_iterator<char> streamIterator(stream);
  std::copy(data.begin(), data.end(), streamIterator);
  stream.close();
  if (stream.fail())
    MLOG_ERROR_CXX("UTIL") << "unable to write to file " << fileName.string();
}

} // namespace FileSystem
} // namespace Rose

#endif
