const char *rex_ordinary_string = "ordinary";
const wchar_t *rex_wide_string = L"wide";
const char *rex_utf8_string = u8"utf8";
const char16_t *rex_utf16_string = u"utf16";
const char32_t *rex_utf32_string = U"utf32";

const char *rex_raw_string = R"rex_tag(alpha  beta
gamma)rex_tag";
const char *rex_utf8_raw_string = u8R"custom_delim(payload  with  spaces
and a newline)custom_delim";
