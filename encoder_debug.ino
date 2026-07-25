/*
 * ENCODER DEBUG USING OLED DISPLAY
 * 
 * Since Serial Monitor isn't working, we'll display everything on the OLED.
 * This will show encoder state in real-time on the screen.
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

int encoder_count = 0;
int clk_state = 1;
int dt_state = 1;
int sw_state = 1;
unsigned long last_change = 0;

void setup() {
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  // Initialize display
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);  // Hang if display fails
  }
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("ENCODER DEBUG");
  display.println("Rotate knob...");
  display.display();
  
  delay(1000);
  
  // Read initial state
  clk_state = digitalRead(PIN_ROTARY_CLK);
  dt_state = digitalRead(PIN_ROTARY_DT);
  sw_state = digitalRead(PIN_ROTARY_SW);
}

void loop() {
  static unsigned long last_update = 0;
  
  int new_clk = digitalRead(PIN_ROTARY_CLK);
  int new_dt = digitalRead(PIN_ROTARY_DT);
  int new_sw = digitalRead(PIN_ROTARY_SW);
  
  // Detect CLK state change
  if (new_clk != clk_state) {
    clk_state = new_clk;
    last_change = millis();
    
    if (new_clk == LOW) {
      if (new_dt == HIGH) {
        encoder_count++;
      } else {
        encoder_count--;
      }
    }
  }
  
  // Detect DT state change
  if (new_dt != dt_state) {
    dt_state = new_dt;
    last_change = millis();
  }
  
  // Detect button change
  if (new_sw != sw_state) {
    sw_state = new_sw;
    last_change = millis();
  }
  
  // Update display every 100ms
  if (millis() - last_update > 100) {
    last_update = millis();
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    // Show encoder count large
    display.println("COUNT:");
    display.print(encoder_count);
    
    // Show pin states small
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("CLK=");
    display.print(clk_state);
    display.print("  DT=");
    display.print(dt_state);
    display.print("  SW=");
    display.println(sw_state);
    
    // Show direction indicator
    display.setCursor(0, 40);
    if (millis() - last_change < 300) {
      if (clk_state == LOW) {
        if (dt_state == HIGH) {
          display.println(">>> CLOCKWISE >>>");
        } else {
          display.println("<<< COUNTER-CW <<<");
        }
      }
      if (sw_state == LOW) {
        display.println("*** BUTTON PRESSED ***");
      }
    }
    
    // Instructions
    display.setCursor(0, 55);
    display.println("Rotate or press!");
    
    display.display();
  }
  
  delay(2);
}
