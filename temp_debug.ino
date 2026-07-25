/*
 * TEMPERATURE SENSOR DEBUG
 * 
 * Display temperature readings on OLED to diagnose sensor issues
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_TEMP_SENSOR     7
#define PIN_SDA             20
#define PIN_SCL             21

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
OneWire oneWire(PIN_TEMP_SENSOR);
DallasTemperature tempSensor(&oneWire);

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);
  }
  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("TEMP DEBUG");
  display.display();
  
  delay(1000);
  
  // Initialize temperature sensor
  tempSensor.begin();
  int device_count = tempSensor.getDeviceCount();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Devices found: ");
  display.println(device_count);
  display.display();
  
  delay(2000);
}

void loop() {
  // Request temperature reading
  tempSensor.requestTemperatures();
  float celsius = tempSensor.getTempCByIndex(0);
  float fahrenheit = (celsius * 9.0 / 5.0) + 32.0;
  
  // Display on OLED
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  if (celsius == DEVICE_DISCONNECTED_C) {
    display.println("NO SENSOR!");
    display.println();
    display.setTextSize(1);
    display.println("Check GPIO 7");
    display.println("Check wiring");
  } else {
    display.print(celsius, 1);
    display.println("C");
    display.println();
    display.print(fahrenheit, 0);
    display.println("F");
  }
  
  display.display();
  delay(500);
}
