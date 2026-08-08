#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,re
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(); parser.add_argument('--output',type=Path,default=Path('api/public-api.json')); args=parser.parse_args()
headers=sorted((root/'include/laststate').glob('*.h'))
entries=[]; symbols=[]
for p in headers:
    data=p.read_bytes(); text=data.decode('utf-8')
    entries.append({'path':str(p.relative_to(root)),'sha256':hashlib.sha256(data).hexdigest()})
    # Conservative C API inventory: exported ls_* function declarations.
    clean=re.sub(r'/\*.*?\*/','',text,flags=re.S)
    clean=re.sub(r'//.*','',clean)
    for m in re.finditer(r'\b(ls_[A-Za-z0-9_]+)\s*\(',clean): symbols.append(m.group(1))
payload={'schema':1,'headers':entries,'symbols':sorted(set(symbols))}
out=root/args.output; out.parent.mkdir(parents=True,exist_ok=True); out.write_text(json.dumps(payload,indent=2)+'\n')
print(f"public API: {len(entries)} headers, {len(payload['symbols'])} symbols")
