#include <Arduino.h>
#include "LittleFS.h"

#include "default_environment.h"

#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "timeSync.h"
#include "dashboard.h"
#include <TZ.h>

#include "Encoder.h"

#include <FastLED.h>

#include "AsyncMqttClient.h"

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
    0x1C, 0x42, 0x62, 0x18, 0xC1, 0x36, 0x85, 0x25, 0x73, 0xE4, 0x43, 0x91, 0xFC, 0x68, 0xE8, 0xB2, 0x0A, 0x99, 0xC5, 0x35 \
  }
#endif

Encoder encoder(PIN_ENCODER_1, PIN_ENCODER_2);

CRGB leds[NUM_LEDS];

AsyncMqttClient mqttClient;

int encoderValue = 0;

void setLed(int num) {
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i == num % NUM_LEDS) {
            leds[i] = 0x00FFFFFF;
        } else {
            leds[i] = 0;
        }
    }

    FastLED.show();
}

String getTopicPrefix() {
    return String("/players/asinello") += WiFi.macAddress();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event) {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);

    mqttClient.publish((getTopicPrefix() + "/alive").c_str(), 1, true, "true");
    mqttClient.subscribe("/players/+/ball", 1);
    mqttClient.subscribe("/players/+/alive", 1);
    mqttClient.subscribe((getTopicPrefix() + "/ball/drop").c_str(), 1);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index,
                   size_t total) {
    Serial.println(topic);
    Serial.println(payload);
}

class PhysicalUserInterfaceMode {

public:
    virtual void loop() {};

    virtual void onButtonPress() {
        Serial.println("Button press ignored");
    };

    virtual void onButtonLongPress() {
        Serial.println("Button longpress ignored");
    };
};

PhysicalUserInterfaceMode *encoderPositionPUIMode = nullptr;

PhysicalUserInterfaceMode *brightnessPUIMode = nullptr;

PhysicalUserInterfaceMode *currentPIOMode = nullptr;

class EncoderPositionPhysicalUserInterfaceMode : public PhysicalUserInterfaceMode {

public:
    void loop() override {
        int rawEncoder = encoder.read();
        int newEncoderValue = (rawEncoder + 2) / 4;
        while (newEncoderValue < 0) {
            //TODO: find better approach
            newEncoderValue += NUM_LEDS;
        }

        if (newEncoderValue != encoderValue) {
            dash.data.encoder = encoderValue;

            encoderValue = newEncoderValue;
            setLed(encoderValue);

            // mqttClient.publish("asinello/rotary", 1, true, String(encoderValue).c_str());
        }
    }

    void onButtonPress() override {
        mqttClient.publish((getTopicPrefix() + "/suffix").c_str(), 1, false, String(encoderValue).c_str());
    }

    void onButtonLongPress() override {
        currentPIOMode = brightnessPUIMode;
    }
};

class BrightnessPhysicalUserInterfaceMode : public PhysicalUserInterfaceMode {
private:
    bool initialize = true;

public:
    void loop() override {
        if (initialize) {
            initialize = false;

            for (auto &led : leds) {
                led = 0x00FFFFFF;
            }

            FastLED.show();
        }

        const int encoderDiff = encoder.readAndReset();
        const int brightness = FastLED.getBrightness();
        const int targetBrightness = constrain(FastLED.getBrightness() + encoderDiff, 0, 255);

        if (targetBrightness != brightness) {
            Serial.printf("Update brightness: %d\n", targetBrightness);

            FastLED.setBrightness(targetBrightness);
            FastLED.show();
        }
    }

    void onButtonPress() override {
        if (FastLED.getBrightness() != configManager.data.Brightness) {
            configManager.data.Brightness = FastLED.getBrightness();
            configManager.save();
        }
        initialize = true;
        currentPIOMode = encoderPositionPUIMode;
    }
};

EncoderPositionPhysicalUserInterfaceMode encoderPositionPUIModeBla;
BrightnessPhysicalUserInterfaceMode brighnessPUIModeBla;

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("Disconnected from MQTT.");

    switch (reason) {
        case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
            Serial.println("Bad server fingerprint.");
            break;

        default:
            Serial.println((int8_t) reason);
            break;
    }

    //TODO:
    //dash.data.error = "MQTT disconnected: " + ((int8_t) reason);
}

void onButtonEvent(__attribute__((unused)) ace_button::AceButton *button, uint8_t eventType,
                   __attribute__((unused)) uint8_t buttonState) {
    switch (eventType) {
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


void setupMqtt() {
    mqttClient.setMaxTopicLength(128);
    mqttClient.setServer(configManager.data.mqttHost, configManager.data.mqttPort);
    mqttClient.setCredentials(configManager.data.mqttUser, configManager.data.mqttPassword);
    mqttClient.setSecure(MQTT_SECURE);
    mqttClient.addServerFingerprint((const uint8_t[]) MQTT_SERVER_FINGERPRINT);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
//    static const char * willTopic = (getTopicPrefix() + "/alive").c_str();
//    mqttClient.setWill(willTopic, 1, true, "false");

    mqttClient.connect();
}

void applyConfiguredBrightness() {
    FastLED.setBrightness(configManager.data.Brightness);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");

    pinMode(D4, OUTPUT);
    digitalWrite(D4, LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);


    LittleFS.begin();
    GUI.begin();
    configManager.begin();

    WiFiManager.begin("Asinello");
    timeSync.begin(TZ_Europe_Berlin);
    dash.begin(750);

    CFastLED::addLeds<CHIPSET, PIN_LEDS, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    applyConfiguredBrightness();

    // WiFi.onStationModeGotIP(onWifiConnect);
    // if (WiFi.isConnected())
    // {
    //   mqttClient.connect();
    // }

    setLed(encoderValue);

    encoderPositionPUIMode = &encoderPositionPUIModeBla;
    brightnessPUIMode = &brighnessPUIModeBla;
    currentPIOMode = encoderPositionPUIMode;

    aceButton.getButtonConfig()->setFeature(
            ButtonConfig::kFeatureLongPress | ButtonConfig::kFeatureSuppressAfterLongPress);
    aceButton.setEventHandler(onButtonEvent);

    setupMqtt();

    configManager.setConfigSaveCallback([] {
        applyConfiguredBrightness();
        setupMqtt();
    });
}

void loop() {
    //software interrupts
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    currentPIOMode->loop();

    aceButton.check();
}
