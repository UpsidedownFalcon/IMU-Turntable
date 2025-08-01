import csv
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt 
import math 
import struct
import sys
import seaborn as sns
from scipy.stats import skew, kurtosis



# 1) Set the name of the rigid body you want
target_body = 'Frame'
file = input("Enter file name: ") 
t_off = 5.14
Y_off = -1.3



# 2) Read the first 8 rows to grab the “Name”, “Category” and sub-field labels
with open(file, newline='') as f:
    reader = csv.reader(f)
    header_rows = [next(reader) for _ in range(8)]
names_row      = header_rows[3]  # row with the rigid-body names
categories_row = header_rows[6]  # row that says 'Rotation' or 'Position'
subfields_row  = header_rows[7]  # row with 'X','Y','Z','W','X','Y','Z',…



# 3) Load the data, using row 7 as the header
df = pd.read_csv(file, skiprows=7)



# 4) Find which columns correspond to the rotation (quaternion) of the target
#    A rotation sub-field is one where names_row == target_body AND categories_row == 'Rotation'
quat_idxs = [i for i, (n, c) in enumerate(zip(names_row, categories_row))
             if n == target_body and c == 'Rotation']

if len(quat_idxs) != 4:
    raise RuntimeError(f"Found {len(quat_idxs)} rotation cols for '{target_body}', expected 4.")



# 5) Map those indices to the actual column names in df (they’ll be e.g. 'X.3', 'Y.3', …)
comp_map = {}
for i in quat_idxs:
    label = subfields_row[i]   # should be 'X', 'Y', 'Z' or 'W'
    comp_map[label] = df.columns[i]

qx_col = comp_map['X']
qy_col = comp_map['Y']
qz_col = comp_map['Z']
qw_col = comp_map['W']



# 6) Extract time and quaternion, convert to floats
time = df['Time (Seconds)'].astype(float)
qx   = df[qx_col].astype(float)
qy   = df[qy_col].astype(float)
qz   = df[qz_col].astype(float)
qw   = df[qw_col].astype(float)

print(qx)

# normalise 
# q = np.array([qx, qy, qz, qw], dtype=float)
# q /= np.linalg.norm(q)
# qx, qy, qz, qw = q



# 7) Quaternion → Euler (roll, pitch, yaw in degrees)
roll = np.degrees(np.arctan2(2*(qw*qx + qy*qz),
                             1 - 2*(qx*qx + qy*qy)))
pitch = np.degrees(np.arcsin(
    np.clip(2*(qw*qy - qz*qx), -1.0, 1.0)))
yaw = np.degrees(np.arctan2(2*(qw*qz + qx*qy),
                            1 - 2*(qy*qy + qz*qz)))  



# # roll  (ϕ)
# roll = np.arctan2(
#     2*(qw*qx + qy*qz),
#     1 - 2*(qx*qx + qy*qy)
# )

# # pitch (θ)
# t = qw*qy - qx*qz
# # optional clamping to avoid rounding errors: t = np.clip(t, -0.5, 0.5)
# pitch = -np.pi/2 + 2*np.arctan2(
#     np.sqrt(1 + 2*t),
#     np.sqrt(1 - 2*t)
# )

# # yaw   (ψ)
# yaw = np.arctan2(
#     2*(qw*qz + qx*qy),
#     1 - 2*(qy*qy + qz*qz)
# )



# 8) True values 
bin_path = input("Enter true trajectory file: ")
with open(bin_path, 'rb') as f:
    # read sample count
    cnt_bytes = f.read(4)
    if len(cnt_bytes) < 4:
        raise IOError("File too short to even contain a count")
    (N,) = struct.unpack('<I', cnt_bytes)

    print(f"# Read {N} samples from {bin_path}")
    print("time, angle")

    # pre-allocate arrays
    times = np.empty(N, dtype=np.float32)
    angles = np.empty(N, dtype=np.float32)

    # read and dump each pair into the arrays
    for i in range(N):
        pair = f.read(8)
        if len(pair) < 8:
            raise IOError(f"Unexpected EOF at sample {i}")
        true_t, true_angle = struct.unpack('<ff', pair)
        times[i] = true_t + t_off
        angles[i] = (-1* true_angle) + Y_off 
        print(f"{true_t:.6f}, {true_angle:.6f}") 



# 9) Plot
plt.figure(figsize=(10,6))
plt.plot(time, roll, label='Roll')
plt.plot(time, pitch, label='Pitch')
plt.plot(time, yaw, label='Yaw')
plt.plot(true_t, true_angle) 
plt.xlabel('Time (s)')
plt.ylabel('Angle (°)')
plt.title(f'Euler Angles Over Time')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show() 


plt.figure()
plt.plot(times, angles, 'x', label='Expected')            # plot angle vs time
plt.plot(time, yaw, 'x', label='Actual')   
plt.xlabel('Time')
plt.ylabel('Angle')
plt.title(f'Trajectory from {bin_path}')
plt.legend() 
plt.grid(True)
plt.tight_layout()
plt.show()

# from typing import List, Tuple, Optional
# import math

