from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
cmake_project = (root / "CMakeLists.txt").read_text(encoding="utf-8")
version_match = re.search(r"project\s*\(\s*latch\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake_project)
if version_match is None:
    raise SystemExit("could not read the Latch version from CMakeLists.txt")
project_version = version_match.group(1)
git_revision = subprocess.run(
    ["git", "rev-parse", "HEAD"], cwd=root, text=True, capture_output=True, check=False
)
if git_revision.returncode == 0:
    revision = git_revision.stdout.strip()
else:
    source_hash = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if not path.is_file() or any(part in {".git", "build", "target"} for part in path.parts):
            continue
        source_hash.update(path.relative_to(root).as_posix().encode())
        source_hash.update(path.read_bytes())
    revision = source_hash.hexdigest()
namespace_seed = hashlib.sha256(f"laststate-latch:{revision}".encode()).hexdigest()
document = {
    "spdxVersion": "SPDX-2.3",
    "dataLicense": "CC0-1.0",
    "SPDXID": "SPDXRef-DOCUMENT",
    "name": f"laststate-latch-{revision[:12]}",
    "documentNamespace": f"https://github.com/laststate/latch/sbom/{namespace_seed}",
    "creationInfo": {
        "created": datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "creators": ["Tool: latch-generate-sbom"],
    },
    "packages": [
        {
            "name": "laststate-latch",
            "SPDXID": "SPDXRef-Package-Latch",
            "versionInfo": project_version,
            "downloadLocation": "https://github.com/laststate/latch",
            "filesAnalyzed": False,
            "licenseConcluded": "Apache-2.0",
            "licenseDeclared": "Apache-2.0",
            "copyrightText": "NOASSERTION",
            "externalRefs": [
                {
                    "referenceCategory": "PACKAGE-MANAGER",
                    "referenceType": "purl",
                    "referenceLocator": "pkg:github/laststate/latch@" + revision,
                }
            ],
        }
    ],
    "relationships": [
        {
            "spdxElementId": "SPDXRef-DOCUMENT",
            "relationshipType": "DESCRIBES",
            "relatedSpdxElement": "SPDXRef-Package-Latch",
        }
    ],
}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
print(args.output)
