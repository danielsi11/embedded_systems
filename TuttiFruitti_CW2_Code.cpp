#include "mbed.h"
#include "math.h"
#include "SHA256.h"
#include <string>

//Photointerrupter input pins
#define I1pin D3
#define I2pin D6
#define I3pin D5

//Incremental encoder input pins
#define CHApin   D12
#define CHBpin   D11

//Motor Drive output pins   //Mask in output byte
#define L1Lpin D1           //0x01
#define L1Hpin A3           //0x02
#define L2Lpin D0           //0x04
#define L2Hpin A6          //0x08
#define L3Lpin D10           //0x10
#define L3Hpin D2          //0x20

#define PWMpin D9

//Motor current sense
#define MCSPpin   A1
#define MCSNpin   A0

//Mapping from sequential drive states to motor phase outputs
/*
State   L1  L2  L3
0       H   -   L
1       -   H   L
2       L   H   -
3       L   -   H
4       -   L   H
5       H   L   -
6       -   -   -
7       -   -   -
*/
//Drive state to output table
const int8_t driveTable[] = {0x12,0x18,0x09,0x21,0x24,0x06,0x00,0x00};

//Mapping from interrupter inputs to sequential rotor states. 0x00 and 0x07 are not valid
const int8_t stateMap[] = {0x07,0x05,0x03,0x04,0x01,0x00,0x02,0x07};  
//const int8_t stateMap[] = {0x07,0x01,0x03,0x02,0x05,0x00,0x04,0x07}; //Alternative if phase order of input or drive is reversed

//Phase lead to make motor spin
int8_t lead = 2;  //2 for forwards, -2 for backwards

//Status LED
//DigitalOut led1(LED1);

//Photointerrupter inputs
InterruptIn I1(I1pin);
InterruptIn I2(I2pin);
InterruptIn I3(I3pin);

//Motor Drive outputs
DigitalOut L1L(L1Lpin);
DigitalOut L1H(L1Hpin);
DigitalOut L2L(L2Lpin);
DigitalOut L2H(L2Hpin);
DigitalOut L3L(L3Lpin);
DigitalOut L3H(L3Hpin);

//Pwm output
PwmOut pwm(PWMpin);

//Initialise the serial port
RawSerial pc(USBTX, USBRX);

//Set a given drive state
void motorOut(int8_t driveState){
    
    //Lookup the output byte from the drive state.
    int8_t driveOut = driveTable[driveState & 0x07];
      
    //Turn off first
    if (~driveOut & 0x01) L1L = 0;
    if (~driveOut & 0x02) L1H = 1;
    if (~driveOut & 0x04) L2L = 0;
    if (~driveOut & 0x08) L2H = 1;
    if (~driveOut & 0x10) L3L = 0;
    if (~driveOut & 0x20) L3H = 1;
    
    //Then turn on
    if (driveOut & 0x01) L1L = 1;
    if (driveOut & 0x02) L1H = 0;
    if (driveOut & 0x04) L2L = 1;
    if (driveOut & 0x08) L2H = 0;
    if (driveOut & 0x10) L3L = 1;
    if (driveOut & 0x20) L3H = 0;
    }
    
    //Convert photointerrupter inputs to a rotor state
inline int8_t readRotorState(){
    return stateMap[I1 + 2*I2 + 4*I3];
    }

//Basic synchronisation routine    
int8_t motorHome() {
    //Put the motor in drive state 0 and wait for it to stabilise
    motorOut(0);
    wait(2.0);
    
    //Get the rotor state
    return readRotorState();
}
    
// Rotor offset at motor state 0
int8_t orState = 0; 

//**************************** THREADS *****************************//
Thread thread_print;
Thread decode_com;
Thread motorCtrlT(osPriorityNormal, 1024);
Thread tune;

//**************************** GLOBAL VARIABLE *********************//
int8_t oldState = 0;    
int state_counter = 0;
Ticker motorCtrlTicker;
Timer t1, t2;
uint8_t receivedKey[8] = {0};
uint64_t newKey;
float s_max = 0;
float r_input = 0;        
int testcount = 0;      //************************NOT NEEDED******************************//
float target;
float static_position;
float integral = 0;
float period = 0.002;
float tune_period[17] = {0};
float total_t = 1000;
int duration[17] = {0};


typedef struct{
    int code;
    // Integer code definition
    // 1 : match nonce is found
    // 2 : report position and velocity

    uint64_t* match_nonce;
    float position;
    float velocity;
    
} mail_t;

