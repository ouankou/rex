
// This is the smaller example where the attribute is unparsed in the wrong location.
// The error is that it is unparsed as: static const int var[2] = {(0x00), (0x60)} __attribute__((align(16)));
static const int __attribute__ ((aligned (16))) var[2] = {0x00,0x60};
