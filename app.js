// app.js

// Global variables for WASM instance
let wasmExports = null;
let wasmMemory = null;
let currentTheme = 'default';

// Modal state variables
let confirmCallback = null;

// WASI System Call Shim/Polyfill for standard browser context
const wasiShim = {
    fd_write: (fd, iovs, iovsLen, nwritten) => {
        // Mock success (return 0)
        return 0;
    },
    fd_close: (fd) => 0,
    fd_seek: (fd, offset, whence, newOffset) => 0,
    fd_read: (fd, iovs, iovsLen, nread) => 0,
    fd_fdstat_get: (fd, buf) => 0,
    fd_fdstat_set_flags: (fd, flags) => 0,
    fd_prestat_get: (fd, buf) => 8, // ERRNO_BADF - no preopened dirs
    fd_prestat_dir_name: (fd, path, pathLen) => 0,
    path_open: (fd, dirflags, path, pathLen, oflags, fsRightsBase, fsRightsInheriting, fdflags, openedFd) => 0,
    environ_sizes_get: (environCount, environBufSize) => 0,
    environ_get: (environ, environBuf) => 0,
    clock_time_get: (id, precision, timeOut) => 0,
    clock_res_get: (id, resolutionPtr) => 0,
    args_sizes_get: (argcPtr, argvBufSizePtr) => 0,
    args_get: (argvPtr, argvBufPtr) => 0,
    sched_yield: () => 0,
    random_get: (buf, bufLen) => {
        if (wasmMemory) {
            const bytes = new Uint8Array(wasmMemory.buffer, buf, bufLen);
            crypto.getRandomValues(bytes);
        }
        return 0;
    },
    proc_exit: (code) => {
        console.warn(`WASM application exited with code: ${code}`);
    }
};

// Wrap wasiShim in a Proxy to dynamically stub out any other missing WASI syscalls
const wasiShimProxy = new Proxy(wasiShim, {
    get: (target, prop) => {
        if (prop in target) {
            return target[prop];
        }
        // Return a default stub that does nothing and returns 0 (success)
        return () => 0;
    }
});

// Imports passed to the WebAssembly module
const importObject = {
    wasi_snapshot_preview1: wasiShimProxy
};

// WebAssembly Memory Helpers
function stringToWasm(str) {
    const encoder = new TextEncoder();
    const bytes = encoder.encode(str + '\0'); // null-terminated UTF-8
    const ptr = wasmExports.wasm_malloc(bytes.length);
    const memory = new Uint8Array(wasmMemory.buffer);
    memory.set(bytes, ptr);
    return ptr;
}

function wasmToString(ptr) {
    const memory = new Uint8Array(wasmMemory.buffer);
    let end = ptr;
    while (memory[end] !== 0) {
        end++;
    }
    const bytes = memory.subarray(ptr, end);
    const decoder = new TextDecoder();
    return decoder.decode(bytes);
}

// ----------------------------------------------------
// WebAssembly Loading and Initialization
// ----------------------------------------------------
async function initWasm() {
    const statusBadge = document.getElementById('wasm-status-badge');
    try {
        statusBadge.textContent = 'Loading WASM...';
        statusBadge.style.color = 'var(--text-secondary)';

        const response = await fetch('calculator.wasm');
        if (!response.ok) {
            throw new Error(`Failed to fetch calculator.wasm (Status: ${response.status})`);
        }
        
        const bytes = await response.arrayBuffer();
        const { instance } = await WebAssembly.instantiate(bytes, importObject);
        
        wasmExports = instance.exports;
        wasmMemory = instance.exports.memory;

        // Initialize static constructors in C++ WASM module
        if (wasmExports._initialize) {
            wasmExports._initialize();
        } else if (wasmExports.__wasm_call_ctors) {
            wasmExports.__wasm_call_ctors();
        }


        statusBadge.textContent = 'Active (C++ Engine Loaded)';
        statusBadge.style.color = 'var(--success)';

        // Synchronize browser localStorage state into WebAssembly memory
        syncStateFromLocalStorage();

        // Initial UI Renderings
        renderHistory();

    } catch (err) {
        console.error('Failed to load C++ WebAssembly Engine:', err);
        statusBadge.textContent = 'Error: Engine Offline';
        statusBadge.style.color = 'var(--error)';
        alert('Could not initialize the C++ expression evaluation engine. Please make sure calculator.wasm is compiled and hosted.');
    }
}

