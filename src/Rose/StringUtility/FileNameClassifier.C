#include "Rose/StringUtility/FileUtility.h"

#include "Rose/StringUtility.h"

#include <algorithm>

#include <cassert>

#include <filesystem>

#include <map>

#include "rose_config.h"

#include "rose_path_resolver.h"

#include <sys/utsname.h>

using namespace std;
using namespace Rose;

namespace {
bool hasPathComponents(const std::filesystem::path &fileName,
                       const char *directory, const char *child) {
  for (auto it = fileName.begin(); it != fileName.end(); ++it) {
    if (*it != directory)
      continue;
    auto next = it;
    if (++next != fileName.end() && *next == child)
      return true;
  }
  return false;
}

vector<string> splitPathComponents(const string &path) {
  vector<string> components;
  Rose::StringUtility::splitStringIntoStrings(path, '/', components);
  return components;
}

void trimTrailingSeparatorComponents(vector<string> &components) {
  while (!components.empty() && components.back().empty())
    components.pop_back();
}

bool componentPathStartsWith(const vector<string> &path,
                             const vector<string> &prefix) {
  if (prefix.size() > path.size())
    return false;
  return std::equal(prefix.begin(), prefix.end(), path.begin());
}

const char *LINUX_INCLUDES[] = {
    "acpi",  "asm",      "asm-generic", "asm-i386", "asm-x86_64", "config",
    "linux", "math-emu", "media",       "net",      "pcmcia",     "rxrpc",
    "scsi",  "sound",    "video",       NULL};

const char *GLIBC_INCLUDES[] = {
    "_G_config.h",   "a.out.h",    "aio.h",       "aliases.h",  "alloca.h",
    "ar.h",          "argp.h",     "argz.h",      "arpa",       "assert.h",
    "bits",          "byteswap.h", "complex.h",   "cpio.h",     "crypt.h",
    "ctype.h",       "dirent.h",   "dlfcn.h",     "elf.h",      "endian.h",
    "envz.h",        "err.h",      "errno.h",     "error.h",    "execinfo.h",
    "fcntl.h",       "features.h", "fenv.h",      "fmtmsg.h",   "fnmatch.h",
    "fpu_control.h", "fstab.h",    "fts.h",       "ftw.h",      "gconv.h",
    "getopt.h",      "glob.h",     "gnu",         "grp.h",      "iconv.h",
    "ieee754.h",     "ifaddrs.h",  "inttypes.h",  "langinfo.h", "lastlog.h",
    "libgen.h",      "libintl.h",  "libio.h",     "limits.h",   "link.h",
    "locale.h",      "malloc.h",   "math.h",      "mcheck.h",   "memory.h",
    "mntent.h",      "monetary.h", "mqueue.h",    "net",        "netash",
    "netatalk",      "netax25",    "netdb.h",     "neteconet",  "netinet",
    "netipx",        "netpacket",  "netrom",      "netrose",    "nfs",
    "nl_types.h",    "nptl",       "nss.h",       "obstack.h",  "paths.h",
    "poll.h",        "printf.h",   "protocols",   "pthread.h",  "pty.h",
    "pwd.h",         "re_comp.h",  "regex.h",     "regexp.h",   "resolv.h",
    "rpc",           "rpcsvc",     "sched.h",     "scsi",       "search.h",
    "semaphore.h",   "setjmp.h",   "sgtty.h",     "shadow.h",   "signal.h",
    "spawn.h",       "stab.h",     "stdint.h",    "stdio.h",    "stdio_ext.h",
    "stdlib.h",      "string.h",   "strings.h",   "stropts.h",  "sys",
    "syscall.h",     "sysexits.h", "syslog.h",    "tar.h",      "termio.h",
    "termios.h",     "tgmath.h",   "thread_db.h", "time.h",     "ttyent.h",
    "ucontext.h",    "ulimit.h",   "unistd.h",    "ustat.h",    "utime.h",
    "utmp.h",        "utmpx.h",    "values.h",    "wait.h",     "wchar.h",
    "wctype.h",      "wordexp.h",  "xlocale.h",   NULL};

const char *STL_INCLUDES[] = {"algorithm",  "bitset", "complex", "deque",
                              "functional", "list",   "map",     "numeric",
                              "queue",      "set",    "stack",   "utility",
                              "valarray",   "vector", NULL};

// CH (2/2/2010): Standard C++ header files (include new C++ 0x header files)
const char *GLIBCXX_INCLUDES[] = {
    "algorithm", "bitset",  "cassert",    "cctype",    "cerrno",    "climits",
    "clocale",   "cmath",   "complex",    "csetjmp",   "csignal",   "cstdarg",
    "cstddef",   "cstdio",  "cstdlib",    "cstring",   "ctime",     "deque",
    "exception", "fstream", "functional", "iomanip",   "ios",       "iosfwd",
    "iostream",  "istream", "iterator",   "limits",    "list",      "locale",
    "map",       "memory",  "new",        "numeric",   "ostream",   "queue",
    "set",       "sstream", "stack",      "stdexcept", "streambuf", "string",
    "typeinfo",  "utility", "valarray",   "vector",    NULL};

bool includePathMatches(const char *const *headers,
                        const std::filesystem::path &fileName) {
  for (const char *const *header = headers; *header != nullptr; ++header) {
    if (hasPathComponents(fileName, "include", *header))
      return true;
  }
  return false;
}

Rose::StringUtility::FileNameLibrary classifyLibrary(const string &fileName) {
  using namespace std::filesystem;

  const path filePath(fileName);
  if (includePathMatches(LINUX_INCLUDES, filePath)) {
    return Rose::StringUtility::FILENAME_LIBRARY_LINUX;
  }
  if (includePathMatches(GLIBC_INCLUDES, filePath)) {
    return Rose::StringUtility::FILENAME_LIBRARY_C;
  }
  if (hasPathComponents(filePath, "lib", "gcc")) {
    return Rose::StringUtility::FILENAME_LIBRARY_GCC;
  }
  static const RosePathRoots roots = resolveRosePaths(nullptr);
  if (filePath.filename() == "rose.h" ||
      rosePathIsWithinTree(roots.rose_include_root, fileName) ||
      rosePathIsWithinTree(roots.compiler_header_root, fileName) ||
      rosePathIsWithinTree(roots.builtin_header_root, fileName)) {
    return Rose::StringUtility::FILENAME_LIBRARY_ROSE;
  }

  {
    const char **substr = STL_INCLUDES;
    while (*substr != NULL) {
      if (filePath.filename() == *substr)
        return Rose::StringUtility::FILENAME_LIBRARY_STL;
      ++substr;
    }
  }

  path p = filePath;
  while (!p.empty()) {
    path parent = p.parent_path();
    if (parent == p)
      break;
    p = parent;
    if (exists(p / path("rose.h")))
      return Rose::StringUtility::FILENAME_LIBRARY_ROSE;

    const char **substr = GLIBCXX_INCLUDES;
    bool isCxxHeader = true;
    while (*substr != NULL) {
      if (!exists(p / path(*substr))) {
        isCxxHeader = false;
        break;
      }
      ++substr;
    }
    if (isCxxHeader)
      return Rose::StringUtility::FILENAME_LIBRARY_STDCXX;
  }

  return Rose::StringUtility::FILENAME_LIBRARY_UNKNOWN;
}
} // namespace

