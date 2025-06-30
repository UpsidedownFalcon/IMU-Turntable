from flask import Flask, render_template, request, jsonify
import io, base64
import numpy as np
import matplotlib.pyplot as plt
import sympy as sp

app = Flask(__name__)

def discretize_function(func, t_start, t_end, step_angle, microsteps, initial_samples=1000):
    """
    Discretize a continuous function for stepper motor control.
    Returns (angles: np.ndarray, dt: float).
    """
    theta_res = step_angle / microsteps

    # estimate max velocity
    t_init     = np.linspace(t_start, t_end, initial_samples)
    theta_init = func(t_init)
    dtheta_dt  = np.diff(theta_init) / np.diff(t_init)
    vmax       = np.max(np.abs(dtheta_dt)) if dtheta_dt.size else 0.0

    dt = theta_res / vmax if vmax > 0 else (t_end - t_start)
    N  = int(np.ceil((t_end - t_start) / dt)) + 1
    times  = np.linspace(t_start, t_end, N)
    angles = func(times)

    ys = np.array(angles, dtype=float)
    if ys.ndim == 0:
        ys = np.full(N, ys.item(), float)
    elif ys.shape != (N,):
        ys = np.broadcast_to(ys, (N,))

    return ys, dt

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/plot", methods=["POST"])
def plot():
    data  = request.get_json()
    items = data.get("functions", [])
    if not items:
        return jsonify({"error": "No functions provided"}), 400

    x = sp.symbols('x')
    parsed = []
    gmin = gmax = None

    for idx, it in enumerate(items, start=1):
        expr_str = it.get("function","").strip()
        dom_str  = it.get("domain","").strip()
        if not expr_str or not dom_str:
            continue

        try:
            a, b = map(float, dom_str.split(","))
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
        ys = np.array(ys, float)
        if ys.ndim==0:
            ys = np.full(mask.sum(), ys.item(), float)
        elif ys.shape!=(mask.sum(),):
            ys = np.broadcast_to(ys, (mask.sum(),))
        ax.plot(xs[mask], ys, label=f"y = {expr_str}")

    ax.set_xlim(gmin, gmax)
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.grid(True)
    ax.legend(loc="best", fontsize="small")

    buf = io.BytesIO()
    fig.savefig(buf, format="png", bbox_inches="tight")
    plt.close(fig)
    buf.seek(0)
    img_b64 = base64.b64encode(buf.read()).decode('ascii')
    return jsonify({"img": img_b64})

def discretize_function(func, t_start, t_end, step_angle, microsteps, initial_samples=1000):
    """
    Discretize a continuous function for stepper motor control, robust to constant functions.
    Returns (angles: np.ndarray, dt: float).
    """
    theta_res = step_angle / microsteps

    # 1) Initial sampling to estimate vmax
    t_init     = np.linspace(t_start, t_end, initial_samples)
    theta_init = func(t_init)

    # Broadcast theta_init into a vector if scalar or mismatched
    theta_arr = np.array(theta_init, dtype=float)
    if theta_arr.ndim == 0:
        theta_arr = np.full(t_init.shape, theta_arr.item(), dtype=float)
    elif theta_arr.shape != t_init.shape:
        theta_arr = np.broadcast_to(theta_arr, t_init.shape)

    dtheta_dt = np.diff(theta_arr) / np.diff(t_init)
    vmax      = np.max(np.abs(dtheta_dt)) if dtheta_dt.size else 0.0

    # 2) Compute Δt
    dt = theta_res / vmax if vmax > 0 else (t_end - t_start)

    # 3) Final sampling
    N      = int(np.ceil((t_end - t_start) / dt)) + 1
    times  = np.linspace(t_start, t_end, N)
    angles = func(times)

    # Broadcast final angles array
    ang_arr = np.array(angles, dtype=float)
    if ang_arr.ndim == 0:
        ang_arr = np.full(times.shape, ang_arr.item(), dtype=float)
    elif ang_arr.shape != times.shape:
        ang_arr = np.broadcast_to(ang_arr, times.shape)

    return ang_arr, dt

def discretize_all_segments(funcs, step_angle, microsteps):
    """
    funcs: list of (f_callable, t_start, t_end)
    Returns a single (times: np.ndarray, angles: np.ndarray) trajectory.
    """
    all_times = []
    all_angles = []

    for f, t_start, t_end in funcs:
        ang_arr, dt = discretize_function(f, t_start, t_end, step_angle, microsteps)
        times = t_start + np.arange(len(ang_arr)) * dt

        # Avoid duplicating the boundary
        if all_times:
            times  = times[1:]
            ang_arr = ang_arr[1:]

        all_times.append(times)
        all_angles.append(ang_arr)

    times  = np.concatenate(all_times)  if all_times  else np.array([])
    angles = np.concatenate(all_angles) if all_angles else np.array([])

    np.savetxt('times.csv', times, delimiter=',')
    np.savetxt('angles.csv', angles, delimiter=',')

    return times, angles

@app.route("/discretize", methods=["POST"])
def discretize():
    """
    Expects JSON:
    {
      "functions": [ { "function": "...", "domain": "start,end" }, … ],
      "step_angle": float,
      "microsteps": int
    }
    Returns:
    {
      "times":  [...],
      "angles": [...]
    }
    """
    data       = request.get_json()
    items      = data.get("functions", [])
    step_angle = float(data.get("step_angle", 1.8))
    microsteps = int(data.get("microsteps", 16))

    if not items:
        return jsonify({"error": "No functions provided"}), 400

    x     = sp.symbols('x')
    funcs = []
    for idx, it in enumerate(items, start=1):
        fn_str = it.get("function","").strip()
        dom    = it.get("domain","").strip()
        if not fn_str or not dom:
            continue

        try:
            t0, t1 = map(float, dom.split(","))
        except:
            return jsonify({"error": f"Invalid domain on row {idx}. Use start,end"}), 400

        try:
            expr = sp.sympify(fn_str)
            f    = sp.lambdify(x, expr, "numpy")
        except:
            return jsonify({"error": f"Invalid expression on row {idx}: {fn_str}"}), 400

        funcs.append((f, t0, t1))

    if not funcs:
        return jsonify({"error": "No valid segments to discretize"}), 400

    times, angles = discretize_all_segments(funcs, step_angle, microsteps)
    return jsonify({
        "times":  times.tolist(),
        "angles": angles.tolist()
    })



if __name__ == "__main__":
    app.run(debug=True)
