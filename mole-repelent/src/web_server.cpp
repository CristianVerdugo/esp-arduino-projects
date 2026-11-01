#include "web_server.h"
#include "motor_settings.h"
#include "ntp_time.h"
#include "wifi_cc3000.h"

static const unsigned long REQUEST_TIMEOUT_MS = 1000;
static const unsigned long IDLE_CLIENT_TIMEOUT_MS = 5000;
static const unsigned long IDLE_SCAN_INTERVAL_MS = 1000;
static const uint8_t REQUEST_LINE_MAX = 96;
static const uint8_t ALL_FIELDS_SEEN = 0x3F;

static Adafruit_CC3000_Server httpServer(WEB_SERVER_PORT);

static unsigned long openedAt[MAX_SERVER_CLIENTS];
static bool tracked[MAX_SERVER_CLIENTS];

void webServerBegin()
{
    httpServer.begin();
    Serial.println(F("[WEB] Listening on port 80."));
}

// Browsers open speculative connections that never send a request; they would fill all server slots.
static void closeIdleClients()
{
    static unsigned long lastScan = 0;
    unsigned long now = millis();
    if (now - lastScan < IDLE_SCAN_INTERVAL_MS)
    {
        return;
    }
    lastScan = now;

    for (uint8_t i = 0; i < MAX_SERVER_CLIENTS; i++)
    {
        Adafruit_CC3000_ClientRef ref = httpServer.getClientRef(i);
        if (!ref.connected())
        {
            tracked[i] = false;
        }
        else if (!tracked[i])
        {
            tracked[i] = true;
            openedAt[i] = now;
        }
        else if (now - openedAt[i] > IDLE_CLIENT_TIMEOUT_MS && ref.available() == 0)
        {
            ref.close();
            tracked[i] = false;
            Serial.println(F("[WEB] Closed idle connection."));
        }
    }
}

// Maps a 4-character query key to its settings field.
static uint16_t *fieldFor(MotorSettings &s, const char *key)
{
    if (!strncmp_P(key, PSTR("rmin"), 4))
        return &s.runMinS;
    if (!strncmp_P(key, PSTR("rmax"), 4))
        return &s.runMaxS;
    if (!strncmp_P(key, PSTR("pmin"), 4))
        return &s.restMinS;
    if (!strncmp_P(key, PSTR("pmax"), 4))
        return &s.restMaxS;
    if (!strncmp_P(key, PSTR("vmin"), 4))
        return &s.speedMin;
    if (!strncmp_P(key, PSTR("vmax"), 4))
        return &s.speedMax;
    return nullptr;
}

