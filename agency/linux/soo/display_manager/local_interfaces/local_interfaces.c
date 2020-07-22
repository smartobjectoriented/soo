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

#include <soo/display_manager/local_interfaces/local_interfaces.h>
#include <soo/display_manager/me_interactions/me_interactions.h>

#define TIMER_TICK      5    /* 5ms (0.05s)*/
/* For testing */
#define VEXPRESS_X      1024
#define VEXPRESS_Y      768
#define VEXPRESS        1

/* static functions prototypes */
/* Initialises screen_management struct */
static int init_screen_part(void);
/* Colors a part of the screen in given color (testing purpose function) */
static int color_part(uint8_t n, uint32_t color);
/* Updates the screen_partitioning struct according to number of displays */
static int repartition_screen(void);
/* Updates the starting adresses of each part of the screen */
static int update_parts_addresses(void);
/* Used to change the number of displays */
static int update_nb_displays(uint8_t n);
/* Updates the display of part n of the screen */
static int update_part_display(int8_t n);
/* Checks if the displays will have to migrate afer addition or removal of a display */
static int check_fb_mig(uint8_t new, uint8_t old);
/* Function that redraws the content of framebuffer when migration is needed */
static void migrate_data(void);
/* Fill unused parts in black */
void black_fill_part(uint8_t n);
/* Timer callback */
enum hrtimer_restart timer_callback_li (struct hrtimer* timer);

/* 1 for display tests */
#if 0
#define DEBUG
#endif

const uint8_t parts_map[MAX_FBS + 1] = {1, 1, 2, 4, 4, 6, 6, 9, 9, 9};
const uint32_t colors_test[MAX_FBS] = {0xF800, 0xFFFF00FF, 0xFFFF0000, 0xABCD0022, 0x0, 0xFACF000A, 0xAAB0F81F, 0xBBC07FF, 0xBFF8A0};
uint32_t* fake_buffers[MAX_FBS];
/* TO DO */
/* Find a way to check when a framebuffer becomes available for initialisation
 * if there are none during init
 * Prevent crashes that could occur when screen is unplugged (to be tested
 * still)
 * Test with different rpi4 screens
 * Change display management to permit having 3 displays taking the whole screen
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

  /* Framebuffer is unlinked to avoid other devices using it (fbcon) */
  /* unlink_framebuffer(registered_fb[0]); */

  /*
   * Framebuffer and video controller are all set, we can initialise
   * screen_partitioning
   */
  init_screen_part();


  /* For informative purpose */
  printk("Resolution is %dx%d\n", screen_man.screen_part->x_total, screen_man.screen_part->y_total);

  #if defined DEBUG
  /* Fake buffers for testing purpose */
  int i, j;
  for(i = 0; i < MAX_FBS; ++i){
    fake_buffers[i] = kmalloc(screen_man.screen_part->size_total, GFP_KERNEL);
    for(j = 0; j < screen_man.screen_part->size_total/4; ++j){
      fake_buffers[i][j] = colors_test[i];
    }
  }
  /* Tests adding and removing displays */
  add_display(1, fake_buffers[0], screen_man.screen_part->size_total);
  //msleep(3000);
  for(j = 0; j < screen_man.screen_part->size_total/4; ++j){
    fake_buffers[0][j] = 0xFFFFFFFF;
  }
  // add_display(2, fake_buffers[1]);
  // msleep(3000);
  // add_display(3, fake_buffers[2]);
  // msleep(3000);
  // add_display(4, fake_buffers[3]);
  // msleep(3000);
  // add_display(1, fake_buffers[8]);
  // msleep(3000);
  // add_display(5, fake_buffers[7]);
  // msleep(3000);
  // remove_display(5);
  // msleep(3000);
  // remove_display(1);
  // msleep(3000);
  // remove_display(2);
  // msleep(3000);
  // remove_display(21);
  // msleep(3000);
  // remove_display(4);
  // msleep(3000);
  // add_display(6, fake_buffers[5]);
  // msleep(3000);
  // add_display(7, fake_buffers[6]);
  // msleep(3000);
  // add_display(8, fake_buffers[7]);
  // msleep(3000);
  // add_display(9, fake_buffers[8]);
  // msleep(3000);
  // add_display(10, fake_buffers[1]);
  // msleep(3000);
  // add_display(11, fake_buffers[0]);
  #endif
  printk("Bye from %s\n", __func__);
  return 0;
}

