struct cuda_traversal {};

// The traversal marker lives in the call-site AST, not in a parameter
// attribute hack.
template <typename LOOP_BODY>
void forall(cuda_traversal, LOOP_BODY loop_body) {}

int main(int argc, char *argv[])
{
   int* value = 0L;

   // This is what users write.
   forall(cuda_traversal(), [=](int i) { value[i] = i; });

   return 0 ;
}
