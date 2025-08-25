# ESP32 IoT Orchestrator - API Specification

## 1. Overview

This document provides the complete REST API specification for the ESP32 IoT Orchestrator. All endpoints follow RESTful principles and return JSON responses.

## 2. API Conventions

### 2.1 Base URL
```
http://{device-ip}/api/v1
```

### 2.2 Authentication
```http
Authorization: Bearer {token}
X-API-Key: {api-key}
```

### 2.3 Common Headers
```http
Content-Type: application/json
Accept: application/json
X-Request-ID: {uuid}
```

### 2.4 Response Format
```json
{
  "success": true,
  "data": {},
  "message": "Operation successful",
  "timestamp": 1706198400000,
  "metadata": {
    "version": "1.0.0",
    "processingTime": 45
  }
}
```

### 2.5 Error Response Format
```json
{
  "success": false,
  "error": {
    "code": "COMPONENT_NOT_FOUND",
    "message": "Component with ID 'sensor-1' not found",
    "details": {
      "componentId": "sensor-1",
      "availableComponents": ["sensor-2", "sensor-3"]
    }
  },
  "timestamp": 1706198400000
}
```

### 2.6 HTTP Status Codes
- `200 OK` - Successful GET, PUT
- `201 Created` - Successful POST creating resource
- `204 No Content` - Successful DELETE
- `400 Bad Request` - Invalid request parameters
- `401 Unauthorized` - Missing or invalid authentication
- `403 Forbidden` - Insufficient permissions
- `404 Not Found` - Resource not found
- `409 Conflict` - Resource conflict
- `429 Too Many Requests` - Rate limit exceeded
- `500 Internal Server Error` - Server error
- `503 Service Unavailable` - Service temporarily unavailable

## 3. System Endpoints

### 3.1 Get System Status
```http
GET /api/v1/system/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "operational",
    "uptime": 3600000,
    "version": "1.0.0",
    "deviceId": "esp32-001",
    "timestamp": 1706198400000,
    "health": {
      "cpu": 45.2,
      "memory": {
        "used": 98304,
        "free": 229376,
        "total": 327680,
        "percentage": 30
      },
      "temperature": 42.5,
      "wifi": {
        "connected": true,
        "ssid": "IoT-Network",
        "rssi": -65,
        "ip": "192.168.1.100"
      }
    },
    "components": {
      "total": 12,
      "active": 10,
      "error": 2
    }
  }
}
```

### 3.2 Get System Information
```http
GET /api/v1/system/info
```

**Response:**
```json
{
  "success": true,
  "data": {
    "hardware": {
      "chip": "ESP32-D0WDQ6",
      "revision": 1,
      "cores": 2,
      "frequency": 240,
      "flash": 4194304,
      "psram": 0
    },
    "firmware": {
      "version": "1.0.0",
      "buildDate": "2025-01-25T10:00:00Z",
      "sdkVersion": "4.4.5",
      "gitCommit": "abc123def"
    },
    "network": {
      "mac": "24:6F:28:12:34:56",
      "hostname": "esp32-orchestrator"
    },
    "features": [
      "wifi",
      "bluetooth",
      "ota",
      "websocket",
      "mqtt"
    ]
  }
}
```

### 3.3 System Configuration
```http
GET /api/v1/system/config
```

**Response:**
```json
{
  "success": true,
  "data": {
    "network": {
      "wifi": {
        "ssid": "IoT-Network",
        "hostname": "esp32-orchestrator"
      }
    },
    "time": {
      "timezone": "UTC",
      "ntpServer": "pool.ntp.org"
    },
    "logging": {
      "level": "INFO",
      "destinations": ["serial", "file"]
    }
  }
}
```

```http
PUT /api/v1/system/config
```

**Request Body:**
```json
{
  "network": {
    "wifi": {
      "ssid": "New-Network",
      "password": "secure-password"
    }
  }
}
```

### 3.4 System Restart
```http
POST /api/v1/system/restart
```

**Request Body:**
```json
{
  "delay": 5000,
  "reason": "Configuration change"
}
```

### 3.5 System Logs
```http
GET /api/v1/system/logs?level=INFO&limit=100&offset=0
```

