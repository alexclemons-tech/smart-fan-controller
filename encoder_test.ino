/*
 * ENCODER RAW PIN STATE DUMP
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

// Store state changes
struct StateChange {
  unsigned long time;
  int clk;
  int dt;
  int sw;
};

const int MAX_CHANGES = 100;
StateChange changes[MAX_CHANGES];
int change_count = 0;

int last_clk = 1;
int last_dt = 1;
int last_sw = 1;

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
  display.println("RAW PIN STATE DUMP");
  display.println("Rotate encoder");
  display.println("Press button to");
  display.println("see the log");
  display.display();
}

void loop() {
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  int current_dt = digitalRead(PIN_ROTARY_DT);
  int current_sw = digitalRead(PIN_ROTARY_SW);
  
  // Log any change
  if ((current_clk != last_clk || current_dt != last_dt || current_sw != last_sw) && change_count < MAX_CHANGES) {
    changes[change_count].time = millis();
    changes[change_count].clk = current_clk;
    changes[change_count].dt = current_dt;
    changes[change_count].sw = current_sw;
    change_count++;
    
    last_clk = current_clk;
    last_dt = current_dt;
    last_sw = current_sw;
  }
  
  // Check if button pressed
  if (current_sw == LOW) {
    delay(500);
    display_log();
    delay(3000);
  }
}

void display_log() {
  int page = 0;
  int start_idx = 0;
  
  while (true) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    
    display.print("Log Page ");
    display.print(page + 1);
    display.print(" of ");
    display.println((change_count + 3) / 4);
    
    display.println();
    
    // Show 4 changes per page
    for (int i = 0; i < 4 && (start_idx + i) < change_count; i++) {
      int idx = start_idx + i;
      display.print("T");
      display.print(changes[idx].time);
      display.print(" C:");
      display.print(changes[idx].clk);
      display.print(" D:");
      display.print(changes[idx].dt);
      display.print(" S:");
      display.println(changes[idx].sw);
    }
    
    display.println();
    display.println("Press button");
    display.println("for next page");
    
    display.display();
    
    // Wait for button or timeout
    unsigned long button_wait_start = millis();
    while (millis() - button_wait_start < 2000) {
      if (digitalRead(PIN_ROTARY_SW) == LOW) {
        page++;
        start_idx += 4;
        if (start_idx >= change_count) {
          reset_log();
          return;
        }
        delay(500);
        break;
      }
      delay(10);
    }
    
    if (millis() - button_wait_start >= 2000) {
      reset_log();
      return;
    }
  }
}

void reset_log() {
  change_count = 0;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Log cleared!");
  display.println("Rotate to record");
  display.println("Press to view");
  display.display();
  delay(2000);
}
