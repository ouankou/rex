typedef int rex_empty_prototype(void);
typedef int rex_unspecified_parameters();

_Static_assert(!__builtin_types_compatible_p(rex_empty_prototype *,
                                             int (*)(int)),
               "an empty prototype rejects arguments");
_Static_assert(__builtin_types_compatible_p(rex_unspecified_parameters *,
                                            int (*)(int)),
               "an unspecified parameter list retains C11 compatibility");
