# !!! 
# IN commands.json: 
# logging -> rate_hz <= min(1e6/dt_us) across the three axes
# !!!

import struct, math

def write_traj(path, dt_us, total_samples, make_angle_fn):
    # Header (32 bytes)
    magic = 0x4C424D47  # 'GMBL' little-endian value
    version = 1
    axis_count = 1
    reserved = 0
    sample_dt_us = dt_us
    total = total_samples
    angle_scale = 1_000_000  # deg * 1e6
    flags = 1  # positions present
    header_crc32 = 0  # not used by firmware in this test

    hdr = struct.pack("<IBBH I Q I I I",
        magic, version, axis_count, reserved,
        sample_dt_us, total, angle_scale, flags, header_crc32)

    with open(path, "wb") as f:
        f.write(hdr)
        for k in range(total_samples):
            deg = make_angle_fn(k)  # degrees as float
            q = int(round(deg * angle_scale))  # fixed point int32
            f.write(struct.pack("<i", q))

# --- Simple shapes for each axis ---
# X: 0 -> +30° ramp over 2s, then hold
def angle_x(k, dt_us=2000):
    t = k * dt_us / 1e6
    if t <= 2.0: return 30.0 * (t/2.0)
    return 30.0

# Y: 0 -> +30° -> 0° triangle over 4s
def angle_y(k, dt_us=1000):
    t = k * dt_us / 1e6
    if t <= 2.0: return 30.0 * (t/2.0)
    if t <= 4.0: return 30.0 * (1 - (t-2.0)/2.0)
    return 0.0

# Z: slow 10° sine for 6s
def angle_z(k, dt_us=5000):
    t = k * dt_us / 1e6
    return 10.0 * math.sin(2*math.pi * t/6.0)

# Choose per-axis dt and duration
x_dt = 2000     # 2 kHz (0.5 ms)
x_T  = 3.0      # seconds
y_dt = 1000     # 1 kHz (1 ms)
y_T  = 5.0
z_dt = 5000     # 200 Hz (5 ms)
z_T  = 6.0

write_traj("trajectory_X.traj", x_dt, int(x_T*1e6/x_dt), lambda k: angle_x(k, x_dt))
write_traj("trajectory_Y.traj", y_dt, int(y_T*1e6/y_dt), lambda k: angle_y(k, y_dt))
write_traj("trajectory_Z.traj", z_dt, int(z_T*1e6/z_dt), lambda k: angle_z(k, z_dt))

print("Wrote trajectory_X/Y/Z.traj")
