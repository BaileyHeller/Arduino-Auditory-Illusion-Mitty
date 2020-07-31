/* CREATED BY BAILEY HELLER.
 * Check out my youtube channel where I post my projects: https://www.youtube.com/channel/UCN3RCnKlhSSYsCFQBdELtPw

 * This code reads specially formatted .txt files off an SD card.
 * It then plays up to 16 beepers at a time according to the SD card instructions.
 * It also displays subtitles written within the .txt file on a I2C LCD screen.

 * You can totally use any/all of this code (if you can somehow decifer it).
*/
#include <SPI.h>
#include <SD.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include "noteHalfPeriod.h"

//SD STUFF
#define MICROS_PER_BEAT 11592 //Measured in midi units rather than usual music time signature 11592
char fileName[] = "jerk.txt"; //.txt file name

//this is taken from parseLinetest.ino
char delimiter[] = " "; //how each integer in the file is seperated
char* parsePosition; //ths is a "pointer" that directs the code to a position within the rawSdInput variable

const char messageIdentifier = '+'; //this is the indicator that signals that the line of SD input is a message to be displayed by the lcd. It is found at the beginning of the third field of the input line
String inputMessage = "";

long sdNoteInput[] = {0,0,0}; //the input of one line of instructions given by (time,channel, note or message) channel 0 is for messages
byte channel;
unsigned long inputTime;

File myFile;


//LCD STUFF
#define BACKLIGHT_PIN     13
char lcdText;
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address


//BUZZER STUFF
#define NUMBER_OF_BUZZERS 16

