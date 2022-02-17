/*
 * hw_watchdog.c
 *
 * Created: 2022. 02. 02. 18:30:23
 * Author : Antal Ilyes
 */ 

// waveform types
#define WAVEFORM_GENERATION_MODE_NORMAL 0
#define WAVEFORM_GENERATION_MODE_CTC 4

// clock prescaler types
#define CLOCK_SELECT_BIT_NO_CLK 0
#define CLOCK_SELECT_BIT_NO_PRESCALING 1
#define CLOCK_SELECT_BIT_PRESCALE_8 8
#define CLOCK_SELECT_BIT_PRESCALE_64 64
#define CLOCK_SELECT_BIT_PRESCALE_256 256
#define CLOCK_SELECT_BIT_PRESCALE_1024 1024
#define CLOCK_SELECT_BIT_EXT_T0_FALLING 6
#define CLOCK_SELECT_BIT_EXT_T0_RISING 7

// CPU clock frequency in Hz
#define F_CPU 8000000U

#include <limits.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>

void init_pins();

// defines for the timer initialization
//      timer frequency, in Hz
#define TIMER_FREQ 100
//		prescaler type select
#define CLOCK_SELECT_BIT CLOCK_SELECT_BIT_PRESCALE_1024
//		waveform type select
#define WAVEFORM_GENERATION_MODE WAVEFORM_GENERATION_MODE_CTC
//		calculate the compare register value based on previous parameters
#if ((CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_1024) || (CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_256) || (CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_64) || (CLOCK_SELECT_BIT == CLOCK_SELECT_BIT_PRESCALE_8))
#define OUTPUT_COMPARE_REGISTER_0_A (F_CPU / CLOCK_SELECT_BIT / TIMER_FREQ)
#else
#define OUTPUT_COMPARE_REGISTER_0_A (F_CPU / TIMER_FREQ)
#endif
void init_timer();

void reset_timer();

void init_pcint();

void set_pin(uint8_t pin, uint8_t state);

uint8_t read_pin(uint8_t pin);

// count of iterations while output pulled high
#define HIGH_OUT_COUNTER_MAX 50000
// validation
#if (HIGH_OUT_COUNTER_MAX > INT32_MAX)
#error "overflow error"
#endif

// possible device states
#define DEVICE_STATUS_INACTIVE 0
#define DEVICE_STATUS_INIT 1
#define DEVICE_STATUS_ACTIVE 2

// how many times timer initiated since input
static volatile uint16_t timer_count_0 = 0;	
static volatile uint16_t timer_count_1 = 0;

// status of devices
static volatile uint8_t device_0_status = DEVICE_STATUS_INACTIVE;
static volatile uint8_t device_1_status = DEVICE_STATUS_INACTIVE;

// current counter for devices
static volatile int32_t counter_device_2 = 0;
static volatile int32_t counter_device_3 = 0;

// the rough time between pings
static volatile uint16_t counter_val_0 = 0;
static volatile uint16_t counter_val_1 = 0;

static volatile uint8_t pin_status = 0;

// when TIM0 timer triggers, calling this ISR
ISR(TIM0_COMPA_vect){
	// check device status
	switch(device_0_status){
		// if the device is active
		case DEVICE_STATUS_ACTIVE:
			// increase counter
			timer_count_0++;
			
			//if the counter reaches the maximum value - which is 5 times the input frequency -, set the device to inactive, set the output to high
			if(timer_count_0 > counter_val_0 * 5){
				counter_device_2 = HIGH_OUT_COUNTER_MAX;
				device_0_status = DEVICE_STATUS_INACTIVE;
				timer_count_0 = 0;
			}
			break;
		// if the device is set to init, count the cycles until set to active - which happens when is in init but next input comes
		case DEVICE_STATUS_INIT:
			counter_val_0++;
			break;
		case DEVICE_STATUS_INACTIVE:
			//do nothing
			break;
	}
	
	switch(device_1_status){
		case DEVICE_STATUS_ACTIVE:
			timer_count_1++;
		
			if(timer_count_1 > counter_val_1 * 5){
				counter_device_3 = HIGH_OUT_COUNTER_MAX;
				device_1_status = DEVICE_STATUS_INACTIVE;
				timer_count_1 = 0;
			}
			break;
		case DEVICE_STATUS_INIT:
			counter_val_1++;
			break;
		case DEVICE_STATUS_INACTIVE:
			//do nothing
			break;
	}
}

// when input pins change state, calling this ISR
ISR(PCINT0_vect){
	// PB0 pulled high
	if(PINB & (1 << PINB0)){
		switch(device_0_status){
			// if device was inactive, set it to init, this way the timer starts counting the time between pings, reset the counter value
			case DEVICE_STATUS_INACTIVE:
				counter_val_0 = 0;
				device_0_status = DEVICE_STATUS_INIT;
				break;
			// if was in init, go to active, the timer starts counting
			case DEVICE_STATUS_INIT:
				if(counter_val_0 > 100){
					device_0_status = DEVICE_STATUS_ACTIVE;
				}
				break;
			// if device is active, reset counter, so it doesn't set the out high
			case DEVICE_STATUS_ACTIVE:
				timer_count_0 = 0;
				break;
		}
	}
	// PB1 pulled high
	if(PINB & (1 << PINB1)){
		switch(device_1_status){
			case DEVICE_STATUS_INACTIVE:
				counter_val_1 = 0;
				device_1_status = DEVICE_STATUS_INIT;
				break;
			case DEVICE_STATUS_INIT:
				if(counter_val_1 > 100){
					device_1_status = DEVICE_STATUS_ACTIVE;
				}
				break;
			case DEVICE_STATUS_ACTIVE:
				timer_count_1 = 0;
				break;
		}
	}
}

int main(void)
{
	CCP = 0xD8;						// unlock protected registers
	CLKPSR = 0;						// no divider = 8MHz

	init_pins();
	init_timer();
	init_pcint();
	
	// main loop
    while (1)
    {
		// if output should be set high, set it (only when the value is the max value, so it was just set recently
		if(counter_device_2 > 0){
			if(counter_device_2 == HIGH_OUT_COUNTER_MAX){
				set_pin(PINB2, 1);
			}
			counter_device_2--;
		}
		// if we keeped in high for long enough, set back to low, decrease one more, so it doesn't come to this state again
		else if(counter_device_2 == 0){
			set_pin(PINB2, 0);
			counter_device_2--;
			
		}
	
		if(counter_device_3 > 0){
			if(counter_device_3 == HIGH_OUT_COUNTER_MAX){
				set_pin(PINB3, 1);
			}
			counter_device_3--;
		}
		else if(counter_device_3 == 0){
			set_pin(PINB3, 0);
			counter_device_3--;
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

// initialize input and output pins
void init_pins(){
	DDRB |= (1 << PINB2);			// Set the pin pin 2 as output
	DDRB |= (1 << PINB3);			// Set the pin pin 3 as output
	DDRB &= ~(1 << PINB0);			// Set the pin pin 0 as input
	DDRB &= ~(1 << PINB1);			// Set the pin pin 1 as input
	PORTB |= (1 << PINB0);			//activate pull-up resistor for pin 0
	PORTB |= (1 << PINB1);			//activate pull-up resistor for pin 1
	set_pin(PINB2, 0);
	set_pin(PINB3, 0);
}

// initialize timer
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

// initialize interrupt for pins
void init_pcint(){
	PCICR |= (1 << PCIE0);
	PCMSK |= (1 << PCINT0);
	PCMSK |= (1 << PCINT1);
}