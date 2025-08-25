# Data Models and JSON Schemas

## 1. Overview

This document defines the authoritative data models and JSON schemas for the ESP32 IoT Orchestrator system. All components and APIs must adhere to these schemas for data exchange and configuration.

## 2. Core System Schemas

### 2.1 System Configuration Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "system-configuration",
  "type": "object",
  "required": ["version", "deviceId", "network"],
  "properties": {
    "version": {
      "type": "string",
      "pattern": "^\\d+\\.\\d+\\.\\d+$"
    },
    "deviceId": {
      "type": "string",
      "minLength": 1,
      "maxLength": 64
    },
    "name": {
      "type": "string",
      "maxLength": 128
    },
    "description": {
      "type": "string",
      "maxLength": 512
    },
    "location": {
      "type": "object",
      "properties": {
        "latitude": {"type": "number"},
        "longitude": {"type": "number"},
        "altitude": {"type": "number"},
        "address": {"type": "string"}
      }
    },
    "network": {
      "type": "object",
      "required": ["wifi"],
      "properties": {
        "wifi": {
          "type": "object",
          "required": ["ssid"],
          "properties": {
            "ssid": {"type": "string"},
            "password": {"type": "string"},
            "hostname": {"type": "string"},
            "staticIP": {
              "type": "object",
              "properties": {
                "ip": {"type": "string", "format": "ipv4"},
                "gateway": {"type": "string", "format": "ipv4"},
                "subnet": {"type": "string", "format": "ipv4"},
                "dns1": {"type": "string", "format": "ipv4"},
                "dns2": {"type": "string", "format": "ipv4"}
              }
            }
          }
        }
      }
    },
    "time": {
      "type": "object",
      "properties": {
        "timezone": {"type": "string"},
        "ntpServer": {"type": "string"},
        "updateInterval": {"type": "integer", "minimum": 3600}
      }
    },
    "logging": {
      "type": "object",
      "properties": {
        "level": {
          "type": "string",
          "enum": ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
        },
        "destinations": {
          "type": "array",
          "items": {
            "type": "string",
            "enum": ["serial", "file", "network", "mqtt"]
          }
        },
        "maxFileSize": {"type": "integer"},
        "maxFiles": {"type": "integer"}
      }
    }
  }
}
```

### 2.2 Component Configuration Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "component-configuration",
  "type": "object",
  "required": ["id", "type", "enabled"],
  "properties": {
    "id": {
      "type": "string",
      "pattern": "^[a-zA-Z0-9_-]+$",
      "minLength": 1,
      "maxLength": 64
    },
    "type": {
      "type": "string",
      "minLength": 1,
      "maxLength": 64
    },
    "name": {
      "type": "string",
      "maxLength": 128
    },
    "enabled": {
      "type": "boolean"
    },
    "version": {
      "type": "string",
      "pattern": "^\\d+\\.\\d+\\.\\d+$"
    },
    "description": {
      "type": "string",
      "maxLength": 512
    },
    "tags": {
      "type": "array",
      "items": {"type": "string"},
      "uniqueItems": true
    },
    "config": {
      "type": "object",
      "additionalProperties": true
    },
    "schedule": {
      "type": "object",
      "properties": {
        "interval": {
          "type": "integer",
          "minimum": 0,
          "description": "Execution interval in milliseconds"
        },
        "cron": {
          "type": "string",
          "description": "Cron expression for scheduling"
        },
        "priority": {
          "type": "integer",
          "minimum": 0,
          "maximum": 10
        },
        "maxExecutionTime": {
          "type": "integer",
          "minimum": 0,
          "description": "Maximum execution time in milliseconds"
        }
      }
    },
    "dependencies": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["componentId"],
        "properties": {
          "componentId": {"type": "string"},
          "required": {"type": "boolean"},
          "version": {"type": "string"}
        }
      }
    },
    "resources": {
      "type": "object",
      "properties": {
        "maxMemory": {"type": "integer"},
        "maxCpu": {"type": "number", "minimum": 0, "maximum": 100},
        "gpioP pins": {
          "type": "array",
          "items": {"type": "integer"}
        }
      }
    }
  }
}
```

## 3. Component Type Schemas

