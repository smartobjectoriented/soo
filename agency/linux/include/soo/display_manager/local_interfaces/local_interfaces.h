#ifndef LOCAL_INTERFACES_H
#define LOCAL_INTERFACES_H

struct screen_partitioning{
  uint32_t nb_displays;
  struct fb_info** fbs;
  uint32_t x_total;
  uint32_t y_total;
  uint32_t size_total;
  uint32_t line_size;
  uint32_t x_part;
  uint32_t y_part;
  uint32_t x_offset;
  uint32_t y_offset;
};

int local_interfaces_init(void);

#endif /* LOCAL_INTERFACES_H */
