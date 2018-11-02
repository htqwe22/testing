
/**********************************************************************************
 * FILE : util_string.h
 * Description:
 * Author: Kevin He
 * Created On: 2018-03-19 , At 11:21:19
 * Modifiled On : 
 * Version : 0.0.1
 * Information :
 **********************************************************************************/

#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include <string.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define USE_BIGENDIAN

int hex_string_to_bin(const char *hexstr, int hexstr_len, unsigned char *binbuf, int binbuff_size);

/**/
int bin_to_hex_string(const unsigned char *binbuf, int binbuff_len, char *hexstr, int hexstr_size);


void *memove2(void *dest, const void *src, size_t n);


char *strstr2(const char *origin, int origin_len, const char *needle);


void swap_byte_arr(uint8_t arr[], int arr_size);

void swap_short_arr(uint16_t arr[], int arr_size);

void swap_int_arr(uint32_t arr[], int arr_size);

void swap_u64_arr(uint64_t arr[], int arr_size);

#ifdef __cplusplus
}
#endif
#endif

