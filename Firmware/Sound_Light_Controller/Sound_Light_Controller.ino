/*
 Controller to light up various body parts behind a poster when a button is press. A recorded voice says the body part.
 By: Nathan Seidle
 SparkFun Electronics
 Date: June 11th, 2015
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 Runs on Mega Pro 5V with ATmega2560

 MP3 Shield + Mega is hard. Avoid using pin 2, 6, 7, and 8. The SPI pins have to be re-routed and CS has to
 be declared during init as 53.

 Does the audio sound like a monster? Covert the WAV or MP3 to 64kHz.

 The enameled wire is 88mA for 10 LEDs. Scrap the enamel off with 30AWG wire strippers. The third wire is ground
 and must be terminated against the negative wire connecting the LEDs (found by trial and error).

 Testing SparkFun amp vs. Amazon Amp: SparkFun is much louder and cleaner with two Brown/Black/Orange resistors
 and solder jumper cleared.

 Audio Board ->
 OUT+/- -> Speaker
 R/G -> MP3 Shield R/-
 +/- -> 5V/GND
 Trimpot for Volume

 Good online editor: https://twistedwave.com/online/ (cut, amplify, VBR save)
 Audio Cutter: http://mp3cut.net/
 MP3 Volume Increase: http://www.mp3louder.com/
 Down sample MP3s to 64kHz: http://online-audio-converter.com/

 10/26/15
 Added Watchdog but it's broken on the ATmega2560. Here's the fix I found:
 http://forum.arduino.cc/index.php?topic=62813.25;wap2
 Seems to work well.
 
*/

//Include various libraries
#include <SPI.h> //SPI library
#include <SdFat.h> //SDFat Library
#include <SdFatUtil.h> //SDFat Util Library
#include <SFEMP3Shield.h> //Mp3 Shield Library
#include <avr/wdt.h> //We need watch dog for this program

SdFat sd; //Create object to handle SD functions

SFEMP3Shield MP3player; //Create MP3 library object

#define EXHIBIT_SIZE 12 //This is the number of buttons to read (and identical to the number of LEDs to control)

//We use the same code for two exhibits but use different MP3 file names
#define SKELETON 1
#define ORGAN 2
//#define EXHIBIT_TYPE SKELETON //Which exhibit are we working on?
#define EXHIBIT_TYPE ORGAN //Which exhibit are we working on?

//Hardware pin definitions
//The buttons array contains the hardware aruino pin numbers to buttons
//The first pin number is button0, 2nd is button1, etc.
//                              BTN 0,  1,  2, 3, 4, 5, 6,  7,  8,  9,  10, 11
const byte buttons[EXHIBIT_SIZE] = {13, 11, 9, 29, 5, 3, 14, 16, 18, 22, 24, 26};

//The lights array contains the hardware arduino pin numbers to lights
//The first pin number is light0, 2nd is light1, etc.
//                             LED 0,  1,  2,  3,  4, 5,  6,  7,  8,  9,  10, 11
const byte lights[EXHIBIT_SIZE] = {12, 10, 28, 30, 4, 31, 15, 17, 19, 23, 25, 27}; //Had to re-write B2 to pin 28


//Keeps track of whether we've played to 2-language title of the bone or if we've
//played the info about this particular button
boolean infoPlayed[EXHIBIT_SIZE];

long buttonTimeout[EXHIBIT_SIZE]; //This array stores the number of millis seconds since a button has been pressed
#define MINTIMEOUT  1500 //This is the minimum amount of time required between button plays, in ms, 250ms works ok

boolean playerStopped = false; //These are booleans used to control the main loop

long lastMillis = 0; //Keeps track of how long since last button press


unsigned long resetTime = 0;

// You can make this time as long as you want,
// it's not limited to 8 seconds like the normal watchdog
#define TIMEOUTPERIOD 2000             