**Query Parameters:**
- `level` - Log level filter (DEBUG, INFO, WARNING, ERROR, CRITICAL)
- `limit` - Number of entries (default: 100, max: 1000)
- `offset` - Pagination offset (default: 0)
- `component` - Filter by component ID
- `startTime` - Start timestamp (Unix ms)
- `endTime` - End timestamp (Unix ms)

**Response:**
```json
{
  "success": true,
  "data": {
    "logs": [
      {
        "timestamp": 1706198400000,
        "level": "INFO",
        "component": "orchestrator",
        "message": "System initialized successfully",
        "context": {}
      }
    ],
    "pagination": {
      "limit": 100,
      "offset": 0,
      "total": 1523
    }
  }
}
```

## 4. Component Endpoints

### 4.1 List Components
```http
GET /api/v1/components?type=sensor&state=active
```

**Query Parameters:**
- `type` - Filter by component type
- `state` - Filter by state (active, error, disabled)
- `tag` - Filter by tag

**Response:**
```json
{
  "success": true,
  "data": {
    "components": [
      {
        "id": "dht22-1",
        "name": "Room Temperature",
        "type": "sensor",
        "subtype": "DHT22",
        "state": "active",
        "enabled": true,
        "version": "1.0.0",
        "tags": ["temperature", "humidity", "indoor"],
        "lastExecution": 1706198395000,
        "nextExecution": 1706198400000
      }
    ],
    "total": 12
  }
}
```

### 4.2 Get Component Details
```http
GET /api/v1/components/{componentId}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "dht22-1",
    "name": "Room Temperature",
    "type": "sensor",
    "subtype": "DHT22",
    "state": "active",
    "enabled": true,
    "version": "1.0.0",
    "description": "Temperature and humidity sensor for room monitoring",
    "configuration": {
      "pin": 22,
      "samplingRate": 5000,
      "unit": "celsius"
    },
    "status": {
      "lastReading": {
        "temperature": 22.5,
        "humidity": 45.2,
        "timestamp": 1706198395000
      },
      "quality": "good",
      "errors": 0
    },
    "schedule": {
      "interval": 5000,
      "priority": 5
    },
    "resources": {
      "memory": 2048,
      "gpios": [22]
    },
    "statistics": {
      "executions": 720,
      "errors": 2,
      "averageExecutionTime": 45,
      "uptime": 3600000
    }
  }
}
```

### 4.3 Create Component
```http
POST /api/v1/components
```

**Request Body:**
```json
{
  "type": "sensor",
  "subtype": "DHT22",
  "name": "Outdoor Temperature",
  "configuration": {
    "pin": 23,
    "samplingRate": 10000
  },
  "tags": ["outdoor", "weather"]
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "dht22-2",
    "message": "Component created successfully"
  }
}
```

### 4.4 Update Component
```http
PUT /api/v1/components/{componentId}
```

**Request Body:**
```json
{
  "name": "Updated Name",
  "enabled": false,
  "configuration": {
    "samplingRate": 15000
  }
}
```

### 4.5 Delete Component
```http
DELETE /api/v1/components/{componentId}
```

### 4.6 Execute Component Now
```http
POST /api/v1/components/{componentId}/execute
```

**Request Body:**
```json
{
  "parameters": {
    "force": true
  },
  "timeout": 5000
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "result": {
      "temperature": 22.8,
      "humidity": 44.5
    },
    "executionTime": 42,
    "timestamp": 1706198400000
  }
}
```

### 4.7 Get Component Execution Options
```http
GET /api/v1/components/{componentId}/options
```

**Response:**
```json
{
  "success": true,
  "data": {
    "commands": [
      {
        "name": "read",
        "description": "Read sensor values",
        "parameters": []
      },
      {
        "name": "calibrate",
        "description": "Calibrate sensor",
        "parameters": [
          {
            "name": "offset",
            "type": "number",
            "required": false,
            "default": 0
          }
        ]
      }
    ]
  }
}
```

### 4.8 Component State Control
```http
POST /api/v1/components/{componentId}/state
```

**Request Body:**
```json
{
  "action": "enable"  // enable, disable, restart
}
```

## 5. Component Schema Endpoints

### 5.1 List Available Component Types
```http
GET /api/v1/schemas/components
```

