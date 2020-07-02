#ifndef LOCAL_INTERFACES_H
#define LOCAL_INTERFACES_H

#define MAX_FBS 9

/* Partition for screen, containing base adress and x and y size for writing */
struct partition{
  uint32_t* base;
  uint32_t x;
  uint32_t y;
};

struct screen_partitioning{
  /* Total nuber of displays */
  uint32_t nb_displays;
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
  /* Screen base address to write to */
  uint32_t* base_addr;
};

int local_interfaces_init(void);

#endif /* LOCAL_INTERFACES_H */
