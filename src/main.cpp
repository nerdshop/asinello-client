#include <Arduino.h>
#include "LittleFS.h"

#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "fetch.h"
#include "configManager.h"
#include "timeSync.h"
#include "dashboard.h"
#include <TZ.h>

#include "Encoder.h"

#include <FastLED.h>

#define COLOR_ORDER GRB
#define CHIPSET WS2812B
#define NUM_LEDS 12

Encoder encoder(D1, D2);

CRGB leds[NUM_LEDS];

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");

  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  WiFiManager.begin(configManager.data.projectName);
  timeSync.begin(TZ_Europe_Berlin);
  dash.begin(750);

  FastLED.addLeds<CHIPSET, D7, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  Serial.println("Hello world");
}

int encoderValue = 0;

void loop()
{
  //software interrupts
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();
  
  if (FastLED.getBrightness() != configManager.data.Brightness) {
    FastLED.setBrightness(configManager.data.Brightness);
    FastLED.show();
  }

  int newEncoderValue = (encoder.read() + 2) / 4;
  while (newEncoderValue < 0) {
    //TODO: find better approach
    newEncoderValue += NUM_LEDS;
  }

  if (newEncoderValue != encoderValue)
  {
    encoderValue = newEncoderValue;
    dash.data.encoder = encoderValue;

    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (i == encoderValue % NUM_LEDS)
      {
        leds[i] = 0x00FFFFFF;
      }
      else
      {
        leds[i] = 0;
      }
    }

    FastLED.show();
  }
}