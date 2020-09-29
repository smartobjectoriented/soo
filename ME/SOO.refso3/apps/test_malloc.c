/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016 Xavier Ruppen <xavier.ruppen@heig-vd.ch>
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

#include <common.h>
#include <heap.h>

enum test_t {
        SAME_MALLOC,
        DIFFERENT_MALLOC,
        MIXED_MALLOC,
        SIMPLE_FREE,
        SEVERAL_FREE,
        MIXED_MALLOC_FREE,
        SIMPLE_MERGE,
        DOUBLE_MERGE,
        NO_SPACE_LEFT,
        REPORTED_BUG,
        TEST_COUNT,
	MEMALIGN
};


extern void dump_heap(const char *info);

void allocate(int size, void **ptr)
{
    printk("malloc(%d): ", size);
    *ptr = malloc(size);
    if (!*ptr) {
        printk("returned NULL\n");
    }
    printk("returned %p\n", *ptr);
}

void liberate(void *ptr)
{
    printk("free(%p)\n", ptr);
    free(ptr);
}

int malloc_test(int test_no)
{
    void *ptr[100];


    printk("Test #%d\n", test_no);

    switch (test_no) {
        case SAME_MALLOC:
            allocate(100, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[4]);
            dump_heap(__func__);
            printk("\n");
            break;

        case DIFFERENT_MALLOC:
            allocate(1000, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(2000, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(1300, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[4]);
            dump_heap(__func__);
            printk("\n");
            break;

        case MIXED_MALLOC:
            allocate(1000, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(2000, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(1300, &ptr[4]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[5]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[6]);
            dump_heap(__func__);
            printk("\n");
            break;

        case SIMPLE_FREE:
            allocate(100, &ptr[0]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[0]);
            dump_heap(__func__);
            printk("\n");
            break; 

        case SEVERAL_FREE:
            allocate(1000, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(2000, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(1300, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[4]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[1]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[2]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[3]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[4]);
            dump_heap(__func__);
            printk("\n");
            break;

        case MIXED_MALLOC_FREE:
            allocate(100, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(200, &ptr[2]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[1]);
            dump_heap(__func__);
            printk("\n");

            allocate(300, &ptr[3]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[2]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[3]);
            dump_heap(__func__);
            printk("\n");
            break;

        case SIMPLE_MERGE:
            allocate(100, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[4]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[1]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[2]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[3]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[4]);
            dump_heap(__func__);
            printk("\n");
            break;

        case DOUBLE_MERGE:
            allocate(100, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(100, &ptr[4]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[1]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[3]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[2]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[4]);
            dump_heap(__func__);
            printk("\n");
            break;

        case NO_SPACE_LEFT:
            allocate(1000, &ptr[1]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[2]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[3]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[4]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[5]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[6]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[7]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[8]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[9]);
            dump_heap(__func__);
            printk("\n");
            allocate(1000, &ptr[10]);
            dump_heap(__func__);
            printk("\n");
            break;

        case REPORTED_BUG:
            allocate(50, &ptr[0]);
            dump_heap(__func__);
            printk("\n");
            allocate(200, &ptr[1]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[0]);
            dump_heap(__func__);
            printk("\n");
         
            allocate(500, &ptr[2]);
            dump_heap(__func__);
            printk("\n");

            liberate(ptr[1]);
            dump_heap(__func__);
            printk("\n");
            liberate(ptr[2]);
            dump_heap(__func__);
            printk("\n");
            break;

        case MEMALIGN:
        {
        	void *ptr[400], *ptr2[400];
        	int i;

        	dump_heap("before");
        	for (i = 0; i < 400; i++) {
        		ptr[i] = memalign(15, SZ_1K);
        		ptr2[i] = malloc(12);
        	}

        	for (i = 0; i < 400; i++) {
        	        free(ptr[i]);
        	        free(ptr2[i]);
        	}
        	dump_heap("after");

        	break;
        }

        default:
            break;
    }

    return 0;
}
