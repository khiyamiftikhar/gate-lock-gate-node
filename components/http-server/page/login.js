const LOCAL_TESTING = false; // ⚠️ Always false in production

const loginForm = document.getElementById('loginForm');
const loginError = document.getElementById('loginError');


document.addEventListener('DOMContentLoaded', async () => {
    // Check if already logged in
    const token = getAuthToken();
    if (token) {
        const isValid = await checkAuthWithServer();
        if (isValid) {
            window.location.href = 'dashboard.html';
            return; // Skip showing login form
        }
    }

    // Otherwise, show login form
    loginForm.addEventListener('submit', handleLogin);
});

function getAuthToken() {
    return sessionStorage.getItem('authToken');
}

// Check authentication with server (returns true/false)
async function checkAuthWithServer() {
    try {
        const response = await fetch('/check-auth', {
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${getAuthToken()}`
            }
        });
        return response.ok; // true if valid, false if not
    } catch (error) {
        console.error('Auth check failed:', error);
        return false;
    }
}

async function handleLogin(e) {
    e.preventDefault();

    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;

    if (LOCAL_TESTING) {
        // Local testing only (no ESP32 required)
        sessionStorage.setItem('authToken', 'local-test-token');
        window.location.href = 'dashboard.html';
    } else {
        await handleServerLogin(username, password);
    }
}

async function handleServerLogin(username, password) {
    try {
        const response = await fetch('/auth', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ username, password })
        });

        const result = await response.json();

        if (response.ok && result.result === 'ok') {
            // Save token securely in memory (sessionStorage)
            sessionStorage.setItem('authToken', result.token);
            window.location.href = 'dashboard.html';
        } else {
            loginError.textContent = 'Invalid username or password';
        }
    } catch (err) {
        loginError.textContent = 'Connection error. Please try again.';
        console.error(err);
    }
}
