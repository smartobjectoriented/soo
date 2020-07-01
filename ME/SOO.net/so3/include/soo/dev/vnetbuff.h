/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2020 Julien Quartier <julien.quartier@bluewin.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef VNETBUFF_H
#define VNETBUFF_H

#include <memory.h>


#define PAGE_COUNT 10
#define CHUNK_SIZE 128
#define CHUNK_COUNT PAGE_SIZE / CHUNK_SIZE

#define READ_ONLY 1


struct vbuff_buff {
        uint8_t *data;
        uint8_t spots[CHUNK_COUNT];
        grant_handle_t grant;
};

struct vbuff_data{
        grant_handle_t grant;
        uint32_t offset;
        uint32_t size;
};

void vbuff_init(struct vbuff_buff* buffs);
void vbuff_free(struct vbuff_buff* buff);

int vbuff_put(struct vbuff_buff* buffs, struct vbuff_data *buff_data, void** data, size_t size);


// TODO fix
#define VBUFF_REGISTER(__name)                  \
                                                \
struct vbuff_buff vbuff_##__name[PAGE_COUNT];   \
\
void inline vbuff_##__name##_init();                                                \
void inline vbuff_##__name##_init(){              \
        vbuff_init(vbuff_##__name);             \
}                                               \
                                                \
void inline vbuff_##__name##_free(){              \
        vbuff_free(vbuff_##__name);             \
}                                               \
                                                \
                                                \
void inline vbuff_##__name##_put(struct vbuff_data *buff_data, void** data, size_t size){         \
        vbuff_put(vbuff_##__name, buff_data, data, size);                                      \
}                                                                                               \




#endif //VNETBUFF_H
