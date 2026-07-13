#ifndef REX_ELSA_SEMANTIC_PROBE_COMPAT_H
#define REX_ELSA_SEMANTIC_PROBE_COMPAT_H

// ELSA parser specimens use these names as test-harness probes.  They are not
// language or compiler builtins and must never leak into ordinary REX input.
#define __testOverload(expr, expected) ((int)sizeof(((void)(expr)), 0))

#ifdef __cplusplus
extern "C++" {
int __checkType(...);
int __checkCalleeDefnLine(...);
}
#else
int __checkType();
int __checkCalleeDefnLine();
#endif

#define __rose_elsa_xfailure_0() 0
#define __rose_elsa_xfailure_1(arg) int arg
#define __rose_elsa_xfailure_select(_0, _1, name, ...) name
#define __cause_xfailure(...)                                                  \
  __rose_elsa_xfailure_select(_, ##__VA_ARGS__, __rose_elsa_xfailure_1,        \
                              __rose_elsa_xfailure_0)(__VA_ARGS__)

#endif
