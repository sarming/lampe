#include <inttypes.h>
#include <avr/sleep.h>
#include <avr/io.h>
/* #include <util/delay.h> */
#include <avr/interrupt.h>

// PCINT3/XTAL1/CLKI/~OC1B/ADC3
#define BUTTON PINB3

// PCINT4/XTAL2/CLKO/OC1B/ADC2
#define LED PB4

#define STATE(a)
enum {OFF, WAIT_ONE, WAIT_TWO, WAIT_THREE, DIM, ON, DIM_WAIT};
enum {UP, DOWN};

#define PWM_MIN 1
#define PWM_MAX 254

volatile uint8_t state;
volatile uint8_t direction;
volatile uint8_t pwm;

void ioinit (void) {
   // Clock prescale: /64 = 125 KHz
   CLKPR = _BV(CLKPCE);
   CLKPR = _BV(CLKPS2) | _BV(CLKPS2);

   TCCR0A |= WGM01; // timer 0 CTC mode
   TCCR0B |= _BV(CS02) | _BV(CS00); // /1024 prescale
   /* OCR0A = 0x80; */

   GTCCR |= _BV(PWM1B) | _BV(COM1B1);
   TCCR1 |= _BV(CS12) | _BV(CS10); // /16 prescale
   /* OCR1B = pwm; */

   /* Enable timer 1 overflow, timer 0 a compare interrupts. */
   TIMSK = _BV(TOIE1); // | _BV(OCIE0A);

   /* DDRB |= _BV(LED); */

   PORTB = _BV(BUTTON); //enable pullup

   // Pin change interrupt on button
   GIMSK |= _BV(PCIE);
   PCMSK |= _BV(PCINT3);

   sei ();
}

void set_timeout(uint8_t cnt) {
   TCNT0 = 0;
   OCR0A = cnt;
   TIFR |= _BV(OCF0A);
   if(cnt)
      TIMSK |= _BV(OCIE0A);
   else
      TIMSK &= ~_BV(OCIE0A);
}

ISR(PCINT0_vect) {
   set_sleep_mode(SLEEP_MODE_IDLE);
   if(state == OFF)
      set_timeout(0x80);
}

ISR (TIMER1_OVF_vect) {
   if (state == DIM ) { // || state == DIM_WAIT ) {
      switch (direction) {
         case UP:
            if (++pwm >= PWM_MAX){
               direction = DOWN;
               --pwm;
            }
            break;

         case DOWN:
            if (--pwm <= PWM_MIN) {
               direction = UP;
               ++pwm;
            }
            break;
      }
      OCR1B = pwm;
   }
}

ISR (TIMER0_COMPA_vect) {
   switch(state){
      case WAIT_ONE:
      case WAIT_TWO:
      case WAIT_THREE:
      case OFF:
         set_sleep_mode(SLEEP_MODE_PWR_DOWN);
         state = OFF;
         break;
      case DIM_WAIT:
         state = DIM;
         break;
   }
   set_timeout(0);
}

uint8_t button() {
   return !(PINB & _BV(BUTTON));

   // SW debounce 
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
   return !old_state;
}


int main (void) {

   state = OFF;
   pwm = PWM_MIN;
   direction = UP;

   ioinit ();

   while(1) {
      switch(state) {
         case WAIT_ONE:
            if(!button()) {
               cli();
               state = WAIT_TWO;
               STATE(WAIT_TWO);
               set_timeout(0x80);
               sei(); 
            }
            break;
         case WAIT_TWO:
            if(button()) {
               cli();
               state = WAIT_THREE;
               STATE(WAIT_THREE);
               set_timeout(0x80);
               sei(); 
            }
            break;
         case WAIT_THREE:
            if(!button()) {
               cli();
               state = ON;
               STATE(ON);
               DDRB |= _BV(LED);
               /* OCR1B = pwm; */
               /* OCR1B = 1; */
               /* OCR0A = 0xff; */
               set_timeout(0);
               sei();
            }
            break;
         case ON:
            if(button()) {
               cli();
               state = DIM_WAIT;
               STATE(DIM_WAIT);
               set_timeout(0xff);
               sei();
            }
            break;
         case DIM_WAIT:
            if(!button()) {
               cli();
               set_timeout(0);
               set_sleep_mode(SLEEP_MODE_PWR_DOWN);
               /* OCR1B = 0; */
               /* pwm = PWM_MIN; */
               /* direction = UP; */
               PORTB &= ~_BV(LED);
               DDRB &= ~_BV(LED);
               state = OFF;
               STATE(OFF);
               sei();
            }
            break;
         case DIM:
            if(!button()) {
               cli();
               set_timeout(0);
               state = ON;
               STATE(ON);
               sei();
            }
            break;
         case OFF:
            if(button()){
               cli();
               state = WAIT_ONE;
               STATE(WAIT_ONE);
               set_timeout(0x80);
               sei();
            }
            break;
      }
      sleep_mode();
      /* _delay_ms(500); */
   }

   return 0;
}
