/*
 * PWM TEST - GPIO 1
 * 
 * Test if PWM works on GPIO 1 with LEDC channel 1
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define PIN_FAN_PWM         1
#define PIN_SDA             20
#define PIN_SCL             21

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t current_pwm = 0;

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);
  }
  
  // Setup PWM on GPIO 1, channel 1
  ledcSetup(1, 25000, 8);  // Channel 1, 25kHz, 8-bit
  ledcAttachPin(PIN_FAN_PWM, 1);
  ledcWrite(1, 0);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("PWM TEST - GPIO 1");
  display.println("Channel 1");
  display.println();
  display.println("Rotating encoder...");
  display.display();
  
  delay(2000);
}

void loop() {
  static int last_clk = 1;
  int current_clk = digitalRead(4);
  int current_dt = digitalRead(5);
  
  if (current_clk != last_clk) {
    last_clk = current_clk;
    
    if (current_clk == LOW) {
      if (current_dt == HIGH) {
        if (current_pwm < 255) current_pwm += 10;
      } else {
        if (current_pwm > 0) current_pwm -= 10;
      }
      ledcWrite(1, current_pwm);
    }
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("PWM:");
  display.print(current_pwm);
  display.print("/255");
  
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Percent: ");
  display.print((current_pwm * 100) / 255);
  display.println("%");
  
  display.setCursor(0, 50);
  if (current_pwm == 0) {
    display.println("Fan should be OFF");
  } else if (current_pwm < 100) {
    display.println("Fan should be SLOW");
  } else {
    display.println("Fan should be FAST");
  }
  
  display.display();
  delay(10);
}
