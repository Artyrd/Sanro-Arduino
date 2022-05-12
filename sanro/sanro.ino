/***************************************************************
 *                                                             *
 *                Taiko Sanro - Arduino                        *
 *   Support Arduino models with ATmega32u4 microprocessors    *
 *                                                             *
 *          Shiky Chang                    Chris               *
 *     zhangxunpx@gmail.com          wisaly@gmail.com          *
 *                                                             *
 ***************************************************************/

#include <limits.h>
#include <Keyboard.h>
#include "cache.h"

//Taiko Jiro by default, Sanrio if defined
//#define MODE_SANRO

// #define ENABLE_NS_JOYSTICK // Nintendo Switch compatability, 0 otherwise
// #define ENABLE_KEYBOARD // Enables Keyboard, not needed if you toggle

// enable toggling between keyboard and nintendo switch
#define ENABLE_TOGGLE 1 // 0 if no toggle, 1 if toggle
#define TOGGLEPIN 7

#define MODE_DEBUG 0

#define CHANNELS 4

// Caches for the soundwave and power
//#define SAMPLE_CACHE_LENGTH 12
#define SAMPLE_CACHE_LENGTH 20
#define POWER_CACHE_LENGTH 5

// Light and heacy hit thresholds
#define LIGHT_THRES 5000

#if MODE_JIRO // disable heavy hits
  #define HEAVY_THRES LONG_MAX
#else
  #define HEAVY_THRES 20000
#endif

long int thresh[4] = {5000, 5000, 5000, 5000};

// Forced sampling frequency
#define FORCED_FREQ 1000
long int cycleTime = 1000000 / FORCED_FREQ;

// bool enable_keyboard = false;
bool mode_switch = true;

unsigned long int lastTime;

int channelSample [CHANNELS];
int lastChannelSample [CHANNELS];
Cache <int, SAMPLE_CACHE_LENGTH> sampleCache [CHANNELS];

long int frame = 0;
int frame_cycle = 10;

long int power [CHANNELS];
Cache <long int, POWER_CACHE_LENGTH> powerCache [CHANNELS];

bool triggered [CHANNELS];
bool ns_inputted = false;

bool printed[CHANNELS];

int pins[] = {A0, A3, A1, A2};  // L ka, R ka, L don, R don
char lightKeys[] = {'e', 'i', 'f', 'j'}; // L ka, R ka, L don, R don

#ifdef MODE_SANRO
char heavyKeys[] = {'r', 'g', 'h', 'u'};
#endif

bool down[4] = {false, false, false, false};

// #ifdef ENABLE_NS_JOYSTICK
#include "Joystick.h"
//const int sensor_button[4] = {SWITCH_BTN_ZL, SWITCH_BTN_ZR, SWITCH_BTN_B, SWITCH_BTN_A};
//   taiko force layout:  <  >  B  A
const int sensor_button[4] = {SWITCH_BTN_NONE, SWITCH_BTN_A, SWITCH_BTN_NONE, SWITCH_BTN_B}; // L ka, R ka, L don, R don 
const int sensor_hat[4] = {SWITCH_HAT_L, 0, SWITCH_HAT_R, 0};
uint8_t down_count[4] = {0, 0, 0, 0};
// #endif

bool newlinePrinted = false;

void setup() {
  Serial.begin (9600);
  #ifdef ENABLE_TOGGLE
    pinMode(TOGGLEPIN, INPUT_PULLUP);
    if (digitalRead(TOGGLEPIN) == true) {
      mode_switch = true;
    } else {
      mode_switch = false;
    }
    attachInterrupt(digitalPinToInterrupt(TOGGLEPIN), button_ISR, CHANGE);
  #endif

  Keyboard.begin ();
  analogReference (DEFAULT);
  //analogSwitchPin(pins[0]);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  for (short int i = 0; i < CHANNELS; i++) {
    power [i] = 0;
    lastChannelSample [i] = 0;
    triggered [i] = false;
  }
  lastTime = 0;
  /*
  if (mode_switch) {
    for (int i = 0; i < 8; ++i) pinMode(i, INPUT_PULLUP);
  }
  */
}

void loop() {
  
  for (short int i = 0; i < CHANNELS; i++) {
    
    channelSample[i] = analogRead(pins [i]);
    sampleCache[i].put(channelSample[i] - lastChannelSample[i]);
    
    long int tempInt;
    tempInt = sampleCache[i].get (-10); // get the oldest reading (12th)
    power [i] -= tempInt * tempInt;
    tempInt = sampleCache[i].get (); // get the newest reading
    power [i] += tempInt * tempInt;
    if (power [i] < LIGHT_THRES) {
      power [i] = 0;
    }
    powerCache [i].put (power [i]);
    //lastChannelSample [i] = channelSample [i];
    //if (powerCache [i].get (1) == 0) {
    if (powerCache [i].get (-1) == 0) {
      triggered [i] = false;
      printed[i] = false;
    }

    if (!triggered [i]) {
      for (short int j = 0; j < POWER_CACHE_LENGTH - 1; j++) {
        if (powerCache[i].get (j - 1) >= powerCache[i].get (j)) {
          break;
        // heavy hits don't register in taiko jiro mode
#ifdef MODE_SANRO
        } else if (powerCache [i].get (1) >= HEAVY_THRES) {
          triggered [i] = true;
          Keyboard.print (heavyKeys[i]);
          break;
#endif
        } else if (powerCache [i].get(-1) >= thresh[i]) {
          // disable don hit if same-side ka is stronger
          if (i == 2 && powerCache[2].get(-1) < powerCache[0].get(-1)) {
            break;
          } else if (i == 3 && powerCache[3].get(-1) < powerCache[1].get(-1)) {
            break;
          }
          // if (!mode_switch) {
          //   Keyboard.print (lightKeys[i]);
          // }
          triggered [i] = true;
          printed[i] = true;
          
          // #ifdef ENABLE_NS_JOYSTICK
          if (down_count[i] <= 2) {
            down_count[i] += 1;
          }
          // #endif

          break;
        }
      }
    }
    // End of each channel
  }

  // #ifdef ENABLE_NS_JOYSTICK
  frame++;
  if (frame > frame_cycle) {
    if (down_count[0] || down_count[1] || down_count[2] || down_count[3]) {
      frame = 0;
      // Serial.print(String(down_count[0]) + " " + String(down_count[1]) + " " + String(down_count[2]) + " " + String(down_count[3]) + "\n");
      Joystick.HAT = 0;
      for (int i = 0; i < 4; i++) {
        // bool state = (down_count[i] & 1);
        bool state = (bool) down_count[i];
        Joystick.Button |= (state ? sensor_button[i] : SWITCH_BTN_NONE);
        Joystick.HAT |= (state ? sensor_hat[i] : 0);
        down_count[i] -= (bool)down_count[i];
        if (state && !mode_switch) {
          Keyboard.print (lightKeys[i]);

        }
      }
      if (Joystick.HAT == 0) { 
        Joystick.HAT = SWITCH_HAT_CENTER;
      }
      if (mode_switch) {
        Joystick.sendState();
        Serial.print("Button:" + (String)Joystick.Button + " HAT:" + (String)Joystick.HAT +"\n");
      }
      
      Joystick.Button = SWITCH_BTN_NONE;
      Joystick.HAT = SWITCH_HAT_CENTER;;
    } else if (frame > frame_cycle * 4 && mode_switch) {
      frame = 0;
      Joystick.sendState();
    }
  }
  //#endif

  #if MODE_DEBUG
    if (power[0] || power[1] || power[2] || power[3] || true) {
      Serial.print(String(power[0]) + "\t" + String(power[1]) + "\t" + String(power[2]) + "\t" + String(power[3]));
      Serial.print("\t | ");
      Serial.print(String(channelSample[0]) + "\t" + String(channelSample[1]) + "\t" + String(channelSample[2]) + "\t" + String(channelSample[3]));
      //Serial.println(String(channelSample[0]-lastChannelSample[0]) + "\t" + String(channelSample[1]-lastChannelSample[1]) + "\t" + String(channelSample[2]-lastChannelSample[2]) + "\t" + String(channelSample[3]-lastChannelSample[3]));
      Serial.print("\t | ");
      Serial.print(String(printed[0]) + "\t" + String(printed[1]) + "\t" + String(printed[2]) + "\t" + String(printed[3]));
      Serial.print("\n");
      newlinePrinted = false;
    } else if (!newlinePrinted) {
      Serial.println("\n");
      newlinePrinted = true;
    }
  #endif
 
  for (int i = 0; i < CHANNELS; i++) {
    lastChannelSample [i] = channelSample [i];
  }

  // Force the sample frequency to be less than 1000Hz
  unsigned int frameTime = micros () - lastTime;
  if (frameTime < cycleTime) {
    delayMicroseconds (cycleTime - frameTime);
  } else {
    // Performance bottleneck;
    //Serial.println ("Exception: forced frequency is too high for the microprocessor to catch up.");
  }
  lastTime = micros ();
}

#ifdef ENABLE_TOGGLE
void button_ISR(){
  if (digitalRead(TOGGLEPIN) == true) {
    mode_switch = true;
  } else {
    mode_switch = false;
  }
}
#endif
