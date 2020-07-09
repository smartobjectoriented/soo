#ifndef LOCAL_INTERFACES_H
#define LOCAL_INTERFACES_H

#define MAX_FBS 9

struct screen_partitioning{
  /* Array containing all the potential base addresses for screen parts */
  uint32_t* mem_spaces[MAX_FBS];
  /* XxY total screen size */
  uint32_t x_total;
  uint32_t y_total;
  /* Total size of framebuffer */
  uint32_t size_total;
  /* Size of a framebuffer line */
  uint32_t line_size;
  /* Number of horizontal parts (lines) */
  uint32_t horizontal_part;
  /* Number of vertical parts (columns) */
  uint32_t vertical_part;
  /* x size of a framebuffer line and offset used for access */
  uint32_t x_offset;
  /* y size of a framebuffer line and offset used for access */
  uint32_t y_offset;
};

struct screen_management{
  /* Total number of displays */
  uint8_t nb_displays;
  /* Which part of the screen is occupied or not by which ME (ID defined) */
  int32_t occupation[MAX_FBS];
  /* Addresses of the framebuffers to copy from */
  uint32_t* vfb_addr[MAX_FBS];
  /* Informations about screen partitioning */
  struct screen_partitioning* screen_part;
  /* Screen base address to write to */
  uint32_t* base_addr;
};

int local_interfaces_init(void);
int add_display(uint8_t id, uint32_t *fb_addr);
int remove_display(uint8_t id);

#endif /* LOCAL_INTERFACES_H */
