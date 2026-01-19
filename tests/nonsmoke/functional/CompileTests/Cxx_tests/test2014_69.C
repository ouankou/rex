
#include <string>

struct Domain_s;

class DomainGraphViz
{
public:
  explicit DomainGraphViz(Domain_s *domain, std::string prefix = "");

  void graphIndicesAndMixSlots(Domain_s *domain, std::string prefix);
  void graphSpecFrac(Domain_s *domain, std::string prefix);
  void graphIreg(Domain_s *domain, std::string prefix);

  virtual ~DomainGraphViz();

  // private:

};
