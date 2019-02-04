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

void writeAddress(address_t address) {
    PORTD = address;
}

void writePin(uint8_t pin, state_t data) {
    if (data == HIGH) {
        PORTB = PORTB  | _BV(pin);
    } else {
        PORTB = PORTB & ~(_BV(pin));
    }
}

state_t readPin(uint8_t pin) {
    uint8_t data = (PINB & _BV(pin));
    if (data > 0) {
        return HIGH;
    }
    return LOW;
}

void refresh() {
    address_t i;
    for (i = 0; i < 128; i++) {
        writeAddress(i);

        writePin(RAS, LOW);
        writePin(RAS, HIGH);
    }

    flags |= MEM_REFRESHED;
}

void writeData(address_t address_h, address_t address_l, state_t data) {
    writeAddress(address_h);
    writePin(DIN, data);
    writePin(RAS, LOW);
    writePin(WE, LOW);
    writeAddress(address_l);
    writePin(CAS, LOW);
    writePin(WE, HIGH);
    writePin(RAS, HIGH);
    writePin(CAS, HIGH);
}

state_t readData(address_t address_h, address_t address_l) {
    writePin(WE, HIGH);
    writeAddress(address_h);
    writePin(RAS, LOW);
    writeAddress(address_l);
    writePin(CAS, LOW);
    state_t data = readPin(DOUT);
    writePin(RAS, HIGH);
    writePin(CAS, HIGH);

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

void readAfterWrite() {
    address_t address_h;
    address_t address_l;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
            writeData(address_h, address_l, HIGH);
            state_t data = readData(address_h, address_l);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                writeData(address_h, address_l, HIGH);
                data = readData(address_h, address_l);
            }

            if (data != HIGH) {
                goto err;
            }

            writeData(address_h, address_l, LOW);
            data = readData(address_h, address_l);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                writeData(address_h, address_l, LOW);
                data = readData(address_h, address_l);
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

void writeMem(state_t state) {
    address_t address_h;
    address_t address_l;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
            writeData(address_h, address_l, state);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                writeData(address_h, address_l, state);
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

void readMem(state_t state) {
    address_t address_h;
    address_t address_l;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
            state_t data = readData(address_h, address_l);

            if ((flags & MEM_REFRESHED) == MEM_REFRESHED) {
                flags &= ~MEM_REFRESHED;

                data = readData(address_h, address_l);
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

    writeMem(HIGH);
    readMem(HIGH);
    writeMem(LOW);
    readMem(LOW);
    readAfterWrite();

    for(;;){
        if ((flags & MEM_OK) == MEM_OK) {
            writePin(LED, HIGH);
        } else {
            writePin(LED, HIGH);
            _delay_ms(100);
            writePin(LED, LOW);
            _delay_ms(100);
        }
    }

    return 0;   /* never reached */
}