const byte buzzerLoud[] = {2,3,5,7,8,9,10,11,12,13,14,15,16,17,18,19}; //the digital pins of each buzzer to play loudly
const byte buzzerQuiet[] = {2,3,5,7,8,9,10,11,12,13,14,15,16,17,18,19}; //the digital pins of each buzzer to play quietly
byte buzzerPin[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //dynamic array of what pin the given beeper should beep from (according to above arrays)

long buzzerFrequency[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //records the frequency that all buzzers are playing
unsigned long buzzerLastPeek[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //records the last time each buzzer switched between its LOW/High states
bool buzzerState[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //records if the pin was last set high or low

#define NOTE_OFFSET 0 //pitch up or down


//GENERAL STUFF
unsigned long loopStart; //the time the current loop started
unsigned long loopTime = 0; //the time during a loop (arbitrary to how many loops have occurred)
unsigned long startTimer;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect.
  }

  Serial.print("Initializing SD card...");

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  // Switch on the backlight
  pinMode ( BACKLIGHT_PIN, OUTPUT );
  digitalWrite ( BACKLIGHT_PIN, HIGH );

  lcd.begin(20,4); // initialize the lcd
  lcd.setCursor(7,1); // go home (,Buddy. I work alone.)
  lcd.print("START!");
  tone(2,50,5);
  delay(1500);
  lcd.clear();

  //set all buzzer pins to output
  for (byte i=0; i < NUMBER_OF_BUZZERS; i++){
    pinMode(buzzerLoud[i],OUTPUT);
    pinMode(buzzerQuiet[i],OUTPUT);
    digitalWrite(buzzerLoud[i],LOW);
    digitalWrite(buzzerQuiet[i],LOW);
  }

}

void loop(){
  
  loopStart = micros();
  
  //Open the file for reading:
  myFile = SD.open(fileName);
  if (myFile) {
    Serial.println(fileName);

    while (myFile.available()) {
      
      
      ReadNoteInput();

      //Serial.println(micros()-startTimer);
      inputTime = sdNoteInput[0];
      //Serial.println(micros()-startTimer);
      while (inputTime > loopTime/MICROS_PER_BEAT){
         loopTime = micros() - loopStart;
         
         Buzz();
         //Serial.println(micros()-startTimer);
         //startTimer = micros();
      }

      //startTimer = micros();
      
      if(inputMessage == ""){ //test if the input was a message
        channel = sdNoteInput[1] -1;//makes the first channel 0

        ///////////////////////////////////////////////////////////////////////////////SET THE FREQUENCY
        //long freq = KeyToFrequency(sdNoteInput[2]);
        long freq = noteHalfPeriod[sdNoteInput[2]+NOTE_OFFSET];

        
        
  
        if (buzzerFrequency[channel] == freq){ //if this frequency is being played by this channel already, turn it off
          buzzerFrequency[channel] = 0;
          digitalWrite(buzzerLoud[channel],LOW); //when off, the buzzer pin is set to low
          digitalWrite(buzzerQuiet[channel],LOW); //same for these
          buzzerState[channel] = false;
          
        } else{
          buzzerFrequency[channel] = freq;

          if (sdNoteInput[3] = 2){
            buzzerPin[channel] = buzzerLoud[channel];
          } else { //this also includes that state of a note being turned off
            buzzerPin[channel] = buzzerQuiet[channel];
          }
        }
        
      } else {//if the input was a message
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(inputMessage);
        
        inputMessage = ""; //reset the input
      }

      //THIS IS THE PART WHERE THEY BEEP AWAY
      loopTime = micros() - loopStart;
      Buzz(); //buzz the beepers with the input of the current time to compare to
      
      //clear the recoded sd input
      //sdNoteInput[] = {0,0,0};

    }
    
  }else{
    Serial.println("couldn't open file");
    while (1);
  }

  myFile.close(); //close the file
  lcd.clear();
  delay(1500);
}

void ReadNoteInput(){

  String inputString = myFile.readStringUntil('\n'); //read the line and store it as a string
  byte stringLength = inputString.length();
  char rawSdInput[stringLength+1]; //not including the message text if there is any

  //find the start of the note or message
  byte firstIndexOfSpace = inputString.indexOf(' ');
  byte secondIndexOfSpace = inputString.indexOf(' ', firstIndexOfSpace+1);

  byte numberOfFields = 4; //how many items there are in the input array (1 if it is a message)
  //Serial.print(inputString.charAt(secondIndexOfSpace+1));
  if(inputString.charAt(secondIndexOfSpace+1) == messageIdentifier){
    inputMessage = inputString.substring(secondIndexOfSpace+2,stringLength-1);
    stringLength -= inputMessage.length()+2; //the '+2' includes the message identifier and the space that proceeds it as part of the message
    numberOfFields = 1; //removes the need for a channel and a note input
  }
  
  inputString.toCharArray(rawSdInput,stringLength+1); //convert the string into char type of the same length as the string it is reading
  
  parsePosition = strtok(rawSdInput, delimiter);
  long value; //the value of the field of the array we are on

  //Serial.print("input array: ");
  for (byte i=0;i<numberOfFields;i++){ //repeat for each array field
    value = atoi(parsePosition);
    sdNoteInput[i] = value;
    //Serial.print(value);
    //Serial.print(", ");
    parsePosition = strtok(NULL, delimiter); //move to next position. If we read a seperator don't store it as a value
  }
  //Serial.print(inputMessage);
  //Serial.println("");
}

void Buzz(){

  for (byte i=0; i<NUMBER_OF_BUZZERS;i++){
  
    //if this buzzer is set to be playing and the last time this buzzer switched states was longer than its frequency then switch its state
    //unsigned long nextBeep = buzzerLastPeek[i] + (buzzerFrequency[i]>>1); //this is when this buzzer should beep. the frequency is divided in half to find the high/low time of the wave

    unsigned long nextBeep = buzzerLastPeek[i] + buzzerFrequency[i]; //this is when this buzzer should beep. the buzzerFrequency[] array is declared in noteHalfPeriod.h
    
    if ((buzzerFrequency[i] > 0) && (nextBeep <= loopTime)){
      //switch the buzzer from whatever state it is currently in
      if (buzzerState[i]){
        digitalWrite(buzzerPin[i],LOW);
        buzzerState[i] = false;
      } else {
        digitalWrite(buzzerPin[i],HIGH);
        buzzerState[i] = true;
      }
      buzzerLastPeek[i] = loopTime; //log now as the time the buzzer beeped
    }
  }

}

//////////////////no longer needed b/c these calculations are already done and formatted into an array within noteHalfPeriod.h
long KeyToFrequency(float key){
  key += NOTE_OFFSET; //shift the note
  float keyPower = double((key-69)/12);
  //this gives how many waves per second
  //long convertedFreq = (1 << (keyPower-1))*440; //https://en.wikipedia.org/wiki/Piano_key_frequencies
  float convertedFreq = double(pow(2,keyPower))*440;
  //return how many microseconds between each wave
  return 1000000/convertedFreq;
}
/*WE DECLARE THAT WE ARE ALL RESPONSIBLE: EDUCATORS, POLITICIANS, SOCIAL ORGANISATIONS, TRADE UNIONS, CHURCHES, FOR SAVING AND PROTECTING OUR MOTHER EARTH AND WE PROCLAIM THAT ANOTHER WORLD IS POSSIBLE.
 *IN THE FUTURE OUR MOTHER EARTH WON’T HAVE TO LIVE THROUGH FORESEEABLE TRAGEDIES SUCH AS BHOPAL, CHERNOBYL, OR THE DESTRUCTION OF ECOSYSTEMS AS HAS OCCURRED IN MANY, TOO MANY, MARINE DISASTERS CAUSED BY IRREGULAR OIL TANKERS.
 *WHOEVER HAS CAUSED INTENTIONALLY ENVIRONMENTAL DISASTERS SHALL BE JUDGED BY THE INTERNATIONAL ENVIRONMENTAL CRIMINAL COURT IN ORDER TO PROVIDE A CONCRETE PROTECTION OF THE ENVIRONMENT BY EFFECTIVE, PROPORTIONAL AND DISSUASIVE SANCTIONS.
*/
