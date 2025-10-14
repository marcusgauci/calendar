// ========================
// Calendar with autosave, autosizing textareas,
// char counter (remaining), and WebSocket to ESP32
// ========================

// DOM Elements
const monthYearDisplay = document.getElementById('month-year');
const calendarGrid = document.getElementById('calendar-grid');
const prevMonthBtn = document.getElementById('prev-month');
const nextMonthBtn = document.getElementById('next-month');
const homeBtn = document.getElementById('home-btn');

// Unsorted priority queue for entries
let entryQueue = [];

// Max chars per note
const MAX_CHARS = 30;

// WebSocket setup
let ws;

function connectWebSocket() {
  ws = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);

  ws.onopen = () => {
    console.log('WebSocket connected to ESP32');
  };

  ws.onclose = (e) => {
    console.log('WebSocket closed. Reconnecting in 3 seconds...', e.reason);
    setTimeout(connectWebSocket, 3000);
  };

  ws.onerror = (err) => {
    console.error('WebSocket error:', err.message);
    ws.close();
  };

  ws.onmessage = (message) => {
    console.log('Message from ESP32:', message.data);
  };
}

try {
connectWebSocket();
} catch (error) {
  console.error(error);
  // Expected output: ReferenceError: nonExistentFunction is not defined
  // (Note: the exact output may be browser-dependent)
}

// Load entries from localStorage
function loadEntriesFromStorage() {
  const saved = localStorage.getItem('calendarNotes');
  if (saved) {
    entryQueue = JSON.parse(saved);
  }
}


// Save entries to localStorage
function saveEntriesToStorage() {
  localStorage.setItem('calendarNotes', JSON.stringify(entryQueue));
}

// Send entry data to ESP32 via WebSocket
function sendEntryToESP32(entry) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    // Send plain text format: "date: note"
    const message = `${entry.date}: ${entry.text}`;
    ws.send(message);
  } else {
    console.warn('WebSocket not connected, cannot send entry');
  }
}

function fetchNotesFromESP32() {
  fetch('http://potplant.local/notes.txt')  // <-- Replace with your ESP32's web address
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.text();
    })
    .then(text => {
      console.log('Fetched notes:', text);

      // Parse each line into entries
      const lines = text.split('\n').filter(line => line.trim().length > 0);

      entryQueue = [];  // clear current entries to avoid duplicates

      lines.forEach(line => {
        const sepIndex = line.indexOf(':');
        if (sepIndex !== -1) {
          const date = line.substring(0, sepIndex).trim();
          const text = line.substring(sepIndex + 1).trim();

          entryQueue.push({
            date,
            text,
            priority: Math.floor(Math.random() * 100)
          });
        }
      });

      saveEntriesToStorage();
      renderCalendar(currentDate);
    })
    .catch(err => {
      console.error('Failed to fetch notes:', err);
    });
}

async function updateClock() {
      const res = await fetch('/time');
      const data = await res.text();
      document.getElementById('clock').textContent = data;
}


// Autosize textarea height based on content
function autoResize(el) {
  el.style.height = 'auto'; // reset height
  el.style.height = (el.scrollHeight + 2) + 'px'; // add small padding
}

// Render the calendar for a given month
function renderCalendar(date) {
  const year = date.getFullYear();
  const month = date.getMonth();
  const firstDay = new Date(year, month, 1).getDay();
  const daysInMonth = new Date(year, month + 1, 0).getDate();

  const monthNames = [
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  ];

  monthYearDisplay.textContent = `${monthNames[month]} ${year}`;
  calendarGrid.innerHTML = '';

  // Add empty cells for days before first of month
  for (let i = 0; i < firstDay; i++) {
    calendarGrid.innerHTML += `<div></div>`;
  }

  // Render each day cell
  for (let day = 1; day <= daysInMonth; day++) {
    const fullDate = `${year}-${String(month + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`;

    const cell = document.createElement('div');

    // Day number label
    const label = document.createElement('div');
    label.textContent = day;

    // Create textarea for notes
    const textarea = document.createElement('textarea');
    textarea.placeholder = '';
    textarea.dataset.date = fullDate;
    textarea.rows = 2;
    textarea.maxLength = MAX_CHARS;

    // Load existing note for this date
    const existing = entryQueue.find(entry => entry.date === fullDate);
    if (existing) {
      textarea.value = existing.text;
      autoResize(textarea);
    }

    // Create character counter element
    const counter = document.createElement('small');
    counter.classList.add('char-counter');

    function updateCounter() {
      const current = textarea.value.length;
      const remaining = MAX_CHARS - current;
      counter.textContent = `${remaining}`;
    }

    updateCounter();

    // On textarea input event
    textarea.addEventListener('input', (e) => {
      const text = e.target.value.trim();

      autoResize(textarea);
      updateCounter();

      const existingIndex = entryQueue.findIndex(entry => entry.date === fullDate);

      if (existingIndex !== -1) {
        if (text === "") {
          entryQueue.splice(existingIndex, 1);
        } else {
          entryQueue[existingIndex].text = text;
        }
      } else if (text !== "") {
        const priority = Math.floor(Math.random() * 100);
        entryQueue.push({ date: fullDate, text, priority });
      }

      saveEntriesToStorage();

      // Send updated entry to ESP32 via WebSocket
      if (text !== "") {
        sendEntryToESP32({ date: fullDate, text, priority: entryQueue.find(e => e.date === fullDate)?.priority });
      }
    });

    let today = new Date();

    if (
      day === today.getDate() &&
      year === today.getFullYear() &&
      month === today.getMonth()
    ) {
      cell.classList.add('current-date');
    }

    // Assemble the cell
    cell.appendChild(label);
    cell.appendChild(textarea);
    cell.appendChild(counter);
    calendarGrid.appendChild(cell);
  }
}

// Navigation buttons
prevMonthBtn.addEventListener('click', () => {
  currentDate.setMonth(currentDate.getMonth() - 1);
  renderCalendar(currentDate);
});
nextMonthBtn.addEventListener('click', () => {
  currentDate.setMonth(currentDate.getMonth() + 1);
  renderCalendar(currentDate);
});

homeBtn.addEventListener("click", function() {
  // Example: redirect to another page
  window.location.href = "http://potplant.local";
});


// Initialize

setInterval(updateClock, 1000);
let currentDate = new Date();
loadEntriesFromStorage();
renderCalendar(currentDate);

fetchNotesFromESP32();