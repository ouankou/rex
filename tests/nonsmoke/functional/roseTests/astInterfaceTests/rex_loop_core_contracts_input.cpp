#include <climits>

long long unroll_descending(const long long *values, long long limit) {
  long long total = 0;
  for (long long index = 20; index > limit; index = index - 3)
    total += values[index];
  return total;
}

long long reference_unroll_descending(const long long *values,
                                      long long limit) {
  long long total = 0;
  for (long long index = 20; index > limit; index = index - 3)
    total += values[index];
  return total;
}

long long unroll_signed_extrema() {
  const long long start = (-9223372036854775807LL - 1);
  long long total = 0;
  for (long long index = start; index < start + 8; index += 3)
    total += index - start + 1;
  return total;
}

long long reference_unroll_signed_extrema() {
  const long long start = (-9223372036854775807LL - 1);
  long long total = 0;
  for (long long index = start; index < start + 8; index += 3)
    total += index - start + 1;
  return total;
}

unsigned long long unroll_unsigned_extrema() {
  const unsigned long long top = ~0ULL;
  unsigned long long total = 0;
  for (unsigned long long index = top - 9; index < top; index += 3)
    total += top - index;
  return total;
}

unsigned long long reference_unroll_unsigned_extrema() {
  const unsigned long long top = ~0ULL;
  unsigned long long total = 0;
  for (unsigned long long index = top - 9; index < top; index += 3)
    total += top - index;
  return total;
}

unsigned long long trip_unsigned_safe() {
  unsigned long long total = 0;
  for (unsigned long long index = ULLONG_MAX - 4; index < ULLONG_MAX; ++index)
    total += ULLONG_MAX - index;
  return total;
}

unsigned long long trip_unsigned_wrap() {
  unsigned long long total = 0;
  for (unsigned long long index = ULLONG_MAX - 3; index < ULLONG_MAX;
       index += 2)
    total += ULLONG_MAX - index;
  return total;
}

long long trip_signed_overflow() {
  long long total = 0;
  for (long long index = LLONG_MAX - 1; index <= LLONG_MAX; ++index)
    total += LLONG_MAX - index;
  return total;
}

long long unroll_indexed_store(long long *values, long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    values[index] += index + 7;
    total += values[index];
  }
  return total;
}

long long reference_unroll_indexed_store(long long *values, long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    values[index] += index + 7;
    total += values[index];
  }
  return total;
}

long long assignment_increment(long long limit) {
  long long index = 17;
  long long total = index;
  for (long long index = 0; index < limit; index = 2 + index)
    total += index;
  return total;
}

long long tiled_descending(long long values[3][12]) {
  long long total = 0;
  for (long long row = 0; row < 3; ++row)
    for (long long column = 10; column > 1; column -= 3)
      total += values[row][column];
  return total;
}

long long reference_tiled_descending(long long values[3][12]) {
  long long total = 0;
  for (long long row = 0; row < 3; ++row)
    for (long long column = 10; column > 1; column -= 3)
      total += values[row][column];
  return total;
}

long long tiled_zero_trip(long long *final_index) {
  long long index = 77;
  long long total = 5;
  for (index = 4; index < 0; ++index)
    total += index;
  *final_index = index;
  return total;
}

long long reference_tiled_zero_trip(long long *final_index) {
  long long index = 77;
  long long total = 5;
  for (index = 4; index < 0; ++index)
    total += index;
  *final_index = index;
  return total;
}

long long tiled_level_two() {
  long long total = 0;
  for (long long row = 0; row < 5; ++row)
    for (long long column = row; column < row + 6; column += 2)
      total = total * 3 + row * 11 + column;
  return total;
}

long long reference_tiled_level_two() {
  long long total = 0;
  for (long long row = 0; row < 5; ++row)
    for (long long column = row; column < row + 6; column += 2)
      total = total * 3 + row * 11 + column;
  return total;
}

long long tiled_level_three() {
  long long total = 0;
  for (long long plane = 0; plane < 3; ++plane)
    for (long long row = 0; row < 4; ++row)
      for (long long column = plane + row; column < plane + row + 5;
           column += 2)
        total = (total * 5 + plane * 17 + row * 7 + column) % 1000003;
  return total;
}

long long reference_tiled_level_three() {
  long long total = 0;
  for (long long plane = 0; plane < 3; ++plane)
    for (long long row = 0; row < 4; ++row)
      for (long long column = plane + row; column < plane + row + 5;
           column += 2)
        total = (total * 5 + plane * 17 + row * 7 + column) % 1000003;
  return total;
}

