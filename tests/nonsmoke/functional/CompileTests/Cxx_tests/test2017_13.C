
class wxAppBase
   {
     public:
       // wxAppBase();
       // virtual ~wxAppBase();

       bool GetExitOnFrameDelete() const { return m_exitOnFrameDelete == Yes; }
       // protected:

       enum { Later = -1, No, Yes } m_exitOnFrameDelete;
   };

