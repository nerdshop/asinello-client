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


Encoder encoder(D1, D2);

void setup() 
{
    Serial.begin(115200);

    LittleFS.begin();
    GUI.begin();
    configManager.begin();
    WiFiManager.begin(configManager.data.projectName);
    timeSync.begin(TZ_Europe_Berlin);
    dash.begin(750);

    Serial.println("Hello world");
}

void loop() 
{
    //software interrupts
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    dash.data.encoder = encoder.read();

}