
/**********************************************************************************
 * FILE : key_ctrl.c
 * Description:
 * Author: Kevin He
 * Created On: 2020-05-21 , At 23:05:23
 * Modifiled On : 
 * Version : 0.0.1
 * Information :
 **********************************************************************************/

#include "key_ctrl.h"
#include <stdint.h>


#define KEY_DEBOUNCE_TIME_MS		10
#define KEY_LONG_PRESS_TIME_MS		1500

#define key_bit(i)		(1<<(i))
#define key_get_bit(value, i)	((value) & key_bit(i))
#define key_set_bit(value, i)	((value) |= key_bit(i))


typedef uint8_t key_event_t;
#define KEY_EVENT_NONE		0
#define KEY_EVENT_SHORT		1
#define KEY_EVENT_LONG		2

#define KEY_EVENT_UP		3
#define KEY_EVENT_IDLE		4


typedef uint16_t key_value_t;
typedef uint16_t key_timestamp_t;

typedef void (*key_event_cb_t)(uint8_t key_id, key_event_t event);

#if 1
typedef enum key_id
{
	KEY0,
	KEY1,	
	KEY2,
	KEY3,
	KEY4,

	KEY_BELL,
	KEY_ESC,
	KEY_OK,
	
	KEY_NUM,
}key_id_t;

#else
typedef uint8_t key_id_t;

#define KEY0		0
#define KEY1 		1
#define KEY2		2
#define KEY3		3
#define KEY4		4
#define KEY5		5
#define KEY6		6
#define KEY_BELL	7
#define KEY_ESC		8
#define KEY_OK		9
#define KEY_NUM		10
#endif

typedef struct 
{
	
}key_value_map_t;


static const key_id_t key_id_map[] =
{
	KEY_BELL,
	KEY0, 	
	KEY1,
	KEY2,
	KEY_OK, 
	KEY3, 		
	KEY_ESC,
};


key_timestamp_t key_ctl_get_timestamp(void)
{
	return 100;
}


int key_ctl_read_value(key_value_t *value)
{
	uint16_t port_value = 0x3344;
	uint8_t i;
	*value = 0;
	for (i = 0; i < KEY_NUM; i++) {
		if (key_get_bit(port_value, i)) {
			key_set_bit(*value, key_id_map[i]);
		}
	}
	return 0;
}

void key_event_common_handler(uint8_t key_id, key_event_t event)
{
	
}


typedef struct
{
	key_event_cb_t event_cb;
}key_event_map_t;

static const key_event_map_t key_map[] = 
{
	[KEY0] = 	{key_event_common_handler},
	[KEY1] = 	{key_event_common_handler},
	[KEY2] = 	{key_event_common_handler},
	[KEY3] = 	{key_event_common_handler},
	[KEY4] = 	{key_event_common_handler},
	[KEY_OK] = 	{key_event_common_handler},
	[KEY_ESC] = {key_event_common_handler},
};


typedef struct key_item
{
	key_timestamp_t start_time;
	key_timestamp_t stop_time;
}key_item_t;



typedef struct key_ctl
{
	key_value_t key_value;
	key_item_t key_item[KEY_NUM];
}key_ctl_t;

static key_ctl_t key_ctrl;

// return invalid bits.
static key_value_t key_value_change_handler(key_value_t change_value, uint8_t is_press)
{
	key_timestamp_t time_now;
	key_value_t clear_value = 0;
	uint8_t i;
	if (change_value == 0)
		return 0;
	//TODO get timestamp ...
	time_now = key_ctl_get_timestamp();
	for (i = 0; i < KEY_NUM; i++) {
		if (key_get_bit(change_value, i)) { 
			if (is_press) {
				if (key_ctrl.key_item[i].start_time == 0) {
					key_ctrl.key_item[i].start_time = time_now; 
				}
				if (time_now - key_ctrl.key_item[i].start_time   < KEY_DEBOUNCE_TIME_MS) {
					clear_value |= key_bit(i);	//clear, next time will come again...
				}else{
					key_ctrl.key_item[i].stop_time = 0; 
					//TODO send short press ...
					if (key_map[i].event_cb)
						key_map[i].event_cb(i, KEY_EVENT_SHORT);
				}
			}else{
				if (key_ctrl.key_item[i].stop_time == 0) {
					key_ctrl.key_item[i].stop_time = time_now; 
				}
				if (time_now - key_ctrl.key_item[i].stop_time   < KEY_DEBOUNCE_TIME_MS) {
					clear_value |= key_bit(i);	//clear, next time will come again...
				}else{
					key_ctrl.key_item[i].start_time = 0; 
					//TODO send press up...
					if (key_map[i].event_cb)
						key_map[i].event_cb(i, KEY_EVENT_UP);
				}
			}
		}
	}
	return clear_value;
}


static void key_value_keep_handler(key_value_t keep_value, key_value_t read_value)
{
	uint8_t i;
	key_value_t press_value, up_value;
		//TODO get timestamp ...
	key_timestamp_t time_now = key_ctl_get_timestamp();
	press_value = read_value & keep_value;
	up_value = (~read_value) & keep_value;
	
	for (i = 0; i < KEY_NUM; i++) {
		// press
		if (press_value)
		if (key_get_bit(press_value, i)) {
			if (key_ctrl.key_item[i].start_time && time_now - key_ctrl.key_item[i].start_time >= KEY_LONG_PRESS_TIME_MS) {
				if (key_map[i].event_cb)
					key_map[i].event_cb(i, KEY_EVENT_LONG);
				key_ctrl.key_item[i].start_time = 0;
			}
		}

		if (key_get_bit(up_value, i)) {
			if (key_ctrl.key_item[i].stop_time && time_now - key_ctrl.key_item[i].stop_time >= KEY_LONG_PRESS_TIME_MS) {
				if (key_map[i].event_cb)
					key_map[i].event_cb(i, KEY_EVENT_IDLE);
				key_ctrl.key_item[i].stop_time = 0;
			}
		}	
		// if up value == full value , re judge timestamp standerd.
		
	}
}

void key_ctl_scan(void)
{
	key_value_t key_value, change_value;
	
	if(key_ctl_read_value(&key_value)) {
		return ;
	}
	
	change_value = key_value ^ key_ctrl.key_value;
	if (change_value) {
		// press bits
		key_value ^= key_value_change_handler(key_value & change_value, 1); 

		// up bits ...
		key_value |= key_value_change_handler((~key_value) & change_value, 0);

	}
	//long press 
	key_value_keep_handler(~change_value, key_value);
	key_ctrl.key_value = key_value;
	
}

