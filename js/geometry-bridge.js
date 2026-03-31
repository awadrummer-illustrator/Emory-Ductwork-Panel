/**
 * Geometry Bridge - Node.js bridge to Python geometry server
 *
 * This module manages a persistent Python file-watching server for fast geometry operations.
 * Eliminates the ~7 second startup overhead per Python call.
 *
 * The server communicates via files in the watch folder (not HTTP).
 */

// Early error logging - write to a known location regardless of errors
(function() {
    try {
        const earlyFs = require('fs');
        const earlyPath = require('path');
        const earlyOs = require('os');
        const earlyLogFile = earlyPath.join(earlyOs.tmpdir(), 'mdux_bridge_early.log');
        earlyFs.writeFileSync(earlyLogFile, 'Geometry Bridge loading at ' + new Date().toISOString() + '\n');
        window.__mduxEarlyLog = function(msg) {
            try { earlyFs.appendFileSync(earlyLogFile, msg + '\n'); } catch(e) {}
        };
        window.__mduxEarlyLog('Early logging initialized');
    } catch (e) {
        // If this fails, Node.js might not be available
        console.error('GEOMETRY-BRIDGE: Early logging failed:', e);
        alert('Geometry Bridge: Node.js not available - ' + e.message);
    }
})();

const { spawn, exec } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

// Watch folder for file-based communication
const WATCH_FOLDER = path.join(os.tmpdir(), 'mdux_watch');
const STATUS_FILE = path.join(WATCH_FOLDER, 'server_status.json');

// Log file for debugging startup issues
const LOG_FILE = path.join(os.tmpdir(), 'mdux_bridge_debug.log');

/**
 * Write to debug log file (for troubleshooting when console isn't visible)
 */
function logToFile(msg) {
    try {
        const timestamp = new Date().toISOString();
        fs.appendFileSync(LOG_FILE, `[${timestamp}] ${msg}\n`);
    } catch (e) {
        // Ignore file write errors
    }
}

// Clear log on startup
try {
    fs.writeFileSync(LOG_FILE, `=== Geometry Bridge Started at ${new Date().toISOString()} ===\n`);
} catch (e) { }

// Python executable candidates to try
const PYTHON_CANDIDATES = [
    'python',
    'python3',
    'py',
    'C:\\Python312\\python.exe',
    'C:\\Python311\\python.exe',
    'C:\\Python310\\python.exe',
    'C:\\Python39\\python.exe',
    process.env.LOCALAPPDATA + '\\Programs\\Python\\Python312\\python.exe',
    process.env.LOCALAPPDATA + '\\Programs\\Python\\Python311\\python.exe',
    process.env.LOCALAPPDATA + '\\Programs\\Python\\Python310\\python.exe'
];

let serverProcess = null;
let serverReady = false;
let pythonExecutable = null;  // Cache the working Python path

/**
 * Ensure watch folder exists
 */
function ensureWatchFolder() {
    if (!fs.existsSync(WATCH_FOLDER)) {
        fs.mkdirSync(WATCH_FOLDER, { recursive: true });
    }
}

/**
 * Get the path to the Python server script
 */
function getServerPath() {
    // Known deployment path - check this first as it's most reliable
    const deployedPath = 'C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Emory-Ductwork-Panel/python/geometry_server.py';
    if (fs.existsSync(deployedPath)) {
        logToFile('Using deployed path: ' + deployedPath);
        return deployedPath;
    }

    // Try CSInterface as backup
    try {
        const csInterface = new CSInterface();
        // SystemPath.EXTENSION value from CEP SDK
        const extRoot = csInterface.getSystemPath('extension');
        logToFile('Extension root from CSInterface: ' + extRoot);
        if (extRoot && !extRoot.includes('invalid') && fs.existsSync(extRoot)) {
            const serverPath = path.join(extRoot, 'python', 'geometry_server.py');
            if (fs.existsSync(serverPath)) {
                return serverPath;
            }
        }
    } catch (e) {
        logToFile('CSInterface error: ' + e.message);
    }

    // Development path fallback
    const devPath = 'e:/Work/Work/Custom Sketchup, Illustrator and Photoshop Scripts and Extensions/Illustrator/Extensions/Emory-Ductwork-Panel/python/geometry_server.py';
    if (fs.existsSync(devPath)) {
        logToFile('Using dev path: ' + devPath);
        return devPath;
    }

    logToFile('ERROR: Could not find geometry_server.py');
    return null;
}

/**
 * Find a working Python executable
 */
