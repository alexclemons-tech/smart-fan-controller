void handle_rotary_encoder() {
  static unsigned long last_read = 0;
  static unsigned long last_count_time = 0;
  const int MIN_PULSE_INTERVAL = 15;  // Minimum time between counts (ms)
  
  unsigned long now = millis();
  
  if (now - last_read < 2) return;
  last_read = now;
  
  int current_clk = digitalRead(PIN_ROTARY_CLK);
  int current_dt = digitalRead(PIN_ROTARY_DT);
  
  // Only count if enough time has passed (prevents bouncing)
  if (current_clk != rotary_state.last_clk && (now - last_count_time) > MIN_PULSE_INTERVAL) {
    rotary_state.last_clk = current_clk;
    
    // Only count on falling edge (HIGH to LOW)
    if (current_clk == LOW) {
      // UP direction only - always increment
      rotary_state.encoder_delta++;
      last_count_time = now;
    }
    
    screen_on_event();
  }
}
