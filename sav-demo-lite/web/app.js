// Global state
let eventSource = null;
let records = [];
let currentRecord = null;

// Statistics
const stats = {
    total: 0,
    interface_based: 0,
    prefix_based: 0,
    allowlist: 0,
    blocklist: 0,
    permit: 0,
    discard: 0,
    ratelimit: 0,
    redirect: 0,
    allowlist_failures: 0,
    blocklist_hits: 0,
    // Threat landscape statistics
    interface_set: new Set(),
    prefix_set: new Set(),
    interface_count: new Map(),  // interface_id -> {count, permit, discard, ratelimit, redirect}
    prefix_count: new Map()      // prefix -> {count, permit, discard, ratelimit, redirect}
};

// Charts
let modeChart = null;
let policyChart = null;

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    setupEventHandlers();
    initializeCharts();
    connectSSE();
});

// Setup event handlers
function setupEventHandlers() {
    document.getElementById('btnStart').addEventListener('click', () => {
        fetch('/api/control/start', { method: 'POST' })
            .then(r => r.json())
            .then(() => {
                updateControlButtons('playing');
            });
    });
    
    document.getElementById('btnPause').addEventListener('click', () => {
        const isPlaying = document.getElementById('btnPause').textContent.includes('Pause');
        const endpoint = isPlaying ? '/api/control/pause' : '/api/control/resume';
        
        fetch(endpoint, { method: 'POST' })
            .then(r => r.json())
            .then(() => {
                updateControlButtons(isPlaying ? 'paused' : 'playing');
            });
    });
    
    document.getElementById('btnReset').addEventListener('click', () => {
        fetch('/api/control/reset', { method: 'POST' })
            .then(r => r.json())
            .then(() => {
                resetUI();
            });
    });
    
    document.getElementById('speedSelect').addEventListener('change', (e) => {
        const speed = e.target.value;
        fetch(`/api/control/speed?value=${speed}`, { method: 'POST' })
            .then(r => r.json());
    });
    
    // Tab switching
    document.querySelectorAll('.analysis-tab').forEach(tab => {
        tab.addEventListener('click', () => {
            switchTab(tab.dataset.tab);
        });
    });
}

// Connect to SSE stream
function connectSSE() {
    if (eventSource) {
        eventSource.close();
    }
    
    eventSource = new EventSource('/api/stream/events');
    
    eventSource.addEventListener('status', (e) => {
        const data = JSON.parse(e.data);
        console.log('Status:', data);
        
        if (data.status === 'playing') {
            updateControlButtons('playing');
        } else if (data.status === 'paused') {
            updateControlButtons('paused');
        } else if (data.status === 'reset') {
            resetUI();
        }
    });
    
    eventSource.addEventListener('record', (e) => {
        const record = JSON.parse(e.data);
        console.log('New record:', record);
        handleNewRecord(record);
    });
    
    eventSource.addEventListener('completed', (e) => {
        const data = JSON.parse(e.data);
        updateStatus('stopped', data.message);
        updateControlButtons('stopped');
    });
    
    eventSource.addEventListener('speed', (e) => {
        const data = JSON.parse(e.data);
        console.log('Speed changed:', data.speed);
    });
    
    eventSource.onerror = (e) => {
        console.error('SSE error:', e);
        setTimeout(connectSSE, 3000); // Reconnect after 3 seconds
    };
}

// Handle new record
function handleNewRecord(record) {
    records.push(record);
    currentRecord = record;
    
    // Update statistics
    updateStatistics(record);
    
    // Update UI
    displayRecord(record);
    updateTimeline(record);
    updateProgress(record);
    updateAnalysisPanels();
}

