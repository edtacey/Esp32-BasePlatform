// ESP32 IoT Orchestrator JavaScript - Enhanced with Data Filtering

// Auto-refresh interval (30 seconds)
const REFRESH_INTERVAL = 30000;
let refreshTimer = null;
let currentDataMode = 'core';

// Initialize dashboard
document.addEventListener('DOMContentLoaded', function() {
    setupEventListeners();
    loadComponents();
    startAutoRefresh();
});

// Set up event listeners for controls
function setupEventListeners() {
    // Data mode radio buttons
    const radioButtons = document.querySelectorAll('input[name="dataMode"]');
    radioButtons.forEach(radio => {
        radio.addEventListener('change', function() {
            if (this.checked) {
                currentDataMode = this.value;
                loadComponents();
            }
        });
    });
    
    // Refresh button
    const refreshBtn = document.getElementById('refreshBtn');
    if (refreshBtn) {
        refreshBtn.addEventListener('click', loadComponents);
    }
    
    // Auto-refresh toggle
    const autoRefreshCheckbox = document.getElementById('autoRefresh');
    if (autoRefreshCheckbox) {
        autoRefreshCheckbox.addEventListener('change', function() {
            if (this.checked) {
                startAutoRefresh();
            } else {
                stopAutoRefresh();
            }
        });
    }
}

// Load components data with filtering
function loadComponents() {
    const container = document.getElementById('components');
    container.innerHTML = '<div class="loading">Loading components...</div>';
    
    // Build API URL with filter parameter
    let apiUrl = '/api/components/data';
    if (currentDataMode !== 'full') {
        apiUrl += '?filter=' + currentDataMode;
    }
    
    console.log('Loading components with mode:', currentDataMode, 'URL:', apiUrl);
    
    fetch(apiUrl)
        .then(response => {
            if (!response.ok) {
                throw new Error('HTTP ' + response.status);
            }
            return response.json();
        })
        .then(data => {
            displayComponents(data.components || [], currentDataMode);
        })
        .catch(error => {
            console.error('Error loading components:', error);
            container.innerHTML = 
                '<div class="error">Error loading components: ' + error.message + '</div>';
        });
}

// Display components in grid
function displayComponents(components, dataMode) {
    const container = document.getElementById('components');
    
    if (components.length === 0) {
        container.innerHTML = '<div class="loading">No components found</div>';
        return;
    }
    
    console.log('Displaying', components.length, 'components in mode:', dataMode);
    
    let html = '';
    components.forEach(comp => {
        html += createComponentCard(comp, dataMode);
    });
    
    container.innerHTML = html;
}

// Create individual component card
function createComponentCard(comp, dataMode) {
    const stateClass = getStateClass(comp.state);
    const hasData = comp.output_data && Object.keys(comp.output_data).length > 0;
    
    let card = `
        <div class="component-card ${stateClass}">
            <div class="component-header">
                <strong>${comp.name || comp.id}</strong>
                <span class="component-id">${comp.id} (${comp.type})</span>
            </div>
            <div class="component-status">${comp.state}</div>
    `;
    
    if (hasData) {
        card += '<div class="component-data">';
        
        if (dataMode === 'core') {
            // Core mode: Show only essential sensor values
            card += formatCoreData(comp.output_data);
        } else if (dataMode === 'diagnostics') {
            // Diagnostics mode: Show all data including debug info
            card += formatDiagnosticData(comp);
        } else {
            // Full mode: Show all output data
            card += formatAllData(comp.output_data);
        }
        
        card += '</div>';
    } else {
        card += '<div class="no-data">No data available</div>';
    }
    
    // Show execution count if available
    if (comp.executions !== undefined || comp.execution_count !== undefined) {
        const execCount = comp.executions || comp.execution_count || 0;
        card += `<div class="execution-count">Executions: ${execCount}</div>`;
    }
    
    card += '</div>';
    return card;
}

