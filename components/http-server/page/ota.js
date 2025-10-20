// State management
let selectedFile = null;

// Configuration: Set to true for local testing, false for ESP32 production
const LOCAL_TESTING = false;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    console.log('DOM Content Loaded');
    
    // Get DOM Elements
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
    
    console.log('Elements found:', {
        logoutBtn: !!logoutBtn,
        fileInput: !!fileInput,
        fileInputWrapper: !!fileInputWrapper,
        updateBtn: !!updateBtn
    });
    
    // Only check auth if not in local testing mode
    if (!LOCAL_TESTING) {
        checkAuthWithServer();
    }
    
    setupEventListeners();
    
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
                window.location.href = 'login.html';
            }
        } catch (error) {
            console.error('Auth check failed:', error);
            window.location.href = 'login.html';
        }
    }
    
    // Setup all event listeners
    function setupEventListeners() {
        console.log('Setting up event listeners...');
        
        // Logout button
        if (logoutBtn) {
            logoutBtn.addEventListener('click', handleLogout);
            console.log('Logout listener attached');
        }
        
        // Menu items navigation
        menuItems.forEach(item => {
            item.addEventListener('click', handleMenuClick);
            item.style.cursor = 'pointer';
        });
        console.log('Menu listeners attached:', menuItems.length);
        
        // File upload
        if (fileInputWrapper) {
            fileInputWrapper.addEventListener('click', () => {
                console.log('File input wrapper clicked!');
                if (fileInput) {
                    fileInput.click();
                    console.log('Triggered file input click');
                }
            });
            console.log('File wrapper listener attached');
        }
        
        if (fileInput) {
            fileInput.addEventListener('change', handleFileSelect);
            console.log('File input change listener attached');
        }
        
        // Update button
        if (updateBtn) {
            updateBtn.addEventListener('click', handleOTAUpdate);
            console.log('Update button listener attached');
        }
        
        console.log('All listeners setup complete');
    }
    
    // Handle logout
    async function handleLogout() {
        console.log('Logout clicked');
        if (!LOCAL_TESTING) {
            try {
                await fetch('/logout', {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${getAuthToken()}`
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
        console.log('File select triggered');
        const file = e.target.files[0];
        console.log('Selected file:', file);
        
        if (!file) {
            console.log('No file selected');
            return;
        }
    
        // Validate file type
        if (!file.name.endsWith('.bin')) {
            console.log('Invalid file type:', file.name);
            showStatus('Please select a .bin file', 'error');
            return;
        }
        
        // Validate file size (2MB limit)
        if (file.size > 2 * 1024 * 1024) {
            console.log('File too large:', file.size);
            showStatus('File size exceeds 2MB limit', 'error');
            return;
        }
    
        console.log('File valid, updating UI');
        
        // Update state and UI
        selectedFile = file;
        fileName.textContent = file.name;
        fileSize.textContent = `Size: ${(file.size / 1024).toFixed(2)} KB`;
        fileInfo.classList.add('active');
        updateBtn.disabled = false;
        statusMessage.innerHTML = '';
        
        console.log('Button enabled:', !updateBtn.disabled);
    }
    
    // Handle OTA Update with chunked upload
    async function handleOTAUpdate() {
        console.log('OTA Update started');
        if (!selectedFile) {
            console.log('No file selected');
            return;
        }
    
        // Disable button during upload
        updateBtn.disabled = true;
        progressContainer.classList.add('active');
        statusMessage.innerHTML = '';
    
        const CHUNK_SIZE = 4096;
        const totalChunks = Math.ceil(selectedFile.size / CHUNK_SIZE);
        let uploadedChunks = 0;
    
        // If in local testing mode, simulate upload
        if (LOCAL_TESTING) {
            simulateUpload(totalChunks);
            return;
        }
    
        try {
            const fileBuffer = await selectedFile.arrayBuffer();
            
            progressText.textContent = 'Starting upload...';
            
            for (let i = 0; i < totalChunks; i++) {
                const start = i * CHUNK_SIZE;
                const end = Math.min(start + CHUNK_SIZE, fileBuffer.byteLength);
                const chunk = fileBuffer.slice(start, end);
                
                const formData = new FormData();
                formData.append('chunk', new Blob([chunk]));
                formData.append('chunkIndex', i);
                formData.append('totalChunks', totalChunks);
                formData.append('fileName', selectedFile.name);
                
                const response = await fetch('/ota/upload', {
                    method: 'POST',
                    headers: {
                     'Authorization': `Bearer ${getAuthToken()}`
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
    
                uploadedChunks++;
                const progress = Math.round((uploadedChunks / totalChunks) * 100);
                updateProgress(progress, uploadedChunks, totalChunks);
            }
    
            await finalizeOTAUpdate();
    
        } catch (error) {
            console.error('OTA Update Error:', error);
            showStatus('Update failed: ' + error.message, 'error');
            updateBtn.disabled = false;
            
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
        }, 50);
    }
    
    // Finalize OTA update
    async function finalizeOTAUpdate() {
        progressText.textContent = 'Applying firmware update...';
        
        try {
            const response = await fetch('/ota/complete', {
                method: 'POST',
                headers: {
                    'Authorization': `Bearer ${getAuthToken()}`
                }
            });
    
            if (response.status === 401) {
                throw new Error('Authentication failed. Please login again.');
            }
    
            if (response.ok) {
                showStatus('Firmware update successful! Device will restart...', 'success');
                
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
});