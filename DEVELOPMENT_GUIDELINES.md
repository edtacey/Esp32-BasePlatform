# ESP32 IoT Orchestrator - Development Guidelines

## 1. Overview

This document establishes the authoritative development standards, practices, and guidelines for the ESP32 IoT Orchestrator project. All contributors must adhere to these guidelines to ensure code quality, maintainability, and consistency.

## 2. Development Environment Setup

### 2.1 Required Tools

```bash
# Core Development Tools
- VSCode (latest stable)
- PlatformIO Core 6.x
- Git 2.x
- Python 3.8+
- Node.js 16+ (for web tools)

# ESP32 Specific
- ESP-IDF 4.4+ (via PlatformIO)
- esptool.py
- ESP32 USB drivers

# Optional but Recommended
- Docker Desktop
- Postman or Insomnia
- Serial monitor (CoolTerm, PuTTY)
```

### 2.2 VSCode Extensions

```json
{
  "recommendations": [
    "platformio.platformio-ide",
    "ms-vscode.cpptools",
    "streetsidesoftware.code-spell-checker",
    "wayou.vscode-todo-highlight",
    "gruntfuggly.todo-tree",
    "mhutchie.git-graph",
    "eamodio.gitlens",
    "shardulm94.trailing-spaces",
    "editorconfig.editorconfig"
  ]
}
```

### 2.3 Project Configuration

**.editorconfig:**
```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{cpp,h,hpp,c}]
indent_style = space
indent_size = 4

[*.{json,yaml,yml}]
indent_style = space
indent_size = 2

[*.md]
trim_trailing_whitespace = false
```

## 3. Coding Standards

### 3.1 C++ Style Guide

#### Naming Conventions

```cpp
// Classes: PascalCase
class ComponentRegistry {};

// Methods: camelCase
void registerComponent();

// Member variables: m_ prefix + camelCase
class Example {
private:
    int m_componentCount;
    String m_deviceId;
};

// Constants: UPPER_SNAKE_CASE
const int MAX_COMPONENTS = 32;
#define DEFAULT_TIMEOUT 5000

// Namespaces: lowercase
namespace orchestrator {
namespace components {
}}

// Enums: PascalCase for type, UPPER_SNAKE_CASE for values
enum ComponentState {
    STATE_UNINITIALIZED = 0,
    STATE_READY = 1,
    STATE_ERROR = 2
};
```

#### File Organization

```cpp
// ComponentRegistry.h
#ifndef COMPONENT_REGISTRY_H
#define COMPONENT_REGISTRY_H

#include <Arduino.h>
#include <vector>
#include "IBaselineComponent.h"

namespace orchestrator {

/**
 * @brief Manages component lifecycle and registration
 * 
 * The ComponentRegistry maintains a collection of all active
 * components and provides methods for registration, lookup,
 * and lifecycle management.
 */
class ComponentRegistry {
public:
    ComponentRegistry();
    ~ComponentRegistry();
    
    // Public methods
    bool registerComponent(IBaselineComponent* component);
    IBaselineComponent* findComponent(const String& id);
    
private:
    // Private members
    std::vector<IBaselineComponent*> m_components;
    uint32_t m_maxComponents;
    
    // Private methods
    bool validateComponent(IBaselineComponent* component);
};

} // namespace orchestrator

#endif // COMPONENT_REGISTRY_H
```

#### Code Formatting

```cpp
// Function definitions
bool ComponentRegistry::registerComponent(IBaselineComponent* component) {
    // Early return for invalid input
    if (!component) {
        Logger::error("ComponentRegistry", "Null component provided");
        return false;
    }
    
    // Single blank line between logical sections
    if (!validateComponent(component)) {
        return false;
    }
    
    // Use auto for complex iterator types
    auto it = std::find_if(m_components.begin(), m_components.end(),
        [&](IBaselineComponent* c) {
            return c->getInfo().id == component->getInfo().id;
        });
    
    // Consistent brace style
    if (it != m_components.end()) {
        Logger::warning("ComponentRegistry", 
                       "Component already registered: " + component->getInfo().id);
        return false;
    }
    
    m_components.push_back(component);
    return true;
}

// Lambda expressions
auto filter = [](const Component& c) -> bool {
    return c.isEnabled() && c.getState() == STATE_READY;
};

// Complex conditionals
if ((component->getState() == STATE_READY || 
     component->getState() == STATE_IDLE) &&
    component->isEnabled() &&
    !component->hasErrors()) {
    // Action
}
```

### 3.2 Memory Management

