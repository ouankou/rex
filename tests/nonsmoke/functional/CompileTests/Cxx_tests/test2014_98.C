template <class T>
class NewLaserMaterialDataBase
   {
   };

namespace LRT_namespace
   {
     class LRTMeshTypes
        {
        };
   }

   namespace NewLaserNamespace {

   // This works fine (because it is a variable declaration):
   // NewLaserMaterialDataBase<LRT_namespace::LRTMeshTypes> X;
   }

// This is a better place and yet we preserve the original bug (which failed to unparse name qualification for the template arguments).
template class NewLaserMaterialDataBase<LRT_namespace::LRTMeshTypes>;
