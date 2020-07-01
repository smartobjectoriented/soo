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


#if 0
#define DEBUG
#endif



//late_initcall(me_interactions_init);
