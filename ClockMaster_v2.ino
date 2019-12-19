#include "FastLED.h"
#include "TM1637Display.h"
#include "math.h"
#include "SimpleRotary.h"

#define NUM_SYNC_OUT 2

/// LED STUFF
#define PIXEL_FPS 16
#define LED_DATA_PIN 6
CRGB leds[NUM_SYNC_OUT]; // NUM_LEDS == NUM_SYNC_OUT

/// DISPLAY STUFF
#define DISPLAY_FPS 6
const int DISPLAY_CLK = A5;
const int DISPLAY_DIO = A4;
TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

/// INPUT STUFF
SimpleRotary rotary(2,3,4);// Pin A, Pin B, Button Pin

// MAIN STUFF
int mainBPM = 120;
bool isPlaying = false;
unsigned long loopMillis = 0;

unsigned long lastTrigger = -1;
unsigned long lastMainTick = -1;
unsigned long nextMainTick = 0;
long timeToNextMainTick = 0;
unsigned long timeSinceLastMainTick = 0;
float tickCount = 0;

const float smallestMultiplier = 0.0625; // == 1/16
const float largestMultilplier = 4;

enum {MODE_GLOBAL, MODE_CHANNEL};
int modeActive = MODE_GLOBAL;
int selectedChannel = 0;

struct CHANNEL {
  float multiplier;
  bool triggerHigh;
  unsigned long lastTrigger;
  int outputPin;
};
typedef struct CHANNEL Channel;
Channel channels[NUM_SYNC_OUT];
int outputPins[NUM_SYNC_OUT] = {A0, A1}; // put em in there in the right order

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_SYNC_OUT);//.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(63);
  
  display.setBrightness(7); //0 to 7

  // set all channels for a start
  for(int i = 0; i < NUM_SYNC_OUT; i++) {
    channels[i].multiplier = 1;
    channels[i].triggerHigh = false;
    channels[i].lastTrigger = 0;
    pinMode(outputPins[i], OUTPUT);
    channels[i].outputPin = outputPins[i];
  }

  // https://github.com/mprograms/SimpleRotary
  // Set the trigger to be either a HIGH or LOW pin (Default: HIGH)
  // Note this sets all three pins to use the same state.
  rotary.setTrigger(HIGH);
  // Set the debounce delay in ms  (Default: 2)
  rotary.setDebounceDelay(5);
  // Set the error correction delay in ms  (Default: 200)
  rotary.setErrorDelay(100);

  //Serial.begin(9600);
  //Serial.println(channels[0]...);
}



//   __    _____ _____ _____ 
//  |  |  |     |     |  _  |
//  |  |__|  |  |  |  |   __|
//  |_____|_____|_____|__|   
//

void loop() {
  updateChannels();

  checkRotationInput();
  checkButtonInput();  

  updateLeds();

  // one display update-cycle usually takes about 25ms
  // so only update the display when we have enough time to do so
  if (timeToNextMainTick > 40){
    updateDisplay();
  }
  
  //Serial.println();
}



//   _____ _____ _ _____ 
//  |     |  _  |_|   | |
//  | | | |     | | | | |
//  |_|_|_|__|__|_|_|___|
//

void updateChannels(){
  // loop time:
  // Serial.print(millis() - loopMillis); Serial.print(",");
  
  bool hasTicked = false;
  bool isFirstTick = lastMainTick == -1;
  loopMillis = millis();
  
  nextMainTick = getTimeOfNextMainTick(loopMillis);

  do {
    loopMillis = millis();
    timeSinceLastMainTick = loopMillis - lastMainTick;
    timeToNextMainTick = nextMainTick - loopMillis;
  } while (timeToNextMainTick > 0 && timeToNextMainTick < 5); // if very short is left, just wait right here  

  if (timeToNextMainTick <= 0){
    lastTrigger = loopMillis;
    lastMainTick = isFirstTick ? loopMillis : nextMainTick; // not true, but this is when it should have ticked
    tickCount += isFirstTick ? 0 : (1/largestMultilplier);
    hasTicked = true;

    //  ____  _____ __    _____ __ __    ____  _____ _____ _____ _____ 
    // |    \|   __|  |  |  _  |  |  |  |    \|   __| __  |  |  |   __|
    // |  |  |   __|  |__|     |_   _|  |  |  |   __| __ -|  |  |  |  |
    // |____/|_____|_____|__|__| |_|    |____/|_____|_____|_____|_____|
    // 
    // see how late we can get
    // Serial.println(timeToNextMainTick);
  }
  //Serial.print(timeToNextMainTick); Serial.print(",");

  // go through all channels
  for(int i = 0; i < NUM_SYNC_OUT; i++) {
    
    if (isPlaying && hasTicked){
      // check if enough time elapsed to set trigger high
      float volcaMultiplier = (channels[i].multiplier*2);
      
      if (fmod (tickCount, 1/volcaMultiplier) == 0){
        digitalWrite(channels[i].outputPin, HIGH);
        digitalWrite(channels[i].outputPin, LOW);

        channels[i].triggerHigh = true;
        channels[i].lastTrigger = loopMillis;
      }
    
    } else {
      channels[i].triggerHigh = false;
    }

    //Serial.print(channels[i].triggerHigh ? 15 : 5); Serial.print(",");
  }

  // Show MainTick on onboard LED
  if (fmod (tickCount, 1.0) == 0){
    //Serial.print(20); Serial.print(",");
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    //Serial.print(5); Serial.print(",");
    if (timeSinceLastMainTick > 20){
     digitalWrite(LED_BUILTIN, LOW);
    }
  }
}

long getTimeOfNextMainTick(unsigned long tickMillis){
  bool isFirstTick = lastMainTick == -1;
  float largestBPM = mainBPM * largestMultilplier;
  float oneMainTickInMillis = 60000 / largestBPM;
  long nextMainTick = isFirstTick ? tickMillis : (lastMainTick + (int) oneMainTickInMillis);
  return nextMainTick;
}



//   _____ _____ _____ _____ _____ _ _____ _____ 
//  | __  |     |_   _|  _  |_   _|_|     |   | |
//  |    -|  |  | | | |     | | | | |  |  | | | |
//  |__|__|_____| |_| |__|__| |_| |_|_____|_|___|
//

void checkRotationInput(){
    int turnDirection = rotary.rotate();

    switch(modeActive){
      
      case MODE_GLOBAL:
        if (turnDirection == 1){
          mainBPM++;
        }
        if (turnDirection == 2){
          mainBPM--;
          if (mainBPM < 0){mainBPM = 0;}
        }
        break;

      case MODE_CHANNEL:
        if (turnDirection == 1){
          channels[selectedChannel].multiplier *= 2;
          if (channels[selectedChannel].multiplier > largestMultilplier){
            channels[selectedChannel].multiplier = largestMultilplier;
          }
        }
        
        if (turnDirection == 2){
          channels[selectedChannel].multiplier /= 2;
          if (channels[selectedChannel].multiplier < smallestMultiplier){
            channels[selectedChannel].multiplier = smallestMultiplier;
          }
        }
        break;
    }
}



//   _____ _____ _____ _____ _____ _____ 
//  | __  |  |  |_   _|_   _|     |   | |
//  | __ -|  |  | | |   | | |  |  | | | |
//  |_____|_____| |_|   |_| |_____|_|___|
//  

void checkButtonInput(){
  int longPushDelay = 1000;
  int buttonState = digitalRead(4);
  static unsigned long pushedDownTimestamp = 0;
  unsigned long pushDuration = 0;
  
  bool shortPushed = false;
  bool longPushed = false;
  static bool stupidFlagINeed = false;

  if(pushedDownTimestamp != 0){
    // we are waiting for release or longpress, so we need the duration
    pushDuration = millis() - pushedDownTimestamp;
  }

  // just found out the button is pressed
  if (buttonState == 0 && pushedDownTimestamp == 0){
    pushedDownTimestamp = millis();
  }

  // button is pressed for a while now
  if (buttonState == 0 && pushedDownTimestamp != 0){
    longPushed = pushDuration >= longPushDelay;
  }

  // button has just been released
  if (buttonState == 1 && pushedDownTimestamp != 0){
    pushedDownTimestamp = 0;
    shortPushed = (pushDuration < longPushDelay) && (pushDuration > 4);
    stupidFlagINeed = false;
  }
  

  if (longPushed && !stupidFlagINeed){
    modeActive = (modeActive == MODE_CHANNEL ? MODE_GLOBAL : MODE_CHANNEL);
    stupidFlagINeed = true;
  
  } else if (shortPushed){
    switch(modeActive){
      
      case MODE_GLOBAL:
        isPlaying = !isPlaying;
        lastMainTick = -1;
        lastTrigger = -1;
        tickCount = 0;
        break;

      case MODE_CHANNEL:
        selectedChannel = (selectedChannel+1) % NUM_SYNC_OUT;
        break;
    }
  }
}



//   __    _____ ____  _____ 
//  |  |  |   __|    \|   __|
//  |  |__|   __|  |  |__   |
//  |_____|_____|____/|_____|
//
                         
void updateLeds(){

  bool dirty = false;

  for(int i = 0; i < NUM_SYNC_OUT; i++) {
    if (channels[i].triggerHigh && (fmod(tickCount, 1/channels[i].multiplier) == 0)){
      leds[i].red = 255;
      dirty = true;
    }

    if (modeActive == MODE_CHANNEL && selectedChannel == i){
      // if in channel mode set color of selected channel to white
      
      if (leds[i].green == 0 || leds[i].blue == 0){
        // leds[i].red = leds[i].red > 32 ? leds[i].red : 32;
        leds[i].green = 32;
        leds[i].blue = 32;
        dirty = true;
      }
    
    } else {
      // if in other mode set it back to red only
      if (leds[i].green != 0 || leds[i].blue != 0){
        leds[i].green = 0;
        leds[i].blue = 0;
        dirty = true;
      }
    }
  }

  EVERY_N_MILLISECONDS(1000/PIXEL_FPS){
    // fade out red in speed of ticks
    // has to be iterated seperately
    for (int i = 0; i < NUM_SYNC_OUT; i++) {
      if (!channels[i].triggerHigh){
        int newVal = leds[i].red - (mainBPM/2*channels[i].multiplier);
        if (newVal < 0){ newVal = 0; }
        // leds[i].red = newVal > leds[i].blue ? newVal : leds[i].blue;
        leds[i].red = newVal;
        dirty = true; 
      }     
    }
  }

  if (dirty){
    FastLED.show();
  }
}



//   ____  _____ _____ _____ __    _____ __ __ 
//  |    \|     |   __|  _  |  |  |  _  |  |  |
//  |  |  |-   -|__   |   __|  |__|     |_   _|
//  |____/|_____|_____|__|  |_____|__|__| |_|  
//
                                           
void updateDisplay(){
  // TODO: only if "dirty":
  //  if displayed value has changed
  //  if mode has changed

  //EVERY_N_MILLISECONDS(1000/DISPLAY_FPS){
    int contentToDisplay;
    bool showDivisionLines = false;
    const uint8_t divisionLines[] = {0x00, SEG_F};
    const uint8_t divisionLineClear[] = {0x00, 0x00};
    const uint8_t clearSingle[] = {0x00};
  
    switch(modeActive) {
      case MODE_GLOBAL:
      // if in global mode display main bpm
        display.showNumberDec((int) mainBPM);
        break;
      
      case MODE_CHANNEL:
      // if in channel mode display multiplikator of selected channel
        float multiplier = channels[selectedChannel].multiplier;
        
        if (multiplier < 1){
          multiplier = 1 / multiplier;
          showDivisionLines = true;
        }

        if (multiplier < 10){
          display.showNumberDec((int) multiplier, false, 1, 2);
          display.setSegments(clearSingle, 1, 3);          
        } else {
          display.showNumberDec((int) multiplier, false, 2, 2);          
        }

        if (showDivisionLines){
          display.setSegments(divisionLines, 2, 0);
        } else {
          display.setSegments(divisionLineClear, 2, 0);
        }
        break;
    }    
  //}
}
