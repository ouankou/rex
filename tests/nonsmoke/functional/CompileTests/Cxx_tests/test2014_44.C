class ostream 
   {
   };

// template <typename T> ostream& operator<< <>(ostream  & a_os,const int a_iv);
// template <typename T> ostream& foo (ostream  & a_os,const int a_iv);

template<typename T> class IndexTM;

// template <typename T> ostream& operator<< (ostream  & a_os,const int a_iv);
template <typename T> ostream& operator<< (ostream  & a_os,const IndexTM<T> & a_iv);

template<typename T> 
class IndexTM
   {
     public:
       // friend std::ostream& operator<< <>(std::ostream  & a_os,const IndexTM & a_iv);
       friend ostream &operator<< <>(ostream &a_os, const IndexTM<T> &a_iv);
   };