### 3.1 Sensor Component Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "sensor-component",
  "type": "object",
  "allOf": [
    {"$ref": "component-configuration"},
    {
      "properties": {
        "sensorConfig": {
          "type": "object",
          "required": ["measurementType", "unit"],
          "properties": {
            "measurementType": {
              "type": "string",
              "enum": ["temperature", "humidity", "pressure", "light", "motion", "distance", "voltage", "current", "custom"]
            },
            "unit": {"type": "string"},
            "minValue": {"type": "number"},
            "maxValue": {"type": "number"},
            "accuracy": {"type": "number"},
            "resolution": {"type": "number"},
            "samplingRate": {
              "type": "integer",
              "minimum": 1,
              "description": "Sampling rate in milliseconds"
            },
            "calibration": {
              "type": "object",
              "properties": {
                "offset": {"type": "number"},
                "scale": {"type": "number"},
                "coefficients": {
                  "type": "array",
                  "items": {"type": "number"}
                }
              }
            },
            "filtering": {
              "type": "object",
              "properties": {
                "enabled": {"type": "boolean"},
                "type": {
                  "type": "string",
                  "enum": ["moving_average", "median", "kalman", "exponential"]
                },
                "windowSize": {"type": "integer"},
                "alpha": {"type": "number"}
              }
            },
            "thresholds": {
              "type": "object",
              "properties": {
                "low": {"type": "number"},
                "high": {"type": "number"},
                "hysteresis": {"type": "number"}
              }
            }
          }
        }
      }
    }
  ]
}
```

### 3.2 Actuator Component Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "actuator-component",
  "type": "object",
  "allOf": [
    {"$ref": "component-configuration"},
    {
      "properties": {
        "actuatorConfig": {
          "type": "object",
          "required": ["actuatorType", "controlMode"],
          "properties": {
            "actuatorType": {
              "type": "string",
              "enum": ["relay", "motor", "servo", "led", "display", "buzzer", "valve", "pump", "custom"]
            },
            "controlMode": {
              "type": "string",
              "enum": ["binary", "pwm", "analog", "digital", "position", "velocity"]
            },
            "gpio": {
              "type": "object",
              "properties": {
                "controlPin": {"type": "integer"},
                "enablePin": {"type": "integer"},
                "directionPin": {"type": "integer"},
                "feedbackPin": {"type": "integer"}
              }
            },
            "limits": {
              "type": "object",
              "properties": {
                "minPosition": {"type": "number"},
                "maxPosition": {"type": "number"},
                "maxSpeed": {"type": "number"},
                "maxAcceleration": {"type": "number"},
                "maxCurrent": {"type": "number"},
                "maxTemperature": {"type": "number"}
              }
            },
            "safety": {
              "type": "object",
              "properties": {
                "emergencyStopEnabled": {"type": "boolean"},
                "watchdogTimeout": {"type": "integer"},
                "failsafePosition": {"type": "number"},
                "interlocks": {
                  "type": "array",
                  "items": {
                    "type": "object",
                    "properties": {
                      "componentId": {"type": "string"},
                      "condition": {"type": "string"}
                    }
                  }
                }
              }
            },
            "feedback": {
              "type": "object",
              "properties": {
                "enabled": {"type": "boolean"},
                "type": {
                  "type": "string",
                  "enum": ["encoder", "potentiometer", "current", "endstop"]
                },
                "scale": {"type": "number"},
                "offset": {"type": "number"}
              }
            }
          }
        }
      }
    }
  ]
}
```

