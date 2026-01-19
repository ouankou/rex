

// Simpler problem that demonstrates case of: decl->get_symbol_from_symbol_table() != NULL
// WARNING: set_function_modifier is incomplete ...
struct Y
   {
     template<typename T> void foo();
     template<typename T> void foo() const;
};
