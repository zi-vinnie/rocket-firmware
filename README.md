# Space Exe Rocket Firmware

Software for the Space Exe rocket, running on an **Arduino Nano ESP32**. The rocket creates its own Wi‑Fi network and shows a web page you can open on a phone or laptop to view live readings and manage flight data.

## What's working

### Wi‑Fi network and web page

The rocket broadcasts a Wi‑Fi network called **Space-Exe-Rocket** (password: `space2200ft`). Once connected, open **http://192.168.4.1** in a browser to see the dashboard with live sensor readings streamed over Server-Sent Events.

![Web dashboard](images/web_dashboard.png)

The dashboard shows:

- **Air pressure** from the BMP085 barometer (in hPa)
- **Altitude** derived from barometric pressure (in m), relative to a calibrated sea-level reference
- **Apogee** — the highest altitude reached since the last clear (in m)
- **Two temperature readings** (in °C) from dedicated temperature sensors — the display is in place, but the sensors have **not been programmed yet**

Tap the calibrated pressure value above the altitude display to **calibrate altitude** — this sets the current barometric reading as the sea-level reference (zero altitude). Calibration is disabled while recording.

### Recording flight data

You can start and stop recording from the dashboard. While recording, sensor readings are sampled every **100 ms** and appended to CSV files on flash.

**Sensor data** (`/data/sensors.csv`) has columns `timestamp`, `pressure`, `altitude`, `temperature1`, and `temperature2`. Each row includes the millisecond timestamp from boot and the current sensor values (2 decimal places).

**Apogee data** (`/data/apogee.csv`) has columns `timestamp` and `apogee`, logging the peak altitude reached so far during the recording session.

Files are flushed to flash every second so that if power is lost mid-flight, data written up to that point should remain intact.

You can **download** each CSV separately from the dashboard or **clear** both files to start fresh. Download and clear are disabled while recording.

### RGB lights (partial)

The onboard RGB LED driver is implemented in `lights.cpp` and can flash the apogee value after landing (white pulse to start, then coloured flashes per digit). This is not yet triggered automatically — the call in `setup()` is still commented out pending post-flight integration.

## Still to do

**Sensors**

- **Temperature sensors** — read the two dedicated temperature sensors properly

**Before launch**

- **Turn Wi‑Fi off from the dashboard** — disable the network during flight to save battery and reduce radio interference

**After landing**

- **RGB light apogee indicator** — call `display_apogee()` after landing to show peak altitude on the onboard LED
- **Turn Wi‑Fi back on automatically** — re-enable the network once the rocket has landed

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project targeting the `arduino_nano_esp32` board. Dependencies are listed in `platformio.ini` (ESPAsyncWebServer, Adafruit BMP085, ArduinoJson).

Upload the firmware with PlatformIO as usual. To update the web dashboard and other files on flash, uncomment `upload_protocol = esptool` in `platformio.ini`, then use "Upload Filesystem Image" from PlatformIO's menu.
