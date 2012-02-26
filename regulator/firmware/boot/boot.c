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

volatile uint16_t addr;

volatile enum
{
	START,
	READ,
	WRITE,
	BOOT
} state = START;

volatile uint8_t page_dirty;
volatile uint16_t page_address = 0x0000;
volatile uint8_t page_buffer[SPM_PAGESIZE];
#define page_offset(addr) ((addr) & (SPM_PAGESIZE - 1))
#define page_start(addr) ((addr) & ~(SPM_PAGESIZE - 1))

void read_page(uint16_t address)
{
	boot_rww_enable_safe();
	page_address = address;
	for (i = 0; i < SPM_PAGESIZE; i++)
	{
		page_buffer[i] = pgm_read_byte(address + i);
	}
	page_dirty = 0;
}

void write_page(void)
{
	if (!page_dirty)
		return;
	
	uint8_t sreg;
	sreg = SREG;
	cli();
	boot_page_erase_safe(page_address);
	for (int i = 0; i < SPM_PAGESIZE; i += 2)
	{
		uint16_t data = (uint16_t)page_buffer[i] + ((uint16_t)page_buffer[i+1] << 8);
		boot_page_fill_safe(page_address + i, data);
	}
	boot_page_write_safe(page_address);
	boot_rww_enable_safe();
	page_dirty = 0;
	SREG = sreg;
}

unsigned char transmit(volatile unsigned char *buffer, unsigned char size)
{
	if (state != READ)
		return 0;
	int i;
	write_page();
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
				addr = (uint16_t)buffer[1] << 8 | (uint16_t)buffer[2];
				switch (buffer[0])
				{
				case 0x00:
					state = READ;
					break;
				case 0x01:
					state = WRITE;
					return SPM_PAGESIZE - page_offset(addr);
				case 0x02:
					state = BOOT;
					break;
				default:
					break;
				}
				return 0;
			}
			break;
		case WRITE:
			if(page_address != page_start(addr))
			{
				write_page();
				read_page(page_start(addr));
			}
			for (i = 0; i < size; i++)
			{
				page_buffer[page_offset(addr)] = buffer[i];
				addr++;
				page_dirty = 1;
			}
			return SPM_PAGESIZE - page_offset(addr);
		default:
			break;
		}
	}
	return 0;
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

	addr = 0x0000;

	read_page(addr);

	sei();

	do
	{
		_delay_ms(1000);
	} while (state != BOOT);

	cli();

	write_page();

	MCUCR = (1<<IVCE);
	MCUCR = (0<<IVSEL);

	app_main = (void *)addr;
	boot_rww_enable_safe();
	app_main();
}