#### RAII Pattern
```cpp
class ScopedLock {
private:
    SemaphoreHandle_t m_semaphore;
    
public:
    explicit ScopedLock(SemaphoreHandle_t semaphore) 
        : m_semaphore(semaphore) {
        xSemaphoreTake(m_semaphore, portMAX_DELAY);
    }
    
    ~ScopedLock() {
        xSemaphoreGive(m_semaphore);
    }
    
    // Delete copy operations
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
};
```

#### Smart Pointers
```cpp
// Prefer unique_ptr for single ownership
std::unique_ptr<IBaselineComponent> component = 
    std::make_unique<DHT22Component>(config);

// Use shared_ptr for shared ownership
std::shared_ptr<ConfigStorage> config = 
    std::make_shared<ConfigStorage>();

// Avoid raw pointers except for:
// 1. Non-owning references
// 2. C API interop
// 3. Performance-critical sections
```

#### Memory Pools
```cpp
template<typename T, size_t Size>
class MemoryPool {
private:
    struct Block {
        T data;
        bool used;
    };
    
    Block m_pool[Size];
    
public:
    T* allocate() {
        for (auto& block : m_pool) {
            if (!block.used) {
                block.used = true;
                return &block.data;
            }
        }
        return nullptr;
    }
    
    void deallocate(T* ptr) {
        // Mark block as unused
    }
};
```

### 3.3 Error Handling

#### Error Codes
```cpp
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_PARAMETER = -1,
    OUT_OF_MEMORY = -2,
    COMPONENT_NOT_FOUND = -3,
    TIMEOUT = -4,
    HARDWARE_ERROR = -5
};

struct Result {
    ErrorCode code;
    String message;
    JsonDocument data;
    
    bool isSuccess() const { return code == ErrorCode::SUCCESS; }
    operator bool() const { return isSuccess(); }
};
```

#### Exception Safety
```cpp
// Use noexcept where appropriate
class Component {
public:
    Component() noexcept = default;
    Component(Component&& other) noexcept;
    
    // Strong exception guarantee
    void updateConfig(const JsonDocument& config) {
        JsonDocument backup = m_config;
        try {
            m_config = config;
            applyConfiguration();
        } catch (...) {
            m_config = backup;  // Rollback
            throw;
        }
    }
};
```

## 4. Component Development

### 4.1 Component Template

```cpp
// MyComponent.h
#ifndef MY_COMPONENT_H
#define MY_COMPONENT_H

#include "components/BaseComponent.h"

class MyComponent : public BaseComponent {
public:
    explicit MyComponent(const String& id);
    ~MyComponent() override;
    
    // IBaselineComponent interface
    bool init(const JsonDocument& config) override;
    void execute() override;
    ExecutionResult executeNow(const JsonDocument& input) override;
    JsonDocument getExecutionOptions() const override;
    void cleanup() override;
    
    // Component-specific methods
    void customMethod();
    
private:
    // Configuration
    struct Config {
        uint8_t pin;
        uint32_t interval;
        bool enableFeature;
    } m_config;
    
    // State
    float m_lastValue;
    uint32_t m_errorCount;
    
    // Helper methods
    bool parseConfiguration(const JsonDocument& config);
    void performMeasurement();
};

#endif // MY_COMPONENT_H
```

### 4.2 Component Implementation Checklist

```markdown
## Component Implementation Checklist

- [ ] Inherits from appropriate base class
- [ ] Implements all pure virtual methods
- [ ] Constructor initializes all members
- [ ] Destructor cleans up resources
- [ ] Configuration validation implemented
- [ ] Error handling for all operations
- [ ] Thread-safe if accessed concurrently
- [ ] Memory leaks checked with analyzer
- [ ] Unit tests written (>80% coverage)
- [ ] Integration test with orchestrator
- [ ] Documentation complete
- [ ] Example usage provided
- [ ] Performance profiled
- [ ] Code reviewed
```

## 5. Testing Standards

### 5.1 Unit Testing

```cpp
// test/test_component_registry.cpp
#include <unity.h>
#include "ComponentRegistry.h"
#include "mocks/MockComponent.h"

void setUp() {
    // Setup before each test
}

void tearDown() {
    // Cleanup after each test
}

void test_registerComponent_validComponent_returnsTrue() {
    // Arrange
    ComponentRegistry registry;
    auto component = std::make_unique<MockComponent>("test-1");
    
    // Act
    bool result = registry.registerComponent(component.get());
    
    // Assert
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, registry.getComponentCount());
}

void test_registerComponent_duplicateId_returnsFalse() {
    // Arrange
    ComponentRegistry registry;
    auto component1 = std::make_unique<MockComponent>("test-1");
    auto component2 = std::make_unique<MockComponent>("test-1");
    
    // Act
    registry.registerComponent(component1.get());
    bool result = registry.registerComponent(component2.get());
    
    // Assert
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(1, registry.getComponentCount());
}

int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_registerComponent_validComponent_returnsTrue);
    RUN_TEST(test_registerComponent_duplicateId_returnsFalse);
    
    return UNITY_END();
}
```

