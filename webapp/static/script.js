const rowsContainer = document.getElementById('rows');
const plotEl        = document.getElementById('plot');
const errDiv        = document.getElementById('error');
const addRowBtn     = document.getElementById('add-row');
const form          = document.getElementById('func-form');

let debounceId = null;
const DEBOUNCE_MS = 500;

// Plot‐all logic
async function doPlot() {
  const rows = Array.from(rowsContainer.querySelectorAll('.row'));
  const payload = {
    functions: rows.map(r => ({
      function: r.querySelector('.func-input').value.trim(),
      domain:   r.querySelector('.domain-input').value.trim()
    }))
  };

  try {
    const res = await fetch('/plot', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify(payload)
    });
    if (!res.ok) {
      const { error } = await res.json();
      throw new Error(error || 'Error plotting');
    }
    const { img } = await res.json();
    plotEl.src = `data:image/png;base64,${img}`;
    errDiv.classList.add('hidden');
  } catch (e) {
    errDiv.textContent = e.message;
    errDiv.classList.remove('hidden');
  }
}

function schedulePlot() {
  clearTimeout(debounceId);
  debounceId = setTimeout(doPlot, DEBOUNCE_MS);
}

// 1) Live‐plot on input change
rowsContainer.addEventListener('input', e => {
  if (e.target.matches('.func-input, .domain-input')) {
    schedulePlot();
  }
});

// 2) Add new row
addRowBtn.addEventListener('click', () => {
  const proto = rowsContainer.querySelector('.row');
  const clone = proto.cloneNode(true);
  clone.querySelector('.func-input').value = '';
  clone.querySelector('.domain-input').value = '';
  rowsContainer.appendChild(clone);
  schedulePlot();
});

// 3) Remove row
rowsContainer.addEventListener('click', e => {
  if (e.target.matches('.remove-row')) {
    const allRows = rowsContainer.querySelectorAll('.row');
    if (allRows.length === 1) {
      const r = allRows[0];
      r.querySelector('.func-input').value = '';
      r.querySelector('.domain-input').value = '';
    } else {
      e.target.closest('.row').remove();
    }
    schedulePlot();
  }
});

// 4) Manual Plot All
form.addEventListener('submit', e => {
  e.preventDefault();
  clearTimeout(debounceId);
  doPlot();
});

// 5) Discretization logic for all segments
const discretizeBtn = document.getElementById('discretize-btn');
const resultPre     = document.getElementById('discrete-result');

discretizeBtn.addEventListener('click', async () => {
  const rows = Array.from(rowsContainer.querySelectorAll('.row'));
  const funcs = rows
    .map(r => ({
      function: r.querySelector('.func-input').value.trim(),
      domain:   r.querySelector('.domain-input').value.trim()
    }))
    .filter(({function: fn, domain}) => fn && domain);

  if (!funcs.length) {
    alert('Enter at least one function + domain.');
    return;
  }

  const stepAngle = parseFloat(document.getElementById('step-angle').value);
  const microsteps = parseInt(document.getElementById('microsteps').value, 10);

  const payload = {
    functions: funcs,
    step_angle: stepAngle,
    microsteps: microsteps
  };

  try {
    const res = await fetch('/discretize', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify(payload)
    });
    if (!res.ok) {
      const { error } = await res.json();
      throw new Error(error || 'Discretization failed');
    }
    const { segments } = await res.json();
    resultPre.textContent = segments.map((seg, i) => {
      const line1 = `Piece ${i+1}: domain=${seg.domain}, dt=${seg.dt.toFixed(6)}s, samples=${seg.angles.length}`;
      const line2 = seg.angles.map(a => a.toFixed(3)).join(', ');
      return line1 + '\n' + line2;
    }).join('\n\n');
    resultPre.classList.remove('hidden');
  } catch (e) {
    alert(e.message);
  }
});