int Rose::StringUtility::directoryDistance(const string &left,
                                           const string &right) {
  vector<string> lvec = splitPathComponents(left);
  vector<string> rvec = splitPathComponents(right);

  assert(!lvec.empty());
  assert(!rvec.empty());

  lvec.pop_back();
  trimTrailingSeparatorComponents(lvec);
  trimTrailingSeparatorComponents(rvec);

  if (componentPathStartsWith(lvec, rvec))
    return 0;

  vector<string>::iterator l = lvec.begin();
  vector<string>::iterator r = rvec.begin();
  // empty body, this is just to advance the iters as long
  // as elts match
  for (; l != lvec.end() && r != rvec.end() && *l == *r; l++, r++)
    ;

  return distance(l, lvec.end()) + distance(r, rvec.end());
}

Rose::StringUtility::OSType Rose::StringUtility::getOSType() {
  struct utsname val;

  int ret = uname(&val);
  if (ret == -1)
    return OS_TYPE_UNKNOWN;

  string sysname = val.sysname;
  if (sysname == "Linux")
    return OS_TYPE_LINUX;
  else
    return OS_TYPE_UNKNOWN;
}

// Update FileNameInfo class with details about where the
// file comes from and what library it might be a part of
Rose::StringUtility::FileNameClassification
Rose::StringUtility::classifyFileName(const string &fileName,
                                      const string &appPath) {
  return classifyFileName(fileName, appPath, std::map<string, string>(),
                          getOSType());
}

