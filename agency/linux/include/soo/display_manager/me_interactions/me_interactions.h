#ifndef ME_INTERACTIONS_H
#define ME_INTERACTIONS_H

#include <soo/dev/vfb.h>

#define MAX_MES 20
#define BASE_TIME 10

/* Struct containing variable parameters of an ME */
struct me_display_param{
  /* Boolean indicating whether or not fb shall be displayed */
  uint8_t display_fb;
  /* Display time limit */
  int32_t time_limit;
};

/* struct containing everything about an ME */
struct me_data{
  /* ME id */
  uint32_t id;
  /* display parameters of this ME */
  struct me_display_param* display_params;
  /* Info about ME's framebuffer */
  uint32_t* fb;
  /* Informations relevant ? */
  uint8_t occupied;
};

struct me_interactions_data{
  uint8_t nb_MEs;
  struct me_data* me_datas[MAX_MES];
  /* struct for high resolution timer */
  struct hrtimer timer_me;
  /* ktime to set timer value */
  ktime_t ktime;
};


int me_interactions_init(void);

/* Callback used when new MEs arrive */
void new_me_callback(struct vfb_fb *fb);

#endif /* ME_INTERACTIONS_H */
