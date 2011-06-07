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
enum
{
	NONE = 0,
	AFTER_ZCROSS = 1,
	AFTER_SWITCH = 2,
	WAIT_ADC = 3,
	WAIT_ZCROSS = 4,
	WAIT_ZCROSS2 = 5
}  state = 0;
volatile int ref_speed = 2000;

// NEVER EVER TRY TO ACCESS PORTS DIRECTLY OR YOUR MOSFETs WILL EXPLODE!!!

void init_ports(void)
{
	DDRB = 0x07;
	PORTB = 0x00;
	DDRD = 0xBC; // 10111100
	PORTD = 0x00;
	DIDR0 = 0x07;
}

void set_led(void)
{
	PORTD |= 0x04;
}

void clr_led(void)
{
	PORTD &= ~0x04;
}
void toggle_led(void)
{
	if (PORTD & 0x04)
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

volatile unsigned char i = 3;
volatile unsigned char j = 0;
volatile int l = 0;
volatile char h = 0;
unsigned short i2c[100];

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
			ref_speed = 100*ch;
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
			TWDR = i2c[j] & 0x00FF;
			h = 1;
		}
		else
		{
			TWDR = (i2c[j] >> 8) & 0x00FF;
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
}

unsigned char sense = 0;

void switch_phase(void)
{
	switch(phase)
	{
	case 0:
		sense = 0xC2;
		ACSR = (ACSR & ~0x03) | 0x03;
		clr_p(0x04);
		set_p(0x01);
		phase = 1;
		break;
	case 1:
		sense = 0xC1;
		ACSR = (ACSR & ~0x03) | 0x02;
		clr_n(0x02);
		set_n(0x04);
		phase = 2;
		break;
	case 2:
		sense = 0xC0;
		ACSR = (ACSR & ~0x03) | 0x03;
		clr_p(0x01);
		set_p(0x02);
		phase = 3;
		break;
	case 3:
		sense = 0xC2;
		ACSR = (ACSR & ~0x03) | 0x02;
		clr_n(0x04);
		set_n(0x01);
		phase = 4;
		break;
	case 4:
		sense = 0xC1;
		ACSR = (ACSR & ~0x03) | 0x03;
		clr_p(0x02);
		set_p(0x04);
		phase = 5;
		break;
	case 5:
		sense = 0xC0;
		ACSR = (ACSR & ~0x03) | 0x02;
		clr_n(0x01);
		set_n(0x02);
		phase = 0;
		break;
	default:
		break;
	}
}

void pulse(unsigned char t)
{
	OCR2A = 0xFF;
	OCR2B = t-1;
	TCNT2 = 0xFF;
	if (t > 0)
	{
		TCCR2A = (1 << COM2B1) |  (1 << WGM21) | (1 << WGM20);
	}
	else
	{
		TCCR2A = (0 << COM2B1) |  (1 << WGM21) | (1 << WGM20);
	}
	TCCR2B = (1 << WGM22) | 0x01;
	i2c[1] = (unsigned short)t;
}

volatile int cent = 0;

SIGNAL(TIMER1_OVF_vect)
{
	cent++;
}

unsigned int time()
{
	if (cent > 0)
		return 0;
	return TCNT1;
}

void init_analog(void)
{
	ADMUX = (1 << REFS1) | (1 << REFS0);
	ADCSRA = (1 << ADIE);
	ADCSRB = (1 << ACME);
	ACSR = (1 << ACIE) | 0x03;
}

unsigned short current = 0;

void measure_current()
{
	ADMUX = 0xC6;
	ACSR |= 1 << ACD;
	ADCSRA |= (1 << ADEN) | (1 << ADSC);
}

SIGNAL(ADC_vect)
{
	switch(state)
	{
	case 3:
		current = ADC;
		i2c[2] = current;
		state = 4;
		OCR1B = TCNT1 + 10;
		break;
	default:
		break;
	}
}

unsigned char bad_cycles = 0;

SIGNAL(TIMER1_COMPA_vect)
{
	switch(state)
	{
	case 1:
		set_led();
		switch_phase();
		state = 2;
		TCNT1 = 0;
		cent = 0;
		OCR1B = 10;
		if (bad_cycles > 0)
		{
			bad_cycles--;
		}
		break;
	case 5:
		clr_led();
		if (bad_cycles < 2)
		{
			switch_phase();
			state = 2;
			TCNT1 = 0;
			cent = 0;
			OCR1B = 10;
			bad_cycles++;
		}
		break;
	default:
		break;
	}
}

unsigned short timeout = 100;
unsigned char power = 2;

SIGNAL(TIMER1_COMPB_vect)
{
	switch(state)
	{
	case 1: // wait for start
		break;
	case 2: // generate pulse
		pulse(power);
		state = 3;
		measure_current();
		break;
	case 3: // wait ADC result
		break;
	case 4: // enable analog comparator
		ADCSRA &= ~(1 << ADEN);
		ADMUX = sense;
		ACSR &= ~(1 << ACD);
		state = 5;
//		OCR1B = OCR1A - 100;
		break;
	case 5: // wait zero-cross
		break;
	default:
		break;
	}
}

SIGNAL(ANALOG_COMP_vect)
{
	unsigned int t = time();
	ACSR |= 1 << ACD;
	ADCSRA |= 1 << ADEN;
	switch(state)
	{
	case 5:
		state = 1;
		OCR1A = t + t/2;
		if (t < ref_speed && power > 0)
			power--;
		else if (t > ref_speed && power < 10)
			power++;
		break;
	default:
		break;
	}
	i2c[0] = t;
}

void init_timer1(void)
{
	TCCR1A = 0x00;
	TCCR1B = 0x01;
	TCCR1C = 0x00;
	TIMSK1 = (1 << TOIE1) | (1 << OCIE1A) | (1 << OCIE1B);
	OCR1A = 0xFFFF;
}

void init_timer2(void)
{
	TIMSK2 = (1 << OCIE2B) | (1 << OCIE2A) | (1 << TOIE2);
}

int main(void)
{

	CLKPR = 0x80;
	CLKPR = 0x00;

	init_ports();
	init_timer1();
	init_timer2();
	init_analog();

	TWAR = 1 << 1;
	TWCR = 0x45;
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWEA);

	sei();
	state = 5;
	while (1)
	{
     		_delay_ms(1000);
     	}
}