// ----------------------------------------------------
// State Syncing (LocalStorage <=> C++ WASM Memory)
// ----------------------------------------------------
function syncStateFromLocalStorage() {
    if (!wasmExports) return;

    // Load Variables
    const savedVars = localStorage.getItem('smart_calc_variables');
    if (savedVars) {
        const ptr = stringToWasm(savedVars);
        wasmExports.load_variables_from_json(ptr);
        wasmExports.wasm_free(ptr);
    } else {
        // Initial defaults from C++ (pi, e)
        saveVariablesToLocalStorage();
    }

    // Load History
    const savedHistory = localStorage.getItem('smart_calc_history');
    if (savedHistory) {
        const ptr = stringToWasm(savedHistory);
        wasmExports.load_history_from_json(ptr);
        wasmExports.wasm_free(ptr);
    }
}

function saveVariablesToLocalStorage() {
    if (!wasmExports) return;
    const jsonPtr = wasmExports.get_variables_json();
    const jsonStr = wasmToString(jsonPtr);
    localStorage.setItem('smart_calc_variables', jsonStr);
}

function saveHistoryToLocalStorage() {
    if (!wasmExports) return;
    const jsonPtr = wasmExports.get_history_json();
    const jsonStr = wasmToString(jsonPtr);
    localStorage.setItem('smart_calc_history', jsonStr);
}

// Get parsed JS array/object from Wasm JSON
function getVariablesList() {
    if (!wasmExports) return {};
    const jsonPtr = wasmExports.get_variables_json();
    return JSON.parse(wasmToString(jsonPtr));
}

function getHistoryList() {
    if (!wasmExports) return [];
    const jsonPtr = wasmExports.get_history_json();
    return JSON.parse(wasmToString(jsonPtr));
}

// ----------------------------------------------------
// Frontend SPA Tab Routing
// ----------------------------------------------------
document.querySelectorAll('.nav-link').forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        
        // Remove active class from all links and tabs
        document.querySelectorAll('.nav-link').forEach(l => l.classList.remove('active'));
        document.querySelectorAll('.tab-view').forEach(tab => tab.classList.remove('active'));
        
        // Add active to current
        const selectedTab = link.getAttribute('data-tab');
        link.classList.add('active');
        document.getElementById(`tab-${selectedTab}`).classList.add('active');
        
        // Refresh tables on tab focus
        if (selectedTab === 'history') {
            renderHistory();
        }
    });
});

// ----------------------------------------------------
// UI Rendering - Calculator Tab
// ----------------------------------------------------
const calcInput = document.getElementById('calc-input');

// Keypad input handlers
document.querySelectorAll('.calc-btn[data-val]').forEach(btn => {
    btn.addEventListener('click', () => {
        const val = btn.getAttribute('data-val');
        const start = calcInput.selectionStart;
        const end = calcInput.selectionEnd;
        const currentText = calcInput.value;
        
        calcInput.value = currentText.substring(0, start) + val + currentText.substring(end);
        calcInput.focus();
        
        // Offset selection
        const newCursorPos = start + val.length;
        calcInput.setSelectionRange(newCursorPos, newCursorPos);
    });
});

document.getElementById('btn-clear').addEventListener('click', () => {
    calcInput.value = '';
    document.getElementById('output-original').textContent = '-';
    document.getElementById('output-postfix').textContent = '-';
    document.getElementById('output-result').textContent = '0';
    document.getElementById('output-result').style.color = 'var(--accent)';
    calcInput.focus();
});

document.getElementById('btn-back').addEventListener('click', () => {
    const start = calcInput.selectionStart;
    const end = calcInput.selectionEnd;
    const currentText = calcInput.value;
    
    if (start === end && start > 0) {
        calcInput.value = currentText.substring(0, start - 1) + currentText.substring(end);
        calcInput.setSelectionRange(start - 1, start - 1);
    } else {
        calcInput.value = currentText.substring(0, start) + currentText.substring(end);
        calcInput.setSelectionRange(start, start);
    }
    calcInput.focus();
});

