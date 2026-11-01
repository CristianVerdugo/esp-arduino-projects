# Mole Repellent - Developer Quick Reference

## Quick Start for Developers

### Project Structure
```
mole-repelent/
├── include/
│   ├── motor.h              // Motor control interface
│   ├── motor_settings.h     // ⭐ Settings structures & time logic
│   ├── ntp_time.h           // ⭐ Time query interface
│   ├── web_server.h         // Web interface
│   └── wifi_cc3000.h        // WiFi module
├── src/
│   ├── main.cpp             // Application entry point
│   ├── motor.cpp            // ⭐ Motor timing logic
│   ├── motor_settings.cpp   // ⭐ Settings persistence & night mode
│   ├── ntp_time.cpp         // ⭐ Time synchronization
│   ├── web_server.cpp       // ⭐ Web interface & settings UI
│   └── wifi_cc3000.cpp      // WiFi connectivity
└── platformio.ini           // Build configuration

⭐ = Modified for night settings feature
```

### Key Changes at a Glance

#### 1. New Global Variables
```cpp
// motor_settings.cpp
MotorSettings motorSettings;      // Day settings (24:00-23:59, applied 8-20)
MotorSettings motorSettingsNight; // Night settings (24:00-23:59, applied 20-8)
```

#### 2. New Constants
```cpp
// motor_settings.h
#define NIGHT_START_HOUR 20  // 8 PM
#define NIGHT_END_HOUR 8     // 8 AM
```

#### 3. New Functions
```cpp
// motor_settings.h/cpp
bool isNightMode();           // Returns true if current time is night hours

// ntp_time.h/cpp
bool getHourBerlin(uint8_t &hour);  // Get current hour in Berlin timezone
```

#### 4. Modified Function Signatures
```cpp
// Was: bool motorSettingsSave(const MotorSettings &s);
// Now: bool motorSettingsSave(const MotorSettings &s, bool isNight = false);
```

---

## Making Changes

### To Adjust Night Start/End Hours
Edit `include/motor_settings.h`:
```cpp
#define NIGHT_START_HOUR 20  // Change to desired start hour (0-23)
#define NIGHT_END_HOUR 8     // Change to desired end hour (0-23)
```
Example for 10 PM - 6 AM:
```cpp
#define NIGHT_START_HOUR 22  // 10 PM
#define NIGHT_END_HOUR 6     // 6 AM
```

### To Change Default Night Settings
Edit `src/motor_settings.cpp`:
```cpp
// Line with motorSettingsNight initialization
MotorSettings motorSettingsNight = {2, 4, 60, 120, 200, 255};
//                                  |  |  |   |    |    |
//                                  |  |  |   |    |    └─ speedMax
//                                  |  |  |   |    └────── speedMin
//                                  |  |  |   └─────────── restMaxS
//                                  |  |  └──────────────── restMinS
//                                  |  └───────────────── runMaxS
//                                  └────────────────────── runMinS
```

### To Change Default Day Settings
Edit `src/motor_settings.cpp`:
```cpp
MotorSettings motorSettings = {2, 6, 20, 40, 200, 255};
```

### To Disable Night Mode Feature
Option A: Make night and day settings identical
```cpp
MotorSettings motorSettings = {2, 6, 20, 40, 200, 255};
MotorSettings motorSettingsNight = {2, 6, 20, 40, 200, 255};  // Same values
```

Option B: Modify motor.cpp to always use day settings
```cpp
// In motorUpdate(), replace:
const MotorSettings &activeSettings = isNightMode() ? motorSettingsNight : motorSettings;
// With:
const MotorSettings &activeSettings = motorSettings;
```

### To Force Day or Night Settings (Testing)
Modify `src/motor_settings.cpp`:
```cpp
bool isNightMode() {
    // For testing: always return true to use night settings
    return true;
    
    // Or: always return false to use day settings
    return false;
    
    // Or: normal operation
    uint8_t hour = 0;
    if (!getHourBerlin(hour)) return false;
    return hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;
}
```

---

## Key Code Snippets

