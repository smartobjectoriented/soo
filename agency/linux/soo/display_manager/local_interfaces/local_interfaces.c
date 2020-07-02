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
#include <soo/display_manager/me_interactions/me_interactions.h>

/* static functions prototypes */
static int init_screen_part(void);
static int color_part(uint8_t n, uint32_t color);
static int repartition_screen(void);
static int update_partition_addresses(void);

#if 0
#define DEBUG
#endif

/* TO DO */
/* Find a way to check when a framebuffer becomes available for initialisation
 * if there are none during init
 * Prevent crashes that could occur when screen is unplugged (to be tested
 * still)
 */

 struct screen_management screen_man;

/*
 * Init function getting the driver created framebuffer and working with it.
 * It will setup the screen_partitioning structure and the offsets going with it
 * to have what we need for further operating with them
 * It is required to have a framebuffer created before we get in this function
 */
int local_interfaces_init(void){

  printk("Hello from %s\n", __func__);

  /* Checking if a framebuffer has been created, failure if not */
  if(registered_fb[0] == NULL){
    printk("registered_fb[0] is NULL, \
      local interfaces init failed l%d function %s\n", __LINE__, __func__);
    return -1;
  }

  /* Going for 32 bits per pixel depth */
  registered_fb[0]->var.bits_per_pixel = 32;
  if(registered_fb[0]->fbops->fb_set_par){
    registered_fb[0]->fbops->fb_set_par(registered_fb[0]);
  }

  /* Framebuffer is unlinked to avoid other devices using it */
  unlink_framebuffer(registered_fb[0]);

  /*
   * Framebuffer and video controller are all set, we can initialise screen
   * screen_partitioning
   */
  init_screen_part();

  /* Setting the number of displays needed for testing */
  screen_man.nb_displays = MAX_FBS;
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

int update_nb_displays(uint8_t n){
  if(n > MAX_FBS) return -1;
  screen_man.nb_displays = n;
  repartition_screen();
  return 0;
}

/* Basic initialisation, taking the needed values from the framebuffer */
static int init_screen_part(void){
  int i;

  screen_man.nb_displays = 1;

  screen_man.screen_part = kzalloc(sizeof(struct screen_partitioning), GFP_KERNEL);
  if(screen_man.screen_part == NULL){
    printk("Allocation problem in %s l%d\n", __func__, __LINE__);
    return -1;
  }

  for(i = 0; i < MAX_FBS; ++i){
    screen_man.occupation[i] = 0;
    screen_man.screen_part->mem_spaces[i] = 0;
  }

  screen_man.screen_part->horizontal_part = 0;
  screen_man.screen_part->vertical_part = 0;
  screen_man.screen_part->x_offset = 0;
  screen_man.screen_part->y_offset = 0;

  screen_man.screen_part->nb_displays = 1;
  screen_man.screen_part->x_total = registered_fb[0]->var.xres;
  screen_man.screen_part->y_total = registered_fb[0]->var.yres;
  screen_man.screen_part->line_size = registered_fb[0]->fix.line_length;
  screen_man.screen_part->size_total = registered_fb[0]->var.yres * screen_man.screen_part->line_size;
  screen_man.screen_part->base_addr = (uint32_t*) registered_fb[0]->screen_base;

  repartition_screen();

  return 0;
}

/* Calculate new offsets and so on for the display to be correct */
static int repartition_screen(void){
  uint8_t nb = screen_man.nb_displays;
  switch(nb){
    case 9 :
    case 8 :
    case 7 :
      screen_man.screen_part->horizontal_part = 3;
      screen_man.screen_part->vertical_part = 3;
      break;
    case 6 :
    case 5 :
      screen_man.screen_part->horizontal_part = 2;
      screen_man.screen_part->vertical_part = 3;
      break;
    case 4 :
    case 3 :
      screen_man.screen_part->horizontal_part = 2;
      screen_man.screen_part->vertical_part = 2;
      break;
    case 2 :
      screen_man.screen_part->horizontal_part = 2;
      screen_man.screen_part->vertical_part = 1;
      break;
    case 1 :
    case 0 :
      screen_man.screen_part->horizontal_part = 1;
      screen_man.screen_part->vertical_part = 1;
      break;
    default :
      printk("Unsupported number of displays for screen partitioning\n");
      return -1;
      break;
  }

  screen_man.screen_part->x_offset = screen_man.screen_part->x_total / screen_man.screen_part->vertical_part;
  screen_man.screen_part->y_offset = screen_man.screen_part->y_total / screen_man.screen_part->horizontal_part;

  update_partition_addresses();

  printk("Screen repartitioned for %d displays\n", nb);
  return 0;
}

/* Calculate the new base addresses of each part of the screen */
static int update_partition_addresses(void){
  int i, j, n;

  n = 0;

  for(i = 0; i < screen_man.screen_part->horizontal_part; ++i){
    for(j = 0; j < screen_man.screen_part->vertical_part; ++j){
      screen_man.screen_part->mem_spaces[n++] = (uint32_t*)(screen_man.screen_part->base_addr +
      ((j * screen_man.screen_part->x_offset) +
      (i * screen_man.screen_part->y_offset * (screen_man.screen_part->line_size/4))));
    }
  }

  return 0;
}

/* Colors part n of the screen with given color (testing purpose function) */
static int color_part(uint8_t n, uint32_t color){
  int i, j;
  uint32_t* address;
  uint32_t x, y, line_size;

  line_size = screen_man.screen_part->line_size;
  address = screen_man.screen_part->mem_spaces[n];
  x = screen_man.screen_part->x_offset;
  y = screen_man.screen_part->y_offset;

  for(i = 0; i < y; ++i){
    for(j = 0; j < x; ++j){
      address[j + i * (screen_man.screen_part->line_size/4)] = color;
    }
  }

  return 0;
}

late_initcall(local_interfaces_init);
