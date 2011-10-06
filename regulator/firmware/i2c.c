/*
	Copyright (C) 2011 Nikolay Ilduganov
*/

#include <avr/io.h>
#include <avr/interrupt.h>

static volatile unsigned char i = 0;
static volatile int l = 0;
static volatile unsigned char buffer[100];

SIGNAL(TWI_vect)
{
	unsigned char ch;
	switch(TWSR & 0xF8)
	{
// MASTER TRANSMITTER/RECEIVER
	// case 0x08: // >START (>SLA+R/W; <(N)ACK)
	// case 0x10: // >Repeated START (>SLA+R/W; <(N)ACK)
	// case 0x38: // Arbitration lost (SLAVE or >START when free)
// MASTER TRANSMITTER
	// case 0x18: // >SLA+W; <ACK (>DATA; <(N)ACK or repeated START or STOP)
	// case 0x20: // >SLA+W; <NACK (>DATA; <(N)ACK or repeated START or STOP or STOP-START)
	// case 0x28: // >SLA+W; <(N)ACK; >DATA; <ACK (>DATA; <(N)ACK or repeated START or STOP or STOP-START)
	// case 0x30: // >SLA+W; <(N)ACK; >DATA; <NACK (>DATA; <(N)ACK or repeated START or STOP or STOP-START)
// MASTER RECEIVER
	// case 0x40: // >SLA+R; <ACK (<DATA; >(N)ACK)
	// case 0x48: // >SLA+R; <NACK (repeated START or STOP or STOP-START)
	// case 0x50: // >SLA+R; <ACK; <DATA; <ACK (<DATA; >(N)ACK)
	// case 0x58: // >SLA+R; <ACK; <DATA; <NACK (repeated START or STOP ot STOP-START)
// SLAVE RECEIVER
	case 0x60: // <SLA+W; >ACK (<DATA; >(N)ACK)
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	// case 0x68: // <SLA+W; >ACK; Arbitration lost ???
	// case 0x70: // <General call; >ACK (<DATA; >(N)ACK)
	// case 0x78: // <General call; Arbitration lost ???
	case 0x80: // <SLA+W; >ACK; <DATA; >ACK (<DATA; >(N)ACK)
		ch = TWDR;
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	// case 0x88: // <SLA+W; >ACK; <DATA; >NACK (N/A)
	// case 0x90: // <General call; >ACK; <Data (<DATA; >(N)ACK)
	// case 0x98: // <General call; >NACK; <Data (N/A)
	// case 0xA0: // STOP or repeated START while slave (N/A)
// SLAVE TRANSMITTER
	case 0xA8: // <SLA+R; >ACK (>END; <NACK or >DATA; <ACK)
		i = 0;
		TWDR = buffer[i];
		i++;
		if (i < l)
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
		}
		break;
	// case 0xB0: // <SLA+R; >ACK; Arbitration lost (>END; <NACK or >DATA; <ACK)
	case 0xB8: // <SLA+R; >ACK; >DATA; <ACK (>END; <NACK or >DATA; <ACK)
		TWDR = buffer[i];
		i++;
		if (i < l)
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
		}
		break;
	// case 0xC0: // <SLA+R; >ACK; >DATA; <NACK (N/A)
	// case 0xC8: // <SLA+R; >ACK; >END; <ACK (N/A)
	default:
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	}
}

void init_i2c(void)
{
	TWAR = 1 << 1;
	TWCR = 0x45;
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWEA);
}

