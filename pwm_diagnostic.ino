/*
 * DIAGNOSTIC: Check what PWM values are being sent to the fan
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_FAN_PWM         1
#define PIN_TEMP_SENSOR     7
#define PIN_SDA             20
#define PIN_SCL             21

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

#define FAN_PWM_FREQ        25000
#define FAN_PWM_CHANNEL     1
#define FAN_PWM_RESOLUTION  8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
OneWire oneWire(PIN_TEMP_SENSOR);
DallasTemperature tempSensor(&oneWire);

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);
  }
  
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  
  tempSensor.begin();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("PWM DIAGNOSTIC");
  display.display();
  
  delay(2000);
}

void loop() {
  tempSensor.requestTemperatures();
  float celsius = tempSensor.getTempCByIndex(0);
  float fahrenheit = (celsius * 9.0 / 5.0) + 32.0;
  
  // Calculate fan speed based on temperature
  uint8_t pwm_value = 0;
  const char* speed_str = "OFF";
  
  if (fahrenheit < 85.0) {
    pwm_value = 0;
    speed_str = "OFF";
  } else if (fahrenheit < 95.0) {
    pwm_value = (40 * 255) / 100;  // 40%
    speed_str = "LOW";
  } else {
    pwm_value = 255;  // 100%
    speed_str = "HIGH";
  }
  
  // Apply PWM
  ledcWrite(FAN_PWM_CHANNEL, pwm_value);
  
  // Display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.print("Temp: ");
  display.print(fahrenheit, 0);
  display.println("F");
  
  display.print("Speed: ");
  display.println(speed_str);
  
  display.print("PWM Value: ");
  display.print(pwm_value);
  display.println("/255");
  
  display.print("Percentage: ");
  display.print((pwm_value * 100) / 255);
  display.println("%");
  
  display.println();
  
  if (pwm_value == 0) {
    display.println("Fan should be OFF");
    display.println("If running: might be");
    display.println("a hardware issue");
  } else if (pwm_value < 100) {
    display.println("Fan should be LOW");
  } else {
    display.println("Fan should be HIGH");
  }
  
  display.display();
  delay(500);
}
