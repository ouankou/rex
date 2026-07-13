#ifndef ROSE_TOKEN_STREAM_INTERVAL_H
#define ROSE_TOKEN_STREAM_INTERVAL_H

struct TokenStreamHalfOpenInterval {
  int begin;
  int end;

  TokenStreamHalfOpenInterval() : begin(0), end(0) {}
  TokenStreamHalfOpenInterval(int input_begin, int input_end)
      : begin(input_begin), end(input_end) {}

  bool empty() const { return begin == end; }
};

enum class TokenStreamIntervalKind {
  leading_whitespace,
  token_subsequence,
  trailing_whitespace,
  else_whitespace
};

#endif
