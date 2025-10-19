const script = document.createElement('script');
script.src = "https://cdn.jsdelivr.net/npm/chart.js";
// assign onload before appending to avoid a race where the script loads
// before we set the handler
script.onload = onChartLoaded;
document.head.appendChild(script);

// example datasets as CSV strings (three CSVs for the three charts)
const demoData = [{"time":"10-19 12:34","value":42},{"time":"10-19 12:24","value":40},{"time":"10-19 12:14","value":38},{"time":"10-19 12:04","value":36},{"time":"10-19 11:54","value":35},{"time":"10-19 11:44","value":34},{"time":"10-19 11:34","value":33},{"time":"10-19 11:24","value":32},{"time":"10-19 11:14","value":31},{"time":"10-19 11:04","value":30}];

const wsProtocol = (location.protocol === 'https:') ? 'wss:' : 'ws:';
var connection = new WebSocket(wsProtocol + '//' + location.hostname + ':81/', ['arduino']);
connection.onopen = function () {
  connection.send('Connect ' + new Date());
};
connection.onerror = function (error) {
  console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
  console.log('Server: ', e.data);
};
connection.onclose = function () {
  console.log('WebSocket connection closed');
};

// parse demo data and render charts once Chart.js has loaded
function onChartLoaded() {
  startHttpPolling(5000);

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', doRenderCharts);
  } else {
    doRenderCharts();
  }
}

// Start HTTP polling for sensor files 
function parseFileContentToSamples(content) {
  const trimmed = (content || '').trim();
  if (!trimmed) return [];
  if (trimmed.startsWith('[')) {
    try {
      const arr = JSON.parse(trimmed);
      return Array.isArray(arr) ? arr : [];
    } catch (err) {
      console.warn('Failed to parse JSON content:', err);
      return [];
    }
  }
  return samples;
}

async function fetchAndUpdateChart(sensor, path) {
  console.log(`Fetching data for sensor "${sensor}" from ${path}`);
  try {
    const resp = await fetch(path, { cache: 'no-store' });
    if (!resp.ok) {
      console.warn(`Failed to fetch ${path}: ${resp.status}`);
      return;
    }
    const text = await resp.text();
    const samples = parseFileContentToSamples(text);
    console.log(samples)
    const canvasId = sensorToCanvas(sensor);
    if (!canvasId) return;
    renderChart(canvasId, samples, sensor);
  } catch (err) {
    console.error('Error fetching sensor data', path, err);
  }
}

function startHttpPolling(interval = 5000) {
  // Map sensors to their file paths
  const mapping = [
    { sensor: 'moisture', path: '/moisture.json' },
    { sensor: 'temperature', path: '/temp.json' },
    { sensor: 'light', path: '/light.json' }
  ];

  // Initial fetch
  mapping.forEach(m => fetchAndUpdateChart(m.sensor, m.path));

  // Periodic polling
  setInterval(() => {
    mapping.forEach(m => fetchAndUpdateChart(m.sensor, m.path));
  }, interval);
}

function sensorToCanvas(sensor) {
  switch (sensor) {
    case 'moisture': return 'chartCanvas1'; 
    case 'temperature': return 'chartCanvas2';
    case 'light': return 'chartCanvas3';
    default: return null;
  }
}

function sendWaterCommand() {
  try {
    connection.send('waterplant');
    console.log('Sent waterplant command to server');
  } catch (err) {
    console.warn('Failed to send water command:', err);
  }
}

// Defer DOM access until the DOM is ready to avoid null element errors
document.addEventListener('DOMContentLoaded', () => {
  const homeBtn = document.getElementById('home-btn');
  const waterBtn = document.getElementById('water-btn');

  if (homeBtn) {
    homeBtn.addEventListener('click', () => {
      window.location.href = "http://potplant.local/calendar.html";
    });
  } else {
    console.warn('home-btn not found in DOM');
  }

  if (waterBtn) {
    waterBtn.addEventListener('click', sendWaterCommand);
  } else {
    console.warn('water-btn not found in DOM');
  }
});


function renderChart(canvasId, data, label) {

  
  const canvas = document.getElementById(canvasId);
  if (!canvas) {
    console.warn(`Canvas with id "${canvasId}" not found, skipping chart render.`);
    return;
  }
  const ctx = canvas.getContext('2d');
  if (!window.myCharts) window.myCharts = {};
  if (window.myCharts[canvasId]) window.myCharts[canvasId].destroy();
  // Expect `data` as an array of objects: { time: "MM-DD HH:MM", value: N }
  // Chart.js expects labels[] and a dataset[]. Map accordingly.
  const ordered = Array.isArray(data) ? data.slice().reverse() : [];
  const labels = ordered.map(d => d.time || '');
  const values = ordered.map(d => (typeof d.value === 'number') ? d.value : Number(d.value) || 0);

  const chartData = {
    labels: labels,
    datasets: [{
      label: label,
      data: values,
      fill: false,
      tension: 0.1,
      borderColor: 'rgba(75, 192, 192, 1)',
      backgroundColor: 'rgba(75, 192, 192, 0.2)'
    }]
  };

  window.myCharts[canvasId] = new Chart(ctx, {
    type: 'line',
    data: chartData,
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        x: { display: true },
        y: { beginAtZero: true }
      }
    }
  });
}