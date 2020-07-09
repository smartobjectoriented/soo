#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/fb.h>

#include <soo/vbstore.h>

#include <soo/display_manager/me_interactions/me_interactions.h>
#include <soo/display_manager/local_interfaces/local_interfaces.h>


static int init_me_interaction_data(void);

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

  /*vfb_register_callback(new_me_callback);*/

  printk("Bye from %s\n", __func__);

  return 0;
}
/*
void new_me_callback(struct vfb_fb *fb){
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

    me_inter_data.me_datas[i]->id = fb->domid;

    me_inter_data.me_datas[i]->occupied = 1;

    add_display(me_inter_data.me_datas[i]->id, (uint32_t*) fb->vaddr);
  }
}
*/
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
    me_inter_data.me_datas[i]->display_params->display_fb = 0;
    me_inter_data.me_datas[i]->display_params->time_limit = 0;
  }

  return 0;
}

late_initcall(me_interactions_init);
