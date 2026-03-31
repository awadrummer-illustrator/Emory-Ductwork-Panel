/**
 * Python Geometry Bridge for Emory Ductwork
 *
 * Communicates with the persistent Python geometry server via file-based IPC.
 * The server (geometry_server.py) watches a folder for input files and writes output files.
 * This eliminates the ~7 second Python startup overhead per call.
 *
 * Usage:
 *   var result = PythonBridge.findConnections(pathsArray);
 *   var groups = PythonBridge.buildGroups(pathsArray);
 */

var PythonBridge = (function() {

    // Configuration
    var DEBUG = true;
    var POLL_INTERVAL_MS = 50;  // How often to check for output (milliseconds)
    var TIMEOUT_MS = 60000;     // Max wait time for server response (60 seconds)
    var SERVER_START_TIMEOUT_MS = 15000;  // Max wait for server to start (15 seconds)

    // Cached extension path
    var _extensionPath = null;

    /**
     * Get the extension root folder path
     */
    function getExtensionPath() {
        if (_extensionPath) return _extensionPath;

        try {
            // Try to find it based on this script's location
            var scriptFile = new File($.fileName);
            if (scriptFile.exists) {
                // Go up from jsx folder to extension root
                var jsxFolder = scriptFile.parent;
                var extensionFolder = jsxFolder.parent;
                if (extensionFolder.exists) {
                    _extensionPath = extensionFolder.fsName;
                    return _extensionPath;
                }
            }
        } catch (e) {
            if (DEBUG) addDebug("[PYBRIDGE] Error getting extension path: " + e);
        }

        // Fallback: hardcoded path
        var fallbackPath = "C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Emory-Ductwork-Panel";
        var fallbackFolder = new Folder(fallbackPath);
        if (fallbackFolder.exists) {
            _extensionPath = fallbackPath;
            return _extensionPath;
        }

        return null;
    }

    /**
     * Get the path to the geometry server script
     */
    function getServerScriptPath() {
        var extPath = getExtensionPath();
        if (!extPath) return null;

        var serverPath = extPath + "/python/geometry_server.py";
        var serverFile = new File(serverPath);
        return serverFile.exists ? serverPath : null;
    }

    /**
     * Try to start the Python geometry server via VBScript (completely hidden)
     * Uses VBScript to launch Python invisibly without blocking
     * Returns true if launch was initiated
     */
    function startServer() {
        if (DEBUG) addDebug("[PYBRIDGE] Attempting to start geometry server...");

        var serverPath = getServerScriptPath();
        if (!serverPath) {
            addDebug("[PYBRIDGE] ERROR: Could not find geometry_server.py");
            return false;
        }

        // Ensure watch folder exists
        getWatchFolder();

        // Convert forward slashes to backslashes for Windows
        var serverPathWin = serverPath.replace(/\//g, "\\");

        // Create a VBScript file to launch Python completely hidden
        var tempFolder = Folder.temp;
        var vbsFile = new File(tempFolder.fsName + "/mdux_start_server.vbs");

        // Try pythonw first (no console), then python
        var pythonCommands = ["pythonw", "python"];
        var launched = false;

        for (var i = 0; i < pythonCommands.length && !launched; i++) {
            var pythonCmd = pythonCommands[i];

            // VBScript to run Python completely hidden
            // WshShell.Run command, 0 = hidden, False = don't wait
            var vbsContent = 'Set WshShell = CreateObject("WScript.Shell")\r\n';
            vbsContent += 'WshShell.Run "' + pythonCmd + ' ""' + serverPathWin + '""", 0, False\r\n';

            if (DEBUG) addDebug("[PYBRIDGE] Trying VBScript with: " + pythonCmd);

            try {
                vbsFile.encoding = "UTF-8";
                vbsFile.open('w');
                vbsFile.write(vbsContent);
                vbsFile.close();

                // Execute the VBScript - this returns immediately
                var execResult = vbsFile.execute();
                if (DEBUG) addDebug("[PYBRIDGE] VBScript execute returned: " + execResult);

                // IMPORTANT: Do NOT delete the VBS file immediately!
                // Windows Script Host needs time to read it.
                launched = true;
            } catch (e) {
                if (DEBUG) addDebug("[PYBRIDGE] Failed with " + pythonCmd + ": " + e);
            }
        }

        if (launched) {
            if (DEBUG) addDebug("[PYBRIDGE] Server launch initiated");
        }

        return launched;
    }

    /**
     * Wait for server to be ready with timeout
     */
    function waitForServerReady(timeoutMs) {
        if (!timeoutMs) timeoutMs = 5000;
        var startTime = new Date().getTime();
        var checkInterval = 200;

        while (new Date().getTime() - startTime < timeoutMs) {
            if (isServerRunning()) {
                if (DEBUG) addDebug("[PYBRIDGE] Server ready after " + (new Date().getTime() - startTime) + "ms");
                return true;
            }
            $.sleep(checkInterval);
        }

        if (DEBUG) addDebug("[PYBRIDGE] Server not ready after " + timeoutMs + "ms timeout");
        return false;
    }

    /**
     * Ensure server is running, starting it if necessary
     * Returns true if server is running, false if it couldn't be started
     */
    function ensureServerRunning() {
        if (isServerRunning()) {
            return true;
        }

        addDebug("[PYBRIDGE] Server not running, attempting auto-start...");

        if (!startServer()) {
            addDebug("[PYBRIDGE] Failed to launch server");
            return false;
        }

        // Wait for server to become ready
        return waitForServerReady(5000);
    }

    /**
     * Get the watch folder path (same as geometry_server.py uses)
     */
    function getWatchFolder() {
        // Use system temp folder + mdux_watch (matches geometry_server.py)
        var tempFolder = Folder.temp;
        var watchFolder = new Folder(tempFolder.fsName + "/mdux_watch");

        // Create if it doesn't exist
        if (!watchFolder.exists) {
            watchFolder.create();
        }

        return watchFolder.fsName;
    }

    /**
     * Generate a unique request ID
     */
    function generateRequestId() {
        var timestamp = new Date().getTime();
        var random = Math.floor(Math.random() * 10000);
        return "req_" + timestamp + "_" + random;
    }

    /**
     * Check if the geometry server is running by reading its status file
     */
    function isServerRunning() {
        try {
            var watchFolder = getWatchFolder();
            var statusFile = new File(watchFolder + "/server_status.json");

            if (!statusFile.exists) {
                if (DEBUG) addDebug("[PYBRIDGE] Server status file not found");
                return false;
            }

            statusFile.encoding = "UTF-8";
            statusFile.open('r');
            var content = statusFile.read();
            statusFile.close();

            var status = JSON.parse(content);
            var now = new Date().getTime();
            var age = now - status.timestamp;

            // Server is running if status is "running" and timestamp is within 5 seconds
            if (status.status === "running" && age < 5000) {
                if (DEBUG) addDebug("[PYBRIDGE] Server is running (status age: " + age + "ms)");
                return true;
            } else {
                if (DEBUG) addDebug("[PYBRIDGE] Server status stale or stopped (age: " + age + "ms, status: " + status.status + ")");
                return false;
            }
        } catch (e) {
            if (DEBUG) addDebug("[PYBRIDGE] Error checking server status: " + e);
            return false;
        }
    }

    /**
     * Write JSON data to a file
     */
    function writeJsonFile(filePath, data) {
        var file = new File(filePath);
        file.encoding = "UTF-8";
        file.open('w');
        file.write(JSON.stringify(data));
        file.close();
        return file.exists;
    }

    /**
     * Read JSON data from a file
     */
    function readJsonFile(filePath) {
        var file = new File(filePath);
        if (!file.exists) {
            return null;
        }
        file.encoding = "UTF-8";
        file.open('r');
        var content = file.read();
        file.close();

        try {
            return JSON.parse(content);
        } catch (e) {
            if (DEBUG) addDebug("[PYBRIDGE] JSON parse error: " + e);
            return null;
        }
    }

    /**
     * Delete a temporary file
     */
    function deleteTempFile(filePath) {
        try {
            var file = new File(filePath);
            if (file.exists) {
                file.remove();
            }
        } catch (e) {
            // Ignore cleanup errors
        }
    }

    /**
     * Convert Illustrator PathItem to JSON-serializable format
     */
    function pathToJson(pathItem) {
        var points = [];
        try {
            var pp = pathItem.pathPoints;
            for (var i = 0; i < pp.length; i++) {
                var anchor = pp[i].anchor;
                points.push({
                    x: anchor[0],
                    y: anchor[1]
                });
            }
        } catch (e) {
            // Invalid path
        }
        return { points: points };
    }

    /**
     * Convert array of PathItems to JSON format
     */
    function pathsToJson(pathItems) {
        var paths = [];
        for (var i = 0; i < pathItems.length; i++) {
            var p = pathItems[i];
            if (!p) continue;

            var pathData = pathToJson(p);
            pathData.id = i;

            // Store reference info for later
            try {
                pathData.layerName = p.layer ? p.layer.name : null;
            } catch (e) {
                pathData.layerName = null;
            }

            paths.push(pathData);
        }
        return paths;
    }

    /**
     * Execute Python geometry operation via file-watching server
     * @param {string} operation - The operation to run (find_connections, build_groups, etc.)
     * @param {Array} pathItems - Array of Illustrator PathItems
     * @param {Object} params - Optional parameters for the operation
     * @returns {Object} Result from Python engine, or null on error
     */
    function executePython(operation, pathItems, params) {
        var startTime = new Date().getTime();

        if (DEBUG) addDebug("[PYBRIDGE] Starting " + operation + " with " + pathItems.length + " paths");

        // Ensure server is running, starting it if necessary
        if (!ensureServerRunning()) {
            addDebug("[PYBRIDGE] ERROR: Geometry server could not be started!");
            addDebug("[PYBRIDGE] Ensure Python is installed and in your system PATH.");
            addDebug("[PYBRIDGE] You can also try running Start_Geometry_Server.bat manually.");
            return null;
        }

        var watchFolder = getWatchFolder();
        var requestId = generateRequestId();

        // Convert paths to JSON
        var pathsJson = pathsToJson(pathItems);

        // Prepare input data (matches what geometry_server.py expects)
        var inputData = {
            operation: operation,
            request_id: requestId,
            paths: pathsJson,
            params: params || {}
        };

        // Write input file to watch folder
        var inputPath = watchFolder + "/" + requestId + "_input.json";
        var outputPath = watchFolder + "/" + requestId + "_output.json";

        if (!writeJsonFile(inputPath, inputData)) {
            addDebug("[PYBRIDGE] ERROR: Failed to write input file to watch folder");
            return null;
        }

        if (DEBUG) addDebug("[PYBRIDGE] Wrote request " + requestId + " to watch folder");

        // Poll for output file (server processes input and writes output)
        var result = null;
        var pollStart = new Date().getTime();
        var outputFile = new File(outputPath);

        while (new Date().getTime() - pollStart < TIMEOUT_MS) {
            // Check if output file exists
            if (outputFile.exists) {
                // Small delay to ensure file is fully written
                $.sleep(20);

                // Read result
                result = readJsonFile(outputPath);

                // Clean up output file
                deleteTempFile(outputPath);

                break;
            }

            // Brief sleep before next poll
            $.sleep(POLL_INTERVAL_MS);
        }

        var elapsed = new Date().getTime() - startTime;

        if (result) {
            if (DEBUG) addDebug("[PYBRIDGE] " + operation + " completed in " + elapsed + "ms (Python: " + (result.time_ms || "?") + "ms)");

            if (result.error) {
                addDebug("[PYBRIDGE] Python error: " + result.error);
                return null;
            }
        } else {
            addDebug("[PYBRIDGE] ERROR: No result from server (timeout after " + elapsed + "ms)");
            addDebug("[PYBRIDGE] The server may have crashed or is not processing requests.");
            // Clean up input file if it still exists (server didn't process it)
            deleteTempFile(inputPath);
        }

        return result;
    }

    /**
     * Find all connections between paths
     * @param {Array} pathItems - Array of PathItems to analyze
     * @param {number} maxDist - Maximum distance for connection detection (default: 10)
     * @returns {Object} { connections: [...], ignored_anchors: [...] }
     */
    function findConnections(pathItems, maxDist, tTolerance, allowNonVertexIntersections) {
        return executePython('find_connections', pathItems, {
            max_dist: maxDist || 10,
            t_tolerance: tTolerance,
            allow_non_vertex_intersections: allowNonVertexIntersections === true
        });
    }

    /**
     * Build groups of connected paths
     * @param {Array} pathItems - Array of PathItems to group
     * @param {number} maxDist - Maximum distance for connection detection
     * @returns {Object} { groups: [[indices], ...], connections: [...] }
     */
    function buildGroups(pathItems, maxDist) {
        return executePython('build_groups', pathItems, { max_dist: maxDist || 10 });
    }

    /**
     * Detect all intersections between paths
     * @param {Array} pathItems - Array of PathItems
     * @returns {Object} { intersections: [...] }
     */
    function detectIntersections(pathItems) {
        return executePython('detect_intersections', pathItems, {});
    }

    /**
     * Orthogonalize paths (snap + make horizontal/vertical)
     * This is the main performance bottleneck - Python is 100x+ faster
     * @param {Array} pathItems - Array of PathItems to orthogonalize
     * @param {number} snapThreshold - Snap distance threshold (default: 5)
     * @returns {Object} { paths: [{id, points}], iterations, time_ms }
     */
    function orthogonalize(pathItems, snapThreshold) {
        return executePython('orthogonalize', pathItems, {
            snap_threshold: snapThreshold || 5,
            steep_min: 17,
            steep_max: 70
        });
    }

    /**
     * Find segment-segment crossovers between different paths
     * Crossovers are where paths cross WITHOUT a vertex at the intersection
     * @param {Array} pathItems - Array of PathItems
     * @returns {Object} { crossovers: [{pathIdx, segmentIdx, crossingPathIdx, crossingSegIdx, point}], time_ms }
     */
    function findCrossovers(pathItems) {
        return executePython('find_crossovers', pathItems, {});
    }

    /**
     * Find anchor points that should snap to nearby segments
     * Uses spatial indexing for O(n log n) instead of O(nÂ²)
     * @param {Array} pathItems - Array of PathItems
     * @param {number} snapThreshold - Maximum snap distance (default: 5)
     * @returns {Object} { snaps: [{path_idx, point_idx, snap_to, distance}], time_ms }
     */
    function snapAnchors(pathItems, snapThreshold) {
        return executePython('snap_anchors', pathItems, {
            snap_threshold: snapThreshold || 5
        });
    }

    /**
     * Find collinear internal anchors suitable for register placement
     * These are internal points (not endpoints) where the path continues straight through
     * @param {Array} pathItems - Array of PathItems
     * @param {number} collinearTolerance - Dot product tolerance (default: 0.005 = ~6 degrees)
     * @returns {Object} { anchors: [{path_idx, point_idx, position, prev_point, next_point}], time_ms }
     */
    function findCollinearAnchors(pathItems, collinearTolerance) {
        return executePython('find_collinear_anchors', pathItems, {
            collinear_tolerance: collinearTolerance || 0.005
        });
    }

    /**
     * Check if Python bridge is available (server is running)
     */
    function isAvailable() {
        return isServerRunning();
    }

    // Public API
    return {
        findConnections: findConnections,
        buildGroups: buildGroups,
        detectIntersections: detectIntersections,
        orthogonalize: orthogonalize,
        findCrossovers: findCrossovers,
        snapAnchors: snapAnchors,
        findCollinearAnchors: findCollinearAnchors,
        isAvailable: isAvailable,
        isServerRunning: isServerRunning,
        startServer: startServer,
        ensureServerRunning: ensureServerRunning,

        // Expose utilities for debugging
        _getWatchFolder: getWatchFolder,
        _pathsToJson: pathsToJson,
        _getExtensionPath: getExtensionPath,
        _getServerScriptPath: getServerScriptPath
    };

})();

// Make available globally
$.global.PythonBridge = PythonBridge;

