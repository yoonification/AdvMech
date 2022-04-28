#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro

#include<stdio.h>
#include<math.h>
#include "i2c_master_noint.h"

// DEVCFG0
#pragma config DEBUG = OFF // disable debugging
#pragma config JTAGEN = OFF // disable jtag
#pragma config ICESEL = ICS_PGx1 // use PGED1 and PGEC1
#pragma config PWP = OFF // disable flash write protect
#pragma config BWP = OFF // disable boot write protect
#pragma config CP = OFF // disable code protect

// DEVCFG1
#pragma config FNOSC = FRCPLL // use fast frc oscillator with pll
#pragma config FSOSCEN = OFF // disable secondary oscillator
#pragma config IESO = OFF // disable switching clocks
#pragma config POSCMOD = OFF // primary osc disabled
#pragma config OSCIOFNC = OFF // disable clock output
#pragma config FPBDIV = DIV_1 // divide sysclk freq by 1 for peripheral bus clock
#pragma config FCKSM = CSDCMD // disable clock switch and FSCM
#pragma config WDTPS = PS1048576 // use largest wdt value
#pragma config WINDIS = OFF // use non-window mode wdt
#pragma config FWDTEN = OFF // wdt disabled
#pragma config FWDTWINSZ = WINSZ_25 // wdt window at 25%

// DEVCFG2 - get the sysclk clock to 48MHz from the 8MHz fast rc internal oscillator
#pragma config FPLLIDIV = DIV_2 // divide input clock to be in range 4-5MHz
#pragma config FPLLMUL = MUL_24 // multiply clock after FPLLIDIV
#pragma config FPLLODIV = DIV_4 // divide clock after FPLLMUL to get 48MHz

// DEVCFG3
#pragma config USERID = 0x1234 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY = OFF // allow multiple reconfigurations

void blink();
void ReadUART1(char * string, int maxLength);
void WriteUART1(const char * string);
unsigned short make16(char a, unsigned char v);
void setPin(unsigned char address, unsigned char reg, unsigned char value);
unsigned char readPin(unsigned char w, unsigned char r, unsigned char reg);


int main() {
    
    i2c_master_setup();
    
    unsigned int W_add = 0b01000000;
    unsigned int R_add = 0b01000001;
    unsigned char IODIR = 0b01111111; // value to send to IODIR for IO pin settings
    __builtin_disable_interrupts(); // disable interrupts while initializing things
    
    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);

    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;

    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;

    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;

    // do your TRIS and LAT commands here
    TRISAbits.TRISA4 = 0;
    LATAbits.LATA4 = 0;
    TRISBbits.TRISB4 = 1;
    
    // B6 and B7 are UART Pins
    
    U1RXRbits.U1RXR = 0b0001; // U1RX is B6
    RPB7Rbits.RPB7R = 0b0001; // U1TX is B7
    
    // turn on UART1 without an interrupt
    U1MODEbits.BRGH = 0; // set baud 230400
    U1BRG = ((48000000 / 230400) / 16) - 1;

    // 8 bit, no parity bit, and 1 stop bit (8N1 setup)
    U1MODEbits.PDSEL = 0;
    U1MODEbits.STSEL = 0;

    // configure TX & RX pins as output & input pins
    U1STAbits.UTXEN = 1;
    U1STAbits.URXEN = 1;

    // enable the uart
    U1MODEbits.ON = 1;
    __builtin_enable_interrupts();

    while (1) {
        // Write to IODIR
        
        setPin(W_add,0x00,IODIR);
        // Write to OLAT
        setPin(W_add,0x0A,0);
        
        while (readPin(W_add,R_add,0x09) == 0) {
            setPin(W_add,0x0A,0b10000000);
        }
        blink();
    }
}
void blink() {
    _CP0_SET_COUNT(0); 
    while (_CP0_GET_COUNT() < 1000000){
        LATAbits.LATA4 = 1;
    }
    while ((_CP0_GET_COUNT() < 2000000) && (_CP0_GET_COUNT() > 1000000)){
        LATAbits.LATA4 = 0;
    }
}

unsigned short make16(char a_or_b, unsigned char v) {
    unsigned short s = 0;
    s = (a_or_b << 15);
    s |= (0b111 << 12);
    s |= (v << 4);
    return s;
}

void ReadUART1(char * message, int maxLength) {
  char data = 0;
  int complete = 0, num_bytes = 0;
  // loop until you get a '\r' or '\n'
  while (!complete) {
    if (U1STAbits.URXDA) { // if data is available
      data = U1RXREG;      // read the data
      if ((data == '\n') || (data == '\r')) {
        complete = 1;
      } else {
        message[num_bytes] = data;
        ++num_bytes;
        // roll over if the array is too small
        if (num_bytes >= maxLength) {
          num_bytes = 0;
        }
      }
    }
  }
  // end the string
  message[num_bytes] = '\0';
}

// Write a character array using UART3
void WriteUART1(const char * string) {
  while (*string != '\0') {
    while (U1STAbits.UTXBF) {
      ; // wait until tx buffer isn't full
    }
    U1TXREG = *string;
    ++string;
  }
}

void setPin(unsigned char address, unsigned char reg, unsigned char value){
    i2c_master_start();
    i2c_master_send(address);
    i2c_master_send(reg);
    i2c_master_send(value);
    i2c_master_stop();
}

unsigned char readPin(unsigned char w, unsigned char r, unsigned char reg){
    i2c_master_start();
    i2c_master_send(w);
    i2c_master_send(reg);
    i2c_master_restart();
    i2c_master_send(r);
    unsigned char result = i2c_master_recv() & 0xff;
    i2c_master_ack(1);
    i2c_master_stop();
    return result;
    
}