/*
	Copyright (C) 2011 Nikolay Ilduganov
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef I2C_BUFFER_SIZE
#define I2C_BUFFER_SIZE 100
#endif

#define I2C_START 0x08
#define I2C_REPEATED_START 0x10
#define I2C_SLA_W_ACK 0x18
#define I2C_SLA_W_NACK 0x20
#define I2C_WRITE_ACK 0x28
#define I2C_WRITE_NACK 0x30
#define I2C_ARBITRATION 0x38
#define I2C_SLA_R_ACK 0x40
#define I2C_SLA_R_NACK 0x48
#define I2C_READ_ACK 0x50
#define I2C_READ_NACK 0x58

#define I2C_SLA_W 0x60
#define I2C_SLA_W_ARBITRATION 0x68
#define I2C_GCA_W 0x70
#define I2C_GCA_W_ARBITRATION 0x78
#define I2C_SLA_W_DATA 0x80
#define I2C_SLA_W_ERROR 0x88
#define I2C_GCA_W_DATA 0x90
#define I2C_GCA_W_ERROR 0x98
#define I2C_STOP 0xA0
#define I2C_SLA_R 0xA8
#define I2C_SLA_R_ARBITRATION 0xB0
#define I2C_SLA_R_DATA 0xB8
#define I2C_SLA_R_END 0xC0
#define I2C_SLA_R_ERROR 0xC8

#define I2C_WAIT 0xF8
#define I2C_ERROR 0x00

static volatile unsigned char i = 0;
static volatile unsigned char l = 0;
static volatile unsigned char buffer[I2C_BUFFER_SIZE];

unsigned char (*i2c_slave_transmit)(volatile unsigned char *buffer, unsigned char size) = 0x0000;
unsigned char (*i2c_slave_receive)(volatile unsigned char *buffer, unsigned char size) = 0x0000;

SIGNAL(TWI_vect)
{
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
	case 0x60: // <SLA+W; >ACK
		i = 0;
		l = 0;
		if (i2c_slave_receive)
			l = i2c_slave_receive(0, 0);
		if (l > 0)
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
		}
		break;
	// case 0x68: // <SLA+W; >ACK; Arbitration lost
		// <DATA; >ACK
		// TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		// <DATA; >NACK
		// TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
	// case 0x70: // <General call; >ACK
		// <DATA; >ACK
		// TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		// <DATA; >NACK
		// TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
	// case 0x78: // <General call; Arbitration lost ???
		// <DATA; >ACK
		// TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		// <DATA; >NACK
		// TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
	case 0x80: // <SLA+W; >ACK; <DATA; >ACK
		buffer[i] = TWDR;
		i++;
		if (i < l)
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			l = 0;
			if (i2c_slave_receive)
				l = i2c_slave_receive(buffer, i);
			i = 0;
			if (l > 0)
			{
				TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
			}
			else
			{
				TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
			}
		}
		break;
	case 0x88: // <SLA+W; >ACK; <DATA; >NACK (N/A)
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	// case 0x90: // <General call; >ACK; <Data (<DATA; >(N)ACK)
	// case 0x98: // <General call; >NACK; <Data (N/A)
	case 0xA0: // STOP or repeated START while slave (N/A)
		if (i2c_slave_receive && i > 0)
			i2c_slave_receive(buffer, i);
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
// SLAVE TRANSMITTER
	case 0xA8: // <SLA+R; >ACK (>END; <NACK or >DATA; <ACK)
		l = 0;
		if (i2c_slave_transmit)
			l = i2c_slave_transmit(buffer, I2C_BUFFER_SIZE);
		i = 0;
		if (l > 0)
		{
			TWDR = buffer[i];
			i++;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			TWDR = 0xFF;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
		}
		break;
	// case 0xB0: // <SLA+R; >ACK; Arbitration lost (>END; <NACK or >DATA; <ACK)
	case 0xB8: // <SLA+R; >ACK; >DATA; <ACK (>END; <NACK or >DATA; <ACK)
		if (i < l)
		{
			TWDR = buffer[i];
			i++;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			l = 0;
			if (i2c_slave_transmit)
				l = i2c_slave_transmit(buffer, I2C_BUFFER_SIZE);
			i = 0;
			if (l > 0)
			{
				TWDR = buffer[i];
				i++;
				TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
			}
			else
			{
				TWDR = 0xFF;
				TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
			}
		}
		break;
	case 0xC0: // <SLA+R; >ACK; >DATA; <NACK (N/A)
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	case 0xC8: // <SLA+R; >ACK; >END; <ACK (N/A)
		// master asks more data
		// TODO: call error handler
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	default:
		// unknown error
		// TODO: call error handler
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	}
}

void i2c_slave_init(unsigned char address)
{
	TWAR = address << 1; // address
	TWAMR = 0x00; // address mask
	TWBR = 0x00; // bitrate
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWEA);
}