// Returns 0 when the request has no query, 1 when settings were saved, 2 when action performed, -1 when rejected.
static int8_t applyQuery(const char *line, bool truncated)
{
    const char *p = strchr(line, '?');
    if (!p)
    {
        return 0;
    }
    if (truncated)
    {
        return -1;
    }

    p++;

    // Check for action commands
    if (strstr_P(line, PSTR("action=toggle")) != nullptr)
    {
        motorDisabled = !motorDisabled;
        Serial.print(F("[WEB] Motor toggled: "));
        Serial.println(motorDisabled ? F("DISABLED") : F("ENABLED"));
        return 2;
    }

    // Check if this is time config
    if (strstr_P(line, PSTR("sthour")) != nullptr)
    {
        uint8_t startHour = 0, endHour = 0;
        bool hasStart = false, hasEnd = false;

        const char *tempP = p;
        while (*tempP && *tempP != ' ')
        {
            if (!strncmp_P(tempP, PSTR("sthour"), 6))
            {
                const char *eq = strchr(tempP, '=');
                if (eq)
                {
                    char *end;
                    long value = strtol(eq + 1, &end, 10);
                    if (value >= 0 && value <= 23)
                    {
                        startHour = (uint8_t)value;
                        hasStart = true;
                    }
                }
            }
            if (!strncmp_P(tempP, PSTR("endhour"), 7))
            {
                const char *eq = strchr(tempP, '=');
                if (eq)
                {
                    char *end;
                    long value = strtol(eq + 1, &end, 10);
                    if (value >= 0 && value <= 23)
                    {
                        endHour = (uint8_t)value;
                        hasEnd = true;
                    }
                }
            }
            while (*tempP && *tempP != '&' && *tempP != ' ')
            {
                tempP++;
            }
            if (*tempP == '&')
            {
                tempP++;
            }
        }

        if (hasStart && hasEnd && timeConfigSave(startHour, endHour))
        {
            return 1;
        }
        return -1;
    }

    // Check if this is for night settings
    bool isNight = strstr_P(line, PSTR("night=1")) != nullptr;
    
    MotorSettings s = isNight ? motorSettingsNight : motorSettings;
    uint8_t seen = 0;
    while (*p && *p != ' ')
    {
        // Skip the "night=1" parameter
        if (!strncmp_P(p, PSTR("night"), 5))
        {
            while (*p && *p != '&' && *p != ' ')
            {
                p++;
            }
            if (*p == '&')
            {
                p++;
            }
            continue;
        }
        
        const char *eq = strchr(p, '=');
        if (!eq || eq - p != 4)
        {
            return -1;
        }
        uint16_t *field = fieldFor(s, p);
        char *end;
        long value = strtol(eq + 1, &end, 10);
        if (!field || end == eq + 1 || value < 0 || value > 65535)
        {
            return -1;
        }
        *field = (uint16_t)value;
        seen |= 1 << (field - (uint16_t *)&s);

        p = end;
        if (*p == '&')
        {
            p++;
        }
        else if (*p && *p != ' ')
        {
            return -1;
        }
    }

    if (seen != ALL_FIELDS_SEEN || !motorSettingsSave(s, isNight))
    {
        return -1;
    }
    return 1;
}

static void sendField(Adafruit_CC3000_ClientRef &client, const __FlashStringHelper *label,
                      const __FlashStringHelper *name, uint16_t value, uint16_t minValue, uint16_t maxValue)
{
    client.fastrprint(F("<p>"));
    client.fastrprint(label);
    client.fastrprint(F(" <input type=number name="));
    client.fastrprint(name);
    client.fastrprint(F(" value="));
    client.print(value);
    client.fastrprint(F(" min="));
    client.print(minValue);
    client.fastrprint(F(" max="));
    client.print(maxValue);
    client.fastrprintln(F("></p>"));
}

