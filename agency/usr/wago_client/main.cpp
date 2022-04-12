/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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

#include <iostream>
#include <unistd.h>
#include "ledcontrol.h"

#define SEC                 1000000
#define MSEC                1000
#define SLEEP               1 * SEC

/**
 * @brief Simulate commands received by the backend for testing purpose
 * 
 */
void test_func()
{
    int count = 0;
    std::vector<int> ids{1, 2, 3, 4, 5, 6};
    LED::Ledctrl ctl;

    ctl.init();

    ctl.turn_all_off();

    while(1) 
    {
        if (count > 5)
        {
            ctl.turn_all_on();
        }
        else
        {
            ctl.turn_on(ids);
        }
        
        usleep(SLEEP);

        if (count > 5)
        {
            ctl.turn_all_off();
            usleep(SLEEP);
            count = 0;
        }
        else
        {
            ctl.turn_off(ids);
            for (int& x : ids) (x < 7) ? x += 6 : x -= 6;
        }
        
        count++;
    }
}

int main(int argc, char *argv[])
{
    LED::Ledctrl ctl;

    ctl.init();

    /** Start infinite processing loop **/
    return ctl.main_loop();
}