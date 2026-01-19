typedef long zlong;

typedef struct hashnode *HashNode;

typedef struct param *Param;


struct hashnode 
   {
     HashNode next;
     char *nam;
     int flags;
   };



typedef const struct gsu_scalar *GsuScalar;
typedef const struct gsu_integer *GsuInteger;
typedef const struct gsu_float *GsuFloat;
typedef const struct gsu_array *GsuArray;
typedef const struct gsu_hash *GsuHash;

struct gsu_scalar {
    char *(*getfn) (Param);
    void (*setfn) (Param, char *);
    void (*unsetfn) (Param, int);
};

struct gsu_integer {
    zlong (*getfn) (Param);
    void (*setfn) (Param, zlong);
    void (*unsetfn) (Param, int);
};

struct gsu_float {
    double (*getfn) (Param);
    void (*setfn) (Param, double);
    void (*unsetfn) (Param, int);
};

struct gsu_array {
    char **(*getfn) (Param);
    void (*setfn) (Param, char **);
    void (*unsetfn) (Param, int);
};

struct gsu_hash {
 // HashTable (*getfn) (Param);
 // void (*setfn) (Param, HashTable);
    void (*unsetfn) (Param, int);
};

extern zlong poundgetfn (Param pm __attribute__((__unused__)));
extern void nullintsetfn (Param pm __attribute__((__unused__)),zlong x __attribute__((__unused__)));
extern void stdunsetfn (Param pm,int exp __attribute__((__unused__)));

extern zlong errnogetfn (Param pm __attribute__((__unused__)));
extern void errnosetfn (Param pm __attribute__((__unused__)),zlong x);

static const struct gsu_integer pound_gsu = { poundgetfn, nullintsetfn, stdunsetfn };
static const struct gsu_integer errno_gsu = { errnogetfn, errnosetfn, stdunsetfn };


struct param 
   {
     struct hashnode node;

     union 
        {
          void *data;
          char **arr;
          char *str;
          zlong val;
          zlong *valptr;
          double dval;

       // HashTable hash;
        } u;

     union 
        {
          GsuScalar s;
          GsuInteger i;
          GsuFloat f;
          GsuArray a;
          GsuHash h;
        } gsu;

     int base;
     int width;
     char *env;
     char *ename;
     Param old;
     int level;
   };


typedef struct param initparam;

static initparam special_params[] = {
    {{((void *)0), "#", (1 << 1) | (1 << 22) | (1 << 10)},
     {((void *)0)},
     {(GsuScalar)(void *)(&(pound_gsu))},
     10,
     0,
     ((void *)0),
     ((void *)0),
     ((void *)0),
     0},
    {{((void *)0), "ERRNO", (1 << 1) | (1 << 22) | 0},
     {((void *)0)},
     {(GsuScalar)(void *)(&(errno_gsu))},
     10,
     0,
     ((void *)0),
     ((void *)0),
     ((void *)0),
     0}};
