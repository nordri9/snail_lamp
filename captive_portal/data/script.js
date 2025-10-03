const toggleButton = document.getElementById('toggleButton');
const ledStatus = document.getElementById('ledStatus');
const colorSlider = document.getElementById('colorSlider');
const currentColorDisplay = document.getElementById('currentColorDisplay');
const brightnessSlider = document.getElementById('brightnessSlider'); // <<< NEW
const brightnessValueSpan = document.getElementById('brightnessValue');
const effectSelect = document.getElementById('effectSelect'); // <<< NEW
const startEffectButton = document.getElementById('startEffectButton'); // <<< NEW
const stopEffectButton = document.getElementById('stopEffectButton');

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
        ledStatus.textContent = 'Glowy ✨';
        ledStatus.className = 'on';
        updateColorDisplay(); // Update color display to reflect current color
    } else {
        ledStatus.textContent = 'sleeping 💤';
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


// Function to send effect command to ESP32
function startEffectOnESP(effectType, speedMs, hue) { // <<< NEW
    fetch(`/start_effect?type=${effectType}&speed=${speedMs}&hue=${hue}`)
        .then(response => response.json())
        .then(data => {
            console.log("Effect started response:", data);
            if (data.status === 'success' && data.effect_type !== 0) {
                // If an effect starts, assume LEDs are ON, update UI
                updateUI(true);
            }
        })
        .catch(error => console.error('Error starting effect:', error));
}

// Function to stop effect command on ESP32
function stopEffectOnESP() { // <<< NEW
    fetch(`/start_effect?type=0`) // Type 0 is EFFECT_NONE
        .then(response => response.json())
        .then(data => {
            console.log("Effect stopped response:", data);
            // After stopping effect, default to last solid color
            // updateUI(true); // Re-activate solid color display (optional, can be removed)
            updateColorDisplay(); // Set the UI display to the last solid color
        })
        .catch(error => console.error('Error stopping effect:', error));
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

    // Stop any running effect before setting a solid color
    stopEffectOnESP(); // <<< NEW: Stop effect when changing solid color
    if (ledStatus.className === 'on') {
        sendColorToESP(currentHue);
    }
});

// Event listener for the brightness slider
brightnessSlider.addEventListener('input', () => {
    const currentBrightness = brightnessSlider.value;
    brightnessValueSpan.textContent = currentBrightness;
    updateColorDisplay();

    // Stop any running effect before setting solid brightness
    stopEffectOnESP(); // <<< NEW: Stop effect when changing solid brightness
    if (ledStatus.className === 'on') {
        sendBrightnessToESP(currentBrightness);
    }
});

// Event listener for "Start Effect" button <<< NEW
startEffectButton.addEventListener('click', () => {
    const selectedEffect = parseInt(effectSelect.value);
    if (selectedEffect === 0) {
        alert("Please select an effect to start.");
        return;
    }

    let speed = 50; // Default speed in ms
    let hue = parseInt(colorSlider.value); // Use current slider hue as a base for effects

    // Adjust speed and hue based on effect type if needed
    if (selectedEffect === 2) { // Theater Chase
        speed = 100; // Slower chase
    } else if (selectedEffect === 3) { // Sparkle
        speed = 20; // Faster sparkle
    } else if (selectedEffect === 4) { // Wave
        speed = 70; // Wave speed
        hue = 240; // Set Wave to blue (hue 240) by default for contrast
    }

    startEffectOnESP(selectedEffect, speed, hue);
});

// Event listener for "Stop Effect" button <<< NEW
stopEffectButton.addEventListener('click', () => {
    stopEffectOnESP();
    // After stopping, re-apply the last solid color
    sendColorToESP(colorSlider.value); // Send the last solid color
    sendBrightnessToESP(brightnessSlider.value); // Send the last solid brightness
    updateUI(true); // Ensure UI shows ON and correct color
});