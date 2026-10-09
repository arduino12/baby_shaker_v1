# Baby Shaker V1

A servo mounted above a stroller wheel rocks the stroller back and forth.
ESP32-C3 SuperMini + 270° servo (TD-8153MG; MG996R also works) + USB power bank,
controlled over Bluetooth LE from a free web app.

```
firmware/   PlatformIO / Arduino (pioarduino core 3.x) + NimBLE-Arduino
app/        Web Bluetooth PWA - plain HTML/JS, no build step
```

## The phone app

**https://arduino12.github.io/baby_shaker_v1/** (Hebrew: add `?lang=he`, or use the language button)

One web page, no app store, no Mac:

| Phone | How |
|---|---|
| Android | Open the app URL in **Chrome** → menu → *Add to Home screen* |
| iPhone | Safari has no Bluetooth. Install the free **Bluefy** browser and open the URL there |

The app must be served over HTTPS (or `localhost`). Every push to `main` that touches
`app/` redeploys it to GitHub Pages ([.github/workflows/pages.yml](.github/workflows/pages.yml)).

Local test on a PC: `cd app && python -m http.server 8000` → open
<http://localhost:8000> in Chrome or Edge.

Connecting: the first time, tap **Connect** and pick "Baby Shaker XXXX" in the browser's
chooser (browsers never connect without that one tap). After that, Chrome remembers the
device and connects by itself when the page opens. If several shakers are remembered, a
dropdown appears. The Connect button is also the status: green = Connect,
orange = connecting, red = Disconnect.

**Auto 1-4**: four buttons, each with its own parameter set stored on the device (so every
phone sees the same ones). Pressing one loads its set and starts moving. Slider changes
apply live; **Save to Auto N** (bottom of the panel, highlighted when there are unsaved
changes) stores them into that button. The app speaks English and Hebrew (right-to-left);
the choice is remembered per phone.

The mode buttons show what the **device** reports, never a guess. A mode change asked for
while the arm is moving waits until the current stroke ends (so nothing jerks): the requested
button gets a pulsing outline and a "switching when the stroke ends" note until then. A stall
is the only thing that stops at once.

**Manual** has its own movement profile, speed and acceleration (stored on the device): from
rest the arm makes a planned move to the slider position; while the slider is being dragged
it follows with a speed/acceleration-limited tracker that keeps its current velocity.

Sliders only move when the drag starts on the knob, so scrolling past them is safe.

Long presses (with a confirmation):
- **Position** box → calibrate the motor (see below).
- A **Statistics** box (last 24 h / last 30 days / all time) → reset that one.

Statistics count activations (Off → Manual/Auto) and run time. They live on the device; the app
sends the phone's clock on connect because the device has none, so the 24 h / 30 day windows
show "—" until a phone has connected since power-up (all-time always counts).

## Wiring

| Signal | C3 pin | Notes |
|---|---|---|
| 5 V rail sense | GPIO0 | 100k/100k divider |
| Servo power switch | GPIO1 | Gate of a logic-level N-MOS switching servo GND; 100k gate pulldown |
| Servo pot wiper | GPIO3 | **Through a divider** (e.g. 10k/10k) - the pot swings up to 5 V |
| Servo signal | GPIO4 | 50 Hz PWM |
| Status LED | GPIO8 | SuperMini's built-in blue LED, active low (`LED_ACTIVE_LOW`) |

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

### Pot calibration, stall detection

Run `cal` on the serial console (or write `01` to the command characteristic - deliberately
not in the app) with the servo free to move. It takes ~20 s:
1. steps 0 → max in 11 points and records the pot voltage at each (piecewise-linear table),
2. measures the pot noise while holding still,
3. times a full-range move each way (10 % → 90 %) - the slower one is the servo's top speed.

The result is stored in NVS (`calinfo` prints it) and enables:
- **Real position**: status reports the measured angle (no `*` in the app).
- **No jump on Manual/Auto entry**: see "Switching the servo on and off" below - the app's
  slider moves to where the arm is.
- **Speed cap**: Auto never plans faster than the measured top speed.
- **Stall detection**: a reference "slow servo" (30 % of top speed) chases each target from the
  arm's real progress. If the pot stays 20° further from the target than that reference for
  1 s, the device switches Off and flags a stall; the app shows a warning. Thresholds:
  `STALL_*` in config.h.

Without a calibration the position shown is the commanded one and stall detection is off.

### Switching the servo on and off

- **On**: a valid PWM pulse at the last known angle starts *before* the power switch closes, so
  the servo never sees a powered frame without a pulse. After `SERVO_SETTLE_MS` (the servo
  electronics also power the pot) a *plausible* pot reading (inside the calibrated span)
  becomes the hold angle, in case the arm was moved while off.
- **Off**: duty 0 takes effect at the next frame, so the pulse in flight finishes (a truncated
  pulse reads as "go to 0°"); one frame later the power is cut and the pin driven low. The
  measured angle is saved in NVS as the last known position (not after a stall - then the pot
  may be what failed).
- **Auto start**: the first move goes from the measured angle to the *nearer* end of the swing,
  capped to `APPROACH_SPEED_DPS` / `APPROACH_ACCEL_DPS2`, then the normal profile runs.

Build / flash (native USB, no buttons needed):
```
cd firmware
pio run -t upload
pio device monitor
```
Serial commands: `off`, `man` (enter Manual where the arm is), `man <deg>`, `auto [1-4]`,
`set <profile> <speed> <accel> <travel> <hold×0.1s> <min>`, `mset <profile> <speed> <accel>` (Manual),
`save`, `cal`, `calinfo`,
`stats`, `stats reset <0|1|2>`, `time <epoch>`, `status`.

### BLE protocol

Service `8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a`, all little-endian:

| Char | UUID suffix | Props | Payload |
|---|---|---|---|
| Mode | `…0002` | R/W/W-no-rsp/N | u8: 0 Off, 1 Manual, 2 Auto; for Auto a 2nd byte picks the button 0-3. Applies when the arm is at rest |
| Position | `…0003` | W / W-no-rsp | u16 angle ×10 (switches to Manual) |
| Manual params | `…0009` | R/W | u8 profile, u8 0, u16 speed, u16 accel (saved 2 s after the last change) |
| Auto params | `…0004` | R/W | active set (live, not saved): u8 profile, u8 0, u16 speed, u16 accel, u16 travel, u16 hold×10, u16 duration min |
| Status | `…0005` | R/N (1 Hz) | u8 mode, u8 flags (bit0 = pot feedback, bit1 = stopped by a stall, bit2 = calibrating), u16 Vbat mV, u16 pos×10, u16 target×10, u16 remaining s (0xFFFF = no limit), u8 active Auto button 0-3, u8 pending mode (mode \| button << 4, 0xFF = none) |
| Command | `…0006` | W | u8 command + args: `01` calibrate, `02` save the active set into the active Auto button, `03 w` reset stats (w: 0 = 24 h, 1 = 30 days, 2 = all time), `04 t0..t3` set time (u32 epoch s) |
| Stats | `…0008` | R/N | 6 × u32: 24 h count, 24 h seconds, 30 d count, 30 d seconds, all-time count, all-time seconds (0xFFFFFFFF = time not known yet) |

Device name: `Baby Shaker XXXX` - the last two bytes of the chip's MAC. The device keeps
advertising whenever one of its 3 link slots is free (NimBLE-Arduino 2.x does not restart
advertising after a disconnect by itself).
