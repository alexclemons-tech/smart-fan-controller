/*
 * PWM FAN TEST
 * 
 * Test if the fan PWM control is actually working
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define PIN_FAN_PWM         10
#define PIN_SDA             20
#define PIN_SCL             21

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_I2C_ADDRESS    0x3C

#define FAN_PWM_FREQ        25000
#define FAN_PWM_CHANNEL     8
#define FAN_PWM_RESOLUTION  8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t current_pwm = 0;

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (1);
  }
  
  // Setup PWM
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("PWM FAN TEST");
  display.println("GPIO 10 (Channel 8)");
  display.println();
  display.println("Rotating encoder");
  display.println("to change PWM...");
  display.display();
  
  delay(2000);
}

void loop() {
  static int last_clk = 1;
  int current_clk = digitalRead(4);  // CLK on GPIO 4
  int current_dt = digitalRead(5);   // DT on GPIO 5
  
  // Detect encoder rotation
  if (current_clk != last_clk) {
    last_clk = current_clk;
    
    if (current_clk == LOW) {
      if (current_dt == HIGH) {
        if (current_pwm < 255) current_pwm += 10;
      } else {
        if (current_pwm > 0) current_pwm -= 10;
      }
      ledcWrite(FAN_PWM_CHANNEL, current_pwm);
    }
  }
  
  // Display current PWM value
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("PWM:");
  display.print(current_pwm);
  display.print("/255");
  
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Percentage: ");
  display.print((current_pwm * 100) / 255);
  display.println("%");
  
  display.setCursor(0, 50);
  if (current_pwm == 0) {
    display.println("Fan should be OFF");
  } else if (current_pwm < 100) {
    display.println("Fan should be LOW");
  } else {
    display.println("Fan should be HIGH");
  }
  
  display.display();
  delay(10);
}
