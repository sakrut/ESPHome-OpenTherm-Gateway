# Development Guide

## Local Testing

For testing local changes **before** committing to GitHub:

### Option 1: Use test file (recommended)

```bash
# Copy test file (if it doesn't exist)
cp example_opentherm.yaml example_opentherm_temp.yaml

# This file uses local components and is in .gitignore
# Build locally
docker run --rm -v "$(pwd):/config" esphome/esphome compile example_opentherm_temp.yaml
```

### Option 2: Temporarily modify example_opentherm.yaml

In `example_opentherm.yaml`, comment out GitHub section and uncomment local:

```yaml
# External components setup
# external_components:
#   - source: github://sakrut/ESPHome-OpenTherm-Gateway
#     components: [ opentherm ]
#     refresh: 0d

# For local development/testing, use:
external_components:
  - source:
      type: local
      path: components
    components: [ opentherm ]
```

**IMPORTANT**: Revert this change before committing!

## Testing from Branch

To test changes from a specific branch (e.g., `localtests`) **before** merging to main:

```yaml
external_components:
  - source: github://sakrut/ESPHome-OpenTherm-Gateway@localtests
    components: [ opentherm ]
    refresh: 0d  # Use 'always' for latest changes
```

`refresh` parameter:
- `0d` - fetch only once (cache)
- `always` - always fetch latest version (for active development)
- `1h`, `12h`, `1d` - specific cache time

## Build Commands

### Docker (recommended)

```bash
# Build from local files
docker run --rm -v "$(pwd):/config" esphome/esphome compile example_opentherm_temp.yaml

# Build from GitHub (main branch)
docker run --rm -v "$(pwd):/config" esphome/esphome compile example_opentherm.yaml

# Validation without build
docker run --rm -v "$(pwd):/config" esphome/esphome config example_opentherm_temp.yaml
```

### ESPHome installed locally

```bash
# Compile
esphome compile example_opentherm_temp.yaml

# Upload (if ESP is connected)
esphome upload example_opentherm_temp.yaml

# Run logs
esphome logs example_opentherm_temp.yaml
```

## Python Validation

```bash
# Check Python syntax
python -m py_compile components/opentherm/__init__.py
python -m py_compile components/opentherm/sensor.py
python -m py_compile components/opentherm/binary_sensor.py
python -m py_compile components/opentherm/climate.py
python -m py_compile components/opentherm/button.py
```

## Development Workflow

1. **Make changes** in `components/opentherm/`
2. **Test locally** with `example_opentherm_temp.yaml`
3. **Commit & push** to branch (e.g., `localtests`)
4. **Test from branch** by changing `@localtests` in external_components
5. **Merge to main** when everything works
6. **Users** automatically get updates from GitHub

## Secrets

For local testing, you need `secrets.yaml`:

```yaml
wifi_ssid: 'your_wifi_ssid'
wifi_password: 'your_wifi_password'
esphome_fallback_password: 'fallback_password'
api_encryption_key: 'QlbhFmEP3L8YiOvFqVwJGqPlPrHMKyGS55vZLH5jrKs='
```

This file is in `.gitignore` - never commit real credentials!

## CI/CD

GitHub Actions automatically builds `build.yaml` on every push to main. Check status at:
https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions
