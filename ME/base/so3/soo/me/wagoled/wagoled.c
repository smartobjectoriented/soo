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

#include <soo/dev/vwagoled.h>
#include <soo/dev/venocean.h>

#include <me/wagoled.h>

/**
 * @brief Extract command from received data.
 * 			
 * 	Data format PTM 210:
 * 			0			1			2 - 5		6			7
 * 		|	RORG	|	DATA	|	TXID	|	STATUS	|	HASH	|
 *
 * 	see https://www.enocean.com/wp-content/uploads/downloads-produkte/en/products/enocean_modules/ptm-210ptm-215/user-manual-pdf/PTM21x_User_Manual_Sep2021.pdf 
 * 
 * @param buf data received from fronted
 * @param len data length
 * @return int cmd or -1 if error
 */
int read_cmd(char *buf, int len)
{
	uint32_t button_id = 0;
	unsigned char cmd = 0;
	int i;

	if (!buf) {
		pr_err(MEWAGOLED_PREFIX " Buffer empty\n");
		return -1;
	}

	if (len < MIN_LENGTH) {
		pr_err(MEWAGOLED_PREFIX " Error: Not enough data\n");
		return -1;
	}

	if (buf[RORG_OFFS] != RORG_BYTE) {
		pr_err(MEWAGOLED_PREFIX " RORG is incorrect\n");
		return -1;
	}

	cmd = buf[CMD_OFFS];
	
	for (i = 0; i < BUTTON_ID_SIZE; i++) {

		button_id |= (buf[i + ID_OFFS] << ((BUTTON_ID_SIZE - i - 1) * 8));
	}

	if (button_id != BUTTON_ID) {
		pr_err(MEWAGOLED_PREFIX " ID don't match\n");
		DBG("BUTTON id: 0x%08X != 0x%08X, CMD: 0x%02X\n", button_id, BUTTON_ID, cmd);
		return -1;
	}

	return cmd;
}

/**
 * @brief Toggle all the leds in one room
 * 	
 * @param id room id
 */
void toggle_room(int id)
{
	/*** Led ids for 2 rooms. These values are for the demo ***/
	static int room1_ids [] = {1, 2, 3, 4, 5, 6};
	static int room2_ids [] = {7, 8, 9, 10, 11, 12};
	static wago_cmd_t room1_status = LED_ON;
	static wago_cmd_t room2_status = LED_ON;
	size_t size = 6;

	switch (id)
	{
	case ROOM1:
		if (vwagoled_set(room1_ids, size, room1_status) < 0) {
			pr_err(MEWAGOLED_PREFIX " Error vwagoled_set()\n");
			break;
		}
		room1_status = (room1_status == LED_ON) ? LED_OFF :LED_ON;
		break;

	case ROOM2:
		if (vwagoled_set(room2_ids, size, room2_status) < 0) {
			pr_err(MEWAGOLED_PREFIX " Error vwagoled_set()\n");
			break;
		}
		room2_status = (room2_status == LED_ON) ? LED_OFF :LED_ON;
		break;
	
	default:
		break;
	}

}

/**
 * @brief Interpret received command
 * 
 * @param cmd received command
 */
void process_cmd(unsigned char cmd){
	switch(cmd) {
		case SWITCH_IS_UP:
			toggle_room(ROOM1);
			break;
		case SWITCH_IS_DOWN:
			toggle_room(ROOM2);
			break;
		default:
			break;
	}
}

int app_thread_main(void *args) {
	int i;
	int cmd;
	int data_len;
	char data[VENOCEAN_BUFFER_SIZE];

	memset(data, 0, VENOCEAN_BUFFER_SIZE);

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk("Welcome to WAGO led ME\n");

	while (1) {
		DBG(MEWAGOLED_PREFIX "ME wagoled waiting for new data\n");
		if ((data_len = venocean_get_data(data)) > 0) {
			
			DBG("ME wagoled data:\n");
			for (i = 0; i < data_len; i++) {
				DBG(MEWAGOLED_PREFIX "[%d]: 0x%02X\n", i, data[i]);
			}

			cmd = read_cmd(data, data_len);
			if (cmd >= 0) {
				printk(MEWAGOLED_PREFIX " received command: 0x%02X\n", cmd);
				process_cmd(cmd);
			} else {
				pr_err(MEWAGOLED_PREFIX "Wrong cmd: %d\n", cmd);
			}

			/*** reset data ***/
			memset(data, 0, VENOCEAN_BUFFER_SIZE);
		}
	}

	return 0;
}
