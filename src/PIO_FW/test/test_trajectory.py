# !!! 
# IN commands.json: 
# logging -> rate_hz <= min(1e6/dt_us) across the three axes
# !!!

#!/usr/bin/env python3
# traj_menu.py — simple terminal UI for single-axis trajectory files

import struct
import csv
import math
import sys
from pathlib import Path

# =========================
# ---- Preset Shapes -----
# Edit these functions to change the default trajectories.
# =========================

def angle_x(k: int, dt_us: int) -> float:
    """Ramp: 0 → +30° over 2s, then hold."""
    t = k * dt_us / 1e6
    if t <= 2.0:
        return 30.0 * (t / 2.0)
    return 30.0

def angle_y(k: int, dt_us: int) -> float:
    """Triangle: 0 → +30° → 0 over 4s, then hold 0°."""
    t = k * dt_us / 1e6
    if t <= 2.0:
        return 30.0 * (t / 2.0)
    if t <= 4.0:
        return 30.0 * (1 - (t - 2.0) / 2.0)
    return 0.0

def angle_z(k: int, dt_us: int) -> float:
    """Slow sine: 10° amplitude, 6s period."""
    t = k * dt_us / 1e6
    return 10.0 * math.sin(2 * math.pi * t / 6.0)

PRESETS = {
    1: ("X ramp (0→30° over 2s, hold)", angle_x),
    2: ("Y triangle (0→30°→0 over 4s)", angle_y),
    3: ("Z sine (10° amplitude, 6s period)", angle_z),
}

# =========================
# ---- File Format I/O ----
# =========================

HDR_FMT = "<IBBH I Q I I I"   # little-endian
HDR_SIZE = 32
MAGIC = 0x4C424D47            # 'GMBL'

def write_traj(path: str, dt_us: int, total_samples: int, make_angle_fn) -> None:
    """Write a single-axis trajectory file with angles in degrees."""
    version = 1
    axis_count = 1
    reserved = 0
    sample_dt_us = dt_us
    total = total_samples
    angle_scale = 1_000_000  # deg * 1e6 fixed-point
    flags = 1                # positions present
    header_crc32 = 0         # unused

    hdr = struct.pack(
        HDR_FMT,
        MAGIC, version, axis_count, reserved,
        sample_dt_us, total, angle_scale, flags, header_crc32
    )
    with open(path, "wb") as f:
        f.write(hdr)
        for k in range(total_samples):
            deg = float(make_angle_fn(k, dt_us))       # float degrees
            q = int(round(deg * angle_scale))          # fixed-point int32
            f.write(struct.pack("<i", q))

def read_traj(path: str):
    """Return (samples_deg: list[float], dt_us: int, header: dict)."""
    with open(path, "rb") as f:
        hdr_bytes = f.read(HDR_SIZE)
        if len(hdr_bytes) != HDR_SIZE:
            raise ValueError("File too small for header")
        magic, version, axis_count, reserved, sample_dt_us, total, angle_scale, flags, header_crc32 = struct.unpack(
            HDR_FMT, hdr_bytes
        )
        if magic != MAGIC:
            raise ValueError(f"Bad magic: 0x{magic:08X} (expected 0x{MAGIC:08X})")

        samples = []
        for _ in range(total):
            b = f.read(4)
            if len(b) != 4:
                raise ValueError("Unexpected EOF in sample data")
            (q,) = struct.unpack("<i", b)
            samples.append(q / float(angle_scale))

    header = {
        "magic": magic, "version": version, "axis_count": axis_count,
        "reserved": reserved, "sample_dt_us": sample_dt_us, "total": total,
        "angle_scale": angle_scale, "flags": flags, "header_crc32": header_crc32
    }
    return samples, sample_dt_us, header

def time_axis(total: int, dt_us: int):
    return [i * dt_us * 1e-6 for i in range(total)]

# =========================
# ---- Utilities ----------
# =========================

def prompt_int(msg: str, min_val=None, max_val=None) -> int:
    while True:
        try:
            v = int(input(msg).strip())
            if (min_val is not None and v < min_val) or (max_val is not None and v > max_val):
                print(f"Please enter an integer in [{min_val}, {max_val}]")
                continue
            return v
        except ValueError:
            print("Please enter a valid integer.")

def prompt_float(msg: str, min_val=None, max_val=None) -> float:
    while True:
        try:
            v = float(input(msg).strip())
            if (min_val is not None and v < min_val) or (max_val is not None and v > max_val):
                print(f"Please enter a number in [{min_val}, {max_val}]")
                continue
            return v
        except ValueError:
            print("Please enter a valid number.")

