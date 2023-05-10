// Antoinette Adduchio
// CPE Final Project 
// 05/09/2023


#include <LiquidCrystal.h>

#include <DHT.h>

#include <Wire.h>

#include <RTClib.h>

#include <Stepper.h>

#include <stdio.h>

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

void U0putchar(char c) {
  while(!(UCSR0A & (1<<UDRE0))); // wait for the transmit buffer to be empty
  UDR0 = c; // send the character
}

//



//Clock and DHT Pins
#define DHTPIN 31 //DHT LEFT PIN FACING BLUE 3.3 center and RIGHT PIN GND
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 real_clock; //SCL and SDA to their corresponding pins on Arduino COMMUNICATION ports
//


// lcd
char sun_sat [7] [12]= {"Sun", "Mon" , "Tue" , "Wed", "Thurs", "Fri" , "Sat"};
const int rs = 2, en = 3, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// 

// LED pins - Port B
#define LED_RED 0
#define LED_GREEN 1
#define LED_BLUE 2

// Stepper motor button pin - Port B
#define stepperButton 6

// LED registers and Stepper motor button registers
volatile unsigned char* port_B = (unsigned char*) 0x25;
volatile unsigned char* ddr_B = (unsigned char*) 0x24;
volatile unsigned char* pin_B = (unsigned char*) 0x23;
// 

// Fan motor pins - Port G
#define ENABLE 2
#define DIN1 1
#define DIN2 0

// Fan motor registers
volatile unsigned char* port_G = (unsigned char*) 0x34;
volatile unsigned char* ddr_G = (unsigned char*) 0x33;
volatile unsigned char* pin_G = (unsigned char*) 0x32;
// 

// On/Off - Port D
#define ON_OFF 2 
volatile unsigned char* port_D = (unsigned char*) 0x2B;
volatile unsigned char* ddr_D = (unsigned char*) 0x2A;
volatile unsigned char* pin_D = (unsigned char*) 0x29;

// water sensor
#define WATERSENSOR 0
volatile unsigned char* my_ADCSRA = 0x7A;
volatile unsigned char* my_ADCSRB = 0x7B;
volatile unsigned char* my_ADMUX = 0x7C;
volatile unsigned int* my_ADC_DATA = 0x78;


// 
volatile unsigned char* my_ECIRA = (unsigned char*) 0x69;
volatile unsigned char* my_EIMSK = (unsigned char*) 0x3D;




// Stepper Motor variables and pin setup
const float STEPS_PER_REV = 32;
const float GEAR_RED = 64;
const float STEPS_PER_OUT_REV = STEPS_PER_REV * GEAR_RED;
int stepsRequired;
int stepperButtonState = 0; 
int ventStatus = 0;
Stepper steppermotor(STEPS_PER_REV, 8, 10, 9, 11);
//
//Output, input, high, low definitions
#define SET_OUTPUT(ddr, pin) *ddr |= (0x01 << pin);
#define SET_INPUT(ddr, pin) *ddr &= ~(0x01 << pin);
#define SET_HIGH(port, pin) *port |= (0x01 << pin);
#define SET_LOW(port, pin) *port &= ~(0x01 << pin);
//
//state switching 
bool stateChange = true; 
unsigned int stateValue = 1; 
int tempThreshold = 21; 
bool onOFF = false; 

ISR(INT2_vect){

  SET_LOW(port_G, ENABLE); // Turns off fan if it was previously on
  onOFF = !onOFF; //on or off bool variable is flipped before 'if' statment to choose correct state
  stateChange = true;
  
  
  if(onOFF == true)
  { 
      stateValue = 2;//go to idle
  }
  else {
  stateValue = 1;// go to disabled
  }
}


//
void setup() {

 
  
  // ADC for water sensor read
  adc_init();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  dht.begin();
  Serial.begin(9600);
  
  real_clock.begin();
  // recieving date and time from computer 
  real_clock.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // interrupt 
  *my_ECIRA |= 0x20;
  *my_EIMSK |= 0x04; 

  // LED's as outputs
  SET_OUTPUT(ddr_B, LED_GREEN); 
  SET_OUTPUT(ddr_B, LED_BLUE); 
  SET_OUTPUT(ddr_B, LED_RED); 

  // DC fan motor outputs
  SET_OUTPUT(ddr_G, ENABLE);
  SET_OUTPUT(ddr_G, DIN1);
  SET_OUTPUT(ddr_G, DIN2);

  //on/off button pin as input
  SET_INPUT(ddr_D, ON_OFF); 
  SET_HIGH(port_D, ON_OFF);

  //stepperButton as input
  SET_INPUT(ddr_B, stepperButton);
  SET_HIGH(port_B, stepperButton);
}


//Loop
void loop() {

  switch (stateValue) {
  case 1:
    Disabled();
    break;
  case 2:
    Idle();
    break;
  case 3:
    Running();
  case 4:
    Error();
    break;
  }
}




//STATE FUNCTIONS 

// Case 1
void Disabled(){  

  // yellow led
  SET_HIGH(port_B, LED_GREEN);
  
  SET_LOW(port_B, LED_BLUE);
  
  SET_HIGH(port_B, LED_RED);
  
  //If state has recently changed to disabled, display timestamp to serial
  if(stateChange == true)
  {
    U0putchar("System Disabled at: ");
    clockDisplay();
    stateChange = false; //State change set to false so timestamp is not repeatedly sent
  }

  else
    {
   U0putchar("Disabled\n");
   delay(200);
  }

  
  
  lcd.clear();
}

