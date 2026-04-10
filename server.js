// server.js
const express = require('express');
const { InfluxDB } = require('@influxdata/influxdb-client');
const cors = require('cors');

// Create Express app
const app = express();
app.use(cors());

// InfluxDB configuration
const INFLUX_URL = 'http://localhost:8086';
const INFLUX_TOKEN = 'mytoken123';
const INFLUX_ORG = 'localmonitor';
const INFLUX_BUCKET = 'my-bucket';

// Create InfluxDB client
const influx = new InfluxDB({ url: INFLUX_URL, token: INFLUX_TOKEN });
const queryApi = influx.getQueryApi(INFLUX_ORG);

// API endpoint to get metrics summary
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

// API endpoint to get aggregated metrics summary
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

// Start the server
const PORT = 3000;
app.listen(PORT, () => {
    console.log(`Server is running on port ${PORT}`);
});