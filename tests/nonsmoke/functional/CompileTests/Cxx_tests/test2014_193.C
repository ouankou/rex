
namespace std
   {
     class string {};
   }

template <class T> void linearIn(T& a_outputT, const void* const inBuf);
// template <class T> void linearIn(T& a_outputT);

template <> void linearIn(std::string& a_outputT, const void* const a_inBuf);
// template <> void linearIn(std::string& a_outputT);

template <>
void linearIn<std::string>(std::string &a_outputT, const void *const a_inBuf) {}
