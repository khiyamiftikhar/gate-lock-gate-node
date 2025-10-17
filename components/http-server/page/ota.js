// State management
let selectedFile = null;

// Configuration: Set to true for local testing, false for ESP32 production
const LOCAL_TESTING = true;

// DOM Elements
const logoutBtn = document.getElementById('logoutBtn');
const menuItems = document.querySelectorAll('.menu-item');
const fileInput = document.getElementById('firmwareFile');
const fileInputWrapper = document.getElementById('fileInputWrapper');
const fileInfo = document.getElementById('fileInfo');
const fileName = document.getElementById('fileName');
const fileSize = document.getElementById('fileSize');
const updateBtn = document.getElementById('updateBtn');
const progressContainer = document.getElementById('progressContainer');
const progressFill = document.getElementById('progressFill');
const progressText = document.getElementById('progressText');
const statusMessage = document.getElementById('statusMessage');

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    // Only check auth if not in local testing mode
    if (!LOCAL_TESTING) {
        checkAuthWithServer();
    }
    
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
                'Authorization': getAuthToken()
            }
        });
        
        if (!response.ok) {
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
    
    // File upload
    fileInputWrapper.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', handleFileSelect);
    
    // Update button
    updateBtn.addEventListener('click', handleOTAUpdate);
}

// Handle logout
async function handleLogout() {
    if (!LOCAL_TESTING) {
        try {
            await fetch('/logout', {
                method: 'POST',
                headers: {
                    'Authorization': getAuthToken()
                }
            });
        } catch (error) {
            console.error('Logout error:', error);
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
    
    if (page === 'home') {
        window.location.href = 'dashboard.html';
    } else if (page === 'ota') {
        window.location.href = 'ota.html';
    }
}

// Handle file selection
function handleFileSelect(e) {
    const file = e.target.files[0];
    if (!file) return;

    // Validate file type
    if (!file.name.endsWith('.bin')) {
        showStatus('Please select a .bin file', 'error');
        return;
    }
    
    // Validate file size (2MB limit)
    if (file.size > 2 * 1024 * 1024) {
        showStatus('File size exceeds 2MB limit', 'error');
        return;
    }

    // Update state and UI
    selectedFile = file;
    fileName.textContent = file.name;
    fileSize.textContent = `Size: ${(file.size / 1024).toFixed(2)} KB`;
    fileInfo.classList.add('active');
    updateBtn.disabled = false;
    statusMessage.innerHTML = '';
}

// Handle OTA Update with chunked upload
async function handleOTAUpdate() {
    if (!selectedFile) return;

    // Disable button during upload
    updateBtn.disabled = true;
    progressContainer.classList.add('active');
    statusMessage.innerHTML = '';

    const CHUNK_SIZE = 4096; // 4KB chunks - suitable for ESP32 buffer
    const totalChunks = Math.ceil(selectedFile.size / CHUNK_SIZE);
    let uploadedChunks = 0;

    // If in local testing mode, simulate upload
    if (LOCAL_TESTING) {
        simulateUpload(totalChunks);
        return;
    }

    try {
        // Read file as array buffer
        const fileBuffer = await selectedFile.arrayBuffer();
        
        progressText.textContent = 'Starting upload...';
        
        // Send file in chunks
        for (let i = 0; i < totalChunks; i++) {
            const start = i * CHUNK_SIZE;
            const end = Math.min(start + CHUNK_SIZE, fileBuffer.byteLength);
            const chunk = fileBuffer.slice(start, end);
            
            // Prepare form data for chunk upload
            const formData = new FormData();
            formData.append('chunk', new Blob([chunk]));
            formData.append('chunkIndex', i);
            formData.append('totalChunks', totalChunks);
            formData.append('fileName', selectedFile.name);
            
            // Upload chunk to ESP32 with auth token
            const response = await fetch('/ota/upload', {
                method: 'POST',
                headers: {
                    'Authorization': getAuthToken()
                },
                body: formData
            });

            if (!response.ok) {
                if (response.status === 401) {
                    throw new Error('Authentication failed. Please login again.');
                }
                const errorText = await response.text();
                throw new Error(`Upload failed at chunk ${i + 1}: ${errorText}`);
            }

            // Update progress
            uploadedChunks++;
            const progress = Math.round((uploadedChunks / totalChunks) * 100);
            updateProgress(progress, uploadedChunks, totalChunks);
        }

        // All chunks uploaded, now finalize
        await finalizeOTAUpdate();

    } catch (error) {
        console.error('OTA Update Error:', error);
        showStatus('Update failed: ' + error.message, 'error');
        updateBtn.disabled = false;
        
        // If auth error, redirect to login
        if (error.message.includes('Authentication')) {
            setTimeout(() => {
                window.location.href = 'login.html';
            }, 2000);
        }
    }
}

// Simulate upload for local testing
function simulateUpload(totalChunks) {
    progressText.textContent = 'Starting upload (SIMULATED)...';
    let uploadedChunks = 0;
    
    const interval = setInterval(() => {
        uploadedChunks++;
        const progress = Math.round((uploadedChunks / totalChunks) * 100);
        updateProgress(progress, uploadedChunks, totalChunks);
        
        if (uploadedChunks >= totalChunks) {
            clearInterval(interval);
            progressText.textContent = 'Applying update (SIMULATED)...';
            
            setTimeout(() => {
                showStatus('Firmware update simulation complete! (This is a test - no actual upload occurred)', 'success');
                setTimeout(() => {
                    resetOTAPage();
                }, 3000);
            }, 1000);
        }
    }, 50); // Simulate fast upload for testing
}

// Finalize OTA update
async function finalizeOTAUpdate() {
    progressText.textContent = 'Applying firmware update...';
    
    try {
        const response = await fetch('/ota/complete', {
            method: 'POST',
            headers: {
                'Authorization': getAuthToken()
            }
        });

        if (response.status === 401) {
            throw new Error('Authentication failed. Please login again.');
        }

        if (response.ok) {
            showStatus('Firmware update successful! Device will restart...', 'success');
            
            // Reset page after 3 seconds
            setTimeout(() => {
                resetOTAPage();
                showStatus('You may need to reconnect after device restarts', 'info');
            }, 3000);
        } else {
            const errorText = await response.text();
            throw new Error('Failed to complete update: ' + errorText);
        }
    } catch (error) {
        throw error;
    }
}

// Update progress bar and text
function updateProgress(percentage, current, total) {
    progressFill.style.width = percentage + '%';
    progressFill.textContent = percentage + '%';
    progressText.textContent = `Uploading... ${current}/${total} chunks (${percentage}%)`;
}

// Show status message
function showStatus(message, type) {
    statusMessage.innerHTML = `<div class="status-message ${type}">${message}</div>`;
}

// Reset OTA page to initial state
function resetOTAPage() {
    selectedFile = null;
    fileInput.value = '';
    fileInfo.classList.remove('active');
    progressContainer.classList.remove('active');
    updateBtn.disabled = true;
    progressFill.style.width = '0%';
    progressFill.textContent = '0%';
    progressText.textContent = 'Preparing upload...';
}