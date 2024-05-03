#define F_CPU 16000000
#define __AVR_ATtiny85__
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/sfr_defs.h>

#define BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (BAUDRATE * 16UL))) - 1)

#define CS_PIN PB0 // CS pin connected to PB2 (digital pin 10)
#define DO_PIN PB1
#define SCK_PIN PB2
#define THERMOMETER_PIN PB2 // CS pin connected to PB2 (digital pin 10)
#include "font.h"

// MAX7219 commands
#define MAX7219_NOOP        0x00

//...
//the values 1 to 8 are for drawing to the corresponding rows of the led matrix
//...
#define MAX7219_DECODEMODE  0x09
#define MAX7219_INTENSITY   0x0A
#define MAX7219_SCANLIMIT   0x0B
#define MAX7219_SHUTDOWN    0x0C
#define MAX7219_DISPLAYTEST 0x0F
volatile uint32_t brightness = 0x0f0e0d0c;

#define NUM_MATRICES 4

volatile uint8_t pulse_start_time;
volatile uint8_t pulse_duration;
volatile uint64_t bitpattern;

void SPI_MasterInit(void) {
//    USISR = (1<<USIWM0) | (1<<USICS1) | (1<<USICLK) | (1<<USITC);
    USICR = (1 << USIWM0) | (1 << USICS1);


}

void SPI_MasterTransmit(uint8_t command, uint8_t data) {
    
   //------------------------- 1
    USICR |= (1<<USITC);
    USICR |= (1<<USITC);
    USISR |= (1<<USIOIF);
    USIDR = command  ; // Start transmission by loading data into SPI data register
    
    PORTB |= (1<<PB4);
    while (!(USISR & (1 << USIOIF))){
        USICR |= (1<<USITC);
    }
  //  PORTB |= 1<<PB4;
    //------------------------- 2
    USICR |= (1<<USITC);
    USISR |=(1<<USIOIF);
    USIDR = data  ; // Start transmission by loading data into SPI data register
    
   // PORTB &= ~(1<<PB4);
    while (!(USISR & (1 << USIOIF))){
        USICR |= (1<<USITC);
    }
    PORTB &= ~(1<<PB4);
}

void MAX7219_SendCommand(uint8_t command, uint32_t data) {
    // Send the command and data
    uint8_t i ;
    // USICR |= (1<<USITC);
    PORTB &= ~(1 << CS_PIN); // Select the MAX7219 chip
    for (i = NUM_MATRICES; i ; i--) {
        //every matrix in the chain get the same command, but a different 8bit chunk from the 64bit input
        SPI_MasterTransmit(command, data>>((i-1)*8) & 0xFF ); 
    }
    PORTB |= (1 << CS_PIN); // Select the MAX7219 chip
    _delay_ms(1);
}
void spi_write(uint8_t data) {
    uint8_t i;
    for (i = 0; i < 8; ++i) {
        if (data & 0x80)
            PORTB |= (1 << DO_PIN);
        else
            PORTB &= ~(1 << DO_PIN);
        //PORTB = PORTB & ~(1 << DO_PIN) | (data & 0x80)>>(7-DO_PIN);
        
        PORTB |= (1 << SCK_PIN);
        PORTB &= ~(1 << SCK_PIN);
        data <<= 1;
    }
}

void max7219_write(uint8_t command, uint32_t data) {
    PORTB &= ~(1 << CS_PIN); // Select the MAX7219 chip
    uint8_t i ;
    for (i = NUM_MATRICES; i ; i--) {
        //every matrix in the chain get the same command, but a different 8bit chunk from the 64bit input
        spi_write(command);
        spi_write(data>>((i-1)*8) & 0xFF ); 
    }
    PORTB |= (1 << CS_PIN); // Select the MAX7219 chip
}

void max7219_Init() {
    DDRB |= (1 << DO_PIN) | (1 << SCK_PIN) | (1 << CS_PIN);
    PORTB |= (1 << CS_PIN);
    
    max7219_write(MAX7219_SCANLIMIT, 0x07070707);  // Set to scan all digits
    max7219_write(MAX7219_DECODEMODE, 0); // No decode mode
    max7219_write(MAX7219_DISPLAYTEST, 0); // Disable display test
    max7219_write(MAX7219_SHUTDOWN, 0x01010101);   // Turn on chip
    max7219_write(MAX7219_INTENSITY, 0x0f0f0f0f); // Set intensity (0-15)
}

