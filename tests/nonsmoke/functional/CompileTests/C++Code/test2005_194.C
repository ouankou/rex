

class X{
  public:
    enum Component { none=-1, p, u, v, w } comp_in_staggered_grid;
    void setComponent (X::Component comp);
};

class Y{
  public:
   void func1(const X x, X::Component  comp = X:: none){

  };
  void func2(const X x);
};

void Y::func2(const X x){

  func1(x, X::none);

};



