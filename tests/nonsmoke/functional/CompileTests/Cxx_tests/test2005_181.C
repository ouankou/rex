
#include <vector>

namespace Geometry
{

namespace Internals
{

template <typename ElementType>
class Descriptor
{  };


} // end namespace Internals

class Vector3d {};

template <typename CoreMesh>
class ZoneBase
{
      public:
   typedef ZoneBase<CoreMesh>               ZoneType;
   typedef typename Internals::Descriptor<ZoneType>      ZoneDescriptor;
};

template <typename CoreMesh>
class MeshBase
{

   public:
   explicit MeshBase();
   protected:
   mutable std::vector<ZoneBase<CoreMesh> >       mZoneCache;

};

template <class Dimension>
class PolyMesh {
};

template <>
MeshBase<PolyMesh<Vector3d> >::
MeshBase()
{}

}

