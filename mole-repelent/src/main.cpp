#include <Arduino.h>

const int MOTOR_RPWM_PIN = 10; // Forward speed PWM
const int MOTOR_LPWM_PIN = 11; // Reverse speed PWM, unused, held low
const int MOTOR_R_EN_PIN = 8;  // Enables the forward half-bridge
const int MOTOR_L_EN_PIN = 9;  // Enables the reverse half-bridge
const int ENABLE = 12;

int retainState = LOW;
int currentState = LOW;
int lastState = LOW;
unsigned long lastDebounceTime = 0;

bool motorOn = false;
unsigned long phaseStart = 0;
unsigned long phaseDuration = 0; // 0 forces an immediate burst on the first loop

const int DEBOUNCE_DELAY = 100;

int debounceButton(int pin);

void setup()
{
    Serial.begin(115200);

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

void loop()
{
    char msg[64];

    debounceButton(ENABLE);

    unsigned long now = millis();
    if (now - phaseStart < phaseDuration)
    {
        return;
    }

    phaseStart = now;

    motorOn = !motorOn;

    if (motorOn && digitalRead(MOTOR_R_EN_PIN) && digitalRead(MOTOR_L_EN_PIN))
    {
        int randomSpeed = random(200, 255); // Speed/Vibration intensity (PWM 0-255)
        phaseDuration = random(2000, 6000); // How long the motor runs (2 to 6 seconds)

        snprintf_P(msg, sizeof(msg), PSTR("[DBG] Vibrating at PWM: %d for %lu ms"), randomSpeed, phaseDuration);
        Serial.println(msg);
        analogWrite(MOTOR_RPWM_PIN, randomSpeed);
    }
    else
    {
        phaseDuration = random(20000, 40000); // How long the motor rests (20 to 40 seconds)

        snprintf_P(msg, sizeof(msg), PSTR("[DBG] Motor off. Sleeping for %lu seconds..."), phaseDuration / 1000);
        Serial.println(msg);
        digitalWrite(MOTOR_RPWM_PIN, LOW);
    }
}

int debounceButton(int pin)
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