// Format core data (essential sensor values only)
function formatCoreData(data) {
    if (!data) return '<div class="no-data">No core data</div>';
    
    console.log('formatCoreData called with data:', Object.keys(data));
    
    let html = '';
    
    // Priority order for core sensor values
    const coreFields = [
        { key: 'temperature', label: 'Temperature', unit: '°C' },
        { key: 'humidity', label: 'Humidity', unit: '%' },
        { key: 'ph', label: 'pH', unit: '' },
        { key: 'current_ph', label: 'pH', unit: '' },
        { key: 'ec', label: 'EC', unit: 'μS/cm' },
        { key: 'current_ec', label: 'EC', unit: 'μS/cm' },
        { key: 'tds', label: 'TDS', unit: 'ppm' },
        { key: 'current_tds', label: 'TDS', unit: 'ppm' },
        { key: 'lux', label: 'Light', unit: 'lux' },
        { key: 'par_umol', label: 'PAR', unit: 'μmol/m²/s' },
        { key: 'ppfd_category', label: 'PPFD Category', unit: '' },
        { key: 'voltage', label: 'Voltage', unit: 'V' },
        { key: 'success', label: 'Status', unit: '' },
        { key: 'calibrated', label: 'Calibrated', unit: '' },
        { key: 'timestamp', label: 'Timestamp', unit: '' }
    ];
    
    coreFields.forEach(field => {
        if (data[field.key] !== undefined && data[field.key] !== null) {
            let value = data[field.key];
            
            if (field.key === 'success') {
                value = value ? '✅ OK' : '❌ Error';
            } else if (field.key === 'calibrated') {
                value = value ? '✅ Yes' : '❌ No';
            } else if (typeof value === 'number') {
                value = value.toFixed(2);
            }
            
            html += `<div class="data-item sensor">${field.label}: <strong>${value}${field.unit}</strong></div>`;
        }
    });
    
    if (html === '') {
        html = '<div class="no-data">No core sensor values available</div>';
    }
    
    console.log('formatCoreData result:', html);
    return html;
}

// Format all output data (full mode)
function formatAllData(data) {
    if (!data) return '<div class="no-data">No data</div>';
    
    let html = '';
    Object.keys(data).forEach(key => {
        let value = data[key];
        if (typeof value === 'object') {
            value = JSON.stringify(value);
        } else if (typeof value === 'number') {
            value = value.toFixed(3);
        } else if (typeof value === 'boolean') {
            value = value ? 'true' : 'false';
        }
        
        html += `<div class="data-item">${key}: <strong>${value}</strong></div>`;
    });
    
    return html;
}

// Format diagnostic data (includes component metadata)
function formatDiagnosticData(comp) {
    let html = formatAllData(comp.output_data);
    
    // Add diagnostic information
    html += '<div class="diagnostic-info">';
    if (comp.execution_count !== undefined) {
        html += `<div class="diag-item">Executions: ${comp.execution_count}</div>`;
    }
    if (comp.error_count !== undefined) {
        html += `<div class="diag-item">Errors: ${comp.error_count}</div>`;
    }
    if (comp.last_execution_ms) {
        html += `<div class="diag-item">Last Exec: ${comp.last_execution_ms}ms</div>`;
    }
    if (comp.last_error) {
        html += `<div class="diag-item error">Last Error: ${comp.last_error}</div>`;
    }
    html += '</div>';
    
    return html;
}

// Get CSS class for component state
function getStateClass(state) {
    switch(state?.toLowerCase()) {
        case 'ready': return 'state-ready';
        case 'executing': return 'state-executing';
        case 'error': return 'state-error';
        case 'initializing': return 'state-initializing';
        default: return 'state-unknown';
    }
}

// Auto-refresh functions
function startAutoRefresh() {
    if (refreshTimer) clearInterval(refreshTimer);
    refreshTimer = setInterval(loadComponents, REFRESH_INTERVAL);
}

function stopAutoRefresh() {
    if (refreshTimer) {
        clearInterval(refreshTimer);
        refreshTimer = null;
    }
}

// Utility functions
function formatTimestamp(timestamp) {
    if (!timestamp) return 'N/A';
    
    const date = new Date(timestamp * 1000);
    return date.toLocaleTimeString();
}

console.log('ESP32 Dashboard with Data Filtering loaded');