// SAV IPFIX Record Viewer - Frontend Logic

let allRecords = [];
let filteredRecords = [];

// Load data on page load
document.addEventListener('DOMContentLoaded', () => {
    loadData();
    setupEventListeners();
});

// Setup event listeners
function setupEventListeners() {
    document.getElementById('filter-type').addEventListener('change', applyFilters);
    document.getElementById('search').addEventListener('input', applyFilters);
}

// Load JSON data
async function loadData() {
    try {
        const response = await fetch('data.json');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const data = await response.json();
        allRecords = data.records || [];
        filteredRecords = [...allRecords];
        
        updateStats(data);
        renderRecords();
    } catch (error) {
        showError(`Failed to load data: ${error.message}`);
        console.error('Error loading data:', error);
    }
}

// Update statistics
function updateStats(data) {
    document.getElementById('total-records').textContent = data.totalRecords || 0;
    document.getElementById('ip-version').textContent = `IPv${data.ipVersion || 4}`;
    
    const totalRules = allRecords.reduce((sum, record) => sum + (record.rules?.length || 0), 0);
    document.getElementById('total-rules').textContent = totalRules;
    
    if (data.generatedAt) {
        const date = new Date(data.generatedAt * 1000);
        document.getElementById('last-updated').textContent = formatTime(date);
    }
}

// Apply filters
function applyFilters() {
    const ruleTypeFilter = document.getElementById('filter-type').value;
    const searchText = document.getElementById('search').value.toLowerCase();
    
    filteredRecords = allRecords.filter(record => {
        // Filter by rule type
        if (ruleTypeFilter !== 'all' && record.ruleTypeName !== ruleTypeFilter) {
            return false;
        }
        
        // Filter by search text
        if (searchText) {
            const matchesIP = record.rules?.some(rule => 
                rule.sourcePrefix?.toLowerCase().includes(searchText)
            );
            const matchesInterface = record.rules?.some(rule => 
                rule.interfaceId?.toString().includes(searchText)
            );
            
            if (!matchesIP && !matchesInterface) {
                return false;
            }
        }
        
        return true;
    });
    
    renderRecords();
}

// Render records
function renderRecords() {
    const container = document.getElementById('records-container');
    
    if (filteredRecords.length === 0) {
        container.innerHTML = `
            <div class="no-records">
                <h2>No Records Found</h2>
                <p>Try adjusting your filters or search criteria.</p>
            </div>
        `;
        return;
    }
    
    container.innerHTML = filteredRecords.map(record => createRecordCard(record)).join('');
}

// Create record card HTML
function createRecordCard(record) {
    const timestamp = record.timestamp ? new Date(record.timestamp) : null;
    const ruleTypeName = record.ruleTypeName || 'unknown';
    const badgeClass = `badge-${ruleTypeName}`;
    
    return `
        <div class="record-card">
            <div class="record-header">
                <div class="record-id">Record #${record.recordId}</div>
                <div class="rule-type-badge ${badgeClass}">${ruleTypeName}</div>
            </div>
            
            <div class="record-info">
                <div class="info-item">
                    <div class="info-label">Timestamp</div>
                    <div class="info-value timestamp">
                        ${timestamp ? formatDateTime(timestamp) : 'N/A'}
                    </div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Rule Type</div>
                    <div class="info-value">${ruleTypeName} (${record.ruleType ?? 'N/A'})</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Target Type</div>
                    <div class="info-value">${record.targetType ?? 'N/A'}</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Policy Action</div>
                    <div class="info-value">${record.policyAction ?? 'N/A'}</div>
                </div>
            </div>
            
            ${createRulesSection(record.rules)}
        </div>
    `;
}

// Create rules section
function createRulesSection(rules) {
    if (!rules || rules.length === 0) {
        return `
            <div class="rules-section">
                <div class="rules-header">
                    Rules
                    <span class="rules-count">0</span>
                </div>
                <p style="color: #888; font-style: italic;">No rules in this record</p>
            </div>
        `;
    }
    
    const rulesRows = rules.map((rule, index) => `
        <tr>
            <td>${index + 1}</td>
            <td>${rule.interfaceId ?? 'N/A'}</td>
            <td class="ip-address">${rule.sourcePrefix || 'N/A'}</td>
            <td>${rule.prefixLength ?? 'N/A'}</td>
        </tr>
    `).join('');
    
    return `
        <div class="rules-section">
            <div class="rules-header">
                Rules
                <span class="rules-count">${rules.length}</span>
            </div>
            <table class="rules-table">
                <thead>
                    <tr>
                        <th>#</th>
                        <th>Interface ID</th>
                        <th>Source Prefix</th>
                        <th>Prefix Length</th>
                    </tr>
                </thead>
                <tbody>
                    ${rulesRows}
                </tbody>
            </table>
        </div>
    `;
}

// Format date and time
function formatDateTime(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    
    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
}

// Format time only
function formatTime(date) {
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    
    return `${hours}:${minutes}`;
}

// Show error message
function showError(message) {
    const errorContainer = document.getElementById('error-container');
    errorContainer.innerHTML = `
        <div class="error">
            <strong>⚠️ Error:</strong> ${message}
        </div>
    `;
    
    // Clear records container
    document.getElementById('records-container').innerHTML = '';
}
