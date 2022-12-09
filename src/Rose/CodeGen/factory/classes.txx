
#ifndef ROSE_CODEGEN_FACTORY_CLASSES_TXX
#define ROSE_CODEGEN_FACTORY_CLASSES_TXX

namespace Rose { namespace CodeGen {

template <typename CRT, typename API>
struct __factory_helper_t<CRT, API, Object::a_class> {
  template <typename... Args>
  static declaration_t<Object::a_class> * instantiate(
      Factory<CRT, API> & factory,
      symbol_t<Object::a_class> * sym,
      SgNamedType * parent,
      Args... args
  );

  template <typename... Args>
  static reference_t<Object::a_class> * reference(
      Factory<CRT, API> & factory,
      symbol_t<Object::a_class> * sym,
      SgNamedType * parent,
      Args... args
  );
};

template <typename CRT, typename API>
template <typename... Args>
declaration_t<Object::a_class> * __factory_helper_t<CRT, API, Object::a_class>::instantiate(
  Factory<CRT, API> & factory,
  symbol_t<Object::a_class> * sym,
  SgNamedType * parent,
  Args... args
) {
  ROSE_ABORT(); // TODO
  return nullptr;
}

template <typename CRT, typename API>
template <typename... Args>
reference_t<Object::a_class> * __factory_helper_t<CRT, API, Object::a_class>::reference(
  Factory<CRT, API> & factory,
  symbol_t<Object::a_class> * sym,
  SgNamedType * parent,
  Args... args
) {
  ROSE_ASSERT(!parent); // TODO case of a field or method
  return sym->get_declaration()->get_type();
}

} }

#endif /*  ROSE_CODEGEN_FACTORY_CLASSES_TXX */