document.getElementById('btn-copy').addEventListener('click', () => {
    const resultText = document.getElementById('output-result').textContent;
    if (resultText && resultText !== '0' && resultText !== 'Error') {
        navigator.clipboard.writeText(resultText).then(() => {
            const btn = document.getElementById('btn-copy');
            const originalText = btn.textContent;
            btn.textContent = 'Copied!';
            setTimeout(() => { btn.textContent = originalText; }, 1500);
        }).catch(err => {
            console.error('Failed to copy result:', err);
        });
    }
});

// Expression evaluation
document.getElementById('btn-evaluate').addEventListener('click', performCalculation);

calcInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
        performCalculation();
    }
});

function performCalculation() {
    if (!wasmExports) {
        alert('C++ WebAssembly calculation engine is offline.');
        return;
    }
    
    const expression = calcInput.value.trim();
    if (!expression) return;
    
    const originalDisplay = document.getElementById('output-original');
    const postfixDisplay = document.getElementById('output-postfix');
    const resultDisplay = document.getElementById('output-result');
    
    originalDisplay.textContent = expression;
    
    const exprPtr = stringToWasm(expression);
    const resultJsonPtr = wasmExports.evaluate_expression(exprPtr);
    wasmExports.wasm_free(exprPtr);
    
    const resultStr = wasmToString(resultJsonPtr);
    const resultObj = JSON.parse(resultStr);
    
    // Save to calculation history
    const calcId = Date.now().toString(36) + Math.random().toString(36).substr(2, 5);
    const now = new Date();
    const dateStr = now.toISOString().split('T')[0];
    const timeStr = now.toTimeString().split(' ')[0];
    
    if (resultObj.success) {
        postfixDisplay.textContent = resultObj.postfix;
        resultDisplay.textContent = resultObj.answer;
        resultDisplay.style.color = 'var(--success)';
        
        // Add to history in C++
        const idPtr = stringToWasm(calcId);
        const datePtr = stringToWasm(dateStr);
        const timePtr = stringToWasm(timeStr);
        const exprPtr2 = stringToWasm(expression);
        const postfixPtr = stringToWasm(resultObj.postfix);
        const answerPtr = stringToWasm(resultObj.answer);
        
        wasmExports.add_history_record(idPtr, datePtr, timePtr, exprPtr2, postfixPtr, answerPtr, true);
        
        wasmExports.wasm_free(idPtr);
        wasmExports.wasm_free(datePtr);
        wasmExports.wasm_free(timePtr);
        wasmExports.wasm_free(exprPtr2);
        wasmExports.wasm_free(postfixPtr);
        wasmExports.wasm_free(answerPtr);
        
    } else {
        postfixDisplay.textContent = 'Error';
        resultDisplay.textContent = resultObj.error;
        resultDisplay.style.color = 'var(--error)';
        
        // Add to history in C++ as failed
        const idPtr = stringToWasm(calcId);
        const datePtr = stringToWasm(dateStr);
        const timePtr = stringToWasm(timeStr);
        const exprPtr2 = stringToWasm(expression);
        const emptyPtr = stringToWasm('');
        const errorPtr = stringToWasm(resultObj.error);
        
        wasmExports.add_history_record(idPtr, datePtr, timePtr, exprPtr2, emptyPtr, errorPtr, false);
        
        wasmExports.wasm_free(idPtr);
        wasmExports.wasm_free(datePtr);
        wasmExports.wasm_free(timePtr);
        wasmExports.wasm_free(exprPtr2);
        wasmExports.wasm_free(emptyPtr);
        wasmExports.wasm_free(errorPtr);
    }
    
    // Save history and update UI
    saveHistoryToLocalStorage();
    renderHistory();
}

// ----------------------------------------------------
// UI Rendering - History Tab
// ----------------------------------------------------
let historySortNewest = true;

document.getElementById('history-search').addEventListener('input', renderHistory);
document.getElementById('btn-toggle-sort').addEventListener('click', () => {
    historySortNewest = !historySortNewest;
    document.getElementById('btn-toggle-sort').textContent = `Sort: ${historySortNewest ? 'Newest First' : 'Oldest First'}`;
    renderHistory();
});

document.getElementById('history-select-all').addEventListener('change', (e) => {
    const isChecked = e.target.checked;
    document.querySelectorAll('.record-checkbox:not(#history-select-all)').forEach(chk => {
        chk.checked = isChecked;
    });
});

