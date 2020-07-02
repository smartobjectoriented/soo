#ifndef ME_INTERACTIONS_H
#define ME_INTERACTIONS_H

#define MAX_MES 20

/* Struct containing variable parameters of an ME */
struct me_display_param{
  /* Boolean indicating whether or not fb shall be displayed */
  uint8_t display_fb;
  /* Display time limit */
  uint32_t time_limit;
};

/* struct containing everything about an ME */
struct me_data{
  /* ME id */
  uint8_t id;
  /* display parameters of this ME */
  struct me_display_param* display_params;
  /* Info about ME's framebuffer */
  uint32_t* fb;
};

struct me_interactions_data{
  uint8_t nb_MEs;
  struct me_data* me_datas[MAX_MES];
};


int me_interactions_init(void);
/* For future use as callback in vfb (when a new ME appears for example) */
/*void cb(struct vfb_fb *fb);*/

#endif /* ME_INTERACTIONS_H */
