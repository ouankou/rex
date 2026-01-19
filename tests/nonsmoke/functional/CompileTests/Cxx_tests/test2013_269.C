void foo(int x) {
  try {
    class AB {
    public:
      int foo(int x) {
        if (!x)
          throw -1;
        return x;
      }
    } x2;
    x2.foo(0);
  } catch (float) {
  } catch (int) {
    class AC {
    public:
      int foo(int x) { throw x; }
    } x_AC;
  }
}