# def process_data_windows(
#     t_start: float,
#     t_end: float,
#     window_size: float,
#     times: List[float],
#     values: List[float],
#     clearance: float
# ) -> Tuple[List[Optional[float]], List[Optional[float]], List[Optional[float]]]:
#     """
#     Partition [t_start, t_end) into windows of width `window_size`.  For each window w:
#       - define inner_start = w_start + clearance
#       - define inner_end   = w_end   - clearance
#       - collect all (t,y) with inner_start <= t <= inner_end
#       - compute mean(y), and min/max successive Δy within that subset
    
#     Returns three lists (one entry per window):
#       means, min_deltas, max_deltas
#     Entries are None if too few points to compute.
#     """
#     if clearance * 2 >= window_size:
#         raise ValueError("clearance is too large for the given window_size")
    
#     # number of windows (round up partial at the end)
#     n_windows = math.ceil((t_end - t_start) / window_size)
    
#     # sort data once
#     paired = sorted(zip(times, values), key=lambda tv: tv[0])
    
#     means: List[Optional[float]] = []
#     min_deltas: List[Optional[float]] = []
#     max_deltas: List[Optional[float]] = []
    
#     for w in range(n_windows):
#         w_start = t_start + w * window_size
#         w_end   = w_start + window_size
        
#         inner_start = w_start + clearance
#         inner_end   = w_end   - clearance
        
#         # grab only the points in [inner_start, inner_end]
#         ys = [y for t, y in paired if inner_start <= t <= inner_end]
        
#         # mean
#         mean_y = sum(ys)/len(ys) if ys else None
        
#         # successive deltas
#         if len(ys) >= 2:
#             deltas = [ys[i+1] - ys[i] for i in range(len(ys)-1)]
#             min_d, max_d = min(deltas), max(deltas)
#         else:
#             min_d = max_d = None
        
#         means.append(mean_y)
#         min_deltas.append(min_d)
#         max_deltas.append(max_d)
    
#     return means, min_deltas, max_deltas




# means, mins, maxs = process_data_windows(
#     t_start=5.860,
#     t_end=43.860,
#     window_size=2.0,
#     times=time,
#     values=pitch,
#     clearance=0.2
# )
# print("means:", means)
# print("min deltas:", mins)
# print("max deltas:", maxs) 



# 10) Error analyses 
def analyze_deviation(values, time, exp_val, exp_time, start_time, end_time, window_size=0.02):
    # Mask data within the desired window
    mask = (time >= start_time) & (time <= end_time)
    values_window = np.array(values)[mask]
    time_window = np.array(time)[mask]

    # Interpolate expected values to align with measured time base
    interp_exp_val = np.interp(time_window, exp_time, exp_val)

    # Compute deviation (same length as time_window)
    deviation = values_window - interp_exp_val

    # Compute resolution (dy) as mean value difference over 0.02s windows
    dy = []
    t_start = start_time

    while t_start + window_size <= end_time:
        t_end = t_start + window_size

        # Mask for current and next windows
        mask_current = (time_window >= t_start) & (time_window < t_end)
        mask_next = (time_window >= t_end) & (time_window < t_end + window_size)

        if np.any(mask_current) and np.any(mask_next):
            mean_current = np.mean(values_window[mask_current])
            mean_next = np.mean(values_window[mask_next])
            dy.append(mean_next - mean_current)

        t_start += window_size

    dy = np.array(dy)

    return deviation, dy




def plot_distributions(deviation, dy):
    plt.figure(figsize=(14, 6))

    # Deviation plot
    plt.subplot(1, 2, 1)
    sns.histplot(deviation, bins=30, kde=True, color='steelblue')
    plt.title("Distribution of Y-Axis Deviation")
    plt.xlabel("Deviation / °")
    plt.ylabel("Frequency")
    plt.grid(True) 

    # Resolution plot
    plt.subplot(1, 2, 2)
    sns.histplot(dy, bins=30, kde=True, color='darkorange')
    plt.title("Distribution of Measured Y-Axis Resolution")
    plt.xlabel("Δ° between adjacent measured positions / °")
    plt.ylabel("Frequency")
    plt.grid(True) 

    plt.tight_layout()
    plt.show()



def describe_distribution(name, data):
    print(f"📈 Statistics for {name}:")
    print(f"  Mean     : {np.mean(data):.4f}")
    print(f"  Std Dev  : {np.std(data):.4f}")
    print(f"  Median   : {np.median(data):.4f}")
    print(f"  Min      : {np.min(data):.4f}")
    print(f"  Max      : {np.max(data):.4f}")
    print(f"  Skewness : {skew(data):.4f}")
    print(f"  Kurtosis : {kurtosis(data):.4f}\n") 


t1 = float(input("Enter start time: "))
t2 = float(input("Enter end time: ")) 

deviation, dy = analyze_deviation(yaw, time, angles, times, t1, t2)
print("Deviation:", deviation)
print("dy:", dy)

plot_distributions(deviation, dy)
describe_distribution("Deviation", deviation)
describe_distribution("ΔY (Resolution)", dy)
