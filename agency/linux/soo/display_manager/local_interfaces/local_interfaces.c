#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/workqueue.h>

#include <soo/vbstore.h>

#include <soo/display_manager/local_interfaces/local_interfaces.h>

#define NB_MES  9

/* static functions prototypes */
static int init_screen_part(void);
static int color_part(uint8_t n, uint32_t color);
static int repartition_screen(void);
static int update_partition_adresses(void);

#if 0
#define DEBUG
#endif

/*
 * Init function getting the driver created framebuffer and working with it.
 * It will setup the screen_partitioning structure and the offsets going with it
 * to have what we need for further operating with them
 * It is required to have a framebuffer created before we get in this function
 */
int local_interfaces_init(void){
  int32_t* test_ptr;
  int32_t color;
  int i, j, rc;

  struct workqueue_struct *wq_key0;
  struct work_struct key0_job; /* struct for key0 related tasks */

  printk("Hello from %s\n", __func__);

  /* Checking if a framebuffer has been created, failure if not */
  if(registered_fb[0] == NULL){
    printk("registered_fb[0] is NULL, \
      local interfaces init failed l%d function %s\n", __LINE__, __func__);
    return -1;
  }

  /* Going for 32 bits per pixel depth */
  registered_fb[0]->var.bits_per_pixel = 32;
  registered_fb[0]->fbops->fb_set_par(registered_fb[0]);

  /* Framebuffer is unlinked to avoid other devices using it */
  unlink_framebuffer(registered_fb[0]);

  /*
   * Framebuffer and video controller are all set, we can initialise screen
   * screen_partitioning
   */
  init_screen_part();

  /* We start with one display and set the relevant data */
  screen_part.nb_displays = 1;
  screen_part.x_total = registered_fb[0]->var.xres;
  screen_part.y_total = registered_fb[0]->var.yres;
  screen_part.line_size = registered_fb[0]->fix.line_length;
  screen_part.size_total = registered_fb[0]->var.yres * screen_part.line_size;
  screen_part.base_addr = (uint32_t*) registered_fb[0]->screen_base;

  /* Setting the number of displays needed for testing */
  screen_part.nb_displays = NB_MES;
  /* Screen will be partitioned according to the number of displays */
  repartition_screen();
  /* The different part are going to be colored here (example for 9)*/
  color_part(0, 0x0);
  color_part(1, 0xFFFFFFF);
  color_part(2, 0x0);
  color_part(3, 0xFFFFFFF);
  color_part(4, 0x0);
  color_part(5, 0xFFFFFFF);
  color_part(6, 0x0);
  color_part(7, 0xFFFFFFF);
  color_part(8, 0x0);
  color_part(0, 0xFFFFFFF);

  printk("Bye from %s\n", __func__);
  return 0;
}

/* Basic initialisation, taking the needed values from the framebuffer */
static int init_screen_part(void){
  int i;

  for(i = 0; i < MAX_FBS; ++i){
    screen_part.mem_spaces[i] = 0;
  }

  screen_part.horizontal_part = 0;
  screen_part.vertical_part = 0;
  screen_part.x_offset = 0;
  screen_part.y_offset = 0;

  screen_part.nb_displays = 1;
  screen_part.x_total = registered_fb[0]->var.xres;
  screen_part.y_total = registered_fb[0]->var.yres;
  screen_part.line_size = registered_fb[0]->fix.line_length;
  screen_part.size_total = registered_fb[0]->var.yres * screen_part.line_size;
  screen_part.base_addr = (uint32_t*) registered_fb[0]->screen_base;

  repartition_screen();

  return 0;
}

/* Calculate new offsets and so on for the display to be correct */
static int repartition_screen(void){
  uint8_t nb = screen_part.nb_displays;
  switch(nb){
    case 9 :
    case 8 :
    case 7 :
      screen_part.horizontal_part = 3;
      screen_part.vertical_part = 3;
      break;
    case 6 :
    case 5 :
      screen_part.horizontal_part = 2;
      screen_part.vertical_part = 3;
      break;
    case 4 :
    case 3 :
      screen_part.horizontal_part = 2;
      screen_part.vertical_part = 2;
      break;
    case 2 :
      screen_part.horizontal_part = 2;
      screen_part.vertical_part = 1;
      break;
    case 1 :
      screen_part.horizontal_part = 1;
      screen_part.vertical_part = 1;
      break;
    default :
      printk("Unsupported number of displays for screen partitioning\n");
      return -1;
      break;
  }

  screen_part.x_offset = screen_part.x_total / screen_part.vertical_part;
  screen_part.y_offset = screen_part.y_total / screen_part.horizontal_part;

  update_partition_adresses();

  printk("Screen repartitioned for %d displays\n", nb);
  return 0;
}

/* Calculate the new base addresses of each part of the screen */
static int update_partition_adresses(void){
  int i, j, n;

  n = 0;

  for(i = 0; i < screen_part.horizontal_part; ++i){
    for(j = 0; j < screen_part.vertical_part; ++j){
      screen_part.mem_spaces[n++] = (uint32_t*)(screen_part.base_addr +
      ((j * screen_part.x_offset) +
      (i * screen_part.y_offset * (screen_part.line_size/4))));
    }
  }

  return 0;
}

/* Colors part n of the screen with given color */
static int color_part(uint8_t n, uint32_t color){
  int i, j;
  uint32_t* address;
  uint32_t x, y, line_size;

  line_size = screen_part.line_size;
  address = screen_part.mem_spaces[n];
  x = screen_part.x_offset;
  y = screen_part.y_offset;

  for(i = 0; i < y; ++i){
    for(j = 0; j < x; ++j){
      address[j + i * (screen_part.line_size/4)] = color;
    }
  }

  return 0;
}

late_initcall(local_interfaces_init);
