# Contributing to ESPHome OpenTherm Gateway

Thank you for your interest in contributing! This project welcomes contributions from the community.

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue with:
- **ESPHome version** you're using
- **Hardware** (ESP8266/ESP32, OpenTherm adapter model)
- **Boiler model** and configuration
- **Configuration YAML** (relevant parts)
- **Log output** with `logger: level: DEBUG`
- **Expected vs actual behavior**

### Suggesting Features

Feature suggestions are welcome! Please:
- Check existing issues first to avoid duplicates
- Describe the use case and benefit
- Consider if it fits the project scope (OpenTherm gateway functionality)

### Pull Requests

1. **Fork** the repository
2. **Create a branch** for your feature/fix
3. **Test thoroughly** with your hardware setup
4. **Follow existing code style**:
   - C++: Match existing formatting in `.cpp`/`.h` files
   - Python: Follow ESPHome component patterns
5. **Update documentation**:
   - Update `Readme.md` if adding user-facing features
   - Update `CLAUDE.md` if changing architecture
   - Add config examples to `example_opentherm.yaml`
6. **Commit messages**: Clear, descriptive (e.g., "Add support for X sensor")
7. **Create PR** with description of changes and testing done

### Testing

Before submitting:
- Build successfully: `esphome compile build.yaml`
- Test on actual hardware if possible
- Check logs for errors/warnings
- Verify Home Assistant integration works

### Development Setup

See [`CLAUDE.md`](CLAUDE.md) for:
- Component architecture details
- OpenTherm protocol documentation
- Build/test commands
- Debugging tips

## Code Review Process

- PRs will be reviewed for code quality, testing, and compatibility
- Feedback may be provided for improvements
- Once approved, changes will be merged

## Questions?

- Open an issue for questions about contributing
- Check existing issues and documentation first

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