**Response:**
```json
{
  "success": true,
  "data": {
    "schemas": [
      {
        "type": "sensor",
        "subtype": "DHT22",
        "version": "1.0.0",
        "description": "Temperature and humidity sensor",
        "configSchema": {
          "$schema": "http://json-schema.org/draft-07/schema#",
          "type": "object",
          "properties": {
            "pin": {
              "type": "integer",
              "minimum": 0,
              "maximum": 39
            },
            "samplingRate": {
              "type": "integer",
              "minimum": 1000
            }
          }
        }
      }
    ]
  }
}
```

### 5.2 Get Component Schema
```http
GET /api/v1/schemas/components/{type}/{subtype}
```

## 6. Data Endpoints

### 6.1 Get Component Data
```http
GET /api/v1/data/{componentId}?start=1706194800000&end=1706198400000&interval=60000
```

**Query Parameters:**
- `start` - Start timestamp (Unix ms)
- `end` - End timestamp (Unix ms)
- `interval` - Data aggregation interval (ms)
- `metrics` - Comma-separated list of metrics

**Response:**
```json
{
  "success": true,
  "data": {
    "componentId": "dht22-1",
    "metrics": {
      "temperature": [
        {"timestamp": 1706194800000, "value": 22.1},
        {"timestamp": 1706194860000, "value": 22.3}
      ],
      "humidity": [
        {"timestamp": 1706194800000, "value": 45.0},
        {"timestamp": 1706194860000, "value": 44.8}
      ]
    },
    "statistics": {
      "temperature": {
        "min": 21.8,
        "max": 23.1,
        "average": 22.4,
        "count": 60
      }
    }
  }
}
```

### 6.2 Stream Component Data (WebSocket)
```
ws://{device-ip}/api/v1/data/stream
```

**Subscribe Message:**
```json
{
  "action": "subscribe",
  "componentIds": ["dht22-1", "led-1"],
  "metrics": ["temperature", "state"]
}
```

**Data Message:**
```json
{
  "type": "data",
  "componentId": "dht22-1",
  "metric": "temperature",
  "value": 22.5,
  "timestamp": 1706198400000
}
```

## 7. GPIO Endpoints

### 7.1 Get GPIO Status
```http
GET /api/v1/gpio/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "pins": [
      {
        "pin": 22,
        "mode": "input",
        "state": "high",
        "allocatedTo": "dht22-1",
        "pull": "none"
      },
      {
        "pin": 2,
        "mode": "output",
        "state": "low",
        "allocatedTo": "led-1",
        "pwm": {
          "enabled": true,
          "frequency": 5000,
          "dutyCycle": 128
        }
      }
    ]
  }
}
```

### 7.2 Configure GPIO
```http
PUT /api/v1/gpio/{pin}
```

**Request Body:**
```json
{
  "mode": "output",
  "state": "high",
  "pull": "up"
}
```

## 8. Event Endpoints

### 8.1 Get System Events
```http
GET /api/v1/events?type=system&limit=50
```

**Response:**
```json
{
  "success": true,
  "data": {
    "events": [
      {
        "id": "evt-123",
        "type": "component.state_changed",
        "timestamp": 1706198400000,
        "source": {
          "type": "component",
          "id": "dht22-1"
        },
        "data": {
          "oldState": "inactive",
          "newState": "active"
        }
      }
    ]
  }
}
```

### 8.2 Subscribe to Events (WebSocket)
```
ws://{device-ip}/api/v1/events/stream
```

**Subscribe Message:**
```json
{
  "action": "subscribe",
  "types": ["component.state_changed", "system.error"],
  "components": ["dht22-1"]
}
```

## 9. File Management Endpoints

### 9.1 List Files
```http
GET /api/v1/files?path=/config
```

**Response:**
```json
{
  "success": true,
  "data": {
    "path": "/config",
    "files": [
      {
        "name": "system.json",
        "size": 1024,
        "modified": 1706198400000,
        "type": "file"
      },
      {
        "name": "components",
        "type": "directory"
      }
    ]
  }
}
```

### 9.2 Upload File
```http
POST /api/v1/files
Content-Type: multipart/form-data
```

**Form Data:**
- `file` - File content
- `path` - Target path

### 9.3 Download File
```http
GET /api/v1/files/download?path=/config/system.json
```

### 9.4 Delete File
```http
DELETE /api/v1/files?path=/config/old.json
```

## 10. OTA Update Endpoints

### 10.1 Check for Updates
```http
GET /api/v1/ota/check
```

