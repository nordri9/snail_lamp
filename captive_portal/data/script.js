const toggleButton = document.getElementById('toggleButton');
const ledStatus = document.getElementById('ledStatus');
const colorSlider = document.getElementById('colorSlider');
const currentColorDisplay = document.getElementById('currentColorDisplay');
const brightnessSlider = document.getElementById('brightnessSlider');
const brightnessValueSpan = document.getElementById('brightnessValue');
const effectSelect = document.getElementById('effectSelect');
const startEffectButton = document.getElementById('startEffectButton');
const stopEffectButton = document.getElementById('stopEffectButton');

// NEW: Get references to the speed slider and its value display
const speedSlider = document.getElementById('speedSlider');
const speedValueSpan = document.getElementById('speedValue');

let effectRunning = false;

// Function to convert HSV (Hue, Saturation, Value) to RGB for UI display
// Hue is 0-359. Saturation is 0-1. Value (brightness) is 0-1.
function hsvToRgb(h, s, v) {
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
    const currentBrightness = parseInt(brightnessSlider.value) / 255.0; // Normalized for hsvToRgb
    currentColorDisplay.style.backgroundColor = hsvToRgb(currentHue, 1, currentBrightness); // S=1 for full saturation
}

// Function to update UI based on LED state (on/off)
function updateUI(state) {
    if (state) {
        ledStatus.textContent = 'Glowy ✨';
        ledStatus.className = 'on';
        updateColorDisplay();
    } else {
        ledStatus.textContent = 'sleeping 💤';
        ledStatus.className = 'off';
        currentColorDisplay.style.backgroundColor = 'transparent';
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
function sendBrightnessToESP(value) {
    fetch(`/set_brightness?value=${value}`)
        .then(response => response.json())
        .then(data => {
            console.log("Brightness set response:", data);
        })
        .catch(error => console.error('Error setting brightness:', error));
}

// Function to send effect command to ESP32
function startEffectOnESP(effectType, speedMs, hue) {
    fetch(`/start_effect?type=${effectType}&speed=${speedMs}&hue=${hue}`)
        .then(response => response.json())
        .then(data => {
            console.log("Effect started response:", data);
            if (data.status === 'success' && data.effect_type !== 0) {
                effectRunning = true;
                updateUI(true);
            }
        })
        .catch(error => console.error('Error starting effect:', error));
}

// Function to stop effect command on ESP32
function stopEffectOnESP() {
    if (!effectRunning) return;

    fetch(`/start_effect?type=0`)
        .then(response => response.json())
        .then(data => {
            console.log("Effect stopped response:", data);
            effectRunning = false;
            updateColorDisplay(); // Reset color display after stopping effect
        })
        .catch(error => console.error('Error stopping effect:', error));
}

// Initial fetch to get the current LED state, hue, and brightness when the page loads
window.onload = function () {
    fetch('/status')
        .then(response => response.json())
        .then(data => {
            updateUI(data.led_state);

            colorSlider.value = data.current_hue;
            brightnessSlider.value = data.current_brightness;
            brightnessValueSpan.textContent = data.current_brightness;

            // NEW: Initialize speed slider value and text (assuming a default or last saved speed)
            speedSlider.value = 100; // You might want to fetch this from ESP32 if you save it
            speedValueSpan.textContent = 100; // Default speed display

            updateColorDisplay();
        })
        .catch(error => console.error('Error fetching initial status:', error));
};

// Event listener for the LED toggle button
toggleButton.addEventListener('click', () => {
    fetch('/toggle')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                updateUI(data.new_state);
            }
        })
        .catch(error => console.error('Error toggling LED:', error));
});

// Event listener for the color slider
colorSlider.addEventListener('input', () => {
    const currentHue = colorSlider.value;
    updateColorDisplay();

    stopEffectOnESP();
    if (ledStatus.className === 'on') {
        sendColorToESP(currentHue);
    }
});

// Event listener for the brightness slider
brightnessSlider.addEventListener('input', () => {
    const currentBrightness = brightnessSlider.value;
    brightnessValueSpan.textContent = currentBrightness;
    updateColorDisplay();

    stopEffectOnESP();
    if (ledStatus.className === 'on') {
        sendBrightnessToESP(currentBrightness);
    }
});

// NEW: Event listener for the speed slider
speedSlider.addEventListener('input', () => {
    const currentSpeed = speedSlider.value;
    speedValueSpan.textContent = currentSpeed;

    // If an effect is running, update its speed dynamically (optional, but good UX)
    if (effectRunning) {
        const selectedEffect = parseInt(effectSelect.value);
        const currentHue = parseInt(colorSlider.value); // Use current hue for the effect
        startEffectOnESP(selectedEffect, currentSpeed, currentHue);
    }
});


// Event listener for "Start Effect" button
startEffectButton.addEventListener('click', () => {
    const selectedEffect = parseInt(effectSelect.value);
    if (selectedEffect === 0) {
        alert("Please select an effect to start.");
        return;
    }

    // Get the speed from the new slider
    const effectSpeed = parseInt(speedSlider.value);
    const hue = parseInt(colorSlider.value); // Use current slider hue as a base for effects

    // No longer hardcode speeds here; use the slider value instead.
    // The previous logic for adjusting speed/hue based on effect type is now removed
    // to rely solely on the slider and hue picker for user control.
    startEffectOnESP(selectedEffect, effectSpeed, hue);
});

// Event listener for "Stop Effect" button
stopEffectButton.addEventListener('click', () => {
    stopEffectOnESP();
    sendColorToESP(colorSlider.value);
    sendBrightnessToESP(brightnessSlider.value);
    updateUI(true);
});