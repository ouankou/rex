
extern int tolower (int __c) __attribute__ ((__nothrow__));
static int posixly_correct;
extern char *getenv (__const char *__name) __attribute__ ((__nothrow__)) __attribute__ ((__nonnull__ (1))) ;
typedef long unsigned int size_t;
typedef unsigned long int wctype_t;


static int
internal_fnmatch (const char *pattern, const char *string, const char *string_end, _Bool no_leading_period, int flags)
{
  register const char *p = pattern, *n = string;
  register unsigned char c;
// # 41 "fnmatch_loop.c"
  while ((c = *p++) != '\0')
    {
      _Bool new_no_leading_period = 0;

      switch (c)
        {

        case '[':
          {
            const char *p_init = p;
            const char *n_init = n;
            register _Bool not;
            char cold;
            unsigned char fn;

            for (;;) {
              if (c == ']')
                break;
            }

            if (!not)
              return 1;
            break;

          matched:

            do
              {
              ignore_next:
                c = *p++;
              }
            while (c != ']');
            if (not)
              return 1;
          }
          break;


        default:
        normal_match:
          if (n == string_end || c != ((flags & (1 << 4)) ? tolower ((unsigned char) *n) : ((unsigned char) *n)))
            return 1;
        }

      no_leading_period = new_no_leading_period;
      ++n;
    }

  return 1;
}


