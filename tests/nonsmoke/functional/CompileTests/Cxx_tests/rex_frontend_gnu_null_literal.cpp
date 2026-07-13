void *rex_gnu_null_pointer = __null;

bool rex_gnu_null_is_preserved() { return rex_gnu_null_pointer == __null; }

int main() { return rex_gnu_null_is_preserved() ? 0 : 1; }
