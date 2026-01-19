// extern "C" {

typedef struct list_tag 
   {
     int a;
     struct list_tag * next;
     list_tag * prev;
   } * list;

list a;

// }
