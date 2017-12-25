#include <inttypes.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// PCINT3/XTAL1/CLKI/~OC1B/ADC3
#define BUTTON PINB3

// PCINT4/XTAL2/CLKO/OC1B/ADC2
#define LED PB4

#define STATE(a)
enum {OFF, WAIT_ONE, WAIT_TWO, WAIT_THREE, ON, DIM, DIM_WAIT};
enum {UP, DOWN};


volatile uint8_t state;
volatile uint8_t direction;
volatile uint8_t pwm;

void ioinit (void) {
   // Clock prescale: /64 = 125 KHz
   CLKPR = _BV(CLKPCE);
   CLKPR = _BV(CLKPS2) | _BV(CLKPS2);

   TCCR0A |= WGM01; // timer 0 CTC mode
   TCCR0B |= _BV(CS02) | _BV(CS00); // /1024 prescale
   OCR0A = 0x80;

   GTCCR |= _BV(PWM1B) | _BV(COM1B1);
   TCCR1 |= _BV(CS12) | _BV(CS10); // /16 prescale

   /* Enable timer 1 overflow, timer 0 a compare interrupts. */
   TIMSK = _BV(TOIE1); // | _BV(OCIE0A);

   /* DDRB |= _BV(LED); */

   PORTB = _BV(BUTTON); //enable pullup

   // Pin change interrupt on button
   GIMSK |= _BV(PCIE);
   PCMSK |= _BV(PCINT3);

}

EMPTY_INTERRUPT(PCINT0_vect);

#define PWM_MIN 1
#define OFFSET 32

uint8_t getpwm() {
   if(pwm <= OFFSET) return PWM_MIN;
   if(pwm >= 255-OFFSET) return 255;
   /* if(pwm == 240 && direction == UP) return 0; */
   /* if(pwm == 16 && direction == DOWN) return 0; */
   return pwm-OFFSET+PWM_MIN;
}

ISR (TIMER1_OVF_vect) {
   if (state == DIM ) { // || state == DIM_WAIT ) {
      switch (direction) {
         case UP:
            if (++pwm == 255){
               direction = DOWN;
            }
            break;
         case DOWN:
            if ( -- pwm == 0) {
               direction = UP;
            }
            break;
      }
      OCR1B = getpwm();
   }
}

int main (void) {

   state = OFF;
   pwm = PWM_MIN;
   direction = UP;

   set_sleep_mode(SLEEP_MODE_PWR_DOWN);

   ioinit ();

   while(1) {
      cli();

      uint8_t old_state = state;
      uint8_t button =  !(PINB & _BV(BUTTON));
      uint8_t timeout = TIFR & _BV(OCF0A);
      TIFR |= _BV(OCF0A);

      switch(old_state) {
         case OFF:
         case WAIT_TWO:
            if(button) ++state;
            if(timeout) state = OFF;
            break;
         case WAIT_ONE:
         case WAIT_THREE:
            if(!button) ++state;
            if(timeout) state = OFF;
            break;
         case ON:
            if(button) state = DIM_WAIT;
            break;
         case DIM_WAIT:
            if(!button) state = OFF;
            if(timeout) state = DIM;
            break;
         case DIM:
            if(!button) state = ON;
            break;
      }

      if(state != old_state) {
         switch(state) {
            case OFF:
               set_sleep_mode(SLEEP_MODE_PWR_DOWN);
               OCR1B = 0;
               /* pwm = PWM_MIN; */
               /* direction = UP; */
               /* PORTB &= ~_BV(LED); */
               DDRB &= ~_BV(LED);
               // fallthrough no break
            case WAIT_ONE:
            case WAIT_TWO:
            case WAIT_THREE:
               TCNT0 = 0;
               /* OCR0A = 0x80; */
               if(state == WAIT_ONE)
                  set_sleep_mode(SLEEP_MODE_IDLE);
               break;
            case ON:
               DDRB |= _BV(LED);
               OCR1B = getpwm();
               /* OCR1B = 254; */
               /* OCR0A = 0xff; */
               break;
            case DIM_WAIT:
               TCNT0 = 0;
               /* OCR0A = 0xff; */
               /* TIFR |= _BV(OCF0A); */
            case DIM:
               break;
         }

      }

      sei();
      sleep_mode();
   }

   return 0;
}
