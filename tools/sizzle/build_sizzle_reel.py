"""Assemble the native suit-viewer and gameplay captures into a short reel."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]
VIEWER = ROOT / "proof_render" / "sizzle-reel" / "native-viewer"
GAMEPLAY = ROOT / "proof_render" / "sizzle-reel" / "gameplay-spiderham"
OUTPUT = ROOT / "proof_render" / "sizzle-reel" / "OpenSpidey-SMU-sizzle.mp4"
AUDIO = ROOT / "proof_render" / "user-facing-stage" / "ready" / "Spider-Man 2" / "game" / "SPIDER.WAV"

# Down selects the next suit and also rotates the viewer model. These are the
# stable upright moments from the deterministic 78-suit pass.
UPRIGHT = [
    0, 4, 5, 8, 11, 12, 16, 18, 19, 21, 25, 27,
    28, 37, 41, 45, 46, 52, 55, 63, 65, 67, 72, 76,
]


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def quote(path: Path) -> str:
    return path.resolve().as_posix().replace("'", "'\\''")


def write_stills(path: Path, entries: list[tuple[Path, float]]) -> None:
    lines: list[str] = ["ffconcat version 1.0"]
    for image, duration in entries:
        lines.extend((f"file '{quote(image)}'", f"duration {duration:.3f}"))
    lines.append(f"file '{quote(entries[-1][0])}'")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def encode_stills(ffmpeg: str, source: Path, target: Path) -> None:
    run(
        [
            ffmpeg, "-y", "-hide_banner", "-loglevel", "warning",
            "-safe", "0", "-f", "concat", "-i", str(source),
            "-vf", "scale=1440:1080:force_original_aspect_ratio=decrease,"
            "pad=1920:1080:(ow-iw)/2:(oh-ih)/2:black,fps=30,format=yuv420p",
            "-an", "-c:v", "libx264", "-preset", "medium", "-crf", "17",
            str(target),
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    ffmpeg = shutil.which("ffmpeg")
    ffprobe = shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("ffmpeg and ffprobe must be on PATH")

    capture = json.loads((VIEWER / "capture.json").read_text(encoding="utf-8"))
    if capture.get("status") != "pass" or len(capture.get("captures", [])) != 78:
        raise RuntimeError("viewer capture is incomplete")
    if any("smu" in item["name"].lower() for item in capture["captures"]):
        raise RuntimeError("viewer capture still contains an SMU display label")

    frames = [Path(item["path"]) for item in capture["captures"]]
    gameplay = sorted(GAMEPLAY.glob("frame_*.png"))
    gameplay_report = json.loads((GAMEPLAY / "capture.json").read_text(encoding="utf-8"))
    if gameplay_report.get("status") != "pass" or len(gameplay) != 167:
        raise RuntimeError("gameplay capture is incomplete")

    output = args.output.resolve()
    work = output.parent / ".build"
    work.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)

    opener_list = work / "opener.ffconcat"
    viewer_list = work / "viewer.ffconcat"
    outro_list = work / "outro.ffconcat"
    write_stills(opener_list, [(frames[0], 0.900)])
    write_stills(viewer_list, [(frames[index], 0.165) for index in UPRIGHT])
    spider_ham = next(Path(item["path"]) for item in capture["captures"] if item["id"] == "smu-spiderham")
    write_stills(outro_list, [(spider_ham, 1.250)])

    opener_video = work / "opener.mp4"
    viewer_video = work / "viewer.mp4"
    gameplay_video = work / "gameplay.mp4"
    outro_video = work / "outro.mp4"
    encode_stills(ffmpeg, opener_list, opener_video)
    encode_stills(ffmpeg, viewer_list, viewer_video)
    encode_stills(ffmpeg, outro_list, outro_video)

    gameplay_list = work / "gameplay.ffconcat"
    write_stills(gameplay_list, [(frame, 0.050) for frame in gameplay])
    run(
        [
            ffmpeg, "-y", "-hide_banner", "-loglevel", "warning",
            "-safe", "0", "-f", "concat", "-i", str(gameplay_list),
            "-vf", "scale=1920:1080:flags=lanczos,fps=30,format=yuv420p",
            "-an", "-c:v", "libx264", "-preset", "medium", "-crf", "17",
            str(gameplay_video),
        ]
    )

    concat_list = work / "segments.ffconcat"
    concat_list.write_text(
        "ffconcat version 1.0\n"
        + "".join(
            f"file '{quote(path)}'\n"
            for path in (opener_video, viewer_video, gameplay_video, outro_video)
        ),
        encoding="utf-8",
    )
    silent_video = work / "silent.mp4"
    run(
        [
            ffmpeg, "-y", "-hide_banner", "-loglevel", "warning",
            "-safe", "0", "-f", "concat", "-i", str(concat_list),
            "-c", "copy", str(silent_video),
        ]
    )

    duration = float(
        subprocess.check_output(
            [ffprobe, "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(silent_video)],
            text=True,
        ).strip()
    )
    fade_out = max(0.0, duration - 1.0)
    run(
        [
            ffmpeg, "-y", "-hide_banner", "-loglevel", "warning",
            "-i", str(silent_video), "-ss", "8", "-i", str(AUDIO),
            "-filter:a", f"volume=0.32,afade=t=in:st=0:d=0.5,afade=t=out:st={fade_out:.3f}:d=1",
            "-map", "0:v:0", "-map", "1:a:0", "-t", f"{duration:.3f}",
            "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart",
            str(output),
        ]
    )

    probe = json.loads(
        subprocess.check_output(
            [ffprobe, "-v", "error", "-show_streams", "-show_format", "-of", "json", str(output)],
            text=True,
        )
    )
    report = {
        "status": "pass",
        "output": str(output),
        "viewerSuitCount": len(UPRIGHT),
        "gameplayFrameCount": len(gameplay),
        "duration": float(probe["format"]["duration"]),
        "streams": [
            {
                "codec": stream.get("codec_name"),
                "type": stream.get("codec_type"),
                "width": stream.get("width"),
                "height": stream.get("height"),
            }
            for stream in probe["streams"]
        ],
    }
    (output.with_suffix(".json")).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
