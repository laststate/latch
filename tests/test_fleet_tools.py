# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_fleet_tools.py
#
# Fleet tool tests. Collector, reporter, bundle validation.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
from __future__ import annotations
import json, sqlite3, sys, tempfile, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from latch_fleet_report import build_report, evaluate_canary
from latch_support_bundle import build_bundle
from latch_symbols import lookup


def main() -> int:
    manifest={"schema":1,"symbols":[{"address":0x1000,"size":0x20,"name":"control_loop"},{"address":0x2000,"size":0x10,"name":"nav_tick"}]}
    assert lookup(manifest,0x1004)["name"]=="control_loop"
    assert lookup(manifest,0x1020) is None
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory); source=root/"events"; source.mkdir(); (source/"a.lep").write_bytes(b"abc"); (source/"health.json").write_text("{}"); (source/"device-private.key").write_text("DO-NOT-SHIP")
        output=root/"support.zip"; metadata=build_bundle(source,output,"auv-07")
        assert len(metadata["files"])==2
        assert metadata["excluded_sensitive_files"]==["device-private.key"]
        with zipfile.ZipFile(output) as archive:
            assert "SUPPORT_MANIFEST.json" in archive.namelist()
            assert "device-private.key" not in archive.namelist()
            saved=json.loads(archive.read("SUPPORT_MANIFEST.json")); assert saved["device_id"]=="auv-07"
        dbpath=root/"fleet.sqlite"; db=sqlite3.connect(dbpath)
        db.executescript("""
        CREATE TABLE events(device_id TEXT,event_id INTEGER,sequence INTEGER,event_type INTEGER,architecture INTEGER,flags INTEGER,fingerprint INTEGER,priority INTEGER,severity INTEGER,build_id TEXT,firmware_version TEXT,variant TEXT,device_group TEXT,mission_id TEXT,dive_id TEXT,node_id TEXT,vehicle_mode TEXT,incident_id TEXT,file_path TEXT,received_ns INTEGER,sha256 TEXT);
        """)
        rows=[("auv-1",1,1,1,1,0,123,0,4,"build-a","1","canary","g","m1","d1","nav","survey",None,"a",1,"x"),
              ("auv-2",2,2,1,1,0,123,0,4,"build-a","1","canary","g","m1","d2","nav","survey",None,"b",2,"y"),
              ("auv-3",3,3,1,1,0,456,0,4,"build-b","2","stable","g","m2","d3","nav","survey",None,"c",3,"z")]
        db.executemany("INSERT INTO events VALUES("+",".join("?" for _ in range(21))+")",rows); db.commit(); db.close()
        report=build_report(dbpath,"build-a","build-b")
        assert report["builds"][0]["build_id"]=="build-a"
        assert report["crash_clusters"][0]["fingerprint"]==123
        assert len(report["single_build_signatures"])==2
        comparison=report["build_comparison"]
        assert comparison["new_signatures"][0]["fingerprint"]==456
        assert comparison["resolved_signatures"][0]["fingerprint"]==123
        db=sqlite3.connect(dbpath)
        decision=evaluate_canary(db,"build-a","build-b",minimum_candidate_devices=1,
                                 maximum_new_crash_signatures=0,maximum_crash_device_rate_delta=1.0)
        db.close()
        assert not decision["promote"]
        assert decision["new_crash_signatures"]==[456]
        report=build_report(dbpath,"build-a","build-b",canary=True,minimum_candidate_devices=1,
                            maximum_new_crash_signatures=1,maximum_crash_device_rate_delta=1.0)
        assert report["canary_decision"]["promote"]
    return 0
if __name__=="__main__": raise SystemExit(main())