async function findPythonExecutable() {
    logToFile('findPythonExecutable() called');

    if (pythonExecutable) {
        logToFile('Using cached Python: ' + pythonExecutable);
        return pythonExecutable;
    }

    // First try using 'where' command to find Python in PATH
    try {
        logToFile('Trying "where python" command...');
        const result = await new Promise((resolve, reject) => {
            exec('where python', { windowsHide: true }, (err, stdout) => {
                if (err) reject(err);
                else resolve(stdout.trim().split('\n')[0]);
            });
        });
        logToFile('where python result: ' + result);
        if (result && fs.existsSync(result.trim())) {
            pythonExecutable = result.trim();
            console.log('[GEOM-BRIDGE] Found Python via where:', pythonExecutable);
            logToFile('Found Python via where: ' + pythonExecutable);
            return pythonExecutable;
        }
    } catch (e) {
        console.log('[GEOM-BRIDGE] "where python" failed:', e.message);
        logToFile('"where python" failed: ' + e.message);
    }

    // Try each candidate
    logToFile('Trying candidate paths...');
    for (const candidate of PYTHON_CANDIDATES) {
        if (!candidate) continue;
        logToFile('Trying candidate: ' + candidate);
        try {
            // Check if it's a file path and exists
            if (candidate.includes('\\') || candidate.includes('/')) {
                if (fs.existsSync(candidate)) {
                    pythonExecutable = candidate;
                    console.log('[GEOM-BRIDGE] Found Python at:', pythonExecutable);
                    logToFile('Found Python at path: ' + pythonExecutable);
                    return pythonExecutable;
                } else {
                    logToFile('Path does not exist: ' + candidate);
                }
            } else {
                // Try to run it
                await new Promise((resolve, reject) => {
                    const proc = spawn(candidate, ['--version'], {
                        windowsHide: true,
                        stdio: ['ignore', 'pipe', 'pipe']
                    });
                    proc.on('error', reject);
                    proc.on('close', (code) => {
                        if (code === 0) resolve();
                        else reject(new Error('Exit code ' + code));
                    });
                });
                pythonExecutable = candidate;
                console.log('[GEOM-BRIDGE] Found Python command:', pythonExecutable);
                logToFile('Found Python command: ' + pythonExecutable);
                return pythonExecutable;
            }
        } catch (e) {
            logToFile('Candidate ' + candidate + ' failed: ' + e.message);
            // Continue to next candidate
        }
    }

    console.error('[GEOM-BRIDGE] Could not find Python executable');
    logToFile('ERROR: Could not find any Python executable');
    return null;
}

/**
 * Check if the server is running by reading status file
 */
function checkServerHealth() {
    return new Promise((resolve) => {
        try {
            if (!fs.existsSync(STATUS_FILE)) {
                resolve(false);
                return;
            }

            const content = fs.readFileSync(STATUS_FILE, 'utf8');
            const status = JSON.parse(content);

            // Check if status is "running" and timestamp is recent (within 5 seconds)
            const now = Date.now();
            const statusAge = now - status.timestamp;
            const isRecent = statusAge < 5000; // 5 seconds

            if (status.status === 'running' && isRecent) {
                console.log('[GEOM-BRIDGE] Server status: running (age: ' + statusAge + 'ms)');
                resolve(true);
            } else {
                console.log('[GEOM-BRIDGE] Server status: stale or stopped (age: ' + statusAge + 'ms)');
                resolve(false);
            }
        } catch (e) {
            console.log('[GEOM-BRIDGE] Health check error:', e.message);
            resolve(false);
        }
    });
}

/**
 * Start the Python geometry server (hidden, no console window)
 */
