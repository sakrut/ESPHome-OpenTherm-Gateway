# Security Policy

## Supported Versions

This project follows ESPHome's release cycle. We recommend using the latest version.

| Version | Supported          |
| ------- | ------------------ |
| Latest  | :white_check_mark: |
| Older   | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability, please:

1. **DO NOT** open a public issue
2. **Email** the maintainer directly (check GitHub profile for contact)
3. **Include**:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

## Security Considerations

### Network Security
- This component communicates with Home Assistant via WiFi
- Ensure your ESPHome device is on a trusted network
- Use strong WiFi passwords and encryption (WPA2/WPA3)
- Consider network segmentation for IoT devices

### Physical Security
- The component controls heating systems
- Ensure physical access to the ESP device is restricted
- Use ESPHome's `api:` encryption and password protection
- Enable `ota:` password for firmware updates

### Recommended Configuration

```yaml
api:
  encryption:
    key: !secret api_encryption_key

ota:
  password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

## Known Limitations

- This is a hardware gateway component - ensure proper electrical safety
- The component intercepts OpenTherm communication - test thoroughly before deployment
- Boiler control carries inherent risks - use at your own risk

## Updates

Security updates will be released as needed. Monitor the repository for updates.
