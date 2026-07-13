__kernel __attribute__((vec_type_hint(float4)))
__attribute__((work_group_size_hint(8, 4, 2))) void
rex_unparser_hinted(__global float *output) {
  output[get_global_id(0)] = 1.0f;
}

__kernel __attribute__((reqd_work_group_size(16, 2, 1))) void
rex_unparser_required(__global float *output) {
  output[get_global_id(0)] = 2.0f;
}
