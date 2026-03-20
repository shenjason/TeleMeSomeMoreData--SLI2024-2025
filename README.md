# TeleMeSomeMoreData

![PlatformIO](https://img.shields.io/badge/PlatformIO-embedded-orange?style=flat&logo=platformio)
![Board](https://img.shields.io/badge/Board-Teensy%204.1-blue?style=flat)
![Radio](https://img.shields.io/badge/Radio-446.5%20MHz%20LoRa-purple?style=flat)
![Unity](https://img.shields.io/badge/Display-Unity%20Engine-black?style=flat&logo=unity)
![License](https://img.shields.io/badge/license-GPL--3.0-green?style=flat)


### Flight and Ground station software for the **AIAA OC Section** entry in [NASA SLI](https://www.nasa.gov/learning-resources/nasa-student-launch/) 2024-2025 competition.

**"TeleMeSomeMoreData"**, is the payload for the 2024-2025 season, the software include three parts: 
Flight software, Ground station control software, and the [Ground Station Display software](https://github.com/shenjason/GroundStationDisplay--SLI2024-2025/). The flight software continuously transmits 82-byte binary telemetry packets from the rocket over LoRa radio at 446.5 MHz. On the ground, the ground station control software decodes incoming packets, computes azimuth and altitude angles via the Haversine formula, and drives a dual-axis stepper gimbal to autonomously point a directional Yagi antenna at the rocket in flight. Live telemetry is streamed over serial to the Ground Station Display Software running on a laptop. A separate 1.2 GHz analog video link transmits live video from the payload to the ground station independently of the telemetry system.


### Team

- **Mentors:** Binay Pandey, Janet Koepke, Robert Koepke
- **Payload Software Lead:** Jason Shen (Owner of repository)
- **Project Manager:** Bingxuan Cheng
- **Avionics Lead:** William Yuan

| | |
|---|---|
| ![Team at NASA SLI Competition](docs/images/IMG_9518.JPG) | ![Ground Station at NASA SLI Launch](docs/images/IMG_9562.JPG) |

---

## 2023-2024 Season (Year 1)

The first year established the core telemetry pipeline: a Teensy 4.1 flight computer collecting IMU, barometric, GPS, and temperature data, packing it into a binary LoRa packet. A basic receive-only ground station decoded the packets and printed labeled fields to a simple ground station interface. Manual antenna pointing was required.

| | |
|---|---|
| ![Subscale Test 2023-2024](docs/images/IMG_7101.jpg) | ![Launch Site 2023-2024](docs/images/IMG_7107.jpg) |

---

## 2024-2025 Season (Year 2) — What We Added

Building on year one, this year added:

- **Autonomous alt-azimuth antenna tracking** — dual-axis stepper gimbal that locks onto and follows the rocket in real time using GPS telemetry from each received packet
- **Improved Unity ground station display** — full 3D flight visualization with live gauges for acceleration, velocity, altitude, GPS position, temperature, pressure, humidity, eCO2, and TVOC
- **More sensors for more data** — air quality sensors (SGP30: TVOC, eCO2) added to subscale firmware; GPS multi-constellation support (GLONASS, Galileo, BeiDou)
- **GPS self-calibration** — ground station now has it's own gps and can acquire its own fix at startup and use it as the reference coordinate, eliminating manual coordinate entry
- **Live video downlink** — a 1.2 GHz analog video transmitter on the payload streams live video to a 12-channel receiver at the ground station, separate from the LoRa telemetry system

| | |
|---|---|
| ![Alt-Azimuth Gimbal Close-Up](docs/images/IMG_9593.JPG) | ![Ground Station at Launch](docs/images/IMG_9588.JPG) |

---

## Repository Structure

```
AIAA_SLI2024-2025/
├── fullscale_flight/          # Payload flight computer firmware
│   ├── platformio.ini
│   └── src/main.cpp
├── fullscale_groundstation/   # Ground station tracking & receive firmware
│   ├── platformio.ini
│   └── src/main.cpp
├── subscale_v1/               # Subscale test firmware V1
│   ├── platformio.ini
│   └── src/main.cpp
├── subscale_v2/               # Subscale test firmware V2 (multi-constellation GPS)
│   ├── platformio.ini
│   └── src/main.cpp
└── data/                      # Recorded flight data logs
    └── DataRaw.csv
```

Each subdirectory is an independent [PlatformIO](https://platformio.org/) project.

---

## Flight Software (`fullscale_flight`)

The flight firmware runs on a **Teensy 4.1** inside the rocket. On every loop it polls all sensors, builds a fixed 82-byte binary packet, writes it to the onboard SD card, and transmits it over LoRa radio.

### Flowchart

Each sensor initializes in order with buzzer audio feedback. A short tone starts each step, a longer tone confirms success, a 3-second low freq tone halts on failure. Here's the flowchart for the flight software:

```
                    [Start]
                       |
              [Initialize Radio Module]
              /                      \
          Fail                     Success
            \                         |
             v              [Initialize Sensors (GPS, IMU, etc)]
          [Halt] <----Fail---/              \---Success
             ^                        [Initialize SD card]
             \--------------Fail-----/
                                          |
                                 [Buzzer Beep for 1s]
                                          |
                        +--------->[Sample Sensor Data]
                        |                 |
                        |    [Is Sensor Data complete?]
                        |         /               \
                        +---No---+               Yes
                                          |
                                [Store sensor data in SD card]
                                          |
                              [Send Sensor Data to Radio Module]
                                          |
                                  (Loop to "Sample Sensor Data")
```

The firmware **blocks until a GPS fix** before completing startup, ensuring every packet contains valid position data.

### Main Loop

```
UpdateGPS() + UpdateBNO() + UpdateBME()
  → if all sensors fresh → build binary packet → transmit LoRa
```

- **BNO055** — polled every 10 ms: Euler angles, linear acceleration
- **BME280** — polled every 10 ms: barometric altitude, pressure, humidity
- **GPS** — polled every loop for new NMEA sentences
- **Thermistor** — temperature via Steinhart-Hart equation on ADC pin A13

### Telemetry Packet Format (82 bytes)

Packets are serialized into a flat binary buffer via a `union Convert` that reinterprets `float`/`int`/`uint` as raw bytes. A 4-byte `"AIAA"` magic header allows the ground station to validate packets before parsing. The buffer is always transmitted at 82 bytes with zeros padding unused space.

| Field | Type | Bytes |
|-------|------|-------|
| Header (`AIAA`) | char[4] | 4 |
| Packet Number | uint32 | 4 |
| Elapsed Time (ms) | uint32 | 4 |
| Rotation X, Y, Z (Euler rad) | float×3 | 12 |
| Linear Acceleration X, Y, Z (m/s²) | float×3 | 12 |
| Altitude (m) | float | 4 |
| Pressure (Pa) | float | 4 |
| Humidity (%) | float | 4 |
| Temperature (°C) | float | 4 |
| Latitude (°) | float | 4 |
| Longitude (°) | float | 4 |
| Battery Level | uint8 | 1 |
| **Total (meaningful)** | | **61** |

### Payload Hardware (summary)

| Component | Role |
|-----------|------|
| Teensy 4.1 | Flight computer |
| Adafruit BNO055 | 9-DOF IMU |
| Adafruit BME280 | Barometric alt/pressure/humidity |
| Adafruit GPS | Position & satellite count |
| NTC Thermistor | Temperature (ADC + Steinhart-Hart) |
| RFM95 LoRa | 446.5 MHz, 17 dBm TX |

---

## Ground Station Control Software (`fullscale_groundstation`)

Receives LoRa packets, decodes telemetry, drives a dual-axis stepper gimbal to track the rocket, and streams labeled data over serial to the display laptop

```
                           [Start]
                              |
                 [Initialize GPS and radio module]
                              |
         [Read and store GPS coordinate values from the GPS module]
                              |
               [Read in data packet from radio module]
                              |
     +-->[Extract GPS coordinate values and altitude values from data packet]
     |                        |
     |    [Use the two GPS coordinates to calculate lateral displacement and distance]
     |                        |
     |         [Use the lateral displacement to calculate azimuth angle]
     |                        |
     |          [Use lateral distance and altitude to calculate altitude angle]
     |                        |
     +---[Move stepper motor to corresponding altitude and azimuth angles]
```

### GPS Self-Calibration

On startup, the ground station acquires its own GPS fix and stores it as the antenna reference coordinate (`ATTENNALAT`, `ATTENNALONG`). No manual coordinate entry is required — the system knows where it is on the launch field.

### Antenna Tracking Algorithm

GPS coordinates extracted from each received packet update `TARGETLAT`/`TARGETLONG`. The firmware then computes:

```
Azimuth  = atan2(ΔX, ΔY)               // horizontal bearing to rocket
Altitude = atan2(lateral_dist, ΔZ)      // vertical elevation angle
```

Both angles feed directly into `AccelStepperWithDistance.moveToAngle()` each loop.

- **Step resolution:** 0.45°/step (800 steps/rev)
- **Max azimuth speed:** 90°/s | **Max elevation speed:** 180°/s

### Serial Output

After each valid packet, the firmware emits a single labeled line at 57600 baud for the display laptop:

```
pn<N>,et<s>,gw<°>,gx<°>,gy<°>,gz<°>,mx<µT>,...,la<°>,lo<°>,ga<m>,sn<count>
```

### Ground Station Hardware

The ground station is built on a custom PCB housed in an enclosure. Three ~12 VDC batteries power the system through onboard connectors. The stepper motor drivers and video subsystem are mounted externally.

| Component | Role |
|-----------|------|
| Teensy 4.1 | Ground station microcontroller |
| Adafruit Ultimate GPS | GNSS reference fix for self-calibration |
| RFM95 LoRa | 446.5 MHz receive via 433 MHz Yagi antenna |
| Stepper Motor Driver ×2 | Drive azimuth and altitude motors |
| Stepper Motor ×2 | Azimuth and altitude gimbal axes |
| Limit Switch ×2 | Azimuth and altitude hard stops |
| ClearClick USB Video Capture | Converts analog video feed to USB for display laptop |
| 1.2 GHz 12-Channel Video Receiver | Receives analog video from rocket |
| 1.2 GHz Yagi | Video receive antenna |
| 3× ~12 VDC Batteries | System power |

### Manual Override

Maunal Override allows for user control over the alti-azimuth tracking system.

| Key | Action |
|-----|--------|
| `1` | Enable autonomous tracking |
| `0` | Disable tracking, home to 0°/0° |
| `w/s` | Elevation ±10° |
| `a/d` | Azimuth ±10° |

---

## Ground Station Display Software

[![Display Repo](https://img.shields.io/badge/Repo-GroundStationDisplay-blue?style=flat&logo=github)](https://github.com/shenjason/GroundStationDisplay--SLI2024-2025)

Built in the **Unity game engine**, the display application reads the labeled serial stream from the ground station Teensy and renders all telemetry in real time. It features a live **3D flight view** showing the rocket's position and trajectory on a terrain map, a **3D rocket model** that mirrors the BNO055 orientation in real time, analog-style **acceleration** and **velocity** gauges, a **vertical altitude bar**, and live readouts for GPS coordinates, elapsed time, lateral displacement (Delta E/W, Delta N/S), temperature, pressure, humidity, eCO2, and TVOC.

<p align="center">
  <img src="docs/images/GroundDisplayInterface.JPG" width="720" alt="Ground Station Display Software"/>
</p>

---

## License

[GNU General Public License v3.0](LICENSE)
