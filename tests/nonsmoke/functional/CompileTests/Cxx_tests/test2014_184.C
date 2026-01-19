namespace internal {

// Note that legacy frontend does not require this to be declared in the
// namespace (but gnu g++ (version 4.4.7) does.
struct X
   {
     void foo();
   };

   int variable;

   void X::foo() { int extension = internal::variable; }

}  // namespace internal

