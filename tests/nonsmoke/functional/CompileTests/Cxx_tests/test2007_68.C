
typedef char PRErrorCallbackLookupFn();

static PRErrorCallbackLookupFn *callback_lookup;

void
voidF()
{
	callback_lookup();
}
