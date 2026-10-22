#include "libxomp.h"

#include <assert.h>

void XOMP_static_even_divide(long start, long orig_size, int chunk_count,
                             int chunk_id, long *chunk_offset,
                             long *chunk_size) {
  assert(orig_size > 0);
  assert(chunk_count > 0);
  assert(chunk_id >= 0);
  long size = orig_size / chunk_count;
  long offset = start + size * chunk_id;
  long remainder = orig_size % chunk_count;
  if (chunk_id < remainder) {
    size++;
  }

  if (chunk_id >= remainder)
    offset += remainder;
  else
    offset += chunk_id;

  *chunk_offset = offset;
  *chunk_size = size;
}
