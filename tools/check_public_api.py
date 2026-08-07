#!/usr/bin/env python3
from pathlib import Path
import json,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]; baseline=root/'api/public-api.json'
if not baseline.exists(): raise SystemExit('missing api/public-api.json')
with tempfile.TemporaryDirectory() as td:
 out=Path(td)/'api.json'; r=subprocess.run([sys.executable,str(root/'tools/generate_public_api_manifest.py'),'--output',str(out)],cwd=root)
 if r.returncode: raise SystemExit(r.returncode)
 current=json.loads(out.read_text()); old=json.loads(baseline.read_text())
missing=sorted(set(old['symbols'])-set(current['symbols']))
if missing:
 print('PUBLIC API COMPATIBILITY: FAIL'); print('\n'.join(' removed: '+x for x in missing)); raise SystemExit(1)
print(f"PUBLIC API COMPATIBILITY: PASS ({len(current['symbols'])} symbols)")
