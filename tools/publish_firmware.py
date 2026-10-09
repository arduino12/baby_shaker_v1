"""Copy the built firmware into the web app, so phones can update over BLE.

    cd firmware && pio run -e esp32c3_supermini
    python tools/publish_firmware.py
    git add app/fw && git commit && git push      # Pages serves it

The app compares app/fw/version.json with the version the board reports and
offers the update when the site has a newer one.
"""
import hashlib
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent.parent
binary = root / 'firmware' / '.pio' / 'build' / 'esp32c3_supermini' / 'firmware.bin'
config = (root / 'firmware' / 'src' / 'config.h').read_text(encoding='utf-8')
version = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', config).group(1)

data = binary.read_bytes()
out = root / 'app' / 'fw'
out.mkdir(exist_ok=True)
(out / 'firmware.bin').write_bytes(data)
(out / 'version.json').write_text(json.dumps({
    'version': version,
    'size': len(data),
    'sha256': hashlib.sha256(data).hexdigest(),
}, indent=2) + '\n', encoding='utf-8')
print(f'published firmware {version}: {len(data)} bytes -> {out}')
