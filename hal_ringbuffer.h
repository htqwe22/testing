#ifndef HAL_RING_BUFFER__H
#define HAL_RING_BUFFER__H

/*
 * Author Kevin He
 * Create On May 5th, 2015
 */


#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#define HAS_INITED 0XA5A5

struct ring_buffer
{
	uint8_t *buffer;
	uint16_t queue_size;
	volatile uint16_t in;
	volatile uint16_t out;
	uint16_t element_size;
	uint16_t  flag; //0XA5A5 inited, 0, destored
};
#define RING_BUFF_NO_DATA (-1)
#define RING_BUFF_OUT_SIZE_SMALL (-2)

#define buffer_empty(buffer) ((buffer)->in == (buffer)->out)
#define buffer_full(buffer) ((buffer)->in + 1 == (buffer)->out || \
							(buffer)->in + 1 == (buffer)->out + (buffer)->queue_size)

#define buffer_almost_full(buffer) ((buffer)->in + 8 == (buffer)->out || \
							(buffer)->in + 8 == (buffer)->out + (buffer)->queue_size)

#define hal_ring_buffer_vaild(buffer) ((buffer)->flag == HAS_INITED)

/*
 * @queue_size is the max number of elements the ring buffer can store.
 * @element_size is the size of each element in bytes.
 * so the size of @buff (BYTE) is (queue_size * element_size)
 */
int hal_ring_buffer_init(struct ring_buffer *buffer, void *buff, uint16_t queue_size, uint16_t element_size);

void hal_ring_buffer_destroy(struct ring_buffer *buffer);

/*
 * return how many elements we really get.
 * @elements is how many elements we want to get
 * @no_pek : 0 means after we get the data, date no longer exists in ringbuffer. if 1, data is still in ringbuffer.
 */
int hal_ring_buffer_get(struct ring_buffer *buffer,void *outData, uint16_t elements, uint8_t no_peek);

/* Don`t use / % , that is too slow*/
/*
 * @elements how many elements we want to put into the ring buffer.
 * return the real number of elements we put in.
 */
int hal_ring_buffer_put(struct ring_buffer *buffer, const void *inData, uint16_t elements);

/*
 * A faster interface to put one element
 * @inData (MUST: the size of @inData should be at least @element_size bytes)
 */
int hal_ring_buffer_put_one(struct ring_buffer *buffer, const void *inData);

/*if return NO_DATA then get failed*/
/*
 * A faster interface to get one element
 * @inData (MUST: the size of @outData should be at least @element_size bytes)
 */
int hal_ring_buffer_get_one(struct ring_buffer *buffer, void *outData);


/*
 * just skip @elements data, return the real num skiped.
 */
int hal_ring_buffer_skip(struct ring_buffer *buffer, uint16_t elements);


uint16_t hal_ring_buffer_space(struct ring_buffer *buffer);

uint16_t hal_ring_buffer_deep(struct ring_buffer *buffer);

void hal_ring_buffer_reset(struct ring_buffer *buffer);



#ifdef __cplusplus
}
#endif

#endif

