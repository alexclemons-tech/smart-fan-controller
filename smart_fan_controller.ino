/*
 * Smart Fan Controller for ESP32-C3 Super Mini
 * 
 * Features:
 * - Temperature-based fan control (OFF / LOW / HIGH)
 * - Sound-reactive mode (reduces fan during speech)
 * - Temperature override safety (disables quiet mode if temp > 95°F)
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
// ESP32-C3 Super Mini has GPIO 0-10, 20, 21
// Avoid GPIO 2 (BOOT), GPIO 8-9 (USB), GPIO 3 (onboard LED)

#define PIN_ROTARY_CLK      4   // Rotary encoder clock
#define PIN_ROTARY_DT       5   // Rotary encoder data
#define PIN_ROTARY_SW       6   // Rotary encoder switch
#define PIN_FAN_PWM         10  // PWM output to TL-C14 fan
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
#define TEMP_OVERRIDE       35   // 95°F - disable quiet mode above this

// Fan speed levels (PWM percentages)
#define FAN_SPEED_OFF       0    // 0%
#define FAN_SPEED_LOW       40   // 40%
#define FAN_SPEED_HIGH      100  // 100%

// Fan PWM settings
#define FAN_PWM_FREQ        25000  // 25kHz PWM frequency
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
#define ENCODER_ACCELERATION 300  // ms for acceleration threshold

// Watchdog timer
#define WATCHDOG_TIMEOUT    60    // 60 seconds - auto-reset if hung

// Encoder debugging
#define DEBUG_ENCODER       true  // Set to false to disable encoder debug output

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
  uint8_t quiet_mode_sensitivity;  // 0-100%, default 50%
  bool quiet_mode_enabled;         // Enable speech detection
  bool temp_override_enabled;      // Enable temp override safety (bypass quiet mode if temp > 95F)
};

Settings settings = {
  .quiet_mode_sensitivity = 50,
  .quiet_mode_enabled = true,
  .temp_override_enabled = true
};

// Fan speed enum
enum FanSpeed {
  FAN_OFF = 0,
  FAN_LOW = 1,
  FAN_HIGH = 2
};

// Runtime state
struct SystemState {
  float current_temp;
  FanSpeed current_fan_speed;  // OFF, LOW, or HIGH
  uint16_t current_sound_level;
  bool sound_detected;
  bool temp_override_active;   // Is override currently active?
  bool screen_on;              // Is OLED display currently on?
  unsigned long last_temp_read;
  unsigned long last_sound_check;
  unsigned long boot_time;     // Timestamp of when system started
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

// Menu system state
enum MenuMode {
  MENU_MAIN,
  MENU_SETTINGS,
  MENU_QUIET_SENSITIVITY,
  MENU_TEMP_OVERRIDE,
  MENU_STATUS
};

struct MenuState {
  MenuMode current_menu;
  uint8_t selected_option;
  uint8_t edit_value;  // Temporary value for editing
  bool in_edit_mode;
  unsigned long last_interaction;
  unsigned long last_screen_activity;
  int last_encoder_value;
};

MenuState menu_state = {
  .current_menu = MENU_STATUS,
  .selected_option = 0,
  .edit_value = 0,
  .in_edit_mode = false,
  .last_interaction = 0,
  .last_screen_activity = 0,
  .last_encoder_value = 0
};

// Rotary encoder state
struct RotaryState {
  int last_clk_state;
  int current_clk_state;
  int last_dt_state;
  int encoder_value;
  unsigned long last_rotate_time;
  unsigned long last_debug_time;
};

RotaryState rotary_state = {
  .last_clk_state = 0,
  .current_clk_state = 0,
  .last_dt_state = 0,
  .encoder_value = 0,
  .last_rotate_time = 0,
  .last_debug_time = 0
};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setup();
void loop();

// Initialization
void init_pins();
void init_display();
void init_sensors();
void init_pwm();
void init_watchdog();
void load_settings();

// Sensor reading
void update_temperature();
void update_sound_level();
void update_override_status();
float celsius_to_fahrenheit(float celsius);
float fahrenheit_to_celsius(float fahrenheit);

// Fan control
void set_fan_speed(FanSpeed speed);
FanSpeed calculate_fan_speed_from_temp();
FanSpeed apply_sound_dampening(FanSpeed base_speed);
const char* fan_speed_to_string(FanSpeed speed);

// Display control
void screen_on_event();
void screen_timeout_check();
void display_sleep();
void display_wake();

// Menu & UI
void handle_rotary_encoder();
void handle_button_press();
void handle_menu_navigation();
void debug_encoder_state();
void update_display();
void draw_main_menu();
void draw_settings_menu();
void draw_quiet_mode_menu();
void draw_temp_override_menu();
void draw_status_screen();

// Settings
void save_settings();

// System & Utilities
void feed_watchdog();
unsigned long get_uptime_seconds();
void format_uptime(unsigned long seconds, char* buffer, size_t max_len);
void debug_print(const char* format, ...);

// ============================================================================
// SETUP & INITIALIZATION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  debug_print("\n\n=== Smart Fan Controller Starting ===\n");
  debug_print("Pin Configuration:\n");
  debug_print("  CLK: GPIO %d\n", PIN_ROTARY_CLK);
  debug_print("  DT:  GPIO %d\n", PIN_ROTARY_DT);
  debug_print("  SW:  GPIO %d\n", PIN_ROTARY_SW);
  
  system_state.boot_time = millis();
  
  init_pins();
  init_display();
  init_sensors();
  init_pwm();
  init_watchdog();
  load_settings();
  
  menu_state.last_interaction = millis();
  menu_state.last_screen_activity = millis();
  
  // Read initial encoder state
  rotary_state.last_clk_state = digitalRead(PIN_ROTARY_CLK);
  rotary_state.last_dt_state = digitalRead(PIN_ROTARY_DT);
  
  debug_print("Setup complete! System ready for operation.\n");
  debug_print("Encoder debugging: %s\n", DEBUG_ENCODER ? "ENABLED" : "DISABLED");
}

void init_pins() {
  debug_print("Initializing pins...\n");
  
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  debug_print("  CLK state: %d\n", digitalRead(PIN_ROTARY_CLK));
  debug_print("  DT state:  %d\n", digitalRead(PIN_ROTARY_DT));
  debug_print("  SW state:  %d\n", digitalRead(PIN_ROTARY_SW));
}

void init_display() {
  debug_print("Initializing OLED display...\n");
  
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    debug_print("ERROR: Failed to initialize OLED!\n");
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
  
  debug_print("OLED initialized on GPIO %d (SDA), GPIO %d (SCL)\n", PIN_SDA, PIN_SCL);
  
  delay(1000);
}

void init_sensors() {
  debug_print("Initializing sensors...\n");
  
  tempSensor.begin();
  debug_print("DS18B20 initialized\n");
  
  analogReadResolution(10);
  debug_print("GY-MAX9814 initialized\n");
}

void init_pwm() {
  debug_print("Initializing PWM...\n");
  
  ledcAttach(PIN_FAN_PWM, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcWrite(PIN_FAN_PWM, 0);
  
  debug_print("PWM configured: %d Hz, %d-bit resolution on GPIO %d\n", FAN_PWM_FREQ, FAN_PWM_RESOLUTION, PIN_FAN_PWM);
}

void init_watchdog() {
  debug_print("Initializing watchdog timer (%d seconds)...\n", WATCHDOG_TIMEOUT);
  
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  
  debug_print("Watchdog enabled. System will auto-reset if unresponsive.\n");
}

void load_settings() {
  debug_print("Loading settings from flash...\n");
  
  preferences.begin("fan_controller", false);
  
  settings.quiet_mode_sensitivity = preferences.getUChar("quiet_sens", 50);
  settings.quiet_mode_enabled = preferences.getBool("quiet_en", true);
  settings.temp_override_enabled = preferences.getBool("temp_override", true);
  
  preferences.end();
  
  debug_print("Settings loaded: quiet_sens=%d, temp_override=%s\n",
              settings.quiet_mode_sensitivity,
              settings.temp_override_enabled ? "ON" : "OFF");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();
  
  feed_watchdog();
  
  // Update temperature (every 500ms)
  if (now - system_state.last_temp_read >= 500) {
    update_temperature();
    update_override_status();
    system_state.last_temp_read = now;
  }
  
  // Update sound level (every 50ms)
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
  
  // Update display (only if screen is on)
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
    debug_print("ERROR: Temperature sensor disconnected!\n");
    system_state.current_temp = 0;
    return;
  }
  
  system_state.current_temp = celsius;
  
  static unsigned long last_debug = 0;
  if (millis() - last_debug > 5000) {
    debug_print("Temp: %.1f°C (%.1f°F), Fan: %s, Override: %s\n", 
                celsius, 
                celsius_to_fahrenheit(celsius),
                fan_speed_to_string(system_state.current_fan_speed),
                system_state.temp_override_active ? "ACTIVE" : "off");
    last_debug = millis();
  }
}

void update_override_status() {
  float temp_f = celsius_to_fahrenheit(system_state.current_temp);
  
  if (settings.temp_override_enabled) {
    if (temp_f > 95.0) {
      system_state.temp_override_active = true;
    } else if (temp_f < 93.0) {
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
// FAN CONTROL - THREE SPEEDS (OFF / LOW / HIGH)
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
  ledcWrite(PIN_FAN_PWM, pwm_value);
}

FanSpeed calculate_fan_speed_from_temp() {
  float temp_f = celsius_to_fahrenheit(system_state.current_temp);
  
  // Simple three-level control:
  // < 85°F: OFF
  // 85-95°F: LOW
  // > 95°F: HIGH
  
  if (temp_f < 85.0) {
    return FAN_OFF;
  } else if (temp_f < 95.0) {
    return FAN_LOW;
  } else {
    return FAN_HIGH;
  }
}

FanSpeed apply_sound_dampening(FanSpeed base_speed) {
  // If temperature override is active, bypass quiet mode entirely
  if (system_state.temp_override_active) {
    return base_speed;
  }
  
  // If quiet mode disabled or no sound detected, use base speed
  if (!settings.quiet_mode_enabled || !system_state.sound_detected) {
    return base_speed;
  }
  
  // If sound detected and quiet mode enabled, reduce speed
  // HIGH -> LOW, LOW -> OFF (unless sensitivity is very high)
  if (base_speed == FAN_HIGH) {
    // Check sensitivity - if > 60%, keep at LOW instead of OFF
    if (settings.quiet_mode_sensitivity >= 60) {
      return FAN_LOW;
    } else {
      return FAN_OFF;
    }
  } else if (base_speed == FAN_LOW) {
    // Only reduce to OFF if sensitivity is low (< 40%)
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
// DISPLAY CONTROL - SCREEN TIMEOUT & POWER SAVING
// ============================================================================

void screen_on_event() {
  menu_state.last_screen_activity = millis();
  
  if (!system_state.screen_on) {
    display_wake();
  }
}

void screen_timeout_check() {
  unsigned long now = millis();
  unsigned long inactivity = now - menu_state.last_screen_activity;
  
  if (inactivity > SCREEN_TIMEOUT && system_state.screen_on) {
    display_sleep();
  }
}

void display_sleep() {
  debug_print("OLED sleep - powering down screen\n");
  display.clearDisplay();
  display.display();
  display.ssd1306_command(0xAE);  // Turn off display
  system_state.screen_on = false;
}

void display_wake() {
  debug_print("OLED wake - powering up screen\n");
  display.ssd1306_command(0xAF);  // Turn on display
  system_state.screen_on = true;
}

// ============================================================================
// ROTARY ENCODER HANDLING WITH DEBUGGING
// ============================================================================

void debug_encoder_state() {
  if (!DEBUG_ENCODER) return;
  
  unsigned long now = millis();
  if (now - rotary_state.last_debug_time < 500) return;  // Print every 500ms max
  
  int clk = digitalRead(PIN_ROTARY_CLK);
  int dt = digitalRead(PIN_ROTARY_DT);
  int sw = digitalRead(PIN_ROTARY_SW);
  
  debug_print("[ENCODER] CLK=%d DT=%d SW=%d | Value=%d\n", clk, dt, sw, rotary_state.encoder_value);
  
  rotary_state.last_debug_time = now;
}

void handle_rotary_encoder() {
  static unsigned long last_read_time = 0;
  unsigned long now = millis();
  
  if (now - last_read_time < 5) return;
  last_read_time = now;
  
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  int current_dt = digitalRead(PIN_ROTARY_DT);
  
  rotary_state.current_clk_state = current_clk;
  
  // Detect state change on CLK pin
  if (current_clk != rotary_state.last_clk_state) {
    rotary_state.last_clk_state = current_clk;
    
    // Read DT to determine direction
    if (current_clk == LOW) {
      if (current_dt == HIGH) {
        rotary_state.encoder_value++;
        if (DEBUG_ENCODER) debug_print("[ENCODER] CLOCKWISE - Value: %d\n", rotary_state.encoder_value);
      } else {
        rotary_state.encoder_value--;
        if (DEBUG_ENCODER) debug_print("[ENCODER] COUNTER-CW - Value: %d\n", rotary_state.encoder_value);
      }
      
      screen_on_event();
    }
  }
  
  rotary_state.last_dt_state = current_dt;
  debug_encoder_state();
}

void handle_button_press() {
  static unsigned long last_press_time = 0;
  static bool button_was_pressed = false;
  
  int button_state = digitalRead(PIN_ROTARY_SW);
  
  if (button_state == LOW && !button_was_pressed) {
    unsigned long now = millis();
    if (now - last_press_time > 50) {
      button_was_pressed = true;
      last_press_time = now;
      menu_state.last_interaction = now;
      
      debug_print("[BUTTON] Pressed\n");
      screen_on_event();
      
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
            menu_state.current_menu = MENU_TEMP_OVERRIDE;
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
      }
    }
  } else if (button_state == HIGH) {
    button_was_pressed = false;
  }
}

void handle_menu_navigation() {
  if (menu_state.current_menu != MENU_STATUS) {
    if (millis() - menu_state.last_interaction > MENU_TIMEOUT) {
      menu_state.current_menu = MENU_STATUS;
      rotary_state.encoder_value = menu_state.last_encoder_value;
      return;
    }
  }
  
  int encoder_delta = rotary_state.encoder_value - menu_state.last_encoder_value;
  
  if (encoder_delta != 0) {
    menu_state.last_encoder_value = rotary_state.encoder_value;
    menu_state.last_interaction = millis();
    
    if (DEBUG_ENCODER) debug_print("[MENU] Delta: %d\n", encoder_delta);
    
    if (menu_state.current_menu == MENU_MAIN) {
      menu_state.selected_option += encoder_delta;
      if (menu_state.selected_option < 0) menu_state.selected_option = 3;
      if (menu_state.selected_option > 3) menu_state.selected_option = 0;
      debug_print("[MENU] Selected option: %d\n", menu_state.selected_option);
      
    } else if (menu_state.current_menu == MENU_QUIET_SENSITIVITY) {
      menu_state.edit_value += encoder_delta * 5;
      if (menu_state.edit_value < 0) menu_state.edit_value = 0;
      if (menu_state.edit_value > 100) menu_state.edit_value = 100;
      debug_print("[MENU] Quiet sensitivity: %d%%\n", menu_state.edit_value);
    }
  }
}

// ============================================================================
// DISPLAY & MENU SYSTEM
// ============================================================================

void update_display() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  switch (menu_state.current_menu) {
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
    case MENU_STATUS:
      draw_status_screen();
      break;
  }
  
  display.display();
}

void draw_main_menu() {
  display.println("=== Main Menu ===");
  display.println();
  
  const char* options[] = {
    "Settings",
    "Status",
    "Quiet Mode",
    "Temp Override"
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
  display.print("Quiet: ");
  display.print(settings.quiet_mode_sensitivity);
  display.println("%");
  
  display.print("Q.Mode: ");
  display.println(settings.quiet_mode_enabled ? "ON" : "OFF");
  
  display.print("TempOvrd: ");
  display.println(settings.temp_override_enabled ? "ON" : "OFF");
  
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
  display.println("When temp > 95F:");
  display.println("- Quiet mode OFF");
  display.println("- Fan to HIGH");
  
  display.println();
  display.print("Current: ");
  if (system_state.temp_override_active) {
    display.println("ACTIVE!");
  } else {
    display.println("Inactive");
  }
  
  display.println("[Press to toggle]");
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
  
  // Display uptime
  char uptime_str[32];
  format_uptime(get_uptime_seconds(), uptime_str, sizeof(uptime_str));
  display.print("Uptime: ");
  display.println(uptime_str);
  
  display.println("[Press for menu]");
}

// ============================================================================
// SETTINGS MANAGEMENT
// ============================================================================

void save_settings() {
  debug_print("Saving settings to flash...\n");
  
  preferences.begin("fan_controller", false);
  
  preferences.putUChar("quiet_sens", settings.quiet_mode_sensitivity);
  preferences.putBool("quiet_en", settings.quiet_mode_enabled);
  preferences.putBool("temp_override", settings.temp_override_enabled);
  
  preferences.end();
  
  debug_print("Settings saved!\n");
}

// ============================================================================
// SYSTEM RELIABILITY & UTILITIES
// ============================================================================

void feed_watchdog() {
  esp_task_wdt_reset();
}

unsigned long get_uptime_seconds() {
  return (millis() - system_state.boot_time) / 1000;
}

void format_uptime(unsigned long seconds, char* buffer, size_t max_len) {
  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  
  snprintf(buffer, max_len, "%ldd%ldh%ldm", days, hours, minutes);
}

void debug_print(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  Serial.print(buffer);
}
