template <typename T> struct type_code;

template <> struct type_code<char8_t> {
  static constexpr int value = 8;
};

template <> struct type_code<unsigned char> {
  static constexpr int value = 1;
};

int distinguish(char8_t) { return type_code<char8_t>::value; }
int distinguish(unsigned char) { return type_code<unsigned char>::value; }

using char8_function = int (*)(char8_t);
using byte_function = int (*)(unsigned char);

char8_function select_char8 = distinguish;
byte_function select_byte = distinguish;

int main() {
  return select_char8(char8_t{}) == 8 &&
                 select_byte(static_cast<unsigned char>(0)) == 1
             ? 0
             : 1;
}
