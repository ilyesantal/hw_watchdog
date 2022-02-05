/*
 * hw_watchdog.c
 *
 * Created: 2022. 02. 02. 18:30:23
 * Author : Antal Ilyes
 */ 

#define WAVEFORM_GENERATION_MODE_NORMAL 0
#define WAVEFORM_GENERATION_MODE_CTC 4

#define CLOCK_SELECT_BIT_NO_CLK 0
#define CLOCK_SELECT_BIT_NO_PRESCALING 1
#define CLOCK_SELECT_BIT_PRESCALE_8 8
#define CLOCK_SELECT_BIT_PRESCALE_64 64
#define CLOCK_SELECT_BIT_PRESCALE_256 256
#define CLOCK_SELECT_BIT_PRESCALE_1024 1024
#define CLOCK_SELECT_BIT_EXT_T0_FALLING 6
#define CLOCK_SELECT_BIT_EXT_T0_RISING 7

#define F_CPU 8000000U

#include <limits.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>

void init_pins();

#define TIMER_FREQ 1000
#define CLOCK_SELECT_BIT CLOCK_SELECT_BIT_PRESCALE_1024
#define WAVEFORM_GENERATION_MODE WAVEFORM_GENERATION_MODE_CTC
#if ((CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_1024) || (CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_256) || (CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_64) || (CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_8))
#define OUTPUT_COMPARE_REGISTER_0_A (F_CPU / CLOCK_SELECT_BIT / TIMER_FREQ)
#else
#define OUTPUT_COMPARE_REGISTER_0_A (F_CPU / TIMER_FREQ)
#endif
void init_timer();

void reset_timer();

void set_pin(uint8_t pin, uint8_t state);

uint8_t read_pin(uint8_t pin);

#define WATCHDOG_SECONDS 10

#if (WATCHDOG_SECONDS * TIMER_FREQ > INT16_MAX)
#error "overflow error"
#endif

#define COUNTER_SET_MAX 10000

#if (COUNTER_SET_MAX > INT16_MAX)
#error "overflow error"
#endif

static volatile uint16_t timer_count_0 = 0;	// how many times timer initiated since input0
static volatile uint16_t timer_count_1 = 0;	// how many times timer initiated since input1

static volatile uint8_t device_0_active = 0;
static volatile uint8_t device_1_active = 0;

static volatile int16_t counter_set_2 = 0;
static volatile int16_t counter_set_3 = 0;

// when TIM0 timer triggers, calling this ISR
ISR(TIM0_COMPA_vect){
	if(device_0_active){
		timer_count_0++;
		
		if(timer_count_0 > TIMER_FREQ * WATCHDOG_SECONDS){
			counter_set_2 = COUNTER_SET_MAX;
			device_0_active = 0;
			timer_count_0 = 0;
		}
	}
	if(device_1_active){
		timer_count_1++;

		if(timer_count_1 > TIMER_FREQ * WATCHDOG_SECONDS){
			counter_set_3 = COUNTER_SET_MAX;
			device_1_active = 0;
			timer_count_1 = 0;
		}
	}
}

int main(void)
{
	CCP = 0xD8;						// unlock protected registers
	CLKPSR = 0;						// no divider = 8MHz

	init_pins();
	init_timer();
	
    while (1) 
    {
		if(PINB & PINB0){	// PB0 pulled high
			if(!device_0_active){
				device_0_active = 1;
			}
			timer_count_0 = 0;
		}
		
		if(PINB & PINB1){	// PB1 pulled high
			if(!device_1_active){
				device_1_active = 1;
			}
			timer_count_1 = 0;
		}
		
		if(counter_set_2 > 0){
			if(counter_set_2 == COUNTER_SET_MAX){
				set_pin(PINB2, 1);
			}
			counter_set_2--;
		}
		else if(counter_set_2 == 0){
			set_pin(PINB2, 0);
			counter_set_2--;
		}
	
		if(counter_set_3 > 0){
			if(counter_set_3 == COUNTER_SET_MAX){
				set_pin(PINB3, 1);
			}
			counter_set_3--;
		}
		else if(counter_set_3 == 0){
			set_pin(PINB3, 0);
			counter_set_3--;
		}
		
		_delay_us(100);
    }
}

void set_pin(uint8_t pin, uint8_t state){
	
	if(state){
		PORTB |= (1 << pin);
	}
	else{
		PORTB &= ~(1 << pin);
	}
}

void init_pins(){
	
	DDRB = 0b00001100;				// PB3 and PB2 set to output, PB1 and PB0 set to input
	PUEB = 0b00000011;				// PB0 and PB1 pull-up enabled
	set_pin(PINB2, 0);
	set_pin(PINB3, 0);
}

void init_timer(){
	
	TCCR0A = 0b00000000;
	
	switch(WAVEFORM_GENERATION_MODE){
		case WAVEFORM_GENERATION_MODE_NORMAL:
			TCCR0B |= 0b00000000;
			break;
		case WAVEFORM_GENERATION_MODE_CTC:
			TCCR0B |= 0b00001000;
			break;
		default:						
			TCCR0B |= 0b00000000;
	}
	
	switch(CLOCK_SELECT_BIT){
		case CLOCK_SELECT_BIT_NO_CLK:
			TCCR0B |= 0b00000000;
			break;
		case CLOCK_SELECT_BIT_NO_PRESCALING:
			TCCR0B |= 0b00000001;
			break;
		case CLOCK_SELECT_BIT_PRESCALE_8:
			TCCR0B |= 0b00000010;
			break;
		case CLOCK_SELECT_BIT_PRESCALE_64:
			TCCR0B |= 0b00000011;
			break;
		case CLOCK_SELECT_BIT_PRESCALE_256:
			TCCR0B |= 0b00000100;
			break;
		case CLOCK_SELECT_BIT_PRESCALE_1024:
			TCCR0B |= 0b00000101;
			break;
		case CLOCK_SELECT_BIT_EXT_T0_FALLING:
			TCCR0B |= 0b00000110;
			break;
		case CLOCK_SELECT_BIT_EXT_T0_RISING:
			TCCR0B |= 0b00000111;
			break;
		default:
			TCCR0B |= 0b00000000;
	}
	
	OCR0AH = OUTPUT_COMPARE_REGISTER_0_A >> 8;
	OCR0AL = OUTPUT_COMPARE_REGISTER_0_A & 0xFF;
		
	TIMSK0 |= (1 << OCIE0A);		// set interrupt for timer
		
	sei();							// enable interrupts
}

void reset_timer(){
	
	cli();
	TCCR0B = 0b00001000;			// deactivate clock
	TCNT0 = 0;						// set clock to 0
	TCCR0B = 0b00001101;			// reactivate clock
	sei();
}