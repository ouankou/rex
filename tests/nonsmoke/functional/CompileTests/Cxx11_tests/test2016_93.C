// Max extend of alignment without extension implemented in legacy frontend 4.12
// (test of code from Raja).
class DepGraphNode128
{
} __attribute__((aligned(128)));

// DQ (12/10/2016): Extended alignment attributes required for RAJA code.
class DepGraphNode256 
{
} __attribute__((aligned(256)));
