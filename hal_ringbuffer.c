/*
 * Author Kevin He
 * Create On May 5th, 2015
 * Modyfied On Dec 20th, 2017. support element queue, and add one interface @hal_ring_buffer_skip
 */

#include "hal_ringbuffer.h"
#include <stdio.h>
#include <string.h>
#ifndef min
#define min(a, b) ((a) < (b) ? (a):(b))
#endif

#define element_addr(n) (buffer->buffer + (n)*(buffer->element_size))
#define buffer_assert()  
//do{if(buffer->flag != HAS_INITED) ASSERT(0,0);}while(0)

/*Basic principle : maxium data size is buffer->buffer_size -1;
  in >= out && in < out + buffer_size */

uint16_t hal_ring_buffer_space(struct ring_buffer *buffer)
{
	uint16_t out;
	if (buffer == NULL) 
		return 0;
	// when use space , we want fill in , @out may change. so
	out =  buffer->out;
	if (buffer->in < out)
		return out - buffer->in -1;
	return buffer->queue_size -1 - buffer->in + out;
}

uint16_t hal_ring_buffer_deep(struct ring_buffer *buffer)
{
	uint16_t in;
	if (buffer == NULL) 
		return 0;
	// when use deep , we want pop , @in may change. so
	in  = buffer->in;
	if (in >= buffer->out)
		return in - buffer->out;
	return buffer->queue_size + in - buffer->out;
	// or buffer->buffer_size - 1 - hal_buffer_space();
}


void hal_ring_buffer_reset(struct ring_buffer *buffer)
{
	if (buffer) 
	{
		buffer_assert();
		buffer->in = buffer->out = 0;
	}
}

void hal_ring_buffer_destroy(struct ring_buffer *buffer)
{
	hal_ring_buffer_reset(buffer);
	buffer->flag = 0;
}

int hal_ring_buffer_init(struct ring_buffer *buffer, void *buff, uint16_t queue_size, uint16_t element_size)
{
	if (buffer)
	{
		buffer->buffer = buff;
		buffer->queue_size = queue_size;
		buffer->element_size = element_size;
		if (buffer->flag == HAS_INITED)
		{
		//	ASSERT(0, 0);
		}
		buffer->flag = HAS_INITED;
		hal_ring_buffer_reset(buffer);
		return 0;
	}
	return -1;
}

int hal_ring_buffer_skip(struct ring_buffer *buffer, uint16_t elements)
{
	int ret;
	uint16_t deep;
	if (buffer == NULL)
		return 0;
	buffer_assert();
	deep = hal_ring_buffer_deep(buffer);
	ret = min(elements, deep);
	if (ret + buffer->out >= buffer->queue_size) {
		buffer->out = ret + buffer->out - buffer->queue_size;
	}else {
		buffer->out += ret; 		
	}
	return ret;
}


int hal_ring_buffer_get(struct ring_buffer *buffer,void *outData, uint16_t elements, uint8_t no_peek)
{
	uint8_t *pdata;	
	uint16_t deep, l;
	int ret;
	if (buffer == NULL || outData == NULL) {
		return -1;
	}	
	buffer_assert();
	pdata = (uint8_t *)outData;
	deep = hal_ring_buffer_deep(buffer);
	ret = min(elements, deep);
	l = min(ret, buffer->queue_size - buffer->out);
//	memcpy(pdata, buffer->buffer + buffer->out, l);
//	memcpy(pdata + l, buffer->buffer, ret - l);
	memcpy(pdata, element_addr(buffer->out), l*buffer->element_size);
	memcpy(pdata + l*buffer->element_size, buffer->buffer, (ret - l)*buffer->element_size);
	if (no_peek == 0) {
		if (ret + buffer->out >= buffer->queue_size) {
			buffer->out = ret + buffer->out - buffer->queue_size;
//			printf("round\r\n");
		}else {
			buffer->out += ret;			
		}
	}
	return ret;
	
}

/* Don`t use / % , that is too slow*/
int hal_ring_buffer_put(struct ring_buffer *buffer, const void *inData, uint16_t elements)
{
	uint8_t *pdata;	
	uint16_t space, l;
	int ret;
	if (buffer == NULL || inData == NULL) {
		return -1;
	}	
	buffer_assert();
	pdata = (uint8_t *)inData;
	space = hal_ring_buffer_space(buffer);
	ret = min(space, elements);	
	if (ret < elements)
	{
//		ASSERT(0, 0);
	}
	l = min(ret, buffer->queue_size - buffer->in);	
//	memcpy(buffer->buffer + buffer->in, pdata , l);
//	memcpy(buffer->buffer, pdata + l, ret - l);
	memcpy(element_addr(buffer->in), pdata , l*buffer->element_size);
	memcpy(buffer->buffer, pdata + l*buffer->element_size, (ret - l)*buffer->element_size);
	if (ret + buffer->in >= buffer->queue_size) {
		buffer->in = ret + buffer->in - buffer->queue_size;
	} else {
		buffer->in += ret;		
	}
	return ret;
}

int hal_ring_buffer_put_one(struct ring_buffer *buffer, const void *inData)
{
	if (buffer_full(buffer)) 
	{
//		ASSERT(0, 0);
		return 0;
	}
	buffer_assert();
	memcpy(element_addr(buffer->in),inData, buffer->element_size);
	if (1 + buffer->in >= buffer->queue_size)
		buffer->in = 0;
	else
		buffer->in++;
	return 1;
}

int hal_ring_buffer_get_one(struct ring_buffer *buffer, void *outData)
{
	if (buffer == NULL || outData == NULL) {
		return RING_BUFF_NO_DATA;
	}	
	if (buffer_empty(buffer)) 
		return RING_BUFF_NO_DATA;
	buffer_assert();
	memcpy(outData, element_addr(buffer->out), buffer->element_size);
	if (1 + buffer->out >= buffer->queue_size)
		buffer->out = 0;
	else
		buffer->out++;
	return 1;
}



