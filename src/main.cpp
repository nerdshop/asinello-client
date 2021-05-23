#include <Arduino.h>
#include "LittleFS.h"

#include "default_environment.h"

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

#include "AsyncMqttClient.h"

#include "localproperties.h"

#include "string"

#include <AceButton.h>
using namespace ace_button;

#define COLOR_ORDER GRB
#define CHIPSET WS2812B
#define NUM_LEDS 12

#if ASYNC_TCP_SSL_ENABLED
#define MQTT_SECURE true
#define MQTT_SERVER_FINGERPRINT                                                                                            \
  {                                                                                                                        \
    0xCA, 0xEB, 0x9E, 0xE7, 0x60, 0x47, 0x13, 0x4C, 0x2F, 0x56, 0xB9, 0x79, 0x70, 0x9D, 0x48, 0x18, 0xCE, 0x07, 0x31, 0xA7 \
  }
#endif

Encoder encoder(D2, D1);

CRGB leds[NUM_LEDS];

AsyncMqttClient mqttClient;

int encoderValue = 0;

void setLed(int num)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i == num % NUM_LEDS)
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

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe("test/lol", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
  mqttClient.publish("test/lol", 0, true, "test 1");
  Serial.println("Publishing at QoS 0");
  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
  Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);
  uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
  Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2);
}

class PhysicalUserInterfaceMode
{

public:
  virtual void loop(){};
  virtual void onButtonPress()
  {
    Serial.println("Button press ignored");
  };
  virtual void onButtonLongPress()
  {
    Serial.println("Button longpress ignored");
  };
};

PhysicalUserInterfaceMode *encoderPositionPUIMode = 0;

PhysicalUserInterfaceMode *brightnessPUIMode = 0;

PhysicalUserInterfaceMode *currentPIOMode = 0;

class EncoderPositionPhysicalUserInterfaceMode : public PhysicalUserInterfaceMode
{

public:
  void loop() override
  {
    int rawEncoder = encoder.read();
    int newEncoderValue = (rawEncoder + 2) / 4;
    while (newEncoderValue < 0)
    {
      //TODO: find better approach
      newEncoderValue += NUM_LEDS;
    }

    if (newEncoderValue != encoderValue)
    {
      dash.data.encoder = encoderValue;

      encoderValue = newEncoderValue;
      setLed(encoderValue);

      // mqttClient.publish("asinello/rotary", 1, true, String(encoderValue).c_str());
    }
  }

  void onButtonLongPress() override
  {
    currentPIOMode = brightnessPUIMode;
  }
};

class BrightnessPhysicalUserInterfaceMode : public PhysicalUserInterfaceMode
{
private:
  bool initialize = true;

public:
  void loop() override
  {
    if (initialize)
    {
      initialize = false;

      for (int i = 0; i < NUM_LEDS; i++)
      {
        leds[i] = 0x00FFFFFF;
      }

      FastLED.show();
    }

    const int encoderDiff = encoder.readAndReset();
    const int brightness = FastLED.getBrightness();
    const int targetBrightness = constrain(FastLED.getBrightness() + encoderDiff, 0, 255);

    if (targetBrightness != brightness)
    {
      Serial.printf("Update brightness: %d\n", targetBrightness);

      FastLED.setBrightness(targetBrightness);
      FastLED.show();
    }
  }

  void onButtonPress()
  {
    if (FastLED.getBrightness() != configManager.data.Brightness)
    {
      configManager.data.Brightness = FastLED.getBrightness();
      configManager.save();
    }
    initialize = true;
    currentPIOMode = encoderPositionPUIMode;
  }
};

EncoderPositionPhysicalUserInterfaceMode encoderPositionPUIModeBla;
BrightnessPhysicalUserInterfaceMode brighnessPUIModeBla;

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  switch (reason)
  {
  case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
    Serial.println("Bad server fingerprint.");
    break;

  default:
    Serial.println((int8_t)reason);
    break;
  }
}

void onButtonEvent(ace_button::AceButton *button, uint8_t eventType,
                   uint8_t buttonState)
{
  switch (eventType)
  {
  case AceButton::kEventReleased:
    currentPIOMode->onButtonPress();
    break;

  case AceButton::kEventLongPressed:
    currentPIOMode->onButtonLongPress();
    break;

  default:
    break;
  }
}

ace_button::AceButton aceButton(PIN_BUTTON);

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(D4, OUTPUT);
  digitalWrite(D4, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
  mqttClient.setSecure(MQTT_SECURE);
  mqttClient.addServerFingerprint((const uint8_t[])MQTT_SERVER_FINGERPRINT);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);

  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  WiFiManager.begin(configManager.data.projectName);
  timeSync.begin(TZ_Europe_Berlin);
  dash.begin(750);

  FastLED.addLeds<CHIPSET, D7, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(configManager.data.Brightness);

  // WiFi.onStationModeGotIP(onWifiConnect);
  // if (WiFi.isConnected())
  // {
  //   mqttClient.connect();
  // }

  setLed(encoderValue);

  encoderPositionPUIMode = &encoderPositionPUIModeBla;
  brightnessPUIMode = &brighnessPUIModeBla;
  currentPIOMode = encoderPositionPUIMode;

  aceButton.getButtonConfig()->setFeature(ButtonConfig::kFeatureLongPress | ButtonConfig::kFeatureSuppressAfterLongPress);
  aceButton.setEventHandler(onButtonEvent);
}

void loop()
{
  //software interrupts
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();

  currentPIOMode->loop();

  aceButton.check();
}
