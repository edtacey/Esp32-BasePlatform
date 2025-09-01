/**
 * @file HttpClientWrapper.cpp
 * @brief Smart HTTP client wrapper implementation
 */

#include "HttpClientWrapper.h"

HttpClientWrapper::HttpClientWrapper() {
    log(Logger::DEBUG, "HttpClientWrapper initialized");
}

HttpClientWrapper::~HttpClientWrapper() {
    m_httpClient.end();
}

HttpResult HttpClientWrapper::get(const String& url, uint32_t timeoutMs) {
    HttpResult result;
    
    // Check if URL is in backoff period
    if (isInBackoff(url)) {
        result.success = false;
        result.shouldDefer = true;
        result.nextRetryMs = getNextRetryTime(url);
        result.error = "URL in backoff period";
        
        uint32_t waitSeconds = (result.nextRetryMs - millis()) / 1000;
        log(Logger::INFO, "🚫 " + url + " in backoff, retry in " + String(waitSeconds) + "s");
        return result;
    }
    
    // Use default timeout if not specified
    if (timeoutMs == 0) {
        timeoutMs = m_defaultTimeoutMs;
    }
    
    log(Logger::DEBUG, "🌐 HTTP GET: " + url);
    
    // Configure HTTP client
    m_httpClient.begin(url);
    m_httpClient.setTimeout(timeoutMs);
    m_httpClient.setConnectTimeout(timeoutMs / 2);
    
    // Perform request
    int httpCode = m_httpClient.GET();
    result.httpCode = httpCode;
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            result.success = true;
            result.response = m_httpClient.getString();
            result.shouldDefer = false;
            
            // Reset failure tracking on success
            resetFailures(url);
            log(Logger::DEBUG, "✅ HTTP GET success: " + url + " (" + String(result.response.length()) + " bytes)");
            
        } else {
            result.success = false;
            result.error = "HTTP " + String(httpCode);
            recordFailure(url);
            
            log(Logger::WARNING, "⚠️ HTTP GET failed: " + url + " - HTTP " + String(httpCode));
        }
    } else {
        result.success = false;
        result.error = "Connection failed: " + String(httpCode);
        recordFailure(url);
        
        log(Logger::ERROR, "❌ HTTP GET connection failed: " + url + " - Error: " + String(httpCode));
    }
    
    // Set retry information if failed
    if (!result.success) {
        result.nextRetryMs = getNextRetryTime(url);
        result.shouldDefer = (result.nextRetryMs > millis());
    }
    
    m_httpClient.end();
    return result;
}

HttpResult HttpClientWrapper::post(const String& url, const String& payload, const String& contentType, uint32_t timeoutMs) {
    HttpResult result;
    
    // Check if URL is in backoff period
    if (isInBackoff(url)) {
        result.success = false;
        result.shouldDefer = true;
        result.nextRetryMs = getNextRetryTime(url);
        result.error = "URL in backoff period";
        
        uint32_t waitSeconds = (result.nextRetryMs - millis()) / 1000;
        log(Logger::INFO, "🚫 " + url + " in backoff, retry in " + String(waitSeconds) + "s");
        return result;
    }
    
    // Use default timeout if not specified
    if (timeoutMs == 0) {
        timeoutMs = m_defaultTimeoutMs;
    }
    
    log(Logger::DEBUG, "🌐 HTTP POST: " + url + " (" + String(payload.length()) + " bytes)");
    
    // Configure HTTP client
    m_httpClient.begin(url);
    m_httpClient.setTimeout(timeoutMs);
    m_httpClient.setConnectTimeout(timeoutMs / 2);
    m_httpClient.addHeader("Content-Type", contentType);
    
    // Perform request
    int httpCode = m_httpClient.POST(payload);
    result.httpCode = httpCode;
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            result.success = true;
            result.response = m_httpClient.getString();
            result.shouldDefer = false;
            
            // Reset failure tracking on success
            resetFailures(url);
            log(Logger::DEBUG, "✅ HTTP POST success: " + url + " - HTTP " + String(httpCode));
            
        } else {
            result.success = false;
            result.error = "HTTP " + String(httpCode);
            recordFailure(url);
            
            log(Logger::WARNING, "⚠️ HTTP POST failed: " + url + " - HTTP " + String(httpCode));
        }
    } else {
        result.success = false;
        result.error = "Connection failed: " + String(httpCode);
        recordFailure(url);
        
        log(Logger::ERROR, "❌ HTTP POST connection failed: " + url + " - Error: " + String(httpCode));
    }
    
    // Set retry information if failed
    if (!result.success) {
        result.nextRetryMs = getNextRetryTime(url);
        result.shouldDefer = (result.nextRetryMs > millis());
    }
    
    m_httpClient.end();
    return result;
}

