#define REX_GDF_VALUE(value) ((value) + 1)

int rex_gdf_macro_expansion_assignment(int input) {
  int result;
  result = REX_GDF_VALUE(input);
  return result;
}
