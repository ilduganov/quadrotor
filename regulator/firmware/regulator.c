/*
	Copyright (C) 2011 Nikolay Ilduganov
*/

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
	PAUSE = 4,
	WAIT_ZCROSS = 5,
	TEST = 6
}  state = NONE;

volatile unsigned int ref_speed = 2000;
volatile unsigned char power_limit = 150;
unsigned int speed = 0;

// NEVER EVER TRY TO ACCESS PORTS DIRECTLY OR YOUR MOSFETs WILL EXPLODE!!!

void init_ports(void)
{
	DDRB = 0x07;
	PORTB = 0x00;
	DDRD = 0xB4; // 10110100
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

void clr_h(unsigned char m)
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

void clr_l(unsigned char m)
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

void set_h(unsigned char m)
{
	clr_l(m);
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

void set_l(unsigned char m)
{
	clr_h(m);
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
volatile unsigned short i2c[100];

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
		if (j < i)
		{
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		}
		else
		{
			b = 0;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
		}
		break;
	default:
		TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
		break;
	}
}

SIGNAL(TIMER2_OVF_vect)
{
}

SIGNAL(TIMER2_COMPA_vect)
{
	TCCR2A = (0 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (1 << WGM20);
	TCCR2B = (0 << WGM22);
	clr_h(0x07);
	clr_l(0x07);
}

SIGNAL(TIMER2_COMPB_vect)
{
}

volatile unsigned char sense = 0;
volatile unsigned char hi = 0;
volatile unsigned char lo = 0;

void switch_phase(void)
{
	switch(phase)
	{
	case 0:
		sense = 0xC2;
		ACSR = (ACSR & ~0x03) | 0x03;
		clr_h(0x04);
		set_h(0x01);
		hi = 0x01;
		lo = 0x02;
		phase = 1;
		break;
	case 1:
		sense = 0xC1;
		ACSR = (ACSR & ~0x03) | 0x02;
		clr_l(0x02);
		set_l(0x04);
		hi = 0x01;
		lo = 0x04;
		phase = 2;
		break;
	case 2:
		sense = 0xC0;
		ACSR = (ACSR & ~0x03) | 0x03;
		clr_h(0x01);
		set_h(0x02);
		hi = 0x02;
		lo = 0x04;
		phase = 3;
		break;
	case 3:
		sense = 0xC2;
		ACSR = (ACSR & ~0x03) | 0x02;
		clr_l(0x04);
		set_l(0x01);
		hi = 0x02;
		lo = 0x01;
		phase = 4;
		break;
	case 4:
		sense = 0xC1;
		ACSR = (ACSR & ~0x03) | 0x03;
		clr_h(0x02);
		set_h(0x04);
		hi = 0x04;
		lo = 0x01;
		phase = 5;
		break;
	case 5:
		sense = 0xC0;
		ACSR = (ACSR & ~0x03) | 0x02;
		clr_l(0x01);
		set_l(0x02);
		hi = 0x04;
		lo = 0x02;
		phase = 0;
		break;
	default:
		break;
	}
	i2c[2] = phase;
}

void pulse(unsigned char t)
{
	if (t > 0)
	{
		set_h(hi);
		set_l(lo);
		OCR2B = t-1;
		TCNT2 = 0xFF;
		TCCR2A = (1 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (1 << WGM20);
		TCCR2B = (0 << WGM22) | 0x02;
		i2c[1] = t;
	}
}

volatile int cent = 0;

SIGNAL(TIMER1_OVF_vect)
{
	cent++;
}

unsigned int time(void)
{
	if (cent > 0)
		return 0;
	return TCNT1;
}

void init_analog(void)
{
	ADMUX = (1 << REFS1) | (1 << REFS0);
	ADCSRA = (1 << ADIE) | 0x02;
	ADCSRB = (1 << ACME);
	ACSR = (1 << ACIE) | 0x03;
}

unsigned short current = 0;

void measure_current(void)
{
	ADMUX = 0xC6;
	ACSR |= 1 << ACD;
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	i = 3;
}

SIGNAL(ADC_vect)
{
	switch(state)
	{
	case WAIT_ADC:
		current = ADC;
		i2c[3] = current;
		i = 4;
		state = PAUSE;
		OCR1B = TCNT1 + 10;
		break;
	default:
		current = ADC;
		if (i < 20)
		{
			i2c[i] = current;
			i++;
		}
		else
		{
			ADCSRA &= ~(1 << ADATE);
		}
		break;
	}
}

unsigned short timeout = 100;
unsigned char power = 1;

unsigned char bad_cycles = 0;

SIGNAL(TIMER1_COMPA_vect)
{
	switch(state)
	{
	case AFTER_ZCROSS:
		clr_led();
		switch_phase();
		state = AFTER_SWITCH;
		TCNT1 = 0;
		cent = 0;
		OCR1B = 10;
		if (bad_cycles > 0)
		{
			bad_cycles--;
		}
		break;
	case WAIT_ZCROSS:
		set_led();
		if (bad_cycles < 2)
		{
			switch_phase();
			state = AFTER_SWITCH;
			TCNT1 = 0;
			cent = 0;
			OCR1B = 10;
			bad_cycles++;
		}
		else if (power > 0)
		{
			power--;
		}
		break;
	case TEST:
		switch_phase();
		TCNT1 = 0;
		cent = 0;
		pulse(100);
		measure_current();
	default:
		break;
	}
}

SIGNAL(TIMER1_COMPB_vect)
{
	switch(state)
	{
	case AFTER_ZCROSS: // wait for start
		break;
	case AFTER_SWITCH: // generate pulse
		pulse(power);
		state = WAIT_ADC;
		measure_current();
		break;
	case WAIT_ADC: // wait ADC result
		break;
	case PAUSE: // enable analog comparator
		ADCSRA &= ~(1 << ADEN);
		ADMUX = sense;
		ACSR &= ~(1 << ACD);
		state = WAIT_ZCROSS;
		break;
	case WAIT_ZCROSS: // wait zero-cross
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
	case WAIT_ZCROSS:
		state = AFTER_ZCROSS;
		if (t > ref_speed - 1000) // startup
		{
			OCR1A = t + 1000;
			if (power < power_limit)
				power++;
		}
		else if (t > ref_speed/2) // too slow
		{
			OCR1A = ref_speed;
			if (power < power_limit)
				power++;
		}
		else if (t < ref_speed/2) // too fast
		{
			OCR1A = t + t;
			if (power > 0)
				power--;
		}

		break;
	default:
		break;
	}
	speed = (speed * 7) + OCR1A;
	speed /= 8;
	i2c[0] = speed;
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
	OCR2A = 200;
	TCCR2A = (0 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (1 << WGM20);
	TCCR2B = (0 << WGM22);
	TIMSK2 = (1 << TOIE2) | (1 << OCIE2B) | (1 << OCIE2A);
	DDRD |= 0x08;
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
	//state = WAIT_ZCROSS;
	state = TEST;
	while (1)
	{
     		_delay_ms(1000);
     	}
}

