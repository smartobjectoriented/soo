#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/ktime.h>
#include <linux/time.h>         /* Needed for the hrtimer */
#include <linux/timer.h>        /* timer support */

#include <soo/vbstore.h>
#include <soo/dev/vfb.h>

#include <soo/display_manager/me_interactions/me_interactions.h>
#include <soo/display_manager/local_interfaces/local_interfaces.h>


#define TIMER_TICK 2000 /* 2000ms (2s) */
#define VEXPRESS_X 1024
#define VEXPRESS_Y 768

/* Function to init ME Interarction structures */
static int init_me_interaction_data(void);
// /* Function to remove an ME */
// static int remove_me(uint16_t id);
/* Timer callback */
enum hrtimer_restart timer_callback_mi (struct hrtimer* timer);

#if 0
#define DEBUG
#endif

struct me_interactions_data me_inter_data;

int me_interactions_init(void){

  printk("Hello from %s\n", __func__);

  if(init_me_interaction_data()){
    printk("init_me_interaction_data failed\n");
    return -1;
  }

  vfb_set_callback_new_domfb(new_me_callback);
  vfb_set_callback_rm_domfb(remove_me_callback);

  printk("Bye from %s\n", __func__);

  return 0;
}

void new_me_callback(struct vfb_domfb *fb, struct fb_info* fb_info){ /* ajouter aux params : , vbus_device *vdev*/
  int i;

  if(me_inter_data.nb_MEs >= MAX_MES){
    printk("Too much MEs in me_interactions\n");
    return;
  }

  ++me_inter_data.nb_MEs;

  for(i = 0; i < MAX_MES; ++i){

    if(me_inter_data.me_datas[i]->occupied) continue;

    me_inter_data.me_datas[i]->display_params->display_fb = 1;
    me_inter_data.me_datas[i]->display_params->time_limit = BASE_TIME;

    me_inter_data.me_datas[i]->id = fb->id;

    me_inter_data.me_datas[i]->occupied = 1;

    add_display(me_inter_data.me_datas[i]->id, (uint32_t*) fb->vaddr, fb_info->var.xres_virtual, fb_info->var.yres_virtual);

    /*
    struct vbus_watch *watch;
    char dir[35];

    watch = kzalloc(sizeof(struct vbus_watch), GFP_ATOMIC);
    sprintf(dir, "device/%01d", vdev->otherend_id);
    vbus_watch_path(vdev, dir, watch, me_leaving_callback);

    */
    break;
  }
}

static int init_me_interaction_data(void){
  int i;

  me_inter_data.nb_MEs = 0;

  /* Allocates all structures with 0s */
  for(i = 0; i < MAX_MES; ++i){
    me_inter_data.me_datas[i] = kzalloc(sizeof(struct me_data), GFP_KERNEL);
    if(me_inter_data.me_datas[i] == NULL){
      printk("Allocation problem in %s l%d\n", __func__, __LINE__);
    }
    me_inter_data.me_datas[i]->display_params = kzalloc(sizeof(struct me_display_param), GFP_KERNEL);
    if(me_inter_data.me_datas[i]->display_params == NULL){
      printk("Allocation problem in %s l%d\n", __func__, __LINE__);
    }
    me_inter_data.me_datas[i]->display_params->time_limit = -1;
  }

  /* Creation of time between callbacks */
  me_inter_data.ktime = ms_to_ktime(TIMER_TICK);
  /* Timer initialization (monotonic -> jiffies style, mode relatif) */
  hrtimer_init(&me_inter_data.timer_me, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  /* Set timer callback */
  me_inter_data.timer_me.function = &timer_callback_mi;
  /* Start the timer */
  hrtimer_start(&me_inter_data.timer_me, me_inter_data.ktime, HRTIMER_MODE_REL);

  return 0;
}

void remove_me_callback(uint16_t id){
  int i;

  /* Nothing to do if there are no MEs */
  if(me_inter_data.nb_MEs <= 0) return;

  /* Check ME's index */
  for(i = 0; i < me_inter_data.nb_MEs; ++i){
    if(me_inter_data.me_datas[i]->id == id) break;
  }

  /* If index is greater than number of MEs there is no ME with this ID */
  if(i >= me_inter_data.nb_MEs) return;

  remove_display(id);

  /* If it's not the last in the array */
  if(i != me_inter_data.nb_MEs - 1){
    /* Left shift other datas */
    for(; i < me_inter_data.nb_MEs - 1; ++i){
      me_inter_data.me_datas[i]->occupied = me_inter_data.me_datas[i + 1]->occupied;
      me_inter_data.me_datas[i]->id = me_inter_data.me_datas[i + 1]->id;
      me_inter_data.me_datas[i]->fb = me_inter_data.me_datas[i + 1]->fb;
      me_inter_data.me_datas[i]->display_params->display_fb = me_inter_data.me_datas[i + 1]->display_params->display_fb;
      me_inter_data.me_datas[i]->display_params->time_limit = me_inter_data.me_datas[i + 1]->display_params->time_limit;
    }
  }

  me_inter_data.me_datas[me_inter_data.nb_MEs - 1]->occupied = 0;
  me_inter_data.me_datas[me_inter_data.nb_MEs - 1]->id = 0;
  me_inter_data.me_datas[me_inter_data.nb_MEs - 1]->fb = NULL;
  me_inter_data.me_datas[me_inter_data.nb_MEs - 1]->display_params->display_fb = 0;
  me_inter_data.me_datas[me_inter_data.nb_MEs - 1]->display_params->time_limit = -1;

  --me_inter_data.nb_MEs;

  return;
}

enum hrtimer_restart timer_callback_mi (struct hrtimer* timer){
  int i;

  for(i = 0; i < me_inter_data.nb_MEs; ++i){
    if(me_inter_data.me_datas[i]->display_params->display_fb){
      if(me_inter_data.me_datas[i]->display_params->time_limit != -1){
        if(me_inter_data.me_datas[i]->display_params->time_limit == 0){
          remove_display(me_inter_data.me_datas[i]->id);
          me_inter_data.me_datas[i]->display_params->time_limit = -1;
        }else{
          --me_inter_data.me_datas[i]->display_params->time_limit;
        }
      }
    }
  }
  /* Restart the timer */
  hrtimer_start(&me_inter_data.timer_me, me_inter_data.ktime, HRTIMER_MODE_REL);
  return HRTIMER_RESTART;
}

late_initcall(me_interactions_init);
