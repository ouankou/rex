#include <stddef.h>
#include <stdint.h>

typedef struct CirrusVGAState {
  int x[5];
} CirrusVGAState;

typedef struct {
  size_t offset;
} VMStateField;

typedef struct {
  VMStateField *fields;
} VMStateDescription;

static const VMStateDescription vmstate_cirrus_vga = {
    .fields = (VMStateField[]){{
        // The bug is that this unparsed to be:
        // .offset = ((char (*)[sizeof(int ()[5])])0)
        // .offset = ( ((char(*) [sizeof(typeof(((CirrusVGAState *)0)->x))]) 0 )
        // ),
        .offset = (size_t)(uintptr_t)((char (*)[sizeof(int[5])])0),
    }}};