// Display record in main panel
function displayRecord(record) {
    const stream = document.getElementById('eventStream');
    
    // Remove empty state if present
    const emptyState = stream.querySelector('.empty-state');
    if (emptyState) {
        emptyState.remove();
    }
    
    // Create record card
    const card = document.createElement('div');
    card.className = 'record-card';
    card.id = `record-${record.record_number}`;
    
    // Determine record type (Macro or Micro)
    const recordType = (record.mappings && record.mappings.length > 0) ? 'micro' : 'macro';
    const typeBadge = recordType === 'micro' 
        ? '<span class="record-type-badge badge-micro">Micro</span>'
        : '<span class="record-type-badge badge-macro">Macro</span>';
    
    // Mappings HTML
    let mappingsHTML = '';
    if (record.mappings && record.mappings.length > 0) {
        mappingsHTML = `
            <div class="mappings-section">
                <div class="mappings-title">
                    <i class="bi bi-list-ul"></i>
                    savMatchedContentList (${record.mappings.length} rules)
                </div>
                ${record.mappings.map((m, i) => `
                    <div class="mapping-item">
                        Rule ${i+1}: Interface ${m.interface_id} → ${m.prefix}/${m.prefix_length} ${m.is_ipv6 ? '(IPv6)' : '(IPv4)'}
                    </div>
                `).join('')}
            </div>
        `;
    } else {
        mappingsHTML = `
            <div class="mappings-section" style="background: rgba(255,255,255,0.05);">
                <div style="opacity: 0.7; font-size: 0.9em;">
                    <i class="bi bi-info-circle"></i> 
                    This is an aggregated (Macro) record. Detailed rules available in Micro records.
                </div>
            </div>
        `;
    }
    
    card.innerHTML = `
        <div class="record-header">
            <div class="record-number">
                <i class="bi bi-file-earmark-text"></i> Record #${record.record_number}
                ${typeBadge}
            </div>
            <div class="record-scenario">${record.scenario_name}</div>
        </div>
        
        <div style="margin-bottom: 15px;">
            <i class="bi bi-clock"></i> <strong>Timestamp:</strong> ${record.timestamp}
        </div>
        
        <div class="record-fields">
            <div class="field-item">
                <div class="field-label"><i class="bi bi-tag"></i> savRuleType</div>
                <div class="field-value">
                    <span class="highlight">${record.rule_type}</span> - ${record.rule_type_name}
                </div>
            </div>
            
            <div class="field-item">
                <div class="field-label"><i class="bi bi-bullseye"></i> savTargetType</div>
                <div class="field-value">
                    <span class="highlight">${record.target_type}</span> - ${record.target_type_name}
                </div>
            </div>
            
            <div class="field-item">
                <div class="field-label"><i class="bi bi-shield-check"></i> savPolicyAction</div>
                <div class="field-value">
                    <span class="highlight">${record.policy_action}</span> - ${record.policy_action_name}
                </div>
            </div>
            
            <div class="field-item">
                <div class="field-label"><i class="bi bi-info-circle"></i> Label</div>
                <div class="field-value">${record.label}</div>
            </div>
        </div>
        
        ${mappingsHTML}
    `;
    
    // Insert at top
    stream.insertBefore(card, stream.firstChild);
    
    // Keep only last 10 records
    const cards = stream.querySelectorAll('.record-card');
    if (cards.length > 10) {
        cards[cards.length - 1].remove();
    }
    
    // Scroll to top
    stream.scrollTop = 0;
}

// Update timeline
function updateTimeline(record) {
    const timeline = document.getElementById('timeline');
    
    // Remove empty state
    const emptyState = timeline.querySelector('.empty-state');
    if (emptyState) {
        emptyState.remove();
    }
    
    // Remove active class from all
    timeline.querySelectorAll('.timeline-item').forEach(item => {
        item.classList.remove('active');
    });
    
    // Create timeline item
    const item = document.createElement('div');
    item.className = 'timeline-item active';
    item.innerHTML = `
        <div style="font-weight: bold; color: var(--primary-color);">
            Record #${record.record_number}
        </div>
        <div style="font-size: 0.85em; color: #718096;">
            ${record.timestamp.split(' ')[1]}
        </div>
        <div style="font-size: 0.8em; margin-top: 5px;">
            ${record.policy_action_name}
        </div>
    `;
    
    timeline.appendChild(item);
    
    // Scroll to bottom
    timeline.scrollTop = timeline.scrollHeight;
}

// Update progress
function updateProgress(record) {
    document.getElementById('progressText').textContent = 
        `${record.record_number} / ${record.total_records}`;
}

