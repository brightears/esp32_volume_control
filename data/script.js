document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM fully loaded and parsed');
    const form = document.getElementById('config-form');
    const statusMessage = document.getElementById('status-message');
    const sensitivitySlider = document.getElementById('sensitivity');
    const sensitivityValue = document.getElementById('sensitivity-value');
    const debugOutput = document.getElementById('debug-output');

    // Load stored configuration from ESP32
    loadStoredConfig();
    
    // Initialize sensitivity slider
    if (sensitivitySlider && sensitivityValue) {
        sensitivitySlider.addEventListener('input', function() {
            sensitivityValue.textContent = this.value;
        });
    }

    // Form submission handler
    if (form) {
        form.addEventListener('submit', handleFormSubmission);
        
        // Add input event listeners for API fields
        ['api-url', 'client-id', 'client-secret', 'sound-zone'].forEach(field => {
            const input = document.getElementById(field);
            if (input) {
                input.addEventListener('change', debounce(testAPIConnection, 1000));
                input.addEventListener('input', () => {
                    // Clear "current" indicator when value changes
                    const label = input.previousElementSibling;
                    if (label && label.querySelector('.current-value')) {
                        label.querySelector('.current-value').remove();
                    }
                });
            }
        });

        // Add change listeners to clear current indicators
        ['ssid', 'password'].forEach(field => {
            const input = document.getElementById(field);
            if (input) {
                input.addEventListener('input', () => {
                    const label = input.previousElementSibling;
                    if (label && label.querySelector('.current-value')) {
                        label.querySelector('.current-value').remove();
                    }
                });
            }
        });
    } else {
        console.error('Form element not found');
        showStatus('Error: Form not found', 'error');
    }
});

// Debounce function to prevent too many API calls
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

// New function to load stored configuration from ESP32
async function loadStoredConfig() {
    try {
        const response = await fetch('/get-stored-config');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const config = await response.json();
        debugLog('Loaded stored configuration');

        // Populate form fields with stored values
        if (config.ssid) {
            const ssidInput = document.getElementById('ssid');
            ssidInput.value = config.ssid;
            ssidInput.setAttribute('data-current', config.ssid);
            debugLog(`Current SSID: ${config.ssid}`);
        }
        
        if (config["api-url"]) {
            const apiUrlInput = document.getElementById('api-url');
            apiUrlInput.value = config["api-url"];
            apiUrlInput.setAttribute('data-current', config["api-url"]);
        }
        
        if (config["client-id"]) {
            const clientIdInput = document.getElementById('client-id');
            clientIdInput.value = config["client-id"];
            clientIdInput.setAttribute('data-current', config["client-id"]);
        }
        
        if (config["sound-zone"]) {
            const soundZoneInput = document.getElementById('sound-zone');
            soundZoneInput.value = config["sound-zone"];
            soundZoneInput.setAttribute('data-current', config["sound-zone"]);
        }
        
        if (config.sensitivity !== undefined) {
            const sensitivitySlider = document.getElementById('sensitivity');
            const sensitivityValue = document.getElementById('sensitivity-value');
            if (sensitivitySlider && sensitivityValue) {
                sensitivitySlider.value = config.sensitivity;
                sensitivityValue.textContent = config.sensitivity;
            }
        }

        // Update connection status if SSID is present
        if (config.ssid) {
            updateConnectionStatus(true);
            document.querySelector('.status-text').textContent = `Connected to: ${config.ssid}`;
            
            // Add "Currently Connected" labels
            document.querySelectorAll('[data-current]').forEach(input => {
                const label = input.previousElementSibling;
                if (label && label.tagName === 'LABEL') {
                    if (!label.querySelector('.current-value')) {
                        const currentValue = document.createElement('span');
                        currentValue.className = 'current-value';
                        currentValue.textContent = ` (Current: ${input.getAttribute('data-current')})`;
                        label.appendChild(currentValue);
                    }
                }
            });
        }

    } catch (error) {
        console.error('Error loading stored configuration:', error);
        debugLog('Error loading stored configuration: ' + error.message);
    }
}
// Save form data to localStorage
function saveFormData() {
    const form = document.getElementById('config-form');
    const formData = new FormData(form);
    const data = Object.fromEntries(formData.entries());
    localStorage.setItem('formData', JSON.stringify(data));
    debugLog('Form data saved to localStorage');
}

// Load form data from localStorage
function loadStoredFormData() {
    const storedData = localStorage.getItem('formData');
    if (storedData) {
        const data = JSON.parse(storedData);
        Object.entries(data).forEach(([key, value]) => {
            const input = document.getElementById(key);
            if (input) {
                input.value = value;
            }
        });
        debugLog('Form data restored from localStorage');
    }
}

async function testAPIConnection() {
    updateConnectionStatus('testing');
    debugLog('Testing API connection...');
    
    const apiUrl = document.getElementById('api-url')?.value;
    const clientId = document.getElementById('client-id')?.value;
    const clientSecret = document.getElementById('client-secret')?.value;
    const soundZone = document.getElementById('sound-zone')?.value;
    
    // Only test if all fields are filled
    if (!apiUrl || !clientId || !clientSecret || !soundZone) {
        updateConnectionStatus(false);
        debugLog('Missing required API credentials');
        return;
    }

    try {
        const response = await fetch('/test-connection', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: new URLSearchParams({
                'api-url': apiUrl,
                'client-id': clientId,
                'client-secret': clientSecret,
                'sound-zone': soundZone
            }).toString()
        });
        
        debugLog(`API test response status: ${response.status}`);
        const result = await response.text();
        const success = response.ok;
        
        updateConnectionStatus(success);
        
        if (!success) {
            showStatus('API Connection test failed: ' + result, 'error');
            debugLog('API connection test failed: ' + result);
        } else {
            showStatus('API Connection successful', 'success');
            debugLog('API connection test successful');
        }
    } catch (error) {
        console.error('Error testing connection:', error);
        updateConnectionStatus(false);
        showStatus('Error testing API connection: ' + error.message, 'error');
        debugLog('Error testing API connection: ' + error.message);
    }
}