### Selecting Settings at Runtime
```cpp
// In motor.cpp motorUpdate()
const MotorSettings &activeSettings = 
    isNightMode() ? motorSettingsNight : motorSettings;

// Use activeSettings for all random timing
int randomSpeed = random((long)activeSettings.speedMin, 
                        (long)activeSettings.speedMax + 1);
phaseDuration = random(activeSettings.runMinS * 1000L, 
                       activeSettings.runMaxS * 1000L + 1);
```

### Checking Night Mode
```cpp
// In motor_settings.cpp isNightMode()
bool isNightMode() {
    uint8_t hour = 0;
    if (!getHourBerlin(hour)) {
        return false;  // If time not available, use day settings
    }
    // Night: hour >= 20 (8 PM) OR hour < 8 (before 8 AM)
    return hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;
}
```

### Saving Settings
```cpp
// Save day settings
motorSettingsSave(newSettings, false);

// Save night settings
motorSettingsSave(newSettings, true);

// Without second parameter, defaults to day settings
motorSettingsSave(newSettings);  // Saves as day settings
```

### Getting Current Hour
```cpp
uint8_t hour;
if (getHourBerlin(hour)) {
    Serial.print("Current hour: ");
    Serial.println(hour);
} else {
    Serial.println("Time not synced yet");
}
```

---

## Web Interface Form Format

### HTML Form Structure
```html
<!-- Day Settings Form -->
<form>
  <input type=number name=rmin value=2 min=2 max=3600>      <!-- runMinS -->
  <input type=number name=rmax value=6 min=2 max=3600>      <!-- runMaxS -->
  <input type=number name=pmin value=20 min=20 max=3600>    <!-- restMinS -->
  <input type=number name=pmax value=40 min=20 max=3600>    <!-- restMaxS -->
  <input type=number name=vmin value=200 min=150 max=255>   <!-- speedMin -->
  <input type=number name=vmax value=255 min=150 max=255>   <!-- speedMax -->
  <input type=hidden name=night value=0>                     <!-- Day marker -->
  <input type=submit value="Save Day Settings">
</form>

<!-- Night Settings Form -->
<form>
  <input type=number name=rmin value=2 min=2 max=3600>      <!-- runMinS -->
  <input type=number name=rmax value=4 min=2 max=3600>      <!-- runMaxS -->
  <input type=number name=pmin value=60 min=20 max=3600>    <!-- restMinS -->
  <input type=number name=pmax value=120 min=20 max=3600>   <!-- restMaxS -->
  <input type=number name=vmin value=200 min=150 max=255>   <!-- speedMin -->
  <input type=number name=vmax value=255 min=150 max=255>   <!-- speedMax -->
  <input type=hidden name=night value=1>                     <!-- Night marker -->
  <input type=submit value="Save Night Settings">
</form>
```

### Query String Format
```
Day settings submission:
GET /?rmin=2&rmax=6&pmin=20&pmax=40&vmin=200&vmax=255&night=0

Night settings submission:
GET /?rmin=2&rmax=4&pmin=60&pmax=120&vmin=200&vmax=255&night=1
```

---

## Testing Checklist

### Unit Tests
- [ ] `motorSettingsValid()` accepts night values
- [ ] `motorSettingsSave(s, true)` saves to motorSettingsNight
- [ ] `motorSettingsSave(s, false)` saves to motorSettings
- [ ] `isNightMode()` returns false for 9 AM
- [ ] `isNightMode()` returns true for 11 PM
- [ ] `isNightMode()` returns true for 6 AM
- [ ] `getHourBerlin(h)` returns false if not synced
- [ ] `getHourBerlin(h)` returns correct hour after sync

### Integration Tests
- [ ] Settings load from EEPROM on startup
- [ ] Settings persist after power cycle
- [ ] Web form saves day settings correctly
- [ ] Web form saves night settings correctly
- [ ] Motor uses day settings between 8 AM - 8 PM
- [ ] Motor uses night settings between 8 PM - 8 AM
- [ ] Motor switches at 8 AM boundary
- [ ] Motor switches at 8 PM boundary

### Manual Testing
- [ ] Web interface loads correctly
- [ ] Day Settings tab displays correct values
- [ ] Night Settings tab displays correct values
- [ ] Tab switching works without page reload
- [ ] Form submission returns "Saved." message
- [ ] Invalid input returns "Rejected" message
- [ ] Motor timing reflects selected settings
- [ ] Time display shows correct Berlin time