Mail<mail_t, 16> mail_box;
Queue<void, 8> inCharQ; 
Mutex newKey_mutex;

//**************************** FUNCTION DECLARATIONS **************//
//Interrupt Service Routine
void check_state();
void serialISR();

//Thread
void print_output();
void decode();
void motorCtrlFn();
void playTune();

//Ticker
void motorCtrlTick();

//Function
void send_nonce(uint64_t* nonce);
void push_condition(float input_position, float input_velocity);

//Main
int main() {
    
    uint8_t sequence[] = {0x45,0x6D,0x62,0x65,0x64,0x64,0x65,0x64,
        0x20,0x53,0x79,0x73,0x74,0x65,0x6D,0x73,
        0x20,0x61,0x72,0x65,0x20,0x66,0x75,0x6E,
        0x20,0x61,0x6E,0x64,0x20,0x64,0x6F,0x20,
        0x61,0x77,0x65,0x73,0x6F,0x6D,0x65,0x20,
        0x74,0x68,0x69,0x6E,0x67,0x73,0x21,0x20,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint64_t* key = (uint64_t*)((int)sequence + 48);
    uint64_t* nonce = (uint64_t*)((int)sequence + 56);
    uint8_t hash[32];
    
    pc.printf("Hello\n\r");
    
    decode_com.start(decode);
    thread_print.start(print_output);
    
    //initialise pwm pin
    pwm.period(period);      // 2 ms period
    pwm.write(1.0f);      // 100% duty cycle, relative to period
    
    //Run the motor synchronisation
    orState = motorHome();
    oldState = orState;
    pc.printf("Rotor origin: %x\n\r",orState);    
    
    //** interrupts for motor states **//
    I1.rise(&check_state); 
    I2.rise(&check_state);
    I3.rise(&check_state);
    I1.fall(&check_state);
    I2.fall(&check_state);
    I3.fall(&check_state);  
    
    t2.start();
    motorCtrlT.start(motorCtrlFn); // Start thread for motor control
    tune.start(playTune); // Start thread for playing tune
    
    
    while(1){
               
        newKey_mutex.lock();
        *key = newKey;
        newKey_mutex.unlock();
               
        SHA256::computeHash(hash, sequence, 64);

       if(hash[0] == 0 && hash[1] == 0){   
            send_nonce(nonce);         
        } 
       *nonce += 1; //increase nonce           
    }
    

}

//**************************** FUNCTION DEFINITIONS **************//
void check_state(){

       int8_t intState = 0;
                
        //Set the motor outputs accordingly to spin the motor          
        intState = readRotorState();
        
        if(intState - oldState == -5){
            state_counter++;
        }
        else if(intState - oldState == 5){
            state_counter--;
        }
        else{
            state_counter += intState - oldState;
        }
        
        oldState = intState;       
        motorOut((intState-orState+lead+6)%6); //+6 to make sure the remainder is positive
        
}

void serialISR(){ //ISR function for new byte
    uint8_t newChar = pc.getc(); 
    inCharQ.put((void*)newChar); //put the pointer into the queue
}

void print_output(){   //printing Nonce
    while(1){     
        osEvent evt = mail_box.get();
        if(evt.status == osEventMail){
            mail_t *mail = (mail_t*)evt.value.p;
            
            if(mail -> code == 1){
                //printing the nonce
                uint8_t* nonce_ptr = (uint8_t*)(mail->match_nonce);
                pc.printf("Nonce: ");
                for(int i=0; i < 8; i++){
                
                    if(*((nonce_ptr)+i) < 16){
                        pc.printf("0%X", *((nonce_ptr)+i));
                    }
                    else{
                        pc.printf("%2X", *((nonce_ptr)+i));              
                    }
                }
                pc.printf("\n\r");
                mail_box.free(mail);  
            }
            
            else if(mail -> code == 2)
                {
                    pc.printf("Rotations (rev): %f\n\r",(mail -> position));
                    pc.printf("Velocity (rev/s): %f\n\r",(mail -> velocity));
                    pc.printf("\n\r");
                    mail_box.free(mail);
                }      
        }
     }      
}

void decode(){
    string temp_key;
    char commandChar;
    int k = 0;
    
    //pc.printf("in decode thread\n\r");
    pc.attach(&serialISR);
    while(1) {
       
        osEvent newEvent = inCharQ.get(); //get the pointer in the queue
        uint8_t newChar = (uint8_t) newEvent.value.p; 
        commandChar = (char)newChar; //change newChar into a char type
        pc.printf(&commandChar); //print whatever the user inputs
        if(newChar == 0x0D){ //if the user press Enter
           newKey_mutex.lock();
           
           if(temp_key[0] == 'K'){
                for(int i=0; i<8; i++){ //For key change command
                    char buf[5] = {'0','x', temp_key[2*i+1], temp_key[2*i+2], 0};
                    receivedKey[7-i] = strtol(buf, NULL, 0); // convert string to integer
                
                    newKey = (newKey << 8) | receivedKey[i];  
                } 
            }
            else if(temp_key[0] == 'V') //For speed command
            {
                    temp_key.erase(temp_key.begin());
                    sscanf(temp_key.c_str(), "%f", &s_max);
                    s_max = s_max * 6;
                   integral = 0;
                       
            }
            
            else if(temp_key[0] == 'R') //For rotation command
            {
                    temp_key.erase(temp_key.begin());
                    sscanf(temp_key.c_str(), "%f", &r_input);
                    r_input = r_input * 6;
                    target = static_position + r_input;
            }
            
            else if(temp_key[0] == 'T') //For tune command
            {
                k = 0;
                float tune_period[17] = {0};
                temp_key.erase(temp_key.begin());
                while(temp_key.length() != 0){                    
                    if(temp_key[1] == '#'){
                        if(temp_key[0] == 'C'){
                            tune_period[k] = 0.003608;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'D'){
                            tune_period[k] = 0.003214;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'E'){
                            tune_period[k] = 0.002863;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'F'){
                            tune_period[k] = 0.002703;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'G'){
                            tune_period[k] = 0.002408;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'A'){
                            tune_period[k] = 0.002145;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'B'){
                            tune_period[k] = 0.001911;
                            duration[k] = temp_key[2];
                        }
                        temp_key.erase(0,3);    
                    }
                    
                    else if(temp_key[1] == '^'){
                        if(temp_key[0] == 'C'){
                            tune_period[k] = 0.004050;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'D'){
                            tune_period[k] = 0.003608;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'E'){
                            tune_period[k] = 0.003214;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'F'){
                            tune_period[k] = 0.003034;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'G'){
                            tune_period[k] = 0.002703;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'A'){
                            tune_period[k] = 0.002408;
                            duration[k] = temp_key[2];
                        }
                        else if(temp_key[0] == 'B'){
                            tune_period[k] = 0.002145;
                            duration[k] = temp_key[2];
                        }
                        temp_key.erase(0,3);
                    }    
                    
                    else{
                        if(temp_key[0] == 'C'){
                            tune_period[k] = 0.003822;
                            duration[k] = temp_key[1];
                        }
                        else if(temp_key[0] == 'D'){
                            tune_period[k] = 0.003405;
                            duration[k] = temp_key[1];
                        }
                        else if(temp_key[0] == 'E'){
                            tune_period[k] = 0.003034;
                            duration[k] = temp_key[1];
                        }
                        else if(temp_key[0] == 'F'){
                            tune_period[k] = 0.002863;
                            duration[k] = temp_key[1];
                        }
                        else if(temp_key[0] == 'G'){
                            tune_period[k] = 0.002551;
                            duration[k] = temp_key[1];
                        }
                        else if(temp_key[0] == 'A'){
                            tune_period[k] = 0.002273;
                            duration[k] = temp_key[1];
                        }
                        else if(temp_key[0] == 'B'){
                            tune_period[k] = 0.002025;
                            duration[k] = temp_key[1];
                        }
                        temp_key.erase(0,2);
                    }
                    
                    k++;
                }    
            }
            
            newKey_mutex.unlock();           
            temp_key.clear(); //clear the temp_key          

        }
        
        else{
            temp_key += (char)newChar;
        }  
    }
}

void motorCtrlFn(){ 
    
    float old_position = 0;
    float velocity = 0;   
    int k_ps = 80; 
    int k_is = 35;
    float T_s;
    float current_e_s;
    float old_e_s = 0;
    
    int k_pr = 75;
    int k_dr = 60;
    float T_r; 
    float current_e_r;
    float old_e_r = 0;
    
    float T;
    
    int iteration = 0;
    float interval_t;
    motorCtrlTicker.attach_us(&motorCtrlTick, 100000);
    
    while(1){
        motorCtrlT.signal_wait(0x1);
        
        //captures the current state_counter
        
        core_util_critical_section_enter();
        static_position = state_counter;
        core_util_critical_section_exit();
        t2.stop();               
        
       velocity = ((static_position - old_position)/t2.read());
       interval_t = t2.read();
        t2.reset();
        t2.start();
        old_position = static_position; 
        iteration++;   
        if(iteration == 10){
            iteration = 0;
            push_condition(static_position/6, velocity/6);
           
        }
        
        if(r_input == 0 && s_max == 0){               //maximum speed for motor for default
            pwm.write(1.0f);
            lead = 2;    
        }
        
        else if(r_input != 0 && s_max == 0){            
            current_e_r = target - static_position;      
            T_r = k_pr*current_e_r + k_dr*((current_e_r - old_e_r)/interval_t);
            old_e_r = current_e_r;
            
            if(velocity >= 0){
                T_s = period*1000000;
                if(T_r <= T_s){
                    T = T_r;
                }
                else{
                    T = T_s;
                }
            }
            
            else{
                T_s = -1*period*1000000;
                if(T_r <= T_s){
                    T = T_s;
                }
                else{
                    T = T_r;
                }
            }
            
            if(T <0){
                lead = -2;
                T = T*-1;
                
                if(T >= period*1000000){
                    pwm.write(1.0f);
                }
      
                else{
                    pwm.write(T/(period*1000000));
                }         
            }      
            else{
                lead = 2;
                if(T >= period*1000000){
                    pwm.write(1.0f);
                }
              
                else{
                    pwm.write(T/(period*1000000));
                } 
            }
            
            
        }
        
        else if(r_input == 0 && s_max != 0){              
            T_r = period*1000000;
            current_e_s = s_max - abs(velocity);
            if(integral <= period*1000000){
                integral += 0.5*interval_t*(old_e_s + current_e_s);
            }
             
            if(velocity >= 0){          
                T_s = k_ps*current_e_s + k_is*integral;
            }
            else{
                T_s = -1*(k_ps*current_e_s + k_is*integral);
            }
            old_e_s = current_e_s;
            
            if(velocity >= 0){
                if(T_r <= T_s){
                    T = T_r;
                }
                else{
                    T = T_s;
                }
            }
            
            else{
                if(T_r <= T_s){
                    T = T_s;
                }
                else{
                    T = T_r;
                }
            }
            
            if(T <0){
                lead = -2;
                T = T*-1;
                if(T >= period*1000000){
                    pwm.write(1.0f);
                }
      
                else{
                    pwm.write(T/(period*1000000));
                }         
            }      
            else{
                lead = 2;
                if(T >= period*1000000){
                    pwm.write(1.0f);
                }
              
                else{
                    pwm.write(T/(period*1000000));
                } 
            }
        }
        
        else{
            
            current_e_r = target - static_position;
            T_r = k_pr*current_e_r + k_dr*((current_e_r - old_e_r)/interval_t);
            old_e_r = current_e_r;
            
            current_e_s = s_max - abs(velocity);
            if(integral <= period*1000000){
                integral += 0.5*interval_t*(old_e_s + current_e_s);
            }
            if(velocity >= 0){          
                T_s = k_ps*current_e_s + k_is*integral;
            }
            else{
                T_s = -1*(k_ps*current_e_s + k_is*integral);
            }
            old_e_s = current_e_s;
            
            if(velocity >= 0){
                if(T_r <= T_s){
                    T = T_r;
                }
                else{
                    T = T_s;
                }
            }
            
            else{
                if(T_r <= T_s){
                    T = T_s;
                }
                else{
                    T = T_r;
                }
            }
            
            if(T <0){
                lead = -2;
                T = T*-1;
                if(T >= period*1000000){
                    pwm.write(1.0f);
                }
      
                else{
                    pwm.write(T/(period*1000000));
                }         
            }      
            else{
                lead = 2;
                if(T >= period*1000000){
                    pwm.write(1.0f);
                }
              
                else{
                    pwm.write(T/(period*1000000));
                } 
            }
        }              
    }    
}

void playTune(){
    
    while(1){
        for(int i = 0; tune_period[i] != 0; i++){
            t1.start();
            while(t1.read() <= duration[i] * 2){
                period = tune_period[i];
            }
            t1.reset();
            if(tune_period[i+1] == 0){
                i = 0;
            }
        }
    }
}

void motorCtrlTick(){
    motorCtrlT.signal_set(0x1);   
}

void send_nonce(uint64_t* nonce){
            mail_t *mail = mail_box.alloc();
            mail->code = 1;
            mail->match_nonce = nonce;
            mail_box.put(mail);        
} 

void push_condition(float input_position, float input_velocity)
{
    mail_t *mail = mail_box.alloc();
    mail -> code = 2;
    mail -> position = input_position;
    mail -> velocity = input_velocity;
    mail_box.put(mail);
}