void printtemp(uint32_t* pic, uint8_t temperature, uint8_t humidity){
    uint8_t i;
    uint8_t j;

    uint8_t temp_str[2];
    uint8_t humid_str[2];
    humidity -= (humidity == 100);
    temp_str[0] = temperature / 10 + '0';
    temp_str[1] = temperature % 10 + '0';
    
    humid_str[0] = humidity / 10 + '0';
    humid_str[1] = humidity % 10 + '0';


    // we want to pad with spaces, sprintf doesn't seem to work. 
    // '@' is the symbol for space because regular space is not in the ascii range of our font
    for (i = 0 ; i < 8; i++ ){
        pic[i] = 1ull << 14;   //draw seperating line to framebuffer
    }


    if (bitpattern & (0x1ull << 23) )pic[4] |= 0x3ull << (30 -(4*(temperature<10))); // if sign bit is set put draw minus sign on framebuffer
    
    for (j= (temp_str[0] == '0') ;j<2;j++){
        for (i = 0 ; i < 5; i++ ){
            pic[i+2] |= ((uint32_t) ascii_chars[temp_str[j]*5+i]) << (26-j*4) ; //draw string on framebuffer
        }
    }
    for (i = 0 ; i < 7; i++ ){
        pic[i+1] |= ((uint32_t) degree_celsius[i]) << 16 ; //draw string on framebuffer
    }


    for (j= (humid_str[0] == '0') ;j<2;j++){
        for (i = 0 ; i < 5; i++ ){
            pic[i+2] |= ((uint32_t) ascii_chars[humid_str[j]*5+i]) << (10-j*4) ; //draw string on framebuffer
        }
    }

    for (i = 0 ; i < 7; i++ ){
        pic[i+1] |= ((uint32_t) percentage[i]) ; //draw string on framebuffer
    }
}

ISR (INT0_vect) {
    // Rising edge detection
   if ((PINB >> THERMOMETER_PIN) & 0x01) //only measure on rising edge so we always get the duration of one pulse plus the delimiter low signal
    {
        pulse_duration = (TCNT0 - pulse_start_time) ;
        pulse_start_time = 0;
        TCNT0=0;
        bitpattern = (bitpattern << 1)  | (pulse_duration > 24); // if pulse duration is longer than 1.5k clockcycles append a 1 to bitpattern, else a 0
    }
    
}
int main(void)
{
    uint8_t i;
    uint8_t j;
    
    PORTB &= ~(1<<PB4);
    max7219_Init();

     
    GIMSK |= (1<<INT0);     // enabling the INT0 (external interrupt) 
    MCUCR |= (1<<ISC00) | (1<<ISC01);    // Configuring as rising edge 
    PORTB &= ~(1<<PB4);
    while (1) {
        TCCR0B= (1<<CS00) | (1<<CS01);
        sei();

        bitpattern=0;

        DDRB  = (1 << CS_PIN) | (1 << DO_PIN);
        PORTB |= (1 << CS_PIN);
        
        PORTB &= ~(1 << THERMOMETER_PIN);   // set pb1 low
        _delay_ms(2);
        PORTB |= (1 << THERMOMETER_PIN);    // after 2ms delay set pb1 high again to initialize data transfer

        DDRB  = (1 << CS_PIN) | (1 << DO_PIN);
        
        _delay_ms(5);
        
        DDRB  = (1 << THERMOMETER_PIN) | (1 << CS_PIN) | (1 << DO_PIN)|(1<<PB4);
        
//        PORTB &= ~(1 << CS_PIN);
        volatile uint8_t temperature = ((bitpattern >> 10)  & 0xff) *  2/ 5;
        volatile uint8_t humidity    = (((bitpattern >> 26) & 0xff))* 10/25;
        TCCR0B= 0;
        cli();
//        _delay_ms(10);
        
        uint32_t pic[8]={0,0,0,0,0,0,0,0};
        printtemp(pic,temperature,humidity);
        uint8_t i;
        max7219_write(0, 0);
        for (i = 0; i<8; i++){
            //MAX7219_SendCommand(0xf0, 0xf8f8f8f1ul+i*2); //draw framebuffer to
            max7219_write(i+1, pic[i]);
            //display
        }
        
    }
}
