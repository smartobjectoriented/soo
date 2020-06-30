#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/fb.h>

#include <soo/vbstore.h>

#include <soo/display_manager/local_interfaces/local_interfaces.h>

#define MAX_FBS 6

#if 1
#define DEBUG
#endif

int local_interfaces_init(void){
  struct fb_info* fb_inf, fb_copy;
  struct fb_info* fbs[MAX_FBS];
  int32_t* test_ptr;
  int32_t color;
  int i, j;

  printk("Hello from %s\n", __func__);

  fb_inf = registered_fb[0];

  if(fb_inf == NULL){
    printk("fb_inf is NULL\n");
    return -1;
  }

  
  for(i = 0; i < MAX_FBS; i++){
    fbs[i] = kmalloc(sizeof(struct fb_info), GFP_KERNEL);
    if(!fbs[i]) printk("Allocation of fb_infor struct nr : %d failed\n", i);
    memcpy(fbs[i], fb_inf, sizeof(struct fb_info));
    register_framebuffer(fbs[i]);
  }
  fbs[0]->screen_base = fbs[0]->screen_base + 1000;


  #if defined DEBUG
  printk("First smem_length : %d\nFirst screen_size : %d\n", fb_inf->fix.smem_len, fb_inf->screen_size);
  printk("FB infos 1:\nyres : %d\nxres : %d\nbpp : %d\nLine length : %d\n", fb_inf->var.yres, fb_inf->var.xres,
            fb_inf->var.bits_per_pixel, fb_inf->fix.line_length);

  fb_inf->var.bits_per_pixel = 32;

  fb_inf->fbops->fb_set_par(fb_inf);

  printk("Second smem_length : %d\nSecond screen_size : %d\n", fb_inf->fix.smem_len, fb_inf->screen_size);
  printk("FB infos 2:\nyres : %d\nxres : %d\nbpp : %d\nLine length : %d\n", fb_inf->var.yres, fb_inf->var.xres,
            fb_inf->var.bits_per_pixel, fb_inf->fix.line_length);
  #endif

  test_ptr = (uint32_t*) fb_inf->screen_base;

  color = 0xF0;
  for(i = 0; i < fb_inf->var.yres; ++i){
    if(i > fb_inf->var.yres/2) color = 0xFF;
    for(j = 0; j < fb_inf->var.xres; ++j){
      test_ptr[j + i * fb_inf->var.xres] = color;
    }
  }
  color = 0;
  for(i = 0; i < fb_inf->var.yres; ++i){
    if(i > fb_inf->var.yres/2) color = 0xFF;
    for(j = 0; j < fb_inf->var.xres; ++j){
      test_ptr[j + i * fb_inf->var.xres] = color;
    }
  }

  printk("Display modifier test done\n");
  printk("Bye from %s\n", __func__);
  return 0;
}

static int local_interfaces_fillfb(void* dest, void* src, size_t size){
  size_t i;
  int32_t *from, *to;
  for(i = 0; i < size / sizeof(int32_t); ++i){
    *to = *from;
  }
}

late_initcall(local_interfaces_init);
