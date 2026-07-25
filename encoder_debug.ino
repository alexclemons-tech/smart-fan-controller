/*
 * MINIMAL BLINK TEST
 * 
 * If this works, we'll see the Serial output.
 * If not, there's a board issue.
 */

void setup() {
  // Delay before starting Serial - gives ESP32 time to boot
  delay(2000);
  
  Serial.begin(115200);
  
  // Send some boot markers
  Serial.write(0xFF);
  Serial.write(0xFE);
  delay(100);
  
  Serial.println("\n\n");
  Serial.println("=====================================");
  Serial.println("MINIMAL BLINK TEST - BOARD IS ALIVE");
  Serial.println("=====================================\n");
  Serial.flush();
}

void loop() {
  Serial.print(".");
  Serial.flush();
  delay(1000);
}
