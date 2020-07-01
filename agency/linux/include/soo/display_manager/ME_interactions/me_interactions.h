#ifndef ME_INTERACTIONS_H
#define ME_INTERACTIONS_H

#include <soo/display_manager/local_interfaces/local_interfaces.h>

#define MAX_MES 9

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
  struct fb_info* f_inf;
};

struct me_interactions_data{
  uint8_t nb_MEs;
  struct me_data** me_datas;
};

struct me_interactions_data me_inter_data;

int me_interactions_init(void);

#endif /* ME_INTERACTIONS_H */
