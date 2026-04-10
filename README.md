# Local Server Monitor Dashboard

A full-stack system monitoring app built entirely offline across 4 phases. Each phase uses a different language, mirroring the kind of multi-language data pipeline work done in industrial/embedded monitoring systems.

**Architecture:**
```
C++ binary → Python collector → InfluxDB → Grafana dashboard
                                              + Node.js API
                                              + Java alert CLI
```

**The Stack:**
- **InfluxDB** — Time-series database. Stores metrics with timestamps. Uses *measurements* (like tables), *tags* (indexed metadata), and *fields* (values).
- **Grafana** — Visualization layer. Connects to InfluxDB to render dashboards with graphs, gauges, and alerts.
- **Flux / InfluxQL** — Query languages for InfluxDB. Flux is newer and more powerful; similar to SQL but optimized for time-series.
- **Docker** — Runs InfluxDB and Grafana locally with no manual installation.

---

## Prerequisites — Install Before You Start

### Docker (for InfluxDB & Grafana)
| Tool | Install |
|------|---------|
| Docker Engine | `sudo apt install docker.io` (Linux) · [docker.com/get-docker](https://www.docker.com/get-docker) (Mac/Windows) |
| Docker Compose | Usually bundled with Docker Desktop · `sudo apt install docker-compose-plugin` (Linux) |

### C++ (Phase 1)
| Tool | Install |
|------|---------|
| GCC / G++ | `sudo apt install build-essential` (Linux) · `brew install gcc` (Mac) |
| Make (optional) | Usually bundled with build-essential |

### Python (Phase 2)
| Package | Install |
|---------|---------|
| psutil | `pip install psutil` |
| influxdb-client | `pip install influxdb-client` |
| Standard library only | `subprocess`, `json`, `time` — no install needed |

### Node.js (Phase 3)
| Package | Install |
|---------|---------|
| Node.js runtime | [nodejs.org](https://nodejs.org) — LTS version |
| express | `npm install express` |
| @influxdata/influxdb-client | `npm install @influxdata/influxdb-client` |
| cors | `npm install cors` |

### Java (Phase 4)
| Tool | Install |
|------|---------|
| JDK 17+ | `sudo apt install default-jdk` · [adoptium.net](https://adoptium.net) |
| Standard library only | `java.net.http` — no Maven needed |

---

## Folder Structure

```
local-server-monitor/
├── docker-compose.yml          ← spins up InfluxDB + Grafana
├── phase1-cpp/
│   ├── metrics.cpp
│   └── Makefile (optional)
├── phase2-python/
│   └── collector.py
├── phase3-node/
│   ├── server.js
│   └── package.json
├── phase4-java/
│   └── AlertChecker.java
└── README.md
```

---

## Phase 0 — Docker: Start InfluxDB & Grafana

**Goal:** Spin up InfluxDB (time-series store) and Grafana (dashboard) as local Docker containers.

### Step 1: Create `docker-compose.yml`
```yaml
version: '3.8'
services:
  influxdb:
    image: influxdb:2.7
    ports:
      - "8086:8086"
    environment:
      DOCKER_INFLUXDB_INIT_MODE: setup
      DOCKER_INFLUXDB_INIT_USERNAME: admin
      DOCKER_INFLUXDB_INIT_PASSWORD: adminpass
      DOCKER_INFLUXDB_INIT_ORG: localmonitor
      DOCKER_INFLUXDB_INIT_BUCKET: metrics
      DOCKER_INFLUXDB_INIT_ADMIN_TOKEN: mytoken123

  grafana:
    image: grafana/grafana:10.4.0
    ports:
      - "3001:3000"
    depends_on:
      - influxdb
```

### Step 2: Start the containers
```bash
docker compose up -d
```

### Step 3: Verify
- InfluxDB UI: `http://localhost:8086` (login: admin / adminpass)
- Grafana UI: `http://localhost:3001` (login: admin / admin)

### Key concepts practiced
- Docker Compose for multi-service local environments
- Port mapping and service dependencies
- Environment-based configuration

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

**Goal:** Call the C++ binary repeatedly, parse its output, and write readings to InfluxDB every 5 seconds.

### Step 1: Create the file
```
phase2-python/collector.py
```

### Step 2: Set up the InfluxDB client
```python
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

client = InfluxDBClient(url="http://localhost:8086", token="mytoken123", org="localmonitor")
write_api = client.write_api(write_options=SYNCHRONOUS)
```

### Step 3: Call the C++ binary
```python
import subprocess, json

result = subprocess.run(["../phase1-cpp/metrics"], capture_output=True, text=True)
data = json.loads(result.stdout)
```

### Step 4: Write a data point to InfluxDB
```python
point = (
    Point("system_metrics")
    .tag("host", "localhost")
    .field("cpu_percent", data["cpu_percent"])
    .field("mem_used_mb", data["mem_used_mb"])
    .field("mem_percent", data["mem_percent"])
)
write_api.write(bucket="metrics", record=point)
```

### Step 5: Loop every 5 seconds
```python
import time

while True:
    # ... call binary, parse, write point ...
    print(f"Recorded: CPU {data['cpu_percent']}% | MEM {data['mem_percent']}%")
    time.sleep(5)
```

### Step 6: Run and verify
```bash
python collector.py
# Check data landed: open http://localhost:8086, go to Data Explorer, query the "metrics" bucket
```

### Key concepts practiced
- `subprocess.run` and capturing stdout
- JSON parsing with `json.loads`
- Writing time-series data points with tags and fields
- Infinite loops with `time.sleep`

---

## Phase 3 — Node.js REST API

**Goal:** Query InfluxDB and serve metrics over HTTP so external tools can consume them.

### Step 1: Initialise the project
```bash
cd phase3-node
npm init -y
npm install express @influxdata/influxdb-client cors
```

### Step 2: Create `server.js` and connect to InfluxDB
```javascript
const express = require('express');
const { InfluxDB } = require('@influxdata/influxdb-client');
const cors = require('cors');

const app = express();
app.use(cors());

const influx = new InfluxDB({ url: 'http://localhost:8086', token: 'mytoken123' });
const queryApi = influx.getQueryApi('localmonitor');
```

### Step 3: Add the `/api/metrics` endpoint using Flux
```javascript
app.get('/api/metrics', async (req, res) => {
    const minutes = parseInt(req.query.minutes) || 10;
    const fluxQuery = `
        from(bucket: "metrics")
          |> range(start: -${minutes}m)
          |> filter(fn: (r) => r._measurement == "system_metrics")
          |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
    `;

    const rows = [];
    await queryApi.collectRows(fluxQuery, (row, tableMeta) => {
        rows.push(tableMeta.toObject(row));
    });
    res.json(rows);
});
```

### Step 4: Add the `/api/metrics/summary` endpoint
```javascript
app.get('/api/metrics/summary', async (req, res) => {
    const minutes = parseInt(req.query.minutes) || 10;
    const fluxQuery = `
        from(bucket: "metrics")
          |> range(start: -${minutes}m)
          |> filter(fn: (r) => r._measurement == "system_metrics")
          |> filter(fn: (r) => r._field == "cpu_percent" or r._field == "mem_percent")
          |> group(columns: ["_field"])
          |> mean()
    `;

    const rows = [];
    await queryApi.collectRows(fluxQuery, (row, tableMeta) => {
        rows.push(tableMeta.toObject(row));
    });
    res.json(rows);
});
```

### Step 5: Start the server
```javascript
app.listen(3000, () => console.log('API running on http://localhost:3000'));
```

### Step 6: Test it
```bash
node server.js
curl http://localhost:3000/api/metrics
curl http://localhost:3000/api/metrics/summary
```

### Key concepts practiced
- Flux query language for time-series data
- Async/await with InfluxDB client
- Express routing and query parameters
- CORS headers for cross-origin requests

---

## Phase 4A — Grafana Dashboard

**Goal:** Connect Grafana to InfluxDB and build a live dashboard with charts and gauges — no code needed.

### Step 1: Add InfluxDB as a data source
1. Open Grafana at `http://localhost:3001`
2. Go to **Connections → Data Sources → Add data source**
3. Select **InfluxDB**
4. Set:
   - Query Language: **Flux**
   - URL: `http://influxdb:8086`
   - Organisation: `localmonitor`
   - Token: `mytoken123`
   - Default bucket: `metrics`
5. Click **Save & Test**

### Step 2: Create a new dashboard
1. Go to **Dashboards → New → New Dashboard**
2. Click **Add visualization**

### Step 3: Add a CPU time-series panel
Write this Flux query in the panel editor:
```flux
from(bucket: "metrics")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "system_metrics")
  |> filter(fn: (r) => r._field == "cpu_percent")
```
Set visualization type to **Time series**, title to `CPU Usage %`.

### Step 4: Add a memory gauge panel
```flux
from(bucket: "metrics")
  |> range(start: -1m)
  |> filter(fn: (r) => r._measurement == "system_metrics")
  |> filter(fn: (r) => r._field == "mem_percent")
  |> last()
```
Set visualization type to **Gauge**, set thresholds at 70 (yellow) and 85 (red).

### Step 5: Set auto-refresh
In the dashboard settings, set the refresh interval to **5s** so it updates live.

### Key concepts practiced
- Connecting Grafana to a time-series data source
- Writing Flux queries in the panel editor
- Gauge vs. time-series visualization types
- Dashboard variables and time range controls

---

## Phase 4B — Java Alert CLI

**Goal:** A command-line tool that calls your Node.js API, checks thresholds, and prints warnings.

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

**Step 1 — Start InfluxDB & Grafana**
```bash
docker compose up -d
```

**Step 2 — Start the collector**
```bash
cd phase2-python
python collector.py
```

**Step 3 — Start the API**
```bash
cd phase3-node
node server.js
```

**Step 4 — View the dashboard**
Open Grafana at `http://localhost:3001` — data should appear within 10 seconds.

**Step 5 — Run the alert checker (optional)**
```bash
cd phase4-java
java AlertChecker
```

---

## What You Can Say in the Interview

> "Over the weekend I built a small version of what your team works on — a multi-language metrics pipeline. A C++ binary reads system stats and prints JSON, Python polls it every 5 seconds and writes to InfluxDB, and Grafana visualises the live data with time-series charts and gauges. I also built a Node.js API that queries InfluxDB using Flux, and a Java CLI alert tool on top of it. The whole thing runs offline with Docker."
