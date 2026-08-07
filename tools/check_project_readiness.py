#!/usr/bin/env python3
from pathlib import Path
import json,re,sys
root=Path(__file__).resolve().parents[1]
errors=[]

def require(cond,msg):
    if not cond: errors.append(msg)
release=(root/'.github/workflows/release.yml').read_text()
require('-DLS_COMMERCIAL_PROFILE=ON' in release,'release packaging does not force LS_COMMERCIAL_PROFILE=ON')
require('commercial-release-' in release,'release artifacts are not explicitly named commercial')
mut=(root/'tools/mutation_campaign.py').read_text()
require("payload['invalid']==0" in mut,'mutation campaign does not fail on invalid mutants')
crypto=(root/'docs/security/crypto-assurance.md').read_text()
require('fail' in crypto.lower() and 'external' in crypto.lower(),'crypto assurance documentation missing fail-closed external-provider policy')
impl=(root/'docs/implementation-status.md').read_text()
require('40/40' in impl or '40' in impl and 'mutation' in impl.lower(),'implementation status is stale on mutation assurance')
# No unresolved implementation placeholders in production sources.
for base in ('src','include','ports','arch'):
    d=root/base
    if not d.exists(): continue
    for p in d.rglob('*'):
        if p.suffix not in {'.c','.h','.S','.s','.cpp','.hpp'}: continue
        t=p.read_text(errors='ignore')
        for marker in ('TODO','FIXME','UNIMPLEMENTED','STUB_ONLY'):
            if marker in t:
                errors.append(f'{p.relative_to(root)} contains {marker}')
# Public header version consistency.
v=(root/'include/laststate/version.h').read_text()
m=re.search(r'LS_VERSION_STRING\s+"([^"]+)"',v)
cm=(root/'CMakeLists.txt').read_text()
pm=re.search(r'project\(latch VERSION ([0-9.]+)',cm)
require(bool(m and pm and m.group(1)==pm.group(1)),'CMake and public header versions disagree')
# Machine-readable mutation report, if present, must be perfect for critical set.
r=root/'artifacts/mutation-report.json'
if r.exists():
    data=json.loads(r.read_text())
    require(data.get('survived')==0,'mutation report has survivors')
    require(data.get('invalid')==0,'mutation report has invalid mutants')
    require(data.get('killed')==data.get('total_declared'),'mutation report is not 100% for declared critical mutants')
if errors:
    print('PROJECT READINESS: FAIL')
    for e in errors: print(' -',e)
    sys.exit(1)
print('PROJECT READINESS: PASS')
