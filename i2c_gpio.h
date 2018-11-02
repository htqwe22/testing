#ifndef I2C_GPIO__H
#define I2C_GPIO__H


void i2c_gpio_start(unsigned short us);

void i2c_gpio_stop(unsigned short us);

/*  Note : this function do not call @func i2c_gpio_start and @func  i2c_gpio_stop();
 *          So ,you should call start and stop yourself whenever you need it.
 *   on success return 0.
 *   otherwise 1 will returned.
 */
int i2c_gpio_write_byte(unsigned short us,unsigned char data);


/*  Note : this function do not call @func i2c_gpio_start and @func  i2c_gpio_stop();
 *          So ,you should call start and stop yourself whenever you need it.
 * if ack == I2C_ACK(value is 0), ACK the byte so can continue reading, else
 * send I2C_NOACK(value is 1)to end the read.
 * return : the byte read from I2C client.
 */
unsigned char i2c_gpio_read_byte(unsigned short us,int ack);

#endif