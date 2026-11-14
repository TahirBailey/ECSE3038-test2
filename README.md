# ECSE3038 Test 2 - The Bathroom Fan

## Project Description
An IoT bathroom extractor fan system that monitors humidity using an ESP32 and DHT22 sensor. The fan operates automatically or manually based on settings received through MQTT.

## Hardware
- ESP32
- DHT22 humidity sensor (GPIO 15)
- Fan indicator LED (GPIO 2)

## API Setup
1. Navigate to the api folder.
2. Create and activate a Python virtual environment.
3. Install dependencies:

   pip install -r requirements.txt

4. Create a .env file containing:
   MONGODB_URI=your_mongodb_connection_string
   MQTT_NAME=tahirbailey

5. Start the API:

   python -m uvicorn app:app --reload

6. Open http://127.0.0.1:8000/docs

## Board Setup
1. Open the board folder in VS Code with PlatformIO.
2. Add your device API key to board/include/secrets.h.
3. Build the ESP32 project.
4. Start the Wokwi simulation.

## MQTT Topics
- ecse3038/tahirbailey/status
- ecse3038/tahirbailey/fan/settings
- ecse3038/tahirbailey/fan/state

## Fan Modes
- auto: Fan turns on when humidity exceeds the configured limit.
- on: Fan stays on.
- off: Fan stays off.