/**********************************************************************************
 * FILE : dv_keys.h
 * Description:
 * Author: Kevin He
 * Created On: 2021-07-27 , At 16:46:11
 * Modifiled On : 
 * Version : 0.0.1
 * Information :
 **********************************************************************************/

#ifndef DV_DV_KEYS_H
#define DV_DV_KEYS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif




#define dv_key_value(i)			(1<<(i))
#define dv_get_key(keys, i)		((keys) &  dv_key_value(i))
#define dv_set_key(keys, i)		((keys) |= dv_key_value(i))
#define dv_clear_key(keys, i)	((keys) &= ~dv_key_value(i))



#define EV_KEY_KEEP_FLAG		0x80
#define EV_KEY_RELEASE_FLAG		0x40

#define EV_KEY_PRESS			0x00
#define EV_KEY_RELEASE			0x40	//(EV_KEY_RELEASE_FLAG | EV_KEY_RELEASE_FLAG)
#define EV_KEY_KEEP_PRESS		0x80	//(EV_KEY_KEEP_FLAG | EV_KEY_PRESS)
#define EV_KEY_KEEP_RELEASE		0xC0	//(EV_KEY_KEEP_FLAG | EV_KEY_RELEASE)

typedef uint8_t keys_event_t;

typedef uint32_t keys_value_t;
typedef void* key_group_t;

/*
 * return 0 is success and out_value the keys value in bitwise, 0 means pressed.
 */
typedef int (*keys_read_callback_t)(keys_value_t *out_value, void *param);


/*return value. value 0 means this key int event is handed, and the same key never comes again expect event changed. 
 */
typedef keys_value_t (*keys_event_callback_t)(keys_value_t keys, keys_event_t event, void *param);

int dv_keys_init(void);

void dv_keys_exit(void);

key_group_t dv_keys_add_group(keys_read_callback_t keys_read, keys_event_callback_t ev_callback, keys_value_t key_mask, void *param);

int dv_keys_del_group(key_group_t handler);

void dv_keys_modify_group_mask(key_group_t handler, keys_value_t new_mask);

void dv_keys_stop(key_group_t handler);

int dv_keys_start(key_group_t handler);






#ifdef __cplusplus
}
#endif

#endif //dv_keys.h

