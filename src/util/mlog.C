/**
 * mlog.cpp
 * The mlogging facilities
 */
#include "mlog.h"
#include <array>
#include <cstring>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

int mlogLevel = DEFAULT_MLOG_LEVEL;

/**
 * this array is used to get the string of each level, thus
 * it must be matching the value of each element of the MLOG_LEVEL enum
 */
const char *mlogLevelToString_C[] = {"FAIL ", // 0
                                     "FATAL", "ERROR", "WARN ", "NONE ",
                                     "KEY  ", "INFO ", "MARCH", "TRACE",
                                     "DEBUG", "UNDEF"};

/**
 * return the Date/Time in the format of YYYY-MM-DD,HH:MM:SS, totally 16
 * characters
 */
char *getDateTime() {
  static thread_local std::array<char, 22> buffer{};
  time_t curtime;
  struct tm timeinfo;

  curtime = time(NULL);
#ifdef _WIN32
  if (localtime_s(&timeinfo, &curtime) != 0) {
#else
  if (localtime_r(&curtime, &timeinfo) == nullptr) {
#endif
    return buffer.data();
  }

  strftime(buffer.data(), buffer.size(), "%F,%H:%M:%S", &timeinfo);
  return buffer.data();
}

std::string mlogLevelToString_CXX(MLOG_LEVEL_t level) {
  switch (level) {
  case MLOG_LEVEL_FAIL:
  case MLOG_LEVEL_FATAL:
    return (std::string("\033[1;31m") + mlogLevelToString_C[level]); // bold red
  case MLOG_LEVEL_ERROR:
    return (std::string("\033[1;35m") +
            mlogLevelToString_C[level]); // bold purple
  case MLOG_LEVEL_WARN:
    return (std::string("\033[1;33m") +
            mlogLevelToString_C[level]); // bold yellow
  case MLOG_LEVEL_NONE:
  case MLOG_LEVEL_KEY:
  case MLOG_LEVEL_INFO:
  case MLOG_LEVEL_MARCH:
  case MLOG_LEVEL_TRACE:
  case MLOG_LEVEL_DEBUG:
    return std::string(mlogLevelToString_C[level]);
  default:
    return std::string(mlogLevelToString_C[MLOG_LEVEL_UNDEFINED]);
  }
}

void mlogAssertFail_C(const char *subject, const char *expr, const char *file,
                      int line, const char *funcname, const char *format, ...) {
  mlog_C(MLOG_LEVEL_FAIL, subject, file, line, funcname, "%s\n", expr);
  if (format != NULL && strlen(format) != 0) {
    FILE *mlogFile = stderr;
    fprintf(mlogFile, "\t\t"); // indentation
    va_list l;
    va_start(l, format);
    vfprintf(mlogFile, format, l);
    va_end(l);
    fflush(mlogFile);
  }
  abort();
}

void mlog_C(MLOG_LEVEL_t level, const char *subject, const char *file, int line,
            const char *funcname, const char *format, ...) {
  FILE *mlogFile = stderr;
  switch (level) {
  case MLOG_LEVEL_FAIL:
  case MLOG_LEVEL_FATAL:
    fprintf(mlogFile, "\033[1m\033[31m"); // bold red
    fprintf(mlogFile, "%s: ",
            mlogLevelToString_C[level]); // print the string of each level
    break;
  case MLOG_LEVEL_ERROR:
    fprintf(mlogFile, "\033[1m\033[35m"); // bold purple
    fprintf(mlogFile, "%s: ",
            mlogLevelToString_C[level]); // print the string of each level
    break;
  case MLOG_LEVEL_WARN:
    fprintf(mlogFile, "\033[1m\033[33m"); // bold yellow
    fprintf(mlogFile, "%s: ",
            mlogLevelToString_C[level]); // print the string of each level
    break;
  case MLOG_LEVEL_NONE:
  case MLOG_LEVEL_KEY:
  case MLOG_LEVEL_INFO:
  case MLOG_LEVEL_MARCH:
  case MLOG_LEVEL_TRACE:
  case MLOG_LEVEL_DEBUG:
    fprintf(mlogFile, "%s: ",
            mlogLevelToString_C[level]); // print the string of each level
    break;
  default:
    fprintf(mlogFile, "%s: ", mlogLevelToString_C[MLOG_LEVEL_UNDEFINED]);
    break;
  }

  /* char * dateTime = getDateTime(); */
  fprintf(mlogFile, "%s\t[%s:%d,\t%s]: ", subject, file, line, funcname);
  fprintf(mlogFile, "\033[0m"); // reset the color

  va_list l;
  va_start(l, format);
  vfprintf(mlogFile, format, l);
  va_end(l);
  fflush(mlogFile);
  /* fprintf(f, "\n"); */
}

void mlogMore_C(const char *format, ...) {
  if (format == NULL || strlen(format) == 0)
    return;
  FILE *mlogFile = stderr;
  fprintf(mlogFile, "\t\t"); // indentation
  va_list l;
  va_start(l, format);
  vfprintf(mlogFile, format, l);
  va_end(l);
  fflush(mlogFile);
}