function togglePassword(inputId) {
    const input = document.getElementById(inputId);
    const button = input.nextElementSibling;
    if (input.type === 'password') {
        input.type = 'text';
        button.textContent = 'Hide';
    } else {
        input.type = 'password';
        button.textContent = 'Show';
    }
}

function showStatus(message, type) {
    const statusMessage = document.getElementById('status-message');
    if (statusMessage) {
        statusMessage.textContent = message;
        statusMessage.className = type;
        statusMessage.style.display = 'block';
    }
    debugLog(`Status [${type}]: ${message}`);
}

function debugLog(message) {
    const debugOutput = document.getElementById('debug-output');
    if (debugOutput) {
        const timestamp = new Date().toLocaleTimeString();
        debugOutput.textContent += `[${timestamp}] ${message}\n`;
        debugOutput.scrollTop = debugOutput.scrollHeight;
        console.log('Debug:', message);
    }
}

async function loadCurrentSensitivity() {
    try {
        const response = await fetch('/get-sensitivity');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const sensitivity = await response.text();
        debugLog('Loaded sensitivity: ' + sensitivity);
        
        const sensitivitySlider = document.getElementById('sensitivity');
        const sensitivityValue = document.getElementById('sensitivity-value');
        if (sensitivitySlider && sensitivityValue) {
            sensitivitySlider.value = sensitivity;
            sensitivityValue.textContent = sensitivity;
        }
    } catch (error) {
        console.error('Error loading sensitivity:', error);
        showStatus('Error loading current sensitivity', 'error');
        debugLog('Error loading sensitivity: ' + error.message);
    }
}
async function handleFormSubmission(event) {
    event.preventDefault();
    debugLog('Form submission started');
    showStatus('Saving configuration...', 'loading');

    const form = event.target;
    const submitButton = form.querySelector('.submit-btn');
    if (submitButton) {
        submitButton.classList.add('loading');
        submitButton.disabled = true;
    }

    try {
        const formData = new FormData(form);
        debugLog('Preparing form data for submission');

        // Validate all required fields
        const requiredFields = ['ssid', 'password', 'api-url', 'client-id', 'client-secret', 'sound-zone', 'sensitivity'];
        for (const field of requiredFields) {
            const value = formData.get(field);
            if (!value || value.trim() === '') {
                throw new Error(`Missing required field: ${field}`);
            }
        }

        // Log the attempt (without sensitive data)
        debugLog(`Attempting to save configuration with SSID: ${formData.get('ssid')}`);

        const response = await fetch('/save', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: new URLSearchParams(formData).toString()
        });

        debugLog(`Server response status: ${response.status}`);
        const result = await response.text();
        debugLog(`Server response: ${result}`);

        if (response.ok) {
            showStatus('Configuration saved successfully. Device will restart...', 'success');
            saveFormData();
            debugLog('Configuration saved. Waiting for device restart...');
            
            // Update stored config indicators
            const inputs = form.querySelectorAll('input[type="text"], input[type="password"]');
            inputs.forEach(input => {
                input.setAttribute('data-current', input.value);
            });
            
            // Wait for device restart
            setTimeout(() => {
                window.location.reload();
            }, 5000);
        } else {
            throw new Error(result || 'Failed to save configuration');
        }
    } catch (error) {
        console.error('Error:', error);
        showStatus('Error saving configuration: ' + error.message, 'error');
        debugLog('Error saving configuration: ' + error.message);
    } finally {
        if (submitButton) {
            submitButton.classList.remove('loading');
            submitButton.disabled = false;
        }
    }
}

async function resetDevice() {
    if (confirm('Are you sure you want to reset the device? This will clear all configuration.')) {
        showStatus('Resetting device...', 'warning');
        debugLog('Device reset initiated');
        try {
            const response = await fetch('/reset');
            if (response.ok) {
                localStorage.removeItem('formData');
                showStatus('Device is resetting. Please wait...', 'success');
                debugLog('Device reset successful. Waiting for restart...');
                setTimeout(() => {
                    window.location.reload();
                }, 5000);
            } else {
                throw new Error('Failed to reset device');
            }
        } catch (error) {
            console.error('Error:', error);
            showStatus('Error resetting device: ' + error.message, 'error');
            debugLog('Error resetting device: ' + error.message);
        }
    }
}

function updateConnectionStatus(status) {
    const statusIndicator = document.querySelector('.status-indicator');
    const statusText = document.querySelector('.status-text');
    if (statusIndicator && statusText) {
        if (status === 'testing') {
            statusIndicator.className = 'status-indicator testing';
            statusText.textContent = 'Testing connection...';
            debugLog('Connection status: Testing');
        } else if (status === true) {
            statusIndicator.className = 'status-indicator connected';
            statusText.textContent = 'Connected';
            debugLog('Connection status: Connected');
        } else {
            statusIndicator.className = 'status-indicator disconnected';
            statusText.textContent = 'Disconnected';
            debugLog('Connection status: Disconnected');
        }
    }
}

// Add window load event listener
window.addEventListener('load', () => {
    debugLog('Page loaded');
    loadStoredConfig();
    loadCurrentSensitivity();
});

