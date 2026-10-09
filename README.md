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

Connecting: tap **Connect** and pick "Baby Shaker XXXX" in the browser's chooser. To have the
page connect to the last device by itself when it opens, Chrome needs two flags (the page shows
this tip when they are off): in `chrome://flags` enable *Experimental Web Platform features*
and *Use the new permissions backend for Web Bluetooth*. After **Disconnect**, **Connect** goes
straight back to the same device (no list, ~1.3 s); the list opens only if it doesn't answer.
Disconnect asks the board to close the link (`CMD_DROP_LINK`): when the phone/PC closes it
instead, its Bluetooth stack keeps the old link for a few seconds and a quick reconnect fails
(measured on Windows: ~4 s and one failed attempt, vs ~1.3 s). If several shakers are
remembered, a dropdown appears. The Connect button is also the status: green = Connect,
orange = connecting, red = Disconnect.

Under the status tiles the app shows the **signal** in both directions: how strongly the device
hears the phone (measured on the board, always available) and how strongly the phone hears the
device (from its advertisements; needs the Chrome flags above). Each is the median of the last 8
readings - single packets swing by ~20 dB. Bars: ≥ −60 dBm 4, −70 3, −80 2, −90 1.

The footer shows the app and firmware versions. Every file is revalidated with the server on
load (service worker, `cache: 'no-cache'`), so a page never mixes files from two releases.

**Auto 1-4**: four buttons, each with its own parameter set stored on the device (so every
phone sees the same ones). Pressing one loads its set and starts moving. Slider changes
apply live; **Save to Auto N** (bottom of the panel, highlighted when there are unsaved
changes) stores them into that button. The app speaks English and Hebrew (right-to-left);
the choice is remembered per phone.

The mode buttons show what the **device** reports, never a guess (tap → lit ≈ 50-80 ms). A mode
change asked for while the arm is moving waits until the motion is finished - Auto completes
its full cycle - and the pot confirms the arm got there; the requested button gets a pulsing
outline and a note until then. A stall is the only thing that stops at once.

**Manual** has its own movement profile, speed and acceleration (stored on the device): from
rest the arm makes a planned move to the slider position; while the slider is being dragged
it follows with a speed/acceleration-limited tracker that keeps its current velocity.

Sliders only move when the drag starts on the knob, so scrolling past them is safe.

Long presses (with a confirmation):
- **Position** box → calibrate the motor (see below).
- A **Statistics** box (last 24 h / last 30 days / all time) → reset that one.

### Firmware update (over BLE)

When the site has a newer firmware than the connected device, a **Firmware update** card
appears. The app downloads `fw/firmware.bin`, checks its SHA-256 against `fw/version.json`,
and streams it to the device (~1 minute on a phone; the motor stops). The device writes it to
the idle OTA slot, verifies the image, switches the boot slot and restarts; the app reconnects.

Releasing a firmware: bump `FW_VERSION` in config.h, then
```
cd firmware && pio run -e esp32c3_supermini
python ../tools/publish_firmware.py      # -> app/fw/firmware.bin + version.json
git add -A && git commit && git push     # Pages serves it; phones see the update
```

Statistics count activations (Off → Manual/Auto) and run time. They live on the device; the app
sends the phone's clock on connect because the device has none, so the 24 h / 30 day windows
show "—" until a phone has connected since power-up (all-time always counts).

## Wiring

| Signal | C3 pin | Notes |
|---|---|---|
| 5 V rail sense | GPIO0 | 100 kΩ from 5 V + 100 kΩ to GND (1 %), 100 nF from GPIO0 to GND |
| Servo power switch | GPIO1 | Logic-level N-MOS (e.g. AO3400A): GPIO1 → 100 Ω → gate, 100 kΩ gate → GND, drain → servo GND wire, source → GND |
| Servo pot wiper | GPIO3 | **Through a divider** (e.g. 10k/10k) - the pot swings up to 5 V |
| Servo signal | GPIO4 | 50 Hz PWM |
| Status LED | GPIO8 | SuperMini's built-in blue LED, active low (`LED_ACTIVE_LOW`) |

Pins, the servo pulse range (`SERVO_MIN_US`, `SERVO_MAX_US`) and the angle range
(`SERVO_MAX_DEG`) are all in [firmware/src/config.h](firmware/src/config.h).

N-MOS: it must switch fully on at 3.3 V gate drive and carry the servo's stall current (2.5-3 A):
AO3400A (SOT-23, ~40 mΩ at 2.5 V) or IRLML6344; avoid SI2302 (2.5 A max) and non-logic-level parts
(IRF540 etc.). The 100 kΩ pulldown keeps the servo off while the C3 boots. Its Rds(on) lifts the
servo ground by I × R under load (2 A × 40 mΩ = 80 mV ≈ 4° on the pot reading).

`SERVO_POWER_SWITCHED` in config.h says whether the N-MOS is fitted (default `false`: the servo
is always powered). It changes how the servo is started - see "Switching the servo on and off".

Cautions:
- With the N-MOS in the servo's GND, the servo ground floats when it is off. The pot reading
  is only used while the servo is powered, and the firmware stops the pulses *before* cutting
  power so the signal pin can't back-feed the servo.
- The pot shares the servo's ground wire with the motor current. While the motor drives hard,
  that current lifts the pot reading by up to ~15° (a single spike can be 40°); the firmware's
  median filter removes the spikes, but the offset itself needs wiring: join the C3's GND to
  the servo's GND wire right at the servo, and add ~1 µF from the pot ADC pin to GND.
