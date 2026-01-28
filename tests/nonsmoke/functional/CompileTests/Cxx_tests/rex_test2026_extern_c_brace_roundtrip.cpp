extern "C" {
int rex_c_function(int value) { return value + 1; }
}

int main() { return rex_c_function(41); }
