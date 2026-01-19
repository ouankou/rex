template <typename t_Type1, typename t_Type2>
class map {
};

struct string {
};

namespace namespace1 {

  template <typename t_t_parm1>
  using t_Class1 =
  map<string, string>;

  class Class2 {
  public:
    virtual t_Class1<double> func1() const = 0;
  };
}

namespace1::t_Class1<double> var1;
namespace1::t_Class1<double> var2;
