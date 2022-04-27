#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro

#include<stdio.h>
#include<math.h>
#include"spinew.h"

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

// DEVCFG2 - get the sysclk. clock to 48MHz from the 8MHz fast rc internal oscillator
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

int main() {
    
    initSPI();
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
    
    //U1RXRbits.U1RXR = 0b0001; // U1RX is B6
    //RPB7Rbits.RPB7R = 0b0001; // U1TX is B7
    
    // turn on UART1 without an interrupt
    //U1MODEbits.BRGH = 0; // set baud 230400
    //U1BRG = ((48000000 / 230400) / 16) - 1;

    // 8 bit, no parity bit, and 1 stop bit (8N1 setup)
    //U1MODEbits.PDSEL = 0;
    //U1MODEbits.STSEL = 0;

    // configure TX & RX pins as output & input pins
    //U1STAbits.UTXEN = 1;
    //U1STAbits.URXEN = 1;

    // enable the uart
    //U1MODEbits.ON = 1;
    unsigned int sine;
    unsigned int tri;
    unsigned short q;
    unsigned short r;
    __builtin_enable_interrupts();

    while (1) {
        _CP0_SET_COUNT(0);
        // pick the voltage for sine and tri waves, an unsigned 8 bit number
        
        while (_CP0_GET_COUNT() < 12000000) {
            //send the sine wave by:
            // make cs low
            LATBbits.LATB3 = 0;
            sine = 127 + 127*sin(2*M_PI*2*_CP0_GET_COUNT()/12000000);
            // turn the voltage into an unsigned 16 bit number
            q = make16(1,sine);
            // send 8 bits
            spi_io(q>>8);
            // send 8 bits
            spi_io(q&0xff);
            // make cs high
            LATBbits.LATB3 = 1;
            
            LATBbits.LATB3 = 0;
            if (_CP0_GET_COUNT() <= 6000000) {
                tri = 255 *_CP0_GET_COUNT()/6000000;
            }
            else {
                tri = 255 - 255 * (_CP0_GET_COUNT()-6000000)/6000000;
            }
            // turn the voltage into an unsigned 16 bit number
            r = make16(0,tri);
            // send 8 bits
            spi_io(r>>8);
            // send 8 bits
            spi_io(r&0xff);
            LATBbits.LATB3 = 1;
        }
    }
}
void blink() {
    if (PORTBbits.RB4 == 0){
        _CP0_SET_COUNT(0); 
        while (_CP0_GET_COUNT() < 12000000){
            LATAbits.LATA4 = 1;
        }
        while ((_CP0_GET_COUNT() < 24000000) && (_CP0_GET_COUNT() > 12000000)){
            LATAbits.LATA4 = 0;
        }
        while ((_CP0_GET_COUNT() < 36000000) && (_CP0_GET_COUNT() > 24000000)){
            LATAbits.LATA4 = 1;
        }
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

/*void ReadUART1(char * message, int maxLength) {
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
}*/