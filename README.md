# Baby Shaker V1

A servo mounted above a stroller wheel rocks the stroller back and forth.
ESP32-C3 SuperMini + 270° servo (TD-8153MG; MG996R also works) + USB power bank,
controlled over Bluetooth LE from a free web app.

```
firmware/   PlatformIO / Arduino (pioarduino core 3.x) + NimBLE-Arduino
app/        Web Bluetooth PWA - plain HTML/JS, no build step
```

## The phone app

One web page, no app store, no Mac:

| Phone | How |
|---|---|
| Android | Open the app URL in **Chrome** → menu → *Add to Home screen* |
| iPhone | Safari has no Bluetooth. Install the free **Bluefy** browser and open the URL there |

The app must be served over HTTPS (or `localhost`). Free hosting: push this repo to
GitHub → *Settings → Pages* → deploy from the `app/` folder (or a `gh-pages` branch).

Local test on a PC: `cd app && python -m http.server 8000` → open
<http://localhost:8000> in Chrome or Edge.

Connecting: the first time, tap **Connect** and pick "Baby Shaker XXXX" in the browser's
chooser (browsers never connect without that one tap). After that, Chrome remembers the
device and connects by itself when the page opens. If several shakers are remembered, a
dropdown appears.

## Wiring

| Signal | C3 pin | Notes |
|---|---|---|
| Servo pot wiper | GPIO0 | **Through a divider** (e.g. 10k/10k) - the pot swings up to 5 V |
| 5 V rail sense | GPIO1 | 100k/100k divider |
| Servo signal | GPIO5 | 50 Hz PWM |
| Servo power switch | GPIO6 | Gate of a logic-level N-MOS switching servo GND; 100k gate pulldown |
| Status LED | GPIO7 | Anode, through ~330 Ω |

Pins, the servo pulse range (`SERVO_MIN_US`, `SERVO_MAX_US`) and the angle range
(`SERVO_MAX_DEG`) are all in [firmware/src/config.h](firmware/src/config.h).

Cautions:
- With the N-MOS in the servo's GND, the servo ground floats when it is off. The pot reading
  is only used while the servo is powered, and the firmware pulls the signal pin low *before*
  cutting power so it can't back-feed the servo.
- "Battery" is the power bank's 5 V output - a power bank does not expose its charge level.
- Many power banks switch off when the load drops below ~50-100 mA. In Off mode the device
  draws far less than that, so the bank may shut down. Pick a bank with an "always on" /
  low-current mode if that matters.

## Firmware

Modes:
- **Off** - PWM stopped, servo unpowered, CPU at 80 MHz, BLE advertising.
- **Manual** - app slider sets the absolute angle in real time. Back to Off after 60 s idle.
- **Auto** - swings ±Travel/2 around mid-range with the chosen profile, holding Hold Time at
  each end. Off when Duration expires (Duration 0 = no limit).

Profiles all respect Speed (max velocity) and Acceleration (max accel/decel):
Trapezoidal (constant accel), S-Curve (smoothstep velocity ramps, jerk-limited),
Sinusoidal (cosine position), Cubical (3u²−2u³ position).

Settings live in NVS and survive power loss.

Pot feedback: run `cal` on the serial console (or write `1` to the command characteristic)
with the pot wired - the servo sweeps 0 → max and stores the readings. Until calibrated,
the reported position is the commanded one (shown with `*` in the app).

Build / flash (native USB, no buttons needed):
```
cd firmware
pio run -t upload
pio device monitor
```
Serial commands: `off`, `man <deg>`, `auto`, `cal`, `status`.

### BLE protocol

Service `8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a`, all little-endian:

| Char | UUID suffix | Props | Payload |
|---|---|---|---|
| Mode | `…0002` | R/W/N | u8: 0 Off, 1 Manual, 2 Auto |
| Position | `…0003` | W / W-no-rsp | u16 angle ×10 (switches to Manual) |
| Auto params | `…0004` | R/W | u8 profile, u8 0, u16 speed, u16 accel, u16 travel, u16 hold×10, u16 duration min |
| Status | `…0005` | R/N (1 Hz) | u8 mode, u8 flags (bit0 = pot feedback), u16 Vbat mV, u16 pos×10, u16 target×10, u16 remaining s (0xFFFF = no limit) |
| Command | `…0006` | W | u8: 1 = calibrate pot |

Device name: `Baby Shaker XXXX` - the last two bytes of the chip's MAC.
