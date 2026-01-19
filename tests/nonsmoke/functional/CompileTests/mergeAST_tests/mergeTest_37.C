// We need a way of knowing when these are different!
// They are both marked as defining declarations!
// Maybe we need the template string in the unique name???

template <typename T> class templated_class_declaration;

// template declaration (definition)
template<typename T>
class templated_class_declaration
   {
     public:
         T t;
};
