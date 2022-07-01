/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallaccchi@heig-vd.ch>
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

#if 0
#define DEBUG
#endif

#include <soo/enocean/p04.h>
#include <soo/enocean/enocean.h>
#include <soo/debug.h>
#include <printk.h>

#define NAME_P04    "[ p04 ] "


void p04_init(p04_t *ws, uint32_t weatherstation_id){
    ws->id = weatherstation_id;
    p04_reset(ws);
}

void p04_wait_event(p04_t *ws){
    enocean_telegram_t *tel;

    DBG(NAME_P04 "Start wait event\n");

    tel = enocean_get_data();
    if (!tel) {
        DBG(NAME_P04 "tel NULL\n");
        return;
    }    

    DBG("%s: got new enocean data: 0x%02X\n", __func__, tel->data[0]);

    DBG(NAME_P04 "RORG : %d\n", tel->rorg);

    if (tel->rorg != BS_4){
        return;
    }

    printk("[ ID ws        ] : 0x%x\n", ws->id);
    printk("[ ID tel       ] : 0x%x\n", tel->sender_id.val);
    printk("[ Ligth sensor ] : 0x%02x\n", tel->data[0]);// correct
	printk("[ Outdoor temp ] : 0x%02x\n", tel->data[1]);// correct
	printk("[ Wind speed   ] : 0x%02x\n", tel->data[2]);// correct
	printk("[ Identifier   ] : 0x%x\n", ws->identifier);
	printk("[ LRNBit       ] : 0x%x\n", ws->LRNBit);
	printk("[ Day0_Night   ] : 0x%x\n", ws->day0_night1);
	printk("[ Rain         ] : 0x%x\n", ws->rain);
    if(tel->sender_id.val == ws->id){
        DBG(NAME_P04 "Good id\n");
        ws->lightSensor = (uint16_t)(((float)tel->data[0] / (float)255) * LIGHT_SENSOR_MAX); //Modifier le calcul pour pas avoir des valeurs par exemple 0.3 vu que c'est des int
        ws->outdoorTemp = (tel->data[1] / (char)255) * TEMP_DIFF - TEMP_REDUC;
        ws->windSpeed = (tel->data[2] / (char)255) * WIND_SPEED_MAX;
        ws->identifier = (tel->data[3] & ID_MASK) >> 4;
        ws->LRNBit = (bool)((tel->data[3] & LRNB_MASK) >> 5);
        ws->day0_night1 = (bool)((tel->data[3] & DAY_NIGHT_MASK) >> 6);
        ws->rain = (bool)((tel->data[3] & RAIN_MASK) >> 7);
        ws->event = true;
    }else{
        DBG(NAME_P04 "Not good id\n");
    }
}

void p04_reset(p04_t *ws){
    ws->lightSensor = 0;   // 8 bits
    ws->outdoorTemp = 0;   // 8 bits
    ws->windSpeed = 0;     // 8 bits
    ws->identifier = 0;    // 4 bits
    ws->LRNBit = false;
    ws->day0_night1 = false;
    ws->rain = false;
    ws->event = false;
}