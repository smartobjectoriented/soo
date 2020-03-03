#ifndef __SHARED_H__
#define __SHARED_H__

#include <avz/config.h>

typedef struct shared_info shared_info_t;
#define __shared_info(d, s, field) ((s)->field)

typedef struct vcpu_info vcpu_info_t;
#define __vcpu_info(v, i, field)   ((i)->field)

extern vcpu_info_t dummy_vcpu_info;

#define shared_info(d, field)      __shared_info(d, (d)->shared_info, field)
#define vcpu_info(v, field)        __vcpu_info(v, (v)->vcpu_info, field)

#endif /* __SHARED_H__ */
