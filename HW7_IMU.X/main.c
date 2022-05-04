#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro

#include <stdio.h>

#include <math.h>

#include "i2c_master_noint.h"
#include "mpu6050.h"

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
#pragma config FPLLODIV = DIV_2 // divide clock after FPLLMUL to get 48MHz

// DEVCFG3
#pragma config USERID = 0 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY = OFF // allow multiple reconfigurations

#define PIC32_SYS_FREQ 48000000ul // 48 million Hz
#define PIC32_DESIRED_BAUD 115200 // Baudrate for RS232

#define DT 0.01
#define A 0.8



void UART1_Startup(void);
void ReadUART1(char * string, int maxLength);
void WriteUART1(const char * string);

void comp_filt(int i, uint8_t* IMU_buf, float* pitch, float* roll, float* ax, float* ay, float* az, float* gx, float* gy);

void blink();

void delay();

int main() {

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
    TRISBbits.TRISB4 = 1;
    LATAbits.LATA4 = 0;
    
    TRISAbits.TRISA4 = 0;
    TRISBbits.TRISB4 = 1;
    
    U1RXRbits.U1RXR = 0b0001; // U1RX is B6
    RPB7Rbits.RPB7R = 0b0001; // U1TX is B7
    
    UART1_Startup();
    
    __builtin_enable_interrupts();
    
    // init the imu
    init_mpu6050();
    
    char m_in[100]; // char array for uart data coming in
    char m_out[200]; // char array for uart data going out
    int i;
    #define NUM_DATA_PNTS 300 // how many data points to collect at 100Hz
    float ax[NUM_DATA_PNTS], ay[NUM_DATA_PNTS], az[NUM_DATA_PNTS], gx[NUM_DATA_PNTS], gy[NUM_DATA_PNTS], gz[NUM_DATA_PNTS], temp[NUM_DATA_PNTS], pitch[NUM_DATA_PNTS], roll[NUM_DATA_PNTS];
    
    pitch[0] = 0.0;
    roll[0] = 0.0;
    //sprintf(m_out,"MPU-6050 WHO_AM_I: %X\r\n",whoami());
    //WriteUART1(m_out);
    char who = whoami(); // ask if the imu is there
    if (who != 0x68){
        // if the imu is not there, get stuck here forever
        while(1){
            LATAbits.LATA4 = 1;
        }
    }
    
    char IMU_buf[IMU_ARRAY_LEN]; // raw 8 bit array for imu data

    while (1) {
        blink();
        
        ReadUART1(m_in,100); // wait for a newline
        // don't actually have to use what is in m
        
        // collect data
        for (i=0; i<NUM_DATA_PNTS; i++){
            _CP0_SET_COUNT(0);
            // read IMU
            blink();
            burst_read_mpu6050(IMU_buf);
            
            comp_filt(i, IMU_buf, pitch, roll, ax, ay, az, gx, gy);
            //sprintf(m_out,"%d %f %f\r\n",NUM_DATA_PNTS-i,pitch[i],roll[i]);
            //WriteUART1(m_out);
            
            while(_CP0_GET_COUNT()<24000000/2/100){}
        }
        // print data
        for (i=1; i<NUM_DATA_PNTS; i++){
            sprintf(m_out,"%d %f %f\r\n",NUM_DATA_PNTS-i,pitch[i],roll[i]);
            WriteUART1(m_out);
        }
        
    }
}

// Read from UART1
// block other functions until you get a '\r' or '\n'
// send the pointer to your char array and the number of elements in the array
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

// Write a character array using UART1
void WriteUART1(const char * string) {
  while (*string != '\0') {
    while (U1STAbits.UTXBF) {
      ; // wait until tx buffer isn't full
    }
    U1TXREG = *string;
    ++string;
  }
}


void UART1_Startup() {
  // disable interrupts
  __builtin_disable_interrupts();

  // turn on UART1 without an interrupt
  U1MODEbits.BRGH = 0; // set baud to PIC32_DESIRED_BAUD
  U1BRG = ((PIC32_SYS_FREQ / PIC32_DESIRED_BAUD) / 16) - 1;

  // 8 bit, no parity bit, and 1 stop bit (8N1 setup)
  U1MODEbits.PDSEL = 0;
  U1MODEbits.STSEL = 0;

  // configure TX & RX pins as output & input pins
  U1STAbits.UTXEN = 1;
  U1STAbits.URXEN = 1;

  // enable the uart
  U1MODEbits.ON = 1;

  __builtin_enable_interrupts();
}

void comp_filt(int i, uint8_t* IMU_buf, float* pitch, float* roll, float* ax, float* ay, float* az, float* gx, float* gy) {
    ax[i] = conv_xXL(IMU_buf);
    ay[i] = conv_yXL(IMU_buf);
    az[i] = conv_zXL(IMU_buf);
    gx[i] = conv_xG(IMU_buf);
    gy[i] = conv_yG(IMU_buf);
    
    float theta_xl = (180.0/M_PI)*atan2f(ax[i],az[i]);
    float phi_xl = (180.0/M_PI)*atan2f(ay[i],az[i]);
   
    pitch[i] += gx[i] * DT;
    pitch[i] = A * theta_xl + (1-A) * pitch[i];
    
    roll[i] += gy[i] * DT;
    roll[i] = A * phi_xl + (1-A) * roll[i];
}

void blink(){
    LATAbits.LATA4 = 1;
    _CP0_SET_COUNT(0);
    while(_CP0_GET_COUNT()<24000000/2/20){}
    LATAbits.LATA4 = 0;
    _CP0_SET_COUNT(0);
    while(_CP0_GET_COUNT()<24000000/2/20){}
}
