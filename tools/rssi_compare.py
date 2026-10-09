"""Compare ESP32-C3 modules' antennas (all running tools/rssi_beacon).

    python tools/rssi_compare.py COM5 COM6 COM17 [--seconds 90]

1. The PC scans every "RSSI xxxx" module: how strongly each one TRANSMITS.
2. Each module reports what it hears from the others (serial): how well each
   one RECEIVES, and a second look at how well each transmits.
A badly matched antenna loses in both directions, so it stands out in both.
"""
import asyncio
import statistics
import sys
import threading
import time

import serial
from bleak import BleakScanner

ports = [a for a in sys.argv[1:] if a.upper().startswith('COM')]
seconds = float(sys.argv[sys.argv.index('--seconds') + 1]) if '--seconds' in sys.argv else 90

pc = {}                 # module name -> list of RSSI seen by the PC
heard = {}              # (receiver, transmitter) -> list of window means (weighted below)
port_of = {}
stop = threading.Event()


def read_port(port):
    s = serial.Serial()
    s.port, s.baudrate, s.dtr, s.rts, s.timeout = port, 115200, False, False, 0.2
    s.open()
    while not stop.is_set():
        line = s.readline().decode(errors='replace').split()
        # S <self (2 words)> <peer (2-3 words)> <count> <mean> <min> <max>
        if len(line) >= 8 and line[0] == 'S':
            me = ' '.join(line[1:3])
            peer = ' '.join(line[3:-4])
            n, mean = int(line[-4]), float(line[-3])
            port_of[me] = port
            heard.setdefault((me, peer), []).append((n, mean))
    s.close()


async def scan():
    def cb(d, a):
        name = a.local_name or d.name or ''
        if name.startswith('RSSI '):
            pc.setdefault(name, []).append(a.rssi)
    async with BleakScanner(cb):
        await asyncio.sleep(seconds)


threads = [threading.Thread(target=read_port, args=(p,), daemon=True) for p in ports]
for t in threads:
    t.start()
print(f'measuring for {seconds:.0f} s ...', flush=True)
asyncio.run(scan())
stop.set()
time.sleep(0.5)

names = sorted(set(pc) | {k[0] for k in heard} | {k[1] for k in heard})


def wmean(rows):
    n = sum(r[0] for r in rows)
    return sum(r[0] * r[1] for r in rows) / n if n else None, n


print('\nPC hears each module (TX):')
print(f'{"module":12} {"port":6} {"samples":>7} {"median":>7} {"mean":>7} {"p10":>6} {"p90":>6}')
for m in names:
    v = sorted(pc.get(m, []))
    if v:
        p10, p90 = v[len(v) // 10], v[(len(v) * 9) // 10]
        print(f'{m:12} {port_of.get(m, "?"):6} {len(v):7d} {statistics.median(v):7.1f} {statistics.mean(v):7.1f} {p10:6d} {p90:6d}')
    else:
        print(f'{m:12} {port_of.get(m, "?"):6}       0       -       -      -      -')

print('\nModule-to-module mean RSSI dBm (row = receiver, column = transmitter):')
print(f'{"rx \\ tx":12}' + ''.join(f'{n:>12}' for n in names))
for rx in names:
    row = ''
    for tx in names:
        mean, n = wmean(heard.get((rx, tx), []))
        row += f'{"-" if mean is None else f"{mean:.1f} ({n})":>12}' if rx != tx else f'{"":>12}'
    print(f'{rx:12}' + row)

print('\nPer module: average over its row (how well it receives) and column (how well it transmits):')
for m in names:
    rx = [wmean(heard[(m, t)])[0] for t in names if t != m and (m, t) in heard]
    tx = [wmean(heard[(r, m)])[0] for r in names if r != m and (r, m) in heard]
    print(f'{m:12} receives {statistics.mean(rx) if rx else float("nan"):6.1f}   transmits {statistics.mean(tx) if tx else float("nan"):6.1f}')