---

## Debugging Tips

### Check Current Settings
```cpp
Serial.print("Day settings - min: ");
Serial.print(motorSettings.restMinS);
Serial.print(", max: ");
Serial.println(motorSettings.restMaxS);

Serial.print("Night settings - min: ");
Serial.print(motorSettingsNight.restMinS);
Serial.print(", max: ");
Serial.println(motorSettingsNight.restMaxS);
```

### Check Time Sync Status
```cpp
if (ntpHasTime()) {
    uint8_t hour;
    if (getHourBerlin(hour)) {
        Serial.print("Hour: ");
        Serial.print(hour);
        Serial.println(isNightMode() ? " (NIGHT)" : " (DAY)");
    }
} else {
    Serial.println("Time not synced yet");
}
```

### Verify EEPROM Reading
```cpp
Serial.print("Loaded day rest: ");
Serial.print(motorSettings.restMinS);
Serial.print("-");
Serial.println(motorSettings.restMaxS);

Serial.print("Loaded night rest: ");
Serial.print(motorSettingsNight.restMinS);
Serial.print("-");
Serial.println(motorSettingsNight.restMaxS);
```

### Test Web Form Parsing
Set a breakpoint in `applyQuery()` to see:
```cpp
bool isNight = strstr_P(line, PSTR("night=1")) != nullptr;
// If true, saving night settings
// If false, saving day settings
```

---

## Common Issues & Fixes

### Issue: Settings not saving
**Check:**
1. Validation: Are all values within valid ranges?
2. EEPROM space: Is there enough free EEPROM?
3. Web form: Is `night` parameter included (0 or 1)?

**Fix:**
```cpp
// Enable debug output
Serial.println("[DBG] Attempting to save settings...");
if (motorSettingsSave(s, isNight)) {
    Serial.println("[DBG] Settings saved successfully");
} else {
    Serial.println("[DBG] Settings validation failed");
}
```

### Issue: Motor not switching at 8 AM/8 PM
**Check:**
1. NTP time synced? Check `berlinTimeString()` output
2. Current hour checking? Log `getHourBerlin(hour)` result
3. Night mode logic? Add debug output to `isNightMode()`

**Fix:**
```cpp
bool isNightMode() {
    uint8_t hour = 0;
    if (!getHourBerlin(hour)) {
        Serial.println("[DBG] Time not synced, using day settings");
        return false;
    }
    bool night = hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;
    Serial.print("[DBG] Hour: ");
    Serial.print(hour);
    Serial.println(night ? " (NIGHT)" : " (DAY)");
    return night;
}
```

### Issue: Web interface shows old settings
**Cause:** EEPROM contains valid old format (magic 0xA6)

**Fix:**
1. Clear EEPROM and restart
2. Or manually update EEPROM magic to 0xA7
3. Or let device use defaults and reconfigure

---

## Performance Notes

### Memory Usage
- Stack: Settings structures are passed by reference (efficient)
- RAM: Two `MotorSettings` structs = 24 bytes total
- Flash: Additional ~3 KB for new code and UI

### CPU Usage
- `isNightMode()` called once per vibration cycle
- `getHourBerlin()` does simple arithmetic (~100 μs)
- Negligible impact on performance

### Power Consumption
- Motor control logic unchanged
- Time checking adds <1 mA average (mostly in NTP sync, not runtime)
- Night mode saves 60-70% motor power during night hours

---

## Version History

### V2.0 (Current) - Night Settings
- Added dual settings mode (day/night)
- Added automatic time-based switching
- Added tabbed web interface
- EEPROM format: magic 0xA7
- Incompatible with V1.0 stored settings

### V1.0 (Previous) - Single Settings
- Single settings mode
- Manual only configuration
- EEPROM format: magic 0xA6
- Simple web interface

---

## Related Documentation

See also:
- `USER_GUIDE_NIGHT_SETTINGS.md` - End-user documentation
- `ARCHITECTURE_NIGHT_SETTINGS.md` - System design diagrams
- `CHANGES_NIGHT_SETTINGS.md` - Detailed changelog
- `BEFORE_AFTER_COMPARISON.md` - Feature comparison
