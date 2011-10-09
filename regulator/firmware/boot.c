/*
        Copyright (C) 2011 Nikolay Ilduganov
*/

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <i2c.c>

static void (*app_main)(void) = 0x0000;

volatile uint16_t addr = 0x0000;

volatile enum
{
	START,
	READ,
	WRITE,
	BOOT
} state = START;

volatile uint8_t page_number;
volatile uint8_t page_buffer[SPM_PAGESIZE];

unsigned char transmit(volatile unsigned char *buffer, unsigned char size)
{
	if (state != READ)
		return 0;
	int i;
	boot_rww_enable_safe();
	for (i = 0; i < 16; i++)
	{
		buffer[i] = pgm_read_byte(addr);
		addr++;
		if (addr > FLASHEND)
			addr = 0x0000;
	}
	return 16;
}

unsigned char receive(volatile unsigned char *buffer, unsigned char size)
{
	if (buffer == 0)
	{
		state = START;
		return 3;
	}
	else
	{
		switch (state)
		{
		case START:
			if (size == 3)
			{
				switch (buffer[0])
				{
				case 0:
					state = READ;
					break;
				case 1:
					state = WRITE;
					break;
				case 2:
					state = BOOT;
					break;
				default:
					break;
				}
				addr = (uint16_t)buffer[1] << 8 | (uint16_t)buffer[2];
			}
			break;
		default:
			break;
		}
		if (state != WRITE)
			return 0;
	}
	// TODO: process page update
	return 16;
}

int main(void)
{
	CLKPR = 0x80;
	CLKPR = 0x00;

	MCUCR = (1<<IVCE);
	MCUCR = (1<<IVSEL);

	i2c_slave_transmit = transmit;
	i2c_slave_receive = receive;
	i2c_slave_init(0x10);
	
	sei();

	do
	{
		_delay_ms(1000);
	} while (state != BOOT);

	cli();

	MCUCR = (1<<IVCE);
	MCUCR = (0<<IVSEL);

	app_main = addr;
	boot_rww_enable_safe();
	app_main();
}
