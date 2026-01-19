
typedef int Int;
typedef unsigned int UInt;
typedef char HChar;

UInt VG_USERREQ__RUNNING_ON_VALGRIND = 0;

void vgPlain_debugLog ( Int level, const HChar* modulename, const HChar* format, ... )
{
   UInt pid;
   Int indent, depth, i;
// va_list vargs;
// printf_buf buf;

   depth = (unsigned)__extension__ ({volatile unsigned int _zzq_args[6]; volatile unsigned int _zzq_result; _zzq_args[0] = (unsigned int)(VG_USERREQ__RUNNING_ON_VALGRIND); _zzq_args[1] = (unsigned int)(0); _zzq_args[2] = (unsigned int)(0); _zzq_args[3] = (unsigned int)(0); _zzq_args[4] = (unsigned int)(0); _zzq_args[5] = (unsigned int)(0); 
   __asm__ volatile("roll $3,  %%edi ; roll $13, %%edi\n\t" "roll $29, %%edi ; roll $19, %%edi\n\t" "xchgl %%ebx,%%ebx" : "=d" (_zzq_result) : "a" (&_zzq_args[0]), "0" (0) : "cc", "memory" ); _zzq_result; });
}
