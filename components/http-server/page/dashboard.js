// DOM Elements
const logoutBtn = document.getElementById('logoutBtn');
const menuItems = document.querySelectorAll('.menu-item');

// Configuration: Set to true for local testing, false for ESP32 production
const LOCAL_TESTING = false;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    // Only check auth if not in local testing mode
    if (!LOCAL_TESTING) {
        checkAuthWithServer();
    }
    
    // Setup event listeners
    setupEventListeners();
});

// Get auth token from session
function getAuthToken() {
    return sessionStorage.getItem('authToken');
}

// Check authentication with server (only in production)
async function checkAuthWithServer() {
    try {
        const response = await fetch('/check-auth', {
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${getAuthToken()}`
            }
        });
        
        if (!response.ok) {
            // Not authenticated, redirect to login
            window.location.href = 'login.html';
        }
    } catch (error) {
        console.error('Auth check failed:', error);
        window.location.href = 'login.html';
    }
}

// Setup all event listeners
function setupEventListeners() {
    // Logout button
    logoutBtn.addEventListener('click', handleLogout);
    
    // Menu items navigation
    menuItems.forEach(item => {
        item.addEventListener('click', handleMenuClick);
        item.style.cursor = 'pointer';
    });
}

// Handle logout
async function handleLogout() {
    const token = sessionStorage.getItem('authToken');

    if (!LOCAL_TESTING && token) {
        try {
            await fetch('/logout', {
                method: 'POST',
                headers: { 'Authorization': token }
            });
        } catch (err) {
            console.warn('Logout request failed:', err);
        }
    }

    sessionStorage.clear();
    window.location.href = 'login.html';
}

// Handle menu item clicks
function handleMenuClick(e) {
    const menuItem = e.currentTarget;
    const page = menuItem.dataset.page;
    
    console.log('Navigating to:', page);
    
    // Navigate to corresponding page
    if (page === 'home') {
        window.location.href = 'dashboard.html';
    } else if (page === 'ota') {
        window.location.href = 'ota.html';
    }
}