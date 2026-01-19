
namespace Geometry
   {
  // class PolygonalMeshTypeInfo {};

     template <typename MeshTypeInfo> class MeshBase
        {
       // MeshTypeInfo x;
     };

     template <class CoordinateSystem>
     class PolyMesh
        {
          public:
            // Forward declaration oc class that will be defined in an alternative re-entrant namespace definition of: Geometry
               class Zone;
        };
   }

template<typename MeshType>
class X2
   {
     public:
//       MeshType x;
//       typedef int Scalar;
         typedef int Zone_typedef;
   };

template <class CoordinateSystem>
class Geometry::PolyMesh<CoordinateSystem>::Zone
   {
     public:
};

// X2<  Geometry::MeshBase < Geometry::PolygonalMeshTypeInfo > >::Scalar *sp;
// X2<  Geometry::MeshBase < Geometry::PolygonalMeshTypeInfo > >::Scalar sp_0;

// X2<  Geometry::MeshBase < int > >::Scalar sp_1;

// X2<  Geometry::MeshBase < Geometry::PolygonalMeshTypeInfo > >::Zone sp_1;

// This will be unparsed as "Zone sp_1;" if the PolyMesh<CoordinateSystem>::ZoneX 
// class is defined in the second re-entrant definition of the namespace Geometry.
// X2<  Geometry::MeshBase < float > >::Zone sp_1;

X2<  Geometry::PolyMesh < float >::Zone >::Zone_typedef sp_2;
