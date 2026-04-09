# Local Server Monitor Dashboard

A full-stack system monitoring app built entirely offline across 4 phases. Each phase uses a different language, mirroring the kind of multi-language data pipeline work done in industrial/embedded monitoring systems.

**Architecture:**
```
C++ binary → Python collector → Node.js API → HTML Dashboard
               (SQLite store)                    + Java alert CLI
```

---

## Prerequisites — Install Before You Start

### C++ (Phase 1)
| Tool | Install |
|------|---------|
| GCC / G++ | `sudo apt install build-essential` (Linux) · `brew install gcc` (Mac) · [MinGW-w64](https://www.mingw-w64.org/) (Windows) |
| Make (optional) | Usually bundled with build-essential |

### Python (Phase 2)
| Package | Install |
|---------|---------|
| psutil | `pip install psutil` |
| Standard library only | `sqlite3`, `subprocess`, `json`, `time` — no install needed |

### Node.js (Phase 3)
| Package | Install |
|---------|---------|
| Node.js runtime | [nodejs.org](https://nodejs.org) — LTS version |
| express | `npm install express` |
| better-sqlite3 | `npm install better-sqlite3` |
| cors | `npm install cors` |

### HTML Dashboard (Phase 4)
| Library | How to get |
|---------|------------|
| Chart.js | Download from [cdnjs.cloudflare.com](https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.min.js) and save locally as `chart.min.js` |

### Java (Phase 4)
| Tool | Install |
|------|---------|
| JDK 17+ | [adoptium.net](https://adoptium.net) · or `sudo apt install default-jdk` |
| Standard library only | `java.net.http`, `com.google.gson` or manual JSON parsing — no Maven needed |

---

## Folder Structure

```
local-server-monitor/
├── phase1-cpp/
│   ├── metrics.cpp
│   └── Makefile (optional)
├── phase2-python/
│   ├── collector.py
│   └── metrics.db          ← generated at runtime
├── phase3-node/
│   ├── server.js
│   └── package.json
├── phase4-dashboard/
│   ├── index.html
│   ├── chart.min.js        ← downloaded offline
│   └── AlertChecker.java
└── README.md
```

---

## Phase 1 — C++ Metrics Binary

**Goal:** Write a program that reads CPU and memory usage from the OS and prints it as a JSON string to stdout.

### Step 1: Create the file
```
phase1-cpp/metrics.cpp
```

### Step 2: Implement CPU reading
- **Linux:** Open and parse `/proc/stat` — read the first line, compute `(total - idle) / total * 100`
- **Mac:** Use `sysctl` with `host_cpu_load_info`
- **Windows:** Use `GetSystemTimes()` from `<windows.h>`

### Step 3: Implement memory reading
- **Linux:** Open and parse `/proc/meminfo` — read `MemTotal`, `MemAvailable`, compute used %
- **Mac:** Use `sysctl hw.memsize` and `vm_stat`
- **Windows:** Use `GlobalMemoryStatusEx()` from `<windows.h>`

### Step 4: Output JSON to stdout
Format your output exactly like this so Python can parse it:
```json
{"cpu_percent": 34.5, "mem_total_mb": 16384, "mem_used_mb": 9200, "mem_percent": 56.2}
```

### Step 5: Compile and test
```bash
g++ -o metrics metrics.cpp
./metrics
# Should print a single JSON line and exit
```

### Key concepts practiced
- File I/O (`ifstream`, reading `/proc` files)
- Structs and pointers
- String parsing (`std::stod`, `std::stoi`)
- `std::cout` for stdout output

---

## Phase 2 — Python Data Collector

**Goal:** Call the C++ binary repeatedly, parse its output, and store readings in SQLite every 5 seconds.

### Step 1: Create the file
```
phase2-python/collector.py
```

### Step 2: Set up SQLite
```python
import sqlite3

conn = sqlite3.connect("metrics.db")
conn.execute("""
    CREATE TABLE IF NOT EXISTS metrics (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        cpu_percent REAL,
        mem_total_mb INTEGER,
        mem_used_mb INTEGER,
        mem_percent REAL
    )
""")
conn.commit()
```

### Step 3: Call the C++ binary
```python
import subprocess, json

result = subprocess.run(["../phase1-cpp/metrics"], capture_output=True, text=True)
data = json.loads(result.stdout)
```

### Step 4: Insert into the database
```python
conn.execute(
    "INSERT INTO metrics (cpu_percent, mem_total_mb, mem_used_mb, mem_percent) VALUES (?, ?, ?, ?)",
    (data["cpu_percent"], data["mem_total_mb"], data["mem_used_mb"], data["mem_percent"])
)
conn.commit()
```

### Step 5: Loop every 5 seconds
```python
import time

while True:
    # ... call binary, parse, insert ...
    print(f"Recorded: CPU {data['cpu_percent']}% | MEM {data['mem_percent']}%")
    time.sleep(5)
```

### Step 6: Run and verify
```bash
python collector.py
# Should print a reading every 5 seconds
# Check the DB: sqlite3 metrics.db "SELECT * FROM metrics LIMIT 5;"
```

### Key concepts practiced
- `subprocess.run` and capturing stdout
- JSON parsing with `json.loads`
- SQLite with parameterised queries (prevents SQL injection)
- Infinite loops with `time.sleep`

---

## Phase 3 — Node.js REST API

**Goal:** Serve the SQLite data over HTTP so the dashboard and Java tool can consume it.

### Step 1: Initialise the project
```bash
cd phase3-node
npm init -y
npm install express better-sqlite3 cors
```

### Step 2: Create `server.js`

```javascript
const express = require('express');
const Database = require('better-sqlite3');
const cors = require('cors');

const app = express();
const db = new Database('../phase2-python/metrics.db', { readonly: true });

app.use(cors());
```

### Step 3: Add the `/api/metrics` endpoint
Returns the last N minutes of readings (default 10).
```javascript
app.get('/api/metrics', (req, res) => {
    const minutes = parseInt(req.query.minutes) || 10;
    const rows = db.prepare(`
        SELECT * FROM metrics
        WHERE timestamp >= datetime('now', '-' || ? || ' minutes')
        ORDER BY timestamp ASC
    `).all(minutes);
    res.json(rows);
});
```

### Step 4: Add the `/api/metrics/summary` endpoint
Returns averages for the last N minutes.
```javascript
app.get('/api/metrics/summary', (req, res) => {
    const minutes = parseInt(req.query.minutes) || 10;
    const row = db.prepare(`
        SELECT
            ROUND(AVG(cpu_percent), 2) AS avg_cpu,
            ROUND(AVG(mem_percent), 2) AS avg_mem,
            ROUND(MAX(cpu_percent), 2) AS max_cpu,
            COUNT(*) AS sample_count
        FROM metrics
        WHERE timestamp >= datetime('now', '-' || ? || ' minutes')
    `).get(minutes);
    res.json(row);
});
```

### Step 5: Start the server
```javascript
app.listen(3000, () => console.log('API running on http://localhost:3000'));
```

### Step 6: Test it
```bash
node server.js
# In another terminal:
curl http://localhost:3000/api/metrics
curl http://localhost:3000/api/metrics/summary
```

### Key concepts practiced
- Express routing and query parameters
- Synchronous SQLite with `better-sqlite3`
- CORS headers for cross-origin browser requests
- JSON responses

---

## Phase 4A — HTML Dashboard

**Goal:** A single HTML page that fetches from your API and shows live-updating line charts.

### Step 1: Download Chart.js for offline use
```bash
# In phase4-dashboard/
curl -o chart.min.js https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.min.js
```

### Step 2: Create `index.html` structure
```html
<!DOCTYPE html>
<html>
<head>
  <title>Server Monitor</title>
  <script src="chart.min.js"></script>
</head>
<body>
  <h1>Server Monitor</h1>
  <canvas id="cpuChart" width="800" height="200"></canvas>
  <canvas id="memChart" width="800" height="200"></canvas>
  <script src="dashboard.js"></script>
</body>
</html>
```

### Step 3: Fetch data from the API
```javascript
async function fetchMetrics() {
    const res = await fetch('http://localhost:3000/api/metrics?minutes=5');
    return await res.json();
}
```

### Step 4: Render charts with Chart.js
```javascript
const data = await fetchMetrics();
const labels = data.map(r => r.timestamp);
const cpuData = data.map(r => r.cpu_percent);

new Chart(document.getElementById('cpuChart'), {
    type: 'line',
    data: { labels, datasets: [{ label: 'CPU %', data: cpuData }] }
});
```

### Step 5: Auto-refresh every 10 seconds
```javascript
setInterval(async () => {
    // re-fetch and update chart.data + chart.update()
}, 10000);
```

### Step 6: Open in browser
```bash
# Just open index.html directly — no server needed for the dashboard itself
```

### Key concepts practiced
- `fetch` and async/await
- Chart.js datasets and `.update()`
- `setInterval` for polling

---

## Phase 4B — Java Alert CLI

**Goal:** A command-line tool that calls your API, checks thresholds, and prints warnings.

### Step 1: Create `AlertChecker.java`

### Step 2: Define a data class
```java
class MetricSummary {
    double avg_cpu;
    double avg_mem;
    double max_cpu;
    int sample_count;
}
```

### Step 3: Call the API using `java.net.http`
```java
import java.net.http.*;
import java.net.URI;

HttpClient client = HttpClient.newHttpClient();
HttpRequest request = HttpRequest.newBuilder()
    .uri(URI.create("http://localhost:3000/api/metrics/summary?minutes=5"))
    .build();

HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
String json = response.body();
```

### Step 4: Parse the JSON manually (no dependency needed)
```java
// Simple approach — extract values with String methods or regex
// Or add Gson: download gson-2.10.1.jar and compile with -cp gson.jar
```

### Step 5: Check thresholds and print alerts
```java
double CPU_THRESHOLD = 80.0;
double MEM_THRESHOLD = 85.0;

if (summary.avg_cpu > CPU_THRESHOLD) {
    System.out.println("ALERT: High CPU usage — " + summary.avg_cpu + "%");
}
if (summary.avg_mem > MEM_THRESHOLD) {
    System.out.println("ALERT: High memory usage — " + summary.avg_mem + "%");
}
```

### Step 6: Compile and run
```bash
javac AlertChecker.java
java AlertChecker
```

### Key concepts practiced
- Java OOP (classes, fields, methods)
- `java.net.http.HttpClient` (modern Java HTTP)
- Interfaces (define a `Checker` interface, implement it)
- Conditional logic and thresholds

---

## Running Everything Together

Open 3 terminals:

**Terminal 1 — Start the collector**
```bash
cd phase2-python
python collector.py
```

**Terminal 2 — Start the API**
```bash
cd phase3-node
node server.js
```

**Terminal 3 — Run the alert checker (optional)**
```bash
cd phase4-dashboard
java AlertChecker
```

Then open `phase4-dashboard/index.html` in your browser.

---

## What You Can Say in the Interview

> "Over the weekend I built a small version of what your team works on — a multi-language metrics pipeline. A C++ binary reads system stats and prints JSON, Python polls it every 5 seconds and stores readings in SQLite, a Node.js API serves the data, and a live HTML dashboard charts it. I also wrote a Java CLI alert tool on top of the same API. The whole thing runs offline."
