from flask import Flask, render_template, request, jsonify
import io, base64
import numpy as np
import matplotlib.pyplot as plt
import sympy as sp
import csv
import struct
import serial
import time

TRAJECTORIES = {
  'X': {'times': None, 'angles': None},
  'Y': {'times': None, 'angles': None},
  'Z': {'times': None, 'angles': None},
}

port = '/dev/ttyACM0'
baudrate = 115200 
serial_conn = serial.Serial(port, baudrate, timeout=2) 

MIN_DT = 0.002

app = Flask(__name__)

def discretize_function(func, t_start, t_end, step_angle, microsteps, initial_samples=1000):
    """
    Discretize a continuous function for stepper motor control, robust to constant functions.
    Returns:
      times:  np.ndarray of timestamps
      angles: np.ndarray of angle values
      dt:      float, time step used
    """
    theta_res = step_angle / microsteps

    # 1) Initial sampling to estimate max velocity
    t_init     = np.linspace(t_start, t_end, initial_samples)
    theta_init = func(t_init)
    theta_arr  = np.array(theta_init, dtype=float)
    if theta_arr.ndim == 0:
        theta_arr = np.full(t_init.shape, theta_arr.item())
    elif theta_arr.shape != t_init.shape:
        theta_arr = np.broadcast_to(theta_arr, t_init.shape)

    dtheta_dt = np.diff(theta_arr) / np.diff(t_init)
    vmax      = np.max(np.abs(dtheta_dt)) if dtheta_dt.size else 0.0

    # 2) Compute dt so each microstep ≤ theta_res
    raw_dt = theta_res / vmax if vmax > 0 else (t_end - t_start)
    dt = max(raw_dt, MIN_DT)

    # 3) Final uniform sampling
    N     = int(np.ceil((t_end - t_start) / dt)) + 1
    times = np.linspace(t_start, t_end, N)
    angles = func(times)

    ang_arr = np.array(angles, dtype=float)
    if ang_arr.ndim == 0:
        ang_arr = np.full(times.shape, ang_arr.item())
    elif ang_arr.shape != times.shape:
        ang_arr = np.broadcast_to(ang_arr, times.shape)

    return times, ang_arr, dt

def discretize_all_segments(funcs, step_angle, microsteps):
    """
    funcs: list of (f_callable, t0, t1)
    Returns a single (times, angles) trajectory by concatenating segments.
    """
    all_times = []
    all_angles = []

    for f, t0, t1 in funcs:
        seg_t, seg_ang, _ = discretize_function(f, t0, t1, step_angle, microsteps)
        if all_times:  # drop duplicate boundary
            seg_t   = seg_t[1:]
            seg_ang = seg_ang[1:]
        all_times.append(seg_t)
        all_angles.append(seg_ang)

    if not all_times:
        return np.array([]), np.array([])

    return np.concatenate(all_times), np.concatenate(all_angles)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/plot", methods=["POST"])
def plot():
    data  = request.get_json()
    items = data.get("functions", [])
    if not items:
        return jsonify({"error":"No functions provided"}), 400

    x = sp.symbols('x')
    parsed = []
    gmin = gmax = None

    for idx, it in enumerate(items, start=1):
        expr_str = it.get("function","").strip()
        dom_str  = it.get("domain","").strip()
        if not expr_str or not dom_str:
            continue
        try:
            a,b = map(float, dom_str.split(","))
        except:
            return jsonify({"error":f"Invalid domain on row {idx}. Use start,end"}),400
        gmin = a if gmin is None or a<gmin else gmin
        gmax = b if gmax is None or b>gmax else gmax
        try:
            expr = sp.sympify(expr_str)
            f    = sp.lambdify(x, expr, "numpy")
        except:
            return jsonify({"error":f"Invalid expression on row {idx}: {expr_str}"}),400
        parsed.append((expr_str,f,a,b))

    if not parsed:
        return jsonify({"error":"No valid function/domain pairs found"}),400

    xs = np.linspace(gmin, gmax, 1000)
    fig, ax = plt.subplots()
    for expr_str,f,a,b in parsed:
        mask = (xs>=a)&(xs<=b)
        if not mask.any(): continue
        ys = f(xs[mask])
        ys = np.array(ys, dtype=float)
        if ys.ndim==0:
            ys = np.full(mask.sum(), ys.item())
        elif ys.shape!=(mask.sum(),):
            ys = np.broadcast_to(ys, (mask.sum(),))
        ax.plot(xs[mask], ys, label=f"y = {expr_str}")
    ax.set_xlim(gmin, gmax)
    ax.set_xlabel("x"); ax.set_ylabel("y"); ax.grid(True); ax.legend(fontsize="small")
    buf = io.BytesIO()
    fig.savefig(buf, format="png", bbox_inches="tight"); plt.close(fig)
    buf.seek(0)
    return jsonify({"img": base64.b64encode(buf.read()).decode('ascii')})

