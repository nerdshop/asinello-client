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

#include "AsyncMqttClient.h"

#include "localproperties.h"

#include "string"

#define COLOR_ORDER GRB
#define CHIPSET WS2812B
#define NUM_LEDS 12

#if ASYNC_TCP_SSL_ENABLED
#define MQTT_SECURE true
#endif

Encoder encoder(D1, D2);

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


void onWifiConnect(const WiFiEventStationModeGotIP& event) {
    mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
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

  setLed(encoderValue);

  WiFi.onStationModeGotIP(onWifiConnect);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
  mqttClient.setSecure(MQTT_SECURE);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.connect();
}

void loop()
{
  //software interrupts
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();

  if (FastLED.getBrightness() != configManager.data.Brightness)
  {
    FastLED.setBrightness(configManager.data.Brightness);
    FastLED.show();
  }
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
  }

}

