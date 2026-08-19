# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/mutation_campaign.py
#
# Mutation testing campaign. Full mutant generation and kill
# rate reporting.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Deterministic broad mutation campaign for Latch critical runtime paths.

Unlike mutation_smoke.py, this campaign spans crypto policy/provider, storage,
spool, LEP/transport, OTA and provisioning. Mutants that fail to compile are
reported as invalid and excluded from the score; surviving valid mutants fail
the command when the score is below the configured gate.
"""
from __future__ import annotations
import argparse, json, subprocess, time
from dataclasses import dataclass, asdict
from pathlib import Path

@dataclass(frozen=True)
class Mutation:
    name:str; file:str; before:str; after:str; target:str; test_regex:str

MUTATIONS=(
Mutation('crypto-assurance-threshold-weaken','src/security/provider.c','(int)assurance >= (int)LS_MIN_CRYPTO_ASSURANCE','(int)assurance > (int)LS_MIN_CRYPTO_ASSURANCE','latch-crypto-commercial-tests','latch-crypto-commercial-tests'),
Mutation('crypto-kat-hkdf-bypass','src/security/provider.c','if (r != LS_OK || !bytes_equal(hkdf, hkdf_expected, sizeof hkdf))\n        return LS_EAUTH;','if (r != LS_OK && !bytes_equal(hkdf, hkdf_expected, sizeof hkdf))\n        return LS_EAUTH;','latch-crypto-commercial-tests','latch-crypto-commercial-tests'),
Mutation('crypto-kat-ciphertext-bypass','src/security/provider.c','!bytes_equal(ciphertext, ciphertext_expected, sizeof ciphertext) ||','!bytes_equal(ciphertext, ciphertext_expected, sizeof ciphertext) &&','latch-crypto-commercial-tests','latch-crypto-commercial-tests'),
Mutation('crypto-kat-tag-bypass','src/security/provider.c','!bytes_equal(tag, tag_expected, sizeof tag))\n        return LS_EAUTH;','bytes_equal(tag, tag_expected, sizeof tag))\n        return LS_EAUTH;','latch-crypto-commercial-tests','latch-crypto-commercial-tests'),
Mutation('crypto-provider-gate-bypass','src/security/provider.c','return ls_security_crypto_provider_ready() ? LS_OK : LS_EAUTH;','return LS_OK;','latch-crypto-commercial-tests','latch-crypto-commercial-tests'),
Mutation('crypto-key-length-weaken','src/security/sha256.c','length != LS_SECURITY_KEY_SIZE','length == LS_SECURITY_KEY_SIZE','latch-security-tests','latch-security-tests'),
Mutation('crypto-policy-key-id-zero','src/security/sha256.c','policy->key_id == 0','policy->key_id != 0','latch-security-tests','latch-security-tests'),
Mutation('constant-time-equality-invert','src/security/sha256.c','return difference == 0;','return difference != 0;','latch-crypto-negative-tests','latch-crypto-negative-tests'),
Mutation('aead-auth-failure-bypass','src/security/aead.c','return LS_EAUTH;','return LS_OK;','latch-crypto-negative-tests','latch-crypto-negative-tests'),
Mutation('secure-storage-header-key-bypass','src/storage/secure_storage.c','header.key_id != secure->key_id ||','header.key_id == secure->key_id ||','latch-secure-storage-tests','latch-secure-storage-tests'),
Mutation('secure-storage-crc-bypass','src/storage/secure_storage.c','stored_crc != ls_crc32(ciphertext, secure->logical_capacity)','stored_crc == ls_crc32(ciphertext, secure->logical_capacity)','latch-secure-storage-tests','latch-secure-storage-tests'),
Mutation('flash-mirror-crc-invert','src/storage/flash_mirror.c','header->header_crc == header_crc(header);','header->header_crc != header_crc(header);','latch-flash-tests','latch-flash-tests'),
Mutation('wear-level-crc-invert','src/storage/wear_level.c','header->header_crc == header_crc(header);','header->header_crc != header_crc(header);','latch-wear-level-tests','latch-wear-level-tests'),
Mutation('memory-flash-01-check-bypass','src/storage/memory_storage.c','((uint8_t)~unit[index] & value) != 0u','((uint8_t)~unit[index] & value) == 0u','latch-flash-tests','latch-flash-tests'),
Mutation('spool-record-length-zero-allowed','src/spool/spool.c','record->length > 0u && record->length <= LS_MAX_EVENT_SIZE','record->length >= 0u && record->length <= LS_MAX_EVENT_SIZE','latch-spool-edges-tests','latch-spool-edges-tests'),
Mutation('spool-emergency-reserve-bypass','src/spool/spool.c','return LS_SPOOL_MAX_RECORDS - LS_SPOOL_RESERVED_EMERGENCY;','return LS_SPOOL_MAX_RECORDS;','latch-spool-priority-tests','latch-spool-priority-tests'),
Mutation('spool-critical-reserve-bypass','src/spool/spool.c','return LS_SPOOL_MAX_RECORDS - LS_SPOOL_RESERVED_CRITICAL;','return LS_SPOOL_MAX_RECORDS;','latch-spool-priority-tests','latch-spool-priority-tests'),
Mutation('spool-data-crc-bypass','src/spool/spool.c','selected.data_crc != ls_crc32(data, selected.length)','selected.data_crc == ls_crc32(data, selected.length)','latch-spool-edges-tests','latch-spool-edges-tests'),
Mutation('http-success-and-to-or','src/transport/network.c','status >= 200u && status < 300u','status >= 200u || status < 300u','latch-defensive-paths-tests','latch-defensive-paths-tests'),
Mutation('http-retry-500-off-by-one','src/transport/network.c','status >= 500u','status > 500u','latch-defensive-paths-tests','latch-defensive-paths-tests'),
Mutation('mqtt-qos-range-weaken','src/transport/network.c','mqtt->qos > 2u','mqtt->qos > 3u','latch-defensive-paths-tests','latch-defensive-paths-tests'),
Mutation('stream-ack-duplicate-reject','src/transport/stream.c','status == LS_LSAK_ACK_STORED || status == LS_LSAK_ACK_DUPLICATE','status == LS_LSAK_ACK_STORED && status == LS_LSAK_ACK_DUPLICATE','latch-stream-edges-tests','latch-stream-edges-tests'),
Mutation('stream-crc-bypass','src/transport/stream.c','crc != ls_crc32(envelope, encoded_length)','crc == ls_crc32(envelope, encoded_length)','latch-stream-edges-tests','latch-stream-edges-tests'),
Mutation('varint-overflow-check-disabled','src/envelope/compression.c','i == 4 && (byte & 0xf0u)','i == 4 && (byte & 0x00u)','latch-defensive-paths-tests','latch-defensive-paths-tests'),
Mutation('ota-signing-key-check-weaken','src/core/ota.c','!signing_key_id || !ls_update_version_allowed(version_counter)','!signing_key_id && !ls_update_version_allowed(version_counter)','latch-ota-tests','latch-ota-tests'),
Mutation('ota-stage-on-failure','src/core/ota.c','if (result == LS_OK)\n        result = backend->stage','if (result != LS_OK)\n        result = backend->stage','latch-ota-tests','latch-ota-tests'),
Mutation('ota-update-stage-on-failure','src/core/ota.c','if (result == LS_OK)\n        result = ls_update_stage','if (result != LS_OK)\n        result = ls_update_stage','latch-ota-tests','latch-ota-tests'),
Mutation('ota-confirm-zero-allowed','src/core/ota.c','if (!backend_valid(backend) || !version_counter)','if (!backend_valid(backend) && !version_counter)','latch-ota-tests','latch-ota-tests'),
Mutation('ota-rollback-zero-allowed','src/core/ota.c','if (!backend_valid(backend) || !failed_version)','if (!backend_valid(backend) && !failed_version)','latch-ota-tests','latch-ota-tests'),
Mutation('provision-monotonic-equal-allowed','src/core/provisioning.c','counter > ls_runtime.persistent.provision_monotonic_counter','counter >= ls_runtime.persistent.provision_monotonic_counter','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('provision-key-zero-invert','src/core/provisioning.c','if (key_id == 0u || !monotonic_allowed(monotonic_counter) ||','if (key_id != 0u || !monotonic_allowed(monotonic_counter) ||','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('provision-rotation-same-key-allowed','src/core/provisioning.c','new_key_id == ls_runtime.persistent.provision_key_id ||\n        !monotonic_allowed(monotonic_counter)','new_key_id != ls_runtime.persistent.provision_key_id ||\n        !monotonic_allowed(monotonic_counter)','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('provision-commit-state-invert','src/core/provisioning.c','ls_runtime.persistent.provision_state != LS_PROVISIONING_ROTATING ||','ls_runtime.persistent.provision_state == LS_PROVISIONING_ROTATING ||','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('provision-attest-limit-offbyone','src/core/provisioning.c','challenge_length > 128u','challenge_length >= 128u','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('supervisor-battery-direction','src/core/supervisor.c','sample->battery_mv < config->minimum_battery_mv','sample->battery_mv > config->minimum_battery_mv','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('supervisor-temperature-direction','src/core/supervisor.c','sample->temperature_c > config->maximum_temperature_c','sample->temperature_c < config->maximum_temperature_c','latch-auv-runtime-tests','latch-auv-runtime-tests'),
Mutation('supervisor-watchdog-direction','src/core/supervisor.c','now - ls_runtime.watchdog_last_feed > ls_runtime.supervisor_config.watchdog_max_stale_ms','now - ls_runtime.watchdog_last_feed < ls_runtime.supervisor_config.watchdog_max_stale_ms','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('supervisor-spool-threshold-direction','src/core/supervisor.c','stats.committed * 100u >=','stats.committed * 100u <','latch-auv-edges-tests','latch-auv-edges-tests'),
Mutation('stream-lsak-length-invert','src/transport/stream.c','length != LS_LSAK_SIZE','length == LS_LSAK_SIZE','latch-stream-edges-tests','latch-stream-edges-tests'),
Mutation('secure-element-destroy-callback-invert','src/security/secure_element.c','if (!element->destroy_key)','if (element->destroy_key)','latch-auv-edges-tests','latch-auv-edges-tests'),
)

def run(cmd,cwd): return subprocess.run(cmd,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)

def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--build',type=Path,default=Path('build/mutation-campaign')); ap.add_argument('--minimum-score',type=float,default=85.0); ap.add_argument('--report',type=Path,default=Path('artifacts/mutation-report.json')); ap.add_argument('--limit',type=int,default=0); args=ap.parse_args()
 repo=Path(__file__).resolve().parents[1]; build=args.build.resolve(); muts=MUTATIONS[:args.limit or None]
 if not (build/'CMakeCache.txt').exists():
  r=run(['cmake','-S',str(repo),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Debug','-DLS_BUILD_TESTS=ON','-DLS_BUILD_LINUX=ON'],repo)
  if r.returncode: raise SystemExit(r.stdout)
 results=[]
 for idx,m in enumerate(muts,1):
  path=repo/m.file; original=path.read_text(); count=original.count(m.before)
  if count!=1: results.append({'name':m.name,'status':'invalid-site','detail':f'occurrences={count}'}); continue
  print(f'[{idx}/{len(muts)}] {m.name}',flush=True); path.write_text(original.replace(m.before,m.after,1))
  try:
   b=run(['cmake','--build',str(build),'--target',m.target,'--parallel'],repo)
   if b.returncode: results.append({'name':m.name,'status':'invalid-compile','detail':b.stdout[-2000:].replace(str(repo),'<repo>').replace(str(build),'<build>')}); continue
   t=run(['ctest','--test-dir',str(build),'-R',f'^{m.test_regex}$','--output-on-failure'],repo)
   results.append({'name':m.name,'status':'killed' if t.returncode else 'survived','detail':t.stdout[-2000:].replace(str(repo),'<repo>').replace(str(build),'<build>')})
  finally:
   path.write_text(original); run(['cmake','--build',str(build),'--target',m.target,'--parallel'],repo)
 valid=[x for x in results if x['status'] in ('killed','survived')]; killed=sum(x['status']=='killed' for x in valid); score=100.0*killed/len(valid) if valid else 0.0
 payload={'schema':1,'generated_unix':int(time.time()),'total_declared':len(muts),'valid_mutants':len(valid),'killed':killed,'survived':len(valid)-killed,'invalid':len(results)-len(valid),'score_percent':round(score,2),'minimum_score_percent':args.minimum_score,'results':results}
 report=(repo/args.report); report.parent.mkdir(parents=True,exist_ok=True); report.write_text(json.dumps(payload,indent=2)+'\n')
 print(f"mutation campaign: {killed}/{len(valid)} killed, score={score:.2f}%, invalid={payload['invalid']}")
 for x in results:
  if x['status']=='survived': print('SURVIVED:',x['name'])
 return 0 if valid and score>=args.minimum_score and not any(x['status']=='survived' for x in results) and payload['invalid']==0 else 1
if __name__=='__main__': raise SystemExit(main())
