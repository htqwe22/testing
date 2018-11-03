/******************************************************************************
* @copyright                  Nationz Co.,Ltd
*               Copyright (c) 2013－2018 All Rights Reserved
*******************************************************************************
* @file  IoUartDrv.c
* @brief  GPIO TO UART driver Source Code File;
* @author wang.qian
* @version v1.00
* @date 2015-4-27

********************************************************************************/
#include "IomDrv.h"
#include "TimerDrv.h"
#include "IoUartDrv.h"

/**
 * @brief: Gpio initialize for uart
 * @param: 
 		NULL
 * @return 
 	    NULL
 *
 */
void IomUartInit(void)
{
    IomInOutSet(CSR_IOUART_OUT, 0);
    IomSetVal(CSR_IOUART_OUT, 1);

	IomInOutSet(CSR_IOUART_IN, 1);	//输入
}

/**
 * @brief:Delay at us
 * @param
 * - us, time
 * @return 
 	NULL
 */
 
#define SYSCORECLK_M	20
void vDelayUs(UINT32 us)    //26us+us
{
#if 1
    TimerInit(T4, SCU_OSC_CLK, 1, us * 10);
    TimerStart(T4);
    while((TIMER(T4)->CSR & TIMER_DONESTS_MSK) == 0);
    TIMER(T4)->CSR |= TIMER_DONESTS_MSK;
    TimerStop(T4);
#else	
	SysTick->LOAD  = SYSCORECLK_M *us;
    /* Load the SysTick Counter Value */
    SysTick->VAL   = 0;
    /* Enable SysTick IRQ and SysTick Timer */
    SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;     
	while(!(SysTick->CTRL & 0x10000));
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk;
#endif
	
}

/**
 * @brief:Uart Send data
 * @param
 * - us, time
 * @return 
   null
 */
#if 0

#define B9600_CNT  82   //78		/*104 - 26 ?*/
#define B4800_CNT  182		/*208 - 26 ?*/

void  vIoUartSendByte(UINT8 SendByte)   //9600
{
	UINT32	delay = B9600_CNT;
    UINT8 i, dat[8];
	//__disable_irq();
    for(i = 0 ; i < 8; i++)
    {
        dat[i] = (SendByte >> i) & 0x01;
    }
    vDelayUs(delay);

    IomSetVal(CSR_IOUART_OUT, 0);
    vDelayUs(delay);

    for(i = 0 ; i < 8; i++)
    {
        IomSetVal(CSR_IOUART_OUT, dat[i]);
        vDelayUs(delay);
    }

    IomSetVal(CSR_IOUART_OUT, 1);
    vDelayUs(delay * 3);
	//__enable_irq();
}

void test_io(void)
{
	volatile uint32_t delay, start, start1;
	int32_t get_value, get_value1;	
	
	start = SysTick->VAL;
	
	while(1)
	{
		Delay(10000);
		start1 = SysTick->VAL;
		get_value = start - start1;
		if (get_value < 0)
		{
			get_value1 = SysTick->LOAD + get_value;
		}
		printf("%d, %d, %d, %d, %d XXXX\n", SysTick->LOAD, start1, start, get_value, get_value1);
	}
	
	get_value = start - SysTick->VAL;
	if (get_value < 0)
	{
		get_value1 = SysTick->LOAD + get_value;
	}
}

#else

#define UART_DELAY(a)		delay += a; while( (start - TIMER(T4)->CVR) < (delay*1060));

//#define UART_DELAY(a)		delay += a;\
//							do {\
//								get_value = start - SysTick->VAL;\
//								if (get_value < 0)\
//								{\
//									get_value = SysTick->LOAD + get_value;\
//								}\
//							}while(get_value < delay*1900);


void  vIoUartSendByte(UINT8 SendByte)
{
	volatile uint32_t delay, start;
//	int32_t get_value;
    UINT8 i, dat[8];
	delay = 0;
	
    for(i = 0 ; i < 8; i++)
    {
        dat[i] = (SendByte >> i) & 0x01;
    }	

	start = 1000*1000;
	TimerInit(T4, SCU_OSC_CLK, 1, start);
	TimerStart(T4);
//	start = SysTick->VAL;

	UART_DELAY(1);

    IomSetVal(CSR_IOUART_OUT, 0);

    UART_DELAY(1);

    for(i = 0 ; i < 8; i++)
    {
        IomSetVal(CSR_IOUART_OUT, dat[i]);
		UART_DELAY(1);
    }

    IomSetVal(CSR_IOUART_OUT, 1);

	UART_DELAY(2);

    TIMER(T4)->CSR |= TIMER_DONESTS_MSK;
    TimerStop(T4);	
}

#endif

/**
 * @brief:Uart Rev data of byte
 * @param
 * - us, time
 * @return 
   null
 */
UINT8 vIoUartRevByte(void)   //9600
{
	UINT8 dst = 0;
	while((IomGetVal(CSR_IOUART_IN) & 0x01));  //等到起始位0
	vDelayUs(0x58);

    for(UINT8 i = 0 ; i < 8; i++)
    {
        dst |= ((IomGetVal(CSR_IOUART_IN) & 0x01)<<i);
		vDelayUs(0x58);
    }

	return dst;
}