long long tiled_vector() {
  long long total = 0;
  for (long long row = 0; row < 5; ++row)
    for (long long column = row; column < row + 5; ++column)
      total = (total * 7 + row * 13 + column) % 1000003;
  return total;
}

long long reference_tiled_vector() {
  long long total = 0;
  for (long long row = 0; row < 5; ++row)
    for (long long column = row; column < row + 5; ++column)
      total = (total * 7 + row * 13 + column) % 1000003;
  return total;
}

long long tiled_direct_body(bool execute) {
  long long total = 9;
  if (execute)
    for (long long index = 1; index < 9; index += 2)
      total = total * 2 + index;
  return total;
}

long long reference_tiled_direct_body(bool execute) {
  long long total = 9;
  if (execute)
    for (long long index = 1; index < 9; index += 2)
      total = total * 2 + index;
  return total;
}

long long tiled_continue(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    if ((index & 1) != 0)
      continue;
    total = total * 3 + index;
  }
  return total;
}

long long reference_tiled_continue(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    if ((index & 1) != 0)
      continue;
    total = total * 3 + index;
  }
  return total;
}

long long loop_with_continue(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    if ((index & 1) != 0)
      continue;
    total += index;
  }
  return total;
}

long long zero_stride(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; index += 0)
    total += index;
  return total;
}

long long negative_stride(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; index += -1)
    total += index;
  return total;
}

long long dynamic_stride(long long limit, long long step) {
  long long total = 0;
  for (long long index = 0; index < limit; index += step)
    total += index;
  return total;
}

long long induction_write(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    total += index;
    index += 1;
  }
  return total;
}

long long induction_address(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    long long *address = &index;
    total += *address;
  }
  return total;
}

void mutate_induction(long long &value) { ++value; }

long long induction_call(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    total += index;
    mutate_induction(index);
  }
  return total;
}

long long bound_write(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    total += index;
    --limit;
  }
  return total;
}

long long loop_with_label(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
  again:
    total += index;
    if (total < 0)
      goto again;
  }
  return total;
}

long long loop_with_break(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; ++index) {
    if (index == 4)
      break;
    total += index;
  }
  return total;
}

long long imperfect_nest(long long values[3][3]) {
  long long total = 0;
  for (long long row = 0; row < 3; ++row) {
    total += row;
    for (long long column = 0; column < 3; ++column)
      total += values[row][column];
  }
  return total;
}

long long mismatched_direction(long long limit) {
  long long total = 0;
  for (long long index = 0; index < limit; --index)
    total += index;
  return total;
}

long long malformed_assignment_increment(long long limit) {
  long long total = 0;
  for (long long index = 1; index < limit; index = index * 2)
    total += index;
  return total;
}

int main() {
  const long long values[24] = {1,  3,  5,  7,  11, 13, 17, 19, 23, 29, 31, 37,
                                41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89};
  long long grid[3][12] = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                           {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
                           {25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36}};
  if (unroll_descending(values, 1) != reference_unroll_descending(values, 1))
    return 1;
  if (unroll_signed_extrema() != reference_unroll_signed_extrema())
    return 2;
  if (unroll_unsigned_extrema() != reference_unroll_unsigned_extrema())
    return 3;
  long long transformed_store[11] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31};
  long long reference_store[11] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31};
  if (unroll_indexed_store(transformed_store, 11) !=
      reference_unroll_indexed_store(reference_store, 11))
    return 11;
  for (int index = 0; index < 11; ++index) {
    if (transformed_store[index] != reference_store[index])
      return 12;
  }
  if (tiled_descending(grid) != reference_tiled_descending(grid))
    return 4;
  long long transformed_final = 0;
  long long reference_final = 0;
  if (tiled_zero_trip(&transformed_final) !=
          reference_tiled_zero_trip(&reference_final) ||
      transformed_final != reference_final || transformed_final != 4)
    return 5;
  if (tiled_level_two() != reference_tiled_level_two())
    return 6;
  if (tiled_level_three() != reference_tiled_level_three())
    return 7;
  if (tiled_vector() != reference_tiled_vector())
    return 10;
  if (tiled_direct_body(true) != reference_tiled_direct_body(true) ||
      tiled_direct_body(false) != reference_tiled_direct_body(false))
    return 8;
  if (tiled_continue(13) != reference_tiled_continue(13))
    return 9;
  return 0;
}
