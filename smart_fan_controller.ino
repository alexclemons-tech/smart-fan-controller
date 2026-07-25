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
  unsigned long last_temp_read;
  unsigned long last_sound_check;
};

SystemState system_state = {
  .current_temp = 0,
  .current_fan_speed = 0,
  .current_sound_level = 0,
  .sound_detected = false,
  .temp_override_active = false,
  .last_temp_read = 0,
  .last_sound_check = 0
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
  bool in_edit_mode;
  unsigned long last_interaction;
};

MenuState menu_state = {
  .current_menu = MENU_MAIN,
  .selected_option = 0,
  .in_edit_mode = false,
  .last_interaction = 0
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

// Menu & UI
void handle_rotary_encoder();
void handle_button_press();
void update_display();
void draw_main_menu();
void draw_settings_menu();
void draw_quiet_mode_menu();
void draw_temp_limits_menu();
void draw_temp_override_menu();
void draw_status_screen();

// Settings
void save_settings();

// Utilities
void debug_print(const char* format, ...);

// ============================================================================
// SETUP & INITIALIZATION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial to stabilize
  
  debug_print("\n\n=== Smart Fan Controller Starting ===\n");
  
  init_pins();
  init_display();
  init_sensors();
  init_pwm();
  load_settings();
  
  debug_print("Setup complete!\n");
}

void init_pins() {
  debug_print("Initializing pins...\n");
  
  // Rotary encoder inputs
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  // Sound sensor (analog, no need to set)
  
  // Temperature sensor (one-wire, handled by library)
  
  // Fan PWM will be handled by PWM initialization
  
  rotary_state.last_clk_state = digitalRead(PIN_ROTARY_CLK);
}

void init_display() {
  debug_print("Initializing OLED display...\n");
  
  // Initialize I2C on custom pins
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    debug_print("ERROR: Failed to initialize OLED!\n");
    while (1);  // Halt if display fails
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
  
  // Initialize DS18B20
  tempSensor.begin();
  debug_print("DS18B20 initialized\n");
  
  // Initialize sound sensor (just ADC)
  analogReadResolution(10);  // 10-bit ADC (0-1023)
  debug_print("GY-MAX9814 initialized\n");
}

