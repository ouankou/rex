
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

static int sad_hpel_motion_search( // MpegEncContext * s,
                                  int *mx_ptr, int *my_ptr, int dmin,
                                  int src_index, int ref_index,
                                  int size, int h)
{
//    MotionEstContext * const c= &s->me;
   const int penalty_factor; // = c->sub_penalty_factor;
    int mx, my, dminh;
    uint8_t *pix, *ptr;
    int stride; // = c->stride;
    const int flags; // = c->sub_flags;
//  uint32_t __attribute__((unused)) * const score_map= c->score_map; const int __attribute__((unused)) xmin= c->xmin; const int __attribute__((unused)) ymin= c->ymin; const int __attribute__((unused)) xmax= c->xmax; const int __attribute__((unused)) ymax= c->ymax; uint8_t *mv_penalty= c->current_mv_penalty; const int pred_x= c->pred_x; const int pred_y= c->pred_y;
    uint32_t* const score_map; // = c->score_map; 
    const int xmin; //= c->xmin; 
    const int ymin; //= c->ymin; 
    const int xmax; //= c->xmax; 
    const int ymax; //= c->ymax; 
    uint8_t *mv_penalty; //= c->current_mv_penalty; 
    const int pred_x; //= c->pred_x; 
    const int pred_y; //= c->pred_y;

    ((void)0);
}