document.getElementById('btn-delete-selected').addEventListener('click', () => {
    const selectedCheckboxes = document.querySelectorAll('.record-checkbox:not(#history-select-all):checked');
    if (selectedCheckboxes.length === 0) {
        alert('Please select one or more history records to delete.');
        return;
    }
    
    showConfirmModal(
        'Delete Selected History?',
        `Are you sure you want to delete the ${selectedCheckboxes.length} selected history records?`,
        () => {
            if (!wasmExports) return;
            selectedCheckboxes.forEach(chk => {
                const id = chk.getAttribute('data-id');
                const idPtr = stringToWasm(id);
                wasmExports.delete_history_record(idPtr);
                wasmExports.wasm_free(idPtr);
            });
            
            saveHistoryToLocalStorage();
            renderHistory();
            document.getElementById('history-select-all').checked = false;
        }
    );
});

document.getElementById('btn-delete-all').addEventListener('click', () => {
    const historyList = getHistoryList();
    if (historyList.length === 0) return;
    
    showConfirmModal(
        'Clear Entire History?',
        'Are you sure you want to permanently clear all calculation history records? This cannot be undone.',
        () => {
            if (!wasmExports) return;
            wasmExports.clear_history();
            saveHistoryToLocalStorage();
            renderHistory();
            document.getElementById('history-select-all').checked = false;
        }
    );
});