bool HttpClientWrapper::isInBackoff(const String& url) const {
    auto it = m_urlFailures.find(url);
    if (it == m_urlFailures.end()) {
        return false;
    }
    
    const UrlFailureInfo& info = it->second;
    return info.inBackoff && (millis() < info.nextRetryMs);
}

uint32_t HttpClientWrapper::getNextRetryTime(const String& url) const {
    auto it = m_urlFailures.find(url);
    if (it == m_urlFailures.end()) {
        return 0; // Can retry immediately
    }
    
    return it->second.nextRetryMs;
}

void HttpClientWrapper::resetFailures(const String& url) {
    auto it = m_urlFailures.find(url);
    if (it != m_urlFailures.end()) {
        log(Logger::INFO, "✅ Resetting failure tracking for: " + url);
        m_urlFailures.erase(it);
    }
}

JsonDocument HttpClientWrapper::getFailureStats() const {
    JsonDocument stats;
    JsonArray urlArray = stats["failed_urls"].to<JsonArray>();
    
    for (const auto& pair : m_urlFailures) {
        JsonObject urlInfo = urlArray.add<JsonObject>();
        urlInfo["url"] = pair.first;
        urlInfo["failure_count"] = pair.second.failureCount;
        urlInfo["last_failure_ms"] = pair.second.lastFailureMs;
        urlInfo["next_retry_ms"] = pair.second.nextRetryMs;
        urlInfo["backoff_ms"] = pair.second.backoffMs;
        urlInfo["in_backoff"] = pair.second.inBackoff;
        
        if (pair.second.inBackoff && pair.second.nextRetryMs > millis()) {
            urlInfo["retry_in_seconds"] = (pair.second.nextRetryMs - millis()) / 1000;
        }
    }
    
    stats["total_failed_urls"] = m_urlFailures.size();
    stats["timestamp"] = millis();
    
    return stats;
}

void HttpClientWrapper::recordFailure(const String& url) {
    UrlFailureInfo& info = m_urlFailures[url];
    info.failureCount++;
    info.lastFailureMs = millis();
    
    updateFailureTracking(url, info);
    
    log(Logger::WARNING, "📊 URL failure recorded: " + url + 
        " (attempts: " + String(info.failureCount) + 
        ", next retry: " + String((info.nextRetryMs - millis()) / 1000) + "s)");
}

uint32_t HttpClientWrapper::calculateBackoff(uint32_t failureCount) const {
    if (failureCount <= 1) {
        return INITIAL_BACKOFF_MS;  // 1 minute
    } else if (failureCount <= 3) {
        return MED_BACKOFF_MS;      // 10 minutes
    } else if (failureCount <= 5) {
        return LONG_BACKOFF_MS;     // 20 minutes
    } else {
        return MAX_BACKOFF_MS;      // 60 minutes (max)
    }
}

void HttpClientWrapper::updateFailureTracking(const String& url, UrlFailureInfo& info) {
    uint32_t currentTime = millis();
    
    // Calculate backoff based on failure count
    info.backoffMs = calculateBackoff(info.failureCount);
    info.nextRetryMs = currentTime + info.backoffMs;
    info.inBackoff = true;
    
    // Log backoff information
    String backoffDesc;
    if (info.backoffMs >= 3600000) {
        backoffDesc = String(info.backoffMs / 60000) + " minutes";
    } else if (info.backoffMs >= 60000) {
        backoffDesc = String(info.backoffMs / 60000) + " minutes";
    } else {
        backoffDesc = String(info.backoffMs / 1000) + " seconds";
    }
    
    log(Logger::INFO, "⏰ URL " + url + " backed off for " + backoffDesc + 
        " (failure #" + String(info.failureCount) + ")");
        
    // Auto-cleanup: Remove very old failures after max backoff period
    if (info.failureCount > MAX_ATTEMPTS && 
        (currentTime - info.lastFailureMs) > (MAX_BACKOFF_MS * 2)) {
        log(Logger::INFO, "🧹 Auto-cleanup old failure tracking for: " + url);
        m_urlFailures.erase(url);
    }
}

void HttpClientWrapper::log(Logger::Level level, const String& message) {
    switch (level) {
        case Logger::DEBUG:
            Logger::debug("HttpWrapper", message);
            break;
        case Logger::INFO:
            Logger::info("HttpWrapper", message);
            break;
        case Logger::WARNING:
            Logger::warning("HttpWrapper", message);
            break;
        case Logger::ERROR:
            Logger::error("HttpWrapper", message);
            break;
        default:
            Logger::info("HttpWrapper", message);
            break;
    }
}