# ESPHome OpenTherm Gateway - Debug & Monitoring Setup

Setup do długoterminowego debugowania i zbierania metryk z ESPHome OpenTherm Gateway.

## Stack

- **Promtail** - Syslog collector z parserem structured logów
- **Loki** - Log aggregation system (30 dni retencji)
- ~~Grafana~~ - Użyj swojej istniejącej instancji Grafany

## Wymagania

- Docker i Docker Compose
- Istniejąca instancja Grafany (lub uruchom swoją)
- ESPHome z skonfigurowanym syslog

## Szybki start

### 1. Uruchom stack

```bash
cd debug/
docker-compose up -d
```

Sprawdź logi:
```bash
docker-compose logs -f promtail
docker-compose logs -f loki
```

### 2. Skonfiguruj ESPHome

Dodaj do swojego pliku ESPHome YAML:

```yaml
logger:
  level: VERBOSE
  logs:
    opentherm.component: VERBOSE

# UWAGA: ESPHome nie ma natywnego wsparcia dla syslog!
# Użyj jednej z poniższych opcji:

# Opcja A: MQTT logging (zalecane)
mqtt:
  broker: 192.168.1.100
  log_topic: esphome/opentherm_gateway/logs
  log_topic_level: VERBOSE

# Następnie forward MQTT -> syslog używając mosquitto_sub:
# mosquitto_sub -h 192.168.1.100 -t 'esphome/opentherm_gateway/logs' | \
#   logger -t esphome -n localhost -P 514 -d

# Opcja B: Serial logging do pliku + rsyslog
# esphome logs opentherm_gateway.yaml 2>&1 | \
#   logger -t esphome -n localhost -P 514 -d
```

### 3. Dodaj Loki datasource do swojej Grafany

W swojej istniejącej Grafanie dodaj datasource:

**Configuration → Data Sources → Add data source → Loki**

- **URL**: `http://<DOCKER_HOST_IP>:3100`
- **Name**: `ESPHome Loki`

Lub użyj provisioning (skopiuj do swojej Grafany):

```yaml
# /etc/grafana/provisioning/datasources/loki.yml
apiVersion: 1

datasources:
  - name: ESPHome Loki
    type: loki
    access: proxy
    url: http://192.168.1.100:3100  # Zmień na swój IP
    isDefault: false
    editable: true
```

## Przykładowe zapytania LogQL

### Podstawowe filtrowanie

```logql
# Wszystkie logi OpenTherm
{job="esphome-opentherm"} |= "OT_PKT"

# Tylko pakiety Master->Slave (termostat->boiler)
{job="esphome-opentherm", direction="MasterToSlave"}

# Tylko pakiety Gateway->Boiler (aktywne zapytania)
{job="esphome-opentherm", direction="GatewayToBoiler"}

# Wszystkie akcje Home Assistant
{job="esphome-opentherm"} |= "HA_CMD"

# Cache operations
{job="esphome-opentherm"} |= "OT_CACHE"
```

### Debugging specyficznych problemów

```logql
# Problem: Krzywa grzewcza zjeżdża do -30°C
# Znajdź wszystkie ujemne wartości TSet (msg_id=1)
{job="esphome-opentherm", msg_id="1"}
  |= "parsed=-"

# Sprawdź context wokół anomalii (5min przed/po)
{job="esphome-opentherm", msg_id="1"}
  |= "parsed=-"
  | line_format "{{.timestamp}} {{.message}}"

# Wszystkie invalid responses
{job="esphome-opentherm", valid="0"}

# Rate limited requests (cache spam)
{job="esphome-opentherm"}
  |= "rate_limited=1"
```

### Metryki i statystyki

```logql
# Rate pakietów OpenTherm (pakiety/sekundę)
rate({job="esphome-opentherm", log_type="OT_PKT"}[5m])

# Liczba pakietów według kierunku
sum by (direction) (
  count_over_time({job="esphome-opentherm", log_type="OT_PKT"}[1h])
)

# Cache hit ratio (%)
sum(count_over_time({job="esphome-opentherm", cache_hit="1"}[1h]))
  /
sum(count_over_time({job="esphome-opentherm"} |= "OT_CACHE"[1h]))
  * 100

# Top 10 najczęściej używanych msg_id
topk(10,
  sum by (msg_id) (
    count_over_time({job="esphome-opentherm", log_type="OT_PKT"}[1h])
  )
)

# Invalid responses per minute
sum(rate({job="esphome-opentherm", valid="0"}[1m]))

# Temperatura w czasie (heatmap)
quantile_over_time(0.5,
  {job="esphome-opentherm", msg_id="1"}
    | regexp "parsed=(?P<temp>-?[\\d.]+)"
    | unwrap temp [5m]
)
```

### Analiza historyczna

```logql
# Historia TSet (krzywa grzewcza) za ostatnie 24h
{job="esphome-opentherm", msg_id="1", direction="MasterToSlave"}
  | regexp "parsed=(?P<tset>-?[\\d.]+)"
  | line_format "{{.tset}}"

# Wszystkie set_temperature commands z HA
{job="esphome-opentherm", ha_action="set_temperature"}
  | logfmt

# Weryfikacje setpoint (success vs fail)
{job="esphome-opentherm", ha_action=~"verify_setpoint|verify_failed"}
```

## Przykładowy Dashboard Grafana

### Panel 1: OpenTherm Packet Rate

