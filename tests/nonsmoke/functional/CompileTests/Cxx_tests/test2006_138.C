
// Test wchar example:
wchar_t *wchar_ptr_null = 0L;
// wchar_t *wchar_ptr_valid = L"\u00FC";
wchar_t *wchar_ptr_valid = L"\u00FC";

// Wide character literals must contain exactly one code point.
wchar_t wchar_ptr_value = L'\u00FC';

// This is some strange variable name!  And not supported by GNU
char *\u00FC = "u-umlaut variable";
