extern int (^rex_callback)(int);
int rex_call(int value) { return rex_callback(value); }
