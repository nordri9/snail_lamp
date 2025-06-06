const toggleButton = document.getElementById('toggleButton');
const ledStatus = document.getElementById('ledStatus');
const colorSlider = document.getElementById('colorSlider');
const currentColorDisplay = document.getElementById('currentColorDisplay');
const brightnessSlider = document.getElementById('brightnessSlider'); // <<< NEW
const brightnessValueSpan = document.getElementById('brightnessValue'); // <<< NEW

// Function to convert HSV (Hue, Saturation, Value) to RGB for UI display
// Hue is 0-359. Saturation is 0-1. Value (brightness) is 0-1.
function hsvToRgb(h, s, v) { // <<< MODIFIED: Added s and v parameters
    let r, g, b;

    h /= 60; // Sector 0 to 5
    const i = Math.floor(h);
    const f = h - i;
    const p = v * (1 - s);
    const q = v * (1 - f * s);
    const t = v * (1 - (1 - f) * s);

    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    return `rgb(${Math.round(r * 255)}, ${Math.round(g * 255)}, ${Math.round(b * 255)})`;
}

// Function to update the color display element on the webpage
function updateColorDisplay() {
    const currentHue = parseInt(colorSlider.value);
    // Convert brightness slider value (0-255) to 0-1 scale for hsvToRgb function
    const currentBrightness = parseInt(brightnessSlider.value) / 255.0;
    currentColorDisplay.style.backgroundColor = hsvToRgb(currentHue, 1, currentBrightness); // S=1 for full saturation
}

// Function to update UI based on LED state (on/off)
function updateUI(state) {
    if (state) {
        ledStatus.textContent = 'ON';
        ledStatus.className = 'on';
        updateColorDisplay(); // Update color display to reflect current color
    } else {
        ledStatus.textContent = 'OFF';
        ledStatus.className = 'off';
        currentColorDisplay.style.backgroundColor = 'transparent'; // Clear color display
    }
}

// Function to send color (hue) to ESP32
function sendColorToESP(hue) {
    fetch(`/set_hue?hue=${hue}`)
        .then(response => response.json())
        .then(data => {
            console.log("Hue set response:", data);
        })
        .catch(error => console.error('Error setting hue:', error));
}

// Function to send brightness (value) to ESP32
function sendBrightnessToESP(value) { // <<< NEW
    fetch(`/set_brightness?value=${value}`)
        .then(response => response.json())
        .then(data => {
            console.log("Brightness set response:", data);
        })
        .catch(error => console.error('Error setting brightness:', error));
}

// Initial fetch to get the current LED state, hue, and brightness when the page loads
window.onload = function () {
    fetch('/status')
        .then(response => response.json())
        .then(data => {
            // Update ON/OFF status
            updateUI(data.led_state);

            // Initialize slider and color display with the fetched hue and brightness
            colorSlider.value = data.current_hue; // Set color slider position
            brightnessSlider.value = data.current_brightness; // <<< NEW: Set brightness slider position
            brightnessValueSpan.textContent = data.current_brightness; // <<< NEW: Update brightness value text

            updateColorDisplay(); // Update color display to reflect fetched color/brightness
        })
        .catch(error => console.error('Error fetching initial status:', error));
};

// Event listener for the LED toggle button
toggleButton.addEventListener('click', () => {
    fetch('/toggle') // Send GET request to /toggle
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                updateUI(data.new_state); // Update UI based on ESP32's response
            }
        })
        .catch(error => console.error('Error toggling LED:', error));
});

// Event listener for the color slider
colorSlider.addEventListener('input', () => {
    const currentHue = colorSlider.value;
    updateColorDisplay(); // Always update the local color display instantly

    if (ledStatus.className === 'on') {
        sendColorToESP(currentHue); // Only send command if LED is ON
    }
});

// Event listener for the brightness slider <<< NEW
brightnessSlider.addEventListener('input', () => {
    const currentBrightness = brightnessSlider.value;
    brightnessValueSpan.textContent = currentBrightness; // Update brightness value text
    updateColorDisplay(); // Update the local color display with new brightness

    if (ledStatus.className === 'on') {
        sendBrightnessToESP(currentBrightness); // Only send command if LED is ON
    }
});