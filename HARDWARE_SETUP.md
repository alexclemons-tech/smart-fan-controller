# Hardware Setup Guide

## Wiring Diagram Reference

### Block Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    12V INPUT (from PSU)                      │
└────────────┬────────────────────────────────────┬────────────┘
             │                                    │
       ┌─────▼─────┐                        ┌────▼────┐
       │   Buck    │                        │  Direct │
       │ Converter │                        │  12V    │
       │ 12V→5V    │                        │  Rail   │
       └─────┬─────┘                        └────┬────┘
             │                                   │
      ┌──────▼──────┐                    ┌──────▼──────┐
      │    5V Rail  │                    │   12V Rail  │
      └──────┬──────┘                    └──────┬──────┘
             │                                   │
      ┌──────┴──────┐                    ┌──────┴──────┐
      │ ESP32-C3    │                    │  MOSFET     │
      │ OLED        │                    │  IRF540N    │
      │ Sensors     │                    │  (PWM Gate) │
      │             │◄────PWM GPIO 10────┤             │
      │             │                    │             │
      │             │                    ├──────┬──────┤
      │             │                    │      │      │
      │             │                    │     Drain  Source
      │             │                    │      │      │
      │        GND ─┼────────────────────┴─────GND   12V Fan
      │             │                            │      │
      └─────────────┘                           GND───GND
```

## Step-by-Step Wiring

### Power Section (Start Here!)

1. **Buck Converter Setup**
   - Connect 12V input to buck converter VIN+ 
   - Connect GND to buck converter VIN-
   - Adjust potentiometer to output exactly **5.0V** (measure with multimeter!)
   - Output terminals: VCC (5V) and GND

2. **Power Rails**
   - Connect buck converter 5V output to a power rail (labeled 5V)
   - Connect buck converter GND to power rail (labeled GND) - **THIS IS YOUR SYSTEM GROUND**
   - Connect 12V PSU GND to system GND (common ground!)

### ESP32-C3 Super Mini

**Power Connections:**
```
Buck 5V Output  ──> ESP32 VCC (Pin 1)
System GND      ──> ESP32 GND (Pin 2, Pin 11)
```

**I2C Connections (OLED):**
```
ESP32 GPIO 19 (SDA) ──[4.7kΩ Pull-up to 5V]──> OLED SDA
ESP32 GPIO 20 (SCL) ──[4.7kΩ Pull-up to 5V]──> OLED SCL
                                                OLED GND ──> System GND
                                                OLED VCC ──> 5V Rail
```

**Sensor Connections:**

*DS18B20 (Temperature):*
```
GPIO 7 ──[4.7kΩ Pull-up to 5V]──> DS18B20 Data (Yellow wire)
System GND                        ──> DS18B20 GND (Black wire)
5V Rail                           ──> DS18B20 VCC (Red wire)
```

*GY-MAX9814 (Sound Sensor):*
```
GPIO 0 (ADC0)  ──> GY-MAX9814 OUT (Analog output)
System GND     ──> GY-MAX9814 GND
5V Rail        ──> GY-MAX9814 VCC
```

**Rotary Encoder:**
```
GPIO 4  ──[100nF Cap to GND]──> Encoder CLK (A)
GPIO 5  ──[100nF Cap to GND]──> Encoder DT (B)
GPIO 6  ──[100nF Cap to GND]──> Encoder SW (Button)
System GND                     ──> Encoder GND
5V Rail (optional stability)  ──> Encoder +5V
```

### OLED Display (SSD1306 0.96")

```
OLED Pin    Connected To         Notes
VCC         5V Rail              Power supply
GND         System GND           Ground
SDA         GPIO 19 + 4.7kΩ      I2C Data
SCL         GPIO 20 + 4.7kΩ      I2C Clock
```

### TL-C14 Fan with MOSFET PWM Control

**IMPORTANT:** The fan is 12V and the ESP32 is 3.3V, so we use a MOSFET gate driver.

**MOSFET Circuit (IRF540N or similar N-channel MOSFET):**
```
              ┌──────────────────────┐
              │   IRF540N MOSFET     │
              │                      │
GPIO 10 ──[1kΩ Resistor]──> Gate (G)│
                          │  Source (S) ──> System GND
                          │  Drain (D) ──> Fan +12V (Red)
                          └──────────────────────┘
                                     ▲
                                     │
                          Fan GND (Black) ──> System GND
                          
                   100nF Cap across Gate-Source
                   recommended for stability
```

**Pinout of IRF540N:**
```
     Gate (G) ─┐    ┌─ Drain (D)
              │    │
    ┌─────────┴────┴──────────┐
    │      IRF540N TO-220     │
    │                         │
    │    1: Gate    2: Drain  │
    │         3: Source       │
    └─────────────────────────┘
            ▼   ▼   ▼
           G   D   S
