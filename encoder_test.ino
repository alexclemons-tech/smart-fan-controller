/*
 * ENCODER TEST - Minimum Pulse Width Filter
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define PIN_ROTARY_CLK      5
#define PIN_ROTARY_DT       4
#define PIN_ROTARY_SW       6
#define PIN_SDA             20
#define PIN_SCL             21

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int encoder_count = 0;
int last_clk = 1;
unsigned long last_count_time = 0;
const int MIN_PULSE_INTERVAL = 15;  // Minimum time between counts (ms)

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
  display.println("ENCODER TEST");
  display.println("Pulse Width Filter");
  display.println("Rotate knob...");
  display.display();
  
  delay(2000);
}

void loop() {
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  int current_dt = digitalRead(PIN_ROTARY_DT);
  int current_sw = digitalRead(PIN_ROTARY_SW);
  
  unsigned long now = millis();
  
  // Only count if enough time has passed since last count
  if (current_clk != last_clk && (now - last_count_time) > MIN_PULSE_INTERVAL) {
    last_clk = current_clk;
    
    if (current_clk == LOW) {
      delay(1);
      int dt_stable = digitalRead(PIN_ROTARY_DT);
      
      if (dt_stable == HIGH) {
        encoder_count++;
      } else {
        encoder_count--;
      }
      
      last_count_time = now;
    }
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.println("ENCODER TEST");
  display.println("(15ms Min Pulse)");
  display.println();
  
  display.print("CLK: ");
  display.println(current_clk ? "HIGH" : "LOW");
  
  display.print("DT: ");
  display.println(current_dt ? "HIGH" : "LOW");
  
  display.print("SW: ");
  display.println(current_sw ? "HIGH" : "LOW");
  
  display.println();
  display.setTextSize(2);
  display.print("Count: ");
  display.println(encoder_count);
  
  display.setTextSize(1);
  display.println();
  display.println("Should be smooth");
  display.println("both directions");
  
  display.display();
  delay(10);
}