- "Battery" is the power bank's 5 V output - a power bank does not expose its charge level.
- Many power banks switch off when the load drops below ~50-100 mA. In Off mode the device
  draws far less than that, so the bank may shut down. Pick a bank with an "always on" /
  low-current mode if that matters.

## Firmware

Modes:
- **Off** - no pulses, servo unpowered (if the N-MOS is fitted), CPU at 80 MHz, BLE advertising.
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

The rule: a pulse is only ever sent for an angle the pot says the arm is at, or for the next
step of a planned motion from there.

- **On**: the pot is read *first*, and pulses start exactly at that angle - no movement.
  Without the N-MOS (`SERVO_POWER_SWITCHED = false`) the servo is always powered, so the pot is
  valid even in Off. With it, the servo is powered with no pulses (limp), after
  `SERVO_SETTLE_MS` its electronics - which also power the pot - are up, the pot is read, then
  pulses start there. Only a *plausible* reading (inside the calibrated span) is trusted;
  without calibration the last known angle is used.
- **Mode changes**: wait until the command has finished its motion (Auto: the full cycle, back
  to the end it started from - `AUTO_FINISH_FULL_CYCLE`) and the pot is within
  `SETTLE_TOL_DEG` of it (or `SETTLE_TIMEOUT_MS`); meanwhile no new stroke starts. The next
  mode starts from exactly that angle.
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
`stats`, `stats reset <0|1|2>`, `time <epoch>`, `status`, `version`, `trace [0|1]`, `loop`.

Serial output never waits (`Serial.setTxTimeoutMs(0)`): with USB plugged into a computer that
isn't reading the port, the core otherwise blocks up to 2 s on every print, which froze the
control loop (coarse motion, false stalls, slow status). A control tick arriving > 60 ms late
is logged as `[loop]` and skips stall detection; `loop` prints the late-tick count and the
longest gap.

Diagnostics on the serial log: every mode change prints the angle it starts from and how long
it took; `[jump] command …` flags a commanded step > `STEP_WARN_DEG` in one 20 ms tick, and
`[jump] pot …` a reading moving faster than `POT_JUMP_DPS` (faster than the servo can).
`trace 1` streams `T <ms> <O|M|A> <command> <pot> [P = change pending]` at 50 Hz.

`env:bletest` builds the same firmware on a random static BLE address with version 1.4.9 - for
testing from a PC whose Bluetooth stack has cached a stale GATT table for the real address
(Windows does), including an OTA update back to the release build.

### Antenna check

The C3 SuperMini's chip antenna is often poorly matched. To compare modules, flash
`tools/rssi_beacon` (PlatformIO project) on each, place them at the same distance from the PC and
run `python tools/rssi_compare.py COM5 COM6 ...`: the PC's RSSI of each module (how well it
transmits) and a module-to-module matrix (how well each receives). Swap two modules' places
and run again to separate the antenna from its position.

### BLE protocol

Service `8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a`, all little-endian:

| Char | UUID suffix | Props | Payload |
|---|---|---|---|
| Mode | `…0002` | R/W/W-no-rsp/N | u8: 0 Off, 1 Manual, 2 Auto; for Auto a 2nd byte picks the button 0-3. Applies when the arm is at rest |
| Position | `…0003` | W / W-no-rsp | u16 angle ×10 (switches to Manual) |
| Manual params | `…0009` | R/W | u8 profile, u8 0, u16 speed, u16 accel (saved 2 s after the last change) |
| Auto params | `…0004` | R/W | active set (live, not saved): u8 profile, u8 0, u16 speed, u16 accel, u16 travel, u16 hold×10, u16 duration min |
| Status | `…0005` | R/N (1 Hz) | u8 mode, u8 flags (bit0 = pot feedback, bit1 = stopped by a stall, bit2 = calibrating), u16 Vbat mV, u16 pos×10, u16 target×10, u16 remaining s (0xFFFF = no limit), u8 active Auto button 0-3, u8 pending mode (mode \| button << 4, 0xFF = none), i8 link RSSI dBm (127 = unknown) |
| Command | `…0006` | W | u8 command + args: `01` calibrate, `02` save the active set into the active Auto button, `03 w` reset stats (w: 0 = 24 h, 1 = 30 days, 2 = all time), `04 t0..t3` set time (u32 epoch s), `05 s0..s3` OTA begin (image size), `06` OTA end (verify, switch, restart), `07` OTA abort, `08` close the link (the app's Disconnect) |
| OTA | `…000a` | R/W/W-no-rsp/N | write: u32 offset + data (≤ MTU - 7). read/notify (every 4 KB): u8 state (0 idle, 1 ready, 2 receiving, 3 done, 4 failed), u8 error, u16 MTU, u32 size, u32 received |
| Info | `…000b` | R | firmware version string |
| Stats | `…0008` | R/N | 6 × u32: 24 h count, 24 h seconds, 30 d count, 30 d seconds, all-time count, all-time seconds (0xFFFFFFFF = time not known yet) |

Device name: `Baby Shaker XXXX` - the last two bytes of the chip's MAC. The device keeps
advertising whenever one of its 3 link slots is free (NimBLE-Arduino 2.x does not restart
advertising after a disconnect by itself).
