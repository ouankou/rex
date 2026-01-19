

class B;

inline void prod(B b );

class B {

  public:

  void prod(int x){
         B z;
        ::prod(z);
  };
};

inline void prod(B b ){};

int main(){return 0;}
