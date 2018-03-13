#include "RFID_Reader.h"
#include <xc.h>
#define _XTAL_FREQ 8000000

void initRFID(void){
    TRISC = TRISC || 0b11000000; //set data direction registers
                        //both need to be 1 even though RC6
                        //is an output, check the datasheet!

        
    SPBRG=205; //set baud rate to 9600
    SPBRGH=0;
    BAUDCONbits.BRG16=1; //set baud rate scaling to 16 bit mode
    TXSTAbits.BRGH=1; //high baud rate select bit
    RCSTAbits.CREN=1; //continous receive mode
    RCSTAbits.SPEN=1; //enable serial port, other settings default
    TXSTAbits.TXEN=1; //enable transmitter, other settings default
    TXSTAbits.SYNC=0; //Asynchronous
    RCSTAbits.RX9=0; //8-bit reception
    
    
}

void sendCharToSerial(char charToSend){
 while (!PIR1bits.TXIF); // wait for flag to be set
 TXREG=charToSend; //transfer char H to transmitter
}

char getCharSerial(void){
while (!PIR1bits.RCIF); //wait for the data to arrive
    return RCREG; //return byte in RCREG
}

void Serial_String(char *string){
    //While the data pointed to isn't a 0x00 do below
    while(*string != 0){
    //Send out the current byte pointed to
    // and increment the pointer
    sendCharToSerial(*string++);
    __delay_us(50); //so we can see each character
    //being printed in turn (remove delay if you want
    //your message to appear almost instantly)
    }
}

unsigned char VerifySignal(unsigned char *Signal){
    unsigned char checksum=0;
    unsigned int hexByte=0;

    //First run through - XOR first two hex bytes
    hexByte = (Signal[3]<<8) + Signal[4];
    checksum = ((Signal[1]<<8) + Signal[2]) ^ hexByte; //First 2 chars XOR second 2

    //Loop through, XORing the previous result with the next hex byte in turn
    for (i=5; i<10; i+=2){
        hexByte = (Signal[i]<<8) + Signal[i+1];
        checksum = checksum ^ hexByte;
    }

    if ((checksum[0]==Signal[11]) & (checksum[1]==Signal[12])){
        return 1;
    } else{
        return 0;
    }
}