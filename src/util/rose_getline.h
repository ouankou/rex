#ifndef Rose_getline_H
#define Rose_getline_H

#include "rosedll.h"

#include <istream>

#include <stdio.h>

#include <string>

#include <unistd.h>

/** Reads a line of text from a stream.
 *
 *  This function reads an entire line from `stream,` storing the text
 * (including the newline and a terminating null character) in a buffer and
 * storing the buffer address in `lineptr.`  This documentation is copied from
 * the GNU source code with a few formatting modifications and name changes.
 * ROSE provides its own getline-style implementation for portability.
 *
 *  Before calling rose_getline(), you should place in `lineptr` the address of
 * a buffer `n` bytes long, allocated with malloc().  If this buffer is long
 * enough to hold the line, rose_getline() stores the line in this buffer.
 * Otherwise, rose_getline() makes the buffer bigger using realloc(), storing
 * the new buffer address back in `lineptr` and the increased size back in `n`.
 *
 *  If you set `lineptr` to a null pointer, and `n` to zero, before the call,
 * then rose_getline() allocates the initial buffer for you by calling malloc().
 *
 *  In either case, when rose_getline() returns, `lineptr` points to the text
 * of the line.
 *
 *  When rose_getline() is successful, it returns the number of characters read
 * (including the newline, but not including the terminating null).  This value
 * enables you to distinguish NUL characters that are part of the line from the
 * NUL character inserted as a terminator.
 *
 *  This is the recommended way to read lines from a stream.  The alternative
 * standard functions are unreliable.
 *
 *  If an error occurs or end of file is reached without any bytes read,
 *  getline() returns -1.
 *
 * @param lineptr Pointer to the buffer pointer (may be null to allocate).
 * @param n Pointer to the buffer length in bytes.
 * @param stream Input stream.
 * @return Number of characters read, or -1 on EOF/error.
 *
 * @{ */
ROSE_UTIL_API ssize_t rose_getline(char **lineptr, size_t *n, FILE *stream);
ROSE_UTIL_API ssize_t rose_getline(char **lineptr, size_t *n,
                                   std::istream &stream);
/** @} */

/** Reads a line of text from a stream.
 *
 *  This function reads an entire line from `stream` and returns the line along
 * with any line termination characters that were present.  Returns an empty
 * string when the end of the stream is reached. NUL characters that appear as
 * part of the input are preserved in the return value.
 *
 * @param stream Input stream.
 * @return Line content including terminators, or empty string on EOF.
 *
 * @{ */
ROSE_UTIL_API std::string rose_getline(FILE *stream);
ROSE_UTIL_API std::string rose_getline(std::istream &stream);
/** @} */

#endif
