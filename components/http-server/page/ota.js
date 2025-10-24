// State management
let selectedFile = null;

// Configuration: Set to true for local testing, false for ESP32 production
const LOCAL_TESTING = false;

document.addEventListener('DOMContentLoaded', () => {
    console.log('DOM Loaded');

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

    if (!LOCAL_TESTING) checkAuthWithServer();
    setupEventListeners();

    function getAuthToken() {
        return sessionStorage.getItem('authToken');
    }

    async function checkAuthWithServer() {
        try {
            const res = await fetch('/check-auth', {
                method: 'GET',
                headers: { 'Authorization': `Bearer ${getAuthToken()}` }
            });
            if (!res.ok) window.location.href = 'login.html';
        } catch {
            window.location.href = 'login.html';
        }
    }

    function setupEventListeners() {
        if (logoutBtn) logoutBtn.addEventListener('click', handleLogout);
        menuItems.forEach(m => m.addEventListener('click', handleMenuClick));

        if (fileInputWrapper) fileInputWrapper.addEventListener('click', () => fileInput.click());
        if (fileInput) fileInput.addEventListener('change', handleFileSelect);
        if (updateBtn) updateBtn.addEventListener('click', handleOTAUpdate);
    }

    async function handleLogout() {
        if (!LOCAL_TESTING) {
            try {
                await fetch('/logout', {
                    method: 'POST',
                    headers: { 'Authorization': `Bearer ${getAuthToken()}` }
                });
            } catch {}
        }
        sessionStorage.clear();
        window.location.href = 'login.html';
    }

    function handleMenuClick(e) {
        const page = e.currentTarget.dataset.page;
        if (page === 'home') window.location.href = 'dashboard.html';
        else if (page === 'ota') window.location.href = 'ota.html';
    }

    function handleFileSelect(e) {
        const file = e.target.files[0];
        if (!file) return;

        if (!file.name.endsWith('.bin')) {
            showStatus('Please select a .bin file', 'error');
            return;
        }
        if (file.size > 2 * 1024 * 1024) {
            showStatus('File size exceeds 2MB limit', 'error');
            return;
        }

        selectedFile = file;
        fileName.textContent = file.name;
        fileSize.textContent = `Size: ${(file.size / 1024).toFixed(2)} KB`;
        fileInfo.classList.add('active');
        updateBtn.disabled = false;
        statusMessage.innerHTML = '';
    }

    async function handleOTAUpdate() {
        if (!selectedFile) {
            showStatus('No file selected', 'error');
            return;
        }

        updateBtn.disabled = true;
        progressContainer.classList.add('active');
        progressText.textContent = 'Uploading...';

        try {
            const formData = new FormData();
            formData.append('firmware', selectedFile);

            const response = await fetch('/ota/upload', {
                method: 'POST',
                headers: {
                    'Authorization': `Bearer ${getAuthToken()}`
                },
                body: formData
            });

            if (!response.ok) {
                const errText = await response.text();
                throw new Error(`Upload failed: ${errText}`);
            }

            updateProgress(100);
            progressText.textContent = 'Upload complete. Finalizing...';
            await finalizeOTAUpdate();

        } catch (err) {
            console.error('OTA Error:', err);
            showStatus('Update failed: ' + err.message, 'error');
            updateBtn.disabled = false;
        }
    }

    async function finalizeOTAUpdate() {
        try {
            const res = await fetch('/ota/complete', {
                method: 'POST',
                headers: { 'Authorization': `Bearer ${getAuthToken()}` }
            });

            if (res.ok) {
                showStatus('Firmware update successful! Device will restart...', 'success');
                setTimeout(resetOTAPage, 3000);
            } else {
                const errText = await res.text();
                throw new Error(errText);
            }
        } catch (e) {
            showStatus('Finalize failed: ' + e.message, 'error');
        }
    }

    function updateProgress(percent) {
        progressFill.style.width = percent + '%';
        progressFill.textContent = percent + '%';
    }

    function showStatus(msg, type) {
        statusMessage.innerHTML = `<div class="status-message ${type}">${msg}</div>`;
    }

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