#define petDog() resetTime = millis();  // This macro will reset the timer

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void watchdogSetup()
{
  cli();  // disable all interrupts
  petDog(); // reset the WDT timer
  MCUSR &= ~(1 << WDRF); // because the data sheet said to

  /*
  WDTCSR configuration:
  WDIE = 1 :Interrupt Enable
  WDE = 1  :Reset Enable - I won't be using this on the 2560
  WDP3 = 0 :For 1000ms Time-out
  WDP2 = 1 :bit pattern is
  WDP1 = 1 :0110  change this for a different
  WDP0 = 0 :timeout period.
  */

  // Enter Watchdog Configuration mode:
  WDTCSR = (1 << WDCE) | (1 << WDE);
  // Set Watchdog settings: interrupte enable, 0110 for timer
  WDTCSR = (1 << WDIE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1) | (0 << WDP0);

  sei();

  Serial.println("finished watchdog setup");  // just here for testing
}


ISR(WDT_vect) // Watchdog timer interrupt.
{
  if (millis() - resetTime > TIMEOUTPERIOD) {
    //Serial.println("This is where it would have rebooted");  // just here for testing
    //petDog();                                          // take these lines out
    resetFunc();     // This will call location zero and cause a reboot.
  }
  else
    Serial.println("WDT Vect run");
}

void setup()
{
  petDog(); //Pet the dog
  wdt_disable(); //We don't want the watchdog during init

  watchdogSetup();

  Serial.begin(57600);
  Serial.println("Coming up...");

  for (byte x = 0 ; x < EXHIBIT_SIZE ; x++)
  {
    pinMode(buttons[x], INPUT_PULLUP); //Define buttons as inputs with pull up resistors
    buttonTimeout[x] = 0; //Reset all the button timeouts

    pinMode(lights[x], OUTPUT); //Define LED as outputs

    infoPlayed[x] = false; //Default to not having played the info for this button
  }

  deactivateLight(-1); //Turn off all lights

  initSD(); //Initialize the SD card
  initMP3Player(); // Initialize the MP3 Shield

  playTrack("bell.mp3"); //Generic start up music

  Serial.println("Light sound controller online");

  //wdt_enable(WDTO_250MS); //Unleash the beast Doesn't work on ATmega2560 base boards
}

void loop()
{
  petDog(); //Reset the watchdog timer

  //Add time to each of the button's timeouts
  //We don't want to play sounds too close to each other, so this keeps track of how much time has been
  //between each button press.
  for (byte x = 0 ; x < EXHIBIT_SIZE ; x++)
    buttonTimeout[x] += (millis() - lastMillis);
  lastMillis = millis();

  int button = checkButtons(); //Returns the number of the first button pressed

  if (button > -1)
  {
    Serial.print("Button pressed: ");
    Serial.println(button);

    //Check to see if enough time has passed to accept this button press
    if (buttonTimeout[button] > MINTIMEOUT)
    {
      Serial.println("Timeout passed so doing action");
      buttonTimeout[button] = 0; //Reset the amount of time to zero

      deactivateLight(-1); //Turn off any other lights
      activateLight(button); //Turn on this one

      playSound(button); //Play this track
    }
  }

  //Turn off MP3 chip when not playing
  if (MP3player.isPlaying() == false && playerStopped == false)
  {
    Serial.println("Shutting down MP3");
    //MP3player.end(); //MP3 player is buzzing. This disables it.
    playerStopped = true;

    Serial.println("Deactivating light");
    deactivateLight(-1); //Turns off all lights
  }

}

//Given a light number, turn it on
//-1 turns them all on
void activateLight(byte lightNumber)
{
  if (lightNumber > EXHIBIT_SIZE)
  {
    Serial.println("Turning on all lights");
    //Turn em all on
    for (byte x = 0 ; x < EXHIBIT_SIZE ; x++)
      digitalWrite(lights[x], HIGH);
  }
  else
  {
    Serial.print("Turning on light #: ");
    Serial.println(lights[lightNumber]);

    //if(lights[lightNumber] == 9) digitalWrite(
    digitalWrite(lights[lightNumber], HIGH);
  }
}

//Given a light number, turn it off
//-1 turns them all off
void deactivateLight(byte lightNumber)
{
  if (lightNumber > EXHIBIT_SIZE)
  {
    //Turn em all off
    for (byte x = 0 ; x < EXHIBIT_SIZE ; x++)
      digitalWrite(lights[x], LOW);
  }
  else
    digitalWrite(lights[lightNumber], LOW);
}

//Checks all the buttons
//Returns the first button that is pressed
int checkButtons(void)
{
  for (byte x = 0 ; x < EXHIBIT_SIZE ; x++)
  {
    petDog(); //Pet the dog

    if (digitalRead(buttons[x]) == LOW)
    {
      delay(10); //Debounce
      return (x);
    }
  }

  return (-1); //No button is pressed
}

