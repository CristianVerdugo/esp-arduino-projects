#include "motor.h"

#if MOTOR_ENABLED

#include "motor_settings.h"

static const int MOTOR_RPWM_PIN = 2; // Forward speed PWM
static const int MOTOR_LPWM_PIN = 4; // Reverse speed PWM, unused, held low
static const int MOTOR_R_EN_PIN = 8;  // Enables the forward half-bridge
static const int MOTOR_L_EN_PIN = 9;  // Enables the reverse half-bridge
static const int ENABLE = 7;

static const int DEBOUNCE_DELAY = 100;

static int retainState = LOW;
static int currentState = LOW;
static int lastState = LOW;
static unsigned long lastDebounceTime = 0;

static bool motorOn = false;
static unsigned long phaseStart = 0;
static unsigned long phaseDuration = 0; // 0 forces an immediate burst on the first loop

static int debounceButton(int pin);

void motorSetup()
{
    pinMode(MOTOR_RPWM_PIN, OUTPUT);
    pinMode(MOTOR_LPWM_PIN, OUTPUT);
    pinMode(MOTOR_R_EN_PIN, OUTPUT);
    pinMode(MOTOR_L_EN_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    pinMode(ENABLE, INPUT_PULLUP);
    digitalWrite(MOTOR_RPWM_PIN, LOW);
    digitalWrite(MOTOR_LPWM_PIN, LOW);
    digitalWrite(MOTOR_R_EN_PIN, LOW);
    digitalWrite(MOTOR_L_EN_PIN, LOW);

    unsigned long seed = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        seed = (seed << 2) ^ analogRead(A5);
    }
    randomSeed(seed);

    Serial.println("[DBG] Randomized Mole Repellent Mode Active.");
}

void motorUpdate()
{
    char msg[64];

    debounceButton(ENABLE);

    // If motor is disabled via web interface, keep it off
    if (motorDisabled)
    {
        digitalWrite(MOTOR_RPWM_PIN, LOW);
        return;
    }

    unsigned long now = millis();
    if (now - phaseStart < phaseDuration)
    {
        return;
    }

    phaseStart = now;

    motorOn = !motorOn;

    // Select appropriate settings based on time of day
    const MotorSettings &activeSettings = isNightMode() ? motorSettingsNight : motorSettings;

    if (motorOn && digitalRead(MOTOR_R_EN_PIN) && digitalRead(MOTOR_L_EN_PIN))
    {
        int randomSpeed = random((long)activeSettings.speedMin, (long)activeSettings.speedMax + 1); // PWM 0-255
        phaseDuration = random(activeSettings.runMinS * 1000L, activeSettings.runMaxS * 1000L + 1);

        snprintf_P(msg, sizeof(msg), PSTR("[DBG] Vibrating at PWM: %d for %lu ms"), randomSpeed, phaseDuration);
        Serial.println(msg);
        analogWrite(MOTOR_RPWM_PIN, randomSpeed);
    }
    else
    {
        phaseDuration = random(activeSettings.restMinS * 1000L, activeSettings.restMaxS * 1000L + 1);

        snprintf_P(msg, sizeof(msg), PSTR("[DBG] Motor off. Sleeping for %lu seconds..."), phaseDuration / 1000);
        Serial.println(msg);
        digitalWrite(MOTOR_RPWM_PIN, LOW);
    }
}

static int debounceButton(int pin)
{
    int reading = digitalRead(pin);

    if (reading != lastState)
    {
        lastDebounceTime = millis();
        Serial.print("[DBG] Pin raw: ");
        Serial.println(reading);
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY && reading != currentState)
    {
        currentState = reading;

        if (currentState == HIGH)
        {
            Serial.println("[DBG] Switch detected");
            retainState = !retainState;

            digitalWrite(LED_BUILTIN, retainState);

            digitalWrite(MOTOR_R_EN_PIN, retainState);
            digitalWrite(MOTOR_L_EN_PIN, retainState);

            Serial.println(retainState ? "[DBG] Motor enabled" : "[DBG] Motor disabled");

            phaseDuration = 0;

            motorOn = false;
        }
    }

    lastState = reading;
    return retainState;
}

#endif // MOTOR_ENABLED
