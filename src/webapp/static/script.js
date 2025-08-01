const rowsContainer   = document.getElementById('rows');
const plotEl          = document.getElementById('plot');
const profileImg      = document.getElementById('profile-plot');
const addRowBtn       = document.getElementById('add-row');
const motorButtons    = document.querySelectorAll('.motor-btn');
const discretizeBtn   = document.getElementById('discretize-btn');
const outputDialog    = document.getElementById('output-dialog');
const stepAngleInput  = document.getElementById('step-angle');
const microstepsInput = document.getElementById('microsteps');

let currentMotor = 'X';
const savedRows  = { X: [], Y: [], Z: [] };

let plotDebounce, profileDebounce;
const DEBOUNCE_MS = 500;

// Read current rows into [{function,domain},…]
function getCurrentRows() {
  return Array.from(rowsContainer.querySelectorAll('.row'))
    .map(r => ({
      function: r.querySelector('.func-input').value.trim(),
      domain:   r.querySelector('.domain-input').value.trim()
    }))
    .filter(o => o.function && o.domain);
}

// Save/load per motor
function saveRows(m) { savedRows[m] = getCurrentRows(); }
function loadRows(m) {
  rowsContainer.innerHTML = '';
  const defs = savedRows[m].length
             ? savedRows[m]
             : [{function:'',domain:''}];
  defs.forEach(d => {
    const row = document.createElement('div');
    row.className = 'row';
    row.innerHTML = `
      <input type="text" class="func-input"   placeholder="e.g. sin(x)"    value="${d.function}">
      <input type="text" class="domain-input" placeholder="Domain: start,end" value="${d.domain}">
      <button type="button" class="remove-row">−</button>
    `;
    rowsContainer.appendChild(row);
  });
}

// Enable/disable discretize button and clear output dialog
function updateDiscretizeState() {
  const ok = ['X','Y','Z'].every(m => {
    const rows = savedRows[m];
    return rows.length>0 && rows.every(r => r.function && r.domain);
  });
  discretizeBtn.disabled = !ok;
  if (!ok) {
    outputDialog.textContent = 'Please fill in both function+domain for X, Y, and Z.';
    outputDialog.classList.remove('hidden');
  } else {
    outputDialog.textContent = '';
    outputDialog.classList.add('hidden');
  }
}

// --- Motor tabs ---
motorButtons.forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelector('.motor-btn.active').classList.remove('active');
    btn.classList.add('active');
    saveRows(currentMotor);
    currentMotor = btn.dataset.motor;
    loadRows(currentMotor);
    triggerPlot(); triggerProfile();
    updateDiscretizeState();
  });
});

// --- Plot piecewise (col 2) ---
async function doPlot() {
  const payload = { functions: getCurrentRows() };
  try {
    const res = await fetch('/plot', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify(payload)
    });
    if (!res.ok) throw new Error((await res.json()).error);
    const { img } = await res.json();
    plotEl.src = `data:image/png;base64,${img}`;
  } catch(_) {}
}
function triggerPlot() {
  clearTimeout(plotDebounce);
  plotDebounce = setTimeout(() => {
    saveRows(currentMotor);
    doPlot();
    updateDiscretizeState();
  }, DEBOUNCE_MS);
}

// --- Plot profile (col 1) ---
async function doProfile() {
  const payload = {
    functions:  getCurrentRows(),
    step_angle: parseFloat(stepAngleInput.value),
    microsteps: parseInt(microstepsInput.value,10)
  };
  try {
    const res = await fetch('/profile', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify(payload)
    });
    if (!res.ok) throw new Error((await res.json()).error);
    const { img } = await res.json();
    profileImg.src = `data:image/png;base64,${img}`;
  } catch(_) {}
}
function triggerProfile() {
  clearTimeout(profileDebounce);
  profileDebounce = setTimeout(() => {
    saveRows(currentMotor);
    doProfile();
    updateDiscretizeState();
  }, DEBOUNCE_MS);
}

// --- Form interactions ---
rowsContainer.addEventListener('input', e => {
  if (e.target.matches('.func-input, .domain-input')) {
    triggerPlot(); triggerProfile();
  }
});
addRowBtn.addEventListener('click', () => {
  const proto = rowsContainer.querySelector('.row');
  const clone = proto.cloneNode(true);
  clone.querySelector('.func-input').value = '';
  clone.querySelector('.domain-input').value = '';
  rowsContainer.appendChild(clone);
  triggerPlot(); triggerProfile();
});
rowsContainer.addEventListener('click', e => {
  if (!e.target.matches('.remove-row')) return;
  const all = rowsContainer.querySelectorAll('.row');
  if (all.length===1) {
    all[0].querySelector('.func-input').value = '';
    all[0].querySelector('.domain-input').value = '';
  } else {
    e.target.closest('.row').remove();
  }
  triggerPlot(); triggerProfile();
});

// --- Discretise button handler ---
discretizeBtn.addEventListener('click', async () => {
  // gather all three motor definitions
  const payload = {
    X: savedRows['X'],
    Y: savedRows['Y'],
    Z: savedRows['Z'],
    step_angle: parseFloat(stepAngleInput.value),
    microsteps: parseInt(microstepsInput.value,10)
  };

  outputDialog.textContent = 'Discretising… please wait';
  outputDialog.classList.remove('hidden');

  try {
    const res = await fetch('/discretize_all', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify(payload)
    });
    if (!res.ok) throw new Error((await res.json()).error);

    outputDialog.textContent = '🎉 Functions successfully discretised for X, Y, and Z.';
    outputDialog.classList.remove('hidden');
  } catch (e) {
    outputDialog.textContent = `❌ ${e.message}`;
    outputDialog.classList.remove('hidden');
  }
});

// --- Initialize ---
loadRows('X');
triggerPlot();
triggerProfile();
updateDiscretizeState();