### 5.2 Integration Testing

```cpp
// test/test_integration_orchestrator.cpp
void test_orchestrator_component_lifecycle() {
    // Test complete component lifecycle
    Orchestrator orchestrator;
    orchestrator.init();
    
    // Register component
    JsonDocument config;
    config["pin"] = 22;
    auto component = ComponentFactory::create("DHT22", "sensor-1", config);
    
    TEST_ASSERT_TRUE(orchestrator.registerComponent(std::move(component)));
    
    // Execute orchestration loop
    for (int i = 0; i < 10; i++) {
        orchestrator.loop();
        delay(100);
    }
    
    // Verify component executed
    auto stats = orchestrator.getComponentStats("sensor-1");
    TEST_ASSERT_TRUE(stats["executions"].as<int>() > 0);
}
```

### 5.3 Test Coverage Requirements

- **Unit Tests**: Minimum 80% code coverage
- **Integration Tests**: All major workflows covered
- **Performance Tests**: Critical paths benchmarked
- **Memory Tests**: No leaks, fragmentation monitored
- **Stress Tests**: System limits validated

## 6. Documentation Standards

### 6.1 Code Documentation

```cpp
/**
 * @brief Registers a component with the orchestrator
 * 
 * This method adds a component to the orchestrator's component
 * registry and initializes it with the provided configuration.
 * 
 * @param component Unique pointer to the component instance
 * @param config Component configuration as JSON document
 * 
 * @return true if registration successful, false otherwise
 * 
 * @throws std::invalid_argument if component is null
 * @throws std::runtime_error if registration fails
 * 
 * @code
 * auto component = std::make_unique<DHT22Component>("sensor-1");
 * JsonDocument config;
 * config["pin"] = 22;
 * orchestrator.registerComponent(std::move(component), config);
 * @endcode
 * 
 * @see unregisterComponent()
 * @since 1.0.0
 */
bool registerComponent(std::unique_ptr<IBaselineComponent> component, 
                       const JsonDocument& config);
```

### 6.2 README Template

```markdown
# Component Name

## Overview
Brief description of the component's purpose and functionality.

## Features
- Feature 1
- Feature 2
- Feature 3

## Requirements
- Hardware requirements
- Software dependencies
- Pin connections

## Installation
```bash
platformio lib install "ComponentName"
```

## Configuration
```json
{
  "type": "sensor",
  "subtype": "DHT22",
  "config": {
    "pin": 22,
    "samplingRate": 5000
  }
}
```

## Usage
```cpp
#include "DHT22Component.h"

auto component = std::make_unique<DHT22Component>("sensor-1");
orchestrator.registerComponent(std::move(component));
```

## API Reference
[Link to detailed API documentation]

## Examples
[Link to example code]

## Troubleshooting
Common issues and solutions.

## License
MIT License
```

## 7. Git Workflow

### 7.1 Branch Strategy

```bash
main                 # Production-ready code
├── develop         # Integration branch
    ├── feature/*   # New features
    ├── bugfix/*    # Bug fixes
    ├── hotfix/*    # Emergency fixes
    └── release/*   # Release preparation
```

### 7.2 Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>

# Types:
- feat: New feature
- fix: Bug fix
- docs: Documentation
- style: Code style changes
- refactor: Code refactoring
- perf: Performance improvement
- test: Test updates
- chore: Build/tool changes

# Examples:
feat(components): add DHT22 temperature sensor support
fix(orchestrator): resolve memory leak in component registry
docs(api): update REST API documentation for v1.1
```

### 7.3 Pull Request Template

```markdown
## Description
Brief description of changes.

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Manual testing completed

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] No warnings generated
- [ ] Tests added/updated
- [ ] Memory usage analyzed

## Related Issues
Fixes #123
```

## 8. Performance Guidelines

### 8.1 Optimization Priorities

1. **Memory Usage** - Most critical on ESP32
2. **Response Time** - API < 100ms
3. **Power Consumption** - For battery operation
4. **Network Efficiency** - Minimize data transfer

### 8.2 Performance Best Practices

```cpp
// Use const references for large objects
void processData(const JsonDocument& data);

// Prefer stack allocation
char buffer[256];  // Instead of dynamic allocation

// Reserve vector capacity
std::vector<Component*> components;
components.reserve(MAX_COMPONENTS);

// Use move semantics
Component createComponent() {
    Component c;
    // ...
    return c;  // NRVO/move
}

// Inline small functions
inline uint32_t getTimestamp() {
    return millis();
}

