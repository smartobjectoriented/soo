#include <iostream>
#include <unistd.h>
#include "ledcontrol.h"

#define SEC                 1000000
#define MSEC                1000
#define SLEEP               1 * SEC

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

    return ctl.main_loop();
}