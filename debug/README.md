# Debug & Monitoring Stack

Long-term debugging for ESPHome OpenTherm Gateway. Loki + Promtail for structured log analysis.

## Quick Start

```bash
cd debug/
./start.sh
```

**Add Loki datasource to your Grafana:**
- URL: `http://<DOCKER_HOST>:3100`
- Or copy: `grafana-provisioning/datasources/loki.yml` to your Grafana

## ESPHome Configuration

**MQTT logging (recommended):**
```yaml
mqtt:
  broker: 192.168.1.100
  log_topic: esphome/opentherm/logs
  log_topic_level: VERBOSE

logger:
  level: VERBOSE
  logs:
    opentherm.component: VERBOSE
```

**Forward MQTT to syslog:**
```bash
mosquitto_sub -h 192.168.1.100 -t 'esphome/opentherm/logs' | \
  logger -t esphome -n localhost -P 514 -d
```

**Alternative - Serial logging:**
```bash
esphome logs opentherm.yaml 2>&1 | \
  logger -t esphome -n localhost -P 514 -d
```

## Useful LogQL Queries

**Basic filtering:**
```logql
# All OpenTherm packets
{job="esphome-opentherm"} |= "OT_PKT"

# Master->Slave (thermostat->boiler)
{job="esphome-opentherm", direction="MasterToSlave"}

# Home Assistant actions
{job="esphome-opentherm"} |= "HA_CMD"
```

**Debugging heating curve (-30°C problem):**
```logql
# Find negative TSet values
{job="esphome-opentherm", msg_id="1"} |= "parsed=-"

# TSet history (24h)
{job="esphome-opentherm", msg_id="1", direction="MasterToSlave"}
  | regexp "parsed=(?P<temp>-?[\\d.]+)"
```

**Metrics:**
```logql
# Packets per second by direction
sum by (direction) (rate({job="esphome-opentherm", log_type="OT_PKT"}[5m]))

# Cache hit ratio (%)
(sum(rate({job="esphome-opentherm", cache_hit="1"}[5m]))
 / sum(rate({job="esphome-opentherm"} |= "OT_CACHE"[5m]))) * 100

# Invalid responses per minute
sum(rate({job="esphome-opentherm", valid="0"}[1m]))
```

## Exported Metrics

Promtail automatically extracts:
- `opentherm_packets_total` - Packet counter
- `cache_hits_total` / `cache_misses_total` - Cache stats
- `invalid_responses_total` - Failed requests
- `temperature_celsius` - Temperature gauge

## Configuration

**Ports:**
- 514/UDP - Syslog (Promtail)
- 3100/TCP - Loki API
- 9080/TCP - Promtail metrics

**Retention:** 30 days (configurable in `loki/loki-config.yml`)

**Storage:** ~100MB/day for VERBOSE logging

## Troubleshooting

**No logs in Loki:**
```bash
# Test syslog
echo "<14>Test log" | nc -u localhost 514

# Check Promtail
docker-compose logs promtail | grep listening

# Check Loki
curl http://localhost:3100/ready
```

**Disk full:**
```yaml
# In loki/loki-config.yml
limits_config:
  retention_period: 168h  # 7 days instead of 30
```

## Data Export

**Export to file:**
```bash
docker run -it --rm --network=host grafana/logcli:latest \
  --addr=http://localhost:3100 \
  query '{job="esphome-opentherm"}' \
  --from="2024-01-01T00:00:00Z" \
  --limit=100000 > logs.txt
```

**Python analysis:**
```python
import requests, pandas as pd
from datetime import datetime, timedelta

def query_loki(query, hours=24):
    end = datetime.now()
    start = end - timedelta(hours=hours)

    params = {
        'query': query,
        'start': int(start.timestamp() * 1e9),
        'end': int(end.timestamp() * 1e9),
        'limit': 5000
    }

    r = requests.get("http://localhost:3100/loki/api/v1/query_range", params=params)
    return r.json()

# Find TSet anomalies
data = query_loki('{job="esphome-opentherm", msg_id="1"}')

# Parse temperatures
temps = []
for stream in data['data']['result']:
    for ts, log in stream['values']:
        import re
        m = re.search(r'parsed=([-\d.]+)', log)
        if m:
            temp = float(m.group(1))
            if temp < -20 or temp > 100:
                print(f"Anomaly: {temp}°C at {ts}")
```

## Stack Components

- **Promtail** - Parses `[OT_PKT]`, `[HA_CMD]`, `[OT_CACHE]` structured logs
- **Loki** - Stores & indexes logs (30d retention, compressed)
- **Grafana** - Use your existing instance (see datasource config)
