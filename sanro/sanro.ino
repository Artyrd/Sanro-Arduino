/***************************************************************
 *                                                             *
 *                Taiko Sanro - Arduino                        *
 *   Support Arduino models with ATmega32u4 microprocessors    *
 *                                                             *
 *          Shiky Chang                    Chris               *
 *     zhangxunpx@gmail.com          wisaly@gmail.com          *
 *                                                             *
 ***************************************************************/

// Compatibility with Taiko Jiro
#define MODE_JIRO 1

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

#include <limits.h>
#include <Keyboard.h>
#include "cache.h"



unsigned long int lastTime;

int channelSample [CHANNELS];
int lastChannelSample [CHANNELS];
Cache <int, SAMPLE_CACHE_LENGTH> sampleCache [CHANNELS];

long int power [CHANNELS];
long int lastPower [CHANNELS];
Cache <long int, POWER_CACHE_LENGTH> powerCache [CHANNELS];

bool triggered [CHANNELS];

bool printed[CHANNELS];

// int pins[] = {A0, A1, A2, A3};  // L don, R don, L kat, R kat
// char lightKeys[] = {'g', 'h', 'f', 'j'};
// char heavyKeys[] = {'t', 'y', 'r', 'u'};
int pins[] = {A0, A3, A1, A2};  // L ka, R ka, L don, R don
char lightKeys[] = {'e', 'i', 'f', 'j'}; // L ka, R ka, L don, R don 
char heavyKeys[] = {'r', 'g', 'h', 'u'};

bool newlinePrinted = false;

void setup() {
  Serial.begin (9600);
  Keyboard.begin ();
  analogReference (DEFAULT);
  for (short int i = 0; i < CHANNELS; i++) {
    power [i] = 0;
    lastPower [i] = 0;
    lastChannelSample [i] = 0;
    triggered [i] = false;
  }
  lastTime = 0;
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
    //if (power [i] < LIGHT_THRES || power[i] < lastPower[i]) {
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
        } else if (powerCache [i].get (1) >= HEAVY_THRES) {
          triggered [i] = true;
          Keyboard.print (heavyKeys[i]);
          break;
        //} else if (powerCache [i].get (1) >= LIGHT_THRES) {
        } else if (powerCache [i].get(-1) >= thresh[i]) {
          // disable don hit if same-side ka is stronger
          if (i == 2 && powerCache[2].get(-1) < powerCache[0].get(-1)) {
            break;
          } else if (i == 3 && powerCache[3].get(-1) < powerCache[1].get(-1)) {
            break;
          }
          triggered [i] = true;
          Keyboard.print (lightKeys[i]);
          printed[i] = true;
          break;
        }
      }
    }
    
    #if MODE_DEBUG
//        Serial.print (power [i]);
//        Serial.print ("\t");
    #endif

    // End of each channel
  }

  #if MODE_DEBUG
//    Serial.print (50000);
//    Serial.print ("\t");
//    Serial.print (0);
//    Serial.print ("\t");
//    Serial.println ("");
    
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
    //Serial.println ("cycle time:" + String(cycleTime));
    //Serial.println(frameTime);
  }
  lastTime = micros ();
}
