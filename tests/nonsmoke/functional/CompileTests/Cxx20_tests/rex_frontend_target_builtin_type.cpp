#if defined(__aarch64__)
typedef __SVFloat32_t rex_aarch64_vector_type;
typedef __mfp8 rex_aarch64_scalar_type;
#elif defined(__riscv)
typedef __rvv_int8mf8_t rex_riscv_vector_type;
#else
#error "this target-builtin specimen requires AArch64 or RISC-V"
#endif