def print_header_info(h: dict):
    print("Header:")
    for k in ("magic","version","axis_count","reserved","sample_dt_us","total","angle_scale","flags","header_crc32"):
        print(f"  {k:14s}: {h[k]}")

def ensure_matplotlib():
    try:
        import matplotlib.pyplot as plt  # noqa
        return True
    except Exception as e:
        print("Matplotlib not available; install it with:\n  pip install matplotlib")
        print(f"(detail: {e})")
        return False

# =========================
# ---- Menu Actions -------
# =========================

def action_generate():
    print("\nChoose preset:")
    for k, (name, _) in PRESETS.items():
        print(f"  {k}. {name}")
    preset_choice = prompt_int("Preset # : ", min_val=min(PRESETS), max_val=max(PRESETS))
    name, fn = PRESETS[preset_choice]

    dt_us = prompt_int("Sample period dt_us (µs) e.g. 1000: ", min_val=1)
    duration_s = prompt_float("Duration (seconds): ", min_val=0.001)
    out_path = input("Output filename (e.g. trajectory_X.traj): ").strip() or "trajectory.traj"

    total_samples = int(round(duration_s * 1e6 / dt_us))
    rate_hz = 1e6 / dt_us

    # Optional: enforce a max logging rate rule if you have one
    # (uncomment and set your own limit if needed)
    # max_rate_hz = 1000.0
    # if rate_hz > max_rate_hz:
    #     print(f"Warning: rate {rate_hz:.1f} Hz exceeds recommended max {max_rate_hz:.1f} Hz")

    write_traj(out_path, dt_us, total_samples, fn)
    print(f"\n✓ Wrote {out_path}")
    print(f"   preset: {name}")
    print(f"   dt_us: {dt_us} (rate ≈ {rate_hz:.2f} Hz), samples: {total_samples}\n")

def action_info():
    path = input("\nTrajectory file path: ").strip()
    samples, dt_us, hdr = read_traj(path)
    rate_hz = 1e6 / dt_us
    print(f"\nFile: {path}")
    print(f"Samples: {len(samples)}  |  dt_us: {dt_us}  |  rate ≈ {rate_hz:.2f} Hz")
    print_header_info(hdr)
    # Peek first few samples
    print("\nFirst 10 samples (deg):")
    print(", ".join(f"{v:.6f}" for v in samples[:10]))
    print()

def action_plot():
    if not ensure_matplotlib():
        return
    import matplotlib.pyplot as plt
    path = input("\nTrajectory file path: ").strip()
    out = input("Save PNG as (blank to show interactively): ").strip()

    samples, dt_us, _ = read_traj(path)
    t = time_axis(len(samples), dt_us)
    plt.plot(t, samples)
    plt.xlabel("Time [s]")
    plt.ylabel("Angle [deg]")
    plt.title(Path(path).name)

    if out:
        out_path = Path(out)
        plt.savefig(out_path, dpi=150, bbox_inches="tight")
        print(f"\n✓ Saved plot to {out_path}\n")
    else:
        print("\n(Close the window to return to menu)\n")
        plt.show()

def action_csv():
    path = input("\nTrajectory file path: ").strip()
    out = input("CSV output filename (default: same name with .csv): ").strip()
    if not out:
        out = str(Path(path).with_suffix(".csv"))
    samples, dt_us, _ = read_traj(path)
    t = time_axis(len(samples), dt_us)
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["time_s", "angle_deg"])
        w.writerows(zip(t, samples))
    print(f"\n✓ Wrote CSV to {out}\n")

# =========================
# ---- Main Menu Loop -----
# =========================

MENU = """
================ Trajectory Tool ================
1) Generate trajectory from preset
2) Show file info + header
3) Plot trajectory (requires matplotlib)
4) Export trajectory to CSV
5) Quit
"""

def main():
    while True:
        print(MENU)
        choice = input("Select an option (1-5): ").strip()
        if choice == "1":
            try:
                action_generate()
            except Exception as e:
                print(f"Error: {e}\n")
        elif choice == "2":
            try:
                action_info()
            except Exception as e:
                print(f"Error: {e}\n")
        elif choice == "3":
            try:
                action_plot()
            except Exception as e:
                print(f"Error: {e}\n")
        elif choice == "4":
            try:
                action_csv()
            except Exception as e:
                print(f"Error: {e}\n")
        elif choice == "5":
            print("Goodbye!")
            sys.exit(0)
        else:
            print("Please enter a number from 1 to 5.\n")

if __name__ == "__main__":
    main()
