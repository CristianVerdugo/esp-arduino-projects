#pragma once

#include <Arduino.h>

#define MOTOR_RUN_MIN_S 2
#define MOTOR_REST_MIN_S 20
#define MOTOR_TIME_MAX_S 3600
#define MOTOR_SPEED_MIN 150
#define MOTOR_SPEED_MAX 255

// All fields are uint16_t so the struct can be indexed as an array.
struct MotorSettings
{
    uint16_t runMinS;
    uint16_t runMaxS;
    uint16_t restMinS;
    uint16_t restMaxS;
    uint16_t speedMin; // PWM
    uint16_t speedMax; // PWM
};

// Time configuration struct
struct TimeConfig
{
    uint8_t nightStartHour; // Hour when night mode starts (0-23)
    uint8_t nightEndHour;   // Hour when night mode ends (0-23)
};

extern MotorSettings motorSettings;      // Day settings (Mode 1)
extern MotorSettings motorSettingsNight; // Night settings (Mode 2)
extern TimeConfig timeConfig;            // Start/end hours for night mode
extern bool motorDisabled;               // When true, motor vibrations are disabled

// Loads settings from EEPROM, falling back to defaults when none valid are stored.
void motorSettingsLoad();

// True when every value is inside its MOTOR_* limits and each min <= max.
bool motorSettingsValid(const MotorSettings &s);

// Validates, persists to EEPROM and applies. Returns false (no change) if invalid.
bool motorSettingsSave(const MotorSettings &s, bool isNight = false);

// Save time configuration (night start/end hours)
bool timeConfigSave(uint8_t startHour, uint8_t endHour);

// Returns true if current time is within night hours
bool isNightMode();

// Software reset function
void softReset();
