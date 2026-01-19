// extern "C" {

typedef struct list_tag 
   {
     int a;
     struct list_tag * next;
#if __cplusplus
     list_tag * prev;
#else
     struct list_tag * prev;
#endif
   } * list;

list a;

// }
