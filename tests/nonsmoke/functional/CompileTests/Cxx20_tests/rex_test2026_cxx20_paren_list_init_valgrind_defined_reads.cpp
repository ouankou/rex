struct RexTest2026ParenListInit {
  int count;
  double scale;
};

RexTest2026ParenListInit
rex_test2026_cxx20_paren_list_init_valgrind_defined_reads() {
  return RexTest2026ParenListInit(4, 2.5);
}
