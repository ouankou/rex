enum {X, Y} val = X;

// This generates the strange case in the legacy frontend/Sage connection of
// case tk_enum in sage_gen_tag_reference.
enum tag {T, F} XYenumVariable = T;
tag anotherEnumVariable;

