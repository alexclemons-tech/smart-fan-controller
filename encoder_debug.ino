/*
 * ENCODER HARDWARE DEBUG SKETCH
 * 
 * This standalone sketch focuses ONLY on rotary encoder debugging.
 * It reads the raw pin states and shows exactly what's happening.
 * 
 * GPIO 4  = CLK (Clock)
 * GPIO 5  = DT  (Data)
 * GPIO 6  = SW  (Switch)
 */

#define PIN_ROTARY_CLK      4
#define PIN_ROTARY_DT       5
#define PIN_ROTARY_SW       6

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ENCODER HARDWARE DEBUG ===\n");
  
  pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT_PULLUP);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  
  Serial.println("Waiting for encoder input...");
  Serial.println("Rotate the knob or press the button.\n");
}

void loop() {
  static int last_clk = 1;
  static int last_dt = 1;
  static int last_sw = 1;
  static unsigned long last_print = 0;
  static int encoder_count = 0;
  
  int clk = digitalRead(PIN_ROTARY_CLK);
  int dt = digitalRead(PIN_ROTARY_DT);
  int sw = digitalRead(PIN_ROTARY_SW);
  
  // Detect CLK state change
  if (clk != last_clk) {
    Serial.print("[CLK CHANGE] ");
    Serial.print(last_clk);
    Serial.print(" -> ");
    Serial.print(clk);
    Serial.print(" | DT=");
    Serial.print(dt);
    Serial.print(" | SW=");
    Serial.println(sw);
    
    if (clk == LOW) {
      if (dt == HIGH) {
        Serial.println("  >>> CLOCKWISE <<<");
        encoder_count++;
      } else {
        Serial.println("  >>> COUNTER-CLOCKWISE <<<");
        encoder_count--;
      }
      Serial.print("  Total count: ");
      Serial.println(encoder_count);
    }
    
    last_clk = clk;
  }
  
  // Detect DT state change
  if (dt != last_dt) {
    Serial.print("[DT CHANGE] ");
    Serial.print(last_dt);
    Serial.print(" -> ");
    Serial.print(dt);
    Serial.print(" | CLK=");
    Serial.print(clk);
    Serial.print(" | SW=");
    Serial.println(sw);
    
    last_dt = dt;
  }
  
  // Detect button press
  if (sw != last_sw) {
    Serial.print("[SW CHANGE] ");
    Serial.print(last_sw);
    Serial.print(" -> ");
    Serial.print(sw);
    Serial.print(" | CLK=");
    Serial.print(clk);
    Serial.print(" | DT=");
    Serial.println(dt);
    
    if (sw == LOW) {
      Serial.println("  >>> BUTTON PRESSED <<<");
    } else {
      Serial.println("  >>> BUTTON RELEASED <<<");
    }
    
    last_sw = sw;
  }
  
  // Print current state every 2 seconds (even if no changes)
  unsigned long now = millis();
  if (now - last_print > 2000) {
    Serial.print("[STATE] CLK=");
    Serial.print(clk);
    Serial.print(" DT=");
    Serial.print(dt);
    Serial.print(" SW=");
    Serial.print(sw);
    Serial.print(" | Count=");
    Serial.println(encoder_count);
    
    last_print = now;
  }
  
  delay(2);  // Small delay to reduce CPU load
}
