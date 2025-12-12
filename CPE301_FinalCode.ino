// CPE 301 Final Project
// Ashley Chen, Vennethe Valenzuela, Chase Neilson, Emily Tran
// Automatic Gardening Device for Green Onion Plant (AGDGOP)

#include <LiquidCrystal.h>
#include <Servo.h>
#include <I2C_RTC.h> //RTC library created by Manjunath CV (will need to be downloaded from libraries)

#define RDA 0x80
#define TBE 0x20  

// Timer Pointers
volatile unsigned char *myTCCR1A  = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B  = (unsigned char *) 0x81;
volatile unsigned char *myTIFR1   = (unsigned char *) 0x36;
volatile unsigned int  *myTCNT1   = (unsigned int *) 0x84;
volatile unsigned char *portDDRE  = (unsigned char*) 0x2D;
volatile unsigned char *portE    = (unsigned char*) 0x2E;
volatile unsigned char *pinE     = (unsigned char*) 0x2C;

// button pointers + variables
volatile unsigned char* portDDRB = (unsigned char*) 0x24;
volatile unsigned char* portB = (unsigned char*) 0x25;
volatile unsigned char* pinB = (unsigned char*) 0x23;
int buttonOn = 0;
bool buttonPressed = false;

// Analog to Digital Pointers 
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

// UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// LCD pin assignment
const int RS = 43, EN = 44, D4 = 19, D5 = 18, D6 = 17, D7 = 16;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);
const char* disabledText[] = {"OFF             ", "                "};
const int disabledSize = (sizeof(disabled)/sizeof(disabled[0]));
const char* errorText[] = {"I study phys at ", "Waterloo (error)"};
const int errorSize = (sizeof(errorText)/sizeof(errorText[0]));
const char* waterText[] = {"I'm thirsty!    ", "                "};
const int waterSize = (sizeof(waterText)/sizeof(waterText[0]));
const char* runningText[] = {"I'm getting a   ", "haircut!        "};
const int runningSize = (sizeof(runningText)/sizeof(runningText[0]));
const char* waterLevelText[] = {"Water Level: 000", "                "};
const int waterLevelSize = (sizeof(waterLevelText)/sizeof(waterLevelText[0]));

// water level initialization
volatile int waterLevelMeasure = 100;
volatile int waterLevelHun;
volatile int waterLevelTen;
volatile int waterLevelOne;
//char waterLevelText[] = "Water Level: 000";

// Servo object to control + servo position
Servo myservo;
int pos = 0;

// myTone variables for buzzer
unsigned long startMillis;
unsigned long currentMillis;

// ultrasonic sensor assignments + reading variables
volatile unsigned char *portH = (unsigned char *)0x102;
volatile unsigned char *portDDRH = (unsigned char *)0x101;
const int trigPinTop = 9; // PH6
const int echoPinTop = 8; // PH5
const int trigPinBottom = 7; // PH4
const int echoPinBottom = 6; // PH3
volatile long durationTop, durationBottom, distanceTop, distanceBottom;

// variable for RTC
static DS1307 RTC;
int hours, minutes, seconds;
int currentState = 1;
char Timestamp[20];


void startInterrupt() {
  buttonPressed = true;
  if(buttonOn == 0){
    buttonOn = 1;
  }
  else if(buttonOn == 1) {
    buttonOn = 0;
  }
}

void setup() {
  // button interrupt
  *portDDRB |= 0b11000000;
  attachInterrupt(digitalPinToInterrupt(2), startInterrupt, FALLING);

  // for buzzer delay
  *portDDRE |= 0x20;

  // set up the ADC
  adc_init();

  // LCD set up number of columns and rows
  lcd.begin(16, 2);

  // attach servo object to pin 5
  myservo.attach(5);

  // ultrasonic sensor initialization
  // trig pins (9 & 7; PH6 & PH4) to output; echo pins (8 & 6; PH5 & PH3) to input
  *portDDRH |= 0x50; // 0101 0000

  // set to computer's date and time
  RTC.begin();
  RTC.setDateTime(__DATE__, __TIME__);

  U0init(9600);
}