@app.route("/discretize", methods=["POST"])
def discretize():
    data       = request.get_json()
    motor      = data.get("motor")     # e.g. "X" or "Y" or "Z"
    items      = data.get("functions", [])
    step_angle = float(data.get("step_angle", 1.8))
    microsteps = int(data.get("microsteps", 16))
    if not items:
        return jsonify({"error":"No functions provided"}),400

    x = sp.symbols('x')
    funcs = []
    for idx,it in enumerate(items, start=1):
        fn = it.get("function","").strip()
        dm = it.get("domain","").strip()
        if not fn or not dm: continue
        try:
            t0,t1 = map(float, dm.split(","))
        except:
            return jsonify({"error":f"Invalid domain on row {idx}. Use start,end"}),400
        try:
            expr = sp.sympify(fn)
            f    = sp.lambdify(x, expr, "numpy")
        except:
            return jsonify({"error":f"Invalid expression on row {idx}: {fn}"}),400
        funcs.append((f,t0,t1))

    if not funcs:
        return jsonify({"error":"No valid segments to discretize"}),400

    times, angles = discretize_all_segments(funcs, step_angle, microsteps)

    if motor in TRAJECTORIES:
        TRAJECTORIES[motor]['times']  = times
        TRAJECTORIES[motor]['angles'] = angles
        print(motor)
        # if motor == 'Z':
        #     save_all_trajectories_csv()

    return jsonify({"times":times.tolist(), "angles":angles.tolist()})

@app.route("/profile", methods=["POST"])
def profile():
    data       = request.get_json()
    items      = data.get("functions", [])
    step_angle = float(data.get("step_angle", 1.8))
    microsteps = int(data.get("microsteps", 16))
    if not items:
        return jsonify({"error":"No functions provided"}),400

    x = sp.symbols('x')
    segments = []
    # first parse each segment
    for idx,it in enumerate(items, start=1):
        fn = it.get("function","").strip()
        dm = it.get("domain","").strip()
        if not fn or not dm: continue
        try:
            t0,t1 = map(float, dm.split(","))
        except:
            return jsonify({"error":f"Invalid domain on row {idx}. Use start,end"}),400
        try:
            expr = sp.sympify(fn)
            f    = sp.lambdify(x, expr, "numpy")
        except:
            return jsonify({"error":f"Invalid expression on row {idx}: {fn}"}),400
        segments.append((fn, f, t0, t1))

    if not segments:
        return jsonify({"error":"No valid segments"}),400

    # build plot with one line per segment
    fig, axs = plt.subplots(3,1, figsize=(6,8), sharex=True)
    for label, f, t0, t1 in segments:
        times, angles, _ = discretize_function(f, t0, t1, step_angle, microsteps)
        vel = np.gradient(angles, times)
        acc = np.gradient(vel,    times)
        axs[0].plot(times, angles, label=label)
        axs[1].plot(times, vel,    label=label)
        axs[2].plot(times, acc,    label=label)

    axs[0].set_ylabel("Angle (°)");    axs[0].grid(True); axs[0].legend(fontsize="small")
    axs[1].set_ylabel("Velocity (°/s)");axs[1].grid(True); axs[1].legend(fontsize="small")
    axs[2].set_ylabel("Acceleration (°/s²)");axs[2].grid(True); axs[2].legend(fontsize="small")
    axs[2].set_xlabel("Time (s)")
    fig.tight_layout()

    buf = io.BytesIO()
    fig.savefig(buf, format="png", bbox_inches="tight")
    plt.close(fig)
    buf.seek(0)
    return jsonify({"img": base64.b64encode(buf.read()).decode('ascii')})


@app.route("/discretize_all", methods=["POST"])
def discretize_all():
    """
    Expects JSON:
    {
      "X": [ { "function": "...", "domain": "start,end" }, … ],
      "Y": [ … ],
      "Z": [ … ],
      "step_angle": float,
      "microsteps": int
    }
    Returns JSON:
    {
      "X": { "times": [...], "angles": [...] },
      "Y": { "times": [...], "angles": [...] },
      "Z": { "times": [...], "angles": [...] }
    }
    """
    data       = request.get_json()
    step_angle = float(data.get("step_angle", 1.8))
    microsteps = int(data.get("microsteps", 16))
    result = {}

    for axis in ("X","Y","Z"):
        segs = data.get(axis, [])
        if not isinstance(segs, list) or not segs:
            return jsonify({"error": f"No segments provided for motor {axis}"}), 400

        # parse piecewise for this axis
        x = sp.symbols('x')
        funcs = []
        for idx, it in enumerate(segs, start=1):
            fn = it.get("function","").strip()
            dm = it.get("domain","").strip()
            if not fn or not dm:
                return jsonify({"error": f"Empty function/domain on {axis} row {idx}"}), 400
            try:
                t0, t1 = map(float, dm.split(","))
            except:
                return jsonify({"error": f"Invalid domain on {axis} row {idx}: {dm}"}), 400
            try:
                expr = sp.sympify(fn)
                f    = sp.lambdify(x, expr, "numpy")
            except:
                return jsonify({"error": f"Invalid expression on {axis} row {idx}: {fn}"}), 400
            funcs.append((f, t0, t1))

        # discretize
        times, angles = discretize_all_segments(funcs, step_angle, microsteps)
        # store into global if you like
        TRAJECTORIES[axis]['times']  = times
        TRAJECTORIES[axis]['angles'] = angles

        result[axis] = {
            "times":  times.tolist(),
            "angles": angles.tolist()
        }

    try: 
        send_trajectories_over_serial(serial_conn, trajectories=TRAJECTORIES) 
    except serial.SerialTimeoutException: 
        return jsonify(error="Serial write timed out"), 500
    except Exception as e: 
        return jsonify(error=f"Serial error: {e}"), 500
    return jsonify(result) 





