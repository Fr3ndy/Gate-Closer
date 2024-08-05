# Gate-Closer

ESP32-based project for automatic sliding gate management via a web interface.

## Project Description

This project allows controlling a sliding gate using an ESP32-based control unit. The ESP32 is connected to a web interface that enables opening, closing, stopping the gate, and managing the courtesy light settings. The system uses a keypad for entering a security code to perform the main operations.

## Electrical Diagram

### Materials Used
- ESP32
- Relay Module
- 4x4 Keypad
- LED
- 5V Power Supply
- Various components (resistors, connecting wires, breadboard)

### Electrical Connections
| ESP32 Pin    | Component       | Description              |
|--------------|-----------------|--------------------------|
| 12           | Relay OPEN      | Gate opening             |
| 33           | Relay STOP      | Gate stop                |
| 14           | Relay LIGHT     | Courtesy light           |
| 35           | Gate OPEN       | Open gate sensor         |
| 26           | Gate OPEN LED   | Open gate LED            |
| 34           | Gate CLOSE      | Closed gate sensor       |
| 25           | Gate CLOSE LED  | Closed gate LED          |
| 4, 16, 17, 5 | Keypad ROWS     | Keypad rows              |
| 18, 19, 21, 22 | Keypad COLS   | Keypad columns           |

The electrical diagram will be available soon.

## Features

### Gate Control
- **Open**: Opens the gate.
- **Stop**: Stops the gate movement.
- **Stay Open**: Keeps the gate open.

### Courtesy Light Management
- Enable/disable the courtesy light.
- Set the activation and deactivation time of the light.
- Set the activation duration of the light.

### Security
- Enter a password via keypad to perform operations on the gate.
- Configure system behavior based on the entered password.

### Web Interface
- Control and manage the gate via a web page.
- View the gate and light status.
- Configure settings through the web interface.

## Project Configuration

### Requirements
- Arduino IDE
- Libraries: WiFi, WebServer, EEPROM, ArduinoJson, Keypad, Ticker

### Installation
1. Clone the repository.
2. Open the `ESP32.ino` file with Arduino IDE.
3. Modify the WiFi credentials in the code:
    ```cpp
    const char* ssid = "****";
    const char* password = "****";
    ```
4. Upload the code to the ESP32.
5. Connect the ESP32 to the circuit as described in the electrical diagram.

### Usage
1. Power on the ESP32 and connect to the configured WiFi network.
2. Open a browser and navigate to the ESP32's IP address (displayed in the serial monitor).
3. Use the web interface to control the gate and manage settings.

## Documentation

### Server Response Code Map
- `{ "code": 1 }` - Open operation completed successfully.
- `{ "code": 2 }` - Stop operation completed successfully.
- `{ "code": 3 }` - Gate in motion.
- `{ "code": 4 }` - Fixed open operation completed successfully.
- `{ "code": 5 }` - Deserialization error during setting.
- `{ "code": 6 }` - Settings saved successfully.
- `{ "code": 7 }` - No data sent for setting.
- `{ "code": 8 }` - Settings returned successfully.
- `{ "code": 9 }` - Gate status returned successfully.

### Useful Resources

- [ESP32](https://it.aliexpress.com/item/1005006336964908.html?spm=a2g0o.productlist.main.1.74dcJYYTJYYT9c&algo_pvid=b82dedef-8127-41d0-acf1-41ef19eff18a&algo_exp_id=b82dedef-8127-41d0-acf1-41ef19eff18a-0&pdp_npi=4%40dis%21EUR%219.01%212.52%21%21%2168.65%2119.22%21%4021039f3e17228458748548204e433c%2112000036806447870%21sea%21IT%212760666466%21X&curPageLogUid=iDSbUGOZlJSm&utparam-url=scene%3Asearch%7Cquery_from%3A)
- [Closer | Euromatic Gate](https://euromaticgate.net/prodotto/closer/#closer_5_8_15_115v)
- [Q81S Control Unit Manual](https://euromaticgate.net/wp-content/uploads/2022/03/Q81S_09_2021_it_rev01_22.pdf)

## Contributions

We welcome contributions! Feel free to fork the project, submit pull requests, or open issues for suggestions and improvements.

## Language Note

The code is currently in Italian and will be translated into English soon.

## License

This project is distributed under the MIT License. See the `LICENSE` file for more details.