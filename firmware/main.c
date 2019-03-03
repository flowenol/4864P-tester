/* Name: main.c
 * Author: flovenol@gmail.com
 * License: MIT License
 */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define RAS PB0
#define CAS PB1
#define WE PB2
#define DOUT PB3
#define DIN PB4
#define LED PB5

#define address_t uint8_t

typedef enum {HIGH, LOW} state_t;

#define MEM_OK 0x01
#define MEM_REFRESHED 0x02

uint8_t flags = MEM_OK;

void write_address(address_t address) {
    PORTD = address;
}

void write_pin(uint8_t pin, state_t data) {
    if (data == HIGH) {
        PORTB = PORTB  | _BV(pin);
    } else {
        PORTB = PORTB & ~(_BV(pin));
    }
}

state_t read_pin(uint8_t pin) {
    uint8_t data = (PINB & _BV(pin));
    if (data > 0) {
        return HIGH;
    }
    return LOW;
}

void refresh() {
    address_t i;
    for (i = 0; i < 128; i++) {
        write_address(i);

        write_pin(RAS, LOW);
        write_pin(RAS, HIGH);
    }

    flags |= MEM_REFRESHED;
}

void write_data(address_t address_h, address_t address_l, state_t data) {
    write_address(address_h);
    write_pin(DIN, data);
    write_pin(RAS, LOW);
    write_pin(WE, LOW);
    write_address(address_l);
    write_pin(CAS, LOW);
    write_pin(WE, HIGH);
    write_pin(RAS, HIGH);
    write_pin(CAS, HIGH);
}

state_t read_data(address_t address_h, address_t address_l) {
    write_pin(WE, HIGH);
    write_address(address_h);
    write_pin(RAS, LOW);
    write_address(address_l);
    write_pin(CAS, LOW);
    state_t data = read_pin(DOUT);
    write_pin(RAS, HIGH);
    write_pin(CAS, HIGH);

    return data;
}

void setup() {

    // Address
    DDRD = 0xFF;

    // Control
    DDRB |= _BV(RAS);
    DDRB |= _BV(CAS);
    DDRB |= _BV(WE);
    DDRB &= ~(_BV(DOUT));
    DDRB |= _BV(DIN);
    DDRB |= _BV(LED);

    _delay_us(500.0);

    uint8_t i;
    for (i = 0; i < 8; i++) {
        refresh();
        _delay_ms(2);
    }

    //65536 - 30 = 65506
    TCNT1 = 65506;   // for 1.8 ms at 16 MHz
    TCCR1A = 0x00;
    TCCR1B = (_BV(CS10)) | (_BV(CS12));  // Timer mode with 1024 prescler
    TIMSK1 = (_BV(TOIE1)); // Enable timer1 overflow interrupt(TOIE1)
    sei();        // Enable global interrupts by setting global interrupt enable bit in SREG
}

ISR (TIMER1_OVF_vect)
{
    refresh();
    TCNT1 = 65506;   // for 1.8 ms at 16 MHz
}

void read_after_write() {
    address_t address_h;
    address_t address_l;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
            write_data(address_h, address_l, HIGH);
            state_t data = read_data(address_h, address_l);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                write_data(address_h, address_l, HIGH);
                data = read_data(address_h, address_l);
            }

            if (data != HIGH) {
                goto err;
            }

            write_data(address_h, address_l, LOW);
            data = read_data(address_h, address_l);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                write_data(address_h, address_l, LOW);
                data = read_data(address_h, address_l);
            }

            if (data != LOW) {
                goto err;
            }

            if (address_l == 0xFF) {
              break;
            }
        }

        if (address_h == 0xFF) {
          break;
        }
    }

    return;
err:
    flags &= ~MEM_OK;
    return;
}

void write_mem(state_t state) {
    address_t address_h;
    address_t address_l;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
            write_data(address_h, address_l, state);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                write_data(address_h, address_l, state);
            }

            if (address_l == 0xFF) {
              break;
            }
        }

        if (address_h == 0xFF) {
          break;
        }
    }
}

void read_mem(state_t state) {
    address_t address_h;
    address_t address_l;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
            state_t data = read_data(address_h, address_l);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                data = read_data(address_h, address_l);
            }

            if (data != state) {
                goto read_err;
            }

            if (address_l == 0xFF) {
              break;
            }
        }

        if (address_h == 0xFF) {
          break;
        }
    }

    return;
read_err:
    flags &= ~MEM_OK;
    return;
}


int main(void)
{
    setup();

    write_mem(HIGH);
    read_mem(HIGH);
    write_mem(LOW);
    read_mem(LOW);
    read_after_write();

    for(;;){
        if ((flags & MEM_OK) == MEM_OK) {
            write_pin(LED, HIGH);
        } else {
            write_pin(LED, HIGH);
            _delay_ms(100);
            write_pin(LED, LOW);
            _delay_ms(100);
        }
    }

    return 0;   /* never reached */
}