// Update statistics
function updateStatistics(record) {
    stats.total++;
    
    // Target type
    if (record.target_type === 0) stats.interface_based++;
    else if (record.target_type === 1) stats.prefix_based++;
    
    // Rule type
    if (record.rule_type === 0) stats.allowlist++;
    else if (record.rule_type === 1) stats.blocklist++;
    
    // Policy action
    if (record.policy_action === 0) stats.permit++;
    else if (record.policy_action === 1) stats.discard++;
    else if (record.policy_action === 2) stats.ratelimit++;
    else if (record.policy_action === 3) stats.redirect++;
    
    // Combination statistics
    if (record.rule_type === 0 && record.policy_action === 1) {
        stats.allowlist_failures++;
    }
    if (record.rule_type === 1 && record.policy_action === 1) {
        stats.blocklist_hits++;
    }
    
    // Collect interface and prefix statistics from mappings
    if (record.mappings && record.mappings.length > 0) {
        record.mappings.forEach(m => {
            // Interface statistics
            const iface = m.interface_id;
            stats.interface_set.add(iface);
            if (!stats.interface_count.has(iface)) {
                stats.interface_count.set(iface, {
                    count: 0,
                    permit: 0,
                    discard: 0,
                    ratelimit: 0,
                    redirect: 0
                });
            }
            const ifaceStats = stats.interface_count.get(iface);
            ifaceStats.count++;
            if (record.policy_action === 0) ifaceStats.permit++;
            else if (record.policy_action === 1) ifaceStats.discard++;
            else if (record.policy_action === 2) ifaceStats.ratelimit++;
            else if (record.policy_action === 3) ifaceStats.redirect++;
            
            // Prefix statistics
            const prefix = `${m.prefix}/${m.prefix_length}`;
            stats.prefix_set.add(prefix);
            if (!stats.prefix_count.has(prefix)) {
                stats.prefix_count.set(prefix, {
                    count: 0,
                    permit: 0,
                    discard: 0,
                    ratelimit: 0,
                    redirect: 0
                });
            }
            const prefixStats = stats.prefix_count.get(prefix);
            prefixStats.count++;
            if (record.policy_action === 0) prefixStats.permit++;
            else if (record.policy_action === 1) prefixStats.discard++;
            else if (record.policy_action === 2) prefixStats.ratelimit++;
            else if (record.policy_action === 3) prefixStats.redirect++;
        });
    }
}

// Update analysis panels
function updateAnalysisPanels() {
    // Threat landscape
    document.getElementById('totalSpoofingEvents').textContent = stats.total;
    document.getElementById('highRiskInterfaces').textContent = stats.interface_set.size;
    document.getElementById('spoofedPrefixes').textContent = stats.prefix_set.size;
    
    // Update rankings
    updateRankings();
    
    // Validation mode statistics
    document.getElementById('interfaceCount').textContent = stats.interface_based;
    document.getElementById('prefixCount').textContent = stats.prefix_based;
    document.getElementById('allowlistCount').textContent = stats.allowlist;
    document.getElementById('blocklistCount').textContent = stats.blocklist;
    
    // Policy tracking
    document.getElementById('permitCount').textContent = stats.permit;
    document.getElementById('discardCountPolicy').textContent = stats.discard;
    document.getElementById('ratelimitCount').textContent = stats.ratelimit;
    document.getElementById('redirectCount').textContent = stats.redirect;
    
    // Update charts
    updateCharts();
}

// Update rankings
function updateRankings() {
    // Top 5 Interfaces
    const interfaceRanking = Array.from(stats.interface_count.entries())
        .sort((a, b) => b[1].count - a[1].count)
        .slice(0, 5);
    
    const interfaceTableBody = document.getElementById('interfaceRankingBody');
    interfaceTableBody.innerHTML = interfaceRanking.map((entry, index) => {
        const [iface, data] = entry;
        const bars = generateActionBars(data);
        const actions = `${data.permit}P ${data.discard}D ${data.redirect}R ${data.ratelimit}L`;
        return `
            <tr>
                <td style="text-align: center; font-weight: bold;">${index + 1}</td>
                <td>Interface ${iface}</td>
                <td style="text-align: center; font-weight: bold;">${data.count}</td>
                <td>${bars} ${actions}</td>
            </tr>
        `;
    }).join('');
    
    // Top 5 Prefixes
    const prefixRanking = Array.from(stats.prefix_count.entries())
        .sort((a, b) => b[1].count - a[1].count)
        .slice(0, 5);
    
    const prefixTableBody = document.getElementById('prefixRankingBody');
    prefixTableBody.innerHTML = prefixRanking.map((entry, index) => {
        const [prefix, data] = entry;
        const bars = generateActionBars(data);
        const actions = `${data.permit}P ${data.discard}D ${data.redirect}R ${data.ratelimit}L`;
        return `
            <tr>
                <td style="text-align: center; font-weight: bold;">${index + 1}</td>
                <td><code>${prefix}</code></td>
                <td style="text-align: center; font-weight: bold;">${data.count}</td>
                <td>${bars} ${actions}</td>
            </tr>
        `;
    }).join('');
}

// Generate action bars for rankings
function generateActionBars(data) {
    const total = data.count;
    const maxBars = 10;
    const barCount = Math.max(1, Math.round((total / stats.total) * maxBars * 5));
    return '█'.repeat(Math.min(barCount, 20));
}

