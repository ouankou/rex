#if _GNUC_ >= 10

// DQ (7/23/2020): This appears to work for legacy frontend 5.0 with GNU 6.1,
// but fails with legacy frontend 6.0 using GNU 6.1, and it works for legacy
// frontend 6.0 and GNU 10.1, so I'm not clear on where the boundaries are for
// when this works.
#if __has_include(<test2020_12.h>)

#endif

#endif