async function startServer() {
    logToFile('startServer() called');

    // Check if already running
    if (await checkServerHealth()) {
        console.log('[GEOM-BRIDGE] Server already running');
        logToFile('Server already running (health check passed)');
        serverReady = true;
        return true;
    }

    logToFile('Server not running, attempting to start...');
    ensureWatchFolder();
    logToFile('Watch folder ensured: ' + WATCH_FOLDER);

    let serverPath;
    try {
        serverPath = getServerPath();
        logToFile('Server path: ' + serverPath);
    } catch (e) {
        logToFile('ERROR getting server path: ' + e.message);
        console.error('[GEOM-BRIDGE] Error getting server path:', e);
        return false;
    }

    console.log('[GEOM-BRIDGE] Starting server:', serverPath);

    // Check if script exists
    if (!fs.existsSync(serverPath)) {
        console.error('[GEOM-BRIDGE] Server script not found:', serverPath);
        logToFile('ERROR: Server script not found at: ' + serverPath);
        return false;
    }
    logToFile('Server script exists');

    // Find Python executable
    const python = await findPythonExecutable();
    if (!python) {
        console.error('[GEOM-BRIDGE] No Python executable found - cannot start server');
        console.error('[GEOM-BRIDGE] Please ensure Python is installed and in your PATH');
        logToFile('ERROR: No Python executable found');
        return false;
    }

    console.log('[GEOM-BRIDGE] Using Python:', python);
    logToFile('Using Python: ' + python);

    logToFile('Spawning Python process...');

    return new Promise((resolve) => {
        try {
            // Spawn Python with hidden window
            serverProcess = spawn(python, [serverPath], {
                cwd: path.dirname(serverPath),
                stdio: ['ignore', 'pipe', 'pipe'],
                windowsHide: true,
                detached: false,
                // Pass through environment variables including PATH
                env: { ...process.env }
            });

            logToFile('Spawn called, waiting for READY signal...');

            let startupTimeout = setTimeout(() => {
                console.log('[GEOM-BRIDGE] Server startup timeout (15s)');
                console.log('[GEOM-BRIDGE] The server may have failed to import required modules');
                logToFile('ERROR: Server startup timeout (15s)');
                resolve(false);
            }, 15000);

            serverProcess.stdout.on('data', (data) => {
                const msg = data.toString().trim();
                console.log('[GEOM-SERVER]', msg);
                logToFile('STDOUT: ' + msg);

                // Server prints "READY:{folder}" when ready
                if (msg.startsWith('READY:')) {
                    clearTimeout(startupTimeout);
                    serverReady = true;
                    console.log('[GEOM-BRIDGE] Server ready, watch folder:', msg.substring(6));
                    logToFile('Server READY! Watch folder: ' + msg.substring(6));
                    resolve(true);
                }
            });

            serverProcess.stderr.on('data', (data) => {
                const msg = data.toString().trim();
                if (msg) {
                    console.log('[GEOM-SERVER-ERR]', msg);
                    logToFile('STDERR: ' + msg);
                }
            });

            serverProcess.on('error', (err) => {
                console.error('[GEOM-BRIDGE] Failed to start server:', err.message);
                console.error('[GEOM-BRIDGE] Python executable:', python);
                console.error('[GEOM-BRIDGE] Server script:', serverPath);
                logToFile('SPAWN ERROR: ' + err.message);
                clearTimeout(startupTimeout);
                resolve(false);
            });

            serverProcess.on('exit', (code) => {
                console.log('[GEOM-BRIDGE] Server exited with code', code);
                logToFile('Server exited with code: ' + code);
                serverReady = false;
                serverProcess = null;
            });

        } catch (err) {
            console.error('[GEOM-BRIDGE] Error starting server:', err.message);
            logToFile('CATCH ERROR: ' + err.message);
            resolve(false);
        }
    });
}

/**
 * Stop the server
 */
async function stopServer() {
    if (serverProcess) {
        console.log('[GEOM-BRIDGE] Stopping server...');
        try {
            serverProcess.kill('SIGTERM');
        } catch (e) {
            console.log('[GEOM-BRIDGE] Error stopping server:', e.message);
        }
        serverProcess = null;
        serverReady = false;
    }
}

/**
 * Restart the server
 */
async function restartServer() {
    await stopServer();
    // Wait a moment for cleanup
    await new Promise(r => setTimeout(r, 500));
    return await startServer();
}

// Public API
window.GeometryBridge = {
    startServer,
    stopServer,
    restartServer,
    checkServerHealth,

    // Status
    isReady: () => serverReady,
    getWatchFolder: () => WATCH_FOLDER
};

// Auto-start server when module loads
console.log('[GEOM-BRIDGE] Module loaded, starting server...');
logToFile('Module loaded, calling startServer()...');
startServer().then(success => {
    if (success) {
        console.log('[GEOM-BRIDGE] Server started successfully');
        logToFile('SUCCESS: Server started successfully');
    } else {
        console.warn('[GEOM-BRIDGE] Server failed to start - operations will use ExtendScript fallback');
        logToFile('FAILED: Server did not start');
    }
}).catch(err => {
    console.error('[GEOM-BRIDGE] startServer() threw:', err);
    logToFile('startServer() EXCEPTION: ' + err.message);
});

// Cleanup on window unload
window.addEventListener('beforeunload', () => {
    stopServer();
});

