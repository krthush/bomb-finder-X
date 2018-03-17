#include <xc.h>
#include "dc_motor.h"
#include "RFID_Reader.h"
#include "Movement.h"
#include "IR_Reading.h"
//#include "LCD.h"

#pragma config OSC = IRCIO  // internal oscillator

#define PWMcycle 1 //need to calculate this

volatile unsigned char ReceivedString[16]; //Global variable to read from RFID
volatile unsigned char i=0;
volatile unsigned char RFID_Read=0;
volatile unsigned char start=0;

// High priority interrupt routine
void interrupt low_priority InterruptHandlerLow ()
{
    if (PIR1bits.RCIF) {
        ReceivedString[i]=RCREG;
        RFID_Read=1;
        if (i==15){
            i=0;
        }else{
            i++;  
        }
        PIR1bits.RCIF=0; // clear the flag
    }
}

// Low priority interrupt routine for button 
// Switches between inert mode vs. start
void interrupt InterruptHandlerHigh () {
    if (INTCONbits.INT0IF) {
        start=1;
        INTCONbits.INT0IF=0; // clear the flag
    }
}

void main(void){
    
    //Initialise Variables
    unsigned char Message[10]; //Code on RFID Card
    unsigned char i=0; //Counter variable
    signed char mode=0; //Robot mode - see switch case tree in main loop
    signed char DirectionFound=0; // Flag for if the robot has decided it knows where the bomb is
    char MoveTime[100]; //Array to store time spent on each type of movement
    char MoveType[100]; //Array to store movement types - 0 is forwards, 1 is left/right
    char Move=0; //Move counter
    // USERVARIABLE TOLERANCES
    unsigned char ScanAngle=6; // PLEASE NOTE: has to be even, units - tenth seconds
    
    // Enable general interrupt stuff
    INTCONbits.GIEH=1; // Global Interrupt Enable bit
    INTCONbits.GIEL=1; // Peripheral/Low priority Interrupt Enable bit
    INTCONbits.PEIE=1; // Enable Peripheral  interrupts
    RCONbits.IPEN=1; // Enable interrupt priority
    
    // Interrupt registers for EUSART
    IPR1bits.RCIP=0; // Low Priority 
    PIE1bits.RCIE=1; // Enable interrupt on serial reception
    
    // Interrupt registers for button
    TRISCbits.RC3=1; //set RC3 pin to be an input pin to recognise the button press
    INTCONbits.INT0IE=1; // Enables the INT0 external interrupt
    
    // Clear interrupt flags
    PIR1bits.RC1IF=0;// clear EUSART interrupt flag
    INTCONbits.INT0IF=0;// clear flag on the button interrupt
    
    // Initialise Motor Structures
    struct DC_motor mL, mR; //declare 2 motor structures
    mL.power=0; //zero power to start
    mL.direction=1; //set default motor direction
    mL.dutyLowByte=(unsigned char *)(&PDC0L); //store address of PWM duty low byte
    mL.dutyHighByte=(unsigned char *)(&PDC0H); //store address of PWM duty high byte
    mL.dir_pin=0; //pin RB0/PWM0 controls direction
    mL.PWMperiod=199; //store PWMperiod for motor
    //same for motorR but different PWM registers and direction pin
    mR.power=0; //zero power to start
    mR.direction=1; //set default motor direction
    mR.dutyLowByte=(unsigned char *)(&PDC1L); //store address of PWM duty low byte
    mR.dutyHighByte=(unsigned char *)(&PDC1H); //store address of PWM duty high byte
    mR.dir_pin=2; //pin RB0/PWM0 controls direction
    mR.PWMperiod=199; //store PWMperiod for motor

    OSCCON = 0x72; //8MHz clock
    while(!OSCCONbits.IOFS); //wait until stable
    
   while(1){
       
       switch (mode) {
           case -1: //Inert Mode
               // Robot has finished start-up, and now ready to go!
               // Robot also enters this mode after successfully finishing the task.
               // If button is pressed while robot is performing, it will return to inert mode.
               // If button is pressed while robot is in inert mode, it will start performing.
               stop(&mL, &mR);
               
               break;
               
           case 0 : //Start-up Mode
                //Initialise EVERYTHING
                initMotorPWM();  //setup PWM registers
                initRFID();
                initLCD();
                initIR();              
               
                // Bot goes forward, stops, then back and stop
                // TODO: do calibration routine here
                fullSpeedAhead(&mL, &mR);
                delay_s(1);
                stop(&mL, &mR);
                fullSpeedBack(&mL, &mR);
                delay_s(1);
                stop(&mL, &mR);
              
                enableSensor(0, 1); // DEBUG ONLY - enable sensors to test signals:
                enableSensor(1, 1); // DEBUG ONLY - enable sensors to test signals:
                mode = -1;  //TODO: Make mode change on button press
               
                break;
               
           case 1 : //Search Mode
                if (DirectionFound==-1) {
                    // Robot is completely lost, move a bit a hope to find it.
                    // PLEASE NOTE: this movement in combination with the
                    // rotation in ScanWithRange causes the robot to spiral 
                    // outwards such that it will ALWAYS get close enough to signal
                    fullSpeedAhead(&mL, &mR);
                    delay_tenth_s(ScanAngle);
                    stop(&mL,&mR);
                    DirectionFound=0;
                    MoveType[Move]=0;
                } else if (DirectionFound==0) {
                    // Scans a wide range if it's unsure about direction
                    DirectionFound = ScanWithRange(&mL, &mR, ScanAngle, &MoveTime[Move]); // USERVARIABLE
                    MoveType[Move] = 1;
                } else if (DirectionFound==1) {
                     // Keeps direction and just scans, robot thinks it's close
                    DirectionFound = ScanIR(&mL, &mR, &Move, &MoveTime, &MoveType); // USERVARIABLE
                    
                } else if (DirectionFound==2) {
                     // Robot thinks its on track, switch to move mode
                     mode=2;
                     MoveType[Move] = 1;
                }
                
                Move++;
               
                break;
               
            case 2 : // Go forward mode
               // Move forward until RFID read and verified or a certain time
               // has elapsed

                if (RFID_Read) {
                    stop(&mL, &mR);
                    if (ReceivedString[0]==0x02 & ReceivedString[15]==0x03){ //If we have a valid ASCII signal
                        if (VerifySignal(ReceivedString)){ //and if the checksum is correct
                            //Put the RFID data into the Message variable
                            for (i=0; i<10; i++){
                                Message[i] = ReceivedString[i+1]; 
                            }
//                             LCDString(Message); //Display code on LCD
                            //Clear the received string 
                            for (i=0; i<16; i++) {
                                ReceivedString[i]=0;
                            }
                            mode = 3; //Return to home!

                        } else { //If the signal doesn't check out
                            fullSpeedBack(&mL,&mR); //Go back a bit then stop
                            delay_tenth_s(5);
                            stop(&mL,&mR);
                            fullSpeedAhead(&mL,&mR); //Try again
                        }  
                    }
                } else {
                    DirectionFound=1;
                    mode=1;
                    fullSpeedAhead(&mL,&mR);
                    delay_tenth_s(5);
                    MoveType[Move] = 0;
                    MoveTime[Move] = 5;
                    Move++;
                }
                
                break;
               
            case 3 : //Return Mode
                //Return to original position using MoveType and MoveTime
                stop(&mL,&mR);
//                for (Move=Move; Move>0; Move--) { //Go backwards along the moves
//                    if (MoveType[Move]==0) { //If move was forwards
//                        fullSpeedBack(&mL,&mR);
//                        delay_tenth_s(MoveTime[Move]);
//                    } else if (MoveType[Move]==1) { //If move was left/right
//                        if (MoveTime[Move]>0) { //If left turn
//                            turnRight(&mL,&mR);
//                            delay_tenth_s(MoveTime[Move]);
//                        } else {
//                            turnLeft(&mL,&mR);
//                            delay_tenth_s(MoveTime[Move]);
//                        }
//                    }
//                }
                break;
       }      
   }
}
