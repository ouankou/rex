

typedef struct {
   __builtin_va_list ap;
} ScanfState;

void GetInt(ScanfState *state)
{
    *__builtin_va_arg(state->ap,int *);
}

