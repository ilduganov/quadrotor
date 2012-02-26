#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdio.h>

#define FIXED 16
#define FIXED2 8
#define FIXED4 4
#define MAX (1l << FIXED)

long mul(long x, long y)
{
	return ((x >> FIXED4) * (y >> FIXED4)) >> FIXED2;
}

volatile unsigned char phase = 0;
volatile unsigned int flags = 0;
volatile unsigned int voltage, pressure;
volatile unsigned int arx_t, ary_t, arz_t;
volatile long arx, ary, arz;
volatile long arx_b = 512;
volatile long ary_b = 512;
volatile long arz_b = 512;
volatile long arx_g = 1;
volatile long ary_g = 1;
volatile long arz_g = 1;
volatile unsigned int ax_t, ay_t, az_t;
volatile long ax, ay, az;
volatile long ax_b = 0;
volatile long ay_b = 0;
volatile long az_b = 0;
volatile long ax_g = 1;
volatile long ay_g = 1;
volatile long az_g = 1;
volatile unsigned int mx_t, my_t, mz_t;
volatile long mx, my, mz;
volatile long mx_b = 0;
volatile long my_b = 0;
volatile long mz_b = 0;
volatile long mx_g = 1;
volatile long my_g = 1;
volatile long mz_g = 1;

// clock

void clock_init(void)
{
	CLKPR = 0x80;
	CLKPR = 0x00;
}

// 16-bit timer

volatile unsigned int epoch = 0;

void timer1_init(void)
{
	TCCR1B = (1 << CS10) | (1 << WGM12);
	OCR1A = 50000;
	OCR1B = 25000;
	TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B);
}

SIGNAL(TIMER1_COMPA_vect)
{
	epoch++;
	flags |= 0x8000;
}

SIGNAL(TIMER1_COMPB_vect)
{
}

unsigned long time(void)
{
	unsigned int tcnt1 = TCNT1;
	unsigned int epoch1 = epoch;
	unsigned int tcnt2 = TCNT1;
	if (tcnt2 < tcnt1)
		epoch1 = epoch - 1;
	return ((unsigned long)epoch1 << 16) + tcnt1;
}

// ADC

void adc_init(void)
{
	ADMUX = 0x02;
	ADCSRA = (7 << ADPS0) | (1 << ADIE) | (1 << ADEN);
	DIDR0 = 0xFF;
}

volatile int count = 0;
volatile char s = 0;

SIGNAL(ADC_vect)
{
	switch(ADMUX & 0x1F)
	{
	case 0x00:
		flags |= 0x0100;
		voltage = ADC;
		break;
	case 0x01:
		pressure = ADC;
		flags |= 0x0200;
		break;
	case 0x02: // x
		arx_t = TCNT1;
		arx = (ADC - arx_b) * arx_g;
		flags |= 0x0001;
		break;
	case 0x03: // y
		ary_t = TCNT1;
		ary = (ADC - ary_b) * ary_g;
		flags |= 0x0002;
		break;
	case 0x04: // z
		arz_t = TCNT1;
		arz = (ADC - arz_b) * arz_g;
		flags |= 0x0004;
		break;
	default:
		break;
	}
}

// I2C

void i2c_init(void)
{
//	PORTC |= 0x03;
//	DDRC &= ~0x03;

	TWBR = (F_CPU / 50000UL - 16) / 2;
	TWCR = 1<<TWEN | 1<<TWIE | 1<<TWEA;
	TWSR = 0x00;
}

volatile char i2c_slave;
volatile char i2c_write;
volatile char i2c_read;
volatile char i2c_buffer[16];
volatile unsigned char i2c_index;
volatile char i2c_error;