void loop() {
//button logic
  if(buttonPressed == true) {
    ms_delay(200);
    buttonPressed = false;
  }

  if(buttonOn == 0) { //off state
    disabled();
  }
  else if(distanceBottom > 5) { //error state
    error();
  }
  else if(waterLevelMeasure < 100) { //thirsty state
    thirsty();
  }
  else if(distanceTop < 10) { //active cutting state
    running();
  }
  else { //idle state
    idle();
  }
}

void disabled() {
  // LED yellow
  *portB &= 0x00;
  *portB |= 0x40;

  // display message
  lcdPrint(disabledText, disabledSize);
  lcd.setCursor(0,1);
  lcd.print("                ");

  if(currentState != 0){ //makes it so the message only prints to serial monitor once
    getTime();
    printMessage(Timestamp);
    currentState = 0;
  }
}

void idle() {
  // LED green
  *portB &= 0x00;
  *portB |= 0x20;

  // check + display water levels
  waterSensor();
  lcdPrint(waterLevelText, waterLevelSize);

  // check ultrasonic sensor readings
  ultrasonicSensorTop();
  ultrasonicSensorBottom();
  
  if(currentState != 1){
    getTime();
    printMessage(Timestamp);
    currentState = 1;
  }
}

void error() {
  // LED red
  *portB &= 0x00;
  *portB |= 0x80;

  // display message
  lcdPrint(errorText, errorSize);

  // check for bald
  ultrasonicSensorBottom();

  if(currentState != 2){
    getTime();
    printMessage(Timestamp);
    currentState = 2;
  }
}

void thirsty() {
  // LED blue
  *portB &= 0x00;
  *portB |= 0x02;

  // buzzer
  myTone(1000); 
  myDelay(10000, 0x02);

  // check water level
  waterSensor();

  // display message
  lcdPrint(waterText, waterSize);

  // check ultrasonic sensor readings
  ultrasonicSensorTop();
  ultrasonicSensorBottom();

  if(currentState != 3){
    getTime();
    printMessage(Timestamp);
    currentState = 3;
  }
}

void running() {
  // LED white
  *portB &= 0x00;
  *portB |= 0x01;

  // display message
  lcdPrint(runningText, runningSize);

  // run servo motor
  motor();

  // check ultrasonic sensor readings
  ultrasonicSensorTop();
  ultrasonicSensorBottom();

  if(currentState != 4){
    getTime();
    printMessage(Timestamp);
    currentState = 4;
  }
}

// delay used for buzzer + ultrasonic sensor
void myDelay(unsigned int ticks, byte prescaler) { 
  // Stop the timer (set prescaler to 000)
  *myTCCR1B &= 0xF8; // 0b1111 1000
  // Set the counts (16 bits -> 2 addresses!)
  *myTCNT1 = (unsigned int) (65536 - ticks);
  // Start the timer (set prescaler to 8)
  *myTCCR1B |= prescaler; 
  // Wait for overflow
  while((*myTIFR1 & 0x01)==0); // 0b0000 0001
  // stop the timer (set prescaler to 000)
  *myTCCR1B &= 0xF8; // 0b1111 1000
  // reset TOV (write a 1 to reset to 0)
  *myTIFR1 |= 0x01; // 0b0000 0001
}

// fuction for toggling buzzer
void myTone(unsigned int milliseconds)
{
  // updates millis variables for new run
  currentMillis = millis();
  startMillis = millis();

  // runs buzzer for specified interval
  while(currentMillis - startMillis <= milliseconds){
    currentMillis = millis();

    *portE ^= 0x20;
    myDelay(10000, 0x02);
  }
}

