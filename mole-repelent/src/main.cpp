#include <Arduino.h>
#include "motor.h"
#include "motor_settings.h"
#include "ntp_time.h"
#include "web_server.h"
#include "wifi_cc3000.h"

void setup()
{
    Serial.begin(115200);

    motorSettingsLoad();
    motorSetup();

#if NETWORK_ENABLED
    if (wifiConnect())
    {
        webServerBegin();
    }
    else
    {
        Serial.println(F("[WIFI] Not connected."));
    }
#endif
}

void loop()
{
    motorUpdate();

#if NETWORK_ENABLED
    if (wifiReconnectIfLost())
    {
        webServerBegin();
    }

    if (wifiOnline())
    {
        ntpUpdate();
        webServerHandle();
    }
#endif
}
