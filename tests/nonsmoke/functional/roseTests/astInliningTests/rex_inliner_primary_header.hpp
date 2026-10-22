inline int rex_inliner_increment(int value) { return value + 1; }

int main() { return rex_inliner_increment(3) == 4 ? 0 : 1; }
