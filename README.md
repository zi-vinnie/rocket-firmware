# Space Exe Rocket Firmware

Software for the Space Exe rocket, running on an **Arduino Nano ESP32**. The rocket creates its own Wi‑Fi network and shows a web page you can open on a phone or laptop to view live readings and manage flight data.

## What's working

### Wi‑Fi network and web page

The rocket broadcasts a Wi‑Fi network called **Space-Exe-Rocket**. Once connected, open **http://192.168.4.1** in a browser to see the dashboard with live sensor readings.

![Web dashboard](images/web_dashboard.png)

The dashboard shows:

- **Air pressure** from the barometer (in hPa)
- **Two temperature readings** (in °C) — the display is in place, but the temperature sensors have **not been programmed yet**

### Recording flight data

You can start and stop recording from the dashboard. While recording, sensor readings are sampled every **100 ms** and appended to a CSV file.

The file has a header row with columns `timestamp`, `pressure`, `temperature1`, and `temperature2`. Each data row includes the millisecond timestamp from boot and the current sensor values (2 decimal places). The file is flushed to flash every second so that if power is lost mid-flight, data written up to that point should remain intact.

You can **download** the CSV from the dashboard or **clear** it to start fresh.

## Still to do

**Sensors**

- **Temperature sensors** — wire up and read the two temperature sensors properly

**Before launch**

- **Turn Wi‑Fi off from the dashboard** — disable the network during flight to save battery and reduce radio interference

**After landing**

- **RGB light apogee indicator** — flash the onboard colour LED to show peak altitude (e.g. number of flashes for digit count, then one flash per digit)
- **Turn Wi‑Fi back on automatically** — re-enable the network once the rocket has landed
- **Peak altitude on the dashboard** — calculate and show how high the rocket went
