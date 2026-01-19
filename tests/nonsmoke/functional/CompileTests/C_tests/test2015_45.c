struct numeric_resolv;

static void numeric_step ( struct numeric_resolv *numeric ) { }

// Unparsed as:
// void (*step)(void *) = (((void *(struct numeric_resolv *))((void *)0)) == 0L?0L : 0L);
// Original code:
// void ( * step ) ( void *object ) = ( ( ( ( typeof ( numeric_step ) * ) ((void *)0) ) == 0L) ? 0L : 0L );
void ( * step ) ( void *object ) = ( ( ( ( typeof ( numeric_step ) * ) ((void *)0) ) == 0L) ? 0L : 0L );