### 3.3 Service Component Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "service-component",
  "type": "object",
  "allOf": [
    {"$ref": "component-configuration"},
    {
      "properties": {
        "serviceConfig": {
          "type": "object",
          "required": ["serviceType", "protocol"],
          "properties": {
            "serviceType": {
              "type": "string",
              "enum": ["mqtt", "http", "websocket", "modbus", "serial", "custom"]
            },
            "protocol": {
              "type": "object",
              "properties": {
                "type": {"type": "string"},
                "version": {"type": "string"},
                "encoding": {
                  "type": "string",
                  "enum": ["json", "binary", "text", "protobuf"]
                }
              }
            },
            "connection": {
              "type": "object",
              "properties": {
                "host": {"type": "string"},
                "port": {"type": "integer"},
                "secure": {"type": "boolean"},
                "auth": {
                  "type": "object",
                  "properties": {
                    "type": {
                      "type": "string",
                      "enum": ["none", "basic", "token", "certificate"]
                    },
                    "credentials": {"type": "object"}
                  }
                },
                "timeout": {"type": "integer"},
                "reconnect": {
                  "type": "object",
                  "properties": {
                    "enabled": {"type": "boolean"},
                    "interval": {"type": "integer"},
                    "maxAttempts": {"type": "integer"}
                  }
                }
              }
            },
            "topics": {
              "type": "array",
              "items": {
                "type": "object",
                "properties": {
                  "name": {"type": "string"},
                  "type": {
                    "type": "string",
                    "enum": ["publish", "subscribe", "both"]
                  },
                  "qos": {"type": "integer", "minimum": 0, "maximum": 2}
                }
              }
            }
          }
        }
      }
    }
  ]
}
```

## 4. API Message Schemas

### 4.1 Request Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "api-request",
  "type": "object",
  "required": ["method", "path"],
  "properties": {
    "id": {"type": "string"},
    "method": {
      "type": "string",
      "enum": ["GET", "POST", "PUT", "DELETE", "PATCH"]
    },
    "path": {"type": "string"},
    "headers": {
      "type": "object",
      "additionalProperties": {"type": "string"}
    },
    "params": {
      "type": "object",
      "additionalProperties": true
    },
    "body": {
      "type": "object",
      "additionalProperties": true
    },
    "timestamp": {
      "type": "integer",
      "description": "Unix timestamp in milliseconds"
    }
  }
}
```

### 4.2 Response Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "api-response",
  "type": "object",
  "required": ["status", "timestamp"],
  "properties": {
    "id": {"type": "string"},
    "status": {
      "type": "integer",
      "minimum": 100,
      "maximum": 599
    },
    "success": {"type": "boolean"},
    "message": {"type": "string"},
    "data": {
      "type": ["object", "array", "null"],
      "additionalProperties": true
    },
    "error": {
      "type": "object",
      "properties": {
        "code": {"type": "string"},
        "message": {"type": "string"},
        "details": {"type": "object"}
      }
    },
    "metadata": {
      "type": "object",
      "properties": {
        "processingTime": {"type": "integer"},
        "version": {"type": "string"},
        "pagination": {
          "type": "object",
          "properties": {
            "page": {"type": "integer"},
            "pageSize": {"type": "integer"},
            "totalPages": {"type": "integer"},
            "totalItems": {"type": "integer"}
          }
        }
      }
    },
    "timestamp": {"type": "integer"}
  }
}
```

## 5. Event Schemas

### 5.1 System Event Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "system-event",
  "type": "object",
  "required": ["eventType", "timestamp", "source"],
  "properties": {
    "id": {"type": "string"},
    "eventType": {
      "type": "string",
      "enum": [
        "system.startup",
        "system.shutdown",
        "system.error",
        "system.warning",
        "component.registered",
        "component.unregistered",
        "component.started",
        "component.stopped",
        "component.error",
        "component.data",
        "config.changed",
        "network.connected",
        "network.disconnected"
      ]
    },
    "timestamp": {"type": "integer"},
    "source": {
      "type": "object",
      "required": ["type", "id"],
      "properties": {
        "type": {
          "type": "string",
          "enum": ["system", "component", "service"]
        },
        "id": {"type": "string"},
        "name": {"type": "string"}
      }
    },
    "severity": {
      "type": "string",
      "enum": ["debug", "info", "warning", "error", "critical"]
    },
    "data": {
      "type": "object",
      "additionalProperties": true
    },
    "context": {
      "type": "object",
      "properties": {
        "correlationId": {"type": "string"},
        "sessionId": {"type": "string"},
        "userId": {"type": "string"}
      }
    }
  }
}
```

### 5.2 Component Event Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "component-event",
  "type": "object",
  "required": ["componentId", "eventType", "timestamp"],
  "properties": {
    "componentId": {"type": "string"},
    "eventType": {
      "type": "string",
      "enum": [
        "state.changed",
        "data.available",
        "threshold.exceeded",
        "error.occurred",
        "command.executed",
        "config.updated"
      ]
    },
    "timestamp": {"type": "integer"},
    "oldValue": {},
    "newValue": {},
    "data": {
      "type": "object",
      "additionalProperties": true
    },
    "metadata": {
      "type": "object",
      "additionalProperties": true
    }
  }
}
```

## 6. Data Storage Schemas

### 6.1 Time Series Data Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "timeseries-data",
  "type": "object",
  "required": ["timestamp", "componentId", "value"],
  "properties": {
    "timestamp": {"type": "integer"},
    "componentId": {"type": "string"},
    "metric": {"type": "string"},
    "value": {
      "type": ["number", "boolean", "string", "object"]
    },
    "unit": {"type": "string"},
    "quality": {
      "type": "string",
      "enum": ["good", "uncertain", "bad"]
    },
    "tags": {
      "type": "object",
      "additionalProperties": {"type": "string"}
    }
  }
}
```

