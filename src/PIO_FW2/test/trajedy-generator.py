import numpy as np
import json
import struct

# ==========================================
# User parameters
# ==========================================
step_angle_deg = float(input("Stepper step angle [deg]: ") or 1.8)
microsteps = int(input("Microstepping value: ") or 8)

# Derived parameters
steps_per_rev = int((360.0 / step_angle_deg) * microsteps)
print(f"Total steps per revolution: {steps_per_rev}")

# ==========================================
# Motion profile parameters (fixed)
# ==========================================
T_move = 10.0      # seconds per 360° move
T_dwell = 2.0      # seconds dwell
T_total = 2*T_move + T_dwell

max_jerk = 10.0    # rad/s^3
max_acc = 5.0      # rad/s^2
max_vel = 2*np.pi / T_move  # rad/s for one rev per 10 s

# Time resolution for generating motion profile
dt = 1e-3  # 1 ms time base
t = np.arange(0, T_total, dt)

# ==========================================
# Generate S-curve trajectory (position vs time)
# ==========================================
def s_curve_profile(t, T, theta_final):
    """Simple 7-segment S-curve normalized to T seconds."""
    # Segment times as fractions of total
    Tj = T / 10
    Ta = 2 * Tj
    Tv = T - 4*Tj  # constant velocity part

    theta = np.zeros_like(t)
    omega = np.zeros_like(t)
    alpha = np.zeros_like(t)

    for i, ti in enumerate(t):
        if ti < Tj:
            alpha[i] = max_jerk * ti
            omega[i] = 0.5 * max_jerk * ti**2
            theta[i] = (1/6) * max_jerk * ti**3
        elif ti < 2*Tj:
            tj = ti - Tj
            alpha[i] = max_jerk*Tj - max_jerk*tj
            omega[i] = 0.5*max_jerk*Tj**2 + max_jerk*Tj*tj - 0.5*max_jerk*tj**2
            theta[i] = (1/6)*max_jerk*Tj**3 + 0.5*max_jerk*Tj**2*tj + 0.5*max_jerk*Tj*tj**2 - (1/6)*max_jerk*tj**3
        elif ti < (2*Tj + Tv):
            alpha[i] = 0
            omega[i] = 0.5*max_jerk*Tj**2 + max_jerk*Tj*(ti - 2*Tj)
            theta[i] = (1/6)*max_jerk*Tj**3 + 0.5*max_jerk*Tj**2*(ti - 2*Tj) + \
                        0.5*max_jerk*Tj*(ti - 2*Tj)**2
        elif ti < (3*Tj + Tv):
            tj = ti - (2*Tj + Tv)
            alpha[i] = -max_jerk*tj
            omega[i] = 0.5*max_jerk*Tj**2 + max_jerk*Tj*Tv - 0.5*max_jerk*tj**2
            theta[i] = (1/6)*max_jerk*Tj**3 + 0.5*max_jerk*Tj**2*Tv + \
                        0.5*max_jerk*Tj*Tv**2 + (0.5*max_jerk*Tj*Tv*tj) - (1/6)*max_jerk*tj**3
        elif ti < (4*Tj + Tv):
            tj = ti - (3*Tj + Tv)
            alpha[i] = -max_jerk*Tj + max_jerk*tj
            omega[i] = 0.5*max_jerk*Tj**2 + max_jerk*Tj*Tv - max_jerk*Tj*tj + 0.5*max_jerk*tj**2
            theta[i] = theta[i-1] + omega[i-1]*dt
        else:
            theta[i] = theta[i-1]
            omega[i] = 0
            alpha[i] = 0

    # Normalize to total displacement theta_final
    theta *= theta_final / theta[-1]
    omega *= (theta_final / theta[-1]) / (t[-1]/T)
    return theta, omega


# Generate 0->360°
theta1, omega1 = s_curve_profile(np.arange(0, T_move, dt), T_move, 2*np.pi)
# Dwell
theta2 = np.ones(int(T_dwell/dt)) * theta1[-1]
omega2 = np.zeros_like(theta2)
# 360->0°
theta3, omega3 = s_curve_profile(np.arange(0, T_move, dt), T_move, -2*np.pi)
theta3 += theta1[-1]

# Concatenate
theta = np.concatenate([theta1, theta2, theta3])
omega = np.concatenate([omega1, omega2, omega3])
t = np.arange(0, len(theta)) * dt

# ==========================================
# Convert angular position to step positions
# ==========================================
steps_per_rad = steps_per_rev / (2*np.pi)
step_positions = theta * steps_per_rad
step_indices = np.round(step_positions).astype(int)

# Rising edge times (unique step changes)
unique_indices, unique_times = np.unique(step_indices, return_index=True)
pulse_times = t[unique_times]

# Compute time deltas
dt_us = np.diff(pulse_times) * 1e6  # convert to microseconds
dir_bits = np.where(np.diff(np.sign(np.diff(theta, prepend=theta[0]))) >= 0, 1, 0)
dir_bits = dir_bits[:len(dt_us)]  # match lengths

# ==========================================
# Write binary and JSON
# ==========================================
record_count = len(dt_us)
with open("trajectory.bin", "wb") as f:
    for i in range(record_count):
        f.write(struct.pack("<IB", int(dt_us[i]), int(dir_bits[i])))

meta = {
    "magic": "STP1",
    "time_unit": "us",
    "record_size_bytes": 5,
    "fields": ["dt_us", "dir"],
    "record_count": record_count,
    "step_angle_deg": step_angle_deg,
    "microsteps": microsteps,
    "steps_per_rev": steps_per_rev,
    "description": "Each record defines time between consecutive rising edges; STEP high/low = dt/2."
}
with open("commands.json", "w") as f:
    json.dump(meta, f, indent=2)

print(f"Generated {record_count} step records.")
print("Files written: trajectory.bin and commands.json")
