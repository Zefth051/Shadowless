#!/usr/bin/env python3
"""Explicit live screen recording, composition checks and short CPU measurements.

Run manually; never invoked by offline test targets. Does not enable audio.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time


def request(path, payload):
    with socket.socket(socket.AF_UNIX) as peer:
        peer.settimeout(20)
        peer.connect(str(path))
        peer.sendall(json.dumps(payload).encode() + b"\n")
        data = b""
        while b"\n" not in data:
            part = peer.recv(65536)
            if not part:
                raise RuntimeError("Recorder disconnected before acknowledgement")
            data += part
        return json.loads(data.split(b"\n")[0])


def pixel(path, x, y, at=2):
    return tuple(subprocess.check_output([
        "ffmpeg", "-v", "error", "-ss", str(at), "-i", str(path), "-frames:v", "1",
        "-vf", f"format=rgb24,crop=1:1:{x}:{y}", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"
    ]))


def verify_position(path):
    # First fixture layer has a red upper half and blue lower half at x/y=10%.
    upper, lower = pixel(path, 240, 140), pixel(path, 240, 380)
    assert len(upper) == len(lower) == 3
    assert upper[0] > 180 and upper[2] < 70, ("upper-left red missing", upper)
    assert lower[2] > 180 and lower[0] < 70, ("lower-left blue missing", lower)


def process_sample(pid):
    fields = Path(f"/proc/{pid}/stat").read_text().split(")", 1)[1].split()
    ticks = int(fields[11]) + int(fields[12])
    rss = int(fields[21]) * os.sysconf("SC_PAGE_SIZE")
    return ticks / os.sysconf("SC_CLK_TCK"), rss / 1048576


def run_case(args, name, scene, replay=True):
    output = args.output / name
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="shadowless-overlay-") as temp:
        runtime = Path(temp)
        environment = dict(os.environ, SHADOWLESS_OVERLAY_SOCKET=str(runtime / "overlay.sock"))
        command = ["gpu-screen-recorder", "-w", args.source, "-f", "60", "-k", "h264", "-c", "mp4",
                   "-ipc", str(runtime / "gsr.sock")]
        if replay:
            command += ["-r", "10", "-restart-replay-on-save", "no", "-o", str(output), "-ro", str(output)]
        else:
            command += ["-o", str(output / "recording.mp4")]
        if scene is not None:
            command += ["-p", str(args.plugin), "-cursor", "no"]
        with (output / "capture.log").open("w") as log:
            process = subprocess.Popen(command, env=environment, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 20
                while not (runtime / "gsr.sock").exists() or (scene is not None and not (runtime / "overlay.sock").exists()):
                    if process.poll() is not None:
                        raise RuntimeError(f"Recorder exited: {output / 'capture.log'}")
                    if time.monotonic() > deadline:
                        raise TimeoutError("Recorder/plugin did not become ready")
                    time.sleep(.05)
                if scene is not None:
                    reply = request(runtime / "overlay.sock", scene)
                    assert reply["ok"], reply
                time.sleep(1)
                start = time.monotonic()
                cpu_start, _ = process_sample(process.pid)
                time.sleep(args.seconds)
                cpu_end, memory = process_sample(process.pid)
                elapsed = time.monotonic() - start
                clip = output / "recording.mp4"
                if replay:
                    reply = request(runtime / "gsr.sock", {"id": 1, "name": "save-replay", "data": {"seconds": 3}})
                    assert reply["result"] == "ok", reply
                    clip = Path(reply["data"])
                reply = request(runtime / "gsr.sock", {"id": 2, "name": "stop"})
                assert reply["result"] == "ok", reply
                process.wait(20)
                assert process.returncode == 0
            finally:
                if process.poll() is None:
                    process.send_signal(signal.SIGINT)
                    try:
                        process.wait(15)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
        subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(clip), "-f", "null", "-"], check=True)
        probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(clip)]))
        (output / "probe.json").write_text(json.dumps(probe, indent=2))
        if scene and scene["enabled"]:
            verify_position(clip)
        result = {"case": name, "cpu_percent_one_core": round((cpu_end - cpu_start) / elapsed * 100, 2),
                  "rss_mib": round(memory, 2), "clip": str(clip)}
        print(json.dumps(result), flush=True)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default="DP-1")
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=5)
    parser.add_argument("--verify-existing", type=Path)
    args = parser.parse_args()
    if args.verify_existing:
        verify_position(args.verify_existing)
        return
    args.plugin = args.plugin.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    fixture = args.output / "fixture.ppm"
    fixture.write_bytes(b"P6\n160 90\n255\n" + bytes([255, 0, 0]) * 160 * 45 + bytes([0, 0, 255]) * 160 * 45)
    image = args.output / "fixture.png"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(fixture), "-frames:v", "1", str(image)], check=True)
    animation = args.output / "animated.mp4"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=size=320x180:rate=15:duration=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", str(animation)], check=True)
    layer = {"id": "static", "path": str(image), "enabled": True, "x": .1, "y": .1, "width": .3, "height": .3, "opacity": 100}
    moving = dict(layer, id="animated", path=str(animation), x=.6, y=.6)
    disabled = {"version": 1, "enabled": False, "layers": []}
    static = dict(disabled, enabled=True, layers=[layer])
    animated = dict(static, layers=[layer, moving])
    results = []
    for name, scene, replay in [("baseline", None, True), ("disabled", disabled, True), ("static", static, True), ("animated", animated, True), ("recording", animated, False)]:
        results.append(run_case(args, name, scene, replay))
    (args.output / "metrics.json").write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
