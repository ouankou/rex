// This test code demonstrates a bug where the const modifier is lost in the unparsing when 
// applied to a typedef type.  Only works with a typedef type!  How bizzare ...

typedef float Float;

float x0;
const float x1 = 0.0;

// const is lost when used with a typedef type (so it does not have anything to do with function parameters)
const Float x2 = 0.0;

