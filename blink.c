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
enum {OFF, WAIT_ON, WAIT_TWO, ON_WAIT, DIM, ON, DIM_WAIT};
enum {UP, DOWN};

#define PWM_MIN 1
#define PWM_MAX 255

volatile uint8_t state = OFF;
volatile uint8_t direction = UP;
volatile uint8_t pwm = PWM_MIN;

uint8_t button() {
   static uint8_t old_state=0;
   static uint8_t change_cnt=0;

   return !(PINB & _BV(BUTTON));

   // SW debounce 
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

void ioinit (void) {
   // Clock prescale: /64
   CLKPR = _BV(CLKPCE);
   CLKPR = _BV(CLKPS2) | _BV(CLKPS2);

   TCCR0A |= WGM01; // timer 0 CTC mode
   TCCR0B |= _BV(CS02) | _BV(CS00); // /1024 prescale
   OCR0A = 0x80;

   GTCCR |= _BV(PWM1B) | _BV(COM1B1);
   TCCR1 |= _BV(CS12) | _BV(CS10); // /1 prescale
   /* OCR1B = pwm; */

   /* Enable timer 1 overflow, timer 0 a compare interrupts. */
   TIMSK = _BV(TOIE1) | _BV(OCIE0A);

   /* DDRB |= _BV(LED); */

   PORTB = _BV(BUTTON); //enable pullup

   // Pin change interrupt on button
   GIMSK |= _BV(PCIE);
   PCMSK |= _BV(PCINT3);

   sei ();

}

ISR(PCINT0_vect) {
   set_sleep_mode(SLEEP_MODE_IDLE);
   if(state == OFF) 
      TCNT0 = 0;
}

ISR (TIMER1_OVF_vect) {
   if (state == DIM ) { // || state == DIM_WAIT ) {
      switch (direction) {
         case UP:
            if (++pwm >= PWM_MAX)
               direction = DOWN;
            break;

         case DOWN:
            if (--pwm <= PWM_MIN)
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
      case ON_WAIT:
      case DIM_WAIT:
         state = DIM;
         OCR0A = 0x80;
         break;
   }
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
               cli();
               /* OCR1B = pwm; */
               DDRB |= _BV(LED);
               state = ON_WAIT;
               STATE(ON_WAIT);
               TCNT0=0;
               OCR0A = 0xff;
               sei();
            }
            break;
         case ON:
            if(button()) {
               cli();
               state = DIM_WAIT;
               STATE(DIM_WAIT);
               TCNT0 = 0;
               OCR0A = 0xff;
               sei();
            }
            break;
         case DIM_WAIT:
            if(!button()) {
               cli();
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
               state = ON;
               STATE(ON);
               sei();
            }
            break;
         case OFF:
            if(button()){
               cli();
               TCNT0=0;
               state=WAIT_ON;
               STATE(WAIT_ON);
               sei();
            }
            break;
      }
      sleep_mode();
      /* _delay_ms(500); */
   }

   return 0;
}
