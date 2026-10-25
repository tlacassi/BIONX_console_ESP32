#!/usr/bin/env python3
"""Inspect current register evidence in a CL1000 text capture; no CAN I/O.

HI/LO pairs are temporal candidates (shared reply ID, no transaction ID).
Type8 queries belong to the logger, not necessarily the original console.
"""
from collections import Counter
from pathlib import Path
import json
import sys

source = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'captures/0000235.TXT'
rows = []
for number, line in enumerate(source.read_text().splitlines(), 1):
    if not line or line.startswith(('#', 'Timestamp;')):
        continue
    stamp, typ, ident, payload = line.split(';')
    ms = ((int(stamp[:2])-1)*86400 + int(stamp[3:5])*3600 + int(stamp[5:7])*60 + int(stamp[7:9]))*1000 + int(stamp[9:])
    rows.append((ms, number, int(typ), int(ident, 16), bytes.fromhex(payload)))

report = {'file': str(source), 'registers': {}}
for reg in (0x1e, 0x1f, 0xd3, 0xd4):
    queries = Counter((typ, node) for _, _, typ, node, data in rows
                      if node in (0x10, 0x20) and len(data) == 2 and data == bytes((0, reg)))
    replies = [data[3] for _, _, typ, node, data in rows
               if node == 8 and typ == 0 and len(data) == 4 and data[:3] == bytes((0, reg, 0))]
    report['registers'][f'{reg:02X}'] = {
        'queries': [{'type': typ, 'node': f'{node:02X}', 'count': count}
                    for (typ, node), count in sorted(queries.items())],
        'reply_count': len(replies), 'distinct_reply_bytes': len(set(replies))}

hi = None
pairs = []
for ms, number, typ, node, data in sorted(rows):
    if node != 8 or typ != 0 or len(data) != 4 or data[0] or data[2]:
        continue
    if data[1] == 0xd3:
        hi = (ms, number, data[3])
    elif data[1] == 0xd4 and hi is not None:
        if 0 <= ms-hi[0] <= 100:
            raw = hi[2]*256+data[3]
            signed = raw-65536 if raw >= 32768 else raw
            pairs.append({'raw_hex': f'{raw:04X}', 'candidate_amps': signed*0.002141,
                          'hi_line': hi[1], 'lo_line': number, 'skew_ms': ms-hi[0]})
        hi = None
report['candidate_pairs'] = len(pairs)
if pairs:
    report['minimum'] = min(pairs, key=lambda p: p['candidate_amps'])
    report['maximum'] = max(pairs, key=lambda p: p['candidate_amps'])
print(json.dumps(report, indent=2))