SIGNAL(TWI_vect)
{
	switch (TWSR & 0xF8)
	{
	// IIC master
	case 0x00: // bus fail
		TWCR = 1<<TWSTO | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		i2c_error = 1;
		break;
	case 0x08: // start transmitted
		if (i2c_write > 0)
		{
			TWDR = i2c_slave;
		}
		else
		{
			TWDR = i2c_slave | 1;
		}
		TWCR = 1<<TWINT | 1<<TWEN | 1<<TWIE;
		break;
	case 0x10: // repeated start transmitted
		if (i2c_write > 0)
		{
			TWDR = i2c_slave;
		}
		else
		{
			TWDR = i2c_slave | 1;
		}
		TWCR = 1<<TWINT | 1<<TWEN | 1<<TWIE;
		break;
	case 0x18: // SLA+W transmitted ACK received
		i2c_index = 0;
		TWDR = i2c_buffer[i2c_index];
		i2c_index++;
		TWCR = 1<<TWINT | 1<<TWEN | 1<<TWIE;
		break;
	case 0x20: // SLA+W transmitted NACK received
		TWCR = 1<<TWSTO | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		i2c_error = 2;
		break;
	case 0x28: // data transmitted ACK received
		if (i2c_index < i2c_write)
		{
			TWDR = i2c_buffer[i2c_index];
			i2c_index++;
			TWCR = 1<<TWINT | 1<<TWEN | 1<<TWIE;
		}
		else if (i2c_read > 0)
		{
			i2c_write = 0;
			TWCR = 1<<TWSTA | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		}
		else
		{
			i2c_write = 0;
			TWCR = 1<<TWSTO | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		}
		break;
	case 0x30: // data transmitted NACK received
		TWCR = 1<<TWSTO | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		i2c_error = 3;
		break;
	case 0x38: // arbitration lost
		TWCR = 1<<TWSTA | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		i2c_error = 4;
		break;
	case 0x40: // SLA+R transmitted ACK received
		i2c_index = 0;
		if (i2c_read > 1)
		{
			TWCR = 1<<TWINT | 1<<TWEA | 1<<TWEN | 1<<TWIE;
		}
		else
		{
			TWCR = 1<<TWINT | 1<<TWEN | 1<<TWIE;
		}
		break;
	case 0x48: // SLA+R transmitted NACK received
		TWCR = 1<<TWSTO | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		i2c_error = 5;
		break;
	case 0x50: // data received ACK returned
		i2c_buffer[i2c_index] = TWDR;
		i2c_index++;
		if (i2c_index > i2c_read)
		{
			TWCR = 1<<TWINT | 1<<TWEN | 1<<TWIE;
		}
		else
		{
			TWCR = 1<<TWINT | 1<<TWEA | 1<<TWEN | 1<<TWIE;
		}
		break;
	case 0x58: // data received NACK returned
		i2c_buffer[i2c_index] = TWDR;
		i2c_read = 0;
		TWCR = 1<<TWSTO | 1<<TWINT | 1<<TWEN | 1<<TWIE;
		// stop
		break;
	// IIC slave
	case 0x68:
	case 0x78:
	case 0x60:
	case 0x70:
	case 0x80:
	case 0x90:
	case 0x88:
	case 0x98:
	case 0xA0:
	case 0xA8:
	case 0xB0:
	case 0xB8:
	case 0xC0:
	case 0xC8:
	default:
		i2c_error = 0xFF;
		break;
	}
}

void twi_write(char addr, char reg, char data)
{
	i2c_buffer[0] = reg; i2c_buffer[1] = data;
	i2c_write = 2;
	i2c_read = 0;
	i2c_slave = addr;
	i2c_error = 0;
	TWCR = 1<<TWSTA | 1<<TWINT | 1<<TWEN | 1<<TWIE;
	while (i2c_write > 0 && i2c_error == 0);
}

char twi_read(char addr, char reg)
{
	i2c_buffer[0] = reg;
	i2c_write = 1;
	i2c_read = 1;
	i2c_slave = addr;
	i2c_error = 0;
	TWCR = 1<<TWSTA | 1<<TWINT | 1<<TWEN | 1<<TWIE;
	while (i2c_read > 0 && i2c_error == 0);
	return i2c_buffer[0];
}

// SERIAL

void serial_init(void)
{
	UBRR0 = 8; // 115200b
	UCSR0A = 1 << U2X0;
	UCSR0B = 1 << RXCIE0 | 1 << TXCIE0 | 1 << RXEN0 | 1 << TXEN0;
	UCSR0C = 3 << UCSZ00 | 1 << USBS0;
}

volatile char uart = 0;

volatile unsigned char serial_buffer_index = 0;
char serial_buffer[256];

void print(void)
{
	serial_buffer_index = 0;
	UDR0 = serial_buffer[serial_buffer_index];
	serial_buffer_index++;
}

void serial_wait(void)
{
	while (serial_buffer[serial_buffer_index]);
}

SIGNAL(USART0_RX_vect)
{
	uart = UDR0;
}

SIGNAL(USART0_UDRE_vect)
{
}

SIGNAL(USART0_TX_vect)
{
	if (serial_buffer[serial_buffer_index])
	{
		UDR0 = serial_buffer[serial_buffer_index];
		serial_buffer_index++;
	}
}

// SPI

void spi_init(void)
{
	DDRB = 1 << PB6;
	SPCR = 1 << SPIE | 1 << SPE;
}

volatile char spi = 0;

SIGNAL(SPI_STC_vect)
{
	spi = SPDR;
}

// 

void acc_on(char on)
{
	twi_write(0x30, 0x23, 0x80);
	if (on)
		twi_write(0x30, 0x20, 0x27);
	else
		twi_write(0x30, 0x20, 0x07);
}

