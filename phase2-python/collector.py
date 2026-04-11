import subprocess
import json
import time
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

METRICS_BINARY = "../phase1-cpp/metrics"

INFLUX_URL    = "http://localhost:8086"
INFLUX_TOKEN  = "mytoken123"
INFLUX_ORG    = "localmonitor"
INFLUX_BUCKET = "metrics"

def main():
    print("Starting metrics collector...")

    while True:
        results = subprocess.run(["../LocalMonitor/phase1-cpp/metrics"], capture_output=True, text=True)
        data = json.loads(results.stdout)
        
        # Write metrics to InfluxDB
        with InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG) as client:
            write_api = client.write_api(write_options=SYNCHRONOUS)
            point = Point("system_metrics")
            point.field("cpu_percent", float(data["cpu_percent"]))
            point.field("mem_percent", float(data["mem_percent"]))
            point.field("mem_used_kb", float(data["mem_used_kb"]))
            write_api.write(bucket=INFLUX_BUCKET, record=point)
            print("Metrics written to InfluxDB")
        
        # Log the collected metrics
        print("Additional metrics written to InfluxDB")
        print(f"Collected metrics: CPU {data['cpu_percent']}%, Memory {data['mem_percent']}%, Used Memory {data['mem_used_kb']} KB")
        
        # Sleep for 5 seconds before collecting the next sample
        time.sleep(5)

if __name__ == "__main__":
    main()