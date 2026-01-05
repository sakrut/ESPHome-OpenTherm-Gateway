#!/bin/bash
# Quick start script for ESPHome OpenTherm Gateway debug stack

set -e

echo "🚀 Starting ESPHome OpenTherm Gateway debug stack..."

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "❌ Error: Docker is not running"
    exit 1
fi

# Check if .env exists, if not copy from example
if [ ! -f .env ]; then
    echo "📝 Creating .env from .env.example..."
    cp .env.example .env
fi

# Start stack
echo "🐳 Starting Docker containers..."
docker-compose up -d

# Wait for Loki to be ready
echo "⏳ Waiting for Loki to be ready..."
timeout=30
while ! curl -s http://localhost:3100/ready > /dev/null 2>&1; do
    sleep 1
    timeout=$((timeout - 1))
    if [ $timeout -eq 0 ]; then
        echo "❌ Error: Loki failed to start"
        docker-compose logs loki
        exit 1
    fi
done

echo "✅ Loki is ready!"

# Test syslog
echo "🧪 Testing syslog reception..."
echo "<14>Test message from ESPHome" | nc -u localhost 514

sleep 2

# Query test message
TEST_RESULT=$(curl -s "http://localhost:3100/loki/api/v1/query?query={job=\"esphome-opentherm\"}" | grep -c "Test message" || true)

if [ "$TEST_RESULT" -gt 0 ]; then
    echo "✅ Syslog reception working!"
else
    echo "⚠️  Warning: Test message not found in Loki. Check configuration."
fi

echo ""
echo "✨ Debug stack is running!"
echo ""
echo "📊 Services:"
echo "  - Grafana:         http://localhost:3001 (admin/admin)"
echo "  - Loki API:        http://localhost:3100"
echo "  - Promtail:        http://localhost:9080/metrics"
echo "  - Syslog UDP:      localhost:514"
echo ""
echo "📖 Next steps:"
echo "  1. Open Grafana at http://localhost:3001"
echo "     Login: admin/admin"
echo ""
echo "  2. Forward ESPHome logs to syslog:"
echo "     esphome logs opentherm.yaml 2>&1 | logger -t esphome -n localhost -P 514 -d"
echo ""
echo "  3. View logs in Grafana:"
echo "     Explore → ESPHome Loki → {job=\"esphome-opentherm\"}"
echo ""
echo "  4. Stop stack:"
echo "     docker-compose down"
echo ""