static void sendPage(Adafruit_CC3000_ClientRef &client, int8_t result)
{
    client.fastrprintln(F("HTTP/1.1 200 OK"));
    client.fastrprintln(F("Content-Type: text/html"));
    client.fastrprintln(F("Connection: close"));
    client.fastrprintln(F(""));
    client.fastrprintln(F("<!DOCTYPE html><html><head><meta name=viewport content=\"width=device-width\">"));
    client.fastrprintln(F("<title>Mole Repellent</title>"));
    client.fastrprintln(F("<style>"));
    client.fastrprintln(F("body{font:12px Arial}"));
    client.fastrprintln(F("button{padding:8px 15px;margin:3px;cursor:pointer;border:1px solid #999}"));
    client.fastrprintln(F(".tabBtn{background:#ccc}.tabBtn.active{background:#4CAF50;color:white}"));
    client.fastrprintln(F(".tab{display:none}.tab.show{display:block}"));
    client.fastrprintln(F("</style></head><body><h1>Mole Repellent</h1>"));

    char timeText[32];
    berlinTimeString(timeText, sizeof(timeText));
    client.fastrprint(F("<p><strong>Time in Adelsdorf:</strong> "));
    client.fastrprint(timeText);
    client.fastrprintln(F("</p>"));

    // Motor status
    client.fastrprint(F("<p><strong>Motor:</strong> "));
    client.fastrprintln(motorDisabled ? F("DISABLED</strong></p>") : F("ENABLED</strong></p>"));

    // Result message
    if (result == 1)
    {
        client.fastrprintln(F("<p style='color:green'><b>Settings Saved.</b></p>"));
    }
    else if (result == 2)
    {
        client.fastrprintln(F("<p style='color:blue'><b>Action completed.</b></p>"));
    }
    else if (result < 0)
    {
        client.fastrprint(F("<p style='color:red'><b>Rejected.</b> Run "));
        client.print(MOTOR_RUN_MIN_S);
        client.fastrprint(F("-"));
        client.print(MOTOR_TIME_MAX_S);
        client.fastrprint(F(" s, rest "));
        client.print(MOTOR_REST_MIN_S);
        client.fastrprint(F("-"));
        client.print(MOTOR_TIME_MAX_S);
        client.fastrprint(F(" s, speed "));
        client.print(MOTOR_SPEED_MIN);
        client.fastrprint(F("-"));
        client.print(MOTOR_SPEED_MAX);
        client.fastrprintln(F(", min must not exceed max.</p>"));
    }

    // Control buttons
    client.fastrprintln(F("<div style='margin:10px 0;'>"));
    client.fastrprint(F("<a href='/?action=toggle' style='text-decoration:none'><button type='button' class='actionBtn'>"));
    client.fastrprint(motorDisabled ? F("Enable Motor") : F("Disable Motor"));
    client.fastrprintln(F("</button></a>"));
    client.fastrprintln(F("</div>"));

    // Tab buttons
    client.fastrprintln(F("<div style='margin:15px 0;border-bottom:2px solid #ddd;'>"));
    client.fastrprintln(F("<button class='tabBtn active' onclick='showTab(0)'>Time Settings</button>"));
    client.fastrprintln(F("<button class='tabBtn' onclick='showTab(1)'>Day Settings</button>"));
    client.fastrprintln(F("<button class='tabBtn' onclick='showTab(2)'>Night Settings</button>"));
    client.fastrprintln(F("</div>"));

    // Time settings tab
    client.fastrprintln(F("<div id='tab0' class='tab show'>"));
    client.fastrprintln(F("<h2>Time Configuration</h2>"));
    client.fastrprintln(F("<p>Configure when Mode 2 (Night) activates</p>"));
    client.fastrprintln(F("<form>"));
    client.fastrprint(F("<p>Night Mode Starts at: <input type=number name=sthour value="));
    client.print(timeConfig.nightStartHour);
    client.fastrprintln(F(" min=0 max=23> (0-23 hours)</p>"));
    client.fastrprint(F("<p>Night Mode Ends at: <input type=number name=endhour value="));
    client.print(timeConfig.nightEndHour);
    client.fastrprintln(F(" min=0 max=23> (0-23 hours)</p>"));
    client.fastrprintln(F("<p><small>Example: Start=20, End=8 means 8 PM to 8 AM</small></p>"));
    client.fastrprintln(F("<p><input type=submit value='Save Time Settings'></p></form>"));
    client.fastrprintln(F("</div>"));

    // Day settings tab (Mode 1)
    client.fastrprintln(F("<div id='tab1' class='tab'>"));
    client.fastrprintln(F("<h2>Day Settings</h2>"));
    client.fastrprint(F("<p>Active when time is between "));
    client.print(timeConfig.nightEndHour);
    client.fastrprint(F(" and "));
    client.print(timeConfig.nightStartHour);
    client.fastrprintln(F(" (outside night hours)</p>"));
    client.fastrprintln(F("<form>"));
    sendField(client, F("Run min (s)"), F("rmin"), motorSettings.runMinS, MOTOR_RUN_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Run max (s)"), F("rmax"), motorSettings.runMaxS, MOTOR_RUN_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Rest min (s)"), F("pmin"), motorSettings.restMinS, MOTOR_REST_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Rest max (s)"), F("pmax"), motorSettings.restMaxS, MOTOR_REST_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Speed min (PWM)"), F("vmin"), motorSettings.speedMin, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    sendField(client, F("Speed max (PWM)"), F("vmax"), motorSettings.speedMax, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    client.fastrprintln(F("<input type=hidden name=night value=0>"));
    client.fastrprintln(F("<p><input type=submit value='Save Day Settings' style='background:#4CAF50;color:white'></p></form>"));
    client.fastrprintln(F("</div>"));

    // Night settings tab (Mode 2)
    client.fastrprintln(F("<div id='tab2' class='tab'>"));
    client.fastrprintln(F("<h2>Night Settings</h2>"));
    client.fastrprint(F("<p>Active when time is between "));
    client.print(timeConfig.nightStartHour);
    client.fastrprint(F(" and "));
    client.print(timeConfig.nightEndHour);
    client.fastrprintln(F(" (during night hours)</p>"));
    client.fastrprintln(F("<p><i>Typically: Less frequent activation for nighttime hours</i></p>"));
    client.fastrprintln(F("<form>"));
    sendField(client, F("Run min (s)"), F("rmin"), motorSettingsNight.runMinS, MOTOR_RUN_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Run max (s)"), F("rmax"), motorSettingsNight.runMaxS, MOTOR_RUN_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Rest min (s)"), F("pmin"), motorSettingsNight.restMinS, MOTOR_REST_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Rest max (s)"), F("pmax"), motorSettingsNight.restMaxS, MOTOR_REST_MIN_S, MOTOR_TIME_MAX_S);
    sendField(client, F("Speed min (PWM)"), F("vmin"), motorSettingsNight.speedMin, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    sendField(client, F("Speed max (PWM)"), F("vmax"), motorSettingsNight.speedMax, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    client.fastrprintln(F("<input type=hidden name=night value=1>"));
    client.fastrprintln(F("<p><input type=submit value='Save Night Settings' style='background:#4CAF50;color:white'></p></form>"));
    client.fastrprintln(F("</div>"));

    client.fastrprintln(F("<script>function showTab(i){"));
    client.fastrprintln(F("var tabs=document.querySelectorAll('.tab');"));
    client.fastrprintln(F("var btns=document.querySelectorAll('.tabBtn');"));
    client.fastrprintln(F("for(var j=0;j<tabs.length;j++){tabs[j].classList.remove('show')}"));
    client.fastrprintln(F("for(var j=0;j<btns.length;j++){btns[j].classList.remove('active')}"));
    client.fastrprintln(F("tabs[i].classList.add('show');"));
    client.fastrprintln(F("btns[i].classList.add('active');}"));
    client.fastrprintln(F("</script></body></html>"));
}

void webServerHandle()
{
    closeIdleClients();

    Adafruit_CC3000_ClientRef client = httpServer.available();
    if (!client)
    {
        return;
    }

    char line[REQUEST_LINE_MAX];
    uint8_t lineLen = 0;
    bool inFirstLine = true;
    bool truncated = false;

    // Consume the request up to the blank line that ends the headers
    uint8_t blankRun = 0;
    unsigned long start = millis();
    while (client.connected() && millis() - start < REQUEST_TIMEOUT_MS)
    {
        if (!client.available())
        {
            continue;
        }
        char c = client.read();

        if (inFirstLine)
        {
            if (c == '\n')
            {
                inFirstLine = false;
            }
            else if (c != '\r')
            {
                if (lineLen < REQUEST_LINE_MAX - 1)
                {
                    line[lineLen++] = c;
                }
                else
                {
                    truncated = true;
                }
            }
        }

        if (c == '\n' && blankRun >= 1)
        {
            break;
        }
        if (c == '\n')
        {
            blankRun = 1;
        }
        else if (c != '\r')
        {
            blankRun = 0;
        }
    }
    line[lineLen] = '\0';

    int8_t result = applyQuery(line, truncated);
    
    // Redirect after action to clear URL parameters (prevents toggle on refresh)
    if (result == 2)
    {
        client.fastrprintln(F("HTTP/1.1 302 Found"));
        client.fastrprintln(F("Location: /"));
        client.fastrprintln(F("Connection: close"));
        client.fastrprintln(F(""));
    }
    else
    {
        sendPage(client, result);
    }

    delay(100); // Let the CC3000 flush the response before closing
    client.close();
    Serial.println(F("[WEB] Request served."));
}