def send_trajectories_over_serial(port: str, baudrate: int, trajectories: dict, timeout: float = 2.0):
    """
    Send the trajectories in the form:
      trajectories = {
        'X': {'times': np.ndarray, 'angles': np.ndarray},
        'Y': {...}, 'Z': {...}
      }
    over USB serial to a Teensy configured to accept:
      BEGIN <Axis> <size>\n
      <raw binary (uint32 N, N×float32 times, N×float32 angles)>
    and finally a "GO\n" command.
    """

    # Open the serial port
    ser = serial.Serial(port, baudrate, timeout=timeout)
    time.sleep(0.2)  # give the Teensy a moment to reset

    for axis in ('X','Y','Z'):
        traj = trajectories.get(axis, {})
        times = traj.get('times')
        angles = traj.get('angles')
        if times is None or angles is None:
            print(f"[!] No data for axis {axis}, skipping.")
            continue

        if len(times) != len(angles):
            raise ValueError(f"Axis {axis}: times and angles length mismatch")

        N = len(times)
        # Build binary payload
        buf = bytearray()
        buf += struct.pack('<I', N)  # 4-byte count
        for t, a in zip(times, angles):
            buf += struct.pack('<ff', float(t), float(a))

        # Send header
        header = f"BEGIN {axis} {len(buf)}\n".encode('ascii')
        ser.write(header)
        ser.flush()

        # Send data
        ser.write(buf)
        ser.flush()

        # Wait for Teensy to ACK (optional)
        ack = ser.readline().decode('ascii', errors='ignore').strip()
        if ack != 'OK':
            print(f"[!] No OK ack for axis {axis}, got: {ack!r}")
        else:
            print(f"[+] Sent {axis}: {N} samples")

    # Finally send the GO command
    ser.write(b"GO\n")
    ser.flush()
    print("[+] Sent GO")

    ser.close()






# def save_all_trajectories_csv(path="all_trajectories.csv"):
#     """
#     Dump TRAJECTORIES for X,Y,Z into one CSV with columns:
#     X_times, X_angles, Y_times, Y_angles, Z_times, Z_angles
#     """
#     # fetch each motor’s data or default to empty list
#     Xt = TRAJECTORIES['X']['times']
#     Xa = TRAJECTORIES['X']['angles']
#     Yt = TRAJECTORIES['Y']['times']
#     Ya = TRAJECTORIES['Y']['angles']
#     Zt = TRAJECTORIES['Z']['times']
#     Za = TRAJECTORIES['Z']['angles']

#     Xt_list = list(Xt) if isinstance(Xt, np.ndarray) else (Xt or [])
#     Xa_list = list(Xa) if isinstance(Xa, np.ndarray) else (Xa or [])
#     Yt_list = list(Yt) if isinstance(Yt, np.ndarray) else (Yt or [])
#     Ya_list = list(Ya) if isinstance(Ya, np.ndarray) else (Ya or [])
#     Zt_list = list(Zt) if isinstance(Zt, np.ndarray) else (Zt or [])
#     Za_list = list(Za) if isinstance(Za, np.ndarray) else (Za or [])

#     max_len = max(len(Xt_list), len(Xa_list),
#                   len(Yt_list), len(Ya_list),
#                   len(Zt_list), len(Za_list))

#     with open(path, 'w', newline='') as f:
#         writer = csv.writer(f)
#         writer.writerow([
#             'X_times','X_angles',
#             'Y_times','Y_angles',
#             'Z_times','Z_angles'
#         ])
#         for i in range(max_len):
#             writer.writerow([
#                 Xt_list[i] if i < len(Xt_list) else '',
#                 Xa_list[i] if i < len(Xa_list) else '',
#                 Yt_list[i] if i < len(Yt_list) else '',
#                 Ya_list[i] if i < len(Ya_list) else '',
#                 Zt_list[i] if i < len(Zt_list) else '',
#                 Za_list[i] if i < len(Za_list) else ''
#             ])
#     print(f"✅ All trajectories saved to {path}")


if __name__ == "__main__":
    app.run(debug=True)