int add_display(uint8_t id, uint32_t *fb_addr, uint16_t x, uint16_t y){
  int i;
  hrtimer_cancel(&screen_man.timerCompt);
  /* Is ME already being displayed ? */
  for(i = 0; i < screen_man.nb_displays; ++i){
    /* Check if this ME is already here */
    if(screen_man.occupation[i] != id) continue;
    /* If we already had it we get here */
    /* In case it changed get (potential) new address */
    screen_man.vfbs[i]->vfb_addr = fb_addr;
    screen_man.vfbs[i]->x = x;
    screen_man.vfbs[i]->y = y;
    /* We just update it and we can leave */
    update_part_display(i);
    return 0;
  }

  /* If full do nothing */
  if(screen_man.nb_displays == screen_man.screen_param->max_displays) return -1;

  /* If there's a spot for the new ME, we fill it */
  screen_man.occupation[screen_man.nb_displays] = id;
  screen_man.vfbs[i]->vfb_addr = fb_addr;
  screen_man.vfbs[i]->x = x;
  screen_man.vfbs[i]->y = y;
  update_nb_displays(screen_man.nb_displays + 1);
  hrtimer_start(&screen_man.timerCompt, screen_man.ktime, HRTIMER_MODE_REL);
  return 0;
}

int remove_display(uint8_t id){
  int i;

  hrtimer_cancel(&screen_man.timerCompt);

  /* If there are no displays there's nothing to remove */
  if(screen_man.nb_displays == 0){
    return -1;
  }

  /* Check it's position (by ID) */
  for(i = 0; i < screen_man.nb_displays; ++i){
    if(screen_man.occupation[i] == id) break;
  }

  /* Check if it's an ID we indeed have, nothing to remove otherwise */
  if(i >= screen_man.nb_displays) return -1;

  /* If it's not the last in the array */
  if(i != screen_man.nb_displays - 1){
    /* Left shift other datas */
    for(; i < screen_man.nb_displays - 1; ++i){
      screen_man.occupation[i] = screen_man.occupation[i + 1];
      screen_man.vfbs[i]->vfb_addr = screen_man.vfbs[i + 1]->vfb_addr;
      screen_man.vfbs[i]->x = screen_man.vfbs[i + 1]->x;
      screen_man.vfbs[i]->y = screen_man.vfbs[i + 1]->y;
    }
  }

  screen_man.occupation[screen_man.nb_displays - 1] = -1;
  screen_man.vfbs[screen_man.nb_displays - 1]->vfb_addr = NULL;
  screen_man.vfbs[screen_man.nb_displays - 1]->x = 0;
  screen_man.vfbs[screen_man.nb_displays - 1]->y = 0;
  update_nb_displays(screen_man.nb_displays - 1);

  hrtimer_start(&screen_man.timerCompt, screen_man.ktime, HRTIMER_MODE_REL);

  return 0;
}

static int update_nb_displays(uint8_t n){
  uint8_t mig, i;
  /* Check if we'll need to transfer what's inside fb after repartitioning */
  mig = check_fb_mig(n, screen_man.nb_displays);
  screen_man.nb_displays = n;
  repartition_screen();
  if(mig){
    migrate_data();
  }else{
    update_part_display(n - 1);
  }
  for(i = screen_man.nb_displays; i < screen_man.screen_part->horizontal_part * screen_man.screen_part->vertical_part; ++i){
    black_fill_part(i);
  }
  return 0;
}

