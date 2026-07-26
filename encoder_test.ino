/*
 * ENCODER TEST - State Machine Decoder
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
int last_dt = 1;
unsigned long last_change_time = 0;
const int DEBOUNCE_DELAY = 5;

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
  display.println("State Machine");
  display.println("Rotate knob...");
  display.display();
  
  delay(2000);
}

void loop() {
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  int current_dt = digitalRead(PIN_ROTARY_DT);
  int current_sw = digitalRead(PIN_ROTARY_SW);
  
  unsigned long now = millis();
  
  // Detect change on CLK
  if (current_clk != last_clk && (now - last_change_time) > DEBOUNCE_DELAY) {
    last_change_time = now;
    
    // CLK changed - check DT state
    if (current_clk == LOW) {
      // CLK went LOW - read DT now
      delay(2);
      current_dt = digitalRead(PIN_ROTARY_DT);
      
      if (current_dt == HIGH) {
        // CLK LOW, DT HIGH = UP
        encoder_count++;
      } else {
        // CLK LOW, DT LOW = DOWN
        encoder_count--;
      }
    }
    
    last_clk = current_clk;
  }
  
  // Also check if DT changed (catches DOWN direction better)
  if (current_dt != last_dt && (now - last_change_time) > DEBOUNCE_DELAY) {
    last_change_time = now;
    
    // DT changed - check CLK state
    if (current_dt == LOW && last_clk == LOW) {
      // Both LOW = DOWN direction
      if (encoder_count > 0) {  // Only count if we haven't gone negative
        encoder_count--;
      }
    }
    
    last_dt = current_dt;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.println("ENCODER TEST");
  display.println("(State Machine)");
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
  display.println("UP and DOWN");
  display.println("both directions");
  
  display.display();
  delay(10);
}
