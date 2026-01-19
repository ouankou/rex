
typedef unsigned short uint8_t;

struct vcpu;

typedef void (*xen_event_channel_notification_t)(struct vcpu *v, unsigned int port);

static xen_event_channel_notification_t xen_consumers[8];

static uint8_t get_xen_consumer(xen_event_channel_notification_t fn)
   {
  unsigned int i;

  // unsigned int x = sizeof(struct { int:-!!(__builtin_types_compatible_p(typeof(xen_consumers), typeof(&xen_consumers[0]))); });

     if ( sizeof(struct { typeof(&xen_consumers[0]); }) )
        {
        }

     return i+1;
   }
