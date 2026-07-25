/*
 * Smart Fan Controller for ESP32-C3 Super Mini
 * 
 * Features:
 * - Temperature-based PWM fan control
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
#define TEMP_OFF            29   // 85°F - fan off below this
#define TEMP_25_PERCENT     29   // 85°F - start of curve
#define TEMP_60_PERCENT     35   // 95°F
#define TEMP_75_PERCENT     40   // 105°F
#define TEMP_100_PERCENT    46   // 115°F
#define TEMP_OVERRIDE       35   // 95°F - disable quiet mode above this

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
  uint8_t temp_off_threshold;      // 85°F default
  uint8_t temp_max_threshold;      // 115°F default
  bool quiet_mode_enabled;         // Enable speech detection
  bool temp_override_enabled;      // Enable temp override safety (bypass quiet mode if temp > 95F)
};

Settings settings = {
  .quiet_mode_sensitivity = 50,
  .temp_off_threshold = 29,
  .temp_max_threshold = 46,
  .quiet_mode_enabled = true,
  .temp_override_enabled = true
};

// Runtime state
struct SystemState {
  float current_temp;
  uint8_t current_fan_speed;  // 0-100%
  uint16_t current_sound_level;
  bool sound_detected;
  bool temp_override_active;  // Is override currently active?
  bool screen_on;             // Is OLED display currently on?
  unsigned long last_temp_read;
  unsigned long last_sound_check;
  unsigned long boot_time;    // Timestamp of when system started
};

SystemState system_state = {
  .current_temp = 0,
  .current_fan_speed = 0,
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
  MENU_TEMP_LIMITS,
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
  int encoder_value;
  unsigned long last_rotate_time;
};

RotaryState rotary_state = {
  .last_clk_state = 0,
  .encoder_value = 0,
  .last_rotate_time = 0
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
void set_fan_speed(uint8_t percentage);
uint8_t calculate_fan_speed_from_temp();
uint8_t apply_sound_dampening(uint8_t base_speed);

// Display control
void screen_on_event();
void screen_timeout_check();
void display_sleep();
void display_wake();

// Menu & UI
void handle_rotary_encoder();
void handle_button_press();
void handle_menu_navigation();
void update_display();
void draw_main_menu();
void draw_settings_menu();
void draw_quiet_mode_menu();
void draw_temp_limits_menu();
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
  
  system_state.boot_time = millis();
  
  init_pins();
  init_display();
  init_sensors();
  init_pwm();
  init_watchdog();
  load_settings();
  
  menu_state.last_interaction = millis();
  menu_state.last_screen_activity = millis();
  
  debug_print("Setup complete! Watchdog enabled for 24/7 reliability.\n");
}

void init_pins() {
  debug_print("Initializing pins...\n");
  
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  rotary_state.last_clk_state = digitalRead(PIN_ROTARY_CLK);
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
  
  // Configure watchdog: 60 second timeout, panics if not fed
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
  esp_task_wdt_add(NULL);  // Subscribe current task
  
  debug_print("Watchdog enabled. Must call feed_watchdog() every %d seconds.\n", WATCHDOG_TIMEOUT);
}

void load_settings() {
  debug_print("Loading settings from flash...\n");
  
  preferences.begin("fan_controller", false);
  
  settings.quiet_mode_sensitivity = preferences.getUChar("quiet_sens", 50);
  settings.temp_off_threshold = preferences.getUChar("temp_off", 29);
  settings.temp_max_threshold = preferences.getUChar("temp_max", 46);
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
  
  // Feed watchdog regularly (prevents auto-reset)
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
  uint8_t base_speed = calculate_fan_speed_from_temp();
  uint8_t final_speed = apply_sound_dampening(base_speed);
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
    debug_print("Temp: %.1f°C (%.1f°F), Fan: %d%%, Uptime: %ld sec\n", 
                celsius, 
                celsius_to_fahrenheit(celsius),
                system_state.current_fan_speed,
                get_uptime_seconds());
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
// FAN CONTROL
// ============================================================================

void set_fan_speed(uint8_t percentage) {
  if (percentage > 100) percentage = 100;
  
  uint8_t pwm_value = (percentage * 255) / 100;
  ledcWrite(PIN_FAN_PWM, pwm_value);
}

uint8_t calculate_fan_speed_from_temp() {
  float temp_c = system_state.current_temp;
  float temp_f = celsius_to_fahrenheit(temp_c);
  
  if (temp_f < 85) {
    return 0;
  } else if (temp_f < 95) {
    return 25 + ((temp_f - 85) / 10) * 35;
  } else if (temp_f < 105) {
    return 60 + ((temp_f - 95) / 10) * 15;
  } else if (temp_f < 115) {
    return 75 + ((temp_f - 105) / 10) * 25;
  } else {
    return 100;
  }
}

uint8_t apply_sound_dampening(uint8_t base_speed) {
  if (system_state.temp_override_active) {
    return base_speed;
  }
  
  if (!settings.quiet_mode_enabled || !system_state.sound_detected) {
    return base_speed;
  }
  
  uint8_t quiet_speed = (settings.quiet_mode_sensitivity * base_speed) / 100;
  uint8_t min_quiet_speed = 20;
  return (quiet_speed < min_quiet_speed) ? min_quiet_speed : quiet_speed;
}

// ============================================================================
// DISPLAY CONTROL - SCREEN TIMEOUT & POWER SAVING
// ============================================================================

void screen_on_event() {
  // Called whenever there's user interaction
  menu_state.last_screen_activity = millis();
  
  if (!system_state.screen_on) {
    display_wake();
  }
}

void screen_timeout_check() {
  unsigned long now = millis();
  unsigned long inactivity = now - menu_state.last_screen_activity;
  
  if (inactivity > SCREEN_TIMEOUT && system_state.screen_on) {
    // Turn off screen after timeout
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
// USER INPUT HANDLING
// ============================================================================

void handle_rotary_encoder() {
  static unsigned long last_read_time = 0;
  if (millis() - last_read_time < 5) return;
  last_read_time = millis();
  
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  
  if (current_clk != rotary_state.last_clk_state) {
    rotary_state.last_clk_state = current_clk;
    
    if (current_clk == LOW) {
      int dt_state = digitalRead(PIN_ROTARY_DT);
      
      if (dt_state == HIGH) {
        rotary_state.encoder_value++;
      } else {
        rotary_state.encoder_value--;
      }
      
      screen_on_event();  // Wake screen on encoder activity
    }
  }
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
      
      screen_on_event();  // Wake screen on button press
      
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
      } else if (menu_state.current_menu == MENU_TEMP_LIMITS) {
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
    
    if (menu_state.current_menu == MENU_MAIN) {
      menu_state.selected_option += encoder_delta;
      if (menu_state.selected_option < 0) menu_state.selected_option = 3;
      if (menu_state.selected_option > 3) menu_state.selected_option = 0;
      
    } else if (menu_state.current_menu == MENU_QUIET_SENSITIVITY) {
      menu_state.edit_value += encoder_delta * 5;
      if (menu_state.edit_value < 0) menu_state.edit_value = 0;
      if (menu_state.edit_value > 100) menu_state.edit_value = 100;
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
    case MENU_TEMP_LIMITS:
      draw_temp_limits_menu();
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

void draw_temp_limits_menu() {
  display.println("=== Temp Limits ===");
  display.println();
  
  display.print("Temp Off: ");
  display.print(settings.temp_off_threshold);
  display.println("C");
  
  display.print("Temp Max: ");
  display.print(settings.temp_max_threshold);
  display.println("C");
  
  display.println();
  display.println("[Press to go back]");
}

void draw_temp_override_menu() {
  display.println("=== Temp Override ===");
  display.println();
  display.print("Status: ");
  display.println(settings.temp_override_enabled ? "ON" : "OFF");
  
  display.println();
  display.println("When temp > 95F:");
  display.println("- Quiet mode OFF");
  display.println("- Fan full power");
  
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
  display.print(system_state.current_fan_speed);
  display.println("%");
  
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
  preferences.putUChar("temp_off", settings.temp_off_threshold);
  preferences.putUChar("temp_max", settings.temp_max_threshold);
  preferences.putBool("quiet_en", settings.quiet_mode_enabled);
  preferences.putBool("temp_override", settings.temp_override_enabled);
  
  preferences.end();
  
  debug_print("Settings saved!\n");
}

// ============================================================================
// SYSTEM RELIABILITY & UTILITIES
// ============================================================================

void feed_watchdog() {
  // Feed the watchdog to prevent auto-reset
  // This must be called at least every 60 seconds
  esp_task_wdt_reset();
}

unsigned long get_uptime_seconds() {
  return (millis() - system_state.boot_time) / 1000;
}

void format_uptime(unsigned long seconds, char* buffer, size_t max_len) {
  // Format uptime as "XdYhZm" (days, hours, minutes)
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