// delay used for servo motor
void ms_delay(unsigned int milliseconds)
{
  // Repeat 1 ms delay 
  for (unsigned int i = 0; i < milliseconds; i++){
    // 25000 ticks = 1 ms
    const int ticks_needed = 25000;
    
    // Stop the timer (set prescaler to 000)
    *myTCCR1B &= 0xF8; // 0b1111 1000
    // Set the counts (16 bits -> 2 addresses!)
    *myTCNT1 = (unsigned int) (65536 - ticks_needed);
    // Start the timer (set prescaler of 64)
    *myTCCR1B |= 0x03; // 0b0000 0011
    // Wait for overflow
    while((*myTIFR1 & 0x01)==0); // 0b0000 0001
    // stop the timer (set prescaler to 000)
    *myTCCR1B &= 0xF8; // 0b1111 1000
    // reset TOV (write a 1 to reset to 0)
    *myTIFR1 |= 0x01; // 0b0000 0001
  }
}

// UART Functions
void U0init(int U0baud){
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

// analog to digital conversion
void adc_init(){
  // setup the A register
  // set bit 7 to 1 to enable the ADC 
  *my_ADCSRA |= 0b10000000;

  // clear bit 5 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11011111;

  // clear bit 3 to 0 to disable the ADC interrupt 
  *my_ADCSRA &= 0b11110111;

  // clear bit 0-2 to 0 to set prescaler selection to slow reading
  *my_ADCSRA &= 0b11111000;

  // setup the B register
  // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11110111;

  // clear bit 2-0 to 0 to set free running mode
  *my_ADCSRB &= 0b11111000;

  // setup the MUX Register
  // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX &= 0b01111111;

  // set bit 6 to 1 for AVCC analog reference
  *my_ADMUX |= 0b01000000;

  // clear bit 5 to 0 for right adjust result
  *my_ADMUX &= 0b11011111;

  // clear bit 4-0 to 0 to reset the channel and gain bits
  *my_ADMUX &= 0b11100000;
}

unsigned int adc_read(unsigned char adc_channel_num) // work with channel 0
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX &= 0b11100000;

  // clear the channel selection bits (MUX 5) hint: it's not in the ADMUX register
  *my_ADCSRB &= 0b11110111;
 
  // set the channel selection bits for channel 0
  *my_ADMUX += adc_channel_num;

  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0b01000000;

  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);

  // return the result in the ADC data register and format the data based on right justification (check the lecture slide)
  unsigned int val = *my_ADC_DATA & 0x3FF;
  return val;
}

void waterSensor() {
  waterLevelMeasure = adc_read(0);
  waterLevelHun = waterLevelMeasure/100;
  waterLevelTen = (waterLevelMeasure%100)/10;
  waterLevelOne = (waterLevelMeasure%10);
  sprintf(waterLevelText[0], "Water Level: %d%d%d", waterLevelHun, waterLevelTen, waterLevelOne);
}

void motor() {
  for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    ms_delay(1);             // waits 15ms for the servo to reach the position
  }
  for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    ms_delay(1);             // waits 15ms for the servo to reach the position
  }
}

void ultrasonicSensorTop() {
  *portH &= 0x00;
  myDelay(32, 0x01);
  *portH |= 0x40;
  myDelay(160, 0x01);
  *portH &= 0x00;

  durationTop = pulseIn(echoPinTop, HIGH);
  distanceTop = durationTop/29/2;
}

void ultrasonicSensorBottom() {
  *portH &= 0x00;
  myDelay(32, 0x01);
  *portH |= 0x10;
  myDelay(160, 0x01);
  *portH &= 0x00;

  durationBottom = pulseIn(echoPinBottom, HIGH);
  distanceBottom = durationBottom/29/2;
}

// printing by line on LCD
void lcdPrint(const char* text[], const int line) {
  for(int i = 0; i < line; i++) {
    lcd.setCursor(0,i);
    lcd.print(text[i]);
  }
}

// printing to serial monitor
void printMessage(char message[]){
  int i = 0;
  while(message[i] != '\0'){
    U0putchar(message[i]);
    i++;
  }
  U0putchar('\n');
}

// retrieve + print time to serial monitor
void getTime(){
  hours = RTC.getHours();
  minutes = RTC.getMinutes();
  seconds = RTC.getSeconds();
  if(hours > 12){
    hours = hours - 12;
    sprintf(Timestamp, "%d:%d:%d PM", hours, minutes, seconds);
  }
  else{
    sprintf(Timestamp, "%d:%d:%d AM", hours, minutes, seconds);
  }
}