Rose::StringUtility::FileNameClassification
Rose::StringUtility::classifyFileName(const string &fileName,
                                      const string &appPath, OSType os) {
  return classifyFileName(fileName, appPath, std::map<string, string>(), os);
}

Rose::StringUtility::FileNameClassification
Rose::StringUtility::classifyFileName(
    const string &filename, const string &appPath,
    const std::map<string, string> &libPathCollection) {
  return classifyFileName(filename, appPath, libPathCollection, getOSType());
}

// Internal function to above public interface, this version
// is exposed just for testing purposes
Rose::StringUtility::FileNameClassification
Rose::StringUtility::classifyFileName(
    const string &fileName, const string &appPathConst,
    const std::map<string, string> &libPathCollection, OSType os) {
  // First, check if this file exists. Filename may be changed
  // into an illegal one by #line directive
  if (!std::filesystem::exists(fileName))
    return FileNameClassification(FILENAME_LOCATION_NOT_EXIST, "Unknown", 0);

  for (std::map<string, string>::const_iterator it = libPathCollection.begin();
       it != libPathCollection.end(); ++it) {
    if (rosePathIsWithinTree(it->first, fileName)) {
      return FileNameClassification(FILENAME_LOCATION_LIBRARY, it->second,
                                    directoryDistance(fileName, appPathConst));
    }
  }

  // Consider all non-absolute paths to be application code.
  if (!std::filesystem::path(fileName).is_absolute()) {
    return FileNameClassification(FILENAME_LOCATION_USER,
                                  "User", // FILENAME_LIBRARY_USER,
                                  0);
  }

  if (rosePathIsWithinTree(appPathConst, fileName)) {
    return FileNameClassification(FILENAME_LOCATION_USER,
                                  "User", // FILENAME_LIBRARY_USER,
                                  0);
  }

  FileNameLibrary filenameLib = classifyLibrary(fileName);
  if (filenameLib != Rose::StringUtility::FILENAME_LIBRARY_UNKNOWN) {
    return FileNameClassification(FILENAME_LOCATION_LIBRARY, filenameLib,
                                  directoryDistance(fileName, appPathConst));
  }

  if (os == OS_TYPE_LINUX) {
    if (rosePathIsWithinTree("/usr", fileName) ||
        rosePathIsWithinTree("/opt", fileName)) {
      return FileNameClassification(FILENAME_LOCATION_LIBRARY, filenameLib,
                                    directoryDistance(fileName, appPathConst));
    }
  }
  return FileNameClassification(FILENAME_LOCATION_UNKNOWN,
                                "Unknown", // FILENAME_LIBRARY_UNKNOWN,
                                directoryDistance(fileName, appPathConst));
}

const string
Rose::StringUtility::stripDotsFromHeaderFileName(const string &name) {
  if (name.empty() || (name[0] != '.' && name[0] != ' '))
    return name;
  return name.substr(name.find(" ") + 1);
}
