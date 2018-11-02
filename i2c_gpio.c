/******************** For porting **********************/
void i2c_gpio_scl_output(void)
{

}
void i2c_gpio_sda_output(void)
{

}

void i2c_gpio_sda_input(void)
{

}

static void i2c_gpio_scl_set(unsigned char bit)
{

}

static void i2c_gpio_sda_set(unsigned char bit)
{
	
}

int i2c_gpio_sda_get(void)
{
	return 0;
}



// period is 4 *ns
static void i2c_udelay(unsigned short us)
{

}


/****************** Common handle ****************************/
static void i2c_gpio_sda_high(unsigned short us)
{
	i2c_gpio_scl_set(0);
	i2c_udelay(us);
	i2c_gpio_sda_set(1);
	i2c_udelay(us);
}

static void i2c_gpio_write_bit(unsigned short us,unsigned char bit)
{
	i2c_gpio_scl_set(0);
	i2c_udelay(us);
	i2c_gpio_sda_set(bit);
	i2c_udelay(us);
	i2c_gpio_scl_set(1);
	i2c_udelay(us<<1);

}

/* ack should be I2C_ACK (value 0) or I2C_NOACK (value 1)*/
static void i2c_gpio_send_ack(unsigned short us,unsigned char ack)
{
	i2c_gpio_write_bit(us,ack);
	i2c_gpio_scl_set(0);
	i2c_udelay(us);
}

static int i2c_gpio_read_bit(unsigned short us)
{
	int value;
	i2c_gpio_scl_set(1);
	i2c_udelay(us);
	value = i2c_gpio_sda_get();
	i2c_udelay(us);
	i2c_gpio_scl_set(0);
	i2c_udelay(us<<1);
	return value;
}


void i2c_gpio_start(unsigned short us)
{	
	i2c_udelay(us);
	i2c_gpio_scl_output();
	i2c_gpio_sda_output();
	
	i2c_gpio_sda_set(1);
	i2c_udelay(us);
	i2c_gpio_scl_set(1);
	i2c_udelay(us);
	i2c_gpio_sda_set(0);
	i2c_udelay(us);
}

void i2c_gpio_stop(unsigned short us)
{
	i2c_gpio_scl_output();
	i2c_gpio_sda_output();
	i2c_gpio_scl_set(0);
	i2c_udelay(us);
	i2c_gpio_sda_set(0);
	i2c_udelay(us);
	i2c_gpio_scl_set(1);
	i2c_udelay(us);
	i2c_gpio_sda_set(1);
	i2c_udelay(us);
}

int i2c_gpio_write_byte(unsigned short us,unsigned char data)
{
	int j;
	int nack;

	for (j = 0; j < 8; j++) {
		i2c_gpio_write_bit(us, data & 0x80);
		data <<= 1;
	}

	i2c_udelay(us);

	/* Look for an <ACK>(negative logic) and return it */
	i2c_gpio_sda_high(us);
	
	i2c_gpio_sda_input();
	nack = i2c_gpio_read_bit(us);
	i2c_gpio_sda_output();

	return nack;	/* not a nack is an ack */
}

/**
 * if ack == I2C_ACK, ACK the byte so can continue reading, else
 * send I2C_NOACK to end the read.
 */
unsigned char i2c_gpio_read_byte(unsigned short us,int ack)
{
	int  data;
	int  j;

	i2c_gpio_sda_high(us);
	data = 0;
	
	i2c_gpio_sda_input();
	for (j = 0; j < 8; j++) {
		data <<= 1;
		data |= (i2c_gpio_read_bit(us)&1) ;
	}
	i2c_gpio_sda_output();
	i2c_gpio_send_ack(us, ack);

	return data;
}