// Minimize string operations
static const char* const COMPONENT_TYPE = "sensor";  // Instead of String

// Cache expensive calculations
class Component {
    mutable uint32_t m_cachedHash = 0;
    mutable bool m_hashValid = false;
    
    uint32_t getHash() const {
        if (!m_hashValid) {
            m_cachedHash = calculateHash();
            m_hashValid = true;
        }
        return m_cachedHash;
    }
};
```

## 9. Security Guidelines

### 9.1 Security Checklist

- [ ] Input validation on all external data
- [ ] Bounds checking on arrays/buffers
- [ ] No hardcoded credentials
- [ ] Secure credential storage (NVS encrypted)
- [ ] HTTPS for sensitive endpoints
- [ ] Authentication required for API
- [ ] Rate limiting implemented
- [ ] Logging excludes sensitive data
- [ ] Buffer overflow prevention
- [ ] SQL injection prevention (if applicable)

### 9.2 Secure Coding Practices

```cpp
// Input validation
bool validateInput(const String& input) {
    if (input.length() > MAX_INPUT_LENGTH) {
        return false;
    }
    
    // Check for malicious patterns
    if (input.indexOf("../") >= 0) {
        return false;
    }
    
    return true;
}

// Secure credential handling
class CredentialManager {
private:
    // Never store plain text
    String m_encryptedPassword;
    
public:
    void setPassword(const String& password) {
        m_encryptedPassword = encrypt(password);
        // Clear original from memory
        memset((void*)password.c_str(), 0, password.length());
    }
};

// Buffer overflow prevention
void safeCopy(char* dest, size_t destSize, const char* src) {
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}
```

## 10. Code Review Checklist

### 10.1 Review Points

```markdown
## Code Review Checklist

### Functionality
- [ ] Code does what it's supposed to do
- [ ] Edge cases handled
- [ ] Error conditions handled
- [ ] No obvious bugs

### Code Quality
- [ ] Follows coding standards
- [ ] No code duplication
- [ ] Clear and meaningful names
- [ ] Appropriate comments
- [ ] No dead code
- [ ] No magic numbers

### Performance
- [ ] No unnecessary loops
- [ ] Efficient algorithms used
- [ ] Memory usage optimized
- [ ] No memory leaks

### Security
- [ ] Input validation present
- [ ] No security vulnerabilities
- [ ] Credentials handled securely
- [ ] No sensitive data in logs

### Testing
- [ ] Unit tests present
- [ ] Tests cover edge cases
- [ ] Integration tests updated
- [ ] Manual testing done

### Documentation
- [ ] Public APIs documented
- [ ] README updated if needed
- [ ] Changelog updated
- [ ] Complex logic explained
```

## 11. Build and Release

### 11.1 Build Configuration

```ini
# platformio.ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

build_flags = 
    -DCORE_DEBUG_LEVEL=2
    -DVERSION=\"${VERSION}\"
    -Wall
    -Wextra
    -Wno-unused-parameter
    
build_unflags = 
    -Werror

[env:esp32dev_release]
extends = env:esp32dev
build_flags = 
    -DCORE_DEBUG_LEVEL=0
    -DNDEBUG
    -O2
```

### 11.2 Release Process

```bash
# 1. Update version
./scripts/update_version.sh 1.1.0

# 2. Run tests
platformio test

# 3. Build release
platformio run -e esp32dev_release

# 4. Generate documentation
doxygen Doxyfile

# 5. Create release package
./scripts/create_release.sh

# 6. Tag release
git tag -a v1.1.0 -m "Release version 1.1.0"
git push origin v1.1.0
```

## 12. Troubleshooting Guide

### 12.1 Common Issues

| Issue | Solution |
|-------|----------|
| Stack overflow | Increase task stack size |
| Heap fragmentation | Use memory pools |
| Watchdog timeout | Add `yield()` or task delays |
| WiFi disconnects | Implement reconnection logic |
| Component not found | Check registration order |

### 12.2 Debug Techniques

```cpp
// Debug macros
#ifdef DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#endif

// Performance profiling
class ProfileTimer {
    uint32_t m_start;
    String m_name;
    
public:
    ProfileTimer(const String& name) : m_name(name) {
        m_start = micros();
    }
    
    ~ProfileTimer() {
        uint32_t duration = micros() - m_start;
        Serial.printf("%s took %u us\n", m_name.c_str(), duration);
    }
};

// Memory monitoring
void printMemoryStats() {
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Max alloc: %d\n", ESP.getMaxAllocHeap());
    Serial.printf("PSRAM free: %d\n", ESP.getFreePsram());
}
```

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-01-25  
**Status**: Authoritative  
**Review Schedule**: Quarterly