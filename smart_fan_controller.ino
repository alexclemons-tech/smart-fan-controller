/*
 * Smart Fan Controller for ESP32-C3 Super Mini
 * 
 * Features:
 * - Temperature-based fan control (OFF / LOW / HIGH)
 * - Sound-reactive mode (reduces fan during speech)
 * - Temperature override safety (disables quiet mode if temp > threshold)
 * - Adjustable override temperature threshold (100-120°F)
 * - OLED menu system for settings
 * - Rotary encoder navigation
 * - Persistent settings storage
 * - OLED screen timeout to prevent burn-in
 * - Watchdog timer for 24/7 reliability
 * - Uptime tracking and display
 * 
 * Hardware:
 * - ESP32-C3 Super Mini
 * - TL-C14 PWM Fan
 * - DS18B20 Temperature Sensor
 * - GY-MAX9814 Sound Sensor
 * - Rotary Encoder Knob
 * - 0.96" SSD1306 OLED Display
 * - Buck Converter (12V -> 5V)
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <Arduino.h>
#include <esp_task_wdt.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

#define PIN_ROTARY_CLK      4   // Rotary encoder clock
#define PIN_ROTARY_DT       5   // Rotary encoder data
#define PIN_ROTARY_SW       6   // Rotary encoder switch
#define PIN_FAN_PWM         1   // PWM output to TL-C14 fan (CHANGED FROM 10)
#define PIN_TEMP_SENSOR     7   // DS18B20 one-wire pin
#define PIN_SOUND_SENSOR    0   // GY-MAX9814 analog input (ADC0)
#define PIN_SDA             20  // I2C SDA for OLED
#define PIN_SCL             21  // I2C SCL for OLED

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

// Temperature thresholds (in Celsius - convert from Fahrenheit)
#define TEMP_OFF            29   // 85°F - fan OFF below this
#define TEMP_LOW            32   // 90°F - fan LOW from 85-90°F
#define TEMP_HIGH           35   // 95°F - fan HIGH above 95°F

// Fan speed levels (PWM percentages)
#define FAN_SPEED_OFF       0    // 0%
#define FAN_SPEED_LOW       40   // 40%
#define FAN_SPEED_HIGH      100  // 100%

// Fan PWM settings
#define FAN_PWM_FREQ        25000  // 25kHz PWM frequency
#define FAN_PWM_CHANNEL     1      // LEDC channel 1 (CHANGED FROM 8)
#define FAN_PWM_RESOLUTION  8      // 8-bit (0-255)

// Sound sensor tuning
#define SOUND_THRESHOLD     100   // ADC threshold for speech detection
#define SOUND_AVERAGING     5     // Number of readings to average
#define SOUND_CHECK_INTERVAL 50   // ms between sound level checks

// Display settings
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

// Menu interaction timeouts
#define MENU_TIMEOUT        10000 // 10 seconds of inactivity returns to status
#define SCREEN_TIMEOUT      120000 // 2 minutes of inactivity - turn off OLED

// Watchdog timer
#define WATCHDOG_TIMEOUT    60    // 60 seconds - auto-reset if hung

// Override temperature range (Fahrenheit)
#define OVERRIDE_TEMP_MIN   100   // 100°F minimum
#define OVERRIDE_TEMP_MAX   120   // 120°F maximum
#define OVERRIDE_TEMP_DEFAULT 95  // 95°F default

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
OneWire oneWire(PIN_TEMP_SENSOR);
DallasTemperature tempSensor(&oneWire);
Preferences preferences;

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================

struct Settings {
  uint8_t quiet_mode_sensitivity;
  bool quiet_mode_enabled;
  bool temp_override_enabled;
  uint8_t override_temp_f;  // Override temperature threshold in Fahrenheit
};

Settings settings = {
  .quiet_mode_sensitivity = 50,
  .quiet_mode_enabled = true,
  .temp_override_enabled = true,
  .override_temp_f = OVERRIDE_TEMP_DEFAULT
};

enum FanSpeed {
  FAN_OFF = 0,
  FAN_LOW = 1,
  FAN_HIGH = 2
};

struct SystemState {
  float current_temp;
  FanSpeed current_fan_speed;
  uint16_t current_sound_level;
  bool sound_detected;
  bool temp_override_active;
  bool screen_on;
  unsigned long last_temp_read;
  unsigned long last_sound_check;
  unsigned long boot_time;
};

SystemState system_state = {
  .current_temp = 0,
  .current_fan_speed = FAN_OFF,
  .current_sound_level = 0,
  .sound_detected = false,
  .temp_override_active = false,
  .screen_on = true,
  .last_temp_read = 0,
  .last_sound_check = 0,
  .boot_time = 0
};

enum MenuMode {
  MENU_STATUS,
  MENU_MAIN,
  MENU_SETTINGS,
  MENU_QUIET_SENSITIVITY,
  MENU_TEMP_OVERRIDE,
  MENU_OVERRIDE_THRESHOLD
};

struct MenuState {
  MenuMode current_menu;
  uint8_t selected_option;  // 0-3 for main menu
  uint8_t edit_value;       // For sensitivity editing
  unsigned long last_interaction;
  unsigned long last_screen_activity;
};

MenuState menu_state = {
  .current_menu = MENU_STATUS,
  .selected_option = 0,
  .edit_value = 50,
  .last_interaction = 0,
  .last_screen_activity = 0
};

struct RotaryState {
  int last_clk;
  int encoder_delta;  // Track changes since last menu update
};

RotaryState rotary_state = {
  .last_clk = 1,
  .encoder_delta = 0
};

// ============================================================================
// SETUP & INITIALIZATION
// ============================================================================

void setup() {
  system_state.boot_time = millis();
  
  init_pins();
  init_display();
  init_sensors();
  init_pwm();
  init_watchdog();
  load_settings();
  
  menu_state.last_interaction = millis();
  menu_state.last_screen_activity = millis();
}

void init_pins() {
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  rotary_state.last_clk = digitalRead(PIN_ROTARY_CLK);
}

void init_display() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);
  }
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Smart Fan");
  display.println("Controller");
  display.println("Starting...");
  display.display();
  
  delay(1000);
}

void init_sensors() {
  tempSensor.begin();
  analogReadResolution(10);
}

void init_pwm() {
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
}

void init_watchdog() {
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
  esp_task_wdt_add(NULL);
}

void load_settings() {
  preferences.begin("fan_controller", false);
  
  settings.quiet_mode_sensitivity = preferences.getUChar("quiet_sens", 50);
  settings.quiet_mode_enabled = preferences.getBool("quiet_en", true);
  settings.temp_override_enabled = preferences.getBool("temp_override", true);
  settings.override_temp_f = preferences.getUChar("override_temp", OVERRIDE_TEMP_DEFAULT);
  
  // Validate override temp is within range
  if (settings.override_temp_f < OVERRIDE_TEMP_MIN || settings.override_temp_f > OVERRIDE_TEMP_MAX) {
    settings.override_temp_f = OVERRIDE_TEMP_DEFAULT;
  }
  
  preferences.end();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();
  
  esp_task_wdt_reset();
  
  // Update temperature
  if (now - system_state.last_temp_read >= 500) {
    update_temperature();
    update_override_status();
    system_state.last_temp_read = now;
  }
  
  // Update sound level
  if (now - system_state.last_sound_check >= SOUND_CHECK_INTERVAL) {
    update_sound_level();
    system_state.last_sound_check = now;
  }
  
  // Calculate and apply fan speed
  FanSpeed base_speed = calculate_fan_speed_from_temp();
  FanSpeed final_speed = apply_sound_dampening(base_speed);
  set_fan_speed(final_speed);
  system_state.current_fan_speed = final_speed;
  
  // Handle user input
  handle_rotary_encoder();
  handle_button_press();
  handle_menu_navigation();
  
  // Check for screen timeout
  screen_timeout_check();
  
  // Update display
  if (system_state.screen_on) {
    update_display();
  }
  
  delay(10);
}

// ============================================================================
// SENSOR UPDATES
// ============================================================================

void update_temperature() {
  tempSensor.requestTemperatures();
  float celsius = tempSensor.getTempCByIndex(0);
  
  if (celsius == DEVICE_DISCONNECTED_C) {
    system_state.current_temp = 0;
    return;
  }
  
  system_state.current_temp = celsius;
}

void update_override_status() {
  float temp_f = celsius_to_fahrenheit(system_state.current_temp);
  
  if (settings.temp_override_enabled) {
    if (temp_f > settings.override_temp_f) {
      system_state.temp_override_active = true;
    } else if (temp_f < (settings.override_temp_f - 2.0)) {
      system_state.temp_override_active = false;
    }
  } else {
    system_state.temp_override_active = false;
  }
}

void update_sound_level() {
  static uint16_t sound_samples[SOUND_AVERAGING];
  static uint8_t sample_index = 0;
  
  uint16_t raw_reading = analogRead(PIN_SOUND_SENSOR);
  sound_samples[sample_index] = raw_reading;
  sample_index = (sample_index + 1) % SOUND_AVERAGING;
  
  uint32_t sum = 0;
  for (int i = 0; i < SOUND_AVERAGING; i++) {
    sum += sound_samples[i];
  }
  uint16_t average = sum / SOUND_AVERAGING;
  
  system_state.current_sound_level = average;
  system_state.sound_detected = (average > SOUND_THRESHOLD);
}

float celsius_to_fahrenheit(float celsius) {
  return (celsius * 9.0 / 5.0) + 32.0;
}

float fahrenheit_to_celsius(float fahrenheit) {
  return (fahrenheit - 32.0) * 5.0 / 9.0;
}

// ============================================================================
// FAN CONTROL
// ============================================================================

void set_fan_speed(FanSpeed speed) {
  uint8_t pwm_percentage = 0;
  
  switch (speed) {
    case FAN_OFF:
      pwm_percentage = FAN_SPEED_OFF;
      break;
    case FAN_LOW:
      pwm_percentage = FAN_SPEED_LOW;
      break;
    case FAN_HIGH:
      pwm_percentage = FAN_SPEED_HIGH;
      break;
  }
  
  uint8_t pwm_value = (pwm_percentage * 255) / 100;
  ledcWrite(FAN_PWM_CHANNEL, pwm_value);
}

FanSpeed calculate_fan_speed_from_temp() {
  float temp_f = celsius_to_fahrenheit(system_state.current_temp);
  
  if (temp_f < 85.0) {
    return FAN_OFF;
  } else if (temp_f < 95.0) {
    return FAN_LOW;
  } else {
    return FAN_HIGH;
  }
}

FanSpeed apply_sound_dampening(FanSpeed base_speed) {
  if (system_state.temp_override_active) {
    return base_speed;
  }
  
  if (!settings.quiet_mode_enabled || !system_state.sound_detected) {
    return base_speed;
  }
  
  if (base_speed == FAN_HIGH) {
    if (settings.quiet_mode_sensitivity >= 60) {
      return FAN_LOW;
    } else {
      return FAN_OFF;
    }
  } else if (base_speed == FAN_LOW) {
    if (settings.quiet_mode_sensitivity < 40) {
      return FAN_OFF;
    }
  }
  
  return base_speed;
}

const char* fan_speed_to_string(FanSpeed speed) {
  switch (speed) {
    case FAN_OFF:
      return "OFF";
    case FAN_LOW:
      return "LOW";
    case FAN_HIGH:
      return "HIGH";
    default:
      return "???";
  }
}

// ============================================================================
// DISPLAY CONTROL
// ============================================================================

void screen_on_event() {
  menu_state.last_screen_activity = millis();
  
  if (!system_state.screen_on) {
    display.ssd1306_command(0xAF);
    system_state.screen_on = true;
  }
}

void screen_timeout_check() {
  unsigned long now = millis();
  unsigned long inactivity = now - menu_state.last_screen_activity;
  
  if (inactivity > SCREEN_TIMEOUT && system_state.screen_on) {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(0xAE);
    system_state.screen_on = false;
  }
}

// ============================================================================
// ROTARY ENCODER HANDLING
// ============================================================================

void handle_rotary_encoder() {
  static unsigned long last_read = 0;
  static unsigned long last_count_time = 0;
  const int MIN_PULSE_INTERVAL = 15;  // Debounce - prevents jitter
  
  unsigned long now = millis();
  
  if (now - last_read < 2) return;
  last_read = now;
  
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  int current_dt = digitalRead(PIN_ROTARY_DT);
  
  // Only count if enough time has passed (prevents bouncing)
  if (current_clk != rotary_state.last_clk && (now - last_count_time) > MIN_PULSE_INTERVAL) {
    rotary_state.last_clk = current_clk;
    
    if (current_clk == LOW) {
      if (current_dt == HIGH) {
        rotary_state.encoder_delta++;
      } else {
        rotary_state.encoder_delta--;
      }
    }
    
    last_count_time = now;
    screen_on_event();
  }
}

void handle_button_press() {
  static unsigned long last_press = 0;
  static bool was_pressed = false;
  
  int sw = digitalRead(PIN_ROTARY_SW);
  
  if (sw == LOW && !was_pressed) {
    unsigned long now = millis();
    if (now - last_press > 50) {
      was_pressed = true;
      last_press = now;
      menu_state.last_interaction = now;
      
      screen_on_event();
      
      // Handle button press
      if (menu_state.current_menu == MENU_STATUS) {
        menu_state.current_menu = MENU_MAIN;
        menu_state.selected_option = 0;
      } else if (menu_state.current_menu == MENU_MAIN) {
        switch (menu_state.selected_option) {
          case 0:
            menu_state.current_menu = MENU_SETTINGS;
            break;
          case 1:
            menu_state.current_menu = MENU_STATUS;
            break;
          case 2:
            menu_state.current_menu = MENU_QUIET_SENSITIVITY;
            menu_state.edit_value = settings.quiet_mode_sensitivity;
            break;
          case 3:
            menu_state.current_menu = MENU_OVERRIDE_THRESHOLD;
            menu_state.edit_value = settings.override_temp_f;
            break;
        }
      } else if (menu_state.current_menu == MENU_SETTINGS) {
        menu_state.current_menu = MENU_MAIN;
      } else if (menu_state.current_menu == MENU_QUIET_SENSITIVITY) {
        settings.quiet_mode_sensitivity = menu_state.edit_value;
        save_settings();
        menu_state.current_menu = MENU_MAIN;
      } else if (menu_state.current_menu == MENU_TEMP_OVERRIDE) {
        settings.temp_override_enabled = !settings.temp_override_enabled;
        save_settings();
        menu_state.current_menu = MENU_MAIN;
      } else if (menu_state.current_menu == MENU_OVERRIDE_THRESHOLD) {
        settings.override_temp_f = menu_state.edit_value;
        save_settings();
        menu_state.current_menu = MENU_MAIN;
      }
    }
  } else if (sw == HIGH) {
    was_pressed = false;
  }
}

void handle_menu_navigation() {
  if (menu_state.current_menu != MENU_STATUS) {
    if (millis() - menu_state.last_interaction > MENU_TIMEOUT) {
      menu_state.current_menu = MENU_STATUS;
      rotary_state.encoder_delta = 0;
      return;
    }
  }
  
  if (rotary_state.encoder_delta == 0) return;
  
  int delta = rotary_state.encoder_delta;
  rotary_state.encoder_delta = 0;  // Reset after reading
  menu_state.last_interaction = millis();
  
  if (menu_state.current_menu == MENU_MAIN) {
    menu_state.selected_option += delta;
    if (menu_state.selected_option < 0) menu_state.selected_option = 3;
    if (menu_state.selected_option > 3) menu_state.selected_option = 0;
    
  } else if (menu_state.current_menu == MENU_QUIET_SENSITIVITY) {
    menu_state.edit_value += delta * 5;
    if (menu_state.edit_value < 0) menu_state.edit_value = 0;
    if (menu_state.edit_value > 100) menu_state.edit_value = 100;
    
  } else if (menu_state.current_menu == MENU_OVERRIDE_THRESHOLD) {
    menu_state.edit_value += delta;
    if (menu_state.edit_value < OVERRIDE_TEMP_MIN) menu_state.edit_value = OVERRIDE_TEMP_MIN;
    if (menu_state.edit_value > OVERRIDE_TEMP_MAX) menu_state.edit_value = OVERRIDE_TEMP_MAX;
  }
}

// ============================================================================
// DISPLAY & MENUS
// ============================================================================

void update_display() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  switch (menu_state.current_menu) {
    case MENU_STATUS:
      draw_status_screen();
      break;
    case MENU_MAIN:
      draw_main_menu();
      break;
    case MENU_SETTINGS:
      draw_settings_menu();
      break;
    case MENU_QUIET_SENSITIVITY:
      draw_quiet_mode_menu();
      break;
    case MENU_TEMP_OVERRIDE:
      draw_temp_override_menu();
      break;
    case MENU_OVERRIDE_THRESHOLD:
      draw_override_threshold_menu();
      break;
  }
  
  display.display();
}

void draw_status_screen() {
  display.println("=== Status ===");
  display.println();
  
  display.print("Temp: ");
  display.print(system_state.current_temp, 1);
  display.print("C (");
  display.print(celsius_to_fahrenheit(system_state.current_temp), 0);
  display.println("F)");
  
  display.print("Fan: ");
  display.println(fan_speed_to_string(system_state.current_fan_speed));
  
  display.print("Sound: ");
  display.print(system_state.current_sound_level);
  display.print("[");
  display.print(system_state.sound_detected ? "ON" : "OFF");
  display.println("]");
  
  display.print("Override: ");
  display.println(system_state.temp_override_active ? "ACTIVE" : "Off");
  
  unsigned long uptime = (millis() - system_state.boot_time) / 1000;
  display.print("Uptime: ");
  display.print(uptime / 3600);
  display.print("h");
  
  display.println();
  display.println("[Press for menu]");
}

void draw_main_menu() {
  display.println("=== Main Menu ===");
  display.println();
  
  const char* options[] = {
    "Settings",
    "Status",
    "Quiet Mode",
    "Override Temp"
  };
  
  for (int i = 0; i < 4; i++) {
    if (i == menu_state.selected_option) {
      display.print(">");
    } else {
      display.print(" ");
    }
    display.println(options[i]);
  }
}

void draw_settings_menu() {
  display.println("=== Settings ===");
  display.println();
  display.print("Quiet Sens: ");
  display.print(settings.quiet_mode_sensitivity);
  display.println("%");
  
  display.print("Q.Mode: ");
  display.println(settings.quiet_mode_enabled ? "ON" : "OFF");
  
  display.print("TempOvrd: ");
  display.println(settings.temp_override_enabled ? "ON" : "OFF");
  
  display.print("OvrdTemp: ");
  display.print(settings.override_temp_f);
  display.println("F");
  
  display.println();
  display.println("[Press to go back]");
}

void draw_quiet_mode_menu() {
  display.println("=== Quiet Mode ===");
  display.println();
  display.print("Sensitivity: ");
  display.print(menu_state.edit_value);
  display.println("%");
  
  display.println("(Knob to adjust)");
  display.println();
  
  uint8_t bar_length = (menu_state.edit_value / 5);
  display.print("[");
  for (int i = 0; i < 20; i++) {
    display.print(i < bar_length ? "*" : "-");
  }
  display.println("]");
  
  display.println();
  display.println("[Press to save]");
}

void draw_temp_override_menu() {
  display.println("=== Temp Override ===");
  display.println();
  display.print("Status: ");
  display.println(settings.temp_override_enabled ? "ON" : "OFF");
  
  display.println();
  display.print("Threshold: ");
  display.print(settings.override_temp_f);
  display.println("F");
  
  display.println();
  display.print("Current: ");
  if (system_state.temp_override_active) {
    display.println("ACTIVE!");
  } else {
    display.println("Inactive");
  }
  
  display.println("[Press to toggle]");
}

void draw_override_threshold_menu() {
  display.println("=== Override Temp ===");
  display.println();
  display.print("Threshold: ");
  display.print(menu_state.edit_value);
  display.println("F");
  
  display.println("(Knob to adjust)");
  display.println("Range: 100-120F");
  display.println();
  
  uint8_t bar_length = ((menu_state.edit_value - OVERRIDE_TEMP_MIN) / 2);
  display.print("[");
  for (int i = 0; i < 10; i++) {
    display.print(i < bar_length ? "*" : "-");
  }
  display.println("]");
  
  display.println();
  display.println("[Press to save]");
}

// ============================================================================
// SETTINGS
// ============================================================================

void save_settings() {
  preferences.begin("fan_controller", false);
  
  preferences.putUChar("quiet_sens", settings.quiet_mode_sensitivity);
  preferences.putBool("quiet_en", settings.quiet_mode_enabled);
  preferences.putBool("temp_override", settings.temp_override_enabled);
  preferences.putUChar("override_temp", settings.override_temp_f);
  
  preferences.end();
}