**Query:**
```logql
sum by (direction) (
  rate({job="esphome-opentherm", log_type="OT_PKT"}[5m])
)
```

**Visualization:** Time series
**Legend:** `{{direction}}`

### Panel 2: Temperatura (TSet) w czasie

**Query:**
```logql
avg_over_time(
  {job="esphome-opentherm", msg_id="1"}
    | regexp "parsed=(?P<temp>-?[\\d.]+)"
    | unwrap temp [5m]
)
```

**Visualization:** Time series
**Y-axis:** Temperature (°C)

### Panel 3: Cache Hit Ratio

**Query:**
```logql
(
  sum(rate({job="esphome-opentherm", cache_hit="1"}[5m]))
    /
  sum(rate({job="esphome-opentherm"} |= "OT_CACHE"[5m]))
) * 100
```

**Visualization:** Stat
**Unit:** percent (0-100)

### Panel 4: Logi (tabela)

**Query:**
```logql
{job="esphome-opentherm"}
  | logfmt
```

**Visualization:** Logs
**Options:**
- Enable unique labels
- Show time
- Wrap lines

## Eksport danych do analizy

### Zapisz logi do pliku

```bash
# Użyj LogCLI (narzędzie Loki)
docker run -it --rm --network=host grafana/logcli:latest \
  --addr=http://localhost:3100 \
  query '{job="esphome-opentherm"}' \
  --from="2024-01-01T00:00:00Z" \
  --to="2024-01-02T00:00:00Z" \
  --limit=100000 \
  > logs.txt
```

### Python script do analizy

```python
import requests
import pandas as pd
from datetime import datetime, timedelta

LOKI_URL = "http://localhost:3100"

def query_loki(query, start, end):
    params = {
        'query': query,
        'start': int(start.timestamp() * 1e9),
        'end': int(end.timestamp() * 1e9),
        'limit': 5000
    }
    response = requests.get(f"{LOKI_URL}/loki/api/v1/query_range", params=params)
    return response.json()

# Przykład: wyciągnij wszystkie TSet values
end = datetime.now()
start = end - timedelta(days=7)

query = '{job="esphome-opentherm", msg_id="1"} | regexp "parsed=(?P<temp>-?[\\\\d.]+)"'
data = query_loki(query, start, end)

# Przekształć do DataFrame
rows = []
for stream in data['data']['result']:
    for value in stream['values']:
        timestamp, log_line = value
        # Parse temp z log line
        import re
        match = re.search(r'parsed=([-\d.]+)', log_line)
        if match:
            rows.append({
                'timestamp': pd.to_datetime(int(timestamp) // 1000000, unit='ms'),
                'temperature': float(match.group(1))
            })

df = pd.DataFrame(rows)
print(df.describe())

# Znajdź anomalie (< -20°C lub > 100°C)
anomalies = df[(df['temperature'] < -20) | (df['temperature'] > 100)]
print("\nAnomalies:")
print(anomalies)
```

## Monitoring i alerting

Loki może wysyłać alerty do Alertmanager. Przykładowa reguła:

```yaml
# /loki/rules/fake/rules.yml
groups:
  - name: esphome_alerts
    interval: 1m
    rules:
      - alert: HighInvalidResponseRate
        expr: |
          sum(rate({job="esphome-opentherm", valid="0"}[5m])) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High rate of invalid OpenTherm responses"
          description: "Invalid response rate is {{ $value }} per second"

      - alert: AnomalousTemperature
        expr: |
          quantile_over_time(0.5,
            {job="esphome-opentherm", msg_id="1"}
              | regexp "parsed=(?P<temp>-?[\\d.]+)"
              | unwrap temp [5m]
          ) < -20 or > 100
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Anomalous temperature reading detected"
          description: "Temperature is {{ $value }}°C"
```

## Troubleshooting

### Brak logów w Loki

1. Sprawdź czy Promtail odbiera syslog:
   ```bash
   docker-compose logs promtail | grep -i "listening"
   ```

2. Sprawdź czy ESPHome wysyła logi:
   ```bash
   # Testuj manualnie
   echo "<14>Test log from ESPHome" | nc -u localhost 514
   ```

3. Sprawdź Loki health:
   ```bash
   curl http://localhost:3100/ready
   ```

### Za dużo danych (dysk się zapełnia)

Zmień retencję w `loki/loki-config.yml`:
```yaml
limits_config:
  retention_period: 168h  # 7 dni zamiast 30
```

Restart:
```bash
docker-compose restart loki
```

### Promtail parsuje źle logi

Testuj regex lokalnie:
```bash
echo '[V][opentherm.component:383]: [OT_PKT] dir=MasterToSlave type=intercept msg_id=1' | \
  grep -oP '\[(?P<log_type>[A-Z_]+)\]'
```

## Porty

- **514/UDP** - Syslog (Promtail)
- **3100/TCP** - Loki API
- **9080/TCP** - Promtail metrics (Prometheus)

## Zasoby

- Loki data: `~/.docker/volumes/debug_loki-data`
- Logs retention: 30 days (configurable)
- Estimated storage: ~100MB/day dla VERBOSE logging

## Przydatne linki

- [LogQL documentation](https://grafana.com/docs/loki/latest/logql/)
- [Promtail pipelines](https://grafana.com/docs/loki/latest/clients/promtail/pipelines/)
- [Grafana Loki dashboard templates](https://grafana.com/grafana/dashboards/)
