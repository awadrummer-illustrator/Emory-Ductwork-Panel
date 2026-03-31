/**
 * Async Python Client - Non-blocking communication with Python geometry server
 *
 * This module handles file-based IPC with the Python server using async operations.
 * Unlike ExtendScript's $.sleep(), this uses setTimeout for non-blocking polling.
 */

(function() {
    'use strict';

    // Use require inside IIFE to avoid global conflicts with geometry-bridge.js
    const _fs = require('fs');
    const _path = require('path');
    const _os = require('os');

    const WATCH_FOLDER = _path.join(_os.tmpdir(), 'mdux_watch');
    const STATUS_FILE = _path.join(WATCH_FOLDER, 'server_status.json');

    /**
     * Ensure watch folder exists
     */
    function ensureWatchFolder() {
        if (!_fs.existsSync(WATCH_FOLDER)) {
            _fs.mkdirSync(WATCH_FOLDER, { recursive: true });
        }
        return WATCH_FOLDER;
    }

    /**
     * Check if server is running by reading status file
     */
    function isServerRunning() {
        try {
            if (!_fs.existsSync(STATUS_FILE)) {
                return false;
            }
            const content = _fs.readFileSync(STATUS_FILE, 'utf8');
            const status = JSON.parse(content);
            const age = Date.now() - status.timestamp;
            return status.status === 'running' && age < 5000;
        } catch (e) {
            return false;
        }
    }

    /**
     * Generate unique request ID
     */
    function generateRequestId() {
        return 'req_' + Date.now() + '_' + Math.floor(Math.random() * 10000);
    }

    /**
     * Execute a Python operation asynchronously (non-blocking)
     *
     * @param {string} operation - Operation name (orthogonalize, find_connections, etc.)
     * @param {Array} pathsJson - Array of path data objects [{id, points: [{x,y}]}]
     * @param {Object} params - Optional parameters for the operation
     * @param {Object} callbacks - { onProgress, onComplete, onError }
     * @returns {Promise} Resolves with result or rejects with error
     */
    function executePythonAsync(operation, pathsJson, params, callbacks) {
        return new Promise((resolve, reject) => {
            const startTime = Date.now();
            const requestId = generateRequestId();

            console.log(`[PY-CLIENT] Starting ${operation} with ${pathsJson.length} paths (id=${requestId})`);

            // Check if server is running
            if (!isServerRunning()) {
                const error = new Error('Python geometry server is not running. Please start it first.');
                if (callbacks && callbacks.onError) callbacks.onError(error);
                reject(error);
                return;
            }

            ensureWatchFolder();

            const inputPath = _path.join(WATCH_FOLDER, `${requestId}_input.json`);
            const outputPath = _path.join(WATCH_FOLDER, `${requestId}_output.json`);

            // Prepare request data
            const requestData = {
                operation: operation,
                request_id: requestId,
                paths: pathsJson,
                params: params || {}
            };

            // Write request file
            try {
                _fs.writeFileSync(inputPath, JSON.stringify(requestData), 'utf8');
                console.log(`[PY-CLIENT] Wrote request file: ${inputPath}`);
            } catch (e) {
                const error = new Error('Failed to write request file: ' + e.message);
                if (callbacks && callbacks.onError) callbacks.onError(error);
                reject(error);
                return;
            }

            // Poll for response (async, non-blocking)
            const pollInterval = 50; // Check every 50ms
            const maxWait = 60000;   // 60 second timeout
            let cancelled = false;

            // Store cancel function if callbacks provided
            if (callbacks) {
                callbacks.cancel = () => { cancelled = true; };
            }

            function pollForResult() {
                // Check for cancellation
                if (cancelled) {
                    console.log(`[PY-CLIENT] Request ${requestId} cancelled`);
                    try { _fs.unlinkSync(inputPath); } catch (e) {}
                    reject(new Error('Request cancelled'));
                    return;
                }

                const elapsed = Date.now() - startTime;

                // Check timeout
                if (elapsed > maxWait) {
                    console.log(`[PY-CLIENT] Request ${requestId} timed out after ${elapsed}ms`);
                    try { _fs.unlinkSync(inputPath); } catch (e) {}
                    const error = new Error(`Python operation timed out after ${maxWait/1000}s`);
                    if (callbacks && callbacks.onError) callbacks.onError(error);
                    reject(error);
                    return;
                }

                // Progress callback
                if (callbacks && callbacks.onProgress) {
                    callbacks.onProgress(elapsed, maxWait);
                }

                // Check if output file exists
                if (_fs.existsSync(outputPath)) {
                    // Brief delay to ensure file is fully written
                    setTimeout(() => {
                        try {
                            const content = _fs.readFileSync(outputPath, 'utf8');
                            const result = JSON.parse(content);

                            // Cleanup files
                            try { _fs.unlinkSync(inputPath); } catch (e) {}
                            try { _fs.unlinkSync(outputPath); } catch (e) {}

                            const totalTime = Date.now() - startTime;
                            console.log(`[PY-CLIENT] ${operation} completed in ${totalTime}ms (Python: ${result.time_ms || '?'}ms)`);

                            if (result.error) {
                                const error = new Error('Python error: ' + result.error);
                                if (callbacks && callbacks.onError) callbacks.onError(error);
                                reject(error);
                            } else {
                                if (callbacks && callbacks.onComplete) callbacks.onComplete(result);
                                resolve(result);
                            }
                        } catch (e) {
                            const error = new Error('Failed to parse response: ' + e.message);
                            if (callbacks && callbacks.onError) callbacks.onError(error);
                            reject(error);
                        }
                    }, 30);
                } else {
                    // Not ready yet, poll again
                    setTimeout(pollForResult, pollInterval);
                }
            }

            // Start polling
            pollForResult();
        });
    }

    /**
     * Orthogonalize paths (async)
     */
    function orthogonalizeAsync(pathsJson, snapThreshold, callbacks) {
        return executePythonAsync('orthogonalize', pathsJson, {
            snap_threshold: snapThreshold || 5,
            steep_min: 17,
            steep_max: 70
        }, callbacks);
    }

    /**
     * Find connections between paths (async)
     */
    function findConnectionsAsync(pathsJson, maxDist, callbacks) {
        return executePythonAsync('find_connections', pathsJson, {
            max_dist: maxDist || 10
        }, callbacks);
    }

    /**
     * Build groups of connected paths (async)
     */
    function buildGroupsAsync(pathsJson, maxDist, callbacks) {
        return executePythonAsync('build_groups', pathsJson, {
            max_dist: maxDist || 10
        }, callbacks);
    }

    // Export API to window
    window.PythonClient = {
        executePythonAsync,
        orthogonalizeAsync,
        findConnectionsAsync,
        buildGroupsAsync,
        isServerRunning,
        ensureWatchFolder,
        WATCH_FOLDER
    };

    console.log('[PY-CLIENT] Async Python client loaded');
})();
