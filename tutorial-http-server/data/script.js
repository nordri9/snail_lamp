const toggleButton = document.getElementById('toggleButton');
const ledStatus = document.getElementById('ledStatus');

// Function to update UI based on LED state
function updateUI(state) {
    if (state) {
        ledStatus.textContent = 'ON';
        ledStatus.className = 'on';
    } else {
        ledStatus.textContent = 'OFF';
        ledStatus.className = 'off';
    }
}

// Initial fetch to get the current state when the page loads
window.onload = function() {
    fetch('/status')
        .then(response => response.json())
        .then(data => {
            updateUI(data.led_state);
        })
        .catch(error => console.error('Error fetching initial status:', error));
};

// Event listener for the button
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