//Plays a given track number
//Then turns off audio to reduce hissing
void playSound(byte trackNumber)
{
  if (MP3player.isPlaying())
  {
    Serial.println("Interrupting track");
    MP3player.stopTrack(); //Stop any previous track
  }

  //MP3player.begin(); //Causes crackling and resets MP3 volume

  char trackName[12];
  getTrackName(trackNumber, trackName);

  Serial.print("Playing: ");
  Serial.println(trackName);

  petDog(); //Pet the dog
  MP3player.playMP3(trackName);
  petDog(); //Pet the dog

  playerStopped = false; //Boolean for main loop to turn off MP3 IC
}

//Based on a button number look up the correct track name
//Return a string that is the mp3 file name
void getTrackName(byte trackNumber, char *trackName)
{
  char beginning[12];
  char middle[12];
  char ending[12];

  sprintf(beginning, "none");
  sprintf(ending, ".mp3");

  //These are for skelton exhibit
  if (EXHIBIT_TYPE == SKELETON)
  {
    if (trackNumber == 0) sprintf(beginning, "stern");
    if (trackNumber == 1) sprintf(beginning, "fem");
    if (trackNumber == 2) sprintf(beginning, "pel");
    if (trackNumber == 3) sprintf(beginning, "rib");
    if (trackNumber == 4) sprintf(beginning, "uln");
    if (trackNumber == 5) sprintf(beginning, "skul");
    if (trackNumber == 6) sprintf(beginning, "tib");
    if (trackNumber == 7) sprintf(beginning, "fib");
    if (trackNumber == 8) sprintf(beginning, "vert");
    if (trackNumber == 9) sprintf(beginning, "rad");
    if (trackNumber == 10) sprintf(beginning, "hum");
    if (trackNumber == 11) sprintf(beginning, "pha");
  }
  else if (EXHIBIT_TYPE == ORGAN)
  {
    //These are for organ exhibit
    if (trackNumber == 0) sprintf(beginning, "bran");
    if (trackNumber == 1) sprintf(beginning, "hrt");
    if (trackNumber == 2) sprintf(beginning, "lung");
    if (trackNumber == 3) sprintf(beginning, "stom");
    if (trackNumber == 4) sprintf(beginning, "liv");
    if (trackNumber == 5) sprintf(beginning, "sml");
    if (trackNumber == 6) sprintf(beginning, "lrg");
    if (trackNumber == 7) sprintf(beginning, "blad");
    if (trackNumber == 8) sprintf(beginning, "pan");
    if (trackNumber == 9) sprintf(beginning, "eso");
    if (trackNumber == 10) sprintf(beginning, "nas");
    if (trackNumber == 11) sprintf(beginning, "trac");
  }

  if (infoPlayed[trackNumber] == false)
  {
    infoPlayed[trackNumber] = true;
    sprintf(middle, "-i");
  }
  else
  {
    infoPlayed[trackNumber] = false;
    sprintf(middle, "");
  }

  sprintf(trackName, "%s%s%s", beginning, middle, ending);
}

//Plays a given track name
//Then turns off audio to reduce hissing
void playTrack(char * track_name)
{
  petDog(); //Pet the dog
  //Not sure how long these functions take
  MP3player.playMP3(track_name);
  petDog(); //Pet the dog

  playerStopped = false; //Boolean for main loop to turn off MP3 IC
}

//Initializes the SD card and checks for an error
void initSD()
{
  //Initialize the SdCard.
  //  if (!sd.begin(SD_SEL, SPI_HALF_SPEED))
  if (!sd.begin(53, SPI_HALF_SPEED)) //Chip select is on 53
    sd.initErrorHalt();
}

//Sets up all of the initialization for the
//MP3 Player Shield. It runs the begin() function, checks
//for errors, applies a patch if found, and sets the volume/
//stero mode.
void initMP3Player()
{
  MP3player.begin(); // init the mp3 player shield

  //Volume level 5/6/2015 - Original setting
  MP3player.setVolume(30, 30); // MP3 Player volume 0=max, 255=lowest (off)

  MP3player.setMonoMode(1); // Mono setting: 0=off, 1 = on, 3=max
}

