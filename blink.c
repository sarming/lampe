#include <inttypes.h>
#include <avr/sleep.h>
#include <avr/io.h>          // (1)
/* #include <util/delay.h> */
#include <avr/interrupt.h>

/* #include "iocompat.h"		/1* Note [1] *1/ */

// PCINT3/XTAL1/CLKI/~OC1B/ADC3
#define BUTTON PINB3

// PCINT4/XTAL2/CLKO/OC1B/ADC2
#define LED PB4

#define STATE(a)
enum {OFF, WAIT_ON, DIM, DIM_WAIT, ON, WAIT_TWO};
enum {UP, DOWN};

volatile uint8_t state = OFF;
volatile uint8_t direction = UP;
volatile uint8_t pwm = 255;

uint8_t button() {
   static uint8_t old_state=0;
   static uint8_t change_cnt=0;

   if (old_state == !!(PINB & _BV(BUTTON)) ) {
      change_cnt = 0;
   } else {
      ++change_cnt;
   }
   if ( change_cnt > 5) {
      old_state = !old_state;
      change_cnt = 0;
   }
   return old_state;
}

void ioinit (void) {
   // Clock prescale: /64
   CLKPR = _BV(CLKPCE);
   CLKPR = _BV(CLKPS2) | _BV(CLKPS2);

   GTCCR = _BV(PWM1B) | _BV(COM1B1);
   TCCR1 |= _BV(CS12) | _BV(CS10);
   // /1024 prescaler
   TCCR0B |= _BV(CS02) | _BV(CS00);
   /* OCR1B = pwm; */

   /* Enable timer 1 overflow, timer 0 a compare interrupts. */
   TIMSK = _BV(TOIE1) | _BV(OCIE0A);

   TCCR0A |= WGM01; // CTC mode
   OCR0A = 0x80;

   DDRB |= _BV(LED);

   GIMSK |= _BV(PCIE);
   PCMSK |= _BV(PCINT3);

   sei ();

}

ISR(PCINT0_vect) {
   set_sleep_mode(SLEEP_MODE_IDLE);
   if(state == OFF) 
      TCNT0 = 0;
   PORTB &= ~_BV(LED);
}

ISR (TIMER1_OVF_vect) {
   if (state == DIM || state == DIM_WAIT ) {
      switch (direction) {
         case UP:
            if (++pwm == 0xff)
               direction = DOWN;
            break;

         case DOWN:
            if (--pwm == 0)
               direction = UP;
            break;
      }
      OCR1B = pwm;
   }
}

ISR (TIMER0_COMPA_vect) {
   switch(state){
      case WAIT_ON:
      case WAIT_TWO:
      case OFF:
         set_sleep_mode(SLEEP_MODE_PWR_DOWN);
         state = OFF;
         break;
      case DIM_WAIT:
         state = DIM;
         break;
   }
   /* PORTB ^= _BV(LED); */

}

int click() {
   static uint8_t was_on;
   if(button()) was_on = 1;
   if(was_on && !button()) {
      was_on = 0;
      return 1;
   }
   return 0;
}

int main (void) {

   ioinit ();

   while(1) {
      switch(state) {
         case WAIT_ON:
            if(!button()) {
               cli();
               state = WAIT_TWO;
               STATE(WAIT_TWO);
               TCNT0=0;
               sei(); 
            }
            break;
         case WAIT_TWO:
            if(button()) {
               OCR1B = pwm;
               state = DIM;
               STATE(ON);
            }
            break;
         case ON:
            if(button()) {
               cli();
               state = DIM_WAIT;
               STATE(DIM_WAIT);
               TCNT0=0;
               sei();
            }
            break;
         case DIM_WAIT:
            if(!button()) {
               set_sleep_mode(SLEEP_MODE_PWR_DOWN);
               state = OFF;
               STATE(OFF);
            }
            break;
         case DIM:
            if(!button()) {
               state = ON;
               STATE(ON);
            }
            break;
         case OFF:
            OCR1B = 0xff;
            /* pwm = 0xff; */
            /* direction = UP; */
            /* PORTB |= _BV(LED); */
            if(button()){
               TCNT0=0;
               state=WAIT_ON;
               STATE(WAIT_ON);
            }
            break;
         default:
            button();
      }
      /* pwm=button()*128; */
      sleep_mode();
      /* PORTB ^= (button << LED); */
      /* _delay_ms(500); */
   }

   return 0;
}
