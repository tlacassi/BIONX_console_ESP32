#!/usr/bin/env python3
"""Replay CL1000 or CAN CSV through the actual C parser; no CAN transmission."""
import collections
import csv
import ctypes as c
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

class Frame(c.Structure):
    _fields_ = [('id', c.c_uint32), ('dlc', c.c_uint8), ('data', c.c_uint8 * 8),
                ('extended', c.c_bool), ('rtr', c.c_bool), ('timestamp_us', c.c_int64)]

class Packet(c.Structure):
    _fields_ = [('kind', c.c_int), ('reg', c.c_uint8), ('raw', c.c_uint16), ('target', c.c_uint8)]

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]) if len(sys.argv) > 1 else root / '0000235.TXT'
blob = source.read_bytes()  # One snapshot, even if the original is later replaced.
print(f'bytes={len(blob)} sha256={hashlib.sha256(blob).hexdigest()}')
ids, kinds, rejected, types, seconds = (collections.Counter() for _ in range(5))
times = []
with tempfile.TemporaryDirectory() as tmp:
    lib = Path(tmp) / 'protocol.so'
    subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                    str(root / 'components/bionx_protocol/bionx_protocol.c'), '-o', str(lib)], check=True)
    parser = c.CDLL(str(lib)).bionx_parse
    parser.argtypes = [c.POINTER(Frame), c.POINTER(Packet)]
    parser.restype = c.c_bool
    records = []
    is_csv = blob.startswith(b'Time Stamp,')
    if is_csv:
        for row in csv.reader(blob.decode('ascii').splitlines()[1:]):
            if not row:
                continue
            # This export does not expose an RTR flag. Only data is replayed.
            length = int(row[5])
            if not 0 <= length <= 8:
                raise ValueError('Invalid DLC')
            records.append((int(row[1], 16), int(row[2].lower() == 'true'),
                            bytes(int(x, 16) for x in row[6:6 + length]), 0))
        print('CSV replay: synthetic timestamp 0; no timing validation or RTR information')
    else:
        for line in blob.decode('ascii').splitlines():
            if not line or line.startswith(('#', 'Timestamp;')):
                continue
            stamp, typ, ident, payload = line.split(';')
            typ, ident, data = int(typ), int(ident, 16), bytes.fromhex(payload)
            if typ not in (0, 1, 8, 9) or len(data) > 8:
                raise ValueError(f'Unsupported record: {line}')
            ms = ((int(stamp[:2]) - 1) * 86400 + int(stamp[3:5]) * 3600
                  + int(stamp[5:7]) * 60 + int(stamp[7:9])) * 1000 + int(stamp[9:])
            records.append((ident, typ, data, ms))
    for ident, typ, data, ms in records:
        frame = Frame(id=ident, dlc=len(data), extended=typ in (1, 9), timestamp_us=ms * 1000)
        frame.data[:len(data)] = data
        packet = Packet()
        ids[f'0x{ident:02X}'] += 1
        types[typ] += 1
        seconds[ms // 1000] += 1
        times.append(ms)
        if parser(c.byref(frame), c.byref(packet)):
            kinds[('QUERY', 'SET', 'REPLY', 'UNKNOWN')[packet.kind]] += 1
            assert packet.reg == data[1]
            assert packet.raw == (int.from_bytes(data[2:], 'big') if len(data) == 4 else 0)
        else:
            rejected[f'0x{ident:02X}:{data.hex()}'] += 1
if is_csv:
    print(f'frames={len(times)} timing=unavailable')
else:
    print(f'frames={len(times)} duration_ms={max(times)-min(times)} peak_per_second={max(seconds.values())}')
print(f'types={dict(types)} ids={dict(ids)}')
print(f'parsed={dict(kinds)} rejected={dict(rejected)}')
print(f'timestamp_regressions={sum(b < a for a, b in zip(times, times[1:]))}')