### 6.2 Log Entry Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "log-entry",
  "type": "object",
  "required": ["timestamp", "level", "message"],
  "properties": {
    "timestamp": {"type": "integer"},
    "level": {
      "type": "string",
      "enum": ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
    },
    "source": {"type": "string"},
    "message": {"type": "string"},
    "context": {
      "type": "object",
      "additionalProperties": true
    },
    "stackTrace": {"type": "string"},
    "errorCode": {"type": "string"}
  }
}
```

## 7. Validation Rules

### 7.1 Common Validation Patterns

```javascript
const validationPatterns = {
  componentId: /^[a-zA-Z0-9_-]{1,64}$/,
  version: /^\d+\.\d+\.\d+$/,
  ipv4: /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/,
  mac: /^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/,
  email: /^[^\s@]+@[^\s@]+\.[^\s@]+$/,
  url: /^https?:\/\/.+$/,
  cron: /^(\*|([0-9]|1[0-9]|2[0-9]|3[0-9]|4[0-9]|5[0-9])|\*\/([0-9]|1[0-9]|2[0-9]|3[0-9]|4[0-9]|5[0-9])) (\*|([0-9]|1[0-9]|2[0-3])|\*\/([0-9]|1[0-9]|2[0-3])) (\*|([1-9]|1[0-9]|2[0-9]|3[0-1])|\*\/([1-9]|1[0-9]|2[0-9]|3[0-1])) (\*|([1-9]|1[0-2])|\*\/([1-9]|1[0-2])) (\*|([0-6])|\*\/([0-6]))$/
};
```

### 7.2 Custom Validators

```cpp
class DataValidator {
public:
    static bool validateComponentConfig(const JsonDocument& config) {
        // Required fields
        if (!config.containsKey("id") || !config.containsKey("type")) {
            return false;
        }
        
        // ID format
        String id = config["id"].as<String>();
        if (!id.matches("^[a-zA-Z0-9_-]+$")) {
            return false;
        }
        
        // Version format if present
        if (config.containsKey("version")) {
            String version = config["version"].as<String>();
            if (!version.matches("^\\d+\\.\\d+\\.\\d+$")) {
                return false;
            }
        }
        
        return true;
    }
    
    static bool validateSensorReading(float value, float min, float max) {
        return value >= min && value <= max && !isnan(value) && !isinf(value);
    }
};
```

## 8. Schema Evolution Strategy

### 8.1 Versioning Rules

1. **Major Version**: Breaking changes to required fields
2. **Minor Version**: New optional fields added
3. **Patch Version**: Documentation or validation updates

### 8.2 Migration Path

```cpp
class SchemaМigrator {
public:
    static JsonDocument migrate(const JsonDocument& data, const String& fromVersion, const String& toVersion) {
        JsonDocument migrated = data;
        
        // Apply migrations in sequence
        if (fromVersion < "1.0.0" && toVersion >= "1.0.0") {
            // Migration logic for 1.0.0
        }
        
        if (fromVersion < "1.1.0" && toVersion >= "1.1.0") {
            // Migration logic for 1.1.0
        }
        
        return migrated;
    }
};
```

## 9. Schema Registry

### 9.1 Registration Format

```cpp
struct SchemaRegistryEntry {
    String schemaId;
    String version;
    JsonDocument schema;
    String hash;
    uint32_t registeredAt;
};

class SchemaRegistry {
private:
    std::map<String, SchemaRegistryEntry> schemas;
    
public:
    bool registerSchema(const String& id, const JsonDocument& schema);
    JsonDocument getSchema(const String& id, const String& version = "latest");
    bool validateAgainstSchema(const String& schemaId, const JsonDocument& data);
};
```

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-01-25  
**Status**: Authoritative