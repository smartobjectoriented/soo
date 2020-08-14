#ifndef LOCAL_INTERFACES_H
#define LOCAL_INTERFACES_H

/* Maximum displays that can be handled */
#define MAX_FBS 9

/* Resolution and address of ME framebuffer */
struct vfb_info {
        /* Horizontal resolution */
        uint16_t x;
        /* Vertical resolution */
        uint16_t y;
        /* Framebuffer virtual address */
        uint32_t *vfb_addr;
};

/* Contains everything about screen parts */
struct screen_partitioning {
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

/* Contains parameters to be shared on Bluetooth */
struct screen_parameters {
        /* Maximum number of displays */
        uint8_t max_displays;
};

/* Contains everything related to Local Interfaces */
struct screen_management {
        /* Total number of displays */
        uint8_t nb_displays;
        /* Which part of the screen is occupied or not by which ME (ID defined) */
        int32_t occupation[MAX_FBS];
        /* Size of the framebuffer in each vfb_addr */
        struct vfb_info* vfbs[MAX_FBS];
        /* Informations about screen partitioning */
        struct screen_partitioning* screen_part;
        /* Screen base address to write to */
        uint32_t* base_addr;
        /* Parameters of the screen */
        struct screen_parameters* screen_param;
        /* struct for high resolution timer */
        struct hrtimer timerCompt;
        /* ktime to set timer value */
        ktime_t ktime;
};

/* Init function for Local Interfaces */
int local_interfaces_init(void);
/* Function used to add a display */
int add_display(uint8_t id, uint32_t *fb_addr, uint16_t x, uint16_t y);
/* Function used to remove a display */
int remove_display(uint8_t id);

#endif /* LOCAL_INTERFACES_H */
