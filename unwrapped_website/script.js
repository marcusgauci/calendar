const script = document.createElement('script');
script.src = "https://cdn.jsdelivr.net/npm/chart.js";
document.head.appendChild(script);

const demoData = `
12
7
19
5
14
9
`;

script.onload = function() {
  fetch('demo-data.txt')
    .then(response => response.text())
    .then(text => {})
      const lines = demoData.split('\n').map(line => parseFloat(line.trim())).filter(n => !isNaN(n));
      renderChart('chartCanvas1', lines, 'Line Chart', 'line');
      renderChart('chartCanvas2', lines, 'Line Chart', 'line');
      renderChart('chartCanvas3', lines, 'Line Chart', 'line');
    
};


function renderChart(canvasId, data, label, type = 'bar') {
  const ctx = document.getElementById(canvasId).getContext('2d');
  if (!window.myCharts) window.myCharts = {};
  if (window.myCharts[canvasId]) window.myCharts[canvasId].destroy();
  window.myCharts[canvasId] = new Chart(ctx, {
    type: type,
    data: {
      labels: data.map((_, i) => `Value ${i + 1}`),
      datasets: [{
        label: label,
        data: data,
        backgroundColor: [
          'rgba(54, 162, 235, 0.6)',
          'rgba(255, 99, 132, 0.6)',
          'rgba(255, 206, 86, 0.6)',
          'rgba(75, 192, 192, 0.6)',
          'rgba(153, 102, 255, 0.6)',
          'rgba(255, 159, 64, 0.6)'
        ]
      }]
    },
    options: {
      responsive: true,
      scales: type === 'pie' ? {} : { y: { beginAtZero: true } }
    }
  });
}