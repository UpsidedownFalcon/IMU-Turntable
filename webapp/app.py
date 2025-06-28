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

@app.route("/discretize", methods=["POST"])
def discretize():
    """
    Expects JSON:
    {
      "functions": [
        { "function": "...", "domain": "start,end" }, ...
      ],
      "step_angle": float,
      "microsteps": int
    }
    Returns:
    {
      "segments": [
        { "domain":"0,10", "dt":0.0029, "angles":[…] }, …
      ]
    }
    """
    data       = request.get_json()
    items      = data.get("functions", [])
    step_angle = float(data.get("step_angle", 1.8))
    microsteps = int(data.get("microsteps", 16))

    if not items:
        return jsonify({"error":"No functions provided"}), 400

    x = sp.symbols('x')
    segments = []
    for idx, it in enumerate(items, start=1):
        fn_str = it.get("function","").strip()
        dom_str= it.get("domain","").strip()
        if not fn_str or not dom_str:
            continue

        try:
            t_start, t_end = map(float, dom_str.split(","))
        except:
            return jsonify({"error":f"Invalid domain on row {idx}. Use start,end"}),400

        try:
            expr = sp.sympify(fn_str)
            f    = sp.lambdify(x, expr, "numpy")
        except:
            return jsonify({"error":f"Invalid expression on row {idx}: {fn_str}"}),400

        angles, dt = discretize_function(f, t_start, t_end, step_angle, microsteps)
        segments.append({
            "domain": dom_str,
            "dt": dt,
            "angles": angles.tolist()
        })

    if not segments:
        return jsonify({"error":"No valid segments to discretize"}),400

    return jsonify({"segments": segments})

if __name__ == "__main__":
    app.run(debug=True)
