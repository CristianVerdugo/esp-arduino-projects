#include "motor_settings.h"
#include "ntp_time.h"
#include <EEPROM.h>
#include <avr/wdt.h>

static const uint8_t SETTINGS_MAGIC = 0xA8;
static const int SETTINGS_ADDRESS = 0;

struct StoredSettings
{
    uint8_t magic;
    MotorSettings daySettings;
    MotorSettings nightSettings;
    TimeConfig timeConfig;
};

MotorSettings motorSettings = {2, 6, 20, 40, 200, 255};
MotorSettings motorSettingsNight = {4, 10, 600, 900, 200, 255}; // Run 4-10s, rest 10-15 min (600-900s)
TimeConfig timeConfig = {22, 8}; // Default: 10 PM to 8 AM
bool motorDisabled = false;

bool motorSettingsValid(const MotorSettings &s)
{
    return s.runMinS >= MOTOR_RUN_MIN_S && s.runMinS <= s.runMaxS && s.runMaxS <= MOTOR_TIME_MAX_S &&
           s.restMinS >= MOTOR_REST_MIN_S && s.restMinS <= s.restMaxS && s.restMaxS <= MOTOR_TIME_MAX_S &&
           s.speedMin >= MOTOR_SPEED_MIN && s.speedMin <= s.speedMax && s.speedMax <= MOTOR_SPEED_MAX;
}

void motorSettingsLoad()
{
    StoredSettings stored;
    EEPROM.get(SETTINGS_ADDRESS, stored);
    
    // Check if EEPROM has valid data with correct magic number
    if (stored.magic == SETTINGS_MAGIC && motorSettingsValid(stored.daySettings) && motorSettingsValid(stored.nightSettings))
    {
        motorSettings = stored.daySettings;
        motorSettingsNight = stored.nightSettings;
        timeConfig = stored.timeConfig;
        Serial.println(F("[CFG] Loaded settings from EEPROM."));
    }
    else
    {
        // Invalid or old format - force write clean defaults to EEPROM
        Serial.println(F("[CFG] EEPROM invalid, resetting to defaults."));
        
        // Create clean stored settings with defaults
        StoredSettings clean = {SETTINGS_MAGIC, motorSettings, motorSettingsNight, timeConfig};
        EEPROM.put(SETTINGS_ADDRESS, clean);
        
        Serial.println(F("[CFG] Defaults written to EEPROM."));
    }
}

bool motorSettingsSave(const MotorSettings &s, bool isNight)
{
    if (!motorSettingsValid(s))
    {
        return false;
    }

    if (isNight)
    {
        motorSettingsNight = s;
    }
    else
    {
        motorSettings = s;
    }

    StoredSettings stored = {SETTINGS_MAGIC, motorSettings, motorSettingsNight, timeConfig};
    EEPROM.put(SETTINGS_ADDRESS, stored);
    Serial.println(isNight ? F("[CFG] Night settings saved.") : F("[CFG] Day settings saved."));
    return true;
}

bool timeConfigSave(uint8_t startHour, uint8_t endHour)
{
    // Validate hours are within 0-23
    if (startHour > 23 || endHour > 23)
    {
        return false;
    }

    timeConfig.nightStartHour = startHour;
    timeConfig.nightEndHour = endHour;

    StoredSettings stored = {SETTINGS_MAGIC, motorSettings, motorSettingsNight, timeConfig};
    EEPROM.put(SETTINGS_ADDRESS, stored);
    Serial.print(F("[CFG] Time config saved: Night from "));
    Serial.print(startHour);
    Serial.print(F(" to "));
    Serial.print(endHour);
    Serial.println(F("."));
    return true;
}

bool isNightMode()
{
#if !NETWORK_ENABLED
    return false;
#endif
    uint8_t hour = 0;
    if (!getHourBerlin(hour))
    {
        return false;
    }

    // If nightStartHour < nightEndHour: night is in middle of day (unusual, but supported)
    // If nightStartHour > nightEndHour: night spans midnight (normal case like 20-8)
    if (timeConfig.nightStartHour < timeConfig.nightEndHour)
    {
        // Night is e.g. 12-16 (noon to 4 PM)
        return hour >= timeConfig.nightStartHour && hour < timeConfig.nightEndHour;
    }
    else
    {
        // Night is e.g. 20-8 (8 PM to 8 AM, spanning midnight)
        return hour >= timeConfig.nightStartHour || hour < timeConfig.nightEndHour;
    }
}

void softReset()
{
    Serial.println(F("[SYS] Soft reset initiated..."));
    delay(100);
    // Trigger watchdog reset
    wdt_enable(WDTO_15MS);
    while (1)
    {
    }
}
