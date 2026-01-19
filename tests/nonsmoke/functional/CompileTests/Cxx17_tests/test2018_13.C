// Folding expressions

template<typename... Args>
  bool f(Args... args) { 
    return (true + ... + args); // OK
  }
