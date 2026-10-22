These specimens use Clang's standard OpenCL builtins without a REX compatibility
preinclude. No OpenCL runtime is needed for their translation tests.

The imported `nbody.cl` used obsolete names that a REX header globally aliased:
`get_global_thread_id`, `get_local_thread_id`, `get_local_thread_size`,
`CL_LOCAL_MEM_FENCE`, and `sqrtf`. The specimen now directly uses the same standard
operations: `get_global_id`, `get_local_id`, `get_local_size`,
`CLK_LOCAL_MEM_FENCE`, and the floating-point overload of `sqrt`.

This preserves the kernel operations previously selected by those macros while
removing their effect on unrelated user code. The test remains registered; its
translation requirements are unchanged.