// Initialize charts
function initializeCharts() {
    // Mode chart
    const modeCtx = document.getElementById('modeChart');
    modeChart = new Chart(modeCtx, {
        type: 'doughnut',
        data: {
            labels: ['Interface-Based', 'Prefix-Based'],
            datasets: [{
                data: [0, 0],
                backgroundColor: ['#667eea', '#764ba2']
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    position: 'bottom'
                },
                title: {
                    display: true,
                    text: 'SAV Validation Mode Distribution'
                }
            }
        }
    });
    
    // Policy chart
    const policyCtx = document.getElementById('policyChart');
    policyChart = new Chart(policyCtx, {
        type: 'bar',
        data: {
            labels: ['Permit', 'Discard', 'Rate-limit', 'Redirect'],
            datasets: [{
                label: 'Policy Actions',
                data: [0, 0, 0, 0],
                backgroundColor: ['#48bb78', '#f56565', '#ed8936', '#ecc94b']
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                },
                title: {
                    display: true,
                    text: 'Policy Action Distribution'
                }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    ticks: {
                        stepSize: 1
                    }
                }
            }
        }
    });
}

// Update charts
function updateCharts() {
    // Mode chart
    if (modeChart) {
        modeChart.data.datasets[0].data = [stats.interface_based, stats.prefix_based];
        modeChart.update();
    }
    
    // Policy chart
    if (policyChart) {
        policyChart.data.datasets[0].data = [
            stats.permit, 
            stats.discard, 
            stats.ratelimit, 
            stats.redirect
        ];
        policyChart.update();
    }
}

// Update control buttons
function updateControlButtons(state) {
    const btnStart = document.getElementById('btnStart');
    const btnPause = document.getElementById('btnPause');
    
    if (state === 'playing') {
        btnStart.disabled = true;
        btnPause.disabled = false;
        btnPause.innerHTML = '<i class="bi bi-pause-fill"></i> Pause';
        updateStatus('playing', 'Playing');
    } else if (state === 'paused') {
        btnStart.disabled = true;
        btnPause.disabled = false;
        btnPause.innerHTML = '<i class="bi bi-play-fill"></i> Resume';
        updateStatus('paused', 'Paused');
    } else if (state === 'stopped') {
        btnStart.disabled = false;
        btnPause.disabled = true;
        updateStatus('stopped', 'Completed');
    }
}

// Update status indicator
function updateStatus(state, text) {
    const dot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');
    
    dot.className = 'status-dot status-' + state;
    statusText.textContent = text;
}

// Reset UI
function resetUI() {
    // Clear records
    records = [];
    currentRecord = null;
    
    // Reset statistics
    stats = {
        total: 0,
        interface_based: 0,
        prefix_based: 0,
        allowlist: 0,
        blocklist: 0,
        permit: 0,
        discard: 0,
        ratelimit: 0,
        redirect: 0
    };
    
    // Clear event stream
    const stream = document.getElementById('eventStream');
    stream.innerHTML = `
        <div class="empty-state">
            <i class="bi bi-broadcast-pin"></i>
            <h3>Waiting for IPFIX Data Stream...</h3>
            <p>Click the Start button to begin real-time SAV record playback</p>
        </div>
    `;
    
    // Clear timeline
    const timeline = document.getElementById('timeline');
    timeline.innerHTML = `
        <div class="empty-state">
            <i class="bi bi-hourglass-split"></i>
            <p>Click Start to begin</p>
        </div>
    `;
    
    // Reset progress
    document.getElementById('progressText').textContent = '0 / 27';
    
    // Reset analysis panels
    updateAnalysisPanels();
    
    // Reset controls
    updateControlButtons('stopped');
    updateStatus('stopped', 'Ready');
}

// Switch analysis tab
function switchTab(tabName) {
    // Update tabs
    document.querySelectorAll('.analysis-tab').forEach(tab => {
        tab.classList.toggle('active', tab.dataset.tab === tabName);
    });
    
    // Update content
    document.querySelectorAll('.analysis-content').forEach(content => {
        content.classList.remove('active');
    });
    
    if (tabName === 'root-cause') {
        document.getElementById('rootCausePanel').classList.add('active');
    } else if (tabName === 'statistics') {
        document.getElementById('statisticsPanel').classList.add('active');
    } else if (tabName === 'policy') {
        document.getElementById('policyPanel').classList.add('active');
    }
}