// Render the dedicated history page list and the quick list in calculator page
function renderHistory() {
    const container = document.getElementById('history-records-container');
    const quickList = document.getElementById('quick-history-list');
    const quickBadge = document.getElementById('quick-history-badge');
    const searchQuery = document.getElementById('history-search').value.toLowerCase().trim();
    
    let list = getHistoryList();
    

    
    // Sort logic
    if (historySortNewest) {
        list.reverse(); // Array comes oldest-first from C++ vector, reverse for newest-first
    }
    
    // Update Quick Panel List (Show all items, newest first)
    const quickListNewest = historySortNewest ? list : [...list].reverse();
    quickBadge.textContent = list.length;
    
    if (list.length === 0) {
        quickList.innerHTML = `<div class="empty-state">No calculations yet. Let's calculate!</div>`;
    } else {
        let qHtml = '';
        quickListNewest.forEach(r => {
            qHtml += `
                <div class="mini-item" onclick="loadExpressionIntoCalc('${r.expression.replace(/'/g, "\\'")}')">
                    <div class="mini-item-expr">${r.expression}</div>
                    <div class="mini-item-ans" style="color: ${r.success ? 'var(--accent)' : 'var(--error)'}">${r.answer}</div>
                </div>
            `;
        });
        quickList.innerHTML = qHtml;
    }
    
    // Filter logic for dedicated history page
    const filtered = list.filter(r => {
        if (!searchQuery) return true;
        
        // Search by keyword, expression, postfix, answer, date
        const matchText = r.expression.toLowerCase().includes(searchQuery) ||
                          r.postfix.toLowerCase().includes(searchQuery) ||
                          r.answer.toLowerCase().includes(searchQuery) ||
                          r.date.toLowerCase().includes(searchQuery);
                          
        if (matchText) return true;
        
        // Search by specific operator (if single char query is an operator)
        if (searchQuery.length === 1 && "+-*/%^".includes(searchQuery)) {
            return r.expression.includes(searchQuery);
        }
        
        return false;
    });
    
    if (filtered.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="8" x2="12" y2="12"></line><line x1="12" y1="16" x2="12.01" y2="16"></line></svg>
                <p>No history records found.</p>
            </div>
        `;
        return;
    }
    
    let html = '';
    filtered.forEach(r => {
        const indicatorClass = r.success ? 'success' : 'failed';
        const answerClass = r.success ? '' : 'failed';
        const checkboxId = `chk-${r.id}`;
        
        html += `
            <div class="record-card">
                <input type="checkbox" class="record-checkbox" data-id="${r.id}" id="${checkboxId}">
                <div class="record-status-indicator ${indicatorClass}"></div>
                <div class="record-info" onclick="loadExpressionIntoCalc('${r.expression.replace(/'/g, "\\'")}')" style="cursor:pointer;">
                    <div>
                        <div class="record-meta">${r.date} &bull; ${r.time}</div>
                        <div class="record-expr">${r.expression}</div>
                    </div>
                    <div>
                        <div class="record-meta">Postfix Stack</div>
                        <div class="record-postfix">${r.postfix || '-'}</div>
                    </div>
                    <div class="record-answer ${answerClass}">
                        ${r.answer}
                    </div>
                </div>
                <button class="record-delete-btn" onclick="deleteSingleHistory('${r.id}')" title="Delete record">
                    <svg viewBox="0 0 24 24"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path><line x1="10" y1="11" x2="10" y2="17"></line><line x1="14" y1="11" x2="14" y2="17"></line></svg>
                </button>
            </div>
        `;
    });
    container.innerHTML = html;
}

function loadExpressionIntoCalc(expr) {
    calcInput.value = expr;
    // Go to calculator tab
    document.querySelector('.nav-link[data-tab="calculator"]').click();
    calcInput.focus();
}

function deleteSingleHistory(id) {
    showConfirmModal(
        'Delete History Entry?',
        'Are you sure you want to delete this specific calculation record?',
        () => {
            if (!wasmExports) return;
            const idPtr = stringToWasm(id);
            wasmExports.delete_history_record(idPtr);
            wasmExports.wasm_free(idPtr);
            
            saveHistoryToLocalStorage();
            renderHistory();
        }
    );
}

// ----------------------------------------------------
// Theme Customizer & Storage Wipe (Settings Tab)
// ----------------------------------------------------
document.querySelectorAll('.theme-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.theme-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        
        currentTheme = btn.getAttribute('data-theme');
        if (currentTheme === 'default') {
            document.body.removeAttribute('data-theme');
        } else {
            document.body.setAttribute('data-theme', currentTheme);
        }
        localStorage.setItem('smart_calc_theme', currentTheme);
    });
});

// Load saved theme
const savedTheme = localStorage.getItem('smart_calc_theme');
if (savedTheme) {
    const targetBtn = document.querySelector(`.theme-btn[data-theme="${savedTheme}"]`);
    if (targetBtn) {
        targetBtn.click();
    }
}

// Wipe Storage
document.getElementById('btn-wipe-data').addEventListener('click', () => {
    showConfirmModal(
        'Reset Storage Memory?',
        'This will clear ALL calculation history from local browser storage and restart the engine. Are you sure?',
        () => {
            localStorage.removeItem('smart_calc_history');
            localStorage.removeItem('smart_calc_variables');
            
            if (wasmExports) {
                wasmExports.clear_history();
                wasmExports.clear_variables();
            }
            
            saveVariablesToLocalStorage();
            saveHistoryToLocalStorage();
            
            renderHistory();
            
            alert('App memory has been fully reset.');
        }
    );
});

// ----------------------------------------------------
// UI Modal Dialog Helpers
// ----------------------------------------------------
const confirmModal = document.getElementById('confirm-modal');

function showConfirmModal(title, desc, onConfirm) {
    document.getElementById('confirm-title').textContent = title;
    document.getElementById('confirm-desc').textContent = desc;
    confirmCallback = onConfirm;
    confirmModal.classList.add('active');
}

function hideConfirmModal() {
    confirmModal.classList.remove('active');
    confirmCallback = null;
}

document.getElementById('confirm-btn-cancel').addEventListener('click', hideConfirmModal);
document.getElementById('confirm-btn-ok').addEventListener('click', () => {
    if (confirmCallback) {
        confirmCallback();
    }
    hideConfirmModal();
});

// Hide modal when clicking backdrop
confirmModal.addEventListener('click', (e) => {
    if (e.target === confirmModal) {
        hideConfirmModal();
    }
});

// ----------------------------------------------------
// Initialize Application on Page Load
// ----------------------------------------------------
window.addEventListener('DOMContentLoaded', () => {
    initWasm();

    // Sidebar Collapse Logic
    const sidebarToggle = document.getElementById('sidebar-toggle');
    if (sidebarToggle) {
        sidebarToggle.addEventListener('click', () => {
            document.body.classList.toggle('sidebar-hidden');
            const isHidden = document.body.classList.contains('sidebar-hidden');
            localStorage.setItem('smart_calc_sidebar_hidden', isHidden);
        });
    }

    // Load saved sidebar state on startup
    const isSidebarHidden = localStorage.getItem('smart_calc_sidebar_hidden') === 'true';
    if (isSidebarHidden) {
        document.body.classList.add('sidebar-hidden');
    }
});
