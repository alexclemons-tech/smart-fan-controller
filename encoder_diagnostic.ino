/*
 * ENCODER DIAGNOSTIC - Real-time CLK/DT pin state visualization
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define PIN_ROTARY_CLK      4
#define PIN_ROTARY_DT       5
#define PIN_ROTARY_SW       6
#define PIN_SDA             20
#define PIN_SCL             21

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);
  }
  
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("ENCODER DIAGNOSTIC");
  display.println("Watch pin states...");
  display.display();
  
  delay(2000);
}

void loop() {
  int clk = digitalRead(PIN_ROTARY_CLK);
  int dt = digitalRead(PIN_ROTARY_DT);
  int sw = digitalRead(PIN_ROTARY_SW);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.println("ENCODER DIAGNOSTIC");
  display.println();
  
  // Show current pin states
  display.print("CLK: ");
  display.println(clk ? "HIGH" : "LOW ");
  
  display.print("DT:  ");
  display.println(dt ? "HIGH" : "LOW ");
  
  display.print("SW:  ");
  display.println(sw ? "HIGH" : "LOW ");
  
  display.println();
  
  // Visual representation
  display.print("CLK: ");
  for (int i = 0; i < 10; i++) {
    display.print(clk ? "*" : " ");
  }
  display.println();
  
  display.print("DT:  ");
  for (int i = 0; i < 10; i++) {
    display.print(dt ? "*" : " ");
  }
  display.println();
  
  display.println();
  display.println("Expected patterns:");
  display.println("UP:   CLK LOW, DT HIGH");
  display.println("DOWN: CLK LOW, DT LOW");
  
  display.display();
  delay(50);
}
