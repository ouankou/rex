struct Domain_s;

typedef class _string 
   {
     public:
          _string (char*);
} string;

class DomainGraphViz
{
public:
  explicit DomainGraphViz(Domain_s *domain, string prefix = "");

  void graphIndicesAndMixSlots(Domain_s *domain, string prefix);
  void graphSpecFrac(Domain_s *domain, string prefix);
  void graphIreg(Domain_s *domain, string prefix);

  virtual ~DomainGraphViz();

  // private:

};