```

### Complete Pin Mapping

| ESP32 GPIO | Function | Connected To | Notes |
|-----------|----------|--------------|-------|
| 0 | ADC0 - Sound | GY-MAX9814 OUT | Analog input |
| 4 | Digital In | Rotary CLK | Input pullup enabled |
| 5 | Digital In | Rotary DT | Input pullup enabled |
| 6 | Digital In | Rotary SW | Input pullup enabled |
| 7 | One-Wire | DS18B20 Data | With 4.7kΩ pull-up |
| 10 | PWM Output | MOSFET Gate | 1kΩ series resistor |
| 19 | I2C SDA | OLED SDA | With 4.7kΩ pull-up |
| 20 | I2C SCL | OLED SCL | With 4.7kΩ pull-up |
| GND | Ground | System GND | Multiple connections |
| VCC | Power | 5V Rail | Multiple connections |

**Avoided Pins:**
- GPIO 2: BOOT pin (don't use!)
- GPIO 3: Onboard LED (reserved)
- GPIO 8, 9: USB serial (reserved)

## Resistor & Capacitor Placement

| Component | Value | Purpose | Location |
|-----------|-------|---------|----------|
| Pull-up | 4.7kΩ | I2C SDA pull-up | Near ESP32 GPIO 19 |
| Pull-up | 4.7kΩ | I2C SCL pull-up | Near ESP32 GPIO 20 |
| Pull-up | 4.7kΩ | One-Wire pull-up | Near ESP32 GPIO 7 |
| Series | 1kΩ | MOSFET gate protection | Between GPIO 10 and gate |
| Debounce | 100nF | Rotary CLK filter | Near encoder |
| Debounce | 100nF | Rotary DT filter | Near encoder |
| Debounce | 100nF | Rotary SW filter | Near encoder |
| Stability | 100nF | MOSFET gate-source | Across MOSFET G-S pins |

## Verification Checklist

Before powering on:

- [ ] Buck converter output set to 5.0V (measured with multimeter)
- [ ] ESP32 VCC connected to 5V rail
- [ ] ESP32 GND connected to system GND
- [ ] System GND connected to 12V PSU GND (common ground!)
- [ ] OLED has correct I2C address (typically 0x3C)
- [ ] All one-wire pull-ups are 4.7kΩ
- [ ] All I2C pull-ups are 4.7kΩ
- [ ] MOSFET gate has 1kΩ series resistor
- [ ] Fan 12V and GND connected correctly to MOSFET
- [ ] No loose wires or exposed connections

## Testing Before Full Assembly

1. **Power Only Test**
   - Connect 5V rail only (no 12V)
   - Check ESP32 boots (may show errors for missing sensors)
   - Verify no smoke or excess heat

2. **OLED Test**
   - Should display startup message
   - Test menu navigation with rotary encoder

3. **Sensor Tests**
   - Temperature: Should read room temperature on status screen
   - Sound: Check serial output for sound level readings
   - Rotary: Encoder should navigate menu smoothly

4. **Fan Control Test**
   - With full power (12V), set temperature above 85°F in menu
   - Fan should spin at low speed (~25%)
   - Increase simulated temp - fan should speed up
   - Test PWM with multimeter (should see varying voltage at gate)

## Troubleshooting Connections

**OLED Not Showing:**
- Verify I2C address with I2C scanner sketch
- Check SDA/SCL not swapped
- Ensure pull-ups are 4.7kΩ (not too high/low)

**Temperature Always 0:**
- One-wire pull-up missing or wrong value
- Check GPIO 7 continuity to sensor
- Try DallasTemperature sensor address scanner

**Sound Sensor Not Responding:**
- Verify GPIO 0 is not in boot mode
- Check analog voltage (0-3.3V) with multimeter
- Try reading raw ADC value

**Rotary Encoder Stuck:**
- Add 100nF debounce capacitors
- Check GPIO 4, 5 not used elsewhere
- Test with encoder sketch to verify hardware

**Fan Won't Spin:**
- Check 12V reaches MOSFET drain
- Verify gate gets PWM signal (multimeter)
- Test MOSFET with fixed PWM (50%) manually
- Ensure fan GND is connected to system GND

## Final Assembly

Once all testing passes:

1. Solder all connections or use quality breadboard
2. Add heat shrink or tape over exposed connections
3. Mount OLED in enclosure (adjust viewing angle)
4. Install rotary encoder through enclosure front
5. Route wiring away from 12V supplies
6. Use cable ties to organize wires
7. Mount fan securely to menu board
8. Seal enclosure to prevent moisture ingress (for outdoor use!)

**Pro Tips:**
- Use a small PCB with screw terminals for power distribution
- Label all connections with heat-shrink labels
- Keep 12V and 5V wires separate to avoid interference
- Use shielded cable for PWM line if experiencing noise
- Test regularly during assembly, don't wait until final installation
