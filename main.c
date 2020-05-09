#include <p18cxxx.h>
#include <delays.h>
#include <stdlib.h>

#pragma config FOSC=INTIO67, PLLCFG=OFF, PRICLKEN=ON,FCMEN=ON, PWRTEN=OFF
#pragma config BOREN=SBORDIS, BORV=250, WDTEN=OFF, WDTPS=4096, PBADEN=OFF
#pragma config HFOFST=OFF, MCLRE=EXTMCLR, STVREN=ON, LVP=OFF, DEBUG= ON

#define FORWARD 16
#define REVERSE 17
#define RIGHT   18
#define LEFT    19
#define FLIP    96
#define DROP    99
#define IRON    23
#define IROFF   54
#define STOP    9
#define SPEED1  0
#define SPEED2  1
#define SPEED3  2
#define SPEED4  3
#define SPEED5  4
#define SPEED6  5
#define SPEED7  6
#define SPEED8  7
#define FAST    14
#define SLOW    63

unsigned char device=0, command=0, value, i, ISRComplete, counter, sensor=0;

unsigned int result;
unsigned char buffer[12];

void DelayFor18TCY( void )
{
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
Nop();
}
void DelayPORXLCD (void)
{
Delay1KTCYx(60); // Delay of 15ms
// Cycles = (TimeDelay * Fosc) / 4
// Cycles = (15ms * 16MHz) / 4
// Cycles = 60,000
return;
}
void DelayXLCD (void)
{
Delay1KTCYx(20); // Delay of 5ms
// Cycles = (TimeDelay * Fosc) / 4
// Cycles = (5ms * 16MHz) / 4
// Cycles = 20,000
return;
}

void low_ISR(void);
void high_ISR(void);
#pragma code high_vector = 0x08	// force the following statement to start at
void high_interrupt (void)		// 0x08
{
    _asm 
    goto  high_ISR
    _endasm 
}
#pragma code 			//return to the default code section
#pragma interrupt high_ISR 

void high_ISR (void)
{
    if(PORTBbits.RB4 == 0) {
        TMR0L = 0x00;
    }              
    else {
        value = TMR0L;
        if(value > 150) {
            i = 0;
            ISRComplete = 0;
        } else {
            buffer[i] = value;
            i++;
        }
    }
    if(i > 11) {
        ISRComplete = 1;
    }
    INTCONbits.RBIF = 0;
}

// Max PWM 255
void speed(int s){
    CCPR5L = s*31;
}

// Speed Up
void fast(){
    if(CCPR5L <= 224){
        CCPR5L = CCPR5L + 31;
    }
}

// Slow Down
void slow(){
    if(CCPR5L >= 31){
        CCPR5L = CCPR5L - 31;
    }
}

void flip(){
    // Right
    for(i = 32; i < 90; i++){
        CCP3CON = (CCP3CON & 0xCF) | ((i & 0x03) << 4);
        CCPR3L = i >> 2;
    }
    
    // Left
    for(i = 32; i < 90; i++){
        CCP4CON = (CCP4CON & 0xCF) | ((i & 0x03) << 4);
        CCPR4L = i >> 2;
    }
}

void drop(){
    CCPR4L = 0;
    for(i = 32; i < 180; i++){
        CCP3CON = (CCP3CON & 0xCF) | ((i & 0x03) << 4);
        CCPR3L = i >> 2;
    }
}

void main( void )
{
    // Configure the Oscillator for 1Mhz internal oscillator operation
	OSCCON2 = 0x00; // No 4X PLL
	OSCCON = 0x30;  // 1MHZ
    OSCTUNEbits.PLLEN = 0;
	
    // PORT A - NC
	TRISA = 0x00;		// Outputs
	ANSELA = 0x00;		// Digital
	
    // PORT B - Pickit 3, IR Sensors
	TRISB = 0xDF;		// Inputs, Except For CCP3 on RB5
	ANSELB = 0x00;		// Digital
	
    // PORT C - Servo PWM
	TRISC = 0x00;		// Outputs
	ANSELC = 0x00;		// Digital
    
    // PORT D - DC Drive Motors
	TRISD = 0x00;		// Outputs
	ANSELD = 0x00;		// Digital
    
    // PORT E - Motor Driver PWM
    ANSELE = 0x00;       // Outputs
    TRISE = 0x00;        // Digital

    // Set up the Timer 0 for 8 bit operation with Input from internal clock divided by 4
	T0CON = 0xD1;  // Divide by 4
    
    // Setup IR Interrupts
    RCONbits.IPEN = 1;      // Enable priority levels on interrupts
    INTCONbits.GIEH = 1;    // Enable all unmasked interrupts
    INTCONbits.GIEL = 0;    // Disable all low priority interrupts
    INTCONbits.RBIE = 1;    // Enable the IOCx port change interrupt
    INTCON2bits.RBIP = 1;   // High Priority
    IOCB = 0x10;            // Enable interrupts on pin RB4
    
    // Clear interrupt flag
    INTCONbits.RBIF = 0;
    
    // Configure PWM
    CCPTMRS1 = 0x00;
    PR2 = 0xFF;
    T2CON = 0b00000110;
    CCP3CON = 0b00001100;
    CCP4CON = 0b00001100;
    CCP5CON = 0b00001100;
    
    while(1) {
        if(!PORTBbits.RB3 && sensor){
            flip();
            Delay1KTCYx(10);
        }
            
        // Process IR Remote Interrupt
        if(ISRComplete) {
           result = 0;
           for(i = 0; i < 12; i++)
           {
               result = result >> 1;
               if(buffer[i] > 70) {
                   result = result | 0x0800;
               }
           }

            device = result >> 7;               // Device is Upper 5 Bits
            command = (result & 0x7f);          // Mask off lower 7 bits to determine command
            
            // Process Remote Commands
            switch(command) {
                case FORWARD:
                    PORTC = 0xA0;
                    break;

                case REVERSE:
                    PORTC = 0x50;
                    break;

                case LEFT:
                    PORTC = 0x60;
                    break;

                case RIGHT:
                    PORTC = 0x90;
                    break;

                case FAST:
                    fast();
                    break;
                    
                case SLOW:
                    slow();
                    break;
                    
                case STOP:
                    speed(0);
                    break;
                    
                case SPEED1:
                    speed(1);
                    break;
                
                case SPEED2:
                    speed(2);
                    break;
                 
                case SPEED3:
                    speed(3);
                    break;
                    
                case SPEED4:
                    speed(4);
                    break;
                    
                case SPEED5:
                    speed(5);
                    break;
                    
                case SPEED6:
                    speed(6);
                    break;
                    
                case SPEED7:
                    speed(7);
                    break;
                 
                case SPEED8:
                    speed(8);
                    break;
                    
                case FLIP:
                    flip();
                    break;
                    
                case DROP:
                    drop();
                    break;
                    
                case IRON:
                    sensor = 1;
                    break;
                    
                case IROFF:
                    sensor = 0;

                default:
                    break;
            }
            ISRComplete = 0;
        }
        Delay10KTCYx(5);
    }
}