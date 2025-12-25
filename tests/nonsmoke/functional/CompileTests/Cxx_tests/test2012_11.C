// This is a failing test for C code (it gets the parents wrong, but it fails silently, so it appears to work).
// since it does not cause any other sort of failure this is not a high priority, but still not perfect.
// This was reported by Robb.

// Note that legacy frontend will normalize this pull out the class declaration
// if it is named. For now we will accept this normalized from from legacy
// frontend and represent it as such in ROSE.
typedef struct tag { int __val[2]; } __fsid_t_A;

// However, legacy frontend will not normalize this code since the structure is
// not named.
typedef struct { int __val[2]; } __fsid_t_B;
