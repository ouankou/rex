
typedef struct PRJob PRJob;

struct PRJob {
	int			links;	
};

static void wstart()
{
         __builtin_offsetof (PRJob, links);
}


