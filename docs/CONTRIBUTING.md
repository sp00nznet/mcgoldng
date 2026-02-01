# Contributing to MCG-NG

Thank you for your interest in contributing to MechCommander Gold: Next Generation!

## Table of Contents

- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Code Style](#code-style)
- [Submitting Changes](#submitting-changes)
- [Areas Needing Help](#areas-needing-help)
- [Code of Conduct](#code-of-conduct)

---

## Getting Started

### Prerequisites

1. Read the [README](../README.md) to understand the project
2. Set up your [development environment](BUILDING.md)
3. Familiarize yourself with the [architecture](ARCHITECTURE.md)
4. Understand the [file formats](FILE_FORMATS.md) we're working with

### Finding Something to Work On

1. Check the [Issues](https://github.com/sp00nznet/mcgoldng/issues) for open tasks
2. Look for issues labeled `good first issue` for beginner-friendly tasks
3. Check the [roadmap](../README.md#roadmap) for planned features

---

## Development Setup

### Fork and Clone

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/YOUR-USERNAME/mcgoldng.git
cd mcgoldng
git remote add upstream https://github.com/sp00nznet/mcgoldng.git
```

### Create a Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/issue-description
```

### Build and Test

```bash
cmake -B build
cmake --build build
```

---

## Code Style

### General Guidelines

- **C++17** standard
- **4 spaces** for indentation (no tabs)
- **120 character** line limit (soft)
- **Descriptive names** over comments
- **RAII** for resource management

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Classes | PascalCase | `FstReader` |
| Functions | camelCase | `readPacket()` |
| Variables | camelCase | `packetCount` |
| Member variables | m_ prefix | `m_fileSize` |
| Constants | UPPER_SNAKE | `MAX_PACKETS` |
| Namespaces | lowercase | `mcgng` |

### File Organization

```cpp
// Header files (.h)
#ifndef MCGNG_MODULE_NAME_H
#define MCGNG_MODULE_NAME_H

#include <standard_headers>
#include "project/headers.h"

namespace mcgng {

class MyClass {
public:
    // Public interface

private:
    // Implementation details
};

} // namespace mcgng

#endif // MCGNG_MODULE_NAME_H
```

```cpp
// Source files (.cpp)
#include "module/module_name.h"

#include <standard_headers>
#include "other/headers.h"

namespace mcgng {

// Implementation

} // namespace mcgng
```

### Documentation

Use Doxygen-style comments for public APIs:

```cpp
/**
 * Brief description.
 *
 * Detailed description if needed.
 *
 * @param name Parameter description
 * @return Return value description
 */
bool doSomething(const std::string& name);
```

### Error Handling

- Use return values for expected failures
- Use exceptions for exceptional situations only
- Always validate external input

```cpp
// Good
bool loadFile(const std::string& path) {
    if (path.empty()) {
        return false;  // Expected case
    }
    // ...
}

// Avoid
void loadFile(const std::string& path) {
    if (path.empty()) {
        throw std::invalid_argument("Empty path");  // Too aggressive
    }
}
```

---

## Submitting Changes

### Before Submitting

1. **Build successfully** with no errors
2. **Test your changes** manually
3. **Follow code style** guidelines
4. **Update documentation** if needed
5. **Keep commits focused** - one logical change per commit

### Commit Messages

Use clear, descriptive commit messages:

```
type: Brief description (max 50 chars)

Longer description if needed. Explain what and why,
not how. Wrap at 72 characters.

Fixes #123
```

**Types:**
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `refactor:` Code change that doesn't fix/add
- `test:` Adding tests
- `chore:` Maintenance tasks

### Pull Request Process

1. **Push your branch:**
   ```bash
   git push origin feature/your-feature-name
   ```

2. **Open a Pull Request** on GitHub

3. **Fill out the PR template:**
   - Description of changes
   - Related issues
   - Testing done

4. **Address feedback** from reviewers

5. **Squash if requested** before merge

### PR Checklist

- [ ] Code builds without errors
- [ ] Code follows style guidelines
- [ ] Changes are tested
- [ ] Documentation updated (if applicable)
- [ ] Commit messages are clear
- [ ] PR description explains the changes

---

## Areas Needing Help

### High Priority

#### Graphics Implementation
- SDL2 window management
- OpenGL rendering pipeline
- Sprite loading and display
- Palette handling for 8-bit graphics

#### Audio System
- SDL2_mixer integration
- Sound effect playback
- Music streaming with crossfade

### Medium Priority

#### File Format Research
- Documenting sprite format details
- Terrain data format
- Mission file structure

#### ABL Scripting
- Porting the interpreter from MC2
- Creating test missions

### Low Priority / Nice to Have

#### Testing
- Unit tests for file readers
- Integration tests
- Performance benchmarks

#### Tooling
- Asset viewer GUI
- Map editor
- Sprite converter

---

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Welcome newcomers
- Focus on constructive feedback
- Assume good intentions

### Unacceptable Behavior

- Harassment or discrimination
- Personal attacks
- Trolling or inflammatory comments
- Publishing others' private information

### Reporting

Report issues to the project maintainers via GitHub issues or email.

---

## Questions?

- Open a [Discussion](https://github.com/sp00nznet/mcgoldng/discussions) for general questions
- Open an [Issue](https://github.com/sp00nznet/mcgoldng/issues) for bugs or feature requests
- Check existing issues before creating new ones

---

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to MCG-NG!