void init_pwm() {
  debug_print("Initializing PWM...\n");
  
  // Configure PWM on ESP32-C3 using new ledcAttach API
  // ledcAttach(pin, frequency, resolution_bits)
  ledcAttach(PIN_FAN_PWM, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcWrite(PIN_FAN_PWM, 0);  // Start at 0%
  
  debug_print("PWM configured: %d Hz, %d-bit resolution on GPIO %d\n", FAN_PWM_FREQ, FAN_PWM_RESOLUTION, PIN_FAN_PWM);
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
  
  // Update temperature (every 500ms)
  if (now - system_state.last_temp_read >= 500) {
    update_temperature();
    update_override_status();  // Check if override should be active
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
  
  // Update display
  update_display();
  
  delay(10);  // Small delay to prevent overwhelming the loop
}

// ============================================================================
// SENSOR UPDATES
// ============================================================================

void update_temperature() {
  tempSensor.requestTemperatures();
  float celsius = tempSensor.getTempCByIndex(0);
  
  // Check for sensor error
  if (celsius == DEVICE_DISCONNECTED_C) {
    debug_print("ERROR: Temperature sensor disconnected!\n");
    system_state.current_temp = 0;
    return;
  }
  
  system_state.current_temp = celsius;
  
  // Debug output every 2 seconds
  static unsigned long last_debug = 0;
  if (millis() - last_debug > 2000) {
    debug_print("Temp: %.1f°C (%.1f°F), Override: %s\n", 
                celsius, 
                celsius_to_fahrenheit(celsius),
                system_state.temp_override_active ? "ACTIVE" : "inactive");
    last_debug = millis();
  }
}

void update_override_status() {
  // Check if temperature exceeds override threshold (95°F / 35°C)
  float temp_f = celsius_to_fahrenheit(system_state.current_temp);
  
  if (settings.temp_override_enabled) {
    if (temp_f > 95.0) {
      system_state.temp_override_active = true;
    } else if (temp_f < 93.0) {  // Hysteresis: turn off at 93°F to prevent chatter
      system_state.temp_override_active = false;
    }
  } else {
    system_state.temp_override_active = false;
  }
}

void update_sound_level() {
  static uint16_t sound_samples[SOUND_AVERAGING];
  static uint8_t sample_index = 0;
  
  // Read analog value from sound sensor
  uint16_t raw_reading = analogRead(PIN_SOUND_SENSOR);
  sound_samples[sample_index] = raw_reading;
  sample_index = (sample_index + 1) % SOUND_AVERAGING;
  
  // Calculate average
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
  
  // Convert percentage to 8-bit PWM value (0-255)
  uint8_t pwm_value = (percentage * 255) / 100;
  ledcWrite(PIN_FAN_PWM, pwm_value);
}

uint8_t calculate_fan_speed_from_temp() {
  float temp_c = system_state.current_temp;
  float temp_f = celsius_to_fahrenheit(temp_c);
  
  // Temperature curve mapping
  if (temp_f < 85) {
    return 0;  // Fan off
  } else if (temp_f < 95) {
    // Linear interpolation: 25% at 85°F to 60% at 95°F
    return 25 + ((temp_f - 85) / 10) * 35;
  } else if (temp_f < 105) {
    // Linear interpolation: 60% at 95°F to 75% at 105°F
    return 60 + ((temp_f - 95) / 10) * 15;
  } else if (temp_f < 115) {
    // Linear interpolation: 75% at 105°F to 100% at 115°F
    return 75 + ((temp_f - 105) / 10) * 25;
  } else {
    return 100;  // Max speed
  }
}

uint8_t apply_sound_dampening(uint8_t base_speed) {
  // If temperature override is active, bypass quiet mode entirely
  if (system_state.temp_override_active) {
    return base_speed;  // Return full base speed without dampening
  }
  
  if (!settings.quiet_mode_enabled || !system_state.sound_detected) {
    return base_speed;  // No dampening
  }
  
  // If sound detected, reduce to configured quiet mode level
  uint8_t quiet_speed = (settings.quiet_mode_sensitivity * base_speed) / 100;
  
  // Ensure we don't go below 20% in quiet mode
  uint8_t min_quiet_speed = 20;
  return (quiet_speed < min_quiet_speed) ? min_quiet_speed : quiet_speed;
}

// ============================================================================
// USER INPUT HANDLING
// ============================================================================

void handle_rotary_encoder() {
  // Read current state with debounce
  static unsigned long last_read_time = 0;
  if (millis() - last_read_time < 5) return;
  last_read_time = millis();
  
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  
  // Detect rotation
  if (current_clk != rotary_state.last_clk_state) {
    rotary_state.last_clk_state = current_clk;
    
    if (current_clk == LOW) {
      // Determine direction
      int dt_state = digitalRead(PIN_ROTARY_DT);
      
      if (dt_state == HIGH) {
        rotary_state.encoder_value++;
        debug_print("Encoder: CW\n");
      } else {
        rotary_state.encoder_value--;
        debug_print("Encoder: CCW\n");
      }
    }
  }
}

void handle_button_press() {
  static unsigned long last_press_time = 0;
  static bool button_was_pressed = false;
  
  int button_state = digitalRead(PIN_ROTARY_SW);
  
  if (button_state == LOW && !button_was_pressed) {
    // Button just pressed
    unsigned long now = millis();
    if (now - last_press_time > 50) {  // Debounce
      debug_print("Button pressed\n");
      button_was_pressed = true;
      last_press_time = now;
      
      // Handle button press based on menu state
      // (Implementation in next section)
    }
  } else if (button_state == HIGH) {
    button_was_pressed = false;
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
  display.print("Quiet Sens: ");
  display.print(settings.quiet_mode_sensitivity);
  display.println("%");
  
  display.print("Quiet Mode: ");
  display.println(settings.quiet_mode_enabled ? "ON" : "OFF");
  
  display.print("Temp Ovrd: ");
  display.println(settings.temp_override_enabled ? "ON" : "OFF");
}

void draw_quiet_mode_menu() {
  display.println("=== Quiet Mode ===");
  display.println();
  display.print("Sensitivity: ");
  display.print(settings.quiet_mode_sensitivity);
  display.println("%");
  
  display.println("(Use knob to adjust)");
  display.println();
  
  // Draw a simple bar graph
  uint8_t bar_length = (settings.quiet_mode_sensitivity / 5);  // 0-20 chars
  display.print("[");
  for (int i = 0; i < 20; i++) {
    display.print(i < bar_length ? "*" : "-");
  }
  display.println("]");
}

void draw_temp_limits_menu() {
  display.println("=== Temp Limits ===");
  display.println();
  
  display.print("Temp Off: ");
  display.print(settings.temp_off_threshold);
  display.println("C (85F)");
  
  display.print("Temp Max: ");
  display.print(settings.temp_max_threshold);
  display.println("C (115F)");
  
  display.println();
  display.println("(Press to edit)");
}

void draw_temp_override_menu() {
  display.println("=== Temp Override ===");
  display.println();
  display.print("Status: ");
  display.println(settings.temp_override_enabled ? "ON" : "OFF");
  
  display.println();
  display.println("When temp > 95F:");
  display.println("- Quiet mode OFF");
  display.println("- Fan runs at");
  display.println("  full capacity");
  
  display.println();
  display.print("Current: ");
  if (system_state.temp_override_active) {
    display.print("ACTIVE!");
  } else {
    display.print("Inactive");
  }
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
  display.print(" [");
  display.print(system_state.sound_detected ? "ON" : "OFF");
  display.println("]");
  
  display.print("Override: ");
  display.println(system_state.temp_override_active ? "ACTIVE" : "Off");
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
// UTILITY FUNCTIONS
// ============================================================================

void debug_print(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  Serial.print(buffer);
}
