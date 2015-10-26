/*
 When button is pressed turn on LED strip
 By: Nathan Seidle
 SparkFun Electronics
 Date: June 11th, 2015
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 Runs on Mega Pro 5V with ATmega2560
 
 Removed status LED from D13 so we can use it on B0
 
 The enameled wire is 88mA for 10 LEDs. Scrape the enamel off with 30AWG wire strippers. The third wire is ground
 and must be terminated against the negative wire connecting the LEDs (found by trial and error).

*/

#define EXHIBIT_SIZE 12 //This is the number of buttons to read (and identical to the number of LEDs to control)

//Hardware pin definitions
//The buttons array contains the hardware aruino pin numbers to buttons
//The first pin number is button0, 2nd is button1, etc.
//                              BTN 0,  1,  2, 3, 4, 5, 6,  7,  8,  9,  10, 11
const byte buttons[EXHIBIT_SIZE] = {13, 11, 9, 7, 5, 3, 14, 16, 18, 22, 24, 26};
//const byte buttons[EXHIBIT_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//The lights array contains the hardware arduino pin numbers to lights
//The first pin number is light0, 2nd is light1, etc.
//                             LED 0,  1,  2, 3, 4, 5, 6,  7,  8,  9,  10, 11
const byte lights[EXHIBIT_SIZE] = {12, 10, 28, 6, 4, 2, 15, 17, 19, 23, 25, 27}; //Had to re-write B2 to pin 28
//const byte lights[EXHIBIT_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void setup()
{
  Serial.begin(57600);
  Serial.println("Testing firmware - press a button and LED will turn on");
  
  for(byte x = 0 ; x < EXHIBIT_SIZE ; x++)
  {
    pinMode(buttons[x], INPUT_PULLUP); //Define buttons as inputs with pull up resistors
    pinMode(lights[x], OUTPUT); //Define LED as outputs
  }

  deactivateLight(-1); //Turn off all lights
}

void loop()
{
  int button = checkButtons(); //Returns the number of the first button pressed
  
  if(button > -1)
  {
    Serial.print("Button pressed: ");
    Serial.println(button);
    
    activateLight(button); //Turn on this one
    
    while(checkButtons() == button) ; //Wait for user to release button
    deactivateLight(-1); //Turn off any other lights
    Serial.println("Button released");
  }
  
}

//Given a light number, turn it on
//-1 turns them all on
void activateLight(byte lightNumber)
{
  if(lightNumber > EXHIBIT_SIZE)
  {
    //Turn em all on
    for(byte x = 0 ; x < EXHIBIT_SIZE ; x++)
      digitalWrite(lights[x], HIGH);
  }
  else
    digitalWrite(lights[lightNumber], HIGH);
}

//Given a light number, turn it off
//-1 turns them all off
void deactivateLight(byte lightNumber)
{
  if(lightNumber > EXHIBIT_SIZE)
  {
    //Turn em all off
    for(byte x = 0 ; x < EXHIBIT_SIZE ; x++)
      digitalWrite(lights[x], LOW);
  }
  else
    digitalWrite(lights[lightNumber], LOW);
}

//Checks all the buttons
//Returns the first button that is pressed
int checkButtons(void)
{
  for(byte x = 0 ; x < EXHIBIT_SIZE ; x++)
    if(digitalRead(buttons[x]) == LOW)
    {
      delay(10); //Debounce
      return(x);
    }

  return(-1); //No button is pressed
}