//Case 2
void Idle()
{
// Green LED
  
  SET_HIGH(port_B, LED_GREEN);
  
  SET_LOW(port_B, LED_BLUE);
  
  SET_LOW(port_B, LED_RED);

  //If state is entered via state change, print timestamp
  if(stateChange == true)
  {
   U0putchar("\nSystem switched to Idle at: ");
   clockDisplay();
   stateChange = false;  
  }
  
 //Temp and Humidity displayed on LCD during Idle and Running states
  displayDHT();
  ventControl();
  
  // Water sensor
  char buffer[20];
  sprintf(buffer, "Water Level: %d\n", getWaterLevel());
  for (int i = 0; i < strlen(buffer); i++) {
    U0putchar(buffer[i]);
}
 

 //If waterlevel is below 100 switch to error
 if(getWaterLevel() < 100)
 {
  stateChange = true;
  stateValue = 4;
 }
 
 //if temperature is above threshold, switch to running
 if(temp() > tempThreshold)
 {
  stateChange = true;
  stateValue = 3; 
 }
}

 // Case 3
int Running()
{  
   //Blue LED
  
  SET_LOW(port_B, LED_GREEN);
  
  SET_HIGH(port_B, LED_BLUE);
  
  SET_LOW(port_B, LED_RED);

  //Turn on fan
  SET_HIGH(port_G, ENABLE);
  
  SET_HIGH(port_G, DIN1);
  
  SET_LOW(port_G, DIN2);
  
   if(stateChange == true)
   {
    U0putchar("\nSystem switched to Running at: ");
    clockDisplay();
    stateChange = false;
   }

   //Temp and Humidity displayed on LCD during Idle and Running states
   displayDHT();
   ventControl();
   
  //Water sensor
 char buffer[20];
 sprintf(buffer, "Water Level: %d\n", getWaterLevel());
 for (int i = 0; i < strlen(buffer); i++) {
  U0putchar(buffer[i]);
}


 //If water level is below threshold switch to error
 if(getWaterLevel() < 100)
 {
  
  SET_LOW(port_G, ENABLE); //Turn off fan before exit
  stateChange = true;
  stateValue = 4;
 }

 //If temperature is below threshold, switch to idle
 if (temp() < tempThreshold)
  {
    
    SET_LOW(port_G, ENABLE); //Turn off fan before exit
    stateChange = true;
    stateValue = 2;
  }
 
}
// Case 4
int Error(){ 
//RED LED
  
  SET_LOW(port_B, LED_GREEN);
  
  SET_LOW(port_B, LED_BLUE);
  
  SET_HIGH(port_B, LED_RED);
  
  if(stateChange == true)
  {
    U0putchar("\nSystem switched to ERROR at: ");
    clockDisplay();
    stateChange = false;
  }

  //error message to LCD
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("!!Error!!");
  lcd.setCursor(1,0);
  lcd.print("!!Low!!");
  
  ventControl();
  char buffer[20];
  sprintf(buffer, "Water Level: %d\n", getWaterLevel());
  for (int i = 0; i < strlen(buffer); i++) {
    U0putchar(buffer[i]);
}

  
 if (getWaterLevel() > 100)
  {
    stateChange = true;
    stateValue = 2;
  }

  
}
//END 




//Function to display time
void clockDisplay()
{
  DateTime now = real_clock.now();
  char buffer[30];
  sprintf(buffer, "%s %d/%d/%d %d:%d:%d\n", sun_sat[now.dayOfTheWeek()], now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());

  for (int i = 0; i < strlen(buffer); i++) {
    U0putchar(buffer[i]);
  }
}


//open and close vent/stepper
void ventControl()
{
    
  stepsRequired = STEPS_PER_OUT_REV/2;
  steppermotor.setSpeed(700);
  if(!((*pin_B & (0x001 << stepperButton))) && ventStatus == 0) {
    // open the vent from closed
    // turn stepper on:
    steppermotor.step(stepsRequired);
    ventStatus = 1;
  } 
  else if(!((*pin_B & (0x001 << stepperButton))) && ventStatus == 1)
  {  // close the vent 
    steppermotor.step(-stepsRequired);
    ventStatus = 0;
  }
  else{
    steppermotor.step(0);
  }
}

    

void displayDHT() {
  delay(2000);
  lcd.clear();
  lcd.print("Temp: ");
  lcd.print(temp());
  lcd.print(char(223));
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Humidity: ");
  lcd.print(humid());
  lcd.print("%");
}


//ADC initializaiton
void adc_init() {
  // set up the A register
  *my_ADCSRA &= 0x00;
  *my_ADCSRA |= 0x80;
  
  // set up the B register
  *my_ADCSRB &= 0x00;
  
  // set up the MUX Register
  *my_ADMUX  &= 0x00;
  *my_ADMUX  |= 0x40;
}

//Helper function to return water level
unsigned int getWaterLevel()
{
  return readWaterLevel(WATERSENSOR);
  
}

//  

unsigned int readWaterLevel(unsigned char adc_channel_num){

  *my_ADMUX  &= 0xE0;       
  *my_ADCSRB &= 0xF7;       
  *my_ADMUX  += adc_channel_num;  
  *my_ADCSRA |= 0x40;       
  
  while((*my_ADCSRA & 0x40)==1);   // wait for the conversion to complete
  
  return *my_ADC_DATA;       // return the result in the ADC data register
}
// help function to retrun to temp
float temp() {
  return (dht.readTemperature());
}

// help to return to humidity 
float humid() {
  return (dht.readHumidity());
}

void displayTempHumid() {
  char buffer[20];
  sprintf(buffer, "Temperature: %.1f%cC\n", temp(), 0xDF);
  for (int i = 0; i < strlen(buffer); i++) {
    U0putchar(buffer[i]);
  }
  sprintf(buffer, "Humidity: %.1f%%\n", humid());
  for (int i = 0; i < strlen(buffer); i++) {
    U0putchar(buffer[i]);
  }
}