/* Basic initialisation, taking the needed values from the framebuffer */
static int init_screen_part(void){
  int i;

  screen_man.screen_part = kzalloc(sizeof(struct screen_partitioning), GFP_KERNEL);
  if(screen_man.screen_part == NULL){
    printk("Allocation problem in %s l%d\n", __func__, __LINE__);
    return -1;
  }

  screen_man.screen_param = kzalloc(sizeof(struct screen_parameters), GFP_KERNEL);
  if(screen_man.screen_param == NULL){
    printk("Allocation problem in %s l%d\n", __func__, __LINE__);
    return -1;
  }

  for(i = 0; i < MAX_FBS; ++i){
    screen_man.vfbs[i] = kzalloc(sizeof(struct vfb_info), GFP_KERNEL);
    if(screen_man.vfbs[i] == NULL){
      printk("Allocation problem in %s l%d\n", __func__, __LINE__);
      return -1;
    }
    screen_man.occupation[i] = -1;
    screen_man.screen_part->mem_spaces[i] = NULL;
  }

  screen_man.screen_part->horizontal_part = 0;
  screen_man.screen_part->vertical_part = 0;
  screen_man.screen_part->x_offset = 0;
  screen_man.screen_part->y_offset = 0;

  screen_man.nb_displays = 0;
  screen_man.screen_param->max_displays = MAX_FBS;
  screen_man.screen_part->x_total = registered_fb[0]->var.xres;
  screen_man.screen_part->y_total = registered_fb[0]->var.yres;
  screen_man.screen_part->line_size = registered_fb[0]->fix.line_length;
  screen_man.screen_part->size_total = registered_fb[0]->var.yres * screen_man.screen_part->line_size;
  screen_man.base_addr = (uint32_t*) registered_fb[0]->screen_base;

  /* Creation of time between callbacks */
  screen_man.ktime = ms_to_ktime(TIMER_TICK);
  /* Timer initialization (monotonic -> jiffies style, mode relatif) */
  hrtimer_init(&screen_man.timerCompt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

  repartition_screen();
  screen_man.timerCompt.function = &timer_callback_li;
  /* Start the timer */
  hrtimer_start(&screen_man.timerCompt, screen_man.ktime, HRTIMER_MODE_REL);

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

  update_parts_addresses();

  printk("Screen repartitioned for %d displays of size %dx%d\n", nb, screen_man.screen_part->x_offset, screen_man.screen_part->y_offset);
  return 0;
}

/* Calculate the new base addresses of each part of the screen */
static int update_parts_addresses(void){
  int i, j, n;

  n = 0;

  for(i = 0; i < screen_man.screen_part->horizontal_part; ++i){
    for(j = 0; j < screen_man.screen_part->vertical_part; ++j){
      screen_man.screen_part->mem_spaces[n++] = (uint32_t*)(screen_man.base_addr +
      ((j * screen_man.screen_part->x_offset) +
      (i * screen_man.screen_part->y_offset * (screen_man.screen_part->line_size/4))));
    }
  }

  return 0;
}

static int update_part_display(int8_t n){
  int i, j;
  uint32_t *dest, *source;
  uint32_t x, y, line_size_dest, line_size_source, x_off, y_off;
  uint32_t col[MAX_FBS] = {0xF800, 0xFFFF00FF, 0xFFFF0000, 0xFFFFFFFF, 0x0, 0xFACF000A, 0xAAB0F81F, 0xBBC07FF, 0xBA0FF800};

  if(n < 0) return -1;

  dest = screen_man.screen_part->mem_spaces[n];
  source = screen_man.vfbs[n]->vfb_addr;

  line_size_dest = screen_man.screen_part->line_size;
  line_size_source = screen_man.vfbs[n]->x * 4;
  x = screen_man.screen_part->x_offset;
  y = screen_man.screen_part->y_offset;
  x_off = (screen_man.vfbs[n]->x / x);
  y_off = (screen_man.vfbs[n]->y / y);

  //printk("x off : %u\ny off : %u\n", x_off, y_off);

  if(source){
    for(i = 0; i < y; ++i){
      for(j = 0; j < x; ++j){
        dest[j + i * (line_size_dest/4)] = source[(j * x_off) + (i * y_off) * (line_size_source/4)];
      }
    }
  }else{
    color_part(n, col[n]);
  }

  return 0;
}
/* Return 0 if migration not needed, 1 if yes */
static int check_fb_mig(uint8_t new, uint8_t old){

  /*printk("New value : %d\nOld value : %d\n", new, old);
  printk("Map value New : %d\nMap value Old : %d\n", parts_map[new], parts_map[old]);*/
  /* If we add/remove a display, mig will be needed when screen parts change */
  return parts_map[new] == parts_map[old] ? 0 : 1;

  return 0;
}

static void migrate_data(void){
  int i;
  /*printk("Migrating Data\n");*/
  for(i = 0; i < screen_man.nb_displays; ++i){
    update_part_display(i);
  }

  return;
}

void black_fill_part(uint8_t n){
  color_part(n, 0);
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

enum hrtimer_restart timer_callback_li (struct hrtimer* timer){
  int i;

  for(i = 0; i < screen_man.nb_displays; ++i){
    update_part_display(i);
  }
  /* We restart the timer */
  hrtimer_start(&screen_man.timerCompt, screen_man.ktime, HRTIMER_MODE_REL);
  return HRTIMER_RESTART;
}

late_initcall(local_interfaces_init);
