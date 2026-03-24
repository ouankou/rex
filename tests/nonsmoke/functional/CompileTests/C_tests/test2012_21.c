
typedef struct builtin *Builtin;

// typedef int (*HandlerFunc) (char *, char **, Options, int);
typedef int (*HandlerFunc)();

struct builtin {
 // struct hashnode node;
 HandlerFunc handlerfunc;
 int minargs;
 int maxargs;
 int funcid;
 char *optstr;
 char *defopts;
};

// static int bin_zpty (char*nam,char**args,Options ops,int func __attribute__((__unused__)));
static int bin_zpty (char*nam,char**args,int func __attribute__((__unused__)));

// static struct builtin bintab[] = { { { ((void *)0), "zpty", 0 }, bin_zpty, 0, -1, 0, "ebdmrwLnt", ((void *)0) }, };
static struct builtin bintab[] = {
    {bin_zpty, 0, -1, 0, "ebdmrwLnt", ((void *)0)},
};
