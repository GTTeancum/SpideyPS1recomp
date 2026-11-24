"""Audit or apply the player-pose calibration to the checked-in SMU actors."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

from suit_proportions import Rig, SUITS, calibrated_blob, rig_offset, uses_human_calibration


def replace(path, content):
    pending = tempfile.NamedTemporaryFile(dir=path.parent, delete=False)
    candidate = Path(pending.name)
    try:
        with pending:
            pending.write(content)
        candidate.replace(path)
    finally:
        candidate.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    pending = 0
    for path in sorted(SUITS.glob("smu-*/actor.psx")):
        suit = path.parent.name
        if not uses_human_calibration(suit):
            continue
        data = path.read_bytes()
        offset, size = rig_offset(data)
        original = data[offset:offset+size]
        corrected = calibrated_blob(original, suit)
        result = data[:offset] + corrected + data[offset+size:]
        report_path = path.with_name("conversion-report.json")
        report = json.loads(report_path.read_text())
        digest = hashlib.sha256(result).hexdigest()
        if corrected == original and report["outputSha256"] == digest:
            continue
        pending += 1
        if args.apply:
            rig = Rig(corrected)
            vertices, _ = rig.evaluate()
            report["neutralBounds"] = [vertices[:, :3].min(0).tolist(), vertices[:, :3].max(0).tolist()]
            report["outputSha256"] = digest
            if corrected != original:
                replace(path, result)
            replace(report_path, (json.dumps(report, indent=2) + "\n").encode())
        print(suit, digest)
    print(f"{pending} actors or reports {'updated' if args.apply else 'pending'}")
    return int(pending > 0 and not args.apply)


if __name__ == "__main__":
    raise SystemExit(main())
