# Smart Fan Controller

A smart fan controller for outdoor menu boards using an ESP32-C3 Super Mini with temperature-based speed control and sound-reactive mode.

## Features

- **Temperature-based PWM fan control** with configurable curves
- **Sound-reactive mode** that reduces fan speed during speech detection
- **OLED menu system** for easy configuration
- **Rotary encoder** navigation and adjustment
- **Persistent settings** stored in ESP32 flash memory

## Hardware Components

| Component | Model | Purpose |
|-----------|-------|---------|
| Microcontroller | ESP32-C3 Super Mini | Main controller |
| Temperature Sensor | DS18B20 | Monitor enclosure temperature |
| Sound Sensor | GY-MAX9814 | Detect speech/audio |
| Fan | TL-C14 (12V PWM) | Active cooling |
| Display | SSD1306 OLED 0.96" | User interface |
| Encoder | Rotary Encoder KY-040 | Menu navigation |
| Power | Buck Converter | 12V → 5V regulation |

## Pin Assignment

| Component | Pin | Notes |
|-----------|-----|-------|
| Rotary CLK | GPIO 4 | Input with pullup |
| Rotary DT | GPIO 5 | Input with pullup |
| Rotary SW | GPIO 6 | Button with pullup |
| Fan PWM | GPIO 10 | PWM output |
| Temp Sensor | GPIO 7 | One-Wire protocol |
| Sound Sensor | GPIO 0 | Analog ADC0 |
| OLED SDA | GPIO 19 | I2C |
| OLED SCL | GPIO 20 | I2C |

**Avoided pins:** GPIO 2 (BOOT), GPIO 8-9 (USB), GPIO 3 (onboard LED)

## Temperature Curve

The fan speed is automatically controlled based on temperature:

| Temperature | Fan Speed |
|-------------|-----------|
| < 85°F | 0% (Off) |
| 85°F - 95°F | 25% - 60% (Linear) |
| 95°F - 105°F | 60% - 75% (Linear) |
| 105°F - 115°F | 75% - 100% (Linear) |
| > 115°F | 100% (Max) |

## Menu System

The OLED display provides navigation through:

1. **Main Menu** - Select from Status, Settings, Quiet Mode, or Temp Limits
2. **Status Screen** - Real-time display of temperature, fan speed, and sound level
3. **Quiet Mode Settings** - Adjust speech detection sensitivity (0-100%)
4. **Temperature Limits** - Configure fan activation and maximum speed thresholds
5. **Settings Menu** - View all current configuration

### Controls

- **Rotary Knob (CW/CCW)** - Navigate menu options or adjust values
- **Rotary Button** - Select/confirm menu items

## Quiet Mode

When enabled, the sound sensor detects speech and reduces fan noise:

- **Sensitivity Range:** 0-100% (adjustable)
- **Minimum Quiet Speed:** 20% (ensures air circulation)
- **Detection Threshold:** ADC value > 100 (configurable)

Example: If base fan speed is 75% and quiet mode is set to 50% sensitivity, fan reduces to ~37.5% when speech is detected.

## Libraries Required

Install via Arduino IDE Library Manager:

- `Adafruit SSD1306` - OLED display driver
- `Adafruit GFX Library` - Graphics library (dependency)
- `DallasTemperature` - DS18B20 temperature sensor
- `OneWire` - One-Wire protocol (dependency)

## Installation

1. Clone this repository
2. Open `smart_fan_controller.ino` in Arduino IDE
3. Install required libraries (see above)
4. Select board: **ESP32-C3 Super Mini**
5. Configure upload settings:
   - Board: `esp32c3`
   - Flash Size: `4MB`
   - Flash Freq: `80MHz`
6. Upload to device
7. Connect via Serial Monitor (115200 baud) for debug output

## Hardware Wiring

### Power Distribution

```
12V Input (from Buck Converter ground return)
├─ 5V Output → ESP32 VCC, OLED VCC, Sensor VCC
└─ 12V Output → Fan VCC (via MOSFET gate controlled by PWM)
```

### MOSFET Driver (for 12V fan)

The TL-C14 fan requires a MOSFET gate driver (e.g., IRF540N):

```
GPIO 10 (PWM) ──[1kΩ]──> Gate (MOSFET)
                         Drain → 12V Fan (+)
                         Source → GND
                         12V GND → System GND
```

## Configuration

Edit the constants at the top of the sketch to customize:

- `TEMP_*` - Temperature thresholds (in Celsius)
- `FAN_PWM_FREQ` - PWM frequency (typically 25kHz for fans)
- `SOUND_THRESHOLD` - Speech detection sensitivity
- `SOUND_AVERAGING` - Noise filtering samples

## Troubleshooting

### Display not showing

- Check I2C address (default 0x3C) - scan with I2C scanner sketch
- Verify SDA/SCL connections to GPIO 19/20
- Ensure pull-up resistors on I2C lines (typically 4.7kΩ)

### Temperature reading stuck at 0°C

- Check one-wire connection to GPIO 7
- Verify 4.7kΩ pull-up resistor on data line
- Check DS18B20 sensor address with OneWire scanner

### Fan not responding to PWM

- Verify MOSFET gate connected to GPIO 10
- Check 12V supply to fan and MOSFET
- Test with fixed PWM speed to isolate software/hardware issues

### Rotary encoder not working

- Check GPIO 4, 5, 6 connections
- Verify pull-up resistors enabled (done in code)
- Add 100nF debounce capacitors if needed

### Sound sensor always detecting speech

- Adjust `SOUND_THRESHOLD` lower (more sensitive)
- Check `SOUND_AVERAGING` value for noise filtering
- Verify analog voltage from sensor is 0-3.3V

## Future Enhancements

- [ ] WiFi connectivity for remote monitoring
- [ ] Data logging to SD card
- [ ] Over-the-air firmware updates
- [ ] Predictive fan speed based on forecast
- [ ] Motion sensor to enable/disable controller
- [ ] Sunrise/sunset automatic scheduling

## License

MIT License - Feel free to use and modify for your projects!
