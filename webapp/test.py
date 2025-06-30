import struct
import numpy as np
import os

def read_trajectory(bin_path):
    """
    Reads a .bin file of the form:
      [uint32 N][ float32 t0, float32 a0, float32 t1, float32 a1, … ]
    Returns two 1-D NumPy arrays: times, angles.
    """
    with open(bin_path, 'rb') as f:
        # 1) read the 4-byte sample count
        N_bytes = f.read(4)
        if len(N_bytes) < 4:
            raise ValueError(f"{bin_path}: too short to contain count")
        N = struct.unpack('<I', N_bytes)[0]

        # 2) read the 2*N floats
        data = np.fromfile(f, dtype=np.float32, count=2 * N)
        if data.size < 2 * N:
            raise ValueError(f"{bin_path}: expected {2*N} floats, got {data.size}")

    # 3) split interleaved array into times & angles
    times  = data[0::2]
    angles = data[1::2]
    return times, angles

def dump_to_csv(times, angles, csv_path):
    """
    Saves two arrays (times, angles) into a CSV with header:
      time,angle
    """
    arr = np.vstack((times, angles)).T   # shape (N,2)
    np.savetxt(csv_path, arr, delimiter=',', header='time,angle', comments='')

if __name__ == '__main__':
    # assume your SD card folder is mounted at E:\traj\
    sd_folder = r"D:\traj"            # <— change to your mount point
    for axis in ('X','Y','Z'):
        bin_file = os.path.join(sd_folder, f"{axis}.bin")
        csv_file = os.path.join(sd_folder, f"{axis}.csv")
        print(f"Reading {bin_file}…")
        t,a = read_trajectory(bin_file)
        print(f" → {t.size} samples; exporting to {csv_file}")
        dump_to_csv(t, a, csv_file)
    print("Done. You can now open the .csv files in Excel.")
