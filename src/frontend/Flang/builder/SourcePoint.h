#ifndef ROSE_FLANG_SOURCE_POINT_H_
#define ROSE_FLANG_SOURCE_POINT_H_

namespace Rose::builder {

// Invocation-local lexical position, independent of the physical file's name
// and coordinates. Both values are positive; zero remains the Sage sentinel
// for a producer that has no corresponding source identity.
struct FlangSourcePointIdentity {
  unsigned int order;
  unsigned int physical_occurrence;
};

} // namespace Rose::builder

#endif
