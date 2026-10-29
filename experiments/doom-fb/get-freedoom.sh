#!/bin/sh
# Obtain legally redistributable Freedoom 0.13.0 data, no system packages.
set -eu
DEST="${HOME}/d1-doom-fb"
mkdir -p "$DEST"
cd "$DEST"
URL=https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip
curl -fL --retry 2 "$URL" -o freedoom-0.13.0.zip
python3 - <<'PY'
from pathlib import Path
import zipfile
archive=Path("freedoom-0.13.0.zip")
with zipfile.ZipFile(archive) as z:
    candidates=[n for n in z.namelist() if Path(n).name.lower()=="freedoom1.wad"]
    if len(candidates)!=1:
        raise SystemExit(f"Expected exactly one freedoom1.wad, found {len(candidates)}")
    data=z.read(candidates[0])
    if len(data)<1000000 or data[:4] not in (b"IWAD",b"PWAD"):
        raise SystemExit("WAD header or size invalid")
    Path("freedoom1.wad").write_bytes(data)
    print("FREEDOOM_WAD_READY:",len(data),"bytes")
PY
sha256sum freedoom1.wad