**Response:**
```json
{
  "success": true,
  "data": {
    "updateAvailable": true,
    "currentVersion": "1.0.0",
    "latestVersion": "1.1.0",
    "releaseNotes": "Bug fixes and performance improvements",
    "size": 1048576,
    "url": "https://updates.example.com/firmware-1.1.0.bin"
  }
}
```

### 10.2 Start Update
```http
POST /api/v1/ota/update
```

**Request Body:**
```json
{
  "url": "https://updates.example.com/firmware-1.1.0.bin",
  "checksum": "sha256:abc123...",
  "autoRestart": true
}
```

### 10.3 Get Update Status
```http
GET /api/v1/ota/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "downloading",
    "progress": 45,
    "bytesDownloaded": 471859,
    "totalBytes": 1048576,
    "estimatedTime": 30
  }
}
```

## 11. Batch Operations

### 11.1 Batch Request
```http
POST /api/v1/batch
```

**Request Body:**
```json
{
  "requests": [
    {
      "id": "req-1",
      "method": "GET",
      "path": "/api/v1/components/dht22-1"
    },
    {
      "id": "req-2",
      "method": "POST",
      "path": "/api/v1/components/led-1/execute",
      "body": {"parameters": {"state": "on"}}
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "responses": [
      {
        "id": "req-1",
        "status": 200,
        "body": {
          "success": true,
          "data": {}
        }
      },
      {
        "id": "req-2",
        "status": 200,
        "body": {
          "success": true,
          "data": {}
        }
      }
    ]
  }
}
```

## 12. Rate Limiting

### 12.1 Rate Limit Headers
```http
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1706199000
```

### 12.2 Rate Limit Response
```json
{
  "success": false,
  "error": {
    "code": "RATE_LIMIT_EXCEEDED",
    "message": "Too many requests",
    "details": {
      "limit": 100,
      "reset": 1706199000,
      "retryAfter": 600
    }
  }
}
```

## 13. Error Codes

| Code | Description |
|------|-------------|
| `INVALID_REQUEST` | Request validation failed |
| `UNAUTHORIZED` | Authentication required |
| `FORBIDDEN` | Insufficient permissions |
| `NOT_FOUND` | Resource not found |
| `CONFLICT` | Resource conflict |
| `COMPONENT_NOT_FOUND` | Component does not exist |
| `COMPONENT_ERROR` | Component execution error |
| `COMPONENT_BUSY` | Component is busy |
| `CONFIGURATION_ERROR` | Invalid configuration |
| `VALIDATION_ERROR` | Data validation failed |
| `RATE_LIMIT_EXCEEDED` | Too many requests |
| `INTERNAL_ERROR` | Internal server error |
| `SERVICE_UNAVAILABLE` | Service temporarily unavailable |

## 14. WebSocket Protocol

### 14.1 Connection
```javascript
const ws = new WebSocket('ws://192.168.1.100/api/v1/ws');
```

### 14.2 Authentication
```json
{
  "type": "auth",
  "token": "bearer-token"
}
```

### 14.3 Message Types

#### Subscribe
```json
{
  "type": "subscribe",
  "channel": "components",
  "filters": {
    "componentIds": ["dht22-1"],
    "events": ["data", "state"]
  }
}
```

#### Unsubscribe
```json
{
  "type": "unsubscribe",
  "channel": "components"
}
```

#### Command
```json
{
  "type": "command",
  "target": "dht22-1",
  "action": "execute",
  "parameters": {}
}
```

#### Data Event
```json
{
  "type": "event",
  "channel": "components",
  "event": "data",
  "data": {
    "componentId": "dht22-1",
    "temperature": 22.5,
    "timestamp": 1706198400000
  }
}
```

### 14.4 Heartbeat
```json
{
  "type": "ping"
}
```

Response:
```json
{
  "type": "pong",
  "timestamp": 1706198400000
}
```

## 15. API Versioning

### 15.1 Version Header
```http
API-Version: 1.0.0
```

### 15.2 Version in URL
```
/api/v1/...
/api/v2/...
```

### 15.3 Version Negotiation
```http
Accept: application/vnd.api+json;version=1.0
```

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-01-25  
**Status**: Authoritative  
**OpenAPI Spec**: Available at `/api/v1/openapi.json`