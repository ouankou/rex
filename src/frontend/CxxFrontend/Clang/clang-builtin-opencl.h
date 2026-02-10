#ifndef SKIP_ROSE_BUILTIN_DECLARATIONS

#ifndef CL_LOCAL_MEM_FENCE
#define CL_LOCAL_MEM_FENCE CLK_LOCAL_MEM_FENCE
#endif

#ifndef get_global_thread_id
#define get_global_thread_id get_global_id
#endif

#ifndef get_local_thread_id
#define get_local_thread_id get_local_id
#endif

#ifndef get_local_thread_size
#define get_local_thread_size get_local_size
#endif

#ifndef sqrtf
#define sqrtf sqrt
#endif

#endif
