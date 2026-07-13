namespace rex_first {
extern "C" int rex_linkage_contract(int);
}

int rex_first::rex_linkage_contract(int value) { return value + 1; }

namespace rex_second {
extern "C" int rex_linkage_contract(int);
}

int main() { return rex_second::rex_linkage_contract(4) == 5 ? 0 : 1; }
