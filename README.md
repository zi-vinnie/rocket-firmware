# Space Exe Rocket Firmware

Software for the Space Exe rocket, running on an **Arduino Nano ESP32**. The rocket creates its own Wi‑Fi network and shows a web page you can open on a phone or laptop to view live readings and manage flight data.

## What's working

### Wi‑Fi network and web page

The rocket broadcasts a Wi‑Fi network called **Space-Exe-Rocket** (password: `space2200ft`). Once connected, open **http://192.168.4.1** in a browser to see the dashboard with live sensor readings streamed over Server-Sent Events.

![Web dashboard](images/web_dashboard.png)

The dashboard shows:

- **Air pressure** from the BMP085 barometer (in hPa)
- **Altitude** derived from barometric pressure (in m), relative to a calibrated sea-level reference
- **Apogee** — the highest altitude reached since the last clear (in m); restored from `apogee.csv` on boot if a previous session was recorded
- **Two temperature readings** (in °C) from dedicated temperature sensors — the display is in place, but the sensors have **not been programmed yet**

Tap the calibrated pressure value above the altitude display to **calibrate altitude** — this sets the current barometric reading as the sea-level reference (zero altitude). Calibration is disabled while recording.

### Recording flight data

You can start and stop recording from the dashboard. While recording, sensor readings are sampled every **100 ms** and appended to CSV files on flash.

**Sensor data** (`/data/sensors.csv`) has columns `timestamp`, `pressure`, `altitude`, `temperature1`, and `temperature2`. Each row includes the millisecond timestamp from boot and the current sensor values (2 decimal places).

**Apogee data** (`/data/apogee.csv`) has columns `timestamp` and `apogee`, logging the peak altitude reached so far during the recording session.

Files are flushed to flash every second so that if power is lost mid-flight, data written up to that point should remain intact.

You can **download** each CSV separately from the dashboard or **clear** both files to start fresh. Download and clear are disabled while recording. While recording, you can also **disable Wi‑Fi** to save battery and reduce radio interference. Wi‑Fi comes back on the next boot (e.g. after pressing reset).

### RGB apogee indicator

After a flight, pressing the **reset button** reboots the rocket. If apogee data exists from a previous recording, the onboard RGB LED flashes the peak altitude before Wi‑Fi starts: a white pulse, then each digit as coloured flashes (red, green, and blue cycle per digit). A decimal point is shown as a short white flash.

## How to use

Connect to the rocket's Wi‑Fi network (**Space-Exe-Rocket**, password `space2200ft`) and open **http://192.168.4.1** in a browser.

### Before launch

1. **Clear data** — tap Clear on the dashboard to wipe any old flight files.
2. **Start recording** — tap Start Recording on the dashboard.
3. **Turn off Wi‑Fi** — tap Disable Wi‑Fi. You will lose connection; that is expected.

### After landing

1. **Press the reset button** on the Arduino to reboot the rocket.
2. **Read apogee from the blinking light** — watch the RGB LED for the peak altitude display.
3. **Connect to Wi‑Fi** from your phone or laptop (the network is back after reset).
4. **Download data** from the dashboard (`sensors.csv` and `apogee.csv`) for processing.

## Still to do

- **Temperature sensors** — read the two dedicated temperature sensors properly

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project targeting the `arduino_nano_esp32` board. Dependencies are listed in `platformio.ini` (ESPAsyncWebServer, Adafruit BMP085, ArduinoJson).

Upload the firmware with PlatformIO as usual. To update the web dashboard and other files on flash, uncomment `upload_protocol = esptool` in `platformio.ini`, then use "Upload Filesystem Image" from PlatformIO's menu.
