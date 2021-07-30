/**********************************************************************************
 * FILE : dv_keys.c
 * Description:
 * Author: Kevin He
 * Created On: 2021-07-27 , At 16:46:11
 * Modifiled On : 
 * Version : 0.0.1
 * Information :
 **********************************************************************************/

#include "dv_keys.h"
#include "FreeRTOS.h"
#include <dv_os_timer.h>
#include <dv_os_pthread.h>
#include <dv_log.h>
#include <power_manager.h>
#include <proj_config.h>
#include <string.h>


#define KEY_WORK_STATE_PAUSE 		0
#define KEY_WORK_STATE_WORK			1
#define KEY_WORK_STATE_DELETE		2
typedef uint8_t work_state_t;

struct key_group
{
	keys_read_callback_t read_callback;
	keys_event_callback_t event_callback;
	void *param;
	keys_value_t key_mask;
	keys_value_t key_value; // store the last value of keys
	keys_value_t keep_mask;
	work_state_t work_flag;
};


#define KEYSCAN_INTERVAL_MS				50
#define KEYSCAN_STOP_AFTER_ALL_UP_MS	1000

static void dv_keyscan_timer_callback(dv_timer_t timer, unsigned long param);

static dv_timer_t scan_timer;
static struct key_group *g_groups[KEY_GROUPS_NUM];


int dv_keys_init(void)
{
	memset(g_groups, 0, sizeof(g_groups));	
	scan_timer = dv_timer_new(KEYSCAN_INTERVAL_MS, dv_keyscan_timer_callback, 0, TIMER_REPEAT_FOREVER);
	return 0;
}

void dv_keys_exit(void)
{
	uint8_t i;
	for (i = 0; i < KEY_GROUPS_NUM; i++) {
		dv_keys_del_group(g_groups[i]);
	}
	dv_os_msleep(500); // wait for memory free ...

	for (i = 0; i < KEY_GROUPS_NUM; i++) {
		dv_os_free(g_groups[i]);
		g_groups[i] = NULL;
	}
	dv_timer_delete(scan_timer);
}

key_group_t dv_keys_add_group(keys_read_callback_t keys_read, keys_event_callback_t ev_callback, keys_value_t key_mask, void *param)
{
	struct key_group *group = (struct key_group *)dv_os_malloc(sizeof(struct key_group));
	uint8_t i;
	for (i = 0; i < KEY_GROUPS_NUM; i++) {
		if (g_groups[i] == NULL)
			break;
	}
	if (i == KEY_GROUPS_NUM) // full
		return NULL;
	
	if (group) {		
		g_groups[i] = group;
		memset(group, 0, sizeof(group));
		group->read_callback = keys_read;
		group->event_callback = ev_callback;
		group->param = param;
		group->key_mask = key_mask;
		group->key_value = key_mask; //default is idle		
		dv_keys_start(group);
	}
	return group;
}

int dv_keys_del_group(key_group_t handler)
{
	struct key_group *group = (struct key_group *)handler;
	group->work_flag = KEY_WORK_STATE_DELETE; // We will delete it in scan loop
	return 0;
}

void dv_keys_stop(key_group_t handler)
{
	struct key_group *group = (struct key_group *)handler;
	group->work_flag = KEY_WORK_STATE_PAUSE;

}

int dv_keys_start(key_group_t handler)
{
	struct key_group *group = (struct key_group *)handler;	
	dv_debug(LOG_LVL_INFO, "dv_start\n");
	if (group) {
		group->work_flag = KEY_WORK_STATE_WORK; 
	}
	return dv_timer_start(scan_timer);
}


void dv_keys_modify_group_mask(key_group_t handler, keys_value_t new_mask)
{
	keys_value_t change_value;
	struct key_group *group = (struct key_group *)handler;
	change_value = group->key_mask ^ new_mask;
	
	// del bits = change_value & group->key_mask
	group->keep_mask &= ~(change_value & group->key_mask);
	
	// add bits = change_value & new_mask;
	group->keep_mask |= change_value & new_mask;
	group->key_mask = new_mask;
}


/*
 * return 0, do not loop it again...
 */
static uint8_t dv_keys_loop(struct key_group *group)
{
	keys_value_t key_value, change_value, press_value, up_value, user_ret;
//	keys_value_t tmp;
	uint8_t loop_continue = 0;
	
	if (group == NULL)
		return 0;

	if (group->work_flag != KEY_WORK_STATE_WORK) {
		if (group->work_flag == KEY_WORK_STATE_DELETE) {
			for (change_value = 0; change_value < KEY_GROUPS_NUM; change_value++) {
				if (g_groups[change_value] == group) {
					g_groups[change_value] = NULL;
					dv_os_free(group);
					break;
				}
			}
		}
		return 0;
	}

	// Read fail ... 
	if (group->read_callback(&key_value, group->param)) {
		dv_debug(LOG_LVL_ERR, "read key fail group is %#x\n", group);
		return 0;
	}
	change_value = (key_value ^ group->key_value) & group->key_mask;
////	printf("read %x change %x, origin %x\n", key_value, change_value, group->key_value);
	if (change_value) {
		// release bits ...
		up_value = change_value & key_value;
		// pressed bits
		press_value = change_value & (~key_value);

		if (press_value) {
			// 1 means user need debounce.
			user_ret = group->event_callback(press_value, EV_KEY_PRESS, group->param) /* & press_value */;
			if (user_ret)
				loop_continue = 1;
////			tmp = key_value;
			key_value |= (user_ret & press_value);		
////			printf("press %x, %x|%x=%x\n", press_value, tmp, user_ret, key_value);
			// set the bits user handled.
			group->keep_mask |= ((~user_ret) & press_value);
////			printf("keep_mask %x\n", group->keep_mask);
		}
		
		if (up_value) {
			// 1 means user need debounce.
			user_ret = group->event_callback(up_value, EV_KEY_RELEASE, group->param) /* & up_value */;
			if (user_ret)
				loop_continue = 1;
////			tmp = key_value;
			key_value ^= (user_ret & up_value);				
////			printf("up %x, %x^%x=%x\n", up_value, tmp, user_ret, key_value);
			// set the bits user handled.
			group->keep_mask |= ((~user_ret) & up_value);			
////			printf("keep_mask %x\n", group->keep_mask);
		}
	}

	/*ADD: change_value = key_value ^ group->key_value;*/
	change_value = (~change_value) & group->key_mask; // othe others is keep value 
	
	//keep release value
	up_value = change_value & group->keep_mask & key_value;
	//keep pressed value
	press_value = change_value & group->keep_mask & (~key_value);
////	printf("long press %x = %x&%x&%x\n", press_value, change_value, group->keep_mask, ~key_value);
////	printf("long up %x = %x&%x&%x\n", up_value, change_value, group->keep_mask, key_value);
	// Clear the bits user handled.
	if (press_value){
		user_ret = group->event_callback(press_value, EV_KEY_KEEP_PRESS, group->param);
		group->keep_mask &= user_ret | (~press_value);	
////		printf("press keep mask %x %x\n", group->keep_mask, user_ret);
	}
	if (up_value) {
		user_ret = group->event_callback(up_value, EV_KEY_KEEP_RELEASE, group->param);
		group->keep_mask &= user_ret | (~up_value) ;
////		printf("up keep %x %x\n", group->keep_mask, user_ret);
	}
	if (group->keep_mask)
		loop_continue = 1;
	
	// backup
	group->key_value = key_value;
	return loop_continue;
}


static void dv_keyscan_timer_callback(dv_timer_t timer, unsigned long param)
{
	uint8_t i;
	uint8_t retval = 0;
	static uint32_t lastwork_timestamp = 0;
	for (i = 0; i < KEY_GROUPS_NUM; i++) {
		retval |= dv_keys_loop(g_groups[i]);
	}
	if (retval == 0) {
		if (dv_os_get_timestamp() - lastwork_timestamp > KEYSCAN_STOP_AFTER_ALL_UP_MS) {
			dv_timer_stop_async(scan_timer);
			dv_debug(LOG_LVL_INFO, "key stop\n");
		}
	}else{
		lastwork_timestamp = dv_os_get_timestamp();
	}
}



