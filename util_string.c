
/**********************************************************************************
 * FILE : util_string.c
 * Description:
 * Author: Kevin He
 * Created On: 2018-03-19 , At 11:21:19
 * Modifiled On : 
 * Version : 0.0.1
 * Information :
 **********************************************************************************/

#include "util_string.h"
#include <stdint.h>
#include <string.h>

//#define USE_LOWERCASE

#ifdef USE_LOWERCASE
static const char *charlist = "0123456789abcdef";
#else
static const char *charlist = "0123456789ABCDEF";
#endif
#define bin_to_hexchar(b)	charlist[(b)]

#ifndef min 
#define min(a, b)	(a)<(b)?(a):(b)
#endif

static inline uint8_t hexchar_to_bin(char hex)
{
	if (hex >= '0' && hex <= '9')
		return hex-'0';
#if 1	
	hex |= 0x20;		//lowercase to lowercase
#else	
	if (hex >= 'A' && hex <= 'F')
		return hex-'A'+10;
#endif
	if (hex >= 'a' && hex <= 'f')
		return hex-'a'+10;	
	return 0;
}

/*return real bin length*/
int hex_string_to_bin(const char *hexstr, int hexstr_len, unsigned char *binbuf, int binbuff_size)
{
	if (hexstr == NULL || binbuf == NULL)
		return 0;

	// reuse hexstr_len as result length
	hexstr_len >>= 1;
	hexstr_len = min(hexstr_len, binbuff_size);
	// reuse binbuff_size as index.
	for (binbuff_size = 0; binbuff_size < hexstr_len; binbuff_size++, hexstr += 2) {
		binbuf[binbuff_size] = (hexchar_to_bin(hexstr[0]) << 4) | hexchar_to_bin(hexstr[1]);
	}
	return hexstr_len;
}

/**/
int bin_to_hex_string(const unsigned char *binbuf, int binbuff_len, char *hexstr, int hexstr_size)
{
	if (hexstr == NULL || binbuf == NULL)
		return 0;
	uint8_t bin; // support binbuff is located last to the end of hexStr 
	// get real transferd bin len.
	binbuff_len = min(binbuff_len, hexstr_size>>1);
	
	// reuse hexstr_size as index.
	for (hexstr_size = 0; hexstr_size < binbuff_len; hexstr_size++, hexstr += 2) {
		bin = binbuf[hexstr_size];
		hexstr[0] = bin_to_hexchar(bin >> 4);
		hexstr[1] = bin_to_hexchar(bin & 0xf);
	}
	
	return hexstr_size<<1;
}


void *memove2(void *dest, const void *src, size_t n)
{
	size_t i;
	if (dest == src || dest == NULL || src == NULL || n == 0)
		return dest;
	if ((const char *)src < (char *)dest && ((char *)dest - (const char*)src < (signed)n)) {
		for (; n ; n--)
			*((char*)dest + n - 1) = *((const char*)src + n - 1);
	}else{
		for (i = 0; i < n; i++)
			*((char*)dest + i) = *((const char*)src + i);
	}
	return dest;
}

char *strstr2(const char *origin, int origin_len, const char *needle)
{
	int i;
	int min_len = strlen(needle);

	if (origin == NULL || needle == NULL || min_len == 0)
		return NULL;

	origin_len -= min_len;
	for (i = 0; i <= origin_len; i++)
	{
		if(0 == memcmp(origin+i, needle, min_len))
			return (char *)origin+i;
	}
	return NULL;
}


#define swap_data(a, b) do{(a) ^= (b); (b) = (a) ^(b); (a) ^= (b);}while(0)

#define swap_arr(arr, arr_size) do {	\
	int index; (arr_size)--; \
	for (index = 0; index < (arr_size); index++, (arr_size)--) swap_data((arr)[index], (arr)[arr_size]); \
}while(0)

void swap_byte_arr(uint8_t arr[], int arr_size)
{
	swap_arr(arr, arr_size);
}

void swap_short_arr(uint16_t arr[], int arr_size)
{
	swap_arr(arr, arr_size);
}

void swap_int_arr(uint32_t arr[], int arr_size)
{
	swap_arr(arr, arr_size);

}

void swap_u64_arr(uint64_t arr[], int arr_size)
{
	swap_arr(arr, arr_size);
}



#if 0
void swap_bytes_unit(void *buff, unsigned int unit_num, short unit_size)
{
	unsigned int i, j;
	unit_num--; // to last index. 
	unsigned char *arr = (unsigned char *)buff;
	
	for (i = 0; i < unit_num; i++, unit_num--)
	{
		for (j = 0; j < unit_size; j++)
			swap_data(arr[i*unit_size +j], arr[unit_num*unit_size +j]);
	}
}
#endif

#if 0
int main(int argc, char **argv)
{
	char a[100];
	char *b = a + 10;
	memcpy(a, "12345678909", 12);
	memove2(b, a, 12);
	printf("%s\n", b);
	return 0;
}
#endif