void mag_on(char on)
{
	twi_write(0x3C, 0x00, 0x14);
	twi_write(0x3C, 0x01, 0x20);
	if (on)
		twi_write(0x3C, 0x02, 0x00);
	else
		twi_write(0x3C, 0x02, 0x03);
}

long acc_x(void)
{
	signed char h = twi_read(0x30, 0x29);
	char l = twi_read(0x30, 0x28);
	return (h << 8) + l;
}

long acc_y(void)
{
	signed char h = twi_read(0x30, 0x2B);
	char l = twi_read(0x30, 0x2A);
	return (h << 8) + l;
}

long acc_z(void)
{
	signed char h = twi_read(0x30, 0x2D);
	char l = twi_read(0x30, 0x2C);
	return (h << 8) + l;
}

long mag_x(void)
{
	signed char h = twi_read(0x3C, 0x03);
	char l = twi_read(0x3C, 0x04);
	return (h << 8) + l;
}

long mag_y(void)
{
	signed char h = twi_read(0x3C, 0x05);
	char l = twi_read(0x3C, 0x06);
	return (h << 8) + l;
}

long mag_z(void)
{
	signed char h = twi_read(0x3C, 0x07);
	char l = twi_read(0x3C, 0x08);
	return (h << 8) + l;
}

int acc_rdy(void)
{
	unsigned char status = twi_read(0x30, 0x27);
	if (status & 0x08)
		return 1;
	return 0;
}

int mag_rdy(void)
{
	unsigned char status = twi_read(0x3C, 0x09);
	if (status & 0x01)
		return 1;
	return 0;
}

int main(void)
{
	clock_init();
	timer1_init();
	adc_init();
	serial_init();
	i2c_init();
//	spi_init();

	sei();

	acc_on(1);
	mag_on(1);

	while (1)
	{
/*
		if (acc_rdy())
		{
			ax_t = ay_t = az_t = TCNT1;
			ax = acc_x();
			ay = acc_y();
			az = acc_z();
		}
		if (mag_rdy())
		{
			mx_t = my_t = mz_t = TCNT1;
			mx = mag_x();
			my = mag_y();
			mz = mag_z();
		}
*/
		switch (flags)
		{
		case 0:
			break;
		case 0x0001: // arx
			sprintf(serial_buffer, "0,%u,%ld\n", arx_t, arx);
			print();
			serial_wait();
			break;
		case 0x0002: // ary
			sprintf(serial_buffer, "1,%u,%ld\n", ary_t, ary);
			print();
			serial_wait();
			break;
		case 0x0004: // arz
			sprintf(serial_buffer, "2,%u,%ld\n", arz_t, arz);
			print();
			serial_wait();
			break;
		case 0x0100: // voltage
			sprintf(serial_buffer, "3,%u\n", voltage);
			print();
			serial_wait();
			break;
		case 0x0200: // pressure
			sprintf(serial_buffer, "4,%u\n", pressure);
			print();
			serial_wait();
			break;
		case 0x8000: // timer
			switch(phase)
			{
			case 0:
				ADMUX = 0x00;
				ADCSRA |= 1 << ADSC;
				phase = 1;
				break;
			case 1:
				ADMUX = 0x01;
				ADCSRA |= 1 << ADSC;
				phase = 2;
				break;
			case 2:
				ADMUX = 0x02;
				ADCSRA |= 1 << ADSC;
				phase = 3;
				break;
			case 3:
				ADMUX = 0x03;
				ADCSRA |= 1 << ADSC;
				phase = 4;
				break;
			case 4:
				ADMUX = 0x04;
				ADCSRA |= 1 << ADSC;
				phase = 5;
				break;
			case 5:
				sprintf(serial_buffer, "5,%u,%ld\n", ax_t, ax);
				print();
				serial_wait();
				sprintf(serial_buffer, "5,%u,%ld\n", ay_t, ay);
				print();
				serial_wait();
				sprintf(serial_buffer, "5,%u,%ld\n", az_t, az);
				print();
				serial_wait();
				phase = 6;
				break;
			case 6:
				sprintf(serial_buffer, "6,%u,%ld\n", mx_t, mx);
				print();
				serial_wait();
				sprintf(serial_buffer, "6,%u,%ld\n", my_t, my);
				print();
				serial_wait();
				sprintf(serial_buffer, "6,%u,%ld\n", mz_t, mz);
				print();
				serial_wait();
				phase = 0;
				break;
			}
			break;
		default:
			sprintf(serial_buffer, "conflict\n");
			print();
			serial_wait();
			break;
		}
		flags = 0;
     	}
}

