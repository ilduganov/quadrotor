/*
	Copyright (C) 2011 Nikolay Ilduganov
*/

#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile char b = 0;
volatile char c = 0;
volatile char phase = 0;
volatile int ref_speed = 0;

// NEVER EVER TRY TO ACCESS PORTS DIRECTLY OR YOUR MOSFETs WILL EXPLODE!!!

void init_ports(void)
{
	DDRB = 0x07;
	PORTB = 0x00;
	DDRD = 0xBC; // 10111100
	PORTD = 0x00;
	DIDR0 = 0x07;
}

void set_led(char on)
{
	if (on == 0)
		PORTD &= ~0x04;
	else
		PORTD |= 0x04;
}

void clr_p(unsigned char m)
{
	char u = 0, v = 0, w = 0;
	if (m & 0x01)
		u = 1;
	if (m & 0x02)
		v = 1;
	if (m & 0x04)
		w = 1;
	PORTD &= ~((1 << 7) | (1 << 5) | (1 << 4));
	DDRD |= (u << 7) | (v << 5) | (w << 4);
}

void clr_n(unsigned char m)
{
	char u = 0, v = 0, w = 0;
	if (m & 0x01)
		u = 1;
	if (m & 0x02)
		v = 1;
	if (m & 0x04)
		w = 1;
	PORTB &= ~((u << 0) | (v << 1) | (w << 2));
}

void set_p(unsigned char m)
{
	clr_n(m);
	char u = 0, v = 0, w = 0;
	if (m & 0x01)
		u = 1;
	if (m & 0x02)
		v = 1;
	if (m & 0x04)
		w = 1;
	PORTD &= ~((1 << 7) | (1 << 5) | (1 << 4));
	DDRD &= ~((u << 7) | (v << 5) | (w << 4));
}

void set_n(unsigned char m)
{
	clr_p(m);
	char u = 0, v = 0, w = 0;
	if (m & 0x01)
		u = 1;
	if (m & 0x02)
		v = 1;
	if (m & 0x04)
		w = 1;
	PORTB |= (u << 0) | (v << 1) | (w << 2);
}

volatile unsigned char i;
volatile unsigned char j;
volatile int l = 0;
volatile char h = 0;
unsigned short t[100];

SIGNAL(TWI_vect)
{
	unsigned char ch;
	switch(TWSR & 0xF8)
	{
	case 0x60:
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	case 0x80:
		ch = TWDR;
		if (ch < 10)
		{
			ref_speed = 1000*ch;
		}
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	case 0xA8:
		TWDR = i;
		j = 0;
		h = 0;
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	case 0xB8:
		if (h == 0)
		{
			TWDR = t[j] & 0x00FF;
			h = 1;
		}
		else
		{
			TWDR = (t[j] >> 8) & 0x00FF;
			j++;
			h = 0;
		}
		if (j == i)
		{
			b = 0;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
		}
		else
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		break;
	default:
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	}
}

SIGNAL(ADC_vect)
{
}

SIGNAL(TIMER2_OVF_vect)
{
	TCCR2B = (1 << WGM22);
	TCCR2A = (0 << COM2B1) |  (1 << WGM21) | (1 << WGM20);
}

SIGNAL(TIMER2_COMPA_vect)
{
}

SIGNAL(TIMER2_COMPB_vect)
{
	clr_n(0x07);
	clr_p(0x07);
}

volatile int cent = 0;

SIGNAL(TIMER1_OVF_vect)
{
	cent++;
}

SIGNAL(TIMER1_COMPA_vect)
{
	ACSR |= (1 << ACIE);
}

SIGNAL(TIMER1_COMPB_vect)
{
}

void start_timer(void)
{
	TCNT1 = 0;
	cent = 0;
}

unsigned int stop_timer(void)
{
	if (cent > 0)
		return 0;
	return TCNT1;
}

void set_timer(unsigned int t)
{
	OCR1A = t;
}

void pulse(unsigned char t)
{
	TCNT2 = 0;
	OCR2A = 0xFF;
	OCR2B = t;
	if (t > 0)
	{
		TCCR2A = (1 << COM2B1) |  (1 << WGM21) | (1 << WGM20);
	}
	else
	{
		TCCR2A = (0 << COM2B1) |  (1 << WGM21) | (1 << WGM20);
	}
	TCCR2B = (1 << WGM22) | 0x01;
}

volatile unsigned char w = 1;

SIGNAL(ANALOG_COMP_vect)
{
	unsigned int time = stop_timer();
	unsigned char pos = 0;
	unsigned char neg = 0;
	clr_p(0x07);
	clr_n(0x07);
	if ((ADMUX & 0x07) == 0x00)
	{
		pos = 0x01;
		neg = 0x02;
		ADMUX = (ADMUX & ~0x07) | 0x01;
	}
	else if ((ADMUX & 0x07) == 0x01)
	{
		pos = 0x04;
		neg = 0x01;
		ADMUX = (ADMUX & ~0x07) | 0x02;
	}
	else if ((ADMUX & 0x07) == 0x02)
	{
		pos = 0x02;
		neg = 0x04;
		ADMUX = (ADMUX & ~0x07) | 0x00;
	}
	if ((ACSR & 0x03) == 0x03)
	{
		ACSR = 0x02;
		set_p(pos);
		set_n(neg);
	}
	else if ((ACSR & 0x03) == 0x02)
	{
		ACSR = 0x03;
		set_p(neg);
		set_n(pos);
	}
	i = 1;
	t[0] = time;
	if (time < 4000)
		set_led(1);
	else
		set_led(0);
/*
	int delta = t - 10000;
	if (delta < 0)
	{
		if (delta < -100) delta = -100;
		pulse(100 + delta);
	}
	else
	{
		if (delta > 100) delta = 100;
		pulse(100 + delta);
	}
*/
	pulse(90);
	start_timer();
}

void init_timer1(void)
{
	TCCR1A = 0;
	TCCR1B = 1;
	TCCR1C = 0;
	TIMSK1 = (1 << TOIE1) | (1 << OCIE1A) | (1 << OCIE1A);
}

void init_timer2(void)
{
	TIMSK2 = (1 << OCIE2B) | (1 << OCIE2A) | (1 << TOIE2);
}

void init_ac(void)
{
	ADMUX = 0xF0;
	ADCSRA = 0;
	ADCSRB = (1 << ACME);
	ACSR = (1 << ACIE) | 0x03;
}

int main(void)
{

	CLKPR = 0x80;
	CLKPR = 0x00;

	init_ports();
	init_timer1();
	init_timer2();
	init_ac();

	set_timer(3000);

	TWAR = 1 << 1;
	TWCR = 0x45;
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWEA);

	sei();
	while (1)
	{
     		_delay_ms(1000);
     	}
}

