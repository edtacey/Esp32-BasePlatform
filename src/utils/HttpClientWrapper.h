/**
 * @file HttpClientWrapper.h
 * @brief Smart HTTP client wrapper with retry and exponential backoff
 * 
 * Provides intelligent retry mechanisms for HTTP requests with URL-specific
 * failure tracking and exponential backoff to prevent overwhelming failed endpoints.
 */

#ifndef HTTP_CLIENT_WRAPPER_H
#define HTTP_CLIENT_WRAPPER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "../utils/Logger.h"

/**
 * @brief HTTP request result with retry information
 */
struct HttpResult {
    bool success = false;
    int httpCode = -1;
    String response = "";
    String error = "";
    uint32_t nextRetryMs = 0;  // When to retry this URL (0 = can retry immediately)
    bool shouldDefer = false;  // Component should defer execution
};

/**
 * @brief URL failure tracking information
 */
struct UrlFailureInfo {
    uint32_t failureCount = 0;
    uint32_t lastFailureMs = 0;
    uint32_t nextRetryMs = 0;
    uint32_t backoffMs = 60000;  // Start with 1 minute backoff
    bool inBackoff = false;
};

/**
 * @brief Smart HTTP client wrapper with retry and backoff logic
 */
class HttpClientWrapper {
private:
    std::map<String, UrlFailureInfo> m_urlFailures;
    HTTPClient m_httpClient;
    uint32_t m_defaultTimeoutMs = 5000;
    
    // Retry policy constants
    static const uint32_t MAX_ATTEMPTS = 5;
    static const uint32_t INITIAL_BACKOFF_MS = 60000;   // 1 minute
    static const uint32_t MED_BACKOFF_MS = 600000;      // 10 minutes  
    static const uint32_t LONG_BACKOFF_MS = 1200000;    // 20 minutes
    static const uint32_t MAX_BACKOFF_MS = 3600000;     // 60 minutes
    
public:
    /**
     * @brief Constructor
     */
    HttpClientWrapper();
    
    /**
     * @brief Destructor
     */
    ~HttpClientWrapper();
    
    /**
     * @brief Perform GET request with smart retry logic
     * @param url URL to fetch
     * @param timeoutMs Request timeout (default: 5000ms)
     * @return HttpResult with success status and retry information
     */
    HttpResult get(const String& url, uint32_t timeoutMs = 0);
    
    /**
     * @brief Perform POST request with smart retry logic
     * @param url URL to post to
     * @param payload POST data
     * @param contentType Content-Type header
     * @param timeoutMs Request timeout (default: 5000ms)  
     * @return HttpResult with success status and retry information
     */
    HttpResult post(const String& url, const String& payload, const String& contentType = "application/json", uint32_t timeoutMs = 0);
    
    /**
     * @brief Check if URL is currently in backoff period
     * @param url URL to check
     * @return true if URL should be avoided due to backoff
     */
    bool isInBackoff(const String& url) const;
    
    /**
     * @brief Get next retry time for a URL
     * @param url URL to check
     * @return Milliseconds when URL can be retried (0 = immediately)
     */
    uint32_t getNextRetryTime(const String& url) const;
    
    /**
     * @brief Reset failure tracking for a URL (call on success)
     * @param url URL to reset
     */
    void resetFailures(const String& url);
    
    /**
     * @brief Get failure statistics for debugging
     * @return JSON document with failure stats
     */
    JsonDocument getFailureStats() const;
    
    /**
     * @brief Set default timeout for requests
     * @param timeoutMs Default timeout in milliseconds
     */
    void setDefaultTimeout(uint32_t timeoutMs) { m_defaultTimeoutMs = timeoutMs; }

private:
    /**
     * @brief Record URL failure and calculate backoff
     * @param url Failed URL
     */
    void recordFailure(const String& url);
    
    /**
     * @brief Calculate backoff time based on failure count
     * @param failureCount Number of consecutive failures
     * @return Backoff time in milliseconds
     */
    uint32_t calculateBackoff(uint32_t failureCount) const;
    
    /**
     * @brief Update URL failure tracking
     * @param url URL that failed
     * @param info Failure info to update
     */
    void updateFailureTracking(const String& url, UrlFailureInfo& info);
    
    /**
     * @brief Log HTTP operation
     * @param level Log level
     * @param message Log message
     */
    void log(Logger::Level level, const String& message);
};

#endif // HTTP_CLIENT_WRAPPER_H