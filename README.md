# ABreezeBot

ABreezeBot is a smart relay automation system with IR remote control and automatic mode based on temperature readings. It’s ideal for controlling fans, heaters, or other appliances in DIY home automation setups.

---

## 🌟 Features

- 🌡️ **Auto Mode** – Turns the connected device ON/OFF based on temperature thresholds using a thermistor sensor.
- 📡 **IR Remote Control** – Manual control with an infrared remote (supports NEC protocol by default).
- ⚡ **Relay Output** – Controls any appliance connected to a relay safely.
- 🟢 **Auto Mode LED Indicator** – Shows when Auto Mode is active.
- 🔄 **Seamless Mode Switching** – Toggle between manual and automatic operation.

---

## 🛠️ Hardware Requirements

- Arduino board (e.g., Uno, Nano)
- 5V relay module
- IR receiver module (Hx1838 or similar)
- 103 Thermistor (10kΩ NTC)
- 10kΩ resistor (for thermistor voltage divider)
- Status LED (+ resistor)
- IR remote (NEC protocol preferred)
- Breadboard and jumper wires
- Power source (USB or external)

---

## 📐 Wiring Diagram

```
Thermistor ---- A0 --- 10kΩ Resistor --- GND
              ↳ 5V

Relay     ----- 5V
              ↳ GND
              ↳ Pin 7

IR Receiver --- 3.3V
              ↳ GND
              ↳ Pin 2

LED        ---- 470Ω Resistor ---- Pin 4
              ↳ GND
            
```

---

## 📦 Installation

1. Clone the repository:  
   ```bash
   git clone https://github.com/yourusername/ABreezeBot.git
   ```

2. Open the project in Arduino IDE or PlatformIO.

3. Install required library:  
   [IRremote by Arduino-IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote)

---

## ⚙️ Configuration

Adjust these settings in `ABreezeBot.ino` to fit your needs:

```cpp
// Pins
#define IR_RECEIVER_PIN 2
#define RELAY_PIN 7
#define LED_PIN 4              // Auto Mode LED
#define THERMISTOR_PIN A0

// Relay logic level
#define RELAY_ON_LEVEL HIGH

// Auto Mode
bool AUTO_MODE_ENABLED = false;           // Default state of Auto Mode
const float TEMPERATURE_THRESHOLD = 19.0; // °C
const float temperatureVariationTolerance = 0.5; // °C hysteresis
#define AUTO_MODE_READ_INTERVAL 360000    // 6 minutes in ms
```

---

## ▶️ Usage

| Action                  | IR Remote Button (NEC Code) |
|------------------------|-----------------------------|
| Toggle relay ON/OFF     | (configure in code)         |
| Enable/Disable Auto Mode| (configure in code)         |

When Auto Mode is enabled:  
- The LED (Pin 8) turns ON as an indicator.  
- The system checks temperature every 6 minutes.  
- Relay activates if:  
  - Temp ≤ threshold - tolerance → Relay ON  
  - Temp ≥ threshold + tolerance → Relay OFF

---

## 📖 How It Works

1. Manual Mode: IR remote controls relay directly.  
2. Auto Mode: Temperature readings control the relay automatically.  
3. LED Indicator: Shows Auto Mode status (ON = Auto Mode active).

---

## 🧑‍💻 Contributing

Pull requests are welcome. Open an issue to discuss bugs or improvements.

---

## 📜 License

MIT License – see LICENSE file.

---

## 💡 Future Enhancements

- OLED display for temperature and mode status.
- Configurable thresholds via remote.
- Add Wi-Fi or Bluetooth remote control.
