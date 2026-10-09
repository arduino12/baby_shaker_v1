"""Compare ESP32-C3 modules' antennas and temperatures (all running tools/rssi_beacon).

    python tools/rssi_compare.py COM5:current COM6:supermini-2 COM7:C3-01M [--on 120] [--off 120]

Phase 1, radio on (--on s):
  - the PC scans every "RSSI xxxx" module: how strongly each one TRANSMITS;
  - each module reports what it hears from the others: how well it RECEIVES.
  A badly matched antenna loses in both directions, so it stands out in both.
Phase 2, radio off (--off s): BLE stopped on every module. Its die temperature
  then shows heat that has nothing to do with the radio (regulator, chip).
"""
import asyncio
import statistics
import sys
import threading
import time

import serial
from bleak import BleakScanner


def arg(name, default):
    return float(sys.argv[sys.argv.index(name) + 1]) if name in sys.argv else default


specs = [a for a in sys.argv[1:] if a.upper().startswith('COM')]
ports = {s.split(':')[0]: (s.split(':')[1] if ':' in s else '') for s in specs}
on_s, off_s = arg('--on', 120), arg('--off', 120)

pc = {}             # module -> RSSI list seen by the PC
heard = {}          # (receiver, transmitter) -> [(count, mean)]
temps = {}          # module -> [(t, temp, radio)]
port_of = {}
serials = {}
stop = threading.Event()
t_start = time.time()


drops = {}          # port -> times it dropped off USB during the run


def read_port(port):
    s = None
    while not stop.is_set():
        try:
            if s is None:   # (re)open: a module that drops off USB (e.g. overheating) may come back
                s = serial.Serial()
                s.port, s.baudrate, s.dtr, s.rts, s.timeout = port, 115200, False, False, 0.2
                s.open()
                serials[port] = s
            w = s.readline().decode(errors='replace').split()
        except (serial.SerialException, OSError):
            drops[port] = drops.get(port, 0) + 1
            serials.pop(port, None)
            try:
                s.close()
            except Exception:
                pass
            s = None
            time.sleep(1)
            continue
        if len(w) >= 8 and w[0] == 'S':
            me, peer = ' '.join(w[1:3]), ' '.join(w[3:-4])
            port_of[me] = port
            heard.setdefault((me, peer), []).append((int(w[-4]), float(w[-3])))
        elif len(w) == 5 and w[0] == 'T':
            me = ' '.join(w[1:3])
            port_of[me] = port
            temps.setdefault(me, []).append((time.time() - t_start, float(w[3]), w[4]))


def send_all(cmd):
    for s in list(serials.values()):
        try:
            s.write((cmd + '\n').encode())
        except (serial.SerialException, OSError):
            pass


async def scan(seconds):
    def cb(d, a):
        name = a.local_name or d.name or ''
        if name.startswith('RSSI '):
            pc.setdefault(name, []).append(a.rssi)
    async with BleakScanner(cb):
        await asyncio.sleep(seconds)


threads = [threading.Thread(target=read_port, args=(p,), daemon=True) for p in ports]
for t in threads:
    t.start()
time.sleep(1)
send_all('radio on')
print(f'phase 1: radio on, {on_s:.0f} s ...', flush=True)
asyncio.run(scan(on_s))
t_off = time.time() - t_start
send_all('radio off')
print(f'phase 2: radio off, {off_s:.0f} s ...', flush=True)
time.sleep(off_s)
send_all('radio on')
stop.set()
time.sleep(0.5)

names = sorted(set(pc) | set(temps) | {k[0] for k in heard} | {k[1] for k in heard})
label = lambda m: f'{m} {ports.get(port_of.get(m, ""), "")}'.strip()


def wmean(rows):
    n = sum(r[0] for r in rows)
    return (sum(r[0] * r[1] for r in rows) / n if n else None), n


print('\nPC hears each module (TX), radio-on phase:')
print(f'{"module":28} {"port":6} {"packets":>7} {"median":>7} {"p10":>5} {"p90":>5}')
for m in names:
    v = sorted(pc.get(m, []))
    if v:
        print(f'{label(m):28} {port_of.get(m, "?"):6} {len(v):7d} {statistics.median(v):7.1f} {v[len(v) // 10]:5d} {v[len(v) * 9 // 10]:5d}')
    else:
        print(f'{label(m):28} {port_of.get(m, "?"):6}       0       -     -     -')

print('\nModule-to-module: mean dBm (packets received)  row = receiver, column = transmitter')
print(f'{"rx \\ tx":12}' + ''.join(f'{n:>16}' for n in names))
for rx in names:
    cells = []
    for tx in names:
        if rx == tx:
            cells.append(f'{"":>16}')
            continue
        mean, n = wmean(heard.get((rx, tx), []))
        cells.append(f'{"0" if mean is None else f"{mean:.0f} ({n})":>16}')
    print(f'{rx:12}' + ''.join(cells))

print('\nPackets each module received from the others (more = better receiver), and was heard (better transmitter):')
for m in names:
    rx = sum(wmean(heard.get((m, t), []))[1] for t in names if t != m)
    tx = sum(wmean(heard.get((r, m), []))[1] for r in names if r != m)
    print(f'{label(m):28} received {rx:6d}   heard by others {tx:6d}')

print('\nDie temperature C (internal sensor):')
print(f'{"module":28} {"start":>6} {"radio on, end":>14} {"radio off, end":>15}')
for m in names:
    v = temps.get(m, [])
    if not v:
        continue
    on = [x[1] for x in v if x[0] < t_off]
    off = [x[1] for x in v if x[0] >= t_off]
    tail = lambda xs: statistics.mean(xs[-8:]) if xs else float('nan')
    print(f'{label(m):28} {v[0][1]:6.1f} {tail(on):14.1f} {tail(off):15.1f}')

if drops:
    print('\nDropped off USB during the run:', ', '.join(f'{p} x{n}' for p, n in drops.items()))
