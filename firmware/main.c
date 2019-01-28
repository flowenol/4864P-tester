/* Name: main.c
 * Author: flovenol@gmail.com
 * License: Apache License ver 2.0
 */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define RAS PD0
#define CAS PD1
#define WE PD2
#define DOUT PD3
#define DIN PD4
#define LED PD5

#define address_t uint8_t

typedef enum {HIGH, LOW} state_t;

#define MEM_OK 0x01
#define MEM_REFRESHED 0x02

uint8_t flags = 0x00;

void writeAddress(address_t address) {
    PORTD = address;
}

void writePin(uint8_t pin, state_t data) {
    if (data == HIGH) {
        PORTD = PORTD  | (1 << pin);
    } else {
        PORTD = PORTD & ~(1 << pin);
    }
}

state_t readPin(uint8_t pin) {
    uint8_t data = (PIND & (1 << pin));
    if (data > 0) {
        return HIGH;
    }
    return LOW;
}

void refresh() {
    address_t i;
    for (i = 0; i < 128; i++) {
        writeAddress(i);
        
        _delay_us(1.0);
        writePin(RAS, LOW);
        _delay_us(1.0);
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
    DDRB |= 1 << RAS;
    DDRB |= 1 << CAS;
    DDRB |= 1 << WE;
    DDRB &= ~(1 << DOUT);
    DDRB |= 1 << DIN;
    DDRB |= 1 << LED;
    
    _delay_us(500.0);
    
    uint8_t i;
    for (i = 0; i < 8; i++) {
        refresh();
        _delay_ms(2);
    }
    
    //65536 - 30 = 65506
    TCNT1 = 65506;   // for 1.8 ms at 16 MHz
    TCCR1A = 0x00;
    TCCR1B = (1<<CS10) | (1<<CS12);  // Timer mode with 1024 prescler
    TIMSK1 = (1 << TOIE1); // Enable timer1 overflow interrupt(TOIE1)
    sei();        // Enable global interrupts by setting global interrupt enable bit in SREG
}

ISR (TIMER1_OVF_vect)
{
    refresh();
    TCNT1 = 65506;   // for 1.8 ms at 16 MHz
}

void checkMem() {
    address_t address_h;
    address_t address_l ;
    for (address_h = 0x00; address_h <= 0xFF; address_h += 0x01) {
        for (address_l = 0x00; address_l <= 0xFF; address_l += 0x01) {
        write_h:
            writeData(address_h, address_l, HIGH);
            state_t data = readData(address_h, address_l);
          
            if ((flags & MEM_REFRESHED) > 0) {
                flags &= ~MEM_REFRESHED;
                goto write_h;
            }
            
            if (data != HIGH) {
                goto end;
            }
            
        write_l:
            writeData(address_h, address_l, LOW);
            data = readData(address_h, address_l);
            
            if ((flags & MEM_REFRESHED) > 0) {
                flags |= ~MEM_REFRESHED;
                goto write_l;
            }
            
            if (data != LOW) {
                goto end;
            }
        }
    }
    
    flags |= MEM_OK;
end:
    return;
}


int main(void)
{
    setup();
    
    checkMem();
    
    for(;;){
        if ((flags & MEM_OK) > 0) {
            writePin(LED, HIGH);
        } else {
            writePin(LED, HIGH);
            _delay_us(100);
            writePin(LED, LOW);
            _delay_us(100);
        }
    }

    return 0;   /* never reached */
}
