// Minimal JSON polyfill for ExtendScript environments lacking native support
if (typeof JSON === "undefined") {
    JSON = {};
}
if (typeof JSON.stringify !== "function") {
    JSON.stringify = function (value) {
        function quote(str) {
            return '"' + str.replace(/\\/g, '\\\\')
                .replace(/"/g, '\\"')
                .replace(/\r/g, '\\r')
                .replace(/\n/g, '\\n')
                .replace(/\f/g, '\\f')
                .replace(/\t/g, '\\t')
                .replace(/\b/g, '\\b') + '"';
        }
        function stringify(val) {
            if (val === null) return "null";
            var type = typeof val;
            if (type === "number" || type === "boolean") return String(val);
            if (type === "string") return quote(val);
            if (val instanceof Array) {
                var arr = [];
                for (var i = 0; i < val.length; i++) arr.push(stringify(val[i]));
                return "[" + arr.join(",") + "]";
            }
            if (type === "object") {
                var props = [];
                for (var key in val) {
                    if (val.hasOwnProperty(key) && typeof val[key] !== "undefined" && typeof val[key] !== "function") {
                        props.push(quote(String(key)) + ":" + stringify(val[key]));
                    }
                }
                return "{" + props.join(",") + "}";
            }
            return "null";
        }
        return stringify(value);
    };
}
if (typeof JSON.parse !== "function") {
    JSON.parse = function (text) {
        if (typeof text !== "string") return null;
        try {
            return eval('(' + text + ')');
        } catch (e) {
            return null;
        }
    };
}

// ============================================================================
// DEBUG CONFIGURATION - ALL DEBUG FLAGS IN ONE PLACE
// ============================================================================
// Set these to true/false to control logging throughout the entire script
// ============================================================================
$.global.MDUX_DEBUG = {
    ENABLED: true,              // Master switch - enables/disables ALL debug logging
    CONNECTIONS: true,          // Log path connection detection details
    INTERSECTIONS: true,        // Log intersection vertex detection
    COMPOUNDING: true,          // Log compounding operations
    COMPONENTS: true            // Log component placement (units, registers, etc.)
};
// ============================================================================

// ============================================================================
// PYTHON GEOMETRY BRIDGE - High-performance geometry operations
// ============================================================================
$.global.MDUX_USE_PYTHON = true;
$.global.MDUX_PYTHON_FALLBACK = false;  // Set to true to enable slow ExtendScript fallback

// Inline Python Bridge - File-based communication (Socket not available in CEP)
var PythonBridge = (function() {
    var ENGINE_SCRIPT = "geometry_engine.py";
    var WATCH_FOLDER = Folder.temp.fsName + "/mdux_watch";
    var serverAvailable = null; // null = unknown, true/false = tested

    // Safe debug function
    function pyDebug(msg) {
        try {
            if ($.global.addDebug && typeof $.global.addDebug === 'function') {
                $.global.addDebug(msg);
            } else if (typeof addDebug === 'function') {
                addDebug(msg);
            }
        } catch (e) {
            try { $.writeln("[PYBRIDGE] " + msg); } catch (e2) { }
        }
    }

    function getEnginePath() {
        try {
            var deployedPath = "C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Magic-Ductwork-Panel/python/" + ENGINE_SCRIPT;
            if (new File(deployedPath).exists) return deployedPath;
            var devPath = "e:/Work/Work/Custom Sketchup, Illustrator and Photoshop Scripts and Extensions/Illustrator/Extensions/Magic-Ductwork-Panel/python/" + ENGINE_SCRIPT;
            if (new File(devPath).exists) return devPath;
            return null;
        } catch (e) { return null; }
    }

    function ensureWatchFolder() {
        var folder = new Folder(WATCH_FOLDER);
        if (!folder.exists) folder.create();
        return folder.exists;
    }

    function pathsToJson(pathItems) {
        var paths = [];
        for (var i = 0; i < pathItems.length; i++) {
            var p = pathItems[i];
            if (!p) continue;
            var points = [];
            try {
                var pp = p.pathPoints;
                for (var j = 0; j < pp.length; j++) {
                    points.push({ x: pp[j].anchor[0], y: pp[j].anchor[1] });
                }
            } catch (e) { }
            var pathData = { id: i, points: points };
            try { pathData.layerName = p.layer ? p.layer.name : null; } catch (e) { }
            paths.push(pathData);
        }
        return paths;
    }

    function writeJsonFile(filePath, data) {
        var file = new File(filePath);
        file.encoding = "UTF-8";
        file.open('w');
        file.write(JSON.stringify(data));
        file.close();
        return file.exists;
    }

    function readJsonFile(filePath) {
        var file = new File(filePath);
        if (!file.exists) return null;
        file.encoding = "UTF-8";
        file.open('r');
        var content = file.read();
        file.close();
        try { return JSON.parse(content); } catch (e) { return null; }
    }

    /**
     * Check if server is running by looking for its heartbeat file
     */
    function checkServerHealth() {
        ensureWatchFolder();
        var statusFile = new File(WATCH_FOLDER + "/server_status.json");
        if (!statusFile.exists) {
            pyDebug("[PYBRIDGE] No server_status.json found");
            return false;
        }
        var status = readJsonFile(statusFile.fsName);
        if (!status) {
            pyDebug("[PYBRIDGE] Could not read server_status.json");
            return false;
        }
        // Check if status is recent (within last 10 seconds)
        var now = new Date().getTime();
        var statusTime = status.timestamp || 0;
        var age = now - statusTime;
        if (age > 10000) {
            pyDebug("[PYBRIDGE] Server status stale: " + age + "ms old");
            return false;
        }
        return true;
    }

    /**
     * Execute operation via file-based communication with persistent server
     */
    function executeViaServer(operation, pathItems, params) {
        var startTime = new Date().getTime();
        var requestId = "req_" + startTime + "_" + Math.floor(Math.random() * 10000);

        pyDebug("[PYBRIDGE] File request: " + operation + " (" + pathItems.length + " paths, id=" + requestId + ")");
        ensureWatchFolder();

        var inputPath = WATCH_FOLDER + "/" + requestId + "_input.json";
        var outputPath = WATCH_FOLDER + "/" + requestId + "_output.json";

        // Write request file
        var requestData = {
            operation: operation,
            request_id: requestId,
            paths: pathsToJson(pathItems),
            params: params || {}
        };

        if (!writeJsonFile(inputPath, requestData)) {
            pyDebug("[PYBRIDGE] ERROR: Failed to write input file");
            return null;
        }
        pyDebug("[PYBRIDGE] Wrote input file, waiting for response...");

        // Wait for output file (server will process and create it)
        var maxWait = 60000; // 60 second timeout
        var pollInterval = 20; // Check every 20ms (faster polling)
        var waited = 0;

        while (waited < maxWait) {
            $.sleep(pollInterval);
            waited += pollInterval;

            // Update progress window if available (keeps UI responsive)
            if (waited % 200 === 0 && $.global.MDUX_PROGRESS_WIN) {
                try { $.global.MDUX_PROGRESS_WIN.update(); } catch (e) { }
            }

            // Check for output file (create fresh File object to avoid caching)
            if (new File(outputPath).exists) {
                $.sleep(30); // Brief pause for file write to complete
                var result = readJsonFile(outputPath);

                // Cleanup files
                try { new File(inputPath).remove(); } catch (e) { }
                try { new File(outputPath).remove(); } catch (e) { }

                var elapsed = new Date().getTime() - startTime;
                if (result) {
                    pyDebug("[PYBRIDGE] Response in " + elapsed + "ms (processing: " + (result.time_ms || "?") + "ms)");
                    return result;
                } else {
                    pyDebug("[PYBRIDGE] ERROR: Could not parse output file");
                    return null;
                }
            }
        }

        // Timeout
        try { new File(inputPath).remove(); } catch (e) { }
        pyDebug("[PYBRIDGE] ERROR: Server timeout after " + maxWait + "ms");
        return null;
    }

    /**
     * Main execution function
     */
    function executePython(operation, pathItems, params) {
        pyDebug("[PYBRIDGE] === Starting " + operation + " with " + pathItems.length + " paths ===");

        // Check server availability
        if (serverAvailable === null) {
            serverAvailable = checkServerHealth();
            pyDebug("[PYBRIDGE] Server check: " + (serverAvailable ? "RUNNING" : "NOT RUNNING"));
        }

        if (serverAvailable) {
            var result = executeViaServer(operation, pathItems, params);
            if (result) {
                pyDebug("[PYBRIDGE] SUCCESS: " + (result.paths ? result.paths.length : 0) + " paths");
                if (result.error) {
                    pyDebug("[PYBRIDGE] Server error: " + result.error);
                    return null;
                }
                return result;
            }
            serverAvailable = null;
            pyDebug("[PYBRIDGE] Request failed, server may be down");
        }

        pyDebug("[PYBRIDGE] Server not available - Python acceleration disabled");
        return null;
    }

    return {
        findConnections: function(p, d) { return executePython('find_connections', p, { max_dist: d || 10 }); },
        buildGroups: function(p, d) { return executePython('build_groups', p, { max_dist: d || 10 }); },
        orthogonalize: function(p, t, lockedPts) { return executePython('orthogonalize', p, { snap_threshold: t || 5, steep_min: 17, steep_max: 70, locked_points: lockedPts || [] }); },
        isAvailable: function() {
            if (serverAvailable === null) {
                serverAvailable = checkServerHealth();
            }
            if (serverAvailable) {
                pyDebug("[PYBRIDGE] isAvailable: YES - server running");
                return true;
            }
            var ep = getEnginePath();
            pyDebug("[PYBRIDGE] isAvailable: Server not running, engine=" + (ep ? "found" : "missing"));
            return false;
        },
        checkServer: function() {
            serverAvailable = checkServerHealth();
            return serverAvailable;
        },
        resetServerStatus: function() {
            serverAvailable = null;
        },
        getWatchFolder: function() {
            return WATCH_FOLDER;
        },
        _getEnginePath: getEnginePath
    };
})();
$.global.PythonBridge = PythonBridge;
// ============================================================================

// Emergency Shim: Define yieldToUI globally to prevent ReferenceError if any calls remain.
// This function intentionally does nothing.
if (typeof yieldToUI === "undefined") {
    var yieldToUI = function () { };
}

/**
 * MagicFinal Constants Configuration
 *
 * All configuration constants, global variables, and helper functions
 * for branch width management and diagnostics.
 *
 * This file can be included in other scripts:
 * //@include "config/Constants.jsxinc"
 */

// ========================================
// DISTANCE & TOLERANCE CONSTANTS
// ========================================

var CLOSE_DIST = 10; // px for loose connection grouping
var UNIT_MERGE_DIST = 10; // px tolerance to merge clustered unit anchors (MUST match CLOSE_DIST so units behave like registers)
var THERMOSTAT_JUNCTION_DIST = 6; // px tolerance to snap thermostat line endpoints to duct junctions
var CONNECTION_DIST = 2; // px stricter threshold for actual compounding
var SNAP_THRESHOLD = 5; // px for snapping anchors
var IGNORED_DIST = 4; // px stricter threshold for Ignored-layer proximity (keeps CLOSE_DIST behavior unchanged)
var RECONNECT_CAPTURE_DIST = Math.max(CONNECTION_DIST, SNAP_THRESHOLD); // px tolerance to remember original junctions

// ========================================
// ANGLE & ITERATION CONSTANTS
// ========================================

var STEEP_MIN = 17; // deg for not orthogonalizing
var STEEP_MAX = 70; // deg for not orthogonalizing
var MAX_ITER = 8; // maximum refinement iterations

// ========================================
// METADATA TAG PREFIXES
// ========================================

var ORTHO_LOCK_TAG = "MD:ORTHO_LOCK"; // marker note to skip re-orthogonalizing processed paths
var ROT_OVERRIDE_PREFIX = "MD:ROT="; // marker prefix for rotation override notes
var ROT_BASE_PREFIX = "MD:BASE_ROT="; // base rotation metadata for paths
var POINT_ROT_PREFIX = "MD:POINT_ROT="; // marker prefix for anchor rotation notes
var PLACED_ROT_PREFIX = "MD:PLACED_ROT="; // marker prefix for placed item rotation notes
var PLACED_SCALE_PREFIX = "MD:PLACED_SCALE="; // marker prefix for placed item custom scale
var PLACED_BASE_ROT_PREFIX = "MD:PLACED_BASE_ROT="; // marker prefix for placed item base rotation from linked file
var BLUE_RIGHTANGLE_BRANCH_TAG = "MD:BLUE90_BRANCH"; // marks blue 90° branch paths that need special handling
var ORIGINAL_LAYER_PREFIX = "MD:ORIG_LAYER="; // marker prefix for remembering original layer
var BRANCH_START_PREFIX = "MD:BRANCH_START="; // marker prefix for custom branch start width
var REGISTER_WIRE_TAG = "MD:REGISTER_WIRE"; // marker to identify generated register wire connectors
var PRE_ORTHO_PREFIX = "MD:PREORTHO="; // marker prefix for storing pre-orthogonalization geometry
var PRE_SCALE_PREFIX = "MD:PRESCALE="; // marker prefix for storing pre-scale metadata
var CENTERLINE_NOTE_TAG = "MD:CENTERLINE"; // marker to identify processed centerlines
var CENTERLINE_ID_PREFIX = "MD:CLID="; // marker prefix for per-centerline unique id

// ========================================
// HELPER: DYNAMIC LOG FILE PATH
// ========================================

function getLogFilePath(logFileName) {
    try {
        // Get the extension root folder dynamically
        var logFolder = $.global.MDUX_JSX_FOLDER;
        if (!logFolder) {
            // Fallback: use current file location
            logFolder = File($.fileName).parent;
        }

        // Convert to string path
        var logFolderPath = "";
        try {
            logFolderPath = logFolder.fsName || logFolder.toString();
        } catch (e) {
            logFolderPath = logFolder.toString();
        }

        // Construct log file path
        var logFilePath = logFolderPath;
        if (logFilePath.charAt(logFilePath.length - 1) !== "/" && logFilePath.charAt(logFilePath.length - 1) !== "\\") {
            logFilePath += "/";
        }
        logFilePath += logFileName;

        return logFilePath;
    } catch (e) {
        // Fallback to a default path if everything fails
        return "C:\\Users\\Chris\\AppData\\Roaming\\Adobe\\CEP\\extensions\\Magic-Ductwork-Panel\\" + logFileName;
    }
}

// ========================================
// FEATURE FLAGS
// ========================================

var BLUE_TRUNK_TAPER_ENABLED = true; // allow blue trunks to taper (set false to restore legacy behavior)

// ========================================
// GLOBAL STATE VARIABLES
// ========================================

var BLUE_BRANCH_CONNECTIONS = []; // caches detected blue right-angle branch connections
var BRANCH_CUSTOM_START_WIDTHS = {}; // per-centerline custom start widths (full width in pts)
var LIMIT_BRANCH_PROCESS_MAP = null; // optional map of centerline ids to limit processing scope
var initialPlacedTransforms = {}; // Captures placed item transforms BEFORE processing
var TRUNK_ENDPOINT_WIDTHS = {}; // Global cache for trunk endpoint widths (key: "x,y", value: halfWidth)

// ========================================
// BRANCH WIDTH MANAGEMENT FUNCTIONS
// ========================================

function resetBranchCustomStartWidths() {
    BRANCH_CUSTOM_START_WIDTHS = {};
}

function setBranchCustomStartWidth(centerlineId, fullWidth) {
    if (!centerlineId) return;
    if (typeof fullWidth !== "number" || !isFinite(fullWidth) || fullWidth <= 0) {
        clearBranchCustomStartWidth(centerlineId);
        return;
    }
    BRANCH_CUSTOM_START_WIDTHS[centerlineId] = fullWidth || 20;
}

function getBranchCustomStartWidth(centerlineId) {
    if (!centerlineId) return null;
    var fullWidth = BRANCH_CUSTOM_START_WIDTHS.hasOwnProperty(centerlineId) ? BRANCH_CUSTOM_START_WIDTHS[centerlineId] : null;
    if (typeof fullWidth !== "number" || !isFinite(fullWidth) || fullWidth <= 0) {
        return null;
    }
    return fullWidth || 20;
}

function clearBranchCustomStartWidth(centerlineId) {
    if (!centerlineId) return;
    if (BRANCH_CUSTOM_START_WIDTHS.hasOwnProperty(centerlineId)) {
        delete BRANCH_CUSTOM_START_WIDTHS[centerlineId];
    }
}

// ========================================
// DIAGNOSTICS
// ========================================

// Lightweight runtime diagnostics (incremental, non-fatal)
var DIAG = {
    skippedRegisterNearThermostat: 0,
    skippedRegisterNearUnit: 0,
    createdRegisters: 0,
    thermostatsCreated: 0,
    thermostatsSkipped: 0,
    registerSkipReasons: [],
    thermostatSkipReasons: []
};

// Debug log
var debugLog = [];

function logDebug(msg) {
    debugLog.push(msg);
}

function writeDebugLog() {
    // Disabled to prevent lockups - debug info shown in dialog instead
    // If you need file logging, uncomment and update the path:
    /*
    try {
        var logFile = new File("E:/OneDrive/Desktop/magic_ductwork_debug.txt");
        if (logFile.open("w")) {
            logFile.write(debugLog.join("\n\n========================================\n\n"));
            logFile.close();
        }
    } catch (e) {
        // Silently fail - don't interrupt workflow
    }
    */
}
/**
 * MagicFinal Layer Definitions
 *
 * All layer name arrays, mappings, and layer-related helper functions.
 * Defines the structure of ductwork layers and their relationships.
 *
 * This file can be included in other scripts:
 * //@include "config/LayerDefinitions.jsxinc"
 *
 * Note: Some functions reference 'doc' which must be defined globally.
 */

// ========================================
// LAYER NAME ARRAYS
// ========================================

// Ductwork piece layers (used for placing pieces and anchor checks)
var DUCTWORK_PIECES = [
    "Thermostats",
    "Units",
    "Secondary Exhaust Registers",
    "Exhaust Registers",
    "Orange Register",
    "Rectangular Registers",
    "Square Registers",
    "Circular Registers"
];

// Ductwork line layers (line-only layers that should not be orthogonalized, etc.)
var DUCTWORK_LINES = [
    "Green Ductwork",
    "Light Green Ductwork",
    "Blue Ductwork",
    "Orange Ductwork",
    "Light Orange Ductwork",
    "Thermostat Lines"
];

// Available ductwork color options
var DUCTWORK_COLOR_OPTIONS = [
    "Green Ductwork",
    "Light Green Ductwork",
    "Blue Ductwork",
    "Orange Ductwork",
    "Light Orange Ductwork"
];

// ========================================
// STYLE MAPPINGS
// ========================================

// Graphic style mapping
var NORMAL_STYLE_MAP = {
    "Green Ductwork": "Green Ductwork",
    "Light Green Ductwork": "Light Green Ductwork",
    "Blue Ductwork": "Blue Ductwork",
    "Orange Ductwork": "Orange Ductwork",
    "Light Orange Ductwork": "Light Orange Ductwork",
    "Thermostat Lines": "Thermostat Lines"
};

// Layer name to color name mapping
var LAYER_TO_COLOR_NAME = {
    "Green Ductwork": "Green Ductwork",
    "Light Green Ductwork": "Light Green Ductwork",
    "Blue Ductwork": "Blue Ductwork",
    "Orange Ductwork": "Orange Ductwork",
    "Light Orange Ductwork": "Light Orange Ductwork"
};

// Ductwork color to register layer mapping
// Each ductwork color places registers on its respective register layer
var DUCTWORK_COLOR_TO_REGISTER_MAP = {
    "Blue Ductwork": "Square Registers",
    "Green Ductwork": "Exhaust Registers",
    "Light Green Ductwork": "Exhaust Registers",
    "Orange Ductwork": "Exhaust Registers",
    "Light Orange Ductwork": "Exhaust Registers"
};

// ========================================
// LAYER HELPER FUNCTIONS
// ========================================

// Returns the register layer name for a given ductwork color layer
function getRegisterLayerForDuctwork(ductworkLayerName) {
    if (!ductworkLayerName) return "Exhaust Registers";
    if (DUCTWORK_COLOR_TO_REGISTER_MAP.hasOwnProperty(ductworkLayerName)) {
        return DUCTWORK_COLOR_TO_REGISTER_MAP[ductworkLayerName];
    }
    return "Exhaust Registers"; // Default
}

// Returns true if the layer name is one of the 5 ductwork color layers (not Thermostat Lines)
function isDuctworkColorLayer(name) {
    if (!name) return false;
    for (var i = 0; i < DUCTWORK_COLOR_OPTIONS.length; i++) {
        if (DUCTWORK_COLOR_OPTIONS[i] === name) return true;
    }
    return false;
}

// Safely get a path's layer name
function getPathLayerName(pathItem) {
    try {
        if (!pathItem) return "";
        var layer = pathItem.layer;
        return layer ? layer.name : "";
    } catch (e) {
        return "";
    }
}

// Check if two paths are on the same ductwork layer
function areSameDuctworkLayer(pathA, pathB) {
    var layerA = getPathLayerName(pathA);
    var layerB = getPathLayerName(pathB);
    if (!layerA || !layerB) return false;
    if (!isDuctworkColorLayer(layerA) || !isDuctworkColorLayer(layerB)) return false;
    return layerA === layerB;
}

function getColorNameForLayer(layerName) {
    if (!layerName) return null;
    if (LAYER_TO_COLOR_NAME.hasOwnProperty(layerName)) return LAYER_TO_COLOR_NAME[layerName];
    return null;
}

function getNormalLayerNameFromColor(colorName) {
    if (!colorName) return "Green Ductwork";
    switch (colorName) {
        case "Green Ductwork":
        case "Light Green Ductwork":
        case "Blue Ductwork":
        case "Orange Ductwork":
        case "Light Orange Ductwork":
            return colorName;
        default:
            return "Green Ductwork";
    }
}

function isBlueDuctworkLayerName(name) {
    if (!name) return false;
    var lower = ("" + name).toLowerCase();
    return lower === "blue ductwork";
}

function isDuctworkLineLayer(name) {
    if (!name) return false;
    var lower = ("" + name).toLowerCase();
    for (var i = 0; i < DUCTWORK_LINES.length; i++) {
        var entry = DUCTWORK_LINES[i];
        if (typeof entry === "string" && lower === entry.toLowerCase()) {
            return true;
        }
    }
    return false;
}

function isDuctworkPieceLayerName(name) {
    if (typeof name !== 'string') return false;
    var n = name.trim();
    for (var i = 0; i < DUCTWORK_PIECES.length; i++) {
        if (DUCTWORK_PIECES[i] === n) return true;
    }
    return false;
}

// ========================================
// LAYER ACCESSORS
// ========================================

// Returns a shallow copy of the ductwork piece layer names
function getDuctworkPieceLayerNames() {
    return DUCTWORK_PIECES.slice();
}

// Returns an array of Layer objects that exist in the document for the ductwork pieces
// Skips missing layers silently. Accepts optional docParam (defaults to active doc).
// NOTE: Requires 'doc' to be defined globally
function getDuctworkPieceLayers(docParam) {
    docParam = docParam || doc;
    var layers = [];
    for (var i = 0; i < DUCTWORK_PIECES.length; i++) {
        try {
            var l = docParam.layers.getByName(DUCTWORK_PIECES[i]);
            if (l) layers.push(l);
        } catch (e) {
            // layer not found — skip
        }
    }
    return layers;
}

// Collects and returns all PathItems from the named Ductwork Pieces layers
// NOTE: Requires getPathsOnLayerAll() to be defined (from LayerUtils)
function getAllPathsInDuctworkPieces(docParam) {
    docParam = docParam || doc;
    var all = [];
    var names = getDuctworkPieceLayerNames();
    for (var i = 0; i < names.length; i++) {
        try {
            var paths = getPathsOnLayerAll(names[i]);
            for (var j = 0; j < paths.length; j++) all.push(paths[j]);
        } catch (e) {
            // skip
        }
    }
    return all;
}

// ========================================
// STYLE HELPERS
// ========================================

function getGraphicStyleNameForLayer(layerName) {
    if (!layerName) return null;
    if (NORMAL_STYLE_MAP.hasOwnProperty(layerName)) return NORMAL_STYLE_MAP[layerName];
    return null;
}

function normalizeStyleKey(name) {
    if (!name) return "";
    return ("" + name).toLowerCase().replace(/[^a-z0-9]/g, "");
}

// Flexible graphic style lookup that handles name variations
// NOTE: Requires 'doc' to be defined globally
function getGraphicStyleByNameFlexible(name) {
    if (!name) return null;
    try {
        var direct = doc.graphicStyles.getByName(name);
        if (direct) return direct;
    } catch (e) { }
    var targetKey = normalizeStyleKey(name);
    if (!targetKey) return null;
    try {
        var styles = doc.graphicStyles;
        for (var i = 0; i < styles.length; i++) {
            var candidate = styles[i];
            if (!candidate) continue;
            var candName = candidate.name || "";
            if (normalizeStyleKey(candName) === targetKey) {
                return candidate;
            }
        }
    } catch (e2) { }
    return null;
}
/**
 * MagicFinal Color Maps
 *
 * All color definitions for centerlines.
 * Provides RGB color values and helper functions for color management.
 *
 * This file can be included in other scripts:
 * //@include "config/ColorMaps.jsxinc"
 *
 * Note: Some functions reference 'doc' which must be defined globally.
 */

// ========================================
// COLOR MAP DEFINITIONS
// ========================================

// Centerline stroke colors (RGB values with hex references)
var CENTERLINE_COLOR_MAP = {
    "Blue Ductwork": [0, 0, 255],           // hex: 0000ff
    "Green Ductwork": [0, 127, 0],          // hex: 007f00
    "Orange Ductwork": [255, 64, 31],       // hex: ff401f
    "Light Orange Ductwork": [255, 166, 72],      // hex: ffa648
    "Light Green Ductwork": [0, 206, 0],          // hex: 00ce00
    "Thermostat Lines": [255, 30, 38]
};

// ========================================
// COLOR HELPER FUNCTIONS
// ========================================

/**
 * Creates an RGB color object with the specified values
 * @param {number} r - Red value (0-255)
 * @param {number} g - Green value (0-255)
 * @param {number} b - Blue value (0-255)
 * @returns {RGBColor} Illustrator RGBColor object
 */
function createRGBColor(r, g, b) {
    var rgb = new RGBColor();
    rgb.red = r;
    rgb.green = g;
    rgb.blue = b;
    return rgb;
}

/**
 * Gets the centerline stroke color for a specific layer
 * @param {string} layerName - Name of the layer
 * @returns {RGBColor|null} RGB color object or null if not found
 */
function getCenterlineStrokeColor(layerName) {
    if (!layerName) return null;
    if (!CENTERLINE_COLOR_MAP.hasOwnProperty(layerName)) return null;
    var arr = CENTERLINE_COLOR_MAP[layerName];
    return createRGBColor(arr[0], arr[1], arr[2]);
}

/**
 * Gets a swatch color by name from the document
 * NOTE: Requires 'doc' to be defined globally
 * NOTE: Requires duplicateColor() function to be available
 * @param {string} name - Name of the swatch
 * @returns {Color|null} Duplicated color object or null if not found
 */
function getSwatchColorByName(name) {
    if (!name) return null;
    try {
        var swatch = doc.swatches.getByName(name);
        if (swatch && swatch.color) {
            return duplicateColor(swatch.color);
        }
    } catch (e) { }
    return null;
}
/**
 * MagicFinal Geometry Utilities
 *
 * Pure geometric functions for distance calculations, vector math,
 * bounds operations, intersection detection, and path analysis.
 *
 * This file can be included in other scripts:
 * //@include "lib/GeometryUtils.jsxinc"
 *
 * Dependencies: None (pure math functions)
 */

// ========================================
// BASIC DISTANCE & VECTOR FUNCTIONS
// ========================================

/**
 * Calculates squared distance between two points (faster than dist)
 * @param {Array} a - Point [x, y]
 * @param {Array} b - Point [x, y]
 * @returns {number} Squared distance
 */
function dist2(a, b) {
    var dx = a[0] - b[0];
    var dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

/**
 * Calculates Euclidean distance between two points
 * @param {Array} a - Point [x, y]
 * @param {Array} b - Point [x, y]
 * @returns {number} Distance
 */
function dist(a, b) {
    return Math.sqrt(dist2(a, b));
}

/**
 * Calculates dot product of two vectors
 * @param {Array} a - Vector [x, y]
 * @param {Array} b - Vector [x, y]
 * @returns {number} Dot product
 */
function dot(a, b) {
    return a[0] * b[0] + a[1] * b[1];
}

/**
 * Checks if two points are almost equal within epsilon
 * @param {Array} a - Point [x, y]
 * @param {Array} b - Point [x, y]
 * @param {number} eps - Epsilon tolerance (default: 0.01)
 * @returns {boolean} True if points are almost equal
 */
function almostEqualPoints(a, b, eps) {
    eps = eps || 0.01;
    return Math.abs(a[0] - b[0]) < eps && Math.abs(a[1] - b[1]) < eps;
}

/**
 * Calculates length of a vector
 * @param {Array} v - Vector [x, y]
 * @returns {number} Vector length
 */
function vectorLength(v) {
    if (!v) return 0;
    return Math.sqrt(v[0] * v[0] + v[1] * v[1]);
}

/**
 * Normalizes a vector to unit length
 * @param {Array} v - Vector [x, y]
 * @returns {Array|null} Normalized vector or null if length is zero
 */
function normalizeVector(v) {
    if (!v) return null;
    var len = vectorLength(v);
    if (len < 0.0001) return null;
    return [v[0] / len, v[1] / len];
}

// ========================================
// ANGLE UTILITIES
// ========================================

/**
 * Normalizes an angle to 0-360 range
 * @param {number} angleDeg - Angle in degrees
 * @returns {number} Normalized angle
 */
function normalizeAngle(angleDeg) {
    if (!isFinite(angleDeg)) return 0;
    var normalized = angleDeg % 360;
    if (normalized < 0) normalized += 360;
    return Math.round(normalized * 1000) / 1000;
}

/**
 * Checks if angle is near a multiple of 45 degrees
 * @param {number} angleDeg - Angle in degrees
 * @param {number} tolerance - Tolerance in degrees (default: 0.5)
 * @returns {boolean} True if near multiple of 45
 */
function isAngleNearMultipleOf45Global(angleDeg, tolerance) {
    if (typeof angleDeg !== "number") return false;
    var tol = (typeof tolerance === "number") ? Math.abs(tolerance) : 0.5;
    var normalized = ((angleDeg % 180) + 180) % 180;
    var remainder = normalized % 45;
    return remainder <= tol || 45 - remainder <= tol;
}

function isAngleNearMultipleOf45(angleDeg, tolerance) {
    return isAngleNearMultipleOf45Global(angleDeg, tolerance);
}

// ========================================
// PATH GEOMETRY FUNCTIONS
// ========================================

/**
 * Gets the direction vector at a path endpoint
 * @param {PathItem} pathItem - Illustrator path
 * @param {number} endpointIndex - Index of endpoint
 * @returns {Array|null} Normalized direction vector or null
 */
function getEndpointDirectionVector(pathItem, endpointIndex) {
    if (!pathItem) return null;
    var pts = null;
    try { pts = pathItem.pathPoints; } catch (e) { pts = null; }
    if (!pts || pts.length < 2) return null;

    var count = pts.length;
    if (endpointIndex <= 0) {
        var a0 = pts[0].anchor;
        var a1 = pts[1].anchor;
        return normalizeVector([a1[0] - a0[0], a1[1] - a0[1]]);
    }
    if (endpointIndex >= count - 1) {
        var aPrev = pts[count - 2].anchor;
        var aLast = pts[count - 1].anchor;
        return normalizeVector([aLast[0] - aPrev[0], aLast[1] - aPrev[1]]);
    }
    var before = pts[endpointIndex - 1].anchor;
    var after = pts[endpointIndex + 1].anchor;
    var anchor = pts[endpointIndex].anchor;
    var vecBefore = normalizeVector([anchor[0] - before[0], anchor[1] - before[1]]);
    var vecAfter = normalizeVector([after[0] - anchor[0], after[1] - anchor[1]]);
    if (!vecBefore && !vecAfter) return null;
    if (vecBefore && !vecAfter) return vecBefore;
    if (!vecBefore && vecAfter) return vecAfter;
    // If both exist, choose the longer supporting segment (favoring smoother transitions)
    var beforeLen = dist(anchor, before);
    var afterLen = dist(anchor, after);
    return beforeLen >= afterLen ? vecBefore : vecAfter;
}

/**
 * Calculates total length of a path by summing segment distances
 * @param {PathItem} pathItem - Illustrator path
 * @returns {number} Total path length
 */
function getPathTotalLength(pathItem) {
    if (!pathItem) return 0;
    var pts = null;
    try { pts = pathItem.pathPoints; } catch (e) { pts = null; }
    if (!pts || pts.length < 2) return 0;
    var total = 0;
    for (var i = 1; i < pts.length; i++) {
        total += dist(pts[i - 1].anchor, pts[i].anchor);
    }
    return total;
}

/**
 * Resolves which segment index to use for an endpoint based on direction
 * @param {PathItem} pathItem - Illustrator path
 * @param {number} endpointIndex - Index of endpoint
 * @param {Array} referenceDirection - Optional reference direction vector
 * @returns {number} Segment index
 */
function resolveSegmentIndexForEndpoint(pathItem, endpointIndex, referenceDirection) {
    if (!pathItem) return 0;
    var pts = null;
    try { pts = pathItem.pathPoints; } catch (e) { pts = null; }
    if (!pts || pts.length < 2) return 0;

    var lastIndex = pts.length - 1;
    if (endpointIndex <= 0) return 0;
    if (endpointIndex >= lastIndex) return Math.max(lastIndex - 1, 0);

    var segBefore = normalizeVector([
        pts[endpointIndex].anchor[0] - pts[endpointIndex - 1].anchor[0],
        pts[endpointIndex].anchor[1] - pts[endpointIndex - 1].anchor[1]
    ]);
    var segAfter = normalizeVector([
        pts[endpointIndex + 1].anchor[0] - pts[endpointIndex].anchor[0],
        pts[endpointIndex + 1].anchor[1] - pts[endpointIndex].anchor[1]
    ]);

    if (!segBefore && !segAfter) return 0;
    if (!segBefore) return endpointIndex;
    if (!segAfter) return endpointIndex - 1;

    if (!referenceDirection) {
        var beforeLen = dist(pts[endpointIndex].anchor, pts[endpointIndex - 1].anchor);
        var afterLen = dist(pts[endpointIndex + 1].anchor, pts[endpointIndex].anchor);
        return beforeLen >= afterLen ? (endpointIndex - 1) : endpointIndex;
    }

    var beforeAlignment = Math.abs(dot(segBefore, referenceDirection));
    var afterAlignment = Math.abs(dot(segAfter, referenceDirection));
    return beforeAlignment <= afterAlignment ? (endpointIndex - 1) : endpointIndex;
}

/**
 * Finds first anchor point in a container (recursive)
 * @param {Object} container - Illustrator container (Layer, GroupItem, etc.)
 * @returns {Array|null} Anchor point [x, y] or null
 */
function findFirstAnchorPointInContainer(container) {
    if (!container) return null;
    var typename = "";
    try { typename = container.typename; } catch (e) { typename = ""; }
    if (typename === "PathItem") {
        var path = container;
        if (path.pathPoints && path.pathPoints.length > 0) {
            var anchor = path.pathPoints[0].anchor;
            if (anchor && anchor.length === 2) return [anchor[0], anchor[1]];
        }
        return null;
    }
    if (typename === "CompoundPathItem") {
        try {
            for (var i = 0; i < container.pathItems.length; i++) {
                var result = findFirstAnchorPointInContainer(container.pathItems[i]);
                if (result) return result;
            }
        } catch (eCmp) { }
        return null;
    }
    try {
        if (container.pageItems) {
            for (var j = 0; j < container.pageItems.length; j++) {
                var resultPage = findFirstAnchorPointInContainer(container.pageItems[j]);
                if (resultPage) return resultPage;
            }
        }
    } catch (ePage) { }
    try {
        if (container.layers) {
            for (var k = 0; k < container.layers.length; k++) {
                var resultLayer = findFirstAnchorPointInContainer(container.layers[k]);
                if (resultLayer) return resultLayer;
            }
        }
    } catch (eLayer) { }
    return null;
}

// ========================================
// INTERSECTION & SEGMENT FUNCTIONS
// ========================================

/**
 * Computes intersection point of two line segments
 * @param {Array} p1 - First point of segment 1
 * @param {Array} p2 - Second point of segment 1
 * @param {Array} p3 - First point of segment 2
 * @param {Array} p4 - Second point of segment 2
 * @param {number} epsilon - Tolerance (default: 1e-6)
 * @returns {Object|null} Intersection info or null
 */
function computeSegmentIntersection(p1, p2, p3, p4, epsilon) {
    epsilon = epsilon || 1e-6;
    var x1 = p1[0], y1 = p1[1];
    var x2 = p2[0], y2 = p2[1];
    var x3 = p3[0], y3 = p3[1];
    var x4 = p4[0], y4 = p4[1];

    var denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (Math.abs(denom) < epsilon) return null;

    var t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    var u = ((x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2)) / denom;

    if (t < -epsilon || t > 1 + epsilon || u < -epsilon || u > 1 + epsilon) return null;

    var ix = x1 + t * (x2 - x1);
    var iy = y1 + t * (y2 - y1);
    return {
        point: { x: ix, y: iy },
        t1: t,
        t2: u
    };
}

/**
 * Checks if two line segments intersect
 * @returns {boolean} True if segments intersect
 */
function segmentsIntersect(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2) {
    var dax = ax2 - ax1, day = ay2 - ay1;
    var dbx = bx2 - bx1, dby = by2 - by1;
    var denom = dax * dby - day * dbx;
    if (Math.abs(denom) < 1e-10) {
        return linesOverlap(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
    }
    var dx = ax1 - bx1, dy = ay1 - by1;
    var t = (dbx * dy - dby * dx) / denom;
    var u = (dax * dy - day * dx) / denom;
    return (t >= 0 && t <= 1 && u >= 0 && u <= 1);
}

/**
 * Checks if two collinear lines overlap
 * @returns {boolean} True if lines overlap
 */
function linesOverlap(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2) {
    var cross1 = (ay1 - by1) * (bx2 - bx1) - (ax1 - bx1) * (by2 - by1);
    var cross2 = (ay2 - by1) * (bx2 - bx1) - (ax2 - bx1) * (by2 - by1);
    if (Math.abs(cross1) > 1e-6 || Math.abs(cross2) > 1e-6) return false;
    var primaryAxis = Math.abs(ax2 - ax1) > Math.abs(ay2 - ay1) ? 'x' : 'y';
    var a1, a2, b1, b2;
    if (primaryAxis === 'x') {
        a1 = Math.min(ax1, ax2); a2 = Math.max(ax1, ax2);
        b1 = Math.min(bx1, bx2); b2 = Math.max(bx1, by2);
    } else {
        a1 = Math.min(ay1, ay2); a2 = Math.max(ay1, ay2);
        b1 = Math.min(by1, by2); b2 = Math.max(by1, by2);
    }
    return !(a2 < b1 || b2 < a1);
}

// ========================================
// BOUNDS UTILITIES
// ========================================

/**
 * Computes bounding box from array of items (uses visibleBounds/geometricBounds)
 * @param {Array} items - Array of Illustrator items
 * @returns {Array|null} Bounds [left, top, right, bottom] or null
 */
function computeBoundsFromItems(items) {
    if (!items || items.length === undefined || items.length === 0) return null;
    var left = Infinity, top = -Infinity, right = -Infinity, bottom = Infinity;
    var found = false;
    for (var i = 0; i < items.length; i++) {
        var it = items[i];
        if (!it) continue;
        var gb = null;
        try { gb = it.visibleBounds || it.geometricBounds; } catch (e) { gb = null; }
        if (!gb || gb.length < 4) continue;
        if (!isFinite(gb[0]) || !isFinite(gb[1]) || !isFinite(gb[2]) || !isFinite(gb[3])) continue;
        if (gb[0] < left) left = gb[0];
        if (gb[1] > top) top = gb[1];
        if (gb[2] > right) right = gb[2];
        if (gb[3] < bottom) bottom = gb[3];
        found = true;
    }
    if (!found) return null;
    return [left, top, right, bottom];
}

/**
 * Computes combined bounds from array of items (returns object format)
 * @param {Array} items - Array of Illustrator items
 * @returns {Object|null} Bounds {left, right, top, bottom} or null
 */
function computeItemsBounds(items) {
    if (!items || !items.length) return null;
    var bounds = { left: Infinity, right: -Infinity, top: -Infinity, bottom: Infinity };
    var found = false;
    for (var i = 0; i < items.length; i++) {
        var item = items[i];
        if (!isItemValid(item)) continue;
        var gb = null;
        try { gb = item.geometricBounds; } catch (e) { gb = null; }
        if (!gb || gb.length < 4) continue;
        var left = gb[0], top = gb[1], right = gb[2], bottom = gb[3];
        if (!isFinite(left) || !isFinite(top) || !isFinite(right) || !isFinite(bottom)) continue;
        if (left < bounds.left) bounds.left = left;
        if (right > bounds.right) bounds.right = right;
        if (top > bounds.top) bounds.top = top;
        if (bottom < bounds.bottom) bounds.bottom = bottom;
        found = true;
    }
    return found ? bounds : null;
}

/**
 * Checks if a point is within bounds
 * @param {Object} bounds - Bounds object {left, right, top, bottom}
 * @param {number} x - X coordinate
 * @param {number} y - Y coordinate
 * @returns {boolean} True if point is within bounds
 */
function pointWithinBounds(bounds, x, y) {
    if (!bounds) return true;
    if (x < bounds.left || x > bounds.right) return false;
    if (y > bounds.top || y < bounds.bottom) return false;
    return true;
}

/**
 * Checks if two bounds intersect (array format)
 * @param {Array} itemBounds - Bounds [left, top, right, bottom]
 * @param {Object} selectionBounds - Bounds {left, right, top, bottom}
 * @returns {boolean} True if bounds intersect
 */
function boundsIntersect(itemBounds, selectionBounds) {
    if (!selectionBounds) return true;
    if (!itemBounds || itemBounds.length < 4) return false;
    var left = itemBounds[0];
    var top = itemBounds[1];
    var right = itemBounds[2];
    var bottom = itemBounds[3];
    if (!isFinite(left) || !isFinite(top) || !isFinite(right) || !isFinite(bottom)) return false;
    if (right < selectionBounds.left) return false;
    if (left > selectionBounds.right) return false;
    if (top < selectionBounds.bottom) return false;
    if (bottom > selectionBounds.top) return false;
    return true;
}

/**
 * Checks if two bounds overlap (both array format)
 * @param {Array} bounds1 - Bounds [left, top, right, bottom]
 * @param {Array} bounds2 - Bounds [left, top, right, bottom]
 * @returns {boolean} True if bounds overlap
 */
function boundsOverlap(bounds1, bounds2) {
    if (!bounds1 || !bounds2 || bounds1.length < 4 || bounds2.length < 4) return false;
    // bounds format: [left, top, right, bottom]
    return !(bounds1[2] < bounds2[0] || bounds1[0] > bounds2[2] ||
        bounds1[3] > bounds2[1] || bounds1[1] < bounds2[3]);
}

/**
 * Creates a bounding box from two points
 * @param {Array} a - Point [x, y]
 * @param {Array} b - Point [x, y]
 * @returns {Object} Bounding box {minX, minY, maxX, maxY}
 */
function makeBBox(a, b) {
    return {
        minX: Math.min(a[0], b[0]),
        minY: Math.min(a[1], b[1]),
        maxX: Math.max(a[0], b[0]),
        maxY: Math.max(a[1], b[1])
    };
}

/**
 * Expands a bounding box by padding
 * @param {Object} b - Bounding box {minX, minY, maxX, maxY}
 * @param {number} pad - Padding amount
 * @returns {Object} Expanded bounding box
 */
function expandBBox(b, pad) {
    return {
        minX: b.minX - pad,
        minY: b.minY - pad,
        maxX: b.maxX + pad,
        maxY: b.maxY + pad
    };
}

/**
 * Checks if bounding box contains a point
 * @param {Object} b - Bounding box {minX, minY, maxX, maxY}
 * @param {Array} p - Point [x, y]
 * @returns {boolean} True if bbox contains point
 */
function bboxContainsPoint(b, p) {
    return p[0] >= b.minX && p[0] <= b.maxX && p[1] >= b.minY && p[1] <= b.maxY;
}

// ========================================
// CUBIC BEZIER FUNCTIONS
// ========================================

/**
 * Evaluates a cubic Bezier curve at parameter t
 * @param {Array} p0 - Control point 0
 * @param {Array} p1 - Control point 1
 * @param {Array} p2 - Control point 2
 * @param {Array} p3 - Control point 3
 * @param {number} t - Parameter (0-1)
 * @returns {Array} Point on curve [x, y]
 */
function cubicAt(p0, p1, p2, p3, t) {
    var mt = 1 - t;
    var t2 = t * t;
    var a = mt * mt * mt;
    var b = 3 * mt * mt * t;
    var c = 3 * mt * t2;
    var d = t * t2;
    return [
        a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
        a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]
    ];
}

/**
 * Finds closest point on cubic Bezier curve to a given point
 * @param {Array} p0 - Control point 0
 * @param {Array} p1 - Control point 1
 * @param {Array} p2 - Control point 2
 * @param {Array} p3 - Control point 3
 * @param {Array} point - Target point [x, y]
 * @returns {Object} {t, pt, dist2} - Parameter, point, and squared distance
 */
function closestPointOnCubic(p0, p1, p2, p3, point) {
    var best = { t: 0, pt: p0, dist2: dist2(p0, point) };
    var samples = 12;
    for (var i = 1; i <= samples; i++) {
        var t = i / samples;
        var pt = cubicAt(p0, p1, p2, p3, t);
        var d2 = dist2(pt, point);
        if (d2 < best.dist2) best = { t: t, pt: pt, dist2: d2 };
    }
    var range = 1 / samples;
    for (var iter = 0; iter < 4; iter++) {
        var t0 = Math.max(0, best.t - range);
        var t1 = Math.min(1, best.t + range);
        var improved = false;
        var steps = 5;
        for (var j = 0; j <= steps; j++) {
            var t = t0 + (t1 - t0) * (j / steps);
            var pt = cubicAt(p0, p1, p2, p3, t);
            var d2 = dist2(pt, point);
            if (d2 < best.dist2) {
                best = { t: t, pt: pt, dist2: d2 };
                improved = true;
            }
        }
        if (!improved) break;
        range *= 0.5;
    }
    return best;
}

// ========================================
// ITEM VALIDATION & UTILITIES
// ========================================

/**
 * Checks if an Illustrator item is valid
 * @param {Object} item - Illustrator item
 * @returns {boolean} True if item is valid
 */
function isItemValid(item) {
    if (!item) return false;
    try {
        if (item.isValid !== undefined && !item.isValid) return false;
    } catch (e) { }
    try {
        var t = item.typename;
        return !!t;
    } catch (e2) {
        return false;
    }
}

/**
 * Unlocks an item and its parent chain
 * @param {Object} item - Illustrator item
 */
function unlockItemChain(item) {
    if (!item) return;
    try {
        if (item.locked) item.locked = false;
    } catch (e) { }
    try {
        var parent = item.parent;
        var guard = 0;
        while (parent && parent !== item && guard < 10) {
            try {
                if (parent.locked) parent.locked = false;
            } catch (e2) { }
            try {
                if (parent === parent.parent) break;
                parent = parent.parent;
            } catch (e2) {
                // Parent may be invalid after copy/paste
                break;
            }
            guard++;
        }
    } catch (e3) { }
}

/**
 * Checks if an item intersects selection bounds
 * @param {Object} item - Illustrator item
 * @param {Object} selectionBounds - Bounds {left, right, top, bottom}
 * @returns {boolean} True if item hits selection
 */
function itemHitsSelection(item, selectionBounds) {
    if (!isItemValid(item)) return false;
    if (!selectionBounds) return true;
    var gb = null;
    try { gb = item.geometricBounds; } catch (e) { gb = null; }
    if (gb && gb.length === 4 && isFinite(gb[0]) && isFinite(gb[1]) && isFinite(gb[2]) && isFinite(gb[3])) {
        return boundsIntersect(gb, selectionBounds);
    }
    var center = null;
    try {
        var left = item.left;
        var top = item.top;
        var width = item.width;
        var height = item.height;
        if (isFinite(left) && isFinite(top) && isFinite(width) && isFinite(height)) {
            center = [left + width / 2, top - height / 2];
        }
    } catch (e2) { }
    if (!center && item.typename === "PathItem" && item.pathPoints && item.pathPoints.length > 0) {
        try {
            var pt = item.pathPoints[0].anchor;
            center = [pt[0], pt[1]];
        } catch (e3) { }
    }
    if (!center) return false;
    return pointWithinBounds(selectionBounds, center[0], center[1]);
}

// ========================================
// PLACEHOLDER FUNCTIONS
// (These return null but are defined to maintain compatibility)
// ========================================

function computeAlignmentOffsetFromFile(file, alignLayerName) {
    return null;
}

function getComponentAlignmentOffsetForType(type, file) {
    return null;
}
/**
 * MagicFinal Layer Utilities
 *
 * Functions for finding, creating, and managing Illustrator layers.
 * Includes deep layer search and path collection utilities.
 *
 * This file can be included in other scripts:
 * //@include "lib/LayerUtils.jsxinc"
 *
 * Dependencies: Requires 'doc' to be defined globally
 */

/**
 * Finds a layer by name, searching recursively through nested layers
 * @param {Object} root - Root container (Document or Layer)
 * @param {string} targetName - Name of layer to find
 * @returns {Layer|null} Found layer or null
 */
function findLayerByNameDeepInContainer(root, targetName) {
    if (!root || !targetName) return null;
    var stack = [];

    function pushLayer(layer) {
        if (layer) stack.push(layer);
    }

    try {
        if (root.typename === "Document") {
            for (var i = 0; i < root.layers.length; i++) pushLayer(root.layers[i]);
        } else if (root.typename === "Layer") {
            pushLayer(root);
        }
    } catch (eInit) {
        return null;
    }

    while (stack.length > 0) {
        var layer = stack.pop();
        if (!layer) continue;

        var lname = "";
        try { lname = layer.name; } catch (eName) { lname = ""; }
        if (lname === targetName) return layer;

        try {
            if (layer.layers && layer.layers.length) {
                for (var j = 0; j < layer.layers.length; j++) {
                    pushLayer(layer.layers[j]);
                }
            }
        } catch (eChildren) { }
    }

    return null;
}

/**
 * Finds a layer by name in the active document
 * @param {string} targetName - Name of layer to find
 * @returns {Layer|null} Found layer or null
 * NOTE: Requires 'doc' to be defined globally
 */
function findLayerByNameDeep(targetName) {
    return findLayerByNameDeepInContainer(doc, targetName);
}

/**
 * Gets or creates a layer by name
 * @param {string} layerName - Name of layer
 * @returns {Layer} Existing or newly created layer
 * NOTE: Requires 'doc' to be defined globally
 */
function getOrCreateLayer(layerName) {
    if (!layerName) return null;
    var existing = findLayerByNameDeep(layerName);
    if (existing) return existing;
    try {
        var newLayer = doc.layers.add();
        newLayer.name = layerName;
        return newLayer;
    } catch (e) {
        return null;
    }
}

/**
 * Resolves the actual ductwork layer name for processing
 * In Emory mode, paths stay on normal layers, only styles are "Emory"
 * @param {string} normalLayerName - Normal layer name
 * @returns {string} Layer name to use
 */
function resolveDuctworkLayerForProcessing(normalLayerName) {
    // In Emory mode, paths stay on normal layers (e.g., "Green Ductwork")
    // Only the STYLES are "Emory" (e.g., "Green Ductwork Emory" style)
    // There are no separate "Emory" layers
    return normalLayerName;
}

/**
 * Gets all PathItems on a specific layer (SELECTED ONLY)
 * @param {string} layerName - Name of layer
 * @returns {Array} Array of PathItems
 * NOTE: Requires isPathSelected() and shouldProcessPath() functions
 * NOTE: Requires 'doc' to be defined globally
 */
function getPathsOnLayerSelected(layerName) {
    var results = [];
    if (!layerName) return results;

    var layer = findLayerByNameDeep(layerName);
    if (!layer) return results;

    function collectFromContainer(container) {
        if (!container) return;
        try {
            // Collect PathItems
            if (container.pathItems) {
                for (var i = 0; i < container.pathItems.length; i++) {
                    var path = container.pathItems[i];
                    if (!path) continue;
                    if (typeof isPathSelected !== 'undefined' && isPathSelected(path)) {
                        results.push(path);
                    } else if (typeof shouldProcessPath !== 'undefined' && shouldProcessPath(path)) {
                        results.push(path);
                    }
                }
            }

            // Collect paths from CompoundPathItems
            if (container.compoundPathItems) {
                for (var c = 0; c < container.compoundPathItems.length; c++) {
                    try {
                        var compound = container.compoundPathItems[c];
                        if (!compound) continue;
                        // Check if compound itself is selected
                        var compoundSelected = false;
                        if (typeof isPathSelected !== 'undefined') {
                            try { compoundSelected = compound.selected; } catch (e) { }
                        }
                        // Extract child paths from compound
                        for (var cp = 0; cp < compound.pathItems.length; cp++) {
                            var childPath = compound.pathItems[cp];
                            if (!childPath) continue;
                            if (compoundSelected) {
                                results.push(childPath);
                            } else if (typeof isPathSelected !== 'undefined' && isPathSelected(childPath)) {
                                results.push(childPath);
                            } else if (typeof shouldProcessPath !== 'undefined' && shouldProcessPath(childPath)) {
                                results.push(childPath);
                            }
                        }
                    } catch (e) { }
                }
            }

            // Recursively process groups
            if (container.groupItems) {
                for (var g = 0; g < container.groupItems.length; g++) {
                    collectFromContainer(container.groupItems[g]);
                }
            }
        } catch (e) { }
    }

    collectFromContainer(layer);
    return results;
}

/**
 * Gets ALL PathItems on a specific layer (not just selected)
 * @param {string} layerName - Name of layer
 * @returns {Array} Array of PathItems
 * NOTE: Requires 'doc' to be defined globally
 */
function getPathsOnLayerAll(layerName) {
    var results = [];
    if (!layerName) return results;

    var layer = findLayerByNameDeep(layerName);
    if (!layer) return results;

    function collectFromContainer(container) {
        if (!container) return;
        try {
            // Collect PathItems
            if (container.pathItems) {
                for (var i = 0; i < container.pathItems.length; i++) {
                    var path = container.pathItems[i];
                    if (path) results.push(path);
                }
            }

            // Collect paths from CompoundPathItems
            if (container.compoundPathItems) {
                for (var c = 0; c < container.compoundPathItems.length; c++) {
                    try {
                        var compound = container.compoundPathItems[c];
                        if (!compound) continue;
                        for (var cp = 0; cp < compound.pathItems.length; cp++) {
                            var childPath = compound.pathItems[cp];
                            if (childPath) results.push(childPath);
                        }
                    } catch (e) { }
                }
            }

            // Recursively process groups
            if (container.groupItems) {
                for (var g = 0; g < container.groupItems.length; g++) {
                    collectFromContainer(container.groupItems[g]);
                }
            }
        } catch (e) { }
    }

    collectFromContainer(layer);
    return results;
}

/**
 * Removes items on a layer that intersect with selection bounds
 * @param {Layer} layer - Illustrator layer
 * @param {Object} selectionBounds - Bounds {left, right, top, bottom}
 * NOTE: Requires isItemValid, itemHitsSelection, unlockItemChain functions
 */
function removeItemsOnLayer(layer, selectionBounds) {
    if (!isItemValid(layer) || !layer.pageItems) return;
    var items = layer.pageItems;
    var toRemove = [];
    for (var i = 0; i < items.length; i++) {
        var item = items[i];
        if (!isItemValid(item)) continue;
        if (itemHitsSelection(item, selectionBounds)) {
            toRemove.push(item);
        }
    }
    for (var r = 0; r < toRemove.length; r++) {
        var target = toRemove[r];
        if (!isItemValid(target)) continue;
        unlockItemChain(target);
        try { target.remove(); } catch (e) { }
    }
}

/**
 * Iterates through all paths in items recursively
 * @param {Array} items - Array of Illustrator items
 * @param {Function} callback - Function to call for each PathItem
 * NOTE: Requires isItemValid function
 */
function forEachPathInItems(items, callback) {
    if (!items || !callback) return;
    function visit(item) {
        if (!isItemValid(item)) return;
        var type = item.typename;
        if (type === "PathItem") {
            callback(item);
        } else if (type === "CompoundPathItem" && item.pathItems) {
            for (var i = 0; i < item.pathItems.length; i++) visit(item.pathItems[i]);
        } else if (type === "GroupItem" && item.pageItems) {
            for (var g = 0; g < item.pageItems.length; g++) visit(item.pageItems[g]);
        }
    }
    for (var i = 0; i < items.length; i++) visit(items[i]);
}
/**
 * MagicFinal Transform Utilities
 *
 * Functions for managing transforms (rotation, scale, matrix operations)
 * on Illustrator items.
 *
 * This file can be included in other scripts:
 * //@include "lib/TransformUtils.jsxinc"
 */

/**
 * Clones matrix values from an Illustrator matrix
 * @param {Matrix} matrix - Illustrator matrix object
 * @returns {Object|null} Cloned matrix values or null
 */
function cloneMatrixValues(matrix) {
    if (!matrix) return null;
    return {
        a: matrix.mValueA,
        b: matrix.mValueB,
        c: matrix.mValueC,
        d: matrix.mValueD,
        tx: matrix.mValueTX,
        ty: matrix.mValueTY
    };
}

/**
 * Applies matrix values to an Illustrator item
 * @param {Object} item - Illustrator item
 * @param {Object} values - Matrix values object
 */
function applyMatrixValues(item, values) {
    if (!item || !values) return;
    var m = new Matrix();
    m.mValueA = values.a;
    m.mValueB = values.b;
    m.mValueC = values.c;
    m.mValueD = values.d;
    m.mValueTX = values.tx;
    m.mValueTY = values.ty;
    item.matrix = m;
}

/**
 * Captures transform (rotation, scale) from a placed item
 * Stores in initialPlacedTransforms global cache
 * @param {PlacedItem} item - Illustrator placed item
 * NOTE: Requires initialPlacedTransforms global variable
 */
function captureItemTransform(item) {
    if (!item || item.typename !== "PlacedItem") return;
    try {
        var b = item.geometricBounds;
        var key = ((b[0] + b[2]) / 2).toFixed(2) + "_" + ((b[1] + b[3]) / 2).toFixed(2);

        // Get actual rotation from matrix
        var rot = null;
        try {
            var m = item.matrix;
            rot = Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
            // DON'T normalize - keep raw angle
        } catch (e) { }

        // Get actual scale from matrix
        var scale = null;
        try {
            var m2 = item.matrix;
            scale = Math.sqrt((m2.mValueA * m2.mValueA) + (m2.mValueB * m2.mValueB)) * 100;
        } catch (e) { }

        initialPlacedTransforms[key] = {
            rotation: rot,
            scale: scale,
            layer: item.layer ? item.layer.name : null
        };
    } catch (e) { }
}
/**
 * MagicFinal Metadata Utilities
 *
 * Functions for reading and writing metadata to Illustrator items using
 * the 'note' field. Manages rotation overrides, centerline IDs, layer names,
 * and other custom metadata.
 *
 * This file can be included in other scripts:
 * //@include "lib/MetadataUtils.jsxinc"
 *
 * Dependencies: Requires Constants.jsxinc to be loaded first (for tag prefixes)
 */

// ========================================
// HELPER FUNCTIONS
// ========================================

function trimString(str) {
    if (str === undefined || str === null) return '';
    return ('' + str).replace(/^\s+|\s+$/g, '');
}

function arrayIndexOf(list, value) {
    if (!list || typeof list.length !== "number") return -1;
    for (var i = 0; i < list.length; i++) {
        if (list[i] === value) return i;
    }
    return -1;
}

// ========================================
// NOTE TOKEN MANAGEMENT
// ========================================

function getNoteString(pathItem) {
    try {
        if (!pathItem) return '';
        var note = pathItem.note;
        return (typeof note === 'string') ? note : '';
    } catch (e) {
        return '';
    }
}

function readNoteTokens(pathItem) {
    var note = getNoteString(pathItem);
    if (!note) return [];
    var raw = note.split('|');
    var tokens = [];
    for (var i = 0; i < raw.length; i++) {
        var token = trimString(raw[i]);
        if (token.length > 0) tokens.push(token);
    }
    return tokens;
}

function writeNoteTokens(pathItem, tokens) {
    if (!pathItem) return;
    var result = [];
    if (tokens && tokens.length) {
        for (var i = 0; i < tokens.length; i++) {
            var token = trimString(tokens[i]);
            if (token.length > 0) result.push(token);
        }
    }
    try {
        pathItem.note = result.join('|');
    } catch (e) { }
}

function ensureNoteTag(pathItem, tag) {
    if (!pathItem || !tag) return;
    var tokens = readNoteTokens(pathItem);
    if (arrayIndexOf(tokens, tag) === -1) {
        tokens.push(tag);
        writeNoteTokens(pathItem, tokens);
    }
}

function removeNoteTag(pathItem, tag) {
    if (!pathItem || !tag) return;
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i] !== tag) filtered.push(tokens[i]);
    }
    writeNoteTokens(pathItem, filtered);
}

function hasNoteTag(pathItem, tag) {
    if (!pathItem || !tag) return false;
    var tokens = readNoteTokens(pathItem);
    return arrayIndexOf(tokens, tag) !== -1;
}

function storePreOrthoGeometry(pathItem) {
    if (!pathItem) return;
    var pts = null;
    try {
        pts = pathItem.pathPoints;
    } catch (e) {
        return;
    }
    if (!pts || pts.length === 0) return;
    var payload = {
        closed: !!pathItem.closed,
        points: []
    };
    for (var i = 0; i < pts.length; i++) {
        var point = pts[i];
        payload.points.push({
            anchor: [point.anchor[0], point.anchor[1]],
            left: [point.leftDirection[0], point.leftDirection[1]],
            right: [point.rightDirection[0], point.rightDirection[1]]
        });
    }
    var encoded = null;
    try {
        encoded = encodeURIComponent(JSON.stringify(payload));
    } catch (eJSON) {
        return;
    }
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var t = 0; t < tokens.length; t++) {
        if (!tokens[t]) continue;
        if (tokens[t].indexOf(PRE_ORTHO_PREFIX) === 0) continue;
        filtered.push(tokens[t]);
    }
    filtered.push(PRE_ORTHO_PREFIX + encoded);
    writeNoteTokens(pathItem, filtered);
}

function getPreOrthoGeometry(pathItem) {
    if (!pathItem) return null;
    var tokens = readNoteTokens(pathItem);
    for (var i = 0; i < tokens.length; i++) {
        var token = tokens[i];
        if (token && token.indexOf(PRE_ORTHO_PREFIX) === 0) {
            var dataString = token.substring(PRE_ORTHO_PREFIX.length);
            try {
                var decoded = decodeURIComponent(dataString);
                return JSON.parse(decoded);
            } catch (e) {
                return null;
            }
        }
    }
    return null;
}

function clearPreOrthoGeometry(pathItem) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (!tokens[i]) continue;
        if (tokens[i].indexOf(PRE_ORTHO_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(pathItem, filtered);
}

function applyPreOrthoGeometry(pathItem, geometry) {
    if (!pathItem || !geometry || !geometry.points || !geometry.points.length) {
        return false;
    }
    try {
        pathItem.closed = !!geometry.closed;
    } catch (eClosed) { }
    var anchorArray = [];
    for (var i = 0; i < geometry.points.length; i++) {
        var point = geometry.points[i];
        anchorArray.push([point.anchor[0], point.anchor[1]]);
    }
    try {
        pathItem.setEntirePath(anchorArray);
    } catch (eSet) {
        return false;
    }
    var pts = null;
    try {
        pts = pathItem.pathPoints;
    } catch (ePts) {
        return false;
    }
    if (!pts || pts.length !== geometry.points.length) {
        return false;
    }
    for (var j = 0; j < pts.length; j++) {
        try {
            var dataPoint = geometry.points[j];
            pts[j].anchor = [dataPoint.anchor[0], dataPoint.anchor[1]];
            pts[j].leftDirection = [dataPoint.left[0], dataPoint.left[1]];
            pts[j].rightDirection = [dataPoint.right[0], dataPoint.right[1]];
        } catch (eAssign) {
            // Continue applying remaining points
        }
    }
    return true;
}

function matrixToObject(matrix) {
    if (!matrix) return null;
    return {
        a: matrix.mValueA,
        b: matrix.mValueB,
        c: matrix.mValueC,
        d: matrix.mValueD,
        tx: matrix.mValueTX,
        ty: matrix.mValueTY
    };
}

function objectToMatrix(obj) {
    if (!obj) return null;
    var m = app.getIdentityMatrix ? app.getIdentityMatrix() : new Matrix();
    m.mValueA = obj.a;
    m.mValueB = obj.b;
    m.mValueC = obj.c;
    m.mValueD = obj.d;
    m.mValueTX = obj.tx;
    m.mValueTY = obj.ty;
    return m;
}

function writePreScaleData(item, data) {
    if (!item || !data) return;
    var encoded;
    try {
        encoded = encodeURIComponent(JSON.stringify(data));
    } catch (e) {
        return;
    }
    var tokens = readNoteTokens(item);
    var tag = PRE_SCALE_PREFIX + encoded;
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i] && tokens[i].indexOf(PRE_SCALE_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        tokens.push(tag);
    }

    try {
        var logFile = new File(getLogFilePath("write-metadata.log"));
        logFile.open("a");
        logFile.writeln("[WRITE] Writing metadata: lastPercent=" + data.lastPercent);
        logFile.writeln("[WRITE]   Item: " + item.typename);
        logFile.close();
    } catch (e) { }

    writeNoteTokens(item, tokens);
}

function storePreScaleData(item) {
    if (!item) return null;
    var existing = getPreScaleData(item);
    if (existing) return existing;

    var data = {
        basePercent: 100,
        lastPercent: 100,
        matrix: null,
        strokeWidth: null
    };
    try {
        data.matrix = matrixToObject(item.matrix);
    } catch (eMatrix) {
        data.matrix = null;
    }

    // Don't store strokeWidth for ductwork lines - they use appearance attributes
    var layerName = "";
    try { layerName = item.layer ? item.layer.name : ""; } catch (e) { layerName = ""; }
    var isLineLayer = layerName && isDuctworkLineLayer(layerName);

    if (item.typename === "PathItem" && !isLineLayer) {
        try {
            data.strokeWidth = item.strokeWidth;
        } catch (eSW) {
            data.strokeWidth = null;
        }
    }
    if (item.typename === "PlacedItem") {
        var currentMetaScale = getPlacedScale(item);
        if (typeof currentMetaScale === "number" && isFinite(currentMetaScale) && currentMetaScale > 0) {
            data.lastPercent = currentMetaScale;
        }
    }
    writePreScaleData(item, data);
    return data;
}

function getPreScaleData(item) {
    if (!item) return null;
    var tokens = readNoteTokens(item);

    try {
        var logFile = new File(getLogFilePath("read-metadata.log"));
        logFile.open("a");
        logFile.writeln("[READ] Reading metadata from: " + item.typename);
        logFile.writeln("[READ]   Found " + tokens.length + " tokens");
        logFile.close();
    } catch (e) { }

    for (var i = 0; i < tokens.length; i++) {
        var token = tokens[i];
        if (token && token.indexOf(PRE_SCALE_PREFIX) === 0) {
            var payload = token.substring(PRE_SCALE_PREFIX.length);
            try {
                var decoded = decodeURIComponent(payload);
                var data = JSON.parse(decoded);
                if (typeof data.basePercent !== "number" || !isFinite(data.basePercent) || data.basePercent <= 0) {
                    data.basePercent = 100;
                }
                if (typeof data.lastPercent !== "number" || !isFinite(data.lastPercent) || data.lastPercent <= 0) {
                    data.lastPercent = data.basePercent || 100;
                }
                if (typeof data.strokeWidth === "undefined") {
                    data.strokeWidth = null;
                }
                if (!data.hasOwnProperty("matrix")) {
                    data.matrix = null;
                }

                try {
                    var logFile = new File(getLogFilePath("read-metadata.log"));
                    logFile.open("a");
                    logFile.writeln("[READ]   Found scale data: lastPercent=" + data.lastPercent);
                    logFile.close();
                } catch (e) { }

                return data;
            } catch (eParse) {
                return null;
            }
        }
    }

    try {
        var logFile = new File(getLogFilePath("read-metadata.log"));
        logFile.open("a");
        logFile.writeln("[READ]   No scale data found");
        logFile.close();
    } catch (e) { }

    return null;
}

function clearPreScaleData(item) {
    if (!item) return;
    try {
        if (item.__MDUXScaleState) {
            item.__MDUXScaleState = { lastPercent: 100 };
        }
    } catch (eState) { }
    var tokens = readNoteTokens(item);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (!tokens[i]) continue;
        if (tokens[i].indexOf(PRE_SCALE_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(item, filtered);
}

function applyScaleToItem(item, percent) {
    if (!item || !isFinite(percent) || percent <= 0) return false;
    var layerName = "";
    try { layerName = item.layer ? item.layer.name : ""; } catch (eLayer) { layerName = ""; }
    var isLineLayer = layerName && isDuctworkLineLayer(layerName);
    var typeName = item.typename;

    var data = getPreScaleData(item);
    if (!data) {
        data = storePreScaleData(item);
        if (!data) return false;
    }

    if (isLineLayer && (typeName === "PathItem" || typeName === "CompoundPathItem")) {
        // For ductwork lines: DON'T use metadata at all!
        // The caller (panel.js) will handle resetting graphic styles before calling this
        // We just scale from 100% to the target percent
        try {
            // Scale directly to the target percent (assumes caller reset the graphic style first)
            item.resize(100, 100, false, false, false, false, percent, Transformation.CENTER);
            return true;
        } catch (eApply) {
            return false;
        }
    }

    var currentPercent = data.lastPercent;
    if (typeName === "PlacedItem") {
        var storedScale = getPlacedScale(item);
        if (typeof storedScale === "number" && isFinite(storedScale) && storedScale > 0) {
            currentPercent = storedScale;
        }
    }
    if (typeof currentPercent !== "number" || !isFinite(currentPercent) || currentPercent <= 0) {
        currentPercent = 100;
    }

    var ratio = percent / currentPercent;
    if (!isFinite(ratio) || ratio <= 0) {
        return false;
    }

    if (Math.abs(ratio - 1) < 0.0001) {
        data.lastPercent = percent;
        writePreScaleData(item, data);
        if (typeName === "PlacedItem") {
            if (Math.abs(percent - 100) < 0.0001) {
                clearPlacedScale(item);
            } else {
                setPlacedScale(item, percent);
            }
        }
        return true;
    }

    var resizePercent = ratio * 100;
    var scaled = false;
    try {
        if (item.resize) {
            item.resize(resizePercent, resizePercent, true, true, true, true, true, Transformation.CENTER);
            scaled = true;
        }
    } catch (eResize) {
        scaled = false;
    }
    if (!scaled) {
        try {
            if (item.resize) {
                item.resize(resizePercent, resizePercent);
                scaled = true;
            }
        } catch (eResizeFallback) {
            scaled = false;
        }
    }

    if (scaled) {
        data.lastPercent = percent;
        writePreScaleData(item, data);
        if (typeName === "PlacedItem") {
            if (Math.abs(percent - 100) < 0.0001) {
                clearPlacedScale(item);
            } else {
                setPlacedScale(item, percent);
            }
        }
    }

    return scaled;
}
// ========================================
// BRANCH WIDTH METADATA
// ========================================

function getBranchStartWidthFromPath(pathItem) {
    if (!pathItem) return null;
    var tokens = readNoteTokens(pathItem);
    for (var i = 0; i < tokens.length; i++) {
        var token = tokens[i];
        if (token && token.indexOf(BRANCH_START_PREFIX) === 0) {
            var raw = token.substring(BRANCH_START_PREFIX.length);
            var val = parseFloat(raw);
            if (isFinite(val) && val > 0) return val;
        }
    }
    return null;
}

function setBranchStartWidthOnPath(pathItem, fullWidth) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i] && tokens[i].indexOf(BRANCH_START_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    if (typeof fullWidth === "number" && isFinite(fullWidth) && fullWidth > 0) {
        filtered.push(BRANCH_START_PREFIX + fullWidth.toFixed(4));
    }
    writeNoteTokens(pathItem, filtered);
}

function clearBranchStartWidthOnPath(pathItem) {
    setBranchStartWidthOnPath(pathItem, null);
}

// ========================================
// EMORY CENTERLINE METADATA
// ========================================

function generateCenterlineId() {
    var stamp = (new Date().getTime()).toString(36);
    var rand = Math.floor(Math.random() * 1e6).toString(36);
    return "CL" + stamp + "_" + rand;
}

// ========================================
// BLUE RIGHT-ANGLE BRANCH MARKERS
// ========================================

function markBlueRightAngleBranch(pathItem) {
    ensureNoteTag(pathItem, BLUE_RIGHTANGLE_BRANCH_TAG);
}

function clearBlueRightAngleBranch(pathItem) {
    removeNoteTag(pathItem, BLUE_RIGHTANGLE_BRANCH_TAG);
    try { delete pathItem.__blueRightAngleBranchData; } catch (e) { }
}

function hasBlueRightAngleBranch(pathItem) {
    return hasNoteTag(pathItem, BLUE_RIGHTANGLE_BRANCH_TAG);
}

// ========================================
// ORIGINAL LAYER NAME
// ========================================

function getOriginalLayerName(pathItem) {
    if (!pathItem) return null;
    var tokens = readNoteTokens(pathItem);
    for (var i = 0; i < tokens.length; i++) {
        var token = tokens[i];
        if (token && token.indexOf(ORIGINAL_LAYER_PREFIX) === 0) {
            var value = token.substring(ORIGINAL_LAYER_PREFIX.length);
            if (value && value.length > 0) return value;
        }
    }
    return null;
}

function setOriginalLayerName(pathItem, layerName) {
    if (!pathItem || !layerName) return;
    var tokens = readNoteTokens(pathItem);
    var tag = ORIGINAL_LAYER_PREFIX + layerName;
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i] && tokens[i].indexOf(ORIGINAL_LAYER_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(pathItem, tokens);
}

function clearOriginalLayerName(pathItem) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i] && tokens[i].indexOf(ORIGINAL_LAYER_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(pathItem, filtered);
}

// ========================================
// ROTATION OVERRIDE (for paths)
// ========================================

function computePathPrimaryAngle(pathItem) {
    if (!pathItem) return 0;
    var pts = null;
    try { pts = pathItem.pathPoints; } catch (e) { return 0; }
    if (!pts || pts.length < 2) return 0;
    for (var i = 0; i < pts.length - 1; i++) {
        var curr = pts[i].anchor;
        var next = pts[i + 1].anchor;
        var dx = next[0] - curr[0];
        var dy = next[1] - curr[1];
        if (Math.abs(dx) > 1e-4 || Math.abs(dy) > 1e-4) {
            return normalizeAngle(Math.atan2(dy, dx) * (180 / Math.PI));
        }
    }
    if (pathItem.closed && pts.length > 1) {
        var last = pts[pts.length - 1].anchor;
        var first = pts[0].anchor;
        var dxClose = first[0] - last[0];
        var dyClose = first[1] - last[1];
        if (Math.abs(dxClose) > 1e-4 || Math.abs(dyClose) > 1e-4) {
            return normalizeAngle(Math.atan2(dyClose, dxClose) * (180 / Math.PI));
        }
    }
    return 0;
}

function getBaseRotation(pathItem) {
    if (!pathItem) return null;
    var tokens = readNoteTokens(pathItem);
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(ROT_BASE_PREFIX) === 0) {
            var raw = tokens[i].substring(ROT_BASE_PREFIX.length);
            var val = parseFloat(raw);
            if (!isNaN(val)) return normalizeAngle(val);
        }
    }
    return null;
}

function setBaseRotation(pathItem, angleDeg) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var tag = ROT_BASE_PREFIX + normalizeAngle(angleDeg);
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(ROT_BASE_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(pathItem, tokens);
}

function ensureBaseRotation(pathItem) {
    var base = getBaseRotation(pathItem);
    if (base === null) {
        base = computePathPrimaryAngle(pathItem);
        setBaseRotation(pathItem, base);
    }
    return base;
}

function setRotationOverride(pathItem, angleDeg) {
    if (!pathItem) return;
    // Store the angle directly without computing base rotation
    var tokens = readNoteTokens(pathItem);
    var tag = ROT_OVERRIDE_PREFIX + normalizeAngle(angleDeg);
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(ROT_OVERRIDE_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(pathItem, tokens);
}

function clearRotationOverride(pathItem) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        // Clear both MD:ROT= and MD:BASE_ROT= tags
        if (tokens[i].toUpperCase().indexOf(ROT_OVERRIDE_PREFIX) === 0) continue;
        if (tokens[i].toUpperCase().indexOf(ROT_BASE_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(pathItem, filtered);
}

// Utility function to clear all rotation metadata from selection
function clearAllRotationMetadata() {
    if (!app.activeDocument || !app.activeDocument.selection || app.activeDocument.selection.length === 0) {
        alert("Please select paths first");
        return;
    }

    var count = 0;
    for (var i = 0; i < app.activeDocument.selection.length; i++) {
        var item = app.activeDocument.selection[i];
        if (item.typename === "PathItem") {
            clearRotationOverride(item);
            count++;
        } else if (item.typename === "GroupItem") {
            var paths = getAllPathItemsInGroup(item);
            for (var j = 0; j < paths.length; j++) {
                clearRotationOverride(paths[j]);
                count++;
            }
        }
    }

    alert("Cleared rotation metadata from " + count + " paths");
}

function getRotationOverride(pathItem) {
    var tokens = readNoteTokens(pathItem);
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(ROT_OVERRIDE_PREFIX) === 0) {
            var raw = tokens[i].substring(ROT_OVERRIDE_PREFIX.length);
            var val = parseFloat(raw);
            if (!isNaN(val)) {
                // Return the angle directly - no base rotation system
                return normalizeAngle(val);
            }
        }
    }
    // Only return a rotation override if one was explicitly set (MD:ROT= tag exists)
    // Paths without explicit override will orthogonalize to standard 0/90/180/270 grid
    return null;
}

// ========================================
// POINT ROTATION (for anchor points)
// ========================================

function setPointRotation(pathItem, angleDeg) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var tag = POINT_ROT_PREFIX + normalizeAngle(angleDeg);
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(POINT_ROT_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(pathItem, tokens);
}

function clearPointRotation(pathItem) {
    if (!pathItem) return;
    var tokens = readNoteTokens(pathItem);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(POINT_ROT_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(pathItem, filtered);
}

function getPointRotation(pathItem) {
    if (!pathItem) return null;
    var tokens = readNoteTokens(pathItem);
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(POINT_ROT_PREFIX) === 0) {
            var raw = tokens[i].substring(POINT_ROT_PREFIX.length);
            var val = parseFloat(raw);
            if (!isNaN(val)) return normalizeAngle(val);
        }
    }
    return null;
}

// ========================================
// PLACED ITEM METADATA
// ========================================

function normalizeSignedAngleValue(angle) {
    if (typeof angle !== "number" || !isFinite(angle)) return 0;
    while (angle > 180) angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
}

function computePlacedPrimaryAngle(item) {
    if (!item) return 0;
    try {
        var m = item.matrix;
        return Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
    } catch (e) {
        return 0;
    }
}

function getPlacedBaseRotation(item) {
    if (!item) return null;
    var tokens = readNoteTokens(item);
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_BASE_ROT_PREFIX) === 0) {
            var raw = tokens[i].substring(PLACED_BASE_ROT_PREFIX.length);
            var val = parseFloat(raw);
            if (!isNaN(val)) return val;
        }
    }
    return null;
}

function setPlacedBaseRotation(item, angleDeg) {
    if (!item) return;
    var tokens = readNoteTokens(item);
    var tag = PLACED_BASE_ROT_PREFIX + angleDeg.toFixed(6);
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_BASE_ROT_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(item, tokens);
}

function clearPlacedBaseRotation(item) {
    if (!item) return;
    var tokens = readNoteTokens(item);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_BASE_ROT_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(item, filtered);
}

function ensurePlacedBaseRotation(item) {
    var base = getPlacedBaseRotation(item);
    if (base === null || !isFinite(base)) {
        base = computePlacedPrimaryAngle(item);
        setPlacedBaseRotation(item, base);
    }
    return base;
}

function getPlacedRotationDelta(item) {
    if (!item) return null;
    var tokens = readNoteTokens(item);
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_ROT_PREFIX) === 0) {
            var raw = tokens[i].substring(PLACED_ROT_PREFIX.length);
            var val = parseFloat(raw);
            if (!isNaN(val)) return normalizeSignedAngleValue(val);
        }
    }
    return null;
}

function getPlacedRotation(item) {
    var delta = getPlacedRotationDelta(item);
    var base = getPlacedBaseRotation(item);
    if (delta === null) {
        if (base === null || !isFinite(base)) return null;
        return normalizeAngle(base);
    }
    if (base === null || !isFinite(base)) base = 0;
    return normalizeAngle(base + delta);
}

function setPlacedRotationDelta(item, deltaDeg) {
    if (!item) return;
    var tokens = readNoteTokens(item);
    var tag = PLACED_ROT_PREFIX + normalizeSignedAngleValue(deltaDeg).toFixed(6);
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_ROT_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(item, tokens);
}

function setPlacedRotation(item, angleDeg) {
    if (!item) return;
    var base = ensurePlacedBaseRotation(item);
    if (base === null || !isFinite(base)) base = 0;
    var delta = normalizeSignedAngleValue(angleDeg - base);
    setPlacedRotationDelta(item, delta);
}

function clearPlacedRotation(item) {
    if (!item) return;
    var tokens = readNoteTokens(item);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_ROT_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(item, filtered);
}

function getPlacedScale(item) {
    if (!item) return null;
    var tokens = readNoteTokens(item);
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_SCALE_PREFIX) === 0) {
            var raw = tokens[i].substring(PLACED_SCALE_PREFIX.length);
            var val = parseFloat(raw);
            if (!isNaN(val) && isFinite(val) && val > 0) return val;
        }
    }
    return null;
}

function setPlacedScale(item, scalePercent) {
    if (!item) return;
    var tokens = readNoteTokens(item);
    var tag = PLACED_SCALE_PREFIX + scalePercent.toFixed(4);
    var replaced = false;
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_SCALE_PREFIX) === 0) {
            tokens[i] = tag;
            replaced = true;
            break;
        }
    }
    if (!replaced) tokens.push(tag);
    writeNoteTokens(item, tokens);
}

function MDUX_getMetadata(item) {
    try {
        var note = item.note || "";
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META GET] Item typename=" + item.typename + ", note=" + (note ? note.substring(0, 100) : "(empty)"));
        }
        if (!note || note.indexOf("MDUX_META:") !== 0) {
            if (typeof MDUX_debugLog === 'function') {
                MDUX_debugLog("[META GET] No MDUX_META prefix, returning null");
            }
            return null;
        }
        var jsonStr = note.substring(10); // Remove "MDUX_META:" prefix
        // Extract only the JSON part (up to first | token separator, if present)
        var pipeIdx = jsonStr.indexOf("|");
        if (pipeIdx !== -1) {
            jsonStr = jsonStr.substring(0, pipeIdx);
        }
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META GET] JSON string (clean): " + jsonStr.substring(0, 200));
        }
        var result = JSON.parse(jsonStr);
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META GET] Parsed: " + JSON.stringify(result).substring(0, 100));
        }
        return result;
    } catch (e) {
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META GET] ERROR: " + e);
        }
        return null;
    }
}

function MDUX_setMetadata(item, metadata) {
    try {
        var jsonStr = JSON.stringify(metadata);
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META SET] Item typename=" + item.typename + ", metadata=" + jsonStr.substring(0, 200));
        }

        // Preserve existing pipe-separated tokens (MD:PLACED_ROT=, MD:PLACED_BASE_ROT=, etc.)
        var existingNote = "";
        try { existingNote = item.note || ""; } catch (e) { existingNote = ""; }
        var pipeSuffix = "";
        if (existingNote.indexOf("MDUX_META:") === 0) {
            var pipeIdx = existingNote.indexOf("|");
            if (pipeIdx !== -1) {
                pipeSuffix = existingNote.substring(pipeIdx); // Keep everything from first | onwards
                if (typeof MDUX_debugLog === 'function') {
                    MDUX_debugLog("[META SET] Preserving pipe tokens: " + pipeSuffix.substring(0, 100));
                }
            }
        }

        item.note = "MDUX_META:" + jsonStr + pipeSuffix;
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META SET] Written. Item note is now: " + (item.note || "(empty)").substring(0, 200));
        }
    } catch (e) {
        if (typeof MDUX_debugLog === 'function') {
            MDUX_debugLog("[META SET] ERROR: " + e);
        }
    }
}

function clearPlacedScale(item) {
    if (!item) return;
    var tokens = readNoteTokens(item);
    var filtered = [];
    for (var i = 0; i < tokens.length; i++) {
        if (tokens[i].toUpperCase().indexOf(PLACED_SCALE_PREFIX) === 0) continue;
        filtered.push(tokens[i]);
    }
    writeNoteTokens(item, filtered);
}

// ========================================
// GROUP SETTINGS MANAGEMENT
// ========================================

function saveSettingsToGroup(group, settings) {
    if (!group || !settings) return;
    try {
        var parts = [
            "width=" + (settings.width || 12), // NOTE: Changed from 28 to 12
            "taper=" + (settings.taper || 30),
            "angle=" + (settings.angle || 0),
            "noTaper=" + (settings.noTaper ? 1 : 0),
            "color=" + (settings.color || ""),
            "mode=" + (settings.mode || "emory")
        ];
        if (settings.centerlineIds && settings.centerlineIds.length) {
            parts.push("centerlineIds=" + settings.centerlineIds.join(","));
        }
        if (settings.branch) {
            parts.push("branch=1");
        }
        if (typeof settings.branchStart === "number" && isFinite(settings.branchStart) && settings.branchStart > 0) {
            parts.push("branchStart=" + settings.branchStart);
        }
        group.note = "DuctworkSettings:" + parts.join(";");
    } catch (e) { }
}

function loadSettingsFromGroup(group) {
    if (!group) return null;
    try {
        if (group.note && group.note.indexOf("DuctworkSettings:") === 0) {
            var payload = group.note.substring("DuctworkSettings:".length);
            var fields = payload.split(";");
            var result = {
                width: 12, // NOTE: Changed from 28 to 12
                taper: 30,
                angle: 0,
                noTaper: false,
                color: "",
                mode: "emory",
                centerlineIds: [],
                branch: false,
                branchStart: null
            };
            for (var i = 0; i < fields.length; i++) {
                var pair = fields[i].split("=");
                if (pair.length !== 2) continue;
                var key = pair[0];
                var val = pair[1];
                if (key === "width") result.width = parseFloat(val) || 12; // NOTE: Changed from 28 to 12
                else if (key === "taper") result.taper = parseFloat(val) || 30;
                else if (key === "angle") result.angle = parseFloat(val) || 0;
                else if (key === "noTaper") result.noTaper = (val === "1");
                else if (key === "color") result.color = val;
                else if (key === "mode") result.mode = val;
                else if (key === "centerlineIds") {
                    if (val && val.length) result.centerlineIds = val.split(",");
                } else if (key === "branch") {
                    result.branch = (val === "1");
                } else if (key === "branchStart") {
                    var parsed = parseFloat(val);
                    if (isFinite(parsed) && parsed > 0) result.branchStart = parsed;
                }
            }
            return result;
        }
    } catch (e) { }
    return null;
}

function loadSettingsFromSelection(selectionItems) {
    if (!selectionItems || selectionItems.length === 0) {
        return {
            width: 12, // NOTE: Changed from 28 to 12 - easily reversible
            taper: 30,
            angle: 0,
            noTaper: false,
            color: "",
            mode: "emory"
        };
    }

    for (var i = 0; i < selectionItems.length; i++) {
        var item = selectionItems[i];
        if (!item) continue;
        var grp = null;
        try {
            if (item.typename === "GroupItem") {
                grp = item;
            } else {
                var parent = item.parent;
                var safetyCounter = 0;
                while (parent && safetyCounter < 50) {
                    try {
                        if (parent.typename === "GroupItem") {
                            grp = parent;
                            break;
                        }
                        var newParent = parent.parent;
                        if (!newParent || newParent === parent) break;
                        parent = newParent;
                    } catch (e) {
                        // Parent may be invalid after copy/paste
                        break;
                    }
                    safetyCounter++;
                }
            }
            if (grp) {
                var loaded = loadSettingsFromGroup(grp);
                if (loaded) return loaded;
            }
        } catch (e) {
            // Item may be invalid, skip it
            continue;
        }
    }

    return {
        width: 12, // NOTE: Changed from 28 to 12 - easily reversible
        taper: 30,
        angle: 0,
        noTaper: false,
        color: "",
        mode: "emory"
    };
}

/**
 * MagicFinal Dialog Utilities
 *
 * Helper functions for creating and styling ScriptUI dialogs.
 *
 * This file can be included in other scripts:
 * //@include "lib/DialogUtils.jsxinc"
 */

/**
 * Creates a dark-themed dialog window
 * @param {string} title - Dialog title
 * @returns {Window|null} Dialog window or null
 */
function createDarkDialogWindow(title) {
    if (typeof Window === "undefined") return null;
    var dlg = new Window("dialog", title || "Dialog");
    dlg.orientation = "column";
    dlg.alignChildren = "fill";
    dlg.spacing = 12;
    dlg.margins = 18;
    try {
        var g = dlg.graphics;
        g.backgroundColor = g.newBrush(g.BrushType.SOLID_COLOR, [0.12, 0.12, 0.12]);
    } catch (e) { }
    return dlg;
}

/**
 * Sets foreground color for static text controls
 * @param {Object} control - ScriptUI control
 * @param {Array} rgbArray - RGB values [r, g, b] in 0-1 range
 */
function setStaticTextColor(control, rgbArray) {
    if (!control || !control.graphics) return;
    try {
        var g = control.graphics;
        var color = rgbArray || [0.85, 0.85, 0.85];
        g.foregroundColor = g.newPen(g.PenType.SOLID_COLOR, color, 1);
    } catch (e) { }
}
/**
 * MagicFinal Main Script
 * Optimized Illustrator ExtendScript v5.4 (Production Build - Refactored)
 * 
 * This is the main orchestrator script that includes all modular components.
 *
 * === REFACTORED STRUCTURE ===
 * - Configuration files (config/)
 * - Utility libraries (lib/)
 * - Main workflow (this file)
 *
 * === ORIGINAL DOCUMENTATION ===
 * Refactored register creation logic to use a single, comprehensive check list of existing points,
 * correctly preventing Orange Ductwork from creating points that overlap with Rectangular Registers.
 * Removed the final "Processing complete!" alert for a silent execution.
 * Suppressed "already part of a compound path" alert by managing userInteractionLevel.
 * Added a pre-processing step to remove existing art and anchors that conflict with the 'Ignored' layer.
 * Corrected the file extension for the 'Place Ductwork at Points' script call.
 * Fixed "isPointIgnored is not a function" error by adding the missing function definition.
 * Fixed inconsistent stroke thickness issue by normalizing stroke properties before compounding
 * and clearing appearance attributes before applying graphic styles.
 * Added support for "Ignore"/"Ignored"/"ignore"/"ignored" layers to skip anchor creation at specified points.
 * Added duplicate anchor detection to prevent overlapping anchor points on target layers.
 * Added special handling for Square Registers to avoid duplicating Rectangular Registers.
 * Added cross-layer duplicate prevention between Units and Register layers (Square/Rectangular).
 *
 * ⚠️⚠️⚠️ CRITICAL FREEZE/CRASH WARNINGS ⚠️⚠️⚠️
 * 1. DO NOT replace componentGroups.indexOf() with manual loop - CAUSES COMPLETE FREEZE
 * 2. DO NOT add extra debug statements inside segment processing loops - CAUSES FREEZE
 * 3. DO NOT add loops inside trunk-to-trunk width inheritance logic - CAUSES FREEZE
 * 4. Any loop that iterates through pathMetaList during rectangle generation will likely FREEZE
 *
 * === TERMINOLOGY ===
 * DOUBLE DUCTWORK: In Emory mode, refers to the filled rectangles, curve connector pieces, and S connector pieces
 *                  that are generated BEHIND the centerlines to create a double-line ductwork appearance.
 *                  These are the visual "filled" shapes that show the duct width, distinct from the centerline strokes.
 *                  Components: Rectangles (straight sections), Curve Connectors (for bends), S Connectors (for offsets)
 *
 * CENTERLINES: The single-line stroked paths that represent the ductwork path. In normal mode, these are styled
 *              with simple strokes. In Emory mode, these are the same centerlines but with Double Ductwork behind them.
 *              Centerlines can be toggled on/off in Emory mode while keeping the Double Ductwork visible.
 *
 * BLUE DUCTWORK ASSEMBLIES: Connected blue ductwork paths (including both trunk lines and their T-intersection
 *                            branches) that are compounded together into a single CompoundPathItem representing
 *                            the complete interconnected system. Each assembly is one network of connected blue paths.
 */

// ========================================
// MAIN SCRIPT BODY
// ========================================
// NOTE: All modules (config/*.jsxinc and lib/*.jsxinc) are included
//       by the entry point script (01 - Magic Final.jsx) before this
//       file is loaded. Do not add @include statements here.
// ========================================

(function () {
    try {
        // --- CONFIG ---
        var CLOSE_DIST = 10; // px for loose connection grouping
        var UNIT_MERGE_DIST = 6; // px tolerance to merge clustered unit anchors
        var THERMOSTAT_JUNCTION_DIST = 6; // px tolerance to snap thermostat line endpoints to duct junctions
        var ORTHO_LOCK_TAG = "MD:ORTHO_LOCK"; // marker note to skip re-orthogonalizing processed paths
        var ROT_OVERRIDE_PREFIX = "MD:ROT="; // marker prefix for rotation override notes
        var POINT_ROT_PREFIX = "MD:POINT_ROT="; // marker prefix for anchor rotation notes
        var PLACED_ROT_PREFIX = "MD:PLACED_ROT="; // marker prefix for placed item rotation notes
        var PLACED_SCALE_PREFIX = "MD:PLACED_SCALE="; // marker prefix for placed item custom scale
        var PLACED_BASE_ROT_PREFIX = "MD:PLACED_BASE_ROT="; // marker prefix for placed item base rotation from linked file
        var CENTERLINE_NOTE_TAG = "MD:CENTERLINE"; // marker to identify processed centerlines
        var CENTERLINE_ID_PREFIX = "MD:CLID="; // marker prefix for per-centerline unique id
        var PRE_ORTHO_PREFIX = "MD:PREORTHO="; // stores pre-orthogonalization geometry
        var PRE_SCALE_PREFIX = "MD:PRESCALE="; // stores pre-scale metadata
        var BLUE_RIGHTANGLE_BRANCH_TAG = "MD:BLUE90_BRANCH"; // marks blue 90° branch paths that need special handling
        var ORIGINAL_LAYER_PREFIX = "MD:ORIG_LAYER="; // marker prefix for remembering original layer
        var CONNECTION_DIST = 2; // px stricter threshold for actual compounding
        var RECONNECT_CAPTURE_DIST = Math.max(CONNECTION_DIST, SNAP_THRESHOLD); // px tolerance to remember original junctions
        var STEEP_MIN = 17; // deg for not orthogonalizing
        var STEEP_MAX = 70; // deg for not orthogonalizing
        var SNAP_THRESHOLD = 5; // px for snapping anchors
        var MAX_ITER = 8; // maximum refinement iterations
        var IGNORED_DIST = 4; // px stricter threshold for Ignored-layer proximity (keeps CLOSE_DIST behavior unchanged)
        var BLUE_BRANCH_CONNECTIONS = []; // caches detected blue right-angle branch connections
        var BLUE_TRUNK_TAPER_ENABLED = true; // allow blue trunks to taper (set false to restore legacy behavior)
        var BRANCH_START_PREFIX = "MD:BRANCH_START="; // marker prefix for custom branch start width
        var BRANCH_CUSTOM_START_WIDTHS = {}; // per-centerline custom start widths (full width in pts)
        var LIMIT_BRANCH_PROCESS_MAP = null; // optional map of centerline ids to limit processing scope
        var SKIP_ALL_BRANCH_ORTHO = false; // controls whether all branch segments stay freeform
        var SKIP_FINAL_REGISTER_ORTHO = false; // controls whether only final register segments stay freeform
        var SKIP_REGISTER_ROTATION = false; // controls whether square registers are rotated to match ductwork angle
        var ORTHO_IGNORED_ANCHORS = []; // ignored anchors collected early for ortho phase (FR-002)
        var ORTHO_REGISTER_ANCHORS = []; // register positions collected early for ortho phase (skip-final check)
        var ORTHO_IGNORE_MARKER_PATHS = []; // paths with ignore markers {path, endpoint, ignoreMarkerIndex}

        // MDUX_getMetadata and MDUX_setMetadata must be defined early so they're available to all functions in this IIFE
        // Note: Use MDUX_debugLog from panel-bridge.jsx which is always available, NOT addDebug which may have uninitialized dependencies
        function MDUX_getMetadata_inner(item) {
            // Use MDUX_debugLog if available (defined in panel-bridge.jsx), otherwise silent
            var dbg = (typeof MDUX_debugLog === 'function') ? MDUX_debugLog : function () { };
            try {
                if (!item) {
                    return null;
                }
                var note = "";
                try {
                    note = item.note || "";
                } catch (eNote) {
                    return null;
                }
                if (!note || note.indexOf("MDUX_META:") !== 0) {
                    return null;
                }
                var jsonStr = note.substring(10); // Remove "MDUX_META:" prefix
                // Extract only the JSON part (up to first | token separator, if present)
                var pipeIdx = jsonStr.indexOf("|");
                if (pipeIdx !== -1) {
                    jsonStr = jsonStr.substring(0, pipeIdx);
                }
                var result = JSON.parse(jsonStr);
                return result;
            } catch (e) {
                return null;
            }
        }

        function MDUX_setMetadata_inner(item, metadata) {
            // Use MDUX_debugLog if available (defined in panel-bridge.jsx), otherwise silent
            var dbg = (typeof MDUX_debugLog === 'function') ? MDUX_debugLog : function () { };
            try {
                if (!item) {
                    return;
                }
                var jsonStr = JSON.stringify(metadata);

                // Preserve existing pipe-separated tokens (MD:PLACED_ROT=, MD:PLACED_BASE_ROT=, etc.)
                var existingNote = "";
                try { existingNote = item.note || ""; } catch (e) { existingNote = ""; }
                var pipeSuffix = "";
                if (existingNote.indexOf("MDUX_META:") === 0) {
                    var pipeIdx = existingNote.indexOf("|");
                    if (pipeIdx !== -1) {
                        pipeSuffix = existingNote.substring(pipeIdx); // Keep everything from first | onwards
                    }
                }

                item.note = "MDUX_META:" + jsonStr + pipeSuffix;
            } catch (e) {
                // Silent fail
            }
        }

        function resetBranchCustomStartWidths() {
            BRANCH_CUSTOM_START_WIDTHS = {};
        }

        function setBranchCustomStartWidth(centerlineId, fullWidth) {
            if (!centerlineId) return;
            if (typeof fullWidth !== "number" || !isFinite(fullWidth) || fullWidth <= 0) {
                clearBranchCustomStartWidth(centerlineId);
                return;
            }
            BRANCH_CUSTOM_START_WIDTHS[centerlineId] = fullWidth;
        }

        function getBranchCustomStartWidth(centerlineId) {
            if (!centerlineId) return null;
            var fullWidth = BRANCH_CUSTOM_START_WIDTHS.hasOwnProperty(centerlineId) ? BRANCH_CUSTOM_START_WIDTHS[centerlineId] : null;
            if (typeof fullWidth !== "number" || !isFinite(fullWidth) || fullWidth <= 0) {
                return null;
            }
            return fullWidth;
        }

        function clearBranchCustomStartWidth(centerlineId) {
            if (!centerlineId) return;
            if (BRANCH_CUSTOM_START_WIDTHS.hasOwnProperty(centerlineId)) {
                delete BRANCH_CUSTOM_START_WIDTHS[centerlineId];
            }
        }

        // Lightweight runtime diagnostics (incremental, non-fatal)
        var DIAG = {
            skippedRegisterNearThermostat: 0,
            skippedRegisterNearUnit: 0,
            createdRegisters: 0,
            thermostatsCreated: 0,
            thermostatsSkipped: 0,
            registerSkipReasons: [],
            thermostatSkipReasons: []
        };

        // --- LAYER GROUP CONFIG ---
        // Ductwork piece layers (used for placing pieces and anchor checks)
        var DUCTWORK_PIECES = [
            "Thermostats",
            "Units",
            "Secondary Exhaust Registers",
            "Exhaust Registers",
            "Orange Register",
            "Rectangular Registers",
            "Square Registers",
            "Circular Registers"
        ];

        // Ductwork line layers (line-only layers that should not be orthogonalized, etc.)
        var DUCTWORK_LINES = [
            "Green Ductwork",
            "Light Green Ductwork",
            "Blue Ductwork",
            "Orange Ductwork",
            "Light Orange Ductwork",
            "Thermostat Lines"
        ];

        var EMORY_LINE_LAYERS = [
            "Green Ductwork Emory",
            "Light Green Ductwork Emory",
            "Blue Ductwork Emory",
            "Orange Ductwork Emory",
            "Light Orange Ductwork Emory",
            "Thermostat Lines"
        ];

        var EMORY_STYLE_MAP = {
            "Green Ductwork Emory": "Green Ductwork Emory",
            "Light Green Ductwork Emory": "Light Green Ductwork Emory",
            "Blue Ductwork Emory": "Blue Ductwork Emory",
            "Orange Ductwork Emory": "Orange Ductwork Emory",
            "Light Orange Ductwork Emory": "Light Orange Ductwork Emory",
            "Thermostat Lines": "Thermostat Line Emory"
        };

        var NORMAL_STYLE_MAP = {
            "Green Ductwork": "Green Ductwork",
            "Light Green Ductwork": "Light Green Ductwork",
            "Blue Ductwork": "Blue Ductwork",
            "Orange Ductwork": "Orange Ductwork",
            "Light Orange Ductwork": "Light Orange Ductwork",
            "Thermostat Lines": "Thermostat Lines"
        };

        var DUCTWORK_COLOR_OPTIONS = [
            "Green Ductwork",
            "Light Green Ductwork",
            "Blue Ductwork",
            "Orange Ductwork",
            "Light Orange Ductwork"
        ];

        var LAYER_TO_COLOR_NAME = {
            "Green Ductwork": "Green Ductwork",
            "Light Green Ductwork": "Light Green Ductwork",
            "Blue Ductwork": "Blue Ductwork",
            "Orange Ductwork": "Orange Ductwork",
            "Light Orange Ductwork": "Light Orange Ductwork",
            "Green Ductwork Emory": "Green Ductwork",
            "Light Green Ductwork Emory": "Light Green Ductwork",
            "Blue Ductwork Emory": "Blue Ductwork",
            "Orange Ductwork Emory": "Orange Ductwork",
            "Light Orange Ductwork Emory": "Light Orange Ductwork"
        };

        function getColorNameForLayer(layerName) {
            if (!layerName) return null;
            if (LAYER_TO_COLOR_NAME.hasOwnProperty(layerName)) return LAYER_TO_COLOR_NAME[layerName];
            return null;
        }

        function getEmoryLayerNameFromColor(colorName) {
            if (!colorName) return "Green Ductwork Emory";
            switch (colorName) {
                case "Green Ductwork": return "Green Ductwork Emory";
                case "Light Green Ductwork": return "Light Green Ductwork Emory";
                case "Blue Ductwork": return "Blue Ductwork Emory";
                case "Orange Ductwork": return "Orange Ductwork Emory";
                case "Light Orange Ductwork": return "Light Orange Ductwork Emory";
                default: return "Green Ductwork Emory";
            }
        }

        function getNormalLayerNameFromColor(colorName) {
            if (!colorName) return "Green Ductwork";
            switch (colorName) {
                case "Green Ductwork":
                case "Light Green Ductwork":
                case "Blue Ductwork":
                case "Orange Ductwork":
                case "Light Orange Ductwork":
                    return colorName;
                default:
                    return "Green Ductwork";
            }
        }

        function isBlueDuctworkLayerName(name) {
            if (!name) return false;
            var lower = ("" + name).toLowerCase();
            return lower === "blue ductwork" || lower === "blue ductwork emory";
        }

        function normalizeStyleKey(name) {
            if (!name) return "";
            return ("" + name).toLowerCase().replace(/[^a-z0-9]/g, "");
        }

        function getGraphicStyleByNameFlexible(name) {
            if (!name) return null;
            try {
                var direct = doc.graphicStyles.getByName(name);
                if (direct) return direct;
            } catch (e) { }
            var targetKey = normalizeStyleKey(name);
            if (!targetKey) return null;
            try {
                var styles = doc.graphicStyles;
                for (var i = 0; i < styles.length; i++) {
                    var candidate = styles[i];
                    if (!candidate) continue;
                    var candName = candidate.name || "";
                    if (normalizeStyleKey(candName) === targetKey) {
                        return candidate;
                    }
                }
            } catch (e2) { }
            return null;
        }

        function isDuctworkLineLayer(name) {
            if (!name) return false;
            if (isEmoryLineLayer(name)) return true;
            var lower = ("" + name).toLowerCase();
            for (var i = 0; i < DUCTWORK_LINES.length; i++) {
                var entry = DUCTWORK_LINES[i];
                if (typeof entry === "string" && lower === entry.toLowerCase()) {
                    return true;
                }
            }
            return false;
        }

        function isEmoryLineLayer(name) {
            if (!name) return false;
            var lower = ("" + name).toLowerCase();
            for (var i = 0; i < EMORY_LINE_LAYERS.length; i++) {
                var entry = EMORY_LINE_LAYERS[i];
                if (typeof entry === "string" && lower === entry.toLowerCase()) {
                    return true;
                }
            }
            return false;
        }

        function getGraphicStyleNameForLayer(layerName, isEmory) {
            if (!layerName) return null;
            if (isEmory) {
                if (EMORY_STYLE_MAP.hasOwnProperty(layerName)) return EMORY_STYLE_MAP[layerName];
            } else {
                if (NORMAL_STYLE_MAP.hasOwnProperty(layerName)) return NORMAL_STYLE_MAP[layerName];
            }
            return null;
        }

        function findLayerByNameDeepInContainer(root, targetName) {
            if (!root || !targetName) return null;
            var stack = [];

            function pushLayer(layer) {
                if (layer) stack.push(layer);
            }

            try {
                if (root.typename === "Document") {
                    for (var i = 0; i < root.layers.length; i++) pushLayer(root.layers[i]);
                } else if (root.typename === "Layer") {
                    pushLayer(root);
                }
            } catch (eInit) {
                return null;
            }

            while (stack.length > 0) {
                var layer = stack.pop();
                if (!layer) continue;

                var lname = "";
                try { lname = layer.name; } catch (eName) { lname = ""; }
                if (lname === targetName) return layer;

                try {
                    if (layer.layers && layer.layers.length) {
                        for (var j = 0; j < layer.layers.length; j++) {
                            pushLayer(layer.layers[j]);
                        }
                    }
                } catch (eChildren) { }
            }

            return null;
        }

        function findLayerByNameDeep(targetName) {
            return findLayerByNameDeepInContainer(doc, targetName);
        }

        if (app.documents.length === 0) return;
        var doc = app.activeDocument;
        var sel = doc.selection;

        var forcedBootstrap = null;
        try {
            if ($.global.MDUX && $.global.MDUX.forcedOptions) {
                forcedBootstrap = $.global.MDUX.forcedOptions;
            }
        } catch (eForcedBootstrap) {
            forcedBootstrap = null;
        }
        if (forcedBootstrap && forcedBootstrap.action === "library") {
            try { registerMDUXExports(); } catch (eRegister) { }
            try { delete $.global.MDUX.forcedOptions; } catch (eDeleteForced) { }
            return;
        }

        if (!sel || sel.length === 0) {
            alert("Select paths to process.");
            return;
        }

        // Store original selection items for later style application
        var originalSelectionItems = [];
        for (var i = 0; i < sel.length; i++) {
            originalSelectionItems.push(sel[i]);
        }

        // CRITICAL: Capture all placed item transforms BEFORE any processing
        // This preserves scales/rotations even on first run when no metadata exists
        var initialPlacedTransforms = {};
        var CREATED_EMORY_SHAPES = [];

        // Global cache for trunk endpoint widths to enable trunk-to-trunk inheritance without loops
        // Key format: "x,y" (rounded coordinates), Value: halfWidth
        var TRUNK_ENDPOINT_WIDTHS = {};

        // Debug log file
        var debugLog = [];
        function logDebug(msg) {
            debugLog.push(msg);
        }
        function writeDebugLog() {
            // Disabled to prevent lockups - debug info shown in dialog instead
            // If you need file logging, uncomment and update the path:
            /*
            try {
                var logFile = new File("E:/OneDrive/Desktop/magic_ductwork_debug.txt");
                if (logFile.open("w")) {
                    logFile.write(debugLog.join("\n\n========================================\n\n"));
                    logFile.close();
                }
            } catch (e) {
                // Silently fail - don't interrupt workflow
            }
            */
        }

        function saveSettingsToGroup(group, settings) {
            if (!group || !settings) return;
            try {
                var parts = [
                    "width=" + (settings.width || 12), // NOTE: Changed from 28 to 12
                    "taper=" + (settings.taper || 30),
                    "angle=" + (settings.angle || 0),
                    "noTaper=" + (settings.noTaper ? 1 : 0),
                    "color=" + (settings.color || ""),
                    "mode=" + (settings.mode || "emory")
                ];
                if (settings.centerlineIds && settings.centerlineIds.length) {
                    parts.push("centerlineIds=" + settings.centerlineIds.join(","));
                }
                if (settings.branch) {
                    parts.push("branch=1");
                }
                if (typeof settings.branchStart === "number" && isFinite(settings.branchStart) && settings.branchStart > 0) {
                    parts.push("branchStart=" + settings.branchStart);
                }
                group.note = "DuctworkSettings:" + parts.join(";");
            } catch (e) { }
        }

        function loadSettingsFromGroup(group) {
            if (!group) return null;
            try {
                if (group.note && group.note.indexOf("DuctworkSettings:") === 0) {
                    var payload = group.note.substring("DuctworkSettings:".length);
                    var fields = payload.split(";");
                    var result = {
                        width: 12, // NOTE: Changed from 28 to 12
                        taper: 30,
                        angle: 0,
                        noTaper: false,
                        color: "",
                        mode: "emory",
                        centerlineIds: [],
                        branch: false,
                        branchStart: null
                    };
                    for (var i = 0; i < fields.length; i++) {
                        var pair = fields[i].split("=");
                        if (pair.length !== 2) continue;
                        var key = pair[0];
                        var val = pair[1];
                        if (key === "width") result.width = parseFloat(val) || 12; // NOTE: Changed from 28 to 12
                        else if (key === "taper") result.taper = parseFloat(val) || 30;
                        else if (key === "angle") result.angle = parseFloat(val) || 0;
                        else if (key === "noTaper") result.noTaper = (val === "1");
                        else if (key === "color") result.color = val;
                        else if (key === "mode") result.mode = val;
                        else if (key === "centerlineIds") {
                            if (val && val.length) result.centerlineIds = val.split(",");
                        } else if (key === "branch") {
                            result.branch = (val === "1");
                        } else if (key === "branchStart") {
                            var parsed = parseFloat(val);
                            if (isFinite(parsed) && parsed > 0) result.branchStart = parsed;
                        }
                    }
                    return result;
                }
            } catch (e) { }
            return null;
        }

        function findEmoryGroupForCenterline(rootLayer, centerlineId) {
            if (!rootLayer || !centerlineId) return null;
            function search(container) {
                if (!container || !container.groupItems) return null;
                for (var gi = 0; gi < container.groupItems.length; gi++) {
                    var grp = container.groupItems[gi];
                    var settings = loadSettingsFromGroup(grp);
                    if (settings && settings.centerlineIds && settings.centerlineIds.length) {
                        for (var si = 0; si < settings.centerlineIds.length; si++) {
                            if (settings.centerlineIds[si] === centerlineId) {
                                return grp;
                            }
                        }
                    }
                    var nested = search(grp);
                    if (nested) return nested;
                }
                return null;
            }
            return search(rootLayer);
        }

        function findEmoryGroupForCenterlineGlobal(centerlineId) {
            if (!doc || !centerlineId) return null;
            try {
                for (var li = 0; li < doc.layers.length; li++) {
                    var layer = doc.layers[li];
                    var grp = findEmoryGroupForCenterline(layer, centerlineId);
                    if (grp) return grp;
                }
            } catch (e) { }
            return null;
        }

        function findEmoryCenterlinesByIds(idList) {
            var results = [];
            if (!doc || !idList || !idList.length) return results;
            var remaining = {};
            for (var i = 0; i < idList.length; i++) {
                if (idList[i]) remaining[idList[i]] = true;
            }
            try {
                for (var pi = 0; pi < doc.pathItems.length; pi++) {
                    var candidate = doc.pathItems[pi];
                    if (!candidate) continue;
                    var cid = getEmoryCenterlineId(candidate);
                    if (cid && remaining[cid]) {
                        results.push(candidate);
                        delete remaining[cid];
                        var still = false;
                        for (var key in remaining) {
                            still = true;
                            break;
                        }
                        if (!still) break;
                    }
                }
            } catch (e) { }
            return results;
        }

        function collectEmoryCenterlinesForProcessing() {
            var results = [];

            addDebug("\ncollectEmoryCenterlinesForProcessing: Collecting centerlines (PathItems and CompoundPathItems) from ductwork layers...");

            // Collect centerlines from ductwork layers
            // After compounding, centerlines are CompoundPathItems containing the connected paths
            // Paths stay on NORMAL layers (Green/Blue/Orange Ductwork), not separate Emory layers
            var ductworkLayerNames = [
                "Green Ductwork",
                "Light Green Ductwork",
                "Blue Ductwork",
                "Orange Ductwork",
                "Light Orange Ductwork"
            ];

            var allCandidates = [];

            for (var layerIdx = 0; layerIdx < ductworkLayerNames.length; layerIdx++) {
                var layerName = ductworkLayerNames[layerIdx];
                var layer = null;
                try {
                    layer = findLayerByNameDeep(layerName);
                } catch (e) {
                    continue;
                }

                if (!layer) continue;

                var layerItemCount = 0;

                // Collect PathItems and CompoundPathItems from layer (including those in groups)
                function collectFromContainer(container) {
                    try {
                        // Collect direct paths (individual open paths not yet compounded)
                        for (var pi = 0; pi < container.pathItems.length; pi++) {
                            var path = container.pathItems[pi];
                            if (!path) continue;
                            // Skip closed paths (these are Double Ductwork rectangles/connectors in Emory mode)
                            try {
                                if (path.closed) continue;
                            } catch (e) {
                                continue;
                            }
                            allCandidates.push(path);
                            layerItemCount++;
                        }

                        // Collect compound paths (connected centerlines after compounding)
                        for (var ci = 0; ci < container.compoundPathItems.length; ci++) {
                            var compound = container.compoundPathItems[ci];
                            if (!compound) continue;
                            allCandidates.push(compound);
                            layerItemCount++;
                        }

                        // Recursively collect from groups
                        for (var gi = 0; gi < container.groupItems.length; gi++) {
                            collectFromContainer(container.groupItems[gi]);
                        }
                    } catch (e) { }
                }

                collectFromContainer(layer);

                addDebug("  Found " + layerItemCount + " centerline items on '" + layerName + "' (PathItems + CompoundPathItems)");
            }

            addDebug("Total candidate centerlines collected: " + allCandidates.length);
            addDebug("\nFiltering candidates for Emory Double Ductwork processing (ONLY SELECTED):");

            for (var i = 0; i < allCandidates.length; i++) {
                var item = allCandidates[i];
                if (!item) {
                    addDebug("  Candidate " + i + ": NULL/UNDEFINED - rejected");
                    continue;
                }

                // CRITICAL: Only process items that were marked for Emory style (i.e., selected)
                if (!isItemMarkedForEmoryStyle(item)) {
                    continue; // Skip silently if not selected
                }

                var itemType = "";
                try { itemType = item.typename; } catch (e) { itemType = "unknown"; }

                var isValidFlag = false;
                try { isValidFlag = item.isValid; } catch (e) { }

                var layerInfo = "unknown";
                try { layerInfo = item.layer ? item.layer.name : "no-layer"; } catch (e) { layerInfo = "error-getting-layer"; }

                // For PathItems, check if closed
                if (itemType === "PathItem") {
                    try {
                        if (item.closed) {
                            addDebug("  Candidate " + i + ": PathItem CLOSED (isValid=" + isValidFlag + ") - rejected (Double Ductwork, not centerline)");
                            continue;
                        }
                    } catch (eClosed) {
                        addDebug("  Candidate " + i + ": PathItem error checking closed (isValid=" + isValidFlag + ") - rejected");
                        continue;
                    }

                    var pointCount = 0;
                    try { pointCount = item.pathPoints ? item.pathPoints.length : 0; } catch (e) { }
                    addDebug("  Candidate " + i + ": PathItem ACCEPTED (isValid=" + isValidFlag + ") - Layer=" + layerInfo + ", Points=" + pointCount);
                    results.push(item);
                }
                // For CompoundPathItems, accept them as centerlines
                else if (itemType === "CompoundPathItem") {
                    var subPathCount = 0;
                    try { subPathCount = item.pathItems ? item.pathItems.length : 0; } catch (e) { }
                    addDebug("  Candidate " + i + ": CompoundPathItem ACCEPTED (isValid=" + isValidFlag + ") - Layer=" + layerInfo + ", SubPaths=" + subPathCount);
                    results.push(item);
                }
                else {
                    addDebug("  Candidate " + i + ": UNSUPPORTED TYPE (" + itemType + ") - rejected");
                }
            }

            addDebug("collectEmoryCenterlinesForProcessing: Returning " + results.length + " eligible centerlines\n");
            return results;
        }

        function resolveDuctworkLayerForProcessing(normalLayerName) {
            // In Emory mode, paths stay on normal layers (e.g., "Green Ductwork")
            // Only the STYLES are "Emory" (e.g., "Green Ductwork Emory" style)
            // There are no separate "Emory" layers
            return normalLayerName;
        }

        function computeBoundsFromItems(items) {
            if (!items || items.length === undefined || items.length === 0) return null;
            var left = Infinity, top = -Infinity, right = -Infinity, bottom = Infinity;
            var found = false;
            for (var i = 0; i < items.length; i++) {
                var it = items[i];
                if (!it) continue;
                var gb = null;
                try { gb = it.visibleBounds || it.geometricBounds; } catch (e) { gb = null; }
                if (!gb || gb.length < 4) continue;
                if (!isFinite(gb[0]) || !isFinite(gb[1]) || !isFinite(gb[2]) || !isFinite(gb[3])) continue;
                if (gb[0] < left) left = gb[0];
                if (gb[1] > top) top = gb[1];
                if (gb[2] > right) right = gb[2];
                if (gb[3] < bottom) bottom = gb[3];
                found = true;
            }
            if (!found) return null;
            return [left, top, right, bottom];
        }

        function findFirstAnchorPointInContainer(container) {
            if (!container) return null;
            var typename = "";
            try { typename = container.typename; } catch (e) { typename = ""; }
            if (typename === "PathItem") {
                var path = container;
                if (path.pathPoints && path.pathPoints.length > 0) {
                    var anchor = path.pathPoints[0].anchor;
                    if (anchor && anchor.length === 2) return [anchor[0], anchor[1]];
                }
                return null;
            }
            if (typename === "CompoundPathItem") {
                try {
                    for (var i = 0; i < container.pathItems.length; i++) {
                        var result = findFirstAnchorPointInContainer(container.pathItems[i]);
                        if (result) return result;
                    }
                } catch (eCmp) { }
                return null;
            }
            try {
                if (container.pageItems) {
                    for (var j = 0; j < container.pageItems.length; j++) {
                        var resultPage = findFirstAnchorPointInContainer(container.pageItems[j]);
                        if (resultPage) return resultPage;
                    }
                }
            } catch (ePage) { }
            try {
                if (container.layers) {
                    for (var k = 0; k < container.layers.length; k++) {
                        var resultLayer = findFirstAnchorPointInContainer(container.layers[k]);
                        if (resultLayer) return resultLayer;
                    }
                }
            } catch (eLayer) { }
            return null;
        }

        function computeAlignmentOffsetFromFile(file, alignLayerName) {
            return null;
        }

        function getComponentAlignmentOffsetForType(type, file) {
            return null;
        }

        function getSwatchColorByName(name) {
            if (!name) return null;
            try {
                var swatch = doc.swatches.getByName(name);
                if (swatch && swatch.color) {
                    return duplicateColor(swatch.color);
                }
            } catch (e) { }
            return null;
        }

        function createRGBColor(r, g, b) {
            var rgb = new RGBColor();
            rgb.red = r;
            rgb.green = g;
            rgb.blue = b;
            return rgb;
        }

        var CENTERLINE_COLOR_MAP = {
            "Blue Ductwork": [0, 0, 255],           // hex: 0000ff
            "Blue Ductwork Emory": [0, 0, 255],     // hex: 0000ff
            "Green Ductwork": [0, 127, 0],          // hex: 007f00
            "Green Ductwork Emory": [0, 127, 0],    // hex: 007f00
            "Orange Ductwork": [255, 64, 31],       // hex: ff401f
            "Orange Ductwork Emory": [255, 64, 31], // hex: ff401f
            "Light Orange Ductwork": [255, 166, 72],      // hex: ffa648
            "Light Orange Ductwork Emory": [255, 166, 72], // hex: ffa648
            "Light Green Ductwork": [0, 206, 0],          // hex: 00ce00
            "Light Green Ductwork Emory": [0, 206, 0],    // hex: 00ce00
            "Thermostat Lines": [255, 30, 38],
            "Thermostat Line Emory": [255, 30, 38]
        };

        // Emory Double Ductwork Stroke Colors (RGB values from hex)
        var EMORY_STROKE_COLOR_MAP = {
            "Blue Ductwork": [0, 0, 255],        // 0000ff
            "Green Ductwork": [0, 255, 0],       // 00ff00
            "Light Orange Ductwork": [255, 166, 72],  // ffa648
            "Orange Ductwork": [255, 64, 31],    // ff401f
            "Light Green Ductwork": [16, 167, 0] // 10a700
        };

        // Emory Double Ductwork Fill Colors (RGB values from hex)
        var EMORY_FILL_COLOR_MAP = {
            "Blue Ductwork": [126, 224, 255],    // 7ee0ff
            "Green Ductwork": [16, 167, 0],      // 10a700
            "Light Orange Ductwork": [255, 194, 120],  // ffc278
            "Orange Ductwork": [251, 182, 57],   // fbb639
            "Light Green Ductwork": [0, 255, 0]  // 00ff00
        };

        function getCenterlineStrokeColor(layerName) {
            if (!layerName) return null;
            if (!CENTERLINE_COLOR_MAP.hasOwnProperty(layerName)) return null;
            var arr = CENTERLINE_COLOR_MAP[layerName];
            return createRGBColor(arr[0], arr[1], arr[2]);
        }

        function applyCenterlineStrokeColor(pathItem) {
            if (!pathItem) return;
            var layerName = "";
            try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (eLayerName) { layerName = ""; }

            var color = getCenterlineStrokeColor(layerName);
            if (!color) return;

            try {
                // STEP 0: Capture existing stroke width BEFORE clearing appearance
                // Use multiple methods to detect the actual stroke width
                var preservedStrokeWidth = 2; // Default base width
                var foundStroke = false;
                try {
                    // Method 1: For CompoundPathItems, check child pathItems strokeWidth
                    if (pathItem.typename === "CompoundPathItem" && pathItem.pathItems && pathItem.pathItems.length > 0) {
                        for (var cpi = 0; cpi < pathItem.pathItems.length; cpi++) {
                            var childPath = pathItem.pathItems[cpi];
                            if (childPath.stroked && childPath.strokeWidth > 0) {
                                preservedStrokeWidth = childPath.strokeWidth;
                                foundStroke = true;
                                addDebug("[STROKE-PRESERVE] Got strokeWidth from child path: " + preservedStrokeWidth);
                                break;
                            }
                        }
                    }

                    // Method 2: Calculate from visibleBounds vs geometricBounds difference
                    if (!foundStroke) {
                        try {
                            var vb = pathItem.visibleBounds;   // includes stroke
                            var gb = pathItem.geometricBounds; // excludes stroke
                            var vWidth = vb[2] - vb[0];
                            var gWidth = gb[2] - gb[0];
                            var strokeDiff = (vWidth - gWidth) / 2; // stroke extends on both sides
                            if (strokeDiff > 0.1) { // meaningful stroke detected
                                preservedStrokeWidth = strokeDiff;
                                foundStroke = true;
                                addDebug("[STROKE-PRESERVE] Got strokeWidth from bounds diff: " + preservedStrokeWidth);
                            }
                        } catch (eBounds) { }
                    }

                    // Method 3: Direct strokeWidth property
                    if (!foundStroke && pathItem.stroked && pathItem.strokeWidth > 0) {
                        preservedStrokeWidth = pathItem.strokeWidth;
                        foundStroke = true;
                        addDebug("[STROKE-PRESERVE] Got strokeWidth from property: " + preservedStrokeWidth);
                    }

                    if (!foundStroke) {
                        addDebug("[STROKE-PRESERVE] No stroke detected, using default 2pt");
                    }
                } catch (eGetWidth) {
                    addDebug("[STROKE-PRESERVE] Error: " + eGetWidth);
                }

                // STEP 1: Unapply all graphic styles first
                try {
                    pathItem.unapplyAll();
                } catch (e) { }

                // STEP 2: Apply the "Basic RGB" or "[None]" graphic style to completely reset appearance
                // This removes all complex appearance attributes like multiple strokes/fills
                try {
                    var basicStyle = null;
                    // Try to find a basic style to reset appearance
                    try {
                        basicStyle = doc.graphicStyles.getByName("[None]");
                    } catch (e) {
                        try {
                            basicStyle = doc.graphicStyles.getByName("Basic RGB");
                        } catch (e2) {
                            // If neither exists, try the first style (usually a basic one)
                            try {
                                if (doc.graphicStyles.length > 0) {
                                    basicStyle = doc.graphicStyles[0];
                                }
                            } catch (e3) { }
                        }
                    }
                    if (basicStyle) {
                        basicStyle.applyTo(pathItem);
                    }
                } catch (e) { }

                // STEP 3: Explicitly clear all appearance properties
                try {
                    pathItem.filled = false;
                } catch (e) { }

                try {
                    pathItem.opacity = 100;
                } catch (e) { }

                try {
                    pathItem.blendingMode = BlendModes.NORMAL;
                } catch (e) { }

                // STEP 4: Apply the new stroke color
                var colorCopy = new RGBColor();
                colorCopy.red = color.red;
                colorCopy.green = color.green;
                colorCopy.blue = color.blue;

                pathItem.stroked = true;
                pathItem.strokeColor = colorCopy;
                pathItem.strokeWidth = preservedStrokeWidth;
                try {
                    pathItem.strokeDashes = [];
                } catch (e) { }
                try {
                    pathItem.strokeCap = StrokeCap.BUTTENDCAP;
                } catch (e) { }

                // STEP 5: If this is a CompoundPathItem, also apply to child paths
                if (pathItem.typename === "CompoundPathItem" && pathItem.pathItems) {
                    for (var cpi = 0; cpi < pathItem.pathItems.length; cpi++) {
                        try {
                            var childPath = pathItem.pathItems[cpi];

                            // Clear child path appearance
                            try { childPath.filled = false; } catch (e) { }
                            try { childPath.opacity = 100; } catch (e) { }
                            try { childPath.blendingMode = BlendModes.NORMAL; } catch (e) { }

                            var childColorCopy = new RGBColor();
                            childColorCopy.red = color.red;
                            childColorCopy.green = color.green;
                            childColorCopy.blue = color.blue;
                            childPath.stroked = true;
                            childPath.strokeColor = childColorCopy;
                            childPath.strokeWidth = preservedStrokeWidth;
                            try { childPath.strokeDashes = []; } catch (e) { }
                            try { childPath.strokeCap = StrokeCap.BUTTENDCAP; } catch (e) { }
                        } catch (eChild) { }
                    }
                }
            } catch (eSetStroke) { }
        }

        function applyStrokeToLayerPaths(layerName, color) {
            if (!layerName || !color) return;

            var targetLayer = null;
            var wasLocked = false;
            var wasVisible = true;

            try {
                targetLayer = findLayerByNameDeep(layerName);
                if (!targetLayer) return;

                // Temporarily unlock and make visible
                wasLocked = targetLayer.locked;
                wasVisible = targetLayer.visible;
                targetLayer.locked = false;
                targetLayer.visible = true;
            } catch (e) {
                return;
            }

            // First, apply to compound paths directly
            try {
                function applyToCompounds(container) {
                    try {
                        if (container.compoundPathItems) {
                            for (var c = 0; c < container.compoundPathItems.length; c++) {
                                var compound = container.compoundPathItems[c];
                                if (!compound) continue;
                                try {
                                    // Capture existing stroke width using multiple methods
                                    var compoundStrokeWidth = 2;
                                    var foundCompStroke = false;
                                    try {
                                        // Method 1: Check child pathItems strokeWidth
                                        if (compound.pathItems && compound.pathItems.length > 0) {
                                            for (var ccpi = 0; ccpi < compound.pathItems.length; ccpi++) {
                                                var ccPath = compound.pathItems[ccpi];
                                                if (ccPath.stroked && ccPath.strokeWidth > 0) {
                                                    compoundStrokeWidth = ccPath.strokeWidth;
                                                    foundCompStroke = true;
                                                    break;
                                                }
                                            }
                                        }
                                        // Method 2: Calculate from bounds difference
                                        if (!foundCompStroke) {
                                            var cvb = compound.visibleBounds;
                                            var cgb = compound.geometricBounds;
                                            var cvWidth = cvb[2] - cvb[0];
                                            var cgWidth = cgb[2] - cgb[0];
                                            var cStrokeDiff = (cvWidth - cgWidth) / 2;
                                            if (cStrokeDiff > 0.1) {
                                                compoundStrokeWidth = cStrokeDiff;
                                                foundCompStroke = true;
                                            }
                                        }
                                        // Method 3: Direct strokeWidth
                                        if (!foundCompStroke && compound.stroked && compound.strokeWidth > 0) {
                                            compoundStrokeWidth = compound.strokeWidth;
                                        }
                                    } catch (eGetW) { }
                                    // Unapply graphic styles first to clear appearance attributes
                                    try { compound.unapplyAll(); } catch (e) { }
                                    // Duplicate color inline
                                    var compoundColorCopy = new RGBColor();
                                    compoundColorCopy.red = color.red;
                                    compoundColorCopy.green = color.green;
                                    compoundColorCopy.blue = color.blue;
                                    compound.stroked = true;
                                    compound.strokeWidth = compoundStrokeWidth;
                                    compound.strokeColor = compoundColorCopy;
                                } catch (e) { }
                            }
                        }
                        if (container.groupItems) {
                            for (var g = 0; g < container.groupItems.length; g++) {
                                applyToCompounds(container.groupItems[g]);
                            }
                        }
                    } catch (e) { }
                }
                applyToCompounds(targetLayer);
            } catch (e) { }

            // Then apply to individual paths (ONLY SELECTED ONES)
            var paths = getPathsOnLayerSelected(layerName);
            for (var i = 0; i < paths.length; i++) {
                var path = paths[i];
                if (!path) continue;
                try { if (path.closed) continue; } catch (eClosedFlag) { }
                try {
                    // Capture existing stroke width using multiple methods
                    var pathStrokeWidth = 2;
                    var foundPathStroke = false;
                    try {
                        // Method 1: Calculate from bounds difference
                        var pvb = path.visibleBounds;
                        var pgb = path.geometricBounds;
                        var pvWidth = pvb[2] - pvb[0];
                        var pgWidth = pgb[2] - pgb[0];
                        var pStrokeDiff = (pvWidth - pgWidth) / 2;
                        if (pStrokeDiff > 0.1) {
                            pathStrokeWidth = pStrokeDiff;
                            foundPathStroke = true;
                        }
                    } catch (ePBounds) { }
                    // Method 2: Direct strokeWidth
                    if (!foundPathStroke && path.stroked && path.strokeWidth > 0) {
                        pathStrokeWidth = path.strokeWidth;
                    }
                    // Unapply graphic styles first to clear appearance attributes
                    try { path.unapplyAll(); } catch (e) { }
                    // Duplicate color inline
                    var pathColorCopy = new RGBColor();
                    pathColorCopy.red = color.red;
                    pathColorCopy.green = color.green;
                    pathColorCopy.blue = color.blue;
                    path.stroked = true;
                    path.strokeWidth = pathStrokeWidth;
                    path.strokeColor = pathColorCopy;
                } catch (eStroke) { }
            }

            // Restore layer state
            try {
                if (targetLayer) {
                    targetLayer.locked = wasLocked;
                    targetLayer.visible = wasVisible;
                }
            } catch (e) { }
        }

        function applyManualCenterlineStrokeStyles() {
            // Removed: Only applied in Emory mode - not used in normal mode
            return;
        }

        function computeItemsBounds(items) {
            if (!items || !items.length) return null;
            var bounds = { left: Infinity, right: -Infinity, top: -Infinity, bottom: Infinity };
            var found = false;
            for (var i = 0; i < items.length; i++) {
                var item = items[i];
                if (!isItemValid(item)) continue;
                var gb = null;
                try { gb = item.geometricBounds; } catch (e) { gb = null; }
                if (!gb || gb.length < 4) continue;
                var left = gb[0], top = gb[1], right = gb[2], bottom = gb[3];
                if (!isFinite(left) || !isFinite(top) || !isFinite(right) || !isFinite(bottom)) continue;
                if (left < bounds.left) bounds.left = left;
                if (right > bounds.right) bounds.right = right;
                if (top > bounds.top) bounds.top = top;
                if (bottom < bounds.bottom) bounds.bottom = bottom;
                found = true;
            }
            return found ? bounds : null;
        }

        function isItemValid(item) {
            if (!item) return false;
            try {
                if (item.isValid !== undefined && !item.isValid) return false;
            } catch (e) { }
            try {
                var t = item.typename;
                return !!t;
            } catch (e2) {
                return false;
            }
        }

        function pointWithinBounds(bounds, x, y) {
            if (!bounds) return true;
            if (x < bounds.left || x > bounds.right) return false;
            if (y > bounds.top || y < bounds.bottom) return false;
            return true;
        }

        function boundsIntersect(itemBounds, selectionBounds) {
            if (!selectionBounds) return true;
            if (!itemBounds || itemBounds.length < 4) return false;
            var left = itemBounds[0];
            var top = itemBounds[1];
            var right = itemBounds[2];
            var bottom = itemBounds[3];
            if (!isFinite(left) || !isFinite(top) || !isFinite(right) || !isFinite(bottom)) return false;
            if (right < selectionBounds.left) return false;
            if (left > selectionBounds.right) return false;
            if (top < selectionBounds.bottom) return false;
            if (bottom > selectionBounds.top) return false;
            return true;
        }

        function unlockItemChain(item) {
            if (!item) return;
            try {
                if (item.locked) item.locked = false;
            } catch (e) { }
            try {
                var parent = item.parent;
                var guard = 0;
                while (parent && parent !== item && guard < 10) {
                    try {
                        if (parent.locked) parent.locked = false;
                    } catch (e2) { }
                    try {
                        if (parent === parent.parent) break;
                        parent = parent.parent;
                    } catch (e2) {
                        // Parent may be invalid after copy/paste
                        break;
                    }
                    guard++;
                }
            } catch (e3) { }
        }

        function itemHitsSelection(item, selectionBounds) {
            if (!isItemValid(item)) return false;
            if (!selectionBounds) return true;
            var gb = null;
            try { gb = item.geometricBounds; } catch (e) { gb = null; }
            if (gb && gb.length === 4 && isFinite(gb[0]) && isFinite(gb[1]) && isFinite(gb[2]) && isFinite(gb[3])) {
                return boundsIntersect(gb, selectionBounds);
            }
            var center = null;
            try {
                var left = item.left;
                var top = item.top;
                var width = item.width;
                var height = item.height;
                if (isFinite(left) && isFinite(top) && isFinite(width) && isFinite(height)) {
                    center = [left + width / 2, top - height / 2];
                }
            } catch (e2) { }
            if (!center && item.typename === "PathItem" && item.pathPoints && item.pathPoints.length > 0) {
                try {
                    var pt = item.pathPoints[0].anchor;
                    center = [pt[0], pt[1]];
                } catch (e3) { }
            }
            if (!center) return false;
            return pointWithinBounds(selectionBounds, center[0], center[1]);
        }

        function removeItemsOnLayer(layer, selectionBounds) {
            if (!isItemValid(layer) || !layer.pageItems) return;
            var items = layer.pageItems;
            var toRemove = [];
            for (var i = 0; i < items.length; i++) {
                var item = items[i];
                if (!isItemValid(item)) continue;
                if (itemHitsSelection(item, selectionBounds)) {
                    toRemove.push(item);
                }
            }
            for (var r = 0; r < toRemove.length; r++) {
                var target = toRemove[r];
                if (!isItemValid(target)) continue;
                unlockItemChain(target);
                try { target.remove(); } catch (e) { }
            }
        }

        function forEachPathInItems(items, callback) {
            if (!items || !callback) return;
            function visit(item) {
                if (!isItemValid(item)) return;
                var type = item.typename;
                if (type === "PathItem") {
                    callback(item);
                } else if (type === "CompoundPathItem" && item.pathItems) {
                    for (var i = 0; i < item.pathItems.length; i++) visit(item.pathItems[i]);
                } else if (type === "GroupItem" && item.pageItems) {
                    for (var g = 0; g < item.pageItems.length; g++) visit(item.pageItems[g]);
                }
            }
            for (var i = 0; i < items.length; i++) visit(items[i]);
        }

        function cloneMatrixValues(matrix) {
            if (!matrix) return null;
            return {
                a: matrix.mValueA,
                b: matrix.mValueB,
                c: matrix.mValueC,
                d: matrix.mValueD,
                tx: matrix.mValueTX,
                ty: matrix.mValueTY
            };
        }

        function applyMatrixValues(item, values) {
            if (!item || !values) return;
            var m = new Matrix();
            m.mValueA = values.a;
            m.mValueB = values.b;
            m.mValueC = values.c;
            m.mValueD = values.d;
            m.mValueTX = values.tx;
            m.mValueTY = values.ty;
            item.matrix = m;
        }

        function toggleCenterlineVisibility(selectionItems, visible) {
            if (!selectionItems || selectionItems.length === 0) return;
            var didChange = false;
            forEachPathInItems(selectionItems, function (pathItem) {
                if (isEmoryCenterline(pathItem)) {
                    try {
                        pathItem.hidden = !visible;
                        didChange = true;
                    } catch (e) { }
                }
            });
            if (didChange) {
                try { app.redraw(); } catch (eRedraw) { }
            }
        }

        function resetEmoryGroups(selectionItems) {
            if (!selectionItems || selectionItems.length === 0) return;
            var emoryGroups = [];
            var connectedGroups = [];

            function collectGroups(item) {
                if (!item) return;
                if (item.typename === "GroupItem") {
                    // Collect Emory groups (have settings metadata)
                    if (loadSettingsFromGroup(item)) {
                        emoryGroups.push(item);
                    }
                    // Also collect "Connected Ductwork Group" groups (our new grouping approach)
                    else if (item.name && item.name.indexOf("Connected Ductwork Group") !== -1) {
                        connectedGroups.push(item);
                    }
                    for (var g = 0; g < item.pageItems.length; g++) {
                        collectGroups(item.pageItems[g]);
                    }
                } else if (item.typename === "CompoundPathItem") {
                    for (var c = 0; c < item.pathItems.length; c++) {
                        collectGroups(item.pathItems[c]);
                    }
                }
            }
            for (var i = 0; i < selectionItems.length; i++) {
                collectGroups(selectionItems[i]);
            }

            // Process Emory groups (extract centerlines, delete rectangles)
            for (var j = 0; j < emoryGroups.length; j++) {
                var grp = emoryGroups[j];
                if (!grp) continue;
                var centerlines = [];
                try { centerlines = EmoryGeometry.extractCenterlinesFromGroup(grp); } catch (eExt) { centerlines = []; }
                try { EmoryGeometry.deleteRectanglesFromGroup(grp); } catch (eDel) { }
                for (var cIdx = 0; cIdx < centerlines.length; cIdx++) {
                    var cl = centerlines[cIdx];
                    if (!cl) continue;
                    try {
                        cl.hidden = false;
                        var destLayer = grp.layer;
                        var originalLayerName = getOriginalLayerName(cl);
                        if (originalLayerName) {
                            try {
                                destLayer = getOrCreateLayer(originalLayerName);
                            } catch (eLookup) {
                                destLayer = grp.layer;
                            }
                        }
                        if (destLayer) {
                            var prevLocked = null;
                            try { prevLocked = destLayer.locked; destLayer.locked = false; } catch (eLock) { }
                            if (cl.layer !== destLayer) {
                                try { cl.move(destLayer, ElementPlacement.PLACEATEND); } catch (eMoveDest) { }
                            }
                            try { if (prevLocked !== null) destLayer.locked = prevLocked; } catch (eRestoreLock) { }
                        }
                        applyCenterlineStrokeColor(cl);
                    } catch (eMove) { }
                    try { clearEmoryCenterlineMetadata(cl); } catch (eMeta) { }
                    try { clearOriginalLayerName(cl); } catch (eClearOrig) { }
                }
                try { grp.remove(); } catch (eRem) { }
            }

            // Process Connected Ductwork Groups (just ungroup them, clearing metadata from paths)
            for (var k = 0; k < connectedGroups.length; k++) {
                var connGrp = connectedGroups[k];
                if (!connGrp) continue;

                // Collect all paths in this group and clear their metadata
                var pathsInGroup = [];
                function collectPathsFromGroup(container) {
                    if (!container) return;
                    try {
                        for (var pi = 0; pi < container.pathItems.length; pi++) {
                            pathsInGroup.push(container.pathItems[pi]);
                        }
                        for (var gi = 0; gi < container.groupItems.length; gi++) {
                            collectPathsFromGroup(container.groupItems[gi]);
                        }
                    } catch (e) { }
                }
                collectPathsFromGroup(connGrp);

                // Move paths out of group to their layer and clear metadata
                var groupLayer = connGrp.layer;
                for (var pi = 0; pi < pathsInGroup.length; pi++) {
                    var path = pathsInGroup[pi];
                    if (!path) continue;
                    try {
                        if (path.parent !== groupLayer) {
                            path.moveToBeginning(groupLayer);
                        }
                        clearEmoryCenterlineMetadata(path);
                        clearOriginalLayerName(path);
                        applyCenterlineStrokeColor(path);
                    } catch (e) { }
                }

                // Remove the empty group
                try { connGrp.remove(); } catch (eRemove) { }
            }
        }

        function revertSelectionToLines(selectionItems) {
            if (!selectionItems || selectionItems.length === 0) return;
            try {
                var selectionBounds = computeItemsBounds(selectionItems);
                if (!selectionBounds) {
                    try { app.executeMenuCommand("deselectall"); } catch (eNoSelCmd) {
                        try { doc.selection = null; } catch (eNoSel) { }
                    }
                    return;
                }

                // Step 1: Release all compound paths in ductwork layers within selection
                var ductworkLayerNames = [
                    "Green Ductwork",
                    "Light Green Ductwork",
                    "Blue Ductwork",
                    "Orange Ductwork",
                    "Light Orange Ductwork"
                ];

                var releasedPaths = [];
                for (var dlIdx = 0; dlIdx < ductworkLayerNames.length; dlIdx++) {
                    var dlName = ductworkLayerNames[dlIdx];
                    var layer = null;
                    try { layer = findLayerByNameDeep(dlName); } catch (e) { continue; }
                    if (!layer) continue;

                    // Find compound paths within bounds and release them
                    function releaseCompoundsInContainer(container) {
                        try {
                            // Release compound paths
                            for (var ci = container.compoundPathItems.length - 1; ci >= 0; ci--) {
                                var compound = container.compoundPathItems[ci];
                                if (!compound) continue;

                                // Check if within selection bounds
                                var itemBounds = null;
                                try { itemBounds = compound.geometricBounds; } catch (e) { continue; }
                                if (!itemBounds || itemBounds.length < 4) continue;

                                if (boundsOverlap(itemBounds, selectionBounds)) {
                                    try {
                                        // Store released paths
                                        var subPathCount = compound.pathItems.length;
                                        compound.selected = true;
                                        app.executeMenuCommand("releasePath");
                                        // After release, the paths become regular PathItems
                                    } catch (eRelease) {
                                        // If release fails, try to continue
                                    }
                                }
                            }

                            // Recursively process groups
                            for (var gi = 0; gi < container.groupItems.length; gi++) {
                                releaseCompoundsInContainer(container.groupItems[gi]);
                            }
                        } catch (e) { }
                    }

                    releaseCompoundsInContainer(layer);
                }

                // Step 2: Remove all Double Ductwork (closed paths) from ductwork layers within selection
                for (var dlIdx2 = 0; dlIdx2 < ductworkLayerNames.length; dlIdx2++) {
                    var dlName2 = ductworkLayerNames[dlIdx2];
                    var layer2 = null;
                    try { layer2 = findLayerByNameDeep(dlName2); } catch (e) { continue; }
                    if (!layer2) continue;

                    function removeClosedPathsInContainer(container) {
                        try {
                            // Remove closed paths (Double Ductwork rectangles/connectors)
                            for (var pi = container.pathItems.length - 1; pi >= 0; pi--) {
                                var path = container.pathItems[pi];
                                if (!path) continue;

                                try {
                                    if (path.closed) {
                                        var pathBounds = null;
                                        try { pathBounds = path.geometricBounds; } catch (e) { continue; }
                                        if (!pathBounds || pathBounds.length < 4) continue;

                                        if (boundsOverlap(pathBounds, selectionBounds)) {
                                            path.remove();
                                        }
                                    }
                                } catch (e) { }
                            }

                            // Recursively process groups
                            for (var gi = 0; gi < container.groupItems.length; gi++) {
                                removeClosedPathsInContainer(container.groupItems[gi]);
                            }
                        } catch (e) { }
                    }

                    removeClosedPathsInContainer(layer2);
                }

                // Step 3: Collect remaining open ductwork paths and reset them
                var linePaths = [];
                forEachPathInItems(selectionItems, function (pathItem) {
                    if (!isItemValid(pathItem)) return;
                    var layerName = "";
                    try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (eLayer) { layerName = ""; }
                    if (layerName && isDuctworkLineLayer(layerName)) {
                        // Only include open paths (centerlines), not closed paths (Double Ductwork)
                        try {
                            if (!pathItem.closed) {
                                linePaths.push(pathItem);
                            }
                        } catch (e) { }
                    }
                });

                // Step 4: Reset Emory groups (removes metadata and ungroupsmembers)
                resetEmoryGroups(selectionItems);

                try { app.executeMenuCommand("deselectall"); } catch (eCmd) {
                    try { doc.selection = null; } catch (eClearSel) { }
                }

                // Step 5: Remove all ductwork pieces (registers, units, thermostats) within selection
                var layerNames = getDuctworkPieceLayerNames();
                // NOTE: Do NOT clear "Thermostat Lines" - only clear ductwork pieces
                for (var i = 0; i < layerNames.length; i++) {
                    var layer = null;
                    try { layer = doc.layers.getByName(layerNames[i]); } catch (e) { layer = null; }
                    if (!layer) continue;
                    var prevLocked = null;
                    var prevVisible = null;
                    try { prevLocked = layer.locked; layer.locked = false; } catch (e1) { }
                    try { prevVisible = layer.visible; layer.visible = true; } catch (e2) { }
                    removeItemsOnLayer(layer, selectionBounds);
                    try { app.redraw(); } catch (eDraw) { }
                    try { if (prevLocked !== null) layer.locked = prevLocked; } catch (e3) { }
                    try { if (prevVisible !== null) layer.visible = prevVisible; } catch (e4) { }
                }

                // Step 6: Strip all styling and metadata from centerline paths
                for (var lp = 0; lp < linePaths.length; lp++) {
                    var pathItem = linePaths[lp];
                    if (!isItemValid(pathItem)) continue;

                    // Clear all metadata
                    try { clearOrthoLock(pathItem); } catch (e5) { }
                    try { clearRotationOverride(pathItem); } catch (e6) { }
                    try { clearPointRotation(pathItem); } catch (e7) { }
                    try { clearEmoryCenterlineMetadata(pathItem); } catch (e8) { }
                    try { clearOriginalLayerName(pathItem); } catch (e9) { }

                    // Unapply all graphic styles and reset to simple colored stroke
                    try { pathItem.unapplyAll(); } catch (e10) { }
                    applyCenterlineStrokeColor(pathItem);
                }

                try { app.redraw(); } catch (eRedraw) { }

                // Restore selection to the centerline paths
                try { app.executeMenuCommand("deselectall"); } catch (eCmd2) {
                    try { doc.selection = null; } catch (eReset) { }
                }

                // Select all the centerline paths
                for (var sp = 0; sp < linePaths.length; sp++) {
                    if (isItemValid(linePaths[sp])) {
                        try { linePaths[sp].selected = true; } catch (eSelect) { }
                    }
                }

                try { app.redraw(); } catch (eRedraw2) { }
            } catch (revertErr) {
                try { alert("Revert to Lines failed: " + revertErr); } catch (alertErr) { }
            }
        }

        function revertSelectionToPreOrtho(selectionItems) {
            var stats = {
                total: 0,
                reverted: 0,
                skipped: 0
            };

            if (!selectionItems || (selectionItems.length === 0 && !selectionItems.typename)) {
                return stats;
            }

            var paths = [];

            function collect(item) {
                if (!item) return;
                try {
                    var typeName = item.typename;
                    if (typeName === "PathItem") {
                        paths.push(item);
                    } else if (typeName === "GroupItem") {
                        for (var gi = 0; gi < item.pageItems.length; gi++) {
                            collect(item.pageItems[gi]);
                        }
                    } else if (typeName === "CompoundPathItem") {
                        for (var ci = 0; ci < item.pathItems.length; ci++) {
                            collect(item.pathItems[ci]);
                        }
                    }
                } catch (e) { }
            }

            if (selectionItems.length === undefined && selectionItems.typename) {
                collect(selectionItems);
            } else {
                for (var i = 0; i < selectionItems.length; i++) {
                    collect(selectionItems[i]);
                }
            }

            stats.total = paths.length;

            for (var p = 0; p < paths.length; p++) {
                var pathItem = paths[p];
                if (!pathItem) {
                    stats.skipped++;
                    continue;
                }
                var geometry = getPreOrthoGeometry(pathItem);
                if (!geometry) {
                    stats.skipped++;
                    continue;
                }
                if (applyPreOrthoGeometry(pathItem, geometry)) {
                    stats.reverted++;
                } else {
                    stats.skipped++;
                }
                clearPreOrthoGeometry(pathItem);
                // CRITICAL: Also clear ORTHO_LOCK flag so path can be re-orthogonalized
                // Otherwise the path will skip orthogonalization next time and won't create new backup
                removeNoteTag(pathItem, ORTHO_LOCK_TAG);
            }

            return stats;
        }

        function layerNameInList(name, list) {
            if (!name || !list) return false;
            for (var i = 0; i < list.length; i++) {
                if (list[i] === name) return true;
            }
            return false;
        }

        function collectScaleTargetsFromItem(item, targets, visited) {
            if (!item) return;
            if (!visited) visited = [];
            if (arrayIndexOf(visited, item) !== -1) return;
            visited.push(item);

            var layerName = "";
            try { layerName = item.layer ? item.layer.name : ""; } catch (e) { layerName = ""; }
            var typeName = item.typename;
            var isPieceLayer = layerNameInList(layerName, DUCTWORK_PIECES);
            var isLineLayer = isDuctworkLineLayer(layerName);

            if (isPieceLayer) {
                if (arrayIndexOf(targets, item) === -1) targets.push(item);
                return;
            }

            // Include ductwork lines for stroke scaling using resize method
            if (isLineLayer && (typeName === "PathItem" || typeName === "CompoundPathItem")) {
                if (arrayIndexOf(targets, item) === -1) targets.push(item);
                return;
            }

            if (typeName === "GroupItem" || typeName === "Layer") {
                try {
                    for (var gi = 0; gi < item.pageItems.length; gi++) {
                        collectScaleTargetsFromItem(item.pageItems[gi], targets, visited);
                    }
                } catch (eGroup) { }
                return;
            }

            // DO NOT recurse into CompoundPathItem children - the reference script doesn't
            // Scale the CompoundPathItem itself, which is already handled above
            if (typeName === "CompoundPathItem") {
                // Already handled above in the ductwork line check
                return;
            }

            if (typeName === "PathItem" || typeName === "PlacedItem" || typeName === "RasterItem" || typeName === "TextFrame") {
                if (arrayIndexOf(targets, item) === -1) targets.push(item);
            }
        }

        function collectScaleTargets(selectionItems) {
            var targets = [];
            if (!selectionItems) return targets;
            if (selectionItems.length === undefined && selectionItems.typename) {
                collectScaleTargetsFromItem(selectionItems, targets, []);
            } else {
                for (var i = 0; i < selectionItems.length; i++) {
                    collectScaleTargetsFromItem(selectionItems[i], targets, []);
                }
            }
            return targets;
        }

        function scaleSelectionAbsolute(percent, selectionItems) {
            var stats = {
                total: 0,
                scaled: 0,
                skipped: 0
            };
            if (!app.documents.length) return stats;
            var doc = app.activeDocument;
            if (!selectionItems) {
                try { selectionItems = doc.selection; } catch (eSel) { selectionItems = null; }
            }
            if (!selectionItems || (selectionItems.length === 0 && !selectionItems.typename)) {
                return stats;
            }

            var targets = collectScaleTargets(selectionItems);
            stats.total = targets.length;

            for (var i = 0; i < targets.length; i++) {
                var target = targets[i];
                if (!target) {
                    stats.skipped++;
                    continue;
                }
                storePreScaleData(target);
                if (applyScaleToItem(target, percent)) {
                    stats.scaled++;
                } else {
                    stats.skipped++;
                }
            }

            try { doc.selection = targets; } catch (eSel2) { }
            try { app.redraw(); } catch (eRedraw) { }
            return stats;
        }

        function resetSelectionScale(selectionItems) {
            var stats = {
                total: 0,
                reset: 0,
                skipped: 0
            };
            if (!app.documents.length) return stats;
            var doc = app.activeDocument;
            if (!selectionItems) {
                try { selectionItems = doc.selection; } catch (eSel) { selectionItems = null; }
            }
            if (!selectionItems || (selectionItems.length === 0 && !selectionItems.typename)) {
                return stats;
            }

            var scaleStats = scaleSelectionAbsolute(100, selectionItems);
            stats.total = scaleStats.total;
            stats.reset = scaleStats.scaled;
            stats.skipped = scaleStats.skipped;

            if (scaleStats.scaled > 0) {
                var resetTargets = collectScaleTargets(selectionItems);
                for (var i = 0; i < resetTargets.length; i++) {
                    var resetItem = resetTargets[i];
                    if (!resetItem) continue;
                    var data = getPreScaleData(resetItem);
                    if (!data) continue;
                    if (Math.abs(data.lastPercent - 100) < 0.0001 && Math.abs((data.basePercent || 100) - 100) < 0.0001) {
                        clearPreScaleData(resetItem);
                    }
                }
            }
            return stats;
        }

        function rotateSelection(angle, selectionItems) {
            var stats = {
                total: 0,
                rotated: 0,
                skipped: 0
            };
            if (!app.documents.length) return stats;
            var doc = app.activeDocument;
            if (!selectionItems) {
                try { selectionItems = doc.selection; } catch (eSel) { selectionItems = null; }
            }
            if (!selectionItems || (selectionItems.length === 0 && !selectionItems.typename)) {
                return stats;
            }

            var targets = [];
            function collect(item) {
                if (!item) return;
                var layerName = "";
                try { layerName = item.layer ? item.layer.name : ""; } catch (e) { layerName = ""; }
                if (layerNameInList(layerName, DUCTWORK_PIECES)) {
                    if (arrayIndexOf(targets, item) === -1) targets.push(item);
                    return;
                }
                var typeName = item.typename;
                if (typeName === "GroupItem") {
                    try {
                        for (var gi = 0; gi < item.pageItems.length; gi++) {
                            collect(item.pageItems[gi]);
                        }
                    } catch (eGroup) { }
                } else if (typeName === "CompoundPathItem") {
                    try {
                        for (var ci = 0; ci < item.pathItems.length; ci++) {
                            collect(item.pathItems[ci]);
                        }
                    } catch (eComp) { }
                }
            }

            if (selectionItems.length === undefined && selectionItems.typename) {
                collect(selectionItems);
            } else {
                for (var i = 0; i < selectionItems.length; i++) {
                    collect(selectionItems[i]);
                }
            }

            stats.total = targets.length;
            for (var t = 0; t < targets.length; t++) {
                var target = targets[t];
                if (!target) {
                    stats.skipped++;
                    continue;
                }
                try {
                    target.rotate(angle, true, true, true, true, Transformation.CENTER);
                    stats.rotated++;
                    if (target.typename === "PlacedItem") {
                        var appliedAngle = null;
                        try {
                            var m = target.matrix;
                            appliedAngle = Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
                        } catch (eMatrix) {
                            appliedAngle = null;
                        }
                        if (appliedAngle !== null && isFinite(appliedAngle)) {
                            setPlacedRotation(target, normalizeAngle(appliedAngle));
                        }
                    }
                } catch (eRotate) {
                    stats.skipped++;
                }
            }
            try { doc.selection = targets; } catch (eSelReset) { }
            try { app.redraw(); } catch (eRedraw) { }
            return stats;
        }

        /**
         * Rotate selection to an ABSOLUTE angle (not cumulative/relative).
         * If the current rotation is 45° and you pass 65°, the item rotates to 65° (not to 110°).
         * Entering 0° resets the item to its base/original orientation.
         */
        function rotateSelectionAbsolute(targetAngle, selectionItems) {
            // Use safe debug logging - MDUX_debugLog is always available from panel-bridge.jsx
            // but addDebug depends on GLOBAL_DEBUG_LOG which isn't initialized until later
            var safeDebug = (typeof MDUX_debugLog === 'function') ? MDUX_debugLog : function () { };
            safeDebug("[ROT-ABS] === rotateSelectionAbsolute called with targetAngle=" + targetAngle + " ===");
            var stats = {
                total: 0,
                rotated: 0,
                skipped: 0
            };
            if (!app.documents.length) return stats;
            var doc = app.activeDocument;
            if (!selectionItems) {
                try { selectionItems = doc.selection; } catch (eSel) { selectionItems = null; }
            }
            if (!selectionItems || (selectionItems.length === 0 && !selectionItems.typename)) {
                safeDebug("[ROT-ABS] No selection items");
                return stats;
            }

            // Collect rotation targets (ductwork pieces only)
            var targets = [];
            function collect(item) {
                if (!item) return;
                var layerName = "";
                try { layerName = item.layer ? item.layer.name : ""; } catch (e) { layerName = ""; }
                if (layerNameInList(layerName, DUCTWORK_PIECES)) {
                    if (arrayIndexOf(targets, item) === -1) targets.push(item);
                    return;
                }
                var typeName = item.typename;
                if (typeName === "GroupItem") {
                    try {
                        for (var gi = 0; gi < item.pageItems.length; gi++) {
                            collect(item.pageItems[gi]);
                        }
                    } catch (eGroup) { }
                } else if (typeName === "CompoundPathItem") {
                    try {
                        for (var ci = 0; ci < item.pathItems.length; ci++) {
                            collect(item.pathItems[ci]);
                        }
                    } catch (eComp) { }
                }
            }

            if (selectionItems.length === undefined && selectionItems.typename) {
                collect(selectionItems);
            } else {
                for (var i = 0; i < selectionItems.length; i++) {
                    collect(selectionItems[i]);
                }
            }

            stats.total = targets.length;
            safeDebug("[ROT-ABS] Found " + targets.length + " rotation targets");

            for (var t = 0; t < targets.length; t++) {
                var target = targets[t];
                if (!target) {
                    stats.skipped++;
                    continue;
                }
                try {
                    var currentAngle = 0;
                    var typeName = target.typename;
                    safeDebug("[ROT-ABS] Target " + t + ": typename=" + typeName);

                    if (typeName === "PlacedItem") {
                        // First check MDUX_RotationOverride - this is set by Process Ductwork
                        // and is the authoritative source for the intended rotation angle
                        var meta = MDUX_getMetadata_inner(target);
                        if (meta && meta.MDUX_RotationOverride !== undefined && meta.MDUX_RotationOverride !== null) {
                            currentAngle = parseFloat(meta.MDUX_RotationOverride);
                            safeDebug("[ROT-ABS] Using MDUX_RotationOverride: " + currentAngle);
                        } else {
                            // Fallback: try MD:PLACED_ROT token (for items rotated via panel)
                            var rotDelta = getPlacedRotationDelta(target);
                            safeDebug("[ROT-ABS] getPlacedRotationDelta returned: " + rotDelta);

                            if (rotDelta !== null && isFinite(rotDelta)) {
                                // Has MD:PLACED_ROT - calculate full rotation
                                var baseRot = ensurePlacedBaseRotation(target);
                                currentAngle = normalizeAngle(baseRot + rotDelta);
                                safeDebug("[ROT-ABS] Calculated from base(" + baseRot + ") + delta(" + rotDelta + ") = " + currentAngle);
                            } else {
                                // BACKWARDS COMPATIBILITY: No metadata at all
                                // Use the visual rotation directly from the transformation matrix
                                var matrixAngle = computePlacedPrimaryAngle(target);
                                currentAngle = normalizeAngle(matrixAngle);
                                safeDebug("[ROT-ABS] LEGACY: Using matrix angle: " + matrixAngle + " -> normalized: " + currentAngle);
                            }
                        }
                        safeDebug("[ROT-ABS] currentAngle = " + currentAngle);
                    } else {
                        // For other items, try to read from metadata or assume 0
                        var storedRot2 = getPlacedRotation(target);
                        if (storedRot2 !== null && isFinite(storedRot2)) {
                            currentAngle = storedRot2;
                        }
                    }

                    // Normalize angles for comparison
                    currentAngle = normalizeAngle(currentAngle);
                    var normalizedTarget = normalizeAngle(targetAngle);
                    safeDebug("[ROT-ABS] normalized: current=" + currentAngle.toFixed(2) + ", target=" + normalizedTarget.toFixed(2));

                    // Calculate the delta needed to reach target angle
                    var deltaAngle = normalizedTarget - currentAngle;

                    // Optimize rotation direction (take shortest path)
                    if (deltaAngle > 180) deltaAngle -= 360;
                    if (deltaAngle < -180) deltaAngle += 360;
                    safeDebug("[ROT-ABS] deltaAngle = " + deltaAngle.toFixed(2));

                    // Skip if already at target angle (within tolerance)
                    if (Math.abs(deltaAngle) < 0.01) {
                        safeDebug("[ROT-ABS] Already at target angle, skipping rotation");
                        // Already at target, but update metadata to be consistent
                        if (typeName === "PlacedItem") {
                            setPlacedRotation(target, normalizedTarget);
                        }
                        stats.rotated++;
                        continue;
                    }

                    // Apply the delta rotation
                    safeDebug("[ROT-ABS] Applying rotation delta: " + deltaAngle.toFixed(2));
                    target.rotate(deltaAngle, true, true, true, true, Transformation.CENTER);
                    stats.rotated++;

                    // Update rotation metadata
                    if (typeName === "PlacedItem") {
                        setPlacedRotation(target, normalizedTarget);
                        safeDebug("[ROT-ABS] Stored MD:PLACED_ROT: " + normalizedTarget);

                        // Also update MDUX_RotationOverride so the display shows the correct angle
                        var updateMeta = MDUX_getMetadata_inner(target) || {};
                        updateMeta.MDUX_RotationOverride = normalizedTarget;
                        MDUX_setMetadata_inner(target, updateMeta);
                        safeDebug("[ROT-ABS] Updated MDUX_RotationOverride: " + normalizedTarget);

                        // Refresh the link to fix the bounding box after rotation
                        // IMPORTANT: Save note before relink - relink wipes metadata!
                        try {
                            var linkedFile = target.file;
                            if (linkedFile && linkedFile.exists) {
                                var savedNote = target.note || "";
                                safeDebug("[ROT-ABS] Saved note before relink: " + savedNote.substring(0, 100));
                                target.file = linkedFile;
                                try { target.relink(linkedFile); } catch (eRl) { }
                                try { target.update(); } catch (eUp) { }
                                // Restore the note after relink
                                if (savedNote) {
                                    target.note = savedNote;
                                    safeDebug("[ROT-ABS] Restored note after relink");
                                }
                                safeDebug("[ROT-ABS] Refreshed link (file+relink+update) to fix bounding box");
                            }
                        } catch (eRelink) {
                            safeDebug("[ROT-ABS] Could not refresh link: " + eRelink);
                        }

                        // Verify what was stored
                        var verifyRot = getPlacedRotation(target);
                        safeDebug("[ROT-ABS] Verify after store: " + verifyRot);
                    }
                } catch (eRotate) {
                    safeDebug("[ROT-ABS] ERROR: " + eRotate);
                    stats.skipped++;
                }
            }

            try { doc.selection = targets; } catch (eSelReset) { }
            try { app.redraw(); } catch (eRedraw) { }
            safeDebug("[ROT-ABS] Complete. Stats: " + JSON.stringify(stats));
            return stats;
        }

        function setLayerLockRecursive(layer, locked) {
            if (!layer) return;
            try { layer.locked = locked; } catch (e) { }
            try {
                for (var i = 0; i < layer.layers.length; i++) {
                    setLayerLockRecursive(layer.layers[i], locked);
                }
            } catch (eSub) { }
        }

        function isolateLayers(allowedNames, message) {
            if (!app.documents.length) return "ERROR:No Illustrator document is open.";
            var doc = app.activeDocument;
            var allowedMap = {};
            for (var i = 0; i < allowedNames.length; i++) {
                allowedMap[allowedNames[i]] = true;
            }
            for (var li = 0; li < doc.layers.length; li++) {
                var layer = doc.layers[li];
                var allow = !!allowedMap[layer.name];
                setLayerLockRecursive(layer, !allow);
                if (allow) {
                    try { layer.visible = true; } catch (eVis) { }
                }
            }
            try { app.redraw(); } catch (eRedraw) { }
            return message;
        }

        function isolateDuctworkParts() {
            return isolateLayers(DUCTWORK_PIECES, "Isolated ductwork part layers.");
        }

        function isolateDuctworkLines() {
            return isolateLayers(DUCTWORK_LINES, "Isolated ductwork line layers.");
        }

        function unlockAllDuctworkLayers() {
            if (!app.documents.length) return "ERROR:No Illustrator document is open.";
            var doc = app.activeDocument;
            var allowed = DUCTWORK_PIECES.concat(DUCTWORK_LINES);
            var allowedMap = {};
            for (var i = 0; i < allowed.length; i++) {
                allowedMap[allowed[i]] = true;
            }
            for (var li = 0; li < doc.layers.length; li++) {
                var layer = doc.layers[li];
                if (allowedMap[layer.name]) {
                    setLayerLockRecursive(layer, false);
                    try { layer.visible = true; } catch (eVis) { }
                }
            }
            try { app.redraw(); } catch (eRedraw) { }
            return "Unlocked ductwork layers.";
        }

        function importDuctworkGraphicStyles() {
            if (!app.documents.length) return "ERROR:No Illustrator document is open.";
            var destDoc = app.activeDocument;
            var sourceFile = new File("E:/Work/Work/Floorplans/Ductwork Assets/DuctworkLines.ai");
            if (!sourceFile.exists) {
                return "ERROR:DuctworkLines.ai not found.";
            }
            var sourceDoc = null;
            try {
                sourceDoc = app.open(sourceFile);
                app.activeDocument = sourceDoc;
                for (var L = 0; L < sourceDoc.layers.length; L++) {
                    try {
                        sourceDoc.layers[L].locked = false;
                        sourceDoc.layers[L].visible = true;
                    } catch (eUnlock) { }
                }
                var items = sourceDoc.pageItems;
                if (!items || items.length === 0) {
                    sourceDoc.close(SaveOptions.DONOTSAVECHANGES);
                    app.activeDocument = destDoc;
                    return "ERROR:Source document contained no artwork.";
                }
                for (var i = 0; i < items.length; i++) {
                    try { items[i].selected = true; } catch (eSel) { }
                }
                app.copy();
                sourceDoc.close(SaveOptions.DONOTSAVECHANGES);
            } catch (e) {
                try {
                    if (sourceDoc) sourceDoc.close(SaveOptions.DONOTSAVECHANGES);
                } catch (eClose) { }
                app.activeDocument = destDoc;
                return "ERROR:Error importing styles: " + e;
            }

            app.activeDocument = destDoc;
            destDoc.selection = null;
            var tempLayerName = "__MDUX_STYLE_IMPORT__";
            var tempLayer;
            try {
                tempLayer = destDoc.layers.getByName(tempLayerName);
            } catch (eTemp) {
                tempLayer = destDoc.layers.add();
                tempLayer.name = tempLayerName;
            }
            tempLayer.locked = false;
            destDoc.activeLayer = tempLayer;
            app.executeMenuCommand("pasteInPlace");
            var pasted = null;
            try { pasted = destDoc.selection; } catch (eSel) { pasted = null; }
            if (pasted) {
                if (pasted.length === undefined) pasted = [pasted];
                for (var p = pasted.length - 1; p >= 0; p--) {
                    try { pasted[p].remove(); } catch (eRem) { }
                }
            }
            destDoc.selection = null;
            try {
                if (!tempLayer.pageItems.length && !tempLayer.groupItems.length) {
                    tempLayer.remove();
                }
            } catch (eRemove) { }
            try { app.redraw(); } catch (eRedraw2) { }
            return "Graphic styles imported.";
        }

        function createStandardLayerBlock() {
            if (!app.documents.length) return "ERROR:No Illustrator document is open.";
            try {
                ensureFinalLayerBlockOrder();
                try { app.redraw(); } catch (eRedraw) { }
                return "Standard ductwork layers ensured.";
            } catch (e) {
                return "ERROR:" + e;
            }
        }

        /**
         * Reset graphic styles on ductwork line items in the selection.
         * This reapplies the appropriate graphic style to each ductwork line,
         * effectively resetting stroke widths to their default values.
         * @param {Array} selectionItems - Items to process (defaults to current selection)
         * @returns {number} Number of items whose styles were reset
         */
        function resetDuctworkLineStyles(selectionItems) {
            if (!app.documents.length) return 0;
            var doc = app.activeDocument;

            var items = selectionItems;
            if (!items) {
                try { items = doc.selection; } catch (eSel) { items = null; }
            }
            if (!items) return 0;

            var processedItems = [];
            if (items.length === undefined && items.typename) {
                processedItems.push(items);
            } else {
                for (var idx = 0; idx < items.length; idx++) {
                    processedItems.push(items[idx]);
                }
            }
            if (processedItems.length === 0) return 0;

            var resetCount = 0;

            // Process each selected item
            for (var i = 0; i < processedItems.length; i++) {
                var item = processedItems[i];
                if (!item) continue;

                try {
                    var layerName = item.layer ? item.layer.name : null;
                    if (!layerName) continue;

                    // Check if this item is on a ductwork line layer
                    var isDuctworkLine = false;
                    for (var j = 0; j < DUCTWORK_LINES.length; j++) {
                        if (layerName === DUCTWORK_LINES[j]) {
                            isDuctworkLine = true;
                            break;
                        }
                    }

                    if (!isDuctworkLine) continue;

                    // Get the appropriate graphic style for this layer
                    var styleName = NORMAL_STYLE_MAP[layerName];
                    if (!styleName) continue;

                    var graphicStyle = getGraphicStyleByNameFlexible(styleName);
                    if (!graphicStyle) continue;

                    // Reapply the graphic style
                    try {
                        graphicStyle.applyTo(item);
                        resetCount++;
                    } catch (eApply) {
                        // Failed to apply, continue
                    }
                } catch (eItem) {
                    // Failed to process item, continue
                }
            }

            return resetCount;
        }

        function summarizeRotationOverrides(selectionItems) {
            var summary = {
                available: false,
                reason: null,
                rotations: [],
                formatted: "",
                count: 0
            };

            if (!app.documents.length) {
                summary.reason = "no-document";
                return summary;
            }

            var doc = app.activeDocument;
            var items = selectionItems;
            if (!items) {
                try { items = doc.selection; } catch (eSel) { items = null; }
            }

            if (!items || (items.length === 0 && !items.typename)) {
                summary.available = true;
                summary.reason = "no-selection";
                return summary;
            }

            var rotationMap = {};
            var rotationList = [];

            function addRotation(value) {
                if (typeof value !== "number" || !isFinite(value)) return;
                var normalized = normalizeAngle(value);
                var key = normalized.toFixed(2);
                if (!rotationMap.hasOwnProperty(key)) {
                    rotationMap[key] = normalized;
                    rotationList.push(normalized);
                }
            }

            function selectionToArray(sel) {
                if (!sel) return [];
                if (sel.length === undefined && sel.typename) {
                    return [sel];
                }
                var arr = [];
                for (var i = 0; i < sel.length; i++) arr.push(sel[i]);
                return arr;
            }

            forEachPathInItems(items, function (pathItem) {
                if (!pathItem) return;
                var layerName = "";
                try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (eLayer) { layerName = ""; }
                if (!layerName || !isDuctworkLineLayer(layerName)) return;

                // Check the path itself for rotation override
                var existing = getRotationOverride(pathItem);
                if (existing !== null && existing !== undefined) {
                    addRotation(existing);
                    return;
                }

                // For compound paths, check MDUX_META first, then child pathItems
                try {
                    if (pathItem.typename === "CompoundPathItem") {
                        // Check compound path's MDUX_META for rotation override
                        var compoundMeta = MDUX_getMetadata(pathItem);
                        if (compoundMeta && compoundMeta.MDUX_RotationOverride !== undefined && compoundMeta.MDUX_RotationOverride !== null) {
                            var compoundRot = parseFloat(compoundMeta.MDUX_RotationOverride);
                            if (isFinite(compoundRot)) {
                                addRotation(compoundRot);
                                return;
                            }
                        }

                        // Check child pathItems for rotation overrides (MD:ROT= tokens are on segments)
                        if (pathItem.pathItems) {
                            for (var cpi = 0; cpi < pathItem.pathItems.length; cpi++) {
                                var childPath = pathItem.pathItems[cpi];
                                var childRot = getRotationOverride(childPath);
                                if (childRot !== null && childRot !== undefined) {
                                    addRotation(childRot);
                                    return; // Found rotation in child, stop checking
                                }
                            }
                        }
                    }
                } catch (eCompound) {
                    // Ignore compound path errors
                }
            });

            var selectionArray = selectionToArray(items);
            MDUX_debugLog("[ROT-SUMMARY] Processing " + selectionArray.length + " selection items...");
            for (var si = 0; si < selectionArray.length; si++) {
                var selectionItem = selectionArray[si];
                if (!selectionItem) continue;
                MDUX_debugLog("[ROT-SUMMARY] Item " + si + ": typename=" + selectionItem.typename);
                if (selectionItem.typename === "PlacedItem") {
                    // Only check for stored rotation override in metadata - do NOT fall back to transform matrix
                    // The rotation override box should only show explicitly set rotation overrides
                    try {
                        var meta = MDUX_getMetadata_inner(selectionItem);
                        MDUX_debugLog("[ROT-SUMMARY] PlacedItem meta: " + (meta ? JSON.stringify(meta).substring(0, 200) : "null"));
                        if (meta && meta.MDUX_RotationOverride !== undefined && meta.MDUX_RotationOverride !== null) {
                            var storedRotOverride = parseFloat(meta.MDUX_RotationOverride);
                            MDUX_debugLog("[ROT-SUMMARY] PlacedItem has MDUX_RotationOverride: " + storedRotOverride);
                            if (isFinite(storedRotOverride)) {
                                addRotation(storedRotOverride);
                            }
                        } else {
                            MDUX_debugLog("[ROT-SUMMARY] PlacedItem has no MDUX_RotationOverride - skipping (will not show in rotation override box)");
                        }
                    } catch (eMetaRot) {
                        MDUX_debugLog("[ROT-SUMMARY] Error reading PlacedItem meta: " + eMetaRot);
                    }
                    continue;
                }

                // Skip anchor points (single-point PathItems) for rotation override summary
                // The rotation override box should only show values from placed ductwork parts
                // Anchors use MD:POINT_ROT= format which is separate from placed item rotation
                if (selectionItem.typename === "PathItem" && selectionItem.pathPoints && selectionItem.pathPoints.length === 1) {
                    continue;
                }

                // Handle CompoundPathItems - check MDUX_META then child paths for rotation override
                if (selectionItem.typename === "CompoundPathItem") {
                    MDUX_debugLog("[ROT-SUMMARY] Found CompoundPathItem in selection, checking for rotation override...");
                    var foundCompoundRot = false;
                    // First check compound path's MDUX_META for rotation override
                    try {
                        var compMeta = MDUX_getMetadata_inner(selectionItem);
                        MDUX_debugLog("[ROT-SUMMARY] Compound MDUX_META: " + (compMeta ? JSON.stringify(compMeta).substring(0, 200) : "null"));
                        if (compMeta && compMeta.MDUX_RotationOverride !== undefined && compMeta.MDUX_RotationOverride !== null) {
                            var compRot = parseFloat(compMeta.MDUX_RotationOverride);
                            MDUX_debugLog("[ROT-SUMMARY] Found MDUX_RotationOverride in compound meta: " + compRot);
                            if (isFinite(compRot)) {
                                addRotation(compRot);
                                foundCompoundRot = true;
                            }
                        }
                    } catch (eCompMeta) {
                        MDUX_debugLog("[ROT-SUMMARY] Error reading compound meta: " + eCompMeta);
                    }

                    // If not found in MDUX_META, check child pathItems for MD:ROT= tokens
                    if (!foundCompoundRot && selectionItem.pathItems) {
                        MDUX_debugLog("[ROT-SUMMARY] Checking " + selectionItem.pathItems.length + " child paths for MD:ROT= tokens...");
                        for (var cpIdx = 0; cpIdx < selectionItem.pathItems.length; cpIdx++) {
                            var childPath = selectionItem.pathItems[cpIdx];
                            var childNote = "";
                            try { childNote = childPath.note || ""; } catch (eNote) { childNote = ""; }
                            MDUX_debugLog("[ROT-SUMMARY] Child " + cpIdx + " note: " + childNote.substring(0, 100));
                            var childRotOverride = getRotationOverride(childPath);
                            if (childRotOverride !== null && childRotOverride !== undefined) {
                                MDUX_debugLog("[ROT-SUMMARY] Found MD:ROT= in child " + cpIdx + ": " + childRotOverride);
                                addRotation(childRotOverride);
                                foundCompoundRot = true;
                                break; // Found rotation in child, stop checking
                            }
                        }
                    }
                    if (!foundCompoundRot) {
                        MDUX_debugLog("[ROT-SUMMARY] No rotation override found in compound path or children");
                    }
                }
            }

            // NOTE: Removed fallback that calculated rotation from path geometry.
            // We only want to show rotation values stored in metadata, not computed from geometry.

            rotationList.sort(function (a, b) {
                return a - b;
            });

            function formatRotationValue(val) {
                var rounded = Math.round(val * 100) / 100;
                if (Math.abs(rounded - Math.round(rounded)) < 1e-6) {
                    return String(Math.round(rounded));
                }
                return rounded.toFixed(2).replace(/\.0+$/, "").replace(/\.([0-9]*[1-9])0+$/, ".$1");
            }

            var formattedList = [];
            for (var ri = 0; ri < rotationList.length; ri++) {
                formattedList.push(formatRotationValue(rotationList[ri]));
            }

            summary.available = true;
            summary.count = rotationList.length;
            summary.rotations = rotationList;
            summary.formatted = formattedList.join(", ");
            return summary;
        }

        function registerMDUXExports() {
            try {
                var mdNamespace = null;
                if (typeof MDUX !== "undefined" && MDUX) {
                    mdNamespace = MDUX;
                } else if ($.global.MDUX) {
                    mdNamespace = $.global.MDUX;
                } else {
                    mdNamespace = {};
                }
                $.global.MDUX = mdNamespace;
                try {
                    MDUX = mdNamespace;
                } catch (eAssign) {
                    // ignore assignment issues
                }
                // Store session ID to detect stale MDUX from previous Illustrator sessions
                // Panel-bridge checks this to know if closures are still valid
                mdNamespace._bridgeSessionId = $.global.MDUX_BRIDGE_SESSION_ID || "";
                mdNamespace.revertSelectionToPreOrtho = function (selectionItems) {
                    var items = selectionItems;
                    if (!items && app.documents.length) {
                        try { items = app.activeDocument.selection; } catch (eSel) { items = null; }
                    }
                    return revertSelectionToPreOrtho(items);
                };
                mdNamespace.rotateSelection = function (angle, selectionItems) {
                    return rotateSelection(angle, selectionItems);
                };
                mdNamespace.rotateSelectionAbsolute = function (targetAngle, selectionItems) {
                    return rotateSelectionAbsolute(targetAngle, selectionItems);
                };
                mdNamespace.scaleSelectionAbsolute = function (percent, selectionItems) {
                    return scaleSelectionAbsolute(percent, selectionItems);
                };
                mdNamespace.resetSelectionScale = function (selectionItems) {
                    return resetSelectionScale(selectionItems);
                };
                mdNamespace.resetDuctworkLineStyles = function (selectionItems) {
                    return resetDuctworkLineStyles(selectionItems);
                };
                mdNamespace.getRotationOverrideSummary = function (selectionItems) {
                    return summarizeRotationOverrides(selectionItems);
                };
                mdNamespace.isolateDuctworkParts = function () {
                    return isolateDuctworkParts();
                };
                mdNamespace.isolateDuctworkLines = function () {
                    return isolateDuctworkLines();
                };
                mdNamespace.unlockAllDuctworkLayers = function () {
                    return unlockAllDuctworkLayers();
                };
                mdNamespace.importDuctworkGraphicStyles = function () {
                    return importDuctworkGraphicStyles();
                };
                mdNamespace.createStandardLayerBlock = function () {
                    return createStandardLayerBlock();
                };
                mdNamespace.checkSkipOrthoState = function (selectionItems) {
                    var items = selectionItems;
                    if (!items && app.documents.length) {
                        try { items = app.activeDocument.selection; } catch (eSel) { items = null; }
                    }
                    return checkSkipOrthoState(items);
                };
                mdNamespace.clearRotationOverride = function (pathItem) {
                    return clearRotationOverride(pathItem);
                };
                mdNamespace.getAllPathItemsInGroup = function (groupItem) {
                    return getAllPathItemsInGroup(groupItem);
                };
            } catch (e) { }
        }

        registerMDUXExports();


        // Helper function to check if two bounds overlap
        function boundsOverlap(bounds1, bounds2) {
            if (!bounds1 || !bounds2 || bounds1.length < 4 || bounds2.length < 4) return false;
            // bounds format: [left, top, right, bottom]
            return !(bounds1[2] < bounds2[0] || bounds1[0] > bounds2[2] ||
                bounds1[3] > bounds2[1] || bounds1[1] < bounds2[3]);
        }

        function captureItemTransform(item) {
            if (!item || item.typename !== "PlacedItem") return;
            try {
                var b = item.geometricBounds;
                var key = ((b[0] + b[2]) / 2).toFixed(2) + "_" + ((b[1] + b[3]) / 2).toFixed(2);

                // Get actual rotation from matrix
                var rot = null;
                try {
                    var m = item.matrix;
                    rot = Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
                    // DON'T normalize - keep raw angle
                } catch (e) { }

                // Get actual scale from matrix
                var scale = null;
                try {
                    var m2 = item.matrix;
                    scale = Math.sqrt((m2.mValueA * m2.mValueA) + (m2.mValueB * m2.mValueB)) * 100;
                } catch (e) { }

                initialPlacedTransforms[key] = {
                    rotation: rot,
                    scale: scale,
                    layer: item.layer ? item.layer.name : null
                };
            } catch (e) { }
        }

        // Capture all placed items in ductwork piece layers
        for (var pi = 0; pi < doc.placedItems.length; pi++) {
            var pItem = doc.placedItems[pi];
            if (!pItem.layer) continue;
            var layerName = pItem.layer.name;
            var isDuctworkPieceLayer = false;
            for (var dpi = 0; dpi < DUCTWORK_PIECES.length; dpi++) {
                if (DUCTWORK_PIECES[dpi] === layerName) {
                    isDuctworkPieceLayer = true;
                    break;
                }
            }
            if (isDuctworkPieceLayer) {
                captureItemTransform(pItem);
            }
        }

        // Returns a shallow copy of the ductwork piece layer names
        function getDuctworkPieceLayerNames() {
            return DUCTWORK_PIECES.slice();
        }

        // Returns an array of Layer objects that exist in the document for the ductwork pieces
        // Skips missing layers silently. Accepts optional docParam (defaults to active doc).
        function getDuctworkPieceLayers(docParam) {
            docParam = docParam || doc;
            var layers = [];
            for (var i = 0; i < DUCTWORK_PIECES.length; i++) {
                try {
                    var l = docParam.layers.getByName(DUCTWORK_PIECES[i]);
                    if (l) layers.push(l);
                } catch (e) {
                    // layer not found — skip
                }
            }
            return layers;
        }

        // Returns true if the provided layer name (string) is part of the Ductwork Pieces group
        function isDuctworkPieceLayerName(name) {
            if (typeof name !== 'string') return false;
            var n = name.trim();
            for (var i = 0; i < DUCTWORK_PIECES.length; i++) if (DUCTWORK_PIECES[i] === n) return true;
            return false;
        }

        // Collects and returns all PathItems from the named Ductwork Pieces layers (uses getPathsOnLayerAll)
        function getAllPathsInDuctworkPieces(docParam) {
            docParam = docParam || doc;
            var all = [];
            var names = getDuctworkPieceLayerNames();
            for (var i = 0; i < names.length; i++) {
                try {
                    var paths = getPathsOnLayerAll(names[i]);
                    for (var j = 0; j < paths.length; j++) all.push(paths[j]);
                } catch (e) {
                    // skip
                }
            }
            return all;
        }

        // --- UTILS ---
        function dist2(a, b) { var dx = a[0] - b[0]; var dy = a[1] - b[1]; return dx * dx + dy * dy; }
        function dist(a, b) { return Math.sqrt(dist2(a, b)); }
        function dot(a, b) { return a[0] * b[0] + a[1] * b[1]; }
        function almostEqualPoints(a, b, eps) { eps = eps || 0.01; return Math.abs(a[0] - b[0]) < eps && Math.abs(a[1] - b[1]) < eps; }

        function vectorLength(v) {
            if (!v) return 0;
            return Math.sqrt(v[0] * v[0] + v[1] * v[1]);
        }

        function normalizeVector(v) {
            if (!v) return null;
            var len = vectorLength(v);
            if (len < 0.0001) return null;
            return [v[0] / len, v[1] / len];
        }

        function getEndpointDirectionVector(pathItem, endpointIndex) {
            if (!pathItem) return null;
            var pts = null;
            try { pts = pathItem.pathPoints; } catch (e) { pts = null; }
            if (!pts || pts.length < 2) return null;

            var count = pts.length;
            if (endpointIndex <= 0) {
                var a0 = pts[0].anchor;
                var a1 = pts[1].anchor;
                return normalizeVector([a1[0] - a0[0], a1[1] - a0[1]]);
            }
            if (endpointIndex >= count - 1) {
                var aPrev = pts[count - 2].anchor;
                var aLast = pts[count - 1].anchor;
                return normalizeVector([aLast[0] - aPrev[0], aLast[1] - aPrev[1]]);
            }
            var before = pts[endpointIndex - 1].anchor;
            var after = pts[endpointIndex + 1].anchor;
            var anchor = pts[endpointIndex].anchor;
            var vecBefore = normalizeVector([anchor[0] - before[0], anchor[1] - before[1]]);
            var vecAfter = normalizeVector([after[0] - anchor[0], after[1] - anchor[1]]);
            if (!vecBefore && !vecAfter) return null;
            if (vecBefore && !vecAfter) return vecBefore;
            if (!vecBefore && vecAfter) return vecAfter;
            // If both exist, choose the longer supporting segment (favoring smoother transitions)
            var beforeLen = dist(anchor, before);
            var afterLen = dist(anchor, after);
            return beforeLen >= afterLen ? vecBefore : vecAfter;
        }

        function getPathTotalLength(pathItem) {
            if (!pathItem) return 0;
            var pts = null;
            try { pts = pathItem.pathPoints; } catch (e) { pts = null; }
            if (!pts || pts.length < 2) return 0;
            var total = 0;
            for (var i = 1; i < pts.length; i++) {
                total += dist(pts[i - 1].anchor, pts[i].anchor);
            }
            return total;
        }

        function resolveSegmentIndexForEndpoint(pathItem, endpointIndex, referenceDirection) {
            if (!pathItem) return 0;
            var pts = null;
            try { pts = pathItem.pathPoints; } catch (e) { pts = null; }
            if (!pts || pts.length < 2) return 0;

            var lastIndex = pts.length - 1;
            if (endpointIndex <= 0) return 0;
            if (endpointIndex >= lastIndex) return Math.max(lastIndex - 1, 0);

            var segBefore = normalizeVector([
                pts[endpointIndex].anchor[0] - pts[endpointIndex - 1].anchor[0],
                pts[endpointIndex].anchor[1] - pts[endpointIndex - 1].anchor[1]
            ]);
            var segAfter = normalizeVector([
                pts[endpointIndex + 1].anchor[0] - pts[endpointIndex].anchor[0],
                pts[endpointIndex + 1].anchor[1] - pts[endpointIndex].anchor[1]
            ]);

            if (!segBefore && !segAfter) return 0;
            if (!segBefore) return endpointIndex;
            if (!segAfter) return endpointIndex - 1;

            if (!referenceDirection) {
                var beforeLen = dist(pts[endpointIndex].anchor, pts[endpointIndex - 1].anchor);
                var afterLen = dist(pts[endpointIndex + 1].anchor, pts[endpointIndex].anchor);
                return beforeLen >= afterLen ? (endpointIndex - 1) : endpointIndex;
            }

            var beforeAlignment = Math.abs(dot(segBefore, referenceDirection));
            var afterAlignment = Math.abs(dot(segAfter, referenceDirection));
            return beforeAlignment <= afterAlignment ? (endpointIndex - 1) : endpointIndex;
        }

        function isAngleNearMultipleOf45Global(angleDeg, tolerance) {
            if (typeof angleDeg !== "number") return false;
            var tol = (typeof tolerance === "number") ? Math.abs(tolerance) : 0.5;
            var normalized = ((angleDeg % 180) + 180) % 180;
            var remainder = normalized % 45;
            return remainder <= tol || 45 - remainder <= tol;
        }

        function isAngleNearMultipleOf45(angleDeg, tolerance) {
            return isAngleNearMultipleOf45Global(angleDeg, tolerance);
        }

        function computeSegmentIntersection(p1, p2, p3, p4, epsilon) {
            epsilon = epsilon || 1e-6;
            var x1 = p1[0], y1 = p1[1];
            var x2 = p2[0], y2 = p2[1];
            var x3 = p3[0], y3 = p3[1];
            var x4 = p4[0], y4 = p4[1];

            var denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
            if (Math.abs(denom) < epsilon) return null;

            var t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            var u = ((x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2)) / denom;

            if (t < -epsilon || t > 1 + epsilon || u < -epsilon || u > 1 + epsilon) return null;

            var ix = x1 + t * (x2 - x1);
            var iy = y1 + t * (y2 - y1);
            return {
                point: { x: ix, y: iy },
                t1: t,
                t2: u
            };
        }

        function detectBlueRightAngleBranches(paths) {
            var results = [];
            if (!paths || !paths.length) return results;

            var tolerance = Math.max(CONNECTION_DIST, 2);
            var tol2 = tolerance * tolerance;
            var angleThreshold = Math.cos(85 * Math.PI / 180); // within ~5 degrees of 90

            for (var i = 0; i < paths.length; i++) {
                clearBlueRightAngleBranch(paths[i]);
            }

            for (var a = 0; a < paths.length; a++) {
                var pathA = paths[a];
                if (!pathA) continue;
                if (pathA.closed) continue;
                var layerA = "";
                try { layerA = pathA.layer ? pathA.layer.name : ""; } catch (eLayerA) { layerA = ""; }
                if (!isDuctworkColorLayer(layerA)) continue; // Must be a ductwork color layer
                var ptsA = null;
                try { ptsA = pathA.pathPoints; } catch (ePtsA) { ptsA = null; }
                if (!ptsA || ptsA.length < 2) continue;

                for (var b = a + 1; b < paths.length; b++) {
                    var pathB = paths[b];
                    if (!pathB || pathB === pathA) continue;
                    if (pathB.closed) continue;
                    var layerB = "";
                    try { layerB = pathB.layer ? pathB.layer.name : ""; } catch (eLayerB) { layerB = ""; }
                    if (layerB !== layerA) continue; // SAME-LAYER ONLY: must match pathA's layer
                    var ptsB = null;
                    try { ptsB = pathB.pathPoints; } catch (ePtsB) { ptsB = null; }
                    if (!ptsB || ptsB.length < 2) continue;

                    var endpointsA = [
                        { index: 0, anchor: ptsA[0].anchor },
                        { index: ptsA.length - 1, anchor: ptsA[ptsA.length - 1].anchor }
                    ];
                    var endpointsB = [
                        { index: 0, anchor: ptsB[0].anchor },
                        { index: ptsB.length - 1, anchor: ptsB[ptsB.length - 1].anchor }
                    ];

                    for (var ea = 0; ea < endpointsA.length; ea++) {
                        var endA = endpointsA[ea];
                        for (var eb = 0; eb < endpointsB.length; eb++) {
                            var endB = endpointsB[eb];
                            if (dist2(endA.anchor, endB.anchor) > tol2) continue;

                            var dirA = getEndpointDirectionVector(pathA, endA.index);
                            var dirB = getEndpointDirectionVector(pathB, endB.index);
                            if (!dirA || !dirB) continue;
                            var alignment = Math.abs(dot(dirA, dirB));
                            if (alignment > angleThreshold) continue;

                            var lengthA = getPathTotalLength(pathA);
                            var lengthB = getPathTotalLength(pathB);

                            var branchPath = lengthA <= lengthB ? pathA : pathB;
                            var branchEndpoint = lengthA <= lengthB ? endA : endB;
                            var mainPath = lengthA <= lengthB ? pathB : pathA;
                            var mainEndpoint = lengthA <= lengthB ? endB : endA;
                            var mainDirection = lengthA <= lengthB ? dirB : dirA;

                            var mainSegmentIndex = resolveSegmentIndexForEndpoint(mainPath, mainEndpoint.index, mainDirection);
                            var connectionPt = [
                                (branchEndpoint.anchor[0] + mainEndpoint.anchor[0]) / 2,
                                (branchEndpoint.anchor[1] + mainEndpoint.anchor[1]) / 2
                            ];

                            markBlueRightAngleBranch(branchPath);
                            branchPath.__blueRightAngleBranchData = {
                                branchAnchorIndex: branchEndpoint.index,
                                mainPath: mainPath,
                                mainSegmentIndex: mainSegmentIndex,
                                connectionPoint: { x: connectionPt[0], y: connectionPt[1] }
                            };

                            results.push({
                                branchPath: branchPath,
                                branchAnchorIndex: branchEndpoint.index,
                                mainPath: mainPath,
                                mainSegmentIndex: mainSegmentIndex,
                                connectionPoint: { x: connectionPt[0], y: connectionPt[1] }
                            });
                        }
                    }

                    for (var sa = 0; sa < ptsA.length - 1; sa++) {
                        var aStart = [ptsA[sa].anchor[0], ptsA[sa].anchor[1]];
                        var aEnd = [ptsA[sa + 1].anchor[0], ptsA[sa + 1].anchor[1]];
                        var segDirA = normalizeVector([aEnd[0] - aStart[0], aEnd[1] - aStart[1]]);
                        if (!segDirA) continue;
                        for (var sb = 0; sb < ptsB.length - 1; sb++) {
                            var bStart = [ptsB[sb].anchor[0], ptsB[sb].anchor[1]];
                            var bEnd = [ptsB[sb + 1].anchor[0], ptsB[sb + 1].anchor[1]];
                            var segDirB = normalizeVector([bEnd[0] - bStart[0], bEnd[1] - bStart[1]]);
                            if (!segDirB) continue;
                            var intersection = computeSegmentIntersection(aStart, aEnd, bStart, bEnd, 1e-6);
                            if (!intersection) continue;
                            if (intersection.t1 <= 0.01 || intersection.t1 >= 0.99 || intersection.t2 <= 0.01 || intersection.t2 >= 0.99) continue;
                            var alignmentSeg = Math.abs(dot(segDirA, segDirB));
                            if (alignmentSeg > angleThreshold) continue;

                            var lengthA = getPathTotalLength(pathA);
                            var lengthB = getPathTotalLength(pathB);

                            var branchPathSeg = lengthA <= lengthB ? pathA : pathB;
                            var mainPathSeg = lengthA <= lengthB ? pathB : pathA;
                            var branchSegIndex = lengthA <= lengthB ? sa : sb;
                            var mainSegIndex = lengthA <= lengthB ? sb : sa;

                            var duplicateExisting = false;
                            for (var rIdx = 0; rIdx < results.length; rIdx++) {
                                var existingEntry = results[rIdx];
                                if (!existingEntry || existingEntry.branchPath !== branchPathSeg) continue;
                                var existingPoint = existingEntry.connectionPoint;
                                if (!existingPoint) continue;
                                var dxExist = existingPoint.x - intersection.point.x;
                                var dyExist = existingPoint.y - intersection.point.y;
                                if (dxExist * dxExist + dyExist * dyExist <= 1) {
                                    duplicateExisting = true;
                                    break;
                                }
                            }
                            if (duplicateExisting) continue;

                            markBlueRightAngleBranch(branchPathSeg);
                            results.push({
                                branchPath: branchPathSeg,
                                branchAnchorIndex: -1,
                                branchSegmentIndex: branchSegIndex,
                                mainPath: mainPathSeg,
                                mainSegmentIndex: mainSegIndex,
                                connectionPoint: { x: intersection.point.x, y: intersection.point.y },
                                multiSide: true
                            });
                        }
                    }
                }
            }

            return results;
        }

        function trimString(str) {
            if (str === undefined || str === null) return '';
            return ('' + str).replace(/^\s+|\s+$/g, '');
        }

        function arrayIndexOf(list, value) {
            if (!list || typeof list.length !== "number") return -1;
            for (var i = 0; i < list.length; i++) {
                if (list[i] === value) return i;
            }
            return -1;
        }

        function normalizeAngle(angleDeg) {
            if (!isFinite(angleDeg)) return 0;
            var normalized = angleDeg % 360;
            if (normalized < 0) normalized += 360;
            return Math.round(normalized * 1000) / 1000;
        }

        function getNoteString(pathItem) {
            try {
                if (!pathItem) return '';
                var note = pathItem.note;
                return (typeof note === 'string') ? note : '';
            } catch (e) {
                return '';
            }
        }

        function readNoteTokens(pathItem) {
            var note = getNoteString(pathItem);
            if (!note) return [];
            var raw = note.split('|');
            var tokens = [];
            for (var i = 0; i < raw.length; i++) {
                var token = trimString(raw[i]);
                if (token.length > 0) tokens.push(token);
            }
            return tokens;
        }

        function writeNoteTokens(pathItem, tokens) {
            if (!pathItem) return;
            var result = [];
            if (tokens && tokens.length) {
                for (var i = 0; i < tokens.length; i++) {
                    var token = trimString(tokens[i]);
                    if (token.length > 0) result.push(token);
                }
            }
            try {
                pathItem.note = result.join('|');
            } catch (e) { }
        }

        function getBranchStartWidthFromPath(pathItem) {
            if (!pathItem) return null;
            var tokens = readNoteTokens(pathItem);
            for (var i = 0; i < tokens.length; i++) {
                var token = tokens[i];
                if (token && token.indexOf(BRANCH_START_PREFIX) === 0) {
                    var raw = token.substring(BRANCH_START_PREFIX.length);
                    var val = parseFloat(raw);
                    if (isFinite(val) && val > 0) return val;
                }
            }
            return null;
        }

        function setBranchStartWidthOnPath(pathItem, fullWidth) {
            if (!pathItem) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i] && tokens[i].indexOf(BRANCH_START_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            if (typeof fullWidth === "number" && isFinite(fullWidth) && fullWidth > 0) {
                filtered.push(BRANCH_START_PREFIX + fullWidth.toFixed(4));
            }
            writeNoteTokens(pathItem, filtered);
        }

        function clearBranchStartWidthOnPath(pathItem) {
            setBranchStartWidthOnPath(pathItem, null);
        }

        function generateCenterlineId() {
            var stamp = (new Date().getTime()).toString(36);
            var rand = Math.floor(Math.random() * 1e6).toString(36);
            return "CL" + stamp + "_" + rand;
        }

        function ensureEmoryCenterlineMetadata(pathItem) {
            if (!pathItem) return null;
            var tokens = readNoteTokens(pathItem);
            var hasTag = false;
            var existingId = null;
            for (var i = 0; i < tokens.length; i++) {
                var token = tokens[i];
                if (token === CENTERLINE_NOTE_TAG) {
                    hasTag = true;
                } else if (token && token.indexOf(CENTERLINE_ID_PREFIX) === 0) {
                    existingId = token.substring(CENTERLINE_ID_PREFIX.length);
                }
            }
            if (!hasTag) tokens.push(CENTERLINE_NOTE_TAG);
            if (!existingId) {
                existingId = generateCenterlineId();
                tokens.push(CENTERLINE_ID_PREFIX + existingId);
            }
            writeNoteTokens(pathItem, tokens);
            return existingId;
        }

        function ensureNoteTag(pathItem, tag) {
            if (!pathItem || !tag) return;
            var tokens = readNoteTokens(pathItem);
            if (arrayIndexOf(tokens, tag) === -1) {
                tokens.push(tag);
                writeNoteTokens(pathItem, tokens);
            }
        }

        function removeNoteTag(pathItem, tag) {
            if (!pathItem || !tag) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i] !== tag) filtered.push(tokens[i]);
            }
            writeNoteTokens(pathItem, filtered);
        }

        function hasNoteTag(pathItem, tag) {
            if (!pathItem || !tag) return false;
            var tokens = readNoteTokens(pathItem);
            return arrayIndexOf(tokens, tag) !== -1;
        }

        function markBlueRightAngleBranch(pathItem) {
            ensureNoteTag(pathItem, BLUE_RIGHTANGLE_BRANCH_TAG);
        }

        function clearBlueRightAngleBranch(pathItem) {
            removeNoteTag(pathItem, BLUE_RIGHTANGLE_BRANCH_TAG);
            try { delete pathItem.__blueRightAngleBranchData; } catch (e) { }
        }

        function hasBlueRightAngleBranch(pathItem) {
            return hasNoteTag(pathItem, BLUE_RIGHTANGLE_BRANCH_TAG);
        }

        function getEmoryCenterlineId(pathItem) {
            if (!pathItem) return null;
            var tokens = readNoteTokens(pathItem);
            for (var i = 0; i < tokens.length; i++) {
                var token = tokens[i];
                if (token && token.indexOf(CENTERLINE_ID_PREFIX) === 0) {
                    return token.substring(CENTERLINE_ID_PREFIX.length);
                }
            }
            return null;
        }

        function clearEmoryCenterlineMetadata(pathItem) {
            if (!pathItem) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                var token = tokens[i];
                if (token === CENTERLINE_NOTE_TAG) continue;
                if (token && token.indexOf(CENTERLINE_ID_PREFIX) === 0) continue;
                if (token && token.indexOf(ORIGINAL_LAYER_PREFIX) === 0) continue;
                filtered.push(token);
            }
            writeNoteTokens(pathItem, filtered);
        }

        function getOriginalLayerName(pathItem) {
            if (!pathItem) return null;
            var tokens = readNoteTokens(pathItem);
            for (var i = 0; i < tokens.length; i++) {
                var token = tokens[i];
                if (token && token.indexOf(ORIGINAL_LAYER_PREFIX) === 0) {
                    var value = token.substring(ORIGINAL_LAYER_PREFIX.length);
                    if (value && value.length > 0) return value;
                }
            }
            return null;
        }

        function setOriginalLayerName(pathItem, layerName) {
            if (!pathItem || !layerName) return;
            var tokens = readNoteTokens(pathItem);
            var tag = ORIGINAL_LAYER_PREFIX + layerName;
            var replaced = false;
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i] && tokens[i].indexOf(ORIGINAL_LAYER_PREFIX) === 0) {
                    tokens[i] = tag;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) tokens.push(tag);
            writeNoteTokens(pathItem, tokens);
        }

        function clearOriginalLayerName(pathItem) {
            if (!pathItem) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i] && tokens[i].indexOf(ORIGINAL_LAYER_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(pathItem, filtered);
        }

        function setRotationOverride(pathItem, angleDeg) {
            if (!pathItem) return;
            // Store the angle directly without computing base rotation
            var tokens = readNoteTokens(pathItem);
            var tag = ROT_OVERRIDE_PREFIX + normalizeAngle(angleDeg);
            var replaced = false;
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(ROT_OVERRIDE_PREFIX) === 0) {
                    tokens[i] = tag;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) tokens.push(tag);
            writeNoteTokens(pathItem, tokens);
        }

        function clearRotationOverride(pathItem) {
            if (!pathItem) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                // Clear both MD:ROT= and MD:BASE_ROT= tags
                if (tokens[i].toUpperCase().indexOf(ROT_OVERRIDE_PREFIX) === 0) continue;
                if (tokens[i].toUpperCase().indexOf(ROT_BASE_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(pathItem, filtered);
        }

        function getRotationOverride(pathItem) {
            var tokens = readNoteTokens(pathItem);
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(ROT_OVERRIDE_PREFIX) === 0) {
                    var raw = tokens[i].substring(ROT_OVERRIDE_PREFIX.length);
                    var val = parseFloat(raw);
                    if (!isNaN(val)) {
                        // Return the angle directly - no base rotation system
                        return normalizeAngle(val);
                    }
                }
            }
            // Only return a rotation override if one was explicitly set (MD:ROT= tag exists)
            // Paths without explicit override will orthogonalize to standard 0/90/180/270 grid
            return null;
        }

        function loadSettingsFromSelection(selectionItems) {
            if (!selectionItems || selectionItems.length === 0) {
                return {
                    width: 20, // default to 20 when no stored settings
                    taper: 30,
                    angle: 0,
                    noTaper: false,
                    color: "",
                    wireToRegister: true, // default to connecting duct to register via wire
                    mode: "emory"
                };
            }

            for (var i = 0; i < selectionItems.length; i++) {
                var item = selectionItems[i];
                if (!item) continue;
                var grp = null;
                try {
                    if (item.typename === "GroupItem") {
                        grp = item;
                    } else {
                        var parent = item.parent;
                        var safetyCounter = 0;
                        while (parent && safetyCounter < 50) {
                            try {
                                if (parent.typename === "GroupItem") {
                                    grp = parent;
                                    break;
                                }
                                var newParent = parent.parent;
                                if (!newParent || newParent === parent) break;
                                parent = newParent;
                            } catch (e) {
                                // Parent may be invalid after copy/paste
                                break;
                            }
                            safetyCounter++;
                        }
                    }
                    if (grp) {
                        var loaded = loadSettingsFromGroup(grp);
                        if (loaded) return loaded;
                    }
                } catch (e) {
                    // Item may be invalid, skip it
                    continue;
                }
            }

            return {
                width: 20, // default to 20 when no stored settings
                taper: 30,
                angle: 0,
                noTaper: false,
                color: "",
                wireToRegister: true, // default to connecting duct to register via wire
                mode: "emory"
            };
        }

        function isEmoryCenterline(pathItem) {
            // Removed: Emory functionality stripped - always returns false
            return false;
        }

        function isEmoryGeneratedAuxPath(pathItem) {
            // Removed: Emory functionality stripped - always returns false
            return false;
        }

        function collectSelectionContext(selectionItems) {
            var context = {
                normalLayers: [],
                emoryLayers: [],
                hasEmoryGroups: false,
                hasAnyEmorySelection: false,
                hasAnyNormalSelection: false,
                canHideCenterlines: false,
                canShowCenterlines: false,
                storedSettings: loadSettingsFromSelection(selectionItems),
                defaultMode: "normal", // Will be auto-detected based on selection below
                selectionColors: [],
                hasEmoryStyles: false,
                hasNormalStyles: false,
                hasDoubleDuctwork: false,
                defaultRotation: null,
                selectedUnitItems: [],
                selectedBranchCenterlines: [],
                branchCenterlineIds: [],
                branchWidthValues: []
            };

            var seenNormal = {};
            var seenEmory = {};
            var seenColors = {};
            var centerlineVisible = false;
            var centerlineHidden = false;
            var branchTargetMap = {};

            function trackLayer(layerName, isEmory) {
                if (!layerName) return;
                if (isEmory) {
                    if (!seenEmory[layerName]) {
                        context.emoryLayers.push(layerName);
                        seenEmory[layerName] = true;
                    }
                    context.hasAnyEmorySelection = true;
                } else {
                    if (!seenNormal[layerName]) {
                        context.normalLayers.push(layerName);
                        seenNormal[layerName] = true;
                    }
                    context.hasAnyNormalSelection = true;
                }
            }

            function registerBranchPath(pathItem) {
                if (!pathItem) return;
                try { if (pathItem.closed) return; } catch (eClosedPath) { }
                var layerName = "";
                try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (eLayerName) { }
                // Allow all ductwork colors (Blue, Green, Orange, Purple) for branch width scaling
                // This enables rescaling of both branches and main trunks
                if (!isDuctworkLineLayer(layerName) && !isEmoryLineLayer(layerName)) return;
                var centerlineId = ensureEmoryCenterlineMetadata(pathItem);
                if (!centerlineId || branchTargetMap[centerlineId]) return;
                branchTargetMap[centerlineId] = true;
                context.selectedBranchCenterlines.push({
                    path: pathItem,
                    centerlineId: centerlineId
                });
                context.branchCenterlineIds.push(centerlineId);
                var storedWidth = getBranchStartWidthFromPath(pathItem);
                if (typeof storedWidth === "number" && isFinite(storedWidth) && storedWidth > 0) {
                    context.branchWidthValues.push(storedWidth);
                }
            }

            function registerBranchPathsByIds(idList) {
                if (!idList || !idList.length) return;
                var foundPaths = findEmoryCenterlinesByIds(idList);
                for (var i = 0; i < foundPaths.length; i++) {
                    registerBranchPath(foundPaths[i]);
                }
            }

            function unitAlreadyCaptured(item) {
                if (!item) return true;
                for (var u = 0; u < context.selectedUnitItems.length; u++) {
                    if (context.selectedUnitItems[u].item === item) return true;
                }
                return false;
            }

            function recordUnitItem(item) {
                if (!item || unitAlreadyCaptured(item)) return;
                var layerName = "";
                try { layerName = item.layer ? item.layer.name : ""; } catch (eLayerCheck) { }
                if (!layerName || layerName.toLowerCase() !== "units") return;
                var originalMatrix = null;
                try { originalMatrix = cloneMatrixValues(item.matrix); } catch (eMatrix) { originalMatrix = null; }
                context.selectedUnitItems.push({
                    item: item,
                    originalMatrix: originalMatrix
                });
            }

            function collectUnitsFromItem(item) {
                if (!item) return;
                var typeName = "";
                try { typeName = item.typename; } catch (eType) { typeName = ""; }
                if (typeName === "PlacedItem") {
                    recordUnitItem(item);
                } else if (typeName === "GroupItem" && item.pageItems) {
                    for (var gi = 0; gi < item.pageItems.length; gi++) {
                        collectUnitsFromItem(item.pageItems[gi]);
                    }
                } else if (typeName === "CompoundPathItem" && item.pathItems) {
                    for (var ci = 0; ci < item.pathItems.length; ci++) {
                        collectUnitsFromItem(item.pathItems[ci]);
                    }
                }
            }

            // Check for Emory graphic styles applied to paths
            function hasEmoryStyleApplied(item) {
                if (!item) return false;
                try {
                    // Check if the item has any appearance attributes
                    if (!item.unapplyAll) return false;

                    // Try to get the applied graphic style name (if any)
                    // In Illustrator ExtendScript, we can't directly get the style name,
                    // but we can check for closed paths on ductwork layers (Double Ductwork indicator)
                    var layerName = "";
                    try { layerName = item.layer ? item.layer.name : ""; } catch (e) { }

                    if (isDuctworkLineLayer(layerName)) {
                        // Check if it's a closed path (Double Ductwork rectangle/connector)
                        try {
                            if (item.typename === "PathItem" && item.closed) {
                                return true;
                            }
                        } catch (e) { }
                    }
                } catch (e) { }
                return false;
            }

            forEachPathInItems(selectionItems, function (pathItem) {
                if (!pathItem) return;
                var layerName = "";
                try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (eLayer) { }

                // Check for Double Ductwork (closed paths on ductwork layers = Emory mode)
                if (isDuctworkLineLayer(layerName)) {
                    try {
                        if (pathItem.typename === "PathItem" && pathItem.closed) {
                            context.hasDoubleDuctwork = true;
                            context.hasEmoryStyles = true;
                        }
                    } catch (e) { }
                }

                var isCenterline = isEmoryCenterline(pathItem);
                var isEmoryLayer = isEmoryLineLayer(layerName);
                if (isCenterline) {
                    var trackingName = layerName;
                    if (!isEmoryLayer) {
                        var colorFromLayer = getColorNameForLayer(layerName);
                        trackingName = getEmoryLayerNameFromColor(colorFromLayer) || layerName;
                    }
                    trackLayer(trackingName, true);
                    if (pathItem.hidden) centerlineHidden = true;
                    else centerlineVisible = true;
                    registerBranchPath(pathItem);
                } else if (isEmoryLayer) {
                    trackLayer(layerName, true);
                } else if (isDuctworkLineLayer(layerName)) {
                    trackLayer(layerName, false);
                    // Check if it has Emory style applied
                    if (hasEmoryStyleApplied(pathItem)) {
                        context.hasEmoryStyles = true;
                    } else {
                        context.hasNormalStyles = true;
                    }
                    registerBranchPath(pathItem);
                }
                var colorName = getColorNameForLayer(layerName);
                if (colorName && !seenColors[colorName]) {
                    seenColors[colorName] = true;
                    context.selectionColors.push(colorName);
                }
            });

            for (var i = 0; i < selectionItems.length; i++) {
                var item = selectionItems[i];
                if (item && item.typename === "GroupItem") {
                    var groupSettings = loadSettingsFromGroup(item);
                    if (groupSettings) {
                        context.hasEmoryGroups = true;
                        // If we found Emory group settings, default to Emory mode
                        if (groupSettings.mode === "emory") {
                            context.hasEmoryStyles = true;
                        }
                        if (groupSettings.centerlineIds && groupSettings.centerlineIds.length) {
                            registerBranchPathsByIds(groupSettings.centerlineIds);
                        }
                    }
                }
            }

            for (var unitIdx = 0; unitIdx < selectionItems.length; unitIdx++) {
                collectUnitsFromItem(selectionItems[unitIdx]);
            }

            context.canHideCenterlines = centerlineVisible;
            context.canShowCenterlines = centerlineHidden;

            // Auto-detect mode based on what's in the selection:
            // 1. If Double Ductwork present -> Emory mode
            // 2. If Emory groups present -> Emory mode
            // 3. If only Emory styles and no normal styles -> Emory mode
            // 4. If only normal selection and no emory -> Normal mode
            // 5. Otherwise -> Emory mode (default)
            if (context.hasDoubleDuctwork || context.hasEmoryGroups ||
                (context.hasAnyEmorySelection && !context.hasAnyNormalSelection) ||
                (context.hasEmoryStyles && !context.hasNormalStyles)) {
                context.defaultMode = "emory";
            } else if (context.hasAnyNormalSelection && !context.hasAnyEmorySelection) {
                context.defaultMode = "normal";
            } else {
                context.defaultMode = "emory";
            }

            context.multipleEmoryLayers = context.emoryLayers.length > 1;
            context.singleEmoryLayer = context.emoryLayers.length === 1 ? context.emoryLayers[0] : null;
            context.multipleNormalLayers = context.normalLayers.length > 1;
            context.singleNormalLayer = context.normalLayers.length === 1 ? context.normalLayers[0] : null;
            context.multipleColorLayers = context.selectionColors.length > 1;

            // Detect existing rotation override from selection
            forEachPathInItems(selectionItems, function (pathItem) {
                if (!pathItem) return;
                if (context.defaultRotation !== null) return; // Already found one
                var layerName = "";
                try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (e) { }
                if (layerName && isDuctworkLineLayer(layerName)) {
                    var existingRot = getRotationOverride(pathItem);
                    if (existingRot !== null && existingRot !== undefined) {
                        context.defaultRotation = existingRot;
                    }
                }
            });

            return context;
        }

        function showUnifiedDuctworkDialog(context, currentSettings) {
            // Simplified dialog for normal mode ductwork processing
            // Emory mode has been removed

            // Get default rotation override from context
            var defaultRotation = (context && context.defaultRotation !== undefined) ? context.defaultRotation : null;

            var result = {
                action: "cancel",
                mode: "normal",
                settings: null,
                rotationOverride: null,
                skipOrthoState: null,
                skipFinalRegisterSegment: SKIP_FINAL_REGISTER_ORTHO
            };

            // Check if selection has MD:NO_ORTHO note
            var skipOrthoState = checkSkipOrthoState(originalSelectionItems);

            if (typeof Window === "undefined") {
                // Fallback: simple confirm
                if (!confirm("Process ductwork?")) return result;
                result.action = "process";
                result.mode = "normal";
                result.skipOrthoState = {
                    checkboxValue: skipOrthoState.hasNote,
                    initialValue: skipOrthoState.hasNote,
                    changed: false
                };
                return result;
            }

            var dlg = createDarkDialogWindow("Magic Ductwork");
            if (!dlg) return result;

            dlg.orientation = "column";
            dlg.alignChildren = ["fill", "top"];
            dlg.spacing = 12;
            dlg.margins = 16;

            // Skip orthogonalization checkbox
            var skipOrthoCheckbox = dlg.add("checkbox", undefined, "Skip orthogonalization (MD:NO_ORTHO)");
            skipOrthoCheckbox.value = skipOrthoState.hasNote;
            setStaticTextColor(skipOrthoCheckbox);

            // Show warning if mixed selection
            var skipOrthoWarning = null;
            if (skipOrthoState.mixed) {
                skipOrthoWarning = dlg.add("statictext", undefined, "Mixed: Some paths skip ortho, others don't. Checking/unchecking will apply to all.", { multiline: true });
                skipOrthoWarning.alignment = "fill";
                setStaticTextColor(skipOrthoWarning, [1.0, 0.65, 0.2]);
            }

            // Separator
            var sep1 = dlg.add("panel");
            sep1.alignment = "fill";

            // Rotation Override Section
            var rotationTitle = dlg.add("statictext", undefined, "Rotation Override (optional):");
            rotationTitle.alignment = "fill";
            setStaticTextColor(rotationTitle);

            var rotationGroup = dlg.add("group");
            rotationGroup.alignChildren = ["left", "center"];

            var rotationLabel = rotationGroup.add("statictext", undefined, "Rotation (degrees):");
            rotationLabel.characters = 18;
            setStaticTextColor(rotationLabel);

            var rotationInput = rotationGroup.add("edittext", undefined, defaultRotation !== null ? String(defaultRotation) : "");
            rotationInput.characters = 10;
            rotationInput.helpTip = "Leave blank to use default rotation. Enter a number to override.";

            // Final segment orthogonalization option
            var skipFinalRegisterCheckbox = dlg.add("checkbox", undefined, "Skip orthogonalizing final segment into registers");
            skipFinalRegisterCheckbox.value = SKIP_FINAL_REGISTER_ORTHO;
            skipFinalRegisterCheckbox.helpTip = "Leave the last run into a register at its natural angle instead of forcing 90°/180°.";
            setStaticTextColor(skipFinalRegisterCheckbox);

            // Separator
            var sep2 = dlg.add("panel");
            sep2.alignment = "fill";

            // Buttons
            var btnRow = dlg.add("group");
            btnRow.orientation = "row";
            btnRow.alignChildren = ["center", "center"];
            btnRow.spacing = 10;

            var processBtn = btnRow.add("button", undefined, "Process", { name: "ok" });
            var cancelBtn = btnRow.add("button", undefined, "Cancel", { name: "cancel" });
            processBtn.preferredSize = [100, 30];
            cancelBtn.preferredSize = [100, 30];

            // Button handlers
            processBtn.onClick = function () {
                result.action = "process";
                result.mode = "normal";

                // Handle rotation override
                var rotText = trimString(rotationInput.text);
                if (rotText) {
                    var rotVal = parseFloat(rotText);
                    if (isFinite(rotVal)) {
                        result.rotationOverride = rotVal;
                    }
                }

                // Capture skipOrtho checkbox state
                result.skipOrthoState = {
                    checkboxValue: !!skipOrthoCheckbox.value,
                    initialValue: skipOrthoState.hasNote,
                    changed: (!!skipOrthoCheckbox.value) !== skipOrthoState.hasNote
                };
                result.skipFinalRegisterSegment = !!skipFinalRegisterCheckbox.value;

                dlg.close(1);
            };

            cancelBtn.onClick = function () {
                result.action = "cancel";
                dlg.close(0);
            };

            dlg.defaultElement = processBtn;
            dlg.cancelElement = cancelBtn;

            dlg.show();
            return result;
        }

        // Legacy function for backward compatibility (redirects to unified dialog)
        function showModeSelectionDialog(context) {
            var unifiedResult = showUnifiedDuctworkDialog(context, null);
            // Convert unified result to legacy format
            return {
                action: unifiedResult.action,
                mode: unifiedResult.mode
            };
        }


        // Removed: showEmorySettingsDialog function (145 lines) - Emory mode removed


        var EmoryGeometry = (function () {
            function deleteRectanglesFromGroup(group) {
                if (!group) return;
                var toRemove = [];
                function traverse(grp) {
                    if (!grp || !grp.pageItems) return;
                    for (var i = 0; i < grp.pageItems.length; i++) {
                        var item = grp.pageItems[i];
                        if (!item) continue;
                        if (item.typename === "PathItem" && item.closed) {
                            toRemove.push(item);
                        } else if (item.typename === "CompoundPathItem") {
                            toRemove.push(item);
                        } else if (item.typename === "GroupItem") {
                            traverse(item);
                        }
                    }
                }
                traverse(group);
                for (var r = 0; r < toRemove.length; r++) {
                    try { toRemove[r].remove(); } catch (e) { }
                }
            }

            function extractCenterlinesFromGroup(group) {
                var results = [];
                var settings = loadSettingsFromGroup(group);
                if (settings && settings.centerlineIds && settings.centerlineIds.length) {
                    results = findEmoryCenterlinesByIds(settings.centerlineIds);
                }
                if (results.length > 0) return results;

                function traverse(grp) {
                    if (!grp || !grp.pageItems) return;
                    for (var i = 0; i < grp.pageItems.length; i++) {
                        var item = grp.pageItems[i];
                        if (!item) continue;
                        if (item.typename === "PathItem" && !item.closed) {
                            results.push(item);
                        } else if (item.typename === "GroupItem") {
                            traverse(item);
                        }
                    }
                }
                traverse(group);
                return results;
            }

            function convertConnectionsToRectangleFormat(connectionData) {
                var result = [];
                if (!connectionData || !connectionData.endpointSegments) return result;
                var segmentConnections = connectionData.endpointSegments;
                for (var i = 0; i < segmentConnections.length; i++) {
                    var entry = segmentConnections[i];
                    if (!entry || !entry.endpoint || !entry.segment) continue;

                    var endPt = entry.endpoint.path.pathPoints[entry.endpoint.index];
                    result.push({
                        branchPath: entry.endpoint.path,
                        branchAnchorIndex: entry.endpoint.index,
                        mainPath: entry.segment.path,
                        mainSegmentIndex: entry.segment.index1,
                        connectionPoint: { x: endPt.anchor[0], y: endPt.anchor[1] }
                    });
                }
                return result;
            }

            function alignAppearance(newPath, sourcePath) {
                if (!newPath || !sourcePath) return;
                try {
                    newPath.stroked = sourcePath.stroked;
                    newPath.strokeColor = duplicateColor(sourcePath.strokeColor);
                    newPath.strokeWidth = sourcePath.strokeWidth;
                    newPath.strokeCap = sourcePath.strokeCap;
                    newPath.strokeJoin = sourcePath.strokeJoin;
                    newPath.strokeMiterLimit = sourcePath.strokeMiterLimit;
                    newPath.filled = sourcePath.filled;
                    if (sourcePath.filled && sourcePath.fillColor) {
                        newPath.fillColor = duplicateColor(sourcePath.fillColor);
                    } else {
                        newPath.filled = false;
                    }
                } catch (e) { }
            }

            function duplicateColor(color) {
                if (!color) return color;
                if (color.typename === "RGBColor") {
                    var dup = new RGBColor();
                    dup.red = color.red;
                    dup.green = color.green;
                    dup.blue = color.blue;
                    return dup;
                }
                if (color.typename === "CMYKColor") {
                    var dupCmyk = new CMYKColor();
                    dupCmyk.cyan = color.cyan;
                    dupCmyk.magenta = color.magenta;
                    dupCmyk.yellow = color.yellow;
                    dupCmyk.black = color.black;
                    return dupCmyk;
                }
                if (color.typename === "SpotColor") {
                    var dupSpot = new SpotColor();
                    dupSpot.spot = color.spot;
                    dupSpot.tint = color.tint;
                    dupSpot.color = duplicateColor(color.color);
                    return dupSpot;
                }
                if (color.typename === "GrayColor") {
                    var dupGray = new GrayColor();
                    dupGray.gray = color.gray;
                    return dupGray;
                }
                return color;
            }

            function toPoint(coords) {
                return { x: coords[0], y: coords[1] };
            }

            function clonePoint(point) {
                return { x: point.x, y: point.y };
            }

            function add(a, b) {
                return { x: a.x + b.x, y: a.y + b.y };
            }

            function subtract(a, b) {
                return { x: a.x - b.x, y: a.y - b.y };
            }

            function scale(point, scalar) {
                return { x: point.x * scalar, y: point.y * scalar };
            }

            function magnitude(point) {
                return Math.sqrt(point.x * point.x + point.y * point.y);
            }

            function normalize(vector) {
                var len = magnitude(vector);
                if (len <= 0.000001) return { x: 1, y: 0 };
                return { x: vector.x / len, y: vector.y / len };
            }

            function dot(a, b) {
                return a.x * b.x + a.y * b.y;
            }

            function clamp(value, min, max) {
                if (value < min) return min;
                if (value > max) return max;
                return value;
            }

            function lerpPoint(a, b, t) {
                return {
                    x: a.x + (b.x - a.x) * t,
                    y: a.y + (b.y - a.y) * t
                };
            }

            function makeSegment(start, end) {
                var startCopy = clonePoint(start);
                var endCopy = clonePoint(end);
                var vector = subtract(endCopy, startCopy);
                var length = magnitude(vector);
                if (length <= 0.000001) {
                    return {
                        start: startCopy,
                        end: endCopy,
                        length: 0,
                        direction: { x: 1, y: 0 },
                        normal: { x: 0, y: 1 }
                    };
                }
                var direction = scale(vector, 1 / length);
                var normal = { x: -direction.y, y: direction.x };
                return {
                    start: startCopy,
                    end: endCopy,
                    length: length,
                    direction: direction,
                    normal: normal
                };
            }

            function locateAlongPath(segments, cumulative, targetDistance, epsilon) {
                targetDistance = clamp(targetDistance, 0, cumulative[cumulative.length - 1]);

                if (segments.length === 0) {
                    return { point: null, normal: { x: 0, y: 1 }, direction: { x: 1, y: 0 } };
                }

                if (targetDistance <= epsilon) {
                    return {
                        point: clonePoint(segments[0].start),
                        normal: segments[0].normal,
                        direction: segments[0].direction
                    };
                }

                if (Math.abs(targetDistance - cumulative[cumulative.length - 1]) <= epsilon) {
                    var last = segments[segments.length - 1];
                    return {
                        point: clonePoint(last.end),
                        normal: last.normal,
                        direction: last.direction
                    };
                }

                for (var i = 0; i < segments.length; i++) {
                    var segStart = cumulative[i];
                    var segEnd = cumulative[i + 1];
                    if (targetDistance <= segEnd + epsilon) {
                        var segment = segments[i];
                        var ratio = 0;
                        var span = segEnd - segStart;
                        if (span > epsilon) {
                            ratio = clamp((targetDistance - segStart) / span, 0, 1);
                        }
                        return {
                            point: lerpPoint(segment.start, segment.end, ratio),
                            normal: segment.normal,
                            direction: segment.direction
                        };
                    }
                }

                var fallback = segments[segments.length - 1];
                return {
                    point: clonePoint(fallback.end),
                    normal: fallback.normal,
                    direction: fallback.direction
                };
            }

            function collectTailSegments(segments, cumulative, bodyLimit, epsilon) {
                var tail = [];
                var totalLength = cumulative[cumulative.length - 1];
                if (totalLength <= bodyLimit + epsilon) return tail;

                for (var i = segments.length - 1; i >= 0; i--) {
                    var segStart = cumulative[i];
                    var segEnd = cumulative[i + 1];
                    if (segStart >= bodyLimit - epsilon) {
                        tail.unshift({
                            segment: segments[i],
                            startDistance: Math.max(segStart, bodyLimit),
                            endDistance: segEnd
                        });
                    } else if (segEnd > bodyLimit + epsilon) {
                        tail.unshift({
                            segment: segments[i],
                            startDistance: bodyLimit,
                            endDistance: segEnd
                        });
                        break;
                    } else {
                        break;
                    }
                }
                return tail;
            }

            function isDiagonalAngle(angleDeg, orthogonalAngle) {
                var base = orthogonalAngle || 0;
                var adjusted = ((angleDeg - base) % 180 + 180) % 180;
                return adjusted > 30 && adjusted < 60 || adjusted > 120 && adjusted < 150;
            }

            function isAngleNearMultipleOf45(angleDeg, tolerance) {
                if (typeof angleDeg !== "number") return false;
                var tol = (typeof tolerance === "number") ? Math.abs(tolerance) : 0.5;
                var normalized = ((angleDeg % 180) + 180) % 180;
                var remainder = normalized % 45;
                return remainder <= tol || 45 - remainder <= tol;
            }

            function resolveBranchConnectionStartEdgeDir(branchConnection, fallbackVector) {
                if (!branchConnection || !branchConnection.mainPath) return null;
                var mainPath = branchConnection.mainPath;
                var pts = null;
                try { pts = mainPath.pathPoints; } catch (ePts) { pts = null; }
                if (!pts || pts.length < 2) return null;

                var segIndex = branchConnection.mainSegmentIndex;
                if (typeof segIndex !== "number" || !isFinite(segIndex)) {
                    if (branchConnection.branchAnchorIndex !== undefined && branchConnection.branchAnchorIndex > 0) {
                        segIndex = branchConnection.branchAnchorIndex - 1;
                    } else {
                        segIndex = 0;
                    }
                }
                if (segIndex < 0) segIndex = 0;
                if (segIndex >= pts.length - 1) segIndex = pts.length - 2;

                var segStart = toPoint(pts[segIndex].anchor);
                var segEnd = toPoint(pts[segIndex + 1].anchor);
                var segVector = subtract(segEnd, segStart);
                var segLength = magnitude(segVector);
                if (segLength <= 0.000001) {
                    if (segIndex > 0) {
                        segStart = toPoint(pts[segIndex - 1].anchor);
                        segEnd = toPoint(pts[segIndex].anchor);
                        segVector = subtract(segEnd, segStart);
                        segLength = magnitude(segVector);
                    }
                    if (segLength <= 0.000001 && segIndex < pts.length - 2) {
                        segStart = toPoint(pts[segIndex + 1].anchor);
                        segEnd = toPoint(pts[segIndex + 2].anchor);
                        segVector = subtract(segEnd, segStart);
                        segLength = magnitude(segVector);
                    }
                    if (segLength <= 0.000001) {
                        return null;
                    }
                }

                var mainDir = scale(segVector, 1 / segLength);

                if (fallbackVector && typeof fallbackVector.x === "number" && typeof fallbackVector.y === "number") {
                    var align = dot(mainDir, fallbackVector);
                    if (align < 0) {
                        mainDir = { x: -mainDir.x, y: -mainDir.y };
                    }
                }

                if (!isFinite(mainDir.x) || !isFinite(mainDir.y)) {
                    if (fallbackVector && typeof fallbackVector.x === "number" && typeof fallbackVector.y === "number") {
                        return normalize(fallbackVector);
                    }
                    return null;
                }

                return mainDir;
            }

            function buildRectangle(sourcePath, startPoint, endPoint, normal, startHalfWidth, endHalfWidth, placementAfter, options) {
                var layer = sourcePath.layer;
                var rectPath = layer.pathItems.add();

                var effectiveNormal = normal;
                if (!effectiveNormal || typeof effectiveNormal.x !== "number" || typeof effectiveNormal.y !== "number" || !isFinite(effectiveNormal.x) || !isFinite(effectiveNormal.y)) {
                    effectiveNormal = { x: 0, y: 1 };
                }
                var endNormal = normalize(effectiveNormal);

                var startEdgeVector = endNormal;
                if (options && options.startEdgeVector && typeof options.startEdgeVector.x === "number" && typeof options.startEdgeVector.y === "number" && isFinite(options.startEdgeVector.x) && isFinite(options.startEdgeVector.y)) {
                    startEdgeVector = normalize(options.startEdgeVector);
                }

                var topStart = add(startPoint, scale(startEdgeVector, startHalfWidth));
                var topEnd = add(endPoint, scale(endNormal, endHalfWidth));
                var bottomEnd = add(endPoint, scale(endNormal, -endHalfWidth));
                var bottomStart = add(startPoint, scale(startEdgeVector, -startHalfWidth));

                rectPath.setEntirePath([
                    [topStart.x, topStart.y],
                    [topEnd.x, topEnd.y],
                    [bottomEnd.x, bottomEnd.y],
                    [bottomStart.x, bottomStart.y]
                ]);

                rectPath.closed = true;
                rectPath.filled = false;
                rectPath.stroked = true;
                rectPath.strokeCap = StrokeCap.ROUNDENDCAP;
                rectPath.strokeJoin = StrokeJoin.ROUNDENDJOIN;

                if (placementAfter) {
                    rectPath.move(placementAfter, ElementPlacement.PLACEAFTER);
                } else {
                    rectPath.move(sourcePath, ElementPlacement.PLACEAFTER);
                }

                return rectPath;
            }

            function buildCornerRectangle(sourcePath, seg1, seg2, halfWidth, scaledGapDistance, placementAfter) {
                var layer = sourcePath.layer;
                var cornerPath = layer.pathItems.add();

                var corner = seg1.end;
                var cornerStart = add(corner, scale(seg1.direction, -scaledGapDistance));
                var cornerEnd = add(corner, scale(seg2.direction, scaledGapDistance));

                var cross = seg1.direction.x * seg2.direction.y - seg1.direction.y * seg2.direction.x;

                var edge1Start = add(cornerStart, scale(seg1.normal, halfWidth));
                var edge1End = add(cornerEnd, scale(seg2.normal, halfWidth));
                var edge2Start = add(cornerStart, scale(seg1.normal, -halfWidth));
                var edge2End = add(cornerEnd, scale(seg2.normal, -halfWidth));

                var outerArcRadius = scaledGapDistance + halfWidth;
                var innerArcRadius = Math.max(scaledGapDistance - halfWidth, halfWidth * 0.3);

                var angleDot = clamp(dot(seg1.direction, seg2.direction), -1, 1);
                var cornerAngleRad = Math.acos(angleDot);
                if (!isFinite(cornerAngleRad)) cornerAngleRad = Math.PI / 2;
                var acuteRatio = 0;
                if (cornerAngleRad > 0 && cornerAngleRad < Math.PI / 2) {
                    acuteRatio = (Math.PI / 2 - cornerAngleRad) / (Math.PI / 2);
                    innerArcRadius = Math.max(innerArcRadius, scaledGapDistance + halfWidth * 0.6 * acuteRatio);
                    outerArcRadius = Math.max(Math.min(outerArcRadius, innerArcRadius - Math.max(halfWidth * 0.35, 0.75)), halfWidth * 0.2);
                }

                if (outerArcRadius >= innerArcRadius) {
                    outerArcRadius = Math.max(innerArcRadius - Math.max(halfWidth * 0.35, 0.75), halfWidth * 0.2);
                }

                var baseHandleFactor = 0.5522847498;
                var handleBoost = 1 + acuteRatio * 0.5;
                var handleLengthInner = innerArcRadius * baseHandleFactor * Math.max(handleBoost * 1.2, 1.05);
                var innerBoost = Math.max(1, 1 + acuteRatio * 0.35);
                var handleLengthOuter = outerArcRadius * baseHandleFactor * innerBoost;

                // Calculate how close we are to 90 degrees
                var cornerAngleDeg = cornerAngleRad * (180 / Math.PI);
                var deviationFrom90 = Math.abs(cornerAngleDeg - 90);
                // Only boost at 90 degrees - apply boost only if within 10 degrees of 90
                var ninetyDegreeBoost = 1.0;
                if (deviationFrom90 <= 10) {
                    // Gradual boost from 90 degrees (max) to 80/100 degrees (no boost)
                    var proximityTo90 = (10 - deviationFrom90) / 10; // 1.0 at 90deg, 0.0 at 80/100deg
                    ninetyDegreeBoost = 1 + proximityTo90 * 3.5; // Up to 4.5x at perfect 90 degrees (50% less than 7x)
                }

                // Note: "inner" variables refer to the inner arc (outer edge of duct visually)
                // "outer" variables refer to the outer arc (inner edge of duct visually)
                var appliedOuterHandleLength = handleLengthOuter * 0.5;
                var appliedInnerHandleLength = handleLengthInner * 2.0 * ninetyDegreeBoost;

                // Cap handle length to prevent oversized handles on small segments
                // Limit to 80% of the gap distance to keep handles proportional
                var maxHandleLength = scaledGapDistance * 0.8;
                if (appliedInnerHandleLength > maxHandleLength) {
                    appliedInnerHandleLength = maxHandleLength;
                }
                if (appliedOuterHandleLength > maxHandleLength) {
                    appliedOuterHandleLength = maxHandleLength;
                }

                var outerStart, outerEnd, innerStart, innerEnd;
                if (cross > 0) {
                    outerStart = edge1Start;
                    outerEnd = edge1End;
                    innerStart = edge2Start;
                    innerEnd = edge2End;
                } else {
                    outerStart = edge2Start;
                    outerEnd = edge2End;
                    innerStart = edge1Start;
                    innerEnd = edge1End;
                }

                if (acuteRatio > 0) {
                    var innerShift = Math.max(scaledGapDistance * 0.3, halfWidth * 0.45) * acuteRatio;
                    innerStart = add(innerStart, scale(seg1.direction, -innerShift));
                    innerEnd = add(innerEnd, scale(seg2.direction, innerShift));
                }

                cornerPath.setEntirePath([
                    [outerStart.x, outerStart.y],
                    [outerEnd.x, outerEnd.y],
                    [innerEnd.x, innerEnd.y],
                    [innerStart.x, innerStart.y]
                ]);

                var points = cornerPath.pathPoints;

                points[0].leftDirection = [outerStart.x, outerStart.y];
                points[0].rightDirection = [
                    outerStart.x + seg1.direction.x * appliedOuterHandleLength,
                    outerStart.y + seg1.direction.y * appliedOuterHandleLength
                ];

                points[1].leftDirection = [
                    outerEnd.x - seg2.direction.x * appliedOuterHandleLength,
                    outerEnd.y - seg2.direction.y * appliedOuterHandleLength
                ];
                points[1].rightDirection = [outerEnd.x, outerEnd.y];

                points[2].leftDirection = [innerEnd.x, innerEnd.y];
                points[2].rightDirection = [
                    innerEnd.x - seg2.direction.x * appliedInnerHandleLength,
                    innerEnd.y - seg2.direction.y * appliedInnerHandleLength
                ];

                points[3].leftDirection = [
                    innerStart.x + seg1.direction.x * appliedInnerHandleLength,
                    innerStart.y + seg1.direction.y * appliedInnerHandleLength
                ];
                points[3].rightDirection = [innerStart.x, innerStart.y];

                try {
                    cornerPath.note = "Corner connector";
                } catch (e) { }

                cornerPath.closed = true;
                cornerPath.filled = false;
                cornerPath.stroked = true;
                cornerPath.strokeCap = StrokeCap.ROUNDENDCAP;
                cornerPath.strokeJoin = StrokeJoin.ROUNDENDJOIN;

                if (placementAfter) {
                    cornerPath.move(placementAfter, ElementPlacement.PLACEAFTER);
                } else {
                    cornerPath.move(sourcePath, ElementPlacement.PLACEAFTER);
                }

                // Force corner pieces to front - always bring to front of layer
                try {
                    cornerPath.move(layer, ElementPlacement.PLACEATBEGINNING);
                } catch (eMove) { }

                return cornerPath;
            }

            function buildSCurve(sourcePath, startPoint, endPoint, direction, prevNormal, nextNormal, halfWidth, placementAfter, seg, scaledGapDistance, prevDirection, nextDirection) {
                var layer = sourcePath.layer;
                var sCurvePath = layer.pathItems.add();

                var totalLength = magnitude(subtract(endPoint, startPoint));

                var startEdge1 = add(startPoint, scale(prevNormal, halfWidth));
                var startEdge2 = add(startPoint, scale(prevNormal, -halfWidth));

                var endEdge1 = add(endPoint, scale(nextNormal, halfWidth));
                var endEdge2 = add(endPoint, scale(nextNormal, -halfWidth));

                sCurvePath.setEntirePath([
                    [startEdge1.x, startEdge1.y],
                    [endEdge2.x, endEdge2.y],
                    [endEdge1.x, endEdge1.y],
                    [startEdge2.x, startEdge2.y]
                ]);

                var points = sCurvePath.pathPoints;
                var handleLength = totalLength / 3;

                var p0Handle = scale(prevDirection, handleLength);
                points[0].leftDirection = points[0].anchor;
                points[0].rightDirection = [points[0].anchor[0] + p0Handle.x, points[0].anchor[1] + p0Handle.y];

                var p1Handle = scale(nextDirection, -handleLength);
                points[1].leftDirection = [points[1].anchor[0] + p1Handle.x, points[1].anchor[1] + p1Handle.y];
                points[1].rightDirection = points[1].anchor;

                var p2Handle = scale(nextDirection, -handleLength);
                points[2].leftDirection = points[2].anchor;
                points[2].rightDirection = [points[2].anchor[0] + p2Handle.x, points[2].anchor[1] + p2Handle.y];

                var p3Handle = scale(prevDirection, handleLength);
                points[3].leftDirection = [points[3].anchor[0] + p3Handle.x, points[3].anchor[1] + p3Handle.y];
                points[3].rightDirection = points[3].anchor;

                sCurvePath.closed = true;
                sCurvePath.filled = false;
                sCurvePath.stroked = true;
                sCurvePath.strokeCap = StrokeCap.ROUNDENDCAP;
                sCurvePath.strokeJoin = StrokeJoin.ROUNDENDJOIN;

                if (placementAfter) {
                    sCurvePath.move(placementAfter, ElementPlacement.PLACEAFTER);
                } else {
                    sCurvePath.move(sourcePath, ElementPlacement.PLACEAFTER);
                }

                return sCurvePath;
            }

            function calculateRectangleHalfWidth(rectangle) {
                var pts = rectangle.pathPoints;
                if (!pts || pts.length < 4) return 20;
                var p0 = { x: pts[0].anchor[0], y: pts[0].anchor[1] };
                var p1 = { x: pts[1].anchor[0], y: pts[1].anchor[1] };
                var p2 = { x: pts[2].anchor[0], y: pts[2].anchor[1] };
                var edge1Length = magnitude(subtract(p1, p0));
                var edge2Length = magnitude(subtract(p2, p1));
                var width = Math.min(edge1Length, edge2Length);
                return width / 2;
            }

            function processLine(path, gapDistance, taperPercent, totalWidth, epsilon, directionToleranceDeg, connections, createdRectangles, noTaper, orthogonalAngle, options) {
                var layerNameForDebug = "";
                try { layerNameForDebug = path.layer ? path.layer.name : ""; } catch (eLayerNameDebug) { layerNameForDebug = ""; }
                var pathNameForDebug = "";
                try { pathNameForDebug = path.name || "(unnamed)"; } catch (eNameDebug) { pathNameForDebug = "(unnamed)"; }

                addDebug("[Emory] Begin rectangle generation for path '" + pathNameForDebug + "' on layer '" + layerNameForDebug + "'.");

                options = options || {};
                var optionCenterlineId = options.centerlineId || null;
                var forceStartAtEnd = !!options.forceStartAtEnd;
                var preserveLastOffGrid = !!options.preserveLastOffGridSegment;
                var treatAsBranch = !!options.treatAsBranch;
                var isBlueDuctwork = isBlueDuctworkLayerName(layerNameForDebug);

                var pathPoints = path.pathPoints;
                if (pathPoints.length < 2) {
                    var emptyHalfWidth = totalWidth / 2;
                    return {
                        shapes: [],
                        metrics: {
                            startHalfWidth: emptyHalfWidth,
                            endHalfWidth: emptyHalfWidth,
                            startPoint: null,
                            endPoint: null,
                            anchorsReversed: false,
                            forceStartAtEndApplied: false,
                            lastSegmentAngle: null,
                            lastSegmentOriginalDiagonal: false,
                            lastSegmentTreatedAsDiagonal: false
                        }
                    };
                }

                var branchConnection = null;
                if (connections) {
                    var branchCandidates = [];
                    for (var c = 0; c < connections.length; c++) {
                        var candidateConn = connections[c];
                        if (candidateConn.branchPath === path) {
                            branchCandidates.push(candidateConn);
                        }
                    }
                    if (branchCandidates.length > 0) {
                        branchConnection = branchCandidates[0];
                        for (var bc = 0; bc < branchCandidates.length; bc++) {
                            if (branchCandidates[bc] && branchCandidates[bc].multiSide) {
                                branchConnection = branchCandidates[bc];
                                break;
                            }
                        }
                    }
                }

                var anchors = [];
                var anchorsReversed = false;
                var forcedStartApplied = false;
                for (var p = 0; p < pathPoints.length; p++) {
                    anchors.push(toPoint(pathPoints[p].anchor));
                }

                if (branchConnection) {
                    var connectionPt = { x: branchConnection.connectionPoint.x, y: branchConnection.connectionPoint.y };
                    if (branchConnection.branchAnchorIndex !== undefined && branchConnection.branchAnchorIndex >= 0) {
                        var connectionIndex = branchConnection.branchAnchorIndex;
                        if (connectionIndex === pathPoints.length - 1) {
                            anchors.reverse();
                            anchorsReversed = !anchorsReversed;
                            connectionIndex = 0;
                            branchConnection.branchAnchorIndex = 0;
                        } else if (connectionIndex !== 0) {
                            var rotatedAnchors = [];
                            for (var rotA = connectionIndex; rotA < anchors.length; rotA++) {
                                rotatedAnchors.push(anchors[rotA]);
                            }
                            for (var rotB = 0; rotB < connectionIndex; rotB++) {
                                rotatedAnchors.push(anchors[rotB]);
                            }
                            anchors = rotatedAnchors;
                            connectionIndex = 0;
                            branchConnection.branchAnchorIndex = 0;
                        }
                        anchors[0] = connectionPt;

                        var farthestIndex = 0;
                        var farthestDistSq = 0;
                        for (var anchorIdx = 1; anchorIdx < anchors.length; anchorIdx++) {
                            var dx = anchors[anchorIdx].x - connectionPt.x;
                            var dy = anchors[anchorIdx].y - connectionPt.y;
                            var d2 = dx * dx + dy * dy;
                            if (d2 > farthestDistSq) {
                                farthestDistSq = d2;
                                farthestIndex = anchorIdx;
                            }
                        }
                        if (farthestIndex === 0 && anchors.length > 1) {
                            farthestIndex = anchors.length - 1;
                        }
                        if (farthestIndex > 0 && farthestIndex < anchors.length - 1) {
                            anchors = anchors.slice(0, farthestIndex + 1);
                        }
                    } else {
                        anchors[0] = connectionPt;
                    }
                }

                if (!branchConnection && forceStartAtEnd && pathPoints.length > 1) {
                    anchors.reverse();
                    anchorsReversed = !anchorsReversed;
                    forcedStartApplied = true;
                }

                var startAnchorPoint = anchors.length ? clonePoint(anchors[0]) : null;
                var endAnchorPoint = anchors.length ? clonePoint(anchors[anchors.length - 1]) : null;

                var taperScale = 1 - clamp(taperPercent, 0, 1);
                if (taperScale <= epsilon) taperScale = 0.01;
                var baseHalfWidth = totalWidth / 2;
                var isBranch = (branchConnection !== null) || treatAsBranch;
                // Distinguish between actual blue-to-blue branches vs blue trunks connected to green
                var isActualBlueBranch = (branchConnection !== null) && isBlueDuctwork;
                var branchConnectionHalfWidth = null;
                var branchOffsetDistance = gapDistance;
                var scaledGapDistance = gapDistance * (baseHalfWidth / 14);
                try {
                    addDebug('[Emory] processLine flags: isBranch=' + isBranch + ', treatAsBranch=' + treatAsBranch + ', isActualBlueBranch=' + isActualBlueBranch + ', isBlueDuctwork=' + isBlueDuctwork + ', branchConnection=' + (branchConnection ? 'YES' : 'NO'));
                } catch (eDbg) { }

                var segments = [];
                for (var s = 1; s < anchors.length; s++) {
                    var seg = makeSegment(anchors[s - 1], anchors[s]);
                    if (seg.length > epsilon) segments.push(seg);
                }
                if (segments.length === 0) {
                    addDebug("[Emory] Path '" + pathNameForDebug + "' has zero usable segments after filtering.");
                    var zeroHalfWidth = baseHalfWidth;
                    return {
                        shapes: [],
                        metrics: {
                            startHalfWidth: zeroHalfWidth,
                            endHalfWidth: zeroHalfWidth,
                            startPoint: startAnchorPoint,
                            endPoint: endAnchorPoint,
                            anchorsReversed: anchorsReversed,
                            forceStartAtEndApplied: forcedStartApplied,
                            lastSegmentAngle: null,
                            lastSegmentOriginalDiagonal: false,
                            lastSegmentTreatedAsDiagonal: false
                        }
                    };
                }

                var scaledGapDistance = gapDistance * (baseHalfWidth / 14);

                // Reset connection half-width baseline for branch calculations
                var branchStartFullWidth = null;

                branchConnectionHalfWidth = baseHalfWidth;
                var customBranchFullWidth = null;
                if (optionCenterlineId) {
                    customBranchFullWidth = getBranchCustomStartWidth(optionCenterlineId);
                }
                var hasCustomBranchWidth = (typeof customBranchFullWidth === "number" && isFinite(customBranchFullWidth) && customBranchFullWidth > 0);

                // Store the original trunk connection width BEFORE potentially overriding with custom branch width
                // This is needed so branches can properly offset from the trunk edge, not the trunk centerline
                var trunkConnectionHalfWidth = branchConnectionHalfWidth;

                if (hasCustomBranchWidth) {
                    branchConnectionHalfWidth = customBranchFullWidth / 2;
                }

                if (isActualBlueBranch) {
                    if (hasCustomBranchWidth) {
                        // When custom branch width is set, use it as the starting width for the branch
                        // The tapering will naturally flow from this custom starting width
                        baseHalfWidth = branchConnectionHalfWidth;
                    } else {
                        var taperedHalfWidth = branchConnectionHalfWidth * taperScale;
                        if (!isFinite(taperedHalfWidth) || taperedHalfWidth <= epsilon) {
                            taperedHalfWidth = branchConnectionHalfWidth * 0.5;
                        }
                        if (taperedHalfWidth > branchConnectionHalfWidth) {
                            taperedHalfWidth = branchConnectionHalfWidth;
                        }
                        if (taperedHalfWidth < 0.5) {
                            taperedHalfWidth = 0.5;
                        }
                        baseHalfWidth = taperedHalfWidth;
                    }
                } else if (hasCustomBranchWidth) {
                    // For non-blue branches (green ductwork) with custom width, use the custom width
                    baseHalfWidth = branchConnectionHalfWidth;
                }

                scaledGapDistance = gapDistance * (baseHalfWidth / 14);

                if (isBranch) {
                    branchStartFullWidth = branchConnectionHalfWidth * 2;
                    if (hasCustomBranchWidth) {
                        if (optionCenterlineId) {
                            setBranchCustomStartWidth(optionCenterlineId, branchStartFullWidth);
                        }
                        setBranchStartWidthOnPath(path, branchStartFullWidth);
                    } else {
                        if (optionCenterlineId) {
                            clearBranchCustomStartWidth(optionCenterlineId);
                        }
                        clearBranchStartWidthOnPath(path);
                    }
                    // Use trunk connection width for offset, not branch width
                    // This ensures branches start at the edge of the trunk, not at the centerline
                    branchOffsetDistance = trunkConnectionHalfWidth;
                } else {
                    if (optionCenterlineId) {
                        clearBranchCustomStartWidth(optionCenterlineId);
                    }
                    clearBranchStartWidthOnPath(path);
                    branchOffsetDistance = gapDistance;
                }

                scaledGapDistance = gapDistance * (baseHalfWidth / 14);
                totalWidth = baseHalfWidth * 2;

                if (isBranch && segments.length > 0) {
                    var connectionPointBranch = anchors[0];
                    var finalSegmentCandidate = segments[segments.length - 1];
                    if (finalSegmentCandidate) {
                        var startDistBranch = Math.pow(finalSegmentCandidate.start.x - connectionPointBranch.x, 2) +
                            Math.pow(finalSegmentCandidate.start.y - connectionPointBranch.y, 2);
                        var endDistBranch = Math.pow(finalSegmentCandidate.end.x - connectionPointBranch.x, 2) +
                            Math.pow(finalSegmentCandidate.end.y - connectionPointBranch.y, 2);
                        if (endDistBranch + (epsilon * epsilon) < startDistBranch) {
                            var reorderedAnchors = [anchors[0]];
                            for (var revIdx = anchors.length - 1; revIdx >= 1; revIdx--) {
                                reorderedAnchors.push(anchors[revIdx]);
                            }
                            anchors = reorderedAnchors;
                            anchorsReversed = !anchorsReversed;
                            segments = [];
                            for (var rebuildIdx = 1; rebuildIdx < anchors.length; rebuildIdx++) {
                                var rebuiltSeg = makeSegment(anchors[rebuildIdx - 1], anchors[rebuildIdx]);
                                if (rebuiltSeg.length > epsilon) segments.push(rebuiltSeg);
                            }
                            if (segments.length === 0) {
                                addDebug("[Emory] Branch path '" + pathNameForDebug + "' lost all segments after orientation correction.");
                                var branchHalfWidth = baseHalfWidth;
                                return {
                                    shapes: [],
                                    metrics: {
                                        startHalfWidth: branchHalfWidth,
                                        endHalfWidth: branchHalfWidth,
                                        startPoint: startAnchorPoint,
                                        endPoint: endAnchorPoint,
                                        anchorsReversed: anchorsReversed,
                                        forceStartAtEndApplied: forcedStartApplied,
                                        lastSegmentAngle: null,
                                        lastSegmentOriginalDiagonal: false,
                                        lastSegmentTreatedAsDiagonal: false
                                    }
                                };
                            }
                            endAnchorPoint = anchors.length ? clonePoint(anchors[anchors.length - 1]) : endAnchorPoint;
                        }
                    }
                }

                var anchorSequences = [{ anchors: anchors.slice(0), preserveLast: preserveLastOffGrid }];

                if (isBranch && branchConnection && branchConnection.multiSide && typeof branchConnection.branchSegmentIndex === "number" && branchConnection.branchSegmentIndex >= 0) {
                    anchorSequences = [];
                    var branchSegIndex = branchConnection.branchSegmentIndex;
                    var connectionClone = clonePoint({ x: branchConnection.connectionPoint.x, y: branchConnection.connectionPoint.y });

                    var forwardAnchors = [clonePoint(connectionClone)];
                    for (var fIdx = branchSegIndex + 1; fIdx < pathPoints.length; fIdx++) {
                        forwardAnchors.push(toPoint(pathPoints[fIdx].anchor));
                    }
                    if (forwardAnchors.length > 1) {
                        anchorSequences.push({ anchors: forwardAnchors, preserveLast: preserveLastOffGrid });
                    }

                    var backwardAnchors = [clonePoint(connectionClone)];
                    for (var bIdx = branchSegIndex; bIdx >= 0; bIdx--) {
                        var backPoint = toPoint(pathPoints[bIdx].anchor);
                        if (bIdx === branchSegIndex && almostEqualPoints([backPoint.x, backPoint.y], [connectionClone.x, connectionClone.y])) {
                            continue;
                        }
                        backwardAnchors.push(backPoint);
                    }
                    if (backwardAnchors.length > 1) {
                        anchorSequences.push({ anchors: backwardAnchors, preserveLast: preserveLastOffGrid });
                    }
                    if (anchorSequences.length === 0) {
                        anchorSequences.push({ anchors: anchors.slice(0), preserveLast: preserveLastOffGrid });
                    }
                }

                var placementAfter = path;
                var rectangles = [];
                var nonCornerShapes = [];
                var cornerShapes = [];
                var segmentCounter = 0;
                var aggregateDiagCounts = {
                    segmentsTotal: 0,
                    sCurves: 0,
                    bodyRects: 0,
                    tailRects: 0,
                    cornerRects: 0,
                    diagonalSegments: 0,
                    skippedShortSegments: 0
                };

                var finalSegmentAngle = null;
                var finalSegmentOriginalDiagonal = false;
                var finalSegmentTreatedAsDiagonal = false;
                var finalEndWidth = baseHalfWidth;

                function processAnchorSequence(anchorSeq, preserveLastFlag) {
                    if (!anchorSeq || anchorSeq.length < 2) return;
                    var localSegments = [];
                    for (var s = 1; s < anchorSeq.length; s++) {
                        var localSeg = makeSegment(anchorSeq[s - 1], anchorSeq[s]);
                        if (localSeg.length > epsilon) {
                            localSegments.push(localSeg);
                        } else {
                            aggregateDiagCounts.skippedShortSegments++;
                        }
                    }
                    if (localSegments.length === 0) return;

                    var connectionStartEdgeDir = null;
                    var sequenceStartsAtConnection = false;
                    if (isBranch && branchConnection && anchorSeq.length > 0) {
                        var seqStart = anchorSeq[0];
                        var connPoint = branchConnection.connectionPoint;
                        if (seqStart && connPoint) {
                            var dxConn = seqStart.x - connPoint.x;
                            var dyConn = seqStart.y - connPoint.y;
                            var epsVal = (typeof epsilon === "number" && isFinite(epsilon)) ? Math.abs(epsilon) : 0;
                            var connTolSq = Math.max(epsVal * epsVal, 0.25);
                            if ((dxConn * dxConn + dyConn * dyConn) <= connTolSq) {
                                sequenceStartsAtConnection = true;
                            }
                        }
                    }
                    if (sequenceStartsAtConnection && isActualBlueBranch) {
                        try {
                            connectionStartEdgeDir = resolveBranchConnectionStartEdgeDir(branchConnection, localSegments[0].normal);
                        } catch (eResolveBranchStartNormal) {
                            connectionStartEdgeDir = null;
                        }
                    }

                    aggregateDiagCounts.segmentsTotal += localSegments.length;

                    var localPrevHadCorner = false;
                    var localPrevWasDiagonal = false;
                    var localPrevCornerGap = 0; // store gap used by previous corner so next segment trims correctly
                    // Initialize the current width - for branches with custom widths, baseHalfWidth is already set to the custom value
                    // Tapering will progressively reduce this value as we move along the branch segments
                    var localCurrentStartWidth = baseHalfWidth;

                    // For endpoint segments: taper toward the endpoint (but NOT for branches at connection point)
                    // For inner segments: progressive taper (normal behavior)
                    var segmentWidths = [];
                    for (var swIdx = 0; swIdx < localSegments.length; swIdx++) {
                        var segmentWidthStart, segmentWidthEnd;
                        var isFirstSegment = (swIdx === 0);
                        var isLastSegment = (swIdx === localSegments.length - 1);

                        // For branches, the first segment is at the connection point and should NOT taper
                        // For non-branches (main trunk), the first segment IS an endpoint and should taper
                        if (isFirstSegment && !isBranch) {
                            // First segment on main trunk: taper points toward start endpoint
                            // narrow (at start) → wider (away from start)
                            segmentWidthStart = baseHalfWidth * taperScale;
                            segmentWidthEnd = baseHalfWidth;
                        } else if (isLastSegment) {
                            // Last segment: taper points toward end endpoint (for both branches and main trunk)
                            // Start width is carried from previous segment
                            // wider (away from end) → narrow (at end)
                            segmentWidthStart = null; // Will be set from previous segment
                            segmentWidthEnd = null; // Will be calculated as progressive taper
                        } else {
                            // Inner segments (and first segment of branches): use progressive tapering (normal behavior)
                            segmentWidthStart = null; // Will be set from previous segment
                            segmentWidthEnd = null; // Will be calculated as progressive taper
                        }

                        segmentWidths.push({ start: segmentWidthStart, end: segmentWidthEnd, isFirst: isFirstSegment, isLast: isLastSegment, isBranchFirst: (isFirstSegment && isBranch) });
                    }

                    // Register wire setup: improved wire handling for register endpoints
                    var connectRegisterWire = !!options.connectRegisterViaWire;
                    var registerEndpointList = null;
                    if (connectRegisterWire) {
                        try { registerEndpointList = path.__mdRegisterEndpoints || null; } catch (eReg) { registerEndpointList = null; }
                        if (registerEndpointList && registerEndpointList.length && GLOBAL_IGNORED_ANCHORS && GLOBAL_IGNORED_ANCHORS.length) {
                            var filteredRegisterEndpoints = [];
                            for (var reIdx = 0; reIdx < registerEndpointList.length; reIdx++) {
                                var endpointEntry = registerEndpointList[reIdx];
                                if (!endpointEntry || !endpointEntry.pos) continue;
                                var endpointPos = [endpointEntry.pos.x, endpointEntry.pos.y];
                                if (isPointIgnored(endpointPos, GLOBAL_IGNORED_ANCHORS)) {
                                    continue;
                                }
                                filteredRegisterEndpoints.push(endpointEntry);
                            }
                            if (filteredRegisterEndpoints.length !== registerEndpointList.length) {
                                registerEndpointList = filteredRegisterEndpoints;
                                try { path.__mdRegisterEndpoints = filteredRegisterEndpoints; } catch (eAssignReg) { }
                            }
                            if (!registerEndpointList.length) {
                                registerEndpointList = null;
                                try { delete path.__mdRegisterEndpoints; } catch (eDelReg) { }
                            }
                        }
                    }
                    // Diagnostic logging for wire mode
                    try {
                        if (connectRegisterWire) {
                            var regCount = registerEndpointList ? registerEndpointList.length : 0;
                            addDebug('[DIAG] Emory.processLine: post-filter registerEndpointCount=' + regCount + ' for centerline ' + (optionCenterlineId || '(unknown)'));
                            if (regCount === 0) {
                                addDebug('[DIAG] Emory.processLine: wire-mode enabled but registerEndpointCount=0; connectRegisterViaWire=' + connectRegisterWire + ', isBlueDuctwork=' + isBlueDuctwork);
                            } else {
                                addDebug('[DIAG] Emory.processLine: wire-mode enabled; registerEndpointCount=' + regCount + ', isBlueDuctwork=' + isBlueDuctwork);
                            }
                        }
                    } catch (eDiag) { }
                    var registerWireEnabled = connectRegisterWire && registerEndpointList && registerEndpointList.length > 0;
                    if (connectRegisterWire) {
                        var regCountWire = registerEndpointList ? registerEndpointList.length : 0;
                        addDebug('[DIAG] EmoryGeometry.processLine: post-filter registerEndpointCount=' + regCountWire + ' for centerline ' + (optionCenterlineId || '(unknown)'));
                    }
                    function matchesRegisterEndpoint(point) {
                        if (!registerWireEnabled || !point) return false;
                        var px = point.x;
                        var py = point.y;
                        if (typeof px !== "number" || typeof py !== "number") return false;
                        var tol = Math.max(0.5, epsilon * 4);
                        var tol2 = tol * tol;
                        for (var ri = 0; ri < registerEndpointList.length; ri++) {
                            var entry = registerEndpointList[ri];
                            if (!entry || !entry.pos) continue;
                            var dx = entry.pos.x - px;
                            var dy = entry.pos.y - py;
                            if ((dx * dx + dy * dy) <= tol2) return true;
                        }
                        return false;
                    }

                    for (var segIdx = 0; segIdx < localSegments.length; segIdx++) {
                        var seg = localSegments[segIdx];
                        // ensure segmentStart is available for early branches (wire creation uses it)
                        var segmentStart = seg.start;
                        if (seg.length <= epsilon) {
                            aggregateDiagCounts.skippedShortSegments++;
                            continue;
                        }

                        // Wire connection mode: if enabled and this is the last segment, create a curved wire instead of rectangles
                        var isLastSegment = (segIdx === localSegments.length - 1);
                        var isSecondToLastSegment = (segIdx === localSegments.length - 2);
                        var nextSegmentCandidate = (segIdx < localSegments.length - 1) ? localSegments[segIdx + 1] : null;
                        var nextWillBeWire = registerWireEnabled && nextSegmentCandidate && (segIdx + 1 === localSegments.length - 1) && matchesRegisterEndpoint(nextSegmentCandidate.end);
                        var useRegisterWire = registerWireEnabled && isLastSegment && matchesRegisterEndpoint(seg.end);
                        var nextSegmentIsWire = (options.wireConnection && !isLastSegment && segIdx === localSegments.length - 2);

                        // Improved register wire logic with junction validation
                        if (useRegisterWire) {
                            // Only create register-wire when the segment start aligns to a junction with Blue/Green ductwork
                            var startIsValidJunction = false;
                            try {
                                var connTol = Math.max(2, (scaledGapDistance || 2) * 1.5);
                                var tol2 = connTol * connTol;
                                if (connections && connections.length) {
                                    for (var cc = 0; cc < connections.length; cc++) {
                                        var c = connections[cc];
                                        if (!c || !c.connectionPoint) continue;
                                        var dx = c.connectionPoint.x - segmentStart.x;
                                        var dy = c.connectionPoint.y - segmentStart.y;
                                        if ((dx * dx + dy * dy) <= tol2) {
                                            try {
                                                var other = (c.mainPath === path) ? c.branchPath : c.mainPath;
                                                var otherLayerName = other && other.layer ? other.layer.name : null;
                                                var otherColor = getColorNameForLayer(otherLayerName);
                                                if (otherColor === 'Blue Ductwork' || otherColor === 'Green Ductwork') {
                                                    startIsValidJunction = true;
                                                    break;
                                                }
                                            } catch (eOther) { }
                                        }
                                    }
                                }
                            } catch (eJ) { }
                            if (!startIsValidJunction) {
                                try {
                                    if (typeof app !== 'undefined' && app && app.documents && app.documents.length) {
                                        var doc = app.activeDocument;
                                        var fbConnTol = Math.max(2, (scaledGapDistance || 2) * 2.0);
                                        var fbTol2 = fbConnTol * fbConnTol;
                                        for (var pi = 0; pi < doc.pathItems.length; pi++) {
                                            var pItem = doc.pathItems[pi];
                                            if (!pItem) continue;
                                            try { if (pItem === path) continue; } catch (eSame4) { }
                                            var layerName = (pItem.layer && pItem.layer.name) ? pItem.layer.name : null;
                                            var colName = getColorNameForLayer(layerName);
                                            if (colName !== 'Blue Ductwork' && colName !== 'Green Ductwork') continue;
                                            try {
                                                var ppts = pItem.pathPoints;
                                                for (var ppi = 0; ppi < ppts.length; ppi++) {
                                                    var ap = ppts[ppi].anchor;
                                                    var dx2 = ap[0] - segmentStart.x;
                                                    var dy2 = ap[1] - segmentStart.y;
                                                    if ((dx2 * dx2 + dy2 * dy2) <= fbTol2) {
                                                        startIsValidJunction = true;
                                                        break;
                                                    }
                                                }
                                            } catch (ePts3) { }
                                            if (startIsValidJunction) break;
                                        }
                                    }
                                } catch (eF) { }
                            }
                            if (!startIsValidJunction) {
                                addDebug('[Emory] Skipping register-wire: seg.start is not a junction with Blue/Green ductwork');
                                continue;
                            }
                            // Defensive: ensure we have coordinates and a valid layer to add the path
                            var ss = segmentStart || seg.start || (seg.end ? { x: seg.end.x, y: seg.end.y } : { x: 0, y: 0 });
                            var se = seg.end || ss;
                            if (!path || !path.layer || !path.layer.pathItems) {
                                addDebug('[Emory] Cannot create register wire: missing path or layer');
                                continue;
                            }
                            try {
                                var wirePath = path.layer.pathItems.add();
                                wirePath.setEntirePath([
                                    [ss.x, ss.y],
                                    [se.x, se.y]
                                ]);
                            } catch (eWireCreate) {
                                addDebug('[Emory] Failed to create register wire path: ' + eWireCreate);
                                continue;
                            }
                            // If wirePath wasn't created for any reason, skip
                            if (typeof wirePath === 'undefined' || !wirePath) {
                                addDebug('[Emory] Skipping wire processing because wirePath is undefined');
                                continue;
                            }
                            wirePath.closed = false;
                            try { wirePath.filled = false; } catch (eFill) { }
                            try { wirePath.stroked = true; } catch (eStroke) { }
                            try { wirePath.strokeWidth = 3; } catch (eWireWidth) { }
                            try {
                                var wireColor = new RGBColor();
                                wireColor.red = 0;
                                wireColor.green = 0;
                                wireColor.blue = 255;
                                wirePath.strokeColor = wireColor;
                            } catch (eWireColor) { }
                            try { wirePath.strokeCap = StrokeCap.ROUNDENDCAP; } catch (eWireCap) { }
                            try { wirePath.strokeJoin = StrokeJoin.ROUNDENDJOIN; } catch (eWireJoin) { }
                            if (placementAfter) {
                                wirePath.move(placementAfter, ElementPlacement.PLACEAFTER);
                            } else {
                                wirePath.move(path, ElementPlacement.PLACEAFTER);
                            }
                            var wirePoints = wirePath.pathPoints;
                            if (wirePoints && wirePoints.length === 2) {
                                var prevDirection = (segIdx > 0) ? localSegments[segIdx - 1].direction : seg.direction;
                                var wireDX = seg.end.x - segmentStart.x;
                                var wireDY = seg.end.y - segmentStart.y;
                                var wireLen = Math.sqrt(wireDX * wireDX + wireDY * wireDY);
                                if (!isFinite(wireLen) || wireLen <= 0) wireLen = 1;
                                var baseHandle = Math.max(wireLen * 0.45, 6);
                                var handleLen = Math.min(wireLen * 0.6, baseHandle);
                                if (handleLen > 40) handleLen = 40;
                                var startPointInfo = wirePoints[0];
                                var endPointInfo = wirePoints[1];
                                if (startPointInfo) {
                                    startPointInfo.leftDirection = [segmentStart.x, segmentStart.y];
                                    startPointInfo.rightDirection = [
                                        segmentStart.x + prevDirection.x * handleLen,
                                        segmentStart.y + prevDirection.y * handleLen
                                    ];
                                    try { startPointInfo.pointType = PointType.SMOOTH; } catch (eWireStartType) { }
                                }
                                var endDir = seg.direction;
                                if (endPointInfo) {
                                    endPointInfo.leftDirection = [
                                        seg.end.x - endDir.x * (handleLen * 0.6),
                                        seg.end.y - endDir.y * (handleLen * 0.6)
                                    ];
                                    endPointInfo.rightDirection = [seg.end.x, seg.end.y];
                                    try { endPointInfo.pointType = PointType.SMOOTH; } catch (eWireEndType) { }
                                }
                            }
                            wirePath.__mdRegisterWire = true;
                            var wireNoteTokens = readNoteTokens(wirePath);
                            if (arrayIndexOf(wireNoteTokens, REGISTER_WIRE_TAG) === -1) {
                                wireNoteTokens.push(REGISTER_WIRE_TAG);
                                writeNoteTokens(wirePath, wireNoteTokens);
                            }
                            var pathTypeWire = isActualBlueBranch ? "Branch" : "Main Trunk";
                            var wireNameBase = pathTypeWire + " - Register Wire";
                            try { wirePath.name = wireNameBase; } catch (eWireName) { }
                            placementAfter = wirePath;
                            nonCornerShapes.push(wirePath);
                            finalEndWidth = 0;
                            localPrevHadCorner = false;
                            localPrevCornerGap = 0;
                            localPrevWasDiagonal = false;
                            localCurrentStartWidth = 0;
                            continue;
                        }

                        if (options.wireConnection && isLastSegment) {
                            // Remove the previous segment's tail rectangle if it exists
                            // The tail from the second-to-last segment extends to where this wire should start
                            if (rectangles && rectangles.length > 0) {
                                try {
                                    var lastRect = rectangles[rectangles.length - 1];
                                    // Check if this is a tail rectangle by looking at its name
                                    if (lastRect.name && lastRect.name.indexOf("Tail") !== -1) {
                                        addDebug('[Emory] Removing last tail rectangle for wire connection: ' + lastRect.name);
                                        lastRect.remove();
                                        rectangles.pop();
                                        nonCornerShapes.pop();
                                        aggregateDiagCounts.tailRects--;
                                    }
                                } catch (eRemove) {
                                    addDebug('[Emory] Failed to remove last tail rectangle: ' + eRemove);
                                }
                            }

                            // Create a curved wire from the segment start to end
                            var wirePath = path.layer.pathItems.add();

                            // Get the previous segment's direction for the handle
                            var prevSeg = (segIdx > 0) ? localSegments[segIdx - 1] : null;
                            var prevDirection = prevSeg ? prevSeg.direction : seg.direction;

                            // Calculate handle length (about 1/3 of segment length)
                            var handleLength = seg.length * 0.33;

                            // Set the path with two points
                            wirePath.setEntirePath([
                                [seg.start.x, seg.start.y],
                                [seg.end.x, seg.end.y]
                            ]);

                            // Set direction handles
                            var pts = wirePath.pathPoints;
                            pts[0].leftDirection = [seg.start.x, seg.start.y];
                            pts[0].rightDirection = [
                                seg.start.x + prevDirection.x * handleLength,
                                seg.start.y + prevDirection.y * handleLength
                            ];
                            pts[1].leftDirection = [seg.end.x, seg.end.y];
                            pts[1].rightDirection = [seg.end.x, seg.end.y];

                            // Style the wire
                            wirePath.filled = false;
                            wirePath.stroked = true;
                            var blueColor = new RGBColor();
                            blueColor.red = 0;
                            blueColor.green = 0;
                            blueColor.blue = 255;
                            wirePath.strokeColor = blueColor;
                            wirePath.strokeWidth = 3;
                            wirePath.closed = false;

                            try {
                                wirePath.name = "Wire Connection";
                            } catch (e) { }

                            addDebug('[Emory] Created wire connection from [' + Math.round(seg.start.x) + ',' + Math.round(seg.start.y) + '] to [' + Math.round(seg.end.x) + ',' + Math.round(seg.end.y) + ']');

                            // Skip the rest of the segment processing
                            continue;
                        }

                        var segmentAngle = Math.atan2(seg.direction.y, seg.direction.x) * (180 / Math.PI);
                        var diagonalOriginal = isDiagonalAngle(segmentAngle, orthogonalAngle);
                        var isDiagonal = diagonalOriginal;

                        // If this is BLUE ductwork and this segment was classified diagonal (e.g. ~40°)
                        // but the next segment is approximately 90°, prefer creating a small corner
                        // connector instead of an S-curve. In that case, disable diagonal handling
                        // so the corner creation path runs below.
                        try {
                            var lookaheadSeg = (segIdx < localSegments.length - 1) ? localSegments[segIdx + 1] : null;
                            if (isBlueDuctwork && isDiagonal && lookaheadSeg) {
                                var laDot = dot(seg.direction, lookaheadSeg.direction);
                                var laAngle = Math.acos(clamp(laDot, -1, 1)) * 180 / Math.PI;
                                if (laAngle >= 80 && laAngle <= 100) {
                                    // disable diagonal handling for this segment to allow corner logic
                                    isDiagonal = false;
                                    addDebug('[Emory] Overriding diagonal S-curve for blue segment at index ' + segIdx + ' because next angle ~' + Math.round(laAngle));
                                }
                            }
                        } catch (eLook) {
                            // ignore lookahead errors
                        }
                        if (preserveLastFlag && segIdx === localSegments.length - 1 && !isAngleNearMultipleOf45(segmentAngle, 0.5)) {
                            isDiagonal = false;
                        }
                        if (segIdx === localSegments.length - 1) {
                            finalSegmentAngle = segmentAngle;
                            finalSegmentOriginalDiagonal = diagonalOriginal;
                            finalSegmentTreatedAsDiagonal = isDiagonal;
                        }
                        var segmentStart = seg.start;
                        var offsetDistance = 0;
                        var startEdgeOverride = (segIdx === 0 && connectionStartEdgeDir) ? connectionStartEdgeDir : null;

                        if (segIdx === 0 && isBranch && (!isBlueDuctwork || isActualBlueBranch)) {
                            // For branches (green/Emory and actual blue branches), offset the first segment from connection point.
                            // When processing blue branches, insert a transition rectangle so the branch meets the main trunk edge.
                            var availableLength = seg.length;
                            var minTailLength = Math.min(Math.max(totalWidth * 0.25, 4), availableLength);
                            var desiredOffset = Math.min(branchOffsetDistance, availableLength);
                            var branchStartOffset = Math.min(desiredOffset, Math.max(availableLength - minTailLength, 0));
                            if (hasCustomBranchWidth) {
                                // When a custom branch width is set, offset by the TRUNK's width (not the branch width)
                                // This ensures the branch starts at the edge of the trunk rectangle, not at the centerline
                                branchStartOffset = desiredOffset;
                            } else if (isActualBlueBranch) {
                                branchStartOffset = desiredOffset;
                                var minRemaining = Math.min(Math.max(totalWidth * 0.15, 2), availableLength);
                                if ((availableLength - branchStartOffset) < minRemaining) {
                                    branchStartOffset = Math.max(availableLength - minRemaining, 0);
                                }
                            }
                            segmentStart = add(seg.start, scale(seg.direction, branchStartOffset));
                            offsetDistance = branchStartOffset;
                        } else if (localPrevHadCorner) {
                            // Trim the start of this segment by the previous corner gap so rectangles meet the corner
                            segmentStart = add(seg.start, scale(seg.direction, localPrevCornerGap || scaledGapDistance));
                            offsetDistance = localPrevCornerGap || scaledGapDistance;
                        } else if (localPrevWasDiagonal) {
                            segmentStart = seg.start;
                            offsetDistance = 0;
                        }

                        if (isDiagonal) {
                            var prevSeg = (segIdx > 0) ? localSegments[segIdx - 1] : null;
                            var nextSeg = (segIdx < localSegments.length - 1) ? localSegments[segIdx + 1] : null;
                            var sCurveStart = seg.start;
                            var sCurveEnd = seg.end;
                            var prevNormal = prevSeg ? prevSeg.normal : seg.normal;
                            var nextNormal = nextSeg ? nextSeg.normal : seg.normal;
                            var flippedNextNormal = { x: -nextNormal.x, y: -nextNormal.y };
                            var prevDirection = prevSeg ? prevSeg.direction : seg.direction;
                            var nextDirection = nextSeg ? nextSeg.direction : seg.direction;

                            var sCurvePath = buildSCurve(
                                path,
                                sCurveStart,
                                sCurveEnd,
                                seg.direction,
                                prevNormal,
                                flippedNextNormal,
                                localCurrentStartWidth,
                                placementAfter,
                                seg,
                                scaledGapDistance,
                                prevDirection,
                                nextDirection
                            );
                            alignAppearance(sCurvePath, path);
                            segmentCounter++;
                            var pathType = isActualBlueBranch ? "Branch" : "Main Trunk";
                            sCurvePath.name = pathType + " - Segment " + segmentCounter + " S-Curve";
                            placementAfter = sCurvePath;
                            rectangles.push(sCurvePath);
                            nonCornerShapes.push(sCurvePath);
                            aggregateDiagCounts.sCurves++;
                            localPrevWasDiagonal = true;
                            localPrevHadCorner = false;
                            continue;
                        }

                        localPrevWasDiagonal = false;
                        if (isDiagonal) aggregateDiagCounts.diagonalSegments++;

                        var nextSeg = (segIdx < localSegments.length - 1) ? localSegments[segIdx + 1] : null;
                        var nextAngle = null;
                        var createCorner = false;
                        var nextIsDiagonal = false;
                        // Small-corner override flag for blue ductwork
                        var smallCornerOverride = false;

                        if (nextSeg) {
                            // suppression flag: if true, skip any corner creation at this junction
                            var suppressCornerBecauseConnectedBlue = false;
                            var dotProduct = dot(seg.direction, nextSeg.direction);
                            nextAngle = Math.acos(clamp(dotProduct, -1, 1)) * 180 / Math.PI;

                            // ALWAYS check for blue connections at this junction, regardless of angle
                            // This prevents unwanted corner creation at T-connections
                            try {
                                if (connections && connections.length) {
                                    // Increase tolerance to catch slightly offset connection points
                                    var connTol = Math.max(2, (scaledGapDistance || 2) * 1.5);
                                    var tol2 = connTol * connTol;
                                    for (var cc = 0; cc < connections.length; cc++) {
                                        var c = connections[cc];
                                        if (!c || !c.connectionPoint) continue;
                                        var dx = c.connectionPoint.x - seg.end.x;
                                        var dy = c.connectionPoint.y - seg.end.y;
                                        if ((dx * dx + dy * dy) <= tol2) {
                                            // Check if THIS path is involved in this connection
                                            var isMainPath = false;
                                            var isBranchPath = false;
                                            try { isMainPath = (c.mainPath === path); } catch (e) { }
                                            try { isBranchPath = (c.branchPath === path); } catch (e) { }

                                            // If this path is the MAIN path (trunk), suppress corner at connection point
                                            if (isMainPath) {
                                                try {
                                                    var branchLayerName = c.branchPath && c.branchPath.layer ? c.branchPath.layer.name : null;
                                                    var branchColor = getColorNameForLayer(branchLayerName);
                                                    if (branchColor === 'Blue Ductwork') {
                                                        suppressCornerBecauseConnectedBlue = true;
                                                        try { addDebug('[Emory] Suppressing corner at seg end - this path is MAIN with blue branch: ' + (c.branchPath.name || branchLayerName || '(unnamed)')); } catch (eDbg) { }
                                                        break;
                                                    }
                                                } catch (eLayerCheck) { }
                                            }

                                            // If this path is the BRANCH path, also check if main is blue
                                            if (isBranchPath && !suppressCornerBecauseConnectedBlue) {
                                                try {
                                                    var mainLayerName = c.mainPath && c.mainPath.layer ? c.mainPath.layer.name : null;
                                                    var mainColor = getColorNameForLayer(mainLayerName);
                                                    if (mainColor === 'Blue Ductwork') {
                                                        suppressCornerBecauseConnectedBlue = true;
                                                        try { addDebug('[Emory] Suppressing corner at seg end - this path is BRANCH with blue main: ' + (c.mainPath.name || mainLayerName || '(unnamed)')); } catch (eDbg) { }
                                                        break;
                                                    }
                                                } catch (eLayerCheck) { }
                                            }
                                        }
                                    }
                                }
                            } catch (eConnCheck) { }

                            // Fallback: sometimes the connections table can miss endpoints
                            // — scan document pathItems for any path (other than this one) that has
                            // an anchor near seg.end and is on a Blue Ductwork layer.
                            try {
                                if (!suppressCornerBecauseConnectedBlue && typeof app !== 'undefined' && app && app.documents && app.documents.length) {
                                    var doc = app.activeDocument;
                                    var fbConnTol = Math.max(2, (scaledGapDistance || 2) * 2.0);
                                    var fbTol2 = fbConnTol * fbConnTol;
                                    for (var pi = 0; pi < doc.pathItems.length; pi++) {
                                        var pItem = doc.pathItems[pi];
                                        if (!pItem) continue;
                                        // skip the same path object if possible
                                        try { if (pItem === path) continue; } catch (eSame) { }
                                        var layerName = (pItem.layer && pItem.layer.name) ? pItem.layer.name : null;
                                        var colName = getColorNameForLayer(layerName);
                                        if (colName !== 'Blue Ductwork') continue;
                                        try {
                                            var ppts = pItem.pathPoints;
                                            for (var ppi = 0; ppi < ppts.length; ppi++) {
                                                var ap = ppts[ppi].anchor;
                                                var dx2 = ap[0] - seg.end.x;
                                                var dy2 = ap[1] - seg.end.y;
                                                if ((dx2 * dx2 + dy2 * dy2) <= fbTol2) {
                                                    suppressCornerBecauseConnectedBlue = true;
                                                    try { addDebug('[Emory] Fallback suppression: found blue pathItem endpoint near seg.end at index ' + pi + ', point ' + ppi); } catch (eDbg2) { }
                                                    break;
                                                }
                                            }
                                        } catch (ePts) { }
                                        if (suppressCornerBecauseConnectedBlue) break;
                                    }
                                }
                            } catch (eFallback) { }

                            // Existing behavior: create full corner for near-90-degree turns (if not suppressed)
                            if (nextAngle >= 80 && nextAngle <= 100) {
                                if (!suppressCornerBecauseConnectedBlue) {
                                    createCorner = true;
                                }
                            }
                            var nextSegmentAngle = Math.atan2(nextSeg.direction.y, nextSeg.direction.x) * (180 / Math.PI);
                            nextIsDiagonal = isDiagonalAngle(nextSegmentAngle, orthogonalAngle);

                            // New behavior: for BLUE ductwork, create a small curved connector for
                            // substantive turns so rectangles visually connect. We don't require
                            // the next segment to be non-diagonal here to catch mixed cases
                            // (e.g. a ~40° segment meeting a 90° segment).
                            if (isBlueDuctwork && !createCorner && !suppressCornerBecauseConnectedBlue) {
                                // Broadened: include more turn angles so small connectors trigger
                                if (nextAngle > 10 && nextAngle < 170) {
                                    createCorner = true;
                                    smallCornerOverride = true;
                                    addDebug('[Emory] Blue small-corner candidate at segIdx=' + segIdx + ', turnAngle=' + Math.round(nextAngle));
                                }
                            }
                        }

                        // determine corner gap to use for this junction (smaller for small blue corners)
                        var cornerGapToUse = smallCornerOverride ? Math.max(scaledGapDistance * 0.35, 1) : scaledGapDistance;
                        var effectiveLength = seg.length - offsetDistance;

                        var segmentBodyLength;
                        if (nextIsDiagonal) {
                            segmentBodyLength = effectiveLength;
                        } else {
                            // use cornerGapToUse so tail/body endpoints align with corner geometry
                            segmentBodyLength = Math.max(effectiveLength - cornerGapToUse, 0);
                        }

                        // Get pre-calculated widths for this segment
                        var segWidth = segmentWidths[segIdx];
                        var bodyStartWidth, bodyEndWidth;

                        // For first segment on main trunk (not branches), use pre-calculated widths
                        if (segWidth.isFirst && !isBranch) {
                            bodyStartWidth = segWidth.start;
                            bodyEndWidth = segWidth.end;
                            localCurrentStartWidth = bodyStartWidth; // Initialize for first segment
                        } else {
                            // For inner segments, last segments, and first segment of branches: use progressive tapering
                            bodyStartWidth = localCurrentStartWidth;
                            bodyEndWidth = localCurrentStartWidth;
                        }

                        if (segmentBodyLength > epsilon) {
                            var bodyEndPoint;
                            if (createCorner && nextSeg) {
                                // end body explicitly at corner start so it meets the corner connector
                                bodyEndPoint = add(seg.end, scale(seg.direction, -cornerGapToUse));
                            } else {
                                bodyEndPoint = lerpPoint(segmentStart, seg.end, segmentBodyLength / effectiveLength);
                            }
                            var bodyRectOptions = startEdgeOverride ? { startEdgeVector: startEdgeOverride } : null;
                            var bodyPath = buildRectangle(
                                path,
                                segmentStart,
                                bodyEndPoint,
                                seg.normal,
                                bodyStartWidth,
                                bodyStartWidth,
                                placementAfter,
                                bodyRectOptions
                            );
                            alignAppearance(bodyPath, path);
                            segmentCounter++;
                            var pathTypeBody = isActualBlueBranch ? "Branch" : "Main Trunk";
                            var coordId = "@[" + Math.round(segmentStart.x) + "," + Math.round(segmentStart.y) + "]";
                            bodyPath.name = pathTypeBody + " - Seg" + segmentCounter + " Body " + coordId;
                            try { addDebug('[Emory] Created ' + bodyPath.name + ' at segIdx=' + segIdx + ', end=[' + Math.round(bodyEndPoint.x) + ',' + Math.round(bodyEndPoint.y) + '], width=' + Math.round(bodyStartWidth * 2)); } catch (eDbg) { }
                            placementAfter = bodyPath;
                            rectangles.push(bodyPath);
                            nonCornerShapes.push(bodyPath);
                            aggregateDiagCounts.bodyRects++;
                        }

                        if (!createCorner && !nextIsDiagonal) {
                            var isLastSegment = (segIdx === localSegments.length - 1);
                            var isFirstSegment = (segIdx === 0);

                            // For blue ductwork with branch-like behavior (actual branches OR trunks connected to green):
                            // skip tail at first segment (connection point)
                            // This prevents creating a cap-like rectangle at the T-connection
                            var skipBlueBranchFirstTail = false;

                            if (!skipBlueBranchFirstTail) {
                                var tailStartPoint = segmentBodyLength > epsilon
                                    ? lerpPoint(segmentStart, seg.end, segmentBodyLength / effectiveLength)
                                    : segmentStart;
                                var currentEndWidth;

                                if (isFirstSegment && !isBranch) {
                                    // First segment on main trunk already has correct end width
                                    currentEndWidth = noTaper ? bodyStartWidth : bodyEndWidth;
                                } else if (isLastSegment) {
                                    // Last segment: taper toward endpoint
                                    // For branches with custom width, this tapers FROM the custom start width
                                    currentEndWidth = noTaper ? localCurrentStartWidth : localCurrentStartWidth * taperScale;
                                } else {
                                    // Inner segments (and first segment of branches): progressive taper
                                    // For branches with custom width, localCurrentStartWidth starts at the custom width
                                    // and each segment tapers progressively from there
                                    currentEndWidth = localCurrentStartWidth * taperScale;
                                }
                                var tailOptions = null;
                                if (startEdgeOverride && (segmentBodyLength <= epsilon)) {
                                    tailOptions = { startEdgeVector: startEdgeOverride };
                                }
                                var tailPath = buildRectangle(
                                    path,
                                    tailStartPoint,
                                    seg.end,
                                    seg.normal,
                                    bodyStartWidth,
                                    currentEndWidth,
                                    placementAfter,
                                    tailOptions
                                );
                                alignAppearance(tailPath, path);
                                var pathTypeTail = isActualBlueBranch ? "Branch" : "Main Trunk";
                                var coordIdTail = "@[" + Math.round(tailStartPoint.x) + "," + Math.round(tailStartPoint.y) + "]";
                                tailPath.name = pathTypeTail + " - Seg" + segmentCounter + " Tail " + coordIdTail;
                                try { addDebug('[Emory] Created ' + tailPath.name + ' at segIdx=' + segIdx + ', end=[' + Math.round(seg.end.x) + ',' + Math.round(seg.end.y) + '], startWidth=' + Math.round(bodyStartWidth * 2) + ', endWidth=' + Math.round(currentEndWidth * 2)); } catch (eDbg) { }
                                placementAfter = tailPath;
                                rectangles.push(tailPath);
                                nonCornerShapes.push(tailPath);
                                localCurrentStartWidth = currentEndWidth;
                                aggregateDiagCounts.tailRects++;
                                finalEndWidth = localCurrentStartWidth;
                            } else {
                                try { addDebug('[Emory] Skipping tail at segIdx=' + segIdx + ' for blue branch first segment (prevents cap at connection)'); } catch (eDbg) { }
                                // For blue branches at first segment, maintain the body width for next segment
                                localCurrentStartWidth = bodyStartWidth;
                            }
                        }

                        if (createCorner && nextSeg) {
                            // Allow a reduced gap for small blue-turn connectors so the curve is tighter
                            // cornerGapToUse already computed above
                            var cornerPath = buildCornerRectangle(
                                path,
                                seg,
                                nextSeg,
                                localCurrentStartWidth,
                                cornerGapToUse,
                                placementAfter
                            );
                            try { cornerPath.filled = true; } catch (e) { }
                            alignAppearance(cornerPath, path);
                            var pathTypeCorner = isActualBlueBranch ? "Branch" : "Main Trunk";
                            var coordIdCorner = "@[" + Math.round(seg.end.x) + "," + Math.round(seg.end.y) + "]";
                            cornerPath.name = pathTypeCorner + " - Corner" + (segmentCounter + 1) + " " + coordIdCorner;
                            try { addDebug('[Emory] Created ' + cornerPath.name + ' (blue small-corner=' + (!!smallCornerOverride) + ') at segIdx=' + segIdx + ', turnAngle=' + Math.round(nextAngle) + ', width=' + Math.round(localCurrentStartWidth * 2)); } catch (eDbg) { }
                            placementAfter = cornerPath;
                            rectangles.push(cornerPath);
                            cornerShapes.push(cornerPath);
                            aggregateDiagCounts.cornerRects++;
                        }

                        // store previous corner gap so the next segment is trimmed correctly
                        localPrevHadCorner = createCorner;
                        localPrevCornerGap = createCorner ? cornerGapToUse : 0;
                        if (localCurrentStartWidth < epsilon) break;
                    }
                }

                if (isBranch && branchConnection && branchConnection.multiSide) {
                    startAnchorPoint = clonePoint({ x: branchConnection.connectionPoint.x, y: branchConnection.connectionPoint.y });
                    var farthestPt = startAnchorPoint ? clonePoint(startAnchorPoint) : null;
                    var farthestDist2 = -1;
                    for (var seqScan = 0; seqScan < anchorSequences.length; seqScan++) {
                        var seqAnchors = anchorSequences[seqScan].anchors;
                        for (var sa = 0; sa < seqAnchors.length; sa++) {
                            var candidate = seqAnchors[sa];
                            var dx = candidate.x - startAnchorPoint.x;
                            var dy = candidate.y - startAnchorPoint.y;
                            var dist2 = dx * dx + dy * dy;
                            if (dist2 > farthestDist2) {
                                farthestDist2 = dist2;
                                farthestPt = clonePoint(candidate);
                            }
                        }
                    }
                    if (farthestPt) {
                        endAnchorPoint = farthestPt;
                    }
                }

                for (var seqIdx = 0; seqIdx < anchorSequences.length; seqIdx++) {
                    var seqEntry = anchorSequences[seqIdx];
                    processAnchorSequence(seqEntry.anchors, seqEntry.preserveLast);
                }

                // DISABLED: End caps for blue ductwork at connection points
                // Blue branches now connect directly to trunk without caps (like green ductwork)
                // if (isBlueDuctwork && isBranch && branchConnection && segments.length > 0) {
                //     var connectionPoint = { x: branchConnection.connectionPoint.x, y: branchConnection.connectionPoint.y };
                //     var firstSeg = segments[0];
                //
                //     // Determine source (main path) half-width and destination (branch first segment) half-width
                //     var sourceHalf = baseHalfWidth; // after inheritance attempt above, baseHalfWidth is likely the source
                //     var destHalf = baseHalfWidth; // fallback
                //     try {
                //         // Try to get a better destination half-width from the first generated tail/body rect (if any)
                //         if (rectangles && rectangles.length > 0) {
                //             var lastRect = rectangles[rectangles.length - 1];
                //             try { destHalf = calculateRectangleHalfWidth(lastRect); } catch (eLast) { destHalf = baseHalfWidth; }
                //         }
                //     } catch (eDest) {}
                //
                //     // Cap length along the segment = destination full width (2 * destHalf)
                //     var capHalfAlong = destHalf;
                //     // Cap across-normal half-width = source half-width so it visually matches the source piece
                //     var capAcrossHalf = sourceHalf;
                //
                //     // Compute cap start/end centered about connectionPoint along the segment direction
                //     var capStart = add(connectionPoint, scale(firstSeg.direction, -capHalfAlong));
                //     var capEnd = add(connectionPoint, scale(firstSeg.direction, capHalfAlong));
                //
                //     var endCapPath = buildRectangle(
                //         path,
                //         capStart,
                //         capEnd,
                //         firstSeg.normal,
                //         capAcrossHalf,
                //         capAcrossHalf,
                //         placementAfter
                //     );
                //     alignAppearance(endCapPath, path);
                //     endCapPath.name = "Branch - End Cap";
                //     placementAfter = endCapPath;
                //     rectangles.push(endCapPath);
                //     try { addDebug('[Emory] Created branch end cap with sourceHalf=' + sourceHalf + ', destHalf=' + destHalf); } catch (eDbgCap) {}
                // }

                addDebug(
                    "[Emory] Path '" + pathNameForDebug + "' summary: segments=" + aggregateDiagCounts.segmentsTotal +
                    ", sCurves=" + aggregateDiagCounts.sCurves +
                    ", bodies=" + aggregateDiagCounts.bodyRects +
                    ", tails=" + aggregateDiagCounts.tailRects +
                    ", corners=" + aggregateDiagCounts.cornerRects +
                    ", diagSegments=" + aggregateDiagCounts.diagonalSegments +
                    ", skippedShort=" + aggregateDiagCounts.skippedShortSegments +
                    ", rectanglesTotal=" + rectangles.length +
                    "."
                );

                var orderedShapes = rectangles.slice();
                if (nonCornerShapes.length > 1) {
                    var orderedNonCorner = nonCornerShapes.slice().reverse();
                    orderedShapes = orderedNonCorner.concat(cornerShapes);
                } else if (nonCornerShapes.length === 1 || cornerShapes.length) {
                    orderedShapes = nonCornerShapes.slice().concat(cornerShapes);
                }

                if (!orderedShapes.length) {
                    addDebug(
                        "[Emory] No rectangles generated for path '" + pathNameForDebug + "'. totalWidth=" + totalWidth +
                        ", gapDistance=" + gapDistance + ", branch=" + isBranch +
                        ", anchors=" + anchors.length + "."
                    );
                }

                if (rectangles.length > 1) {
                    rectangles.reverse();
                }

                var metricsResult = {
                    startHalfWidth: baseHalfWidth,
                    endHalfWidth: finalEndWidth,
                    startPoint: startAnchorPoint,
                    endPoint: endAnchorPoint,
                    anchorsReversed: anchorsReversed,
                    forceStartAtEndApplied: forcedStartApplied,
                    lastSegmentAngle: finalSegmentAngle,
                    lastSegmentOriginalDiagonal: finalSegmentOriginalDiagonal,
                    lastSegmentTreatedAsDiagonal: finalSegmentTreatedAsDiagonal,
                    branchStartFullWidth: branchStartFullWidth
                };

                return {
                    shapes: orderedShapes,
                    metrics: metricsResult,
                    cornerShapes: cornerShapes
                };
            }

            return {
                deleteRectanglesFromGroup: deleteRectanglesFromGroup,
                extractCenterlinesFromGroup: extractCenterlinesFromGroup,
                convertConnectionsToRectangleFormat: convertConnectionsToRectangleFormat,
                processLine: processLine,
                alignAppearance: alignAppearance,
                calculateRectangleHalfWidth: calculateRectangleHalfWidth
            };
        })();

        // Removed: runEmoryGeometry function (960 lines) - Emory mode functionality stripped

        function clearOrthoLock(pathItem) {
            if (!pathItem) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i] === ORTHO_LOCK_TAG) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(pathItem, filtered);
        }

        function createDarkDialogWindow(title) {
            if (typeof Window === "undefined") return null;
            var dlg = new Window("dialog", title || "Rotation Override");
            dlg.orientation = "column";
            dlg.alignChildren = "fill";
            dlg.spacing = 12;
            dlg.margins = 18;
            try {
                var g = dlg.graphics;
                g.backgroundColor = g.newBrush(g.BrushType.SOLID_COLOR, [0.12, 0.12, 0.12]);
            } catch (e) { }
            return dlg;
        }

        function setStaticTextColor(control, rgbArray) {
            if (!control || !control.graphics) return;
            try {
                var g = control.graphics;
                var color = rgbArray || [0.85, 0.85, 0.85];
                g.foregroundColor = g.newPen(g.PenType.SOLID_COLOR, color, 1);
            } catch (e) { }
        }

        function showRotationOverrideDialog(defaultValue) {
            var instructions = "Rotation override in degrees for selected paths:";
            var hintText = "Leave blank to keep current values. Use Clear to reset to 0 degrees.";
            if (typeof Window === "undefined") {
                var fallback = prompt(instructions + " (blank = keep, enter 0 for zero override)", defaultValue);
                if (fallback === null) return { action: "cancel", value: "" };
                var fallbackTrimmed = trimString(fallback);
                if (fallbackTrimmed.length === 0) return { action: "blank", value: "" };
                if (/^clear$/i.test(fallbackTrimmed)) return { action: "value", value: "0" };
                var fallbackParsed = parseFloat(fallbackTrimmed);
                if (isNaN(fallbackParsed)) {
                    return null;
                }
                return { action: "value", value: fallbackTrimmed };
            }

            var dlg = createDarkDialogWindow("Rotation Override");
            if (!dlg) return null;

            var message = dlg.add("statictext", undefined, instructions, { multiline: true });
            message.alignment = "fill";
            setStaticTextColor(message);

            var hint = dlg.add("statictext", undefined, hintText, { multiline: true });
            hint.alignment = "fill";
            setStaticTextColor(hint, [0.65, 0.65, 0.65]);

            var input = dlg.add("edittext", undefined, (defaultValue !== undefined && defaultValue !== null) ? defaultValue : "");
            input.characters = 24;
            input.active = true;

            var errorText = dlg.add("statictext", undefined, " ", { multiline: true });
            errorText.alignment = "fill";
            errorText.visible = false;
            setStaticTextColor(errorText, [0.95, 0.35, 0.35]);

            input.onChanging = function () {
                if (errorText.visible) {
                    errorText.visible = false;
                    errorText.text = " ";
                }
            };

            var buttonGroup = dlg.add("group");
            buttonGroup.alignment = "right";
            buttonGroup.spacing = 8;
            var cancelBtn = buttonGroup.add("button", undefined, "Cancel", { name: "cancel" });
            var clearBtn = buttonGroup.add("button", undefined, "Clear");
            var processBtn = buttonGroup.add("button", undefined, "Process Ductwork", { name: "ok" });

            var revertGroup = dlg.add("group");
            revertGroup.alignment = "fill";
            revertGroup.margins = 0;
            revertGroup.orientation = "row";
            revertGroup.alignChildren = ["fill", "center"];
            var revertBtn = revertGroup.add("button", undefined, "Revert to Lines");
            revertBtn.alignment = "fill";

            dlg.defaultElement = processBtn;
            dlg.cancelElement = cancelBtn;

            var finalResult = null;

            function setResult(action, value) {
                finalResult = { action: action, value: value || "" };
            }

            function validateAndClose() {
                var raw = trimString(input.text);
                if (raw.length === 0) {
                    setResult("blank", "");
                    dlg.close(1);
                    return;
                }
                if (/^clear$/i.test(raw)) {
                    setResult("clear", "");
                    dlg.close(1);
                    return;
                }
                var parsed = parseFloat(raw);
                if (isNaN(parsed)) {
                    errorText.text = "Enter a numeric angle or choose Clear.";
                    errorText.visible = true;
                    return;
                }
                setResult("value", raw);
                dlg.close(1);
            }

            processBtn.onClick = validateAndClose;
            clearBtn.onClick = function () {
                input.text = "0";
                setResult("value", "0");
                dlg.close(1);
            };
            cancelBtn.onClick = function () {
                setResult("cancel", "");
                dlg.close(0);
            };
            revertBtn.onClick = function () {
                setResult("revert", "");
                dlg.close(1);
            };

            var response = dlg.show();
            if (!finalResult) return null;
            if (finalResult.action === "cancel") return finalResult;
            if (response !== 1 && finalResult.action !== "value") return finalResult;
            return finalResult;
        }

        function handleRotationOverridePrompt(paths) {
            if (!paths || paths.length === 0) return true;
            var defaultVal = '';
            for (var i = 0; i < paths.length; i++) {
                var existing = getRotationOverride(paths[i]);
                if (existing !== null) {
                    defaultVal = existing;
                    break;
                }
            }
            var dialogResult = showRotationOverrideDialog(defaultVal);
            if (!dialogResult) return true;
            if (dialogResult.action === "cancel") return false;
            if (dialogResult.action === "blank") return true;
            if (dialogResult.action === "clear") {
                dialogResult.value = "0";
            }
            if (dialogResult.action === "revert") {
                revertSelectionToLines(originalSelectionItems);
                return "revert";
            }
            var trimmed = trimString(dialogResult.value);
            if (trimmed.length === 0) return true;
            var parsed = parseFloat(trimmed);
            if (isNaN(parsed)) {
                return true;
            }
            var normalized = normalizeAngle(parsed);
            for (var s = 0; s < paths.length; s++) {
                setRotationOverride(paths[s], normalized);
                clearOrthoLock(paths[s]);
            }
            return true;
        }

        function setPointRotation(pathItem, angleDeg) {
            if (!pathItem) return;
            if (!isFinite(angleDeg)) {
                clearPointRotation(pathItem);
                return;
            }
            var tokens = readNoteTokens(pathItem);
            var tag = POINT_ROT_PREFIX + normalizeAngle(angleDeg);
            var replaced = false;
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(POINT_ROT_PREFIX) === 0) {
                    tokens[i] = tag;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) tokens.push(tag);
            writeNoteTokens(pathItem, tokens);
        }

        function clearPointRotation(pathItem) {
            if (!pathItem) return;
            var tokens = readNoteTokens(pathItem);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(POINT_ROT_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(pathItem, filtered);
        }

        function getPointRotation(pathItem) {
            var tokens = readNoteTokens(pathItem);
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(POINT_ROT_PREFIX) === 0) {
                    var raw = tokens[i].substring(POINT_ROT_PREFIX.length);
                    var val = parseFloat(raw);
                    if (!isNaN(val)) return normalizeAngle(val);
                }
            }
            return null;
        }

        function hasIgnoreNote(pathItem) {
            try {
                if (!pathItem || typeof pathItem.note !== 'string') return false;
                return pathItem.note.toLowerCase().indexOf('ignore') !== -1;
            } catch (e) {
                return false;
            }
        }

        function hasOrthoLock(pathItem) {
            try {
                return pathItem && typeof pathItem.note === 'string' && pathItem.note.indexOf(ORTHO_LOCK_TAG) !== -1;
            } catch (e) {
                return false;
            }
        }

        function hasNoOrthoNote(pathItem) {
            try {
                if (!pathItem || typeof pathItem.note !== 'string') return false;
                return pathItem.note.indexOf('MD:NO_ORTHO') !== -1;
            } catch (e) {
                return false;
            }
        }

        function setNoOrthoNote(pathItem, enable) {
            if (!pathItem) return;
            try {
                var note = pathItem.note || '';
                if (enable) {
                    // Add MD:NO_ORTHO if not present
                    if (note.indexOf('MD:NO_ORTHO') === -1) {
                        pathItem.note = note && note.length > 0 ? (note + '|MD:NO_ORTHO') : 'MD:NO_ORTHO';
                    }
                } else {
                    // Remove MD:NO_ORTHO if present
                    if (note.indexOf('MD:NO_ORTHO') !== -1) {
                        var tokens = note.split('|');
                        var filtered = [];
                        for (var i = 0; i < tokens.length; i++) {
                            if (tokens[i] !== 'MD:NO_ORTHO' && tokens[i].length > 0) {
                                filtered.push(tokens[i]);
                            }
                        }
                        pathItem.note = filtered.join('|');
                    }
                }
            } catch (e) { }
        }

        function checkSkipOrthoState(selectionItems) {
            var hasWithNote = false;
            var hasWithoutNote = false;

            forEachPathInItems(selectionItems, function (pathItem) {
                if (!pathItem) return;
                var layerName = "";
                try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (e) { }
                if (layerName && isDuctworkLineLayer(layerName)) {
                    if (hasNoOrthoNote(pathItem)) {
                        hasWithNote = true;
                    } else {
                        hasWithoutNote = true;
                    }
                }
            });

            return {
                hasNote: hasWithNote && !hasWithoutNote, // All have note
                mixed: hasWithNote && hasWithoutNote     // Some have, some don't
            };
        }

        function flagOrthoLock(pathItem) {
            try {
                if (!pathItem) return;
                var note = '';
                try { note = pathItem.note; } catch (e) { note = ''; }
                if (typeof note !== 'string') note = '';
                if (note.indexOf(ORTHO_LOCK_TAG) === -1) {
                    pathItem.note = note && note.length > 0 ? (note + '|' + ORTHO_LOCK_TAG) : ORTHO_LOCK_TAG;
                }
            } catch (e) { }
        }

        function isSteepAngle(angleDeg) {
            angleDeg = (angleDeg + 360) % 360;
            return (
                (angleDeg >= STEEP_MIN && angleDeg <= STEEP_MAX) ||
                (angleDeg >= 107 && angleDeg <= 160) ||
                (angleDeg >= 197 && angleDeg <= 250) ||
                (angleDeg >= 287 && angleDeg <= 340)
            );
        }

        function closestPointOnSegment(a, b, p) {
            var ab = [b[0] - a[0], b[1] - a[1]];
            var ap = [p[0] - a[0], p[1] - a[1]];
            var ab2 = dot(ab, ab);
            if (ab2 === 0) return { pt: a, t: 0 };
            var t = dot(ap, ab) / ab2;
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            return { pt: [a[0] + ab[0] * t, a[1] + ab[1] * t], t: t };
        }

        function cubicAt(p0, p1, p2, p3, t) {
            var mt = 1 - t;
            var mt2 = mt * mt;
            var t2 = t * t;
            var a = mt2 * mt;
            var b = 3 * mt2 * t;
            var c = 3 * mt * t2;
            var d = t * t2;
            return [
                a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
                a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]
            ];
        }

        function closestPointOnCubic(p0, p1, p2, p3, point) {
            var best = { t: 0, pt: p0, dist2: dist2(p0, point) };
            var samples = 12;
            for (var i = 1; i <= samples; i++) {
                var t = i / samples;
                var pt = cubicAt(p0, p1, p2, p3, t);
                var d2 = dist2(pt, point);
                if (d2 < best.dist2) best = { t: t, pt: pt, dist2: d2 };
            }
            var range = 1 / samples;
            for (var iter = 0; iter < 4; iter++) {
                var t0 = Math.max(0, best.t - range);
                var t1 = Math.min(1, best.t + range);
                var improved = false;
                var steps = 5;
                for (var j = 0; j <= steps; j++) {
                    var t = t0 + (t1 - t0) * (j / steps);
                    var pt = cubicAt(p0, p1, p2, p3, t);
                    var d2 = dist2(pt, point);
                    if (d2 < best.dist2) {
                        best = { t: t, pt: pt, dist2: d2 };
                        improved = true;
                    }
                }
                if (!improved) break;
                range *= 0.5;
            }
            return best;
        }

        function segmentsIntersect(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2) {
            var dax = ax2 - ax1, day = ay2 - ay1;
            var dbx = bx2 - bx1, dby = by2 - by1;
            var denom = dax * dby - day * dbx;
            if (Math.abs(denom) < 1e-10) {
                return linesOverlap(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
            }
            var dx = ax1 - bx1, dy = ay1 - by1;
            var t = (dbx * dy - dby * dx) / denom;
            var u = (dax * dy - day * dx) / denom;
            return (t >= 0 && t <= 1 && u >= 0 && u <= 1);
        }

        // Returns intersection point [x, y] or null if no intersection
        function getSegmentIntersectionPoint(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2) {
            var dax = ax2 - ax1, day = ay2 - ay1;
            var dbx = bx2 - bx1, dby = by2 - by1;
            var denom = dax * dby - day * dbx;
            if (Math.abs(denom) < 1e-10) return null; // Parallel or collinear
            var dx = ax1 - bx1, dy = ay1 - by1;
            var t = (dbx * dy - dby * dx) / denom;
            var u = (dax * dy - day * dx) / denom;
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                return [ax1 + t * dax, ay1 + t * day];
            }
            return null;
        }

        function linesOverlap(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2) {
            var cross1 = (ay1 - by1) * (bx2 - bx1) - (ax1 - bx1) * (by2 - by1);
            var cross2 = (ay2 - by1) * (bx2 - bx1) - (ax2 - bx1) * (by2 - by1);
            if (Math.abs(cross1) > 1e-6 || Math.abs(cross2) > 1e-6) return false;
            var primaryAxis = Math.abs(ax2 - ax1) > Math.abs(ay2 - ay1) ? 'x' : 'y';
            var a1, a2, b1, b2;
            if (primaryAxis === 'x') {
                a1 = Math.min(ax1, ax2); a2 = Math.max(ax1, ax2);
                b1 = Math.min(bx1, bx2); b2 = Math.max(bx1, by2);
            } else {
                a1 = Math.min(ay1, ay2); a2 = Math.max(ay1, ay2);
                b1 = Math.min(by1, by2); b2 = Math.max(by1, by2);
            }
            return !(a2 < b1 || b2 < a1);
        }

        function makeBBox(a, b) {
            return {
                minX: Math.min(a[0], b[0]),
                minY: Math.min(a[1], b[1]),
                maxX: Math.max(a[0], b[0]),
                maxY: Math.max(a[1], b[1])
            };
        }
        function expandBBox(b, pad) {
            return {
                minX: b.minX - pad,
                minY: b.minY - pad,
                maxX: b.maxX + pad,
                maxY: b.maxY + pad
            };
        }
        function bboxContainsPoint(b, p) {
            return p[0] >= b.minX && p[0] <= b.maxX && p[1] >= b.minY && p[1] <= b.maxY;
        }

        // --- GLOBAL DEBUG LOG ---
        // DEBUG_MODE: References $.global.MDUX_DEBUG.ENABLED at top of file
        var DEBUG_MODE = $.global.MDUX_DEBUG && $.global.MDUX_DEBUG.ENABLED;
        var GLOBAL_DEBUG_LOG = [];
        function addDebug(msg) {
            // PERFORMANCE: Skip all logging if debug mode is off
            if (!DEBUG_MODE) return;

            // Collect in local memory
            GLOBAL_DEBUG_LOG.push(msg);

            // ALSO write to global buffer that panel can read
            try {
                var timestamp = new Date().toString();
                var logEntry = "[" + timestamp + "] " + msg;
                if (typeof $.global.MDUX_debugBuffer === "undefined") {
                    $.global.MDUX_debugBuffer = [];
                }
                $.global.MDUX_debugBuffer.push(logEntry);
                // Keep only last 1000 entries
                if ($.global.MDUX_debugBuffer.length > 1000) {
                    $.global.MDUX_debugBuffer.shift();
                }
            } catch (eGlobal) { }

            // Also write to file
            try {
                var logFile = new File(getLogFilePath("debug.log"));
                logFile.open("a");
                logFile.writeln("[" + new Date().toISOString() + "] " + msg);
                logFile.close();
            } catch (e) {
                // If file logging fails, at least we have in-memory log
            }
        }
        // Expose addDebug to global scope for PythonBridge IIFE
        $.global.addDebug = addDebug;

        // Diagnostic: Check PythonBridge availability now that addDebug exists
        addDebug("[PYBRIDGE-INIT] Checking PythonBridge availability...");
        addDebug("[PYBRIDGE-INIT] MDUX_USE_PYTHON=" + $.global.MDUX_USE_PYTHON);
        addDebug("[PYBRIDGE-INIT] typeof PythonBridge=" + typeof PythonBridge);
        if (typeof PythonBridge !== 'undefined') {
            addDebug("[PYBRIDGE-INIT] Calling isAvailable()...");
            try {
                var pyAvail = PythonBridge.isAvailable();
                addDebug("[PYBRIDGE-INIT] isAvailable() returned: " + pyAvail);
            } catch (pyErr) {
                addDebug("[PYBRIDGE-INIT] isAvailable() threw error: " + pyErr);
            }
        } else {
            addDebug("[PYBRIDGE-INIT] PythonBridge is undefined!");
        }

        function showDebugDialog() {
            if (GLOBAL_DEBUG_LOG.length === 0) return;

            var dlg = new Window("dialog", "Debug Log");
            dlg.preferredSize = [900, 600];

            var textGroup = dlg.add("group");
            textGroup.orientation = "column";
            textGroup.alignment = ["fill", "fill"];
            textGroup.alignChildren = ["fill", "fill"];

            var debugText = textGroup.add("edittext", undefined, "", { multiline: true, scrolling: true });
            debugText.preferredSize = [880, 550];
            debugText.text = GLOBAL_DEBUG_LOG.join("\n");
            debugText.graphics.font = ScriptUI.newFont("Courier New", "REGULAR", 10);
            debugText.graphics.backgroundColor = debugText.graphics.newBrush(debugText.graphics.BrushType.SOLID_COLOR, [0.1, 0.1, 0.1]);
            debugText.graphics.foregroundColor = debugText.graphics.newPen(debugText.graphics.PenType.SOLID_COLOR, [0.9, 0.9, 0.9], 1);

            var btnGroup = dlg.add("group");
            btnGroup.alignment = ["center", "bottom"];

            var copyBtn = btnGroup.add("button", undefined, "Copy to Clipboard");
            copyBtn.onClick = function () {
                // Copy is not directly supported, but user can select all and Ctrl+C
                debugText.active = true;
                debugText.textselection = [0, debugText.text.length];
            };

            var closeBtn = btnGroup.add("button", undefined, "Close", { name: "ok" });

            dlg.show();
        }

        // --- RESET GLOBAL OPTIONS ---
        SKIP_ALL_BRANCH_ORTHO = false;
        SKIP_FINAL_REGISTER_ORTHO = false;
        SKIP_REGISTER_ROTATION = false;

        // --- PROGRESS DIALOG HELPER ---
        // Creates a progress dialog for large selections to prevent perceived lockup
        var progressWin = null;
        var progressBar = null;
        var progressLabel = null;
        var PROGRESS_STEPS = 12; // Total number of major steps
        var currentStep = 0;

        function initProgress(pathCount) {
            // Only show progress for larger selections
            if (pathCount < 15) return;
            try {
                progressWin = new Window('palette', 'Processing Ductwork...', undefined, { closeButton: false });
                progressWin.orientation = 'column';
                progressWin.alignChildren = ['fill', 'top'];

                progressLabel = progressWin.add('statictext', undefined, 'Initializing...');
                progressLabel.preferredSize = [300, 20];

                progressBar = progressWin.add('progressbar', undefined, 0, PROGRESS_STEPS);
                progressBar.preferredSize = [300, 20];

                var pathInfo = progressWin.add('statictext', undefined, 'Processing ' + pathCount + ' paths');
                pathInfo.preferredSize = [300, 20];

                progressWin.show();
                $.global.MDUX_PROGRESS_WIN = progressWin; // Store globally for Python bridge polling
            } catch (e) {
                progressWin = null; // If window creation fails, continue without progress
                $.global.MDUX_PROGRESS_WIN = null;
            }
        }

        function updateProgress(stepName) {
            currentStep++;
            if (progressWin && progressLabel && progressBar) {
                try {
                    progressLabel.text = stepName;
                    progressBar.value = currentStep;
                    progressWin.update();
                    // REMOVED: app.redraw() and $.sleep() - these were causing 600ms+ delays per step
                } catch (e) { }
            }
            // Only log progress for debugging, skip if performance-critical
            // addDebug("[PROGRESS] Step " + currentStep + "/" + PROGRESS_STEPS + ": " + stepName);
        }

        // Helper function to yield to UI during heavy loops
        // function yieldToUI() {
        //     try {
        //         if (progressWin) progressWin.update();
        //         app.redraw();
        //         $.sleep(30);
        //     } catch (e) { }
        // }

        function closeProgress() {
            if (progressWin) {
                try {
                    progressWin.close();
                } catch (e) { }
                progressWin = null;
                $.global.MDUX_PROGRESS_WIN = null;
            }
        }

        // --- COLLECT PATHS ---
        addDebug("=== MAGIC DUCTWORK STARTING ===");
        addDebug("Selection has " + sel.length + " items");

        var allPaths = [];
        var SELECTED_PATHS = [];
        var CREATED_ANCHOR_PATHS = [];
        var ACTIVE_CENTERLINES = [];
        var CARVE_OUT_COMPOUNDS = []; // Track compound paths created during carve-out for styling

        // Ductwork parts layers to exclude from path collection
        var DUCTWORK_PARTS_LAYERS = {
            "Units": true,
            "Square Registers": true,
            "Rectangular Registers": true,
            "Circular Registers": true,
            "Exhaust Registers": true,
            "Orange Register": true,
            "Secondary Exhaust Registers": true,
            "Thermostats": true
        };

        function walkAndCollect(item) {
            if (!item) return;
            if (item.typename === "GroupItem") {
                for (var i = 0; i < item.pageItems.length; i++) walkAndCollect(item.pageItems[i]);
            } else if (item.typename === "CompoundPathItem") {
                for (var j = 0; j < item.pathItems.length; j++) walkAndCollect(item.pathItems[j]);
            } else if (item.typename === "PathItem") {
                if (!item.guides && !item.clipping) {
                    try {
                        var itemLayerName = item.layer ? item.layer.name : null;
                        var isOnDuctworkPartsLayer = itemLayerName && DUCTWORK_PARTS_LAYERS[itemLayerName];
                        var pointCount = item.pathPoints ? item.pathPoints.length : 0;

                        // Exclude MULTI-POINT paths on ductwork parts layers (component outlines)
                        // But ALLOW 1-POINT paths (anchors) since those are legitimate placement markers
                        if (isOnDuctworkPartsLayer && pointCount > 1) {
                            // Skip component outlines silently for performance
                            return;
                        }

                        allPaths.push(item);
                    } catch (e) {
                        // If we can't get layer name, include the path anyway
                        allPaths.push(item);
                    }
                }
            }
        }
        for (var i = 0; i < sel.length; i++) walkAndCollect(sel[i]);
        SELECTED_PATHS = allPaths.slice();
        addDebug("Collected " + allPaths.length + " paths from selection");
        if (allPaths.length === 0) {
            alert("No valid path items.");
            return;
        }

        // Initialize progress dialog for larger selections
        initProgress(allPaths.length);
        updateProgress("Normalizing strokes...");

        // *** EARLY STROKE NORMALIZATION ***
        // Normalize all selected paths to the smallest stroke width in the selection
        // This ensures consistent stroke widths before any processing begins
        normalizeStrokesToSmallest(allPaths);

        var selectionContext = collectSelectionContext(originalSelectionItems);

        var forcedOptions = null;
        try {
            if ($.global.MDUX && $.global.MDUX.forcedOptions) {
                forcedOptions = $.global.MDUX.forcedOptions;
                delete $.global.MDUX.forcedOptions;
            }
        } catch (forcedErr) {
            forcedOptions = null;
        }

        var startupChoice;
        if (forcedOptions) {
            startupChoice = {
                action: forcedOptions.action || "process",
                mode: "normal",
                rotationOverride: (typeof forcedOptions.rotationOverride === "number" && isFinite(forcedOptions.rotationOverride)) ? forcedOptions.rotationOverride : null,
                skipAllBranchSegments: !!forcedOptions.skipAllBranchSegments,
                skipFinalRegisterSegment: !!forcedOptions.skipFinalRegisterSegment,
                skipRegisterRotation: !!forcedOptions.skipRegisterRotation,
                enableRegisterCarve: !!forcedOptions.enableRegisterCarve,
                enableOverlapCarve: !!forcedOptions.enableOverlapCarve
            };
            if (typeof forcedOptions.skipOrtho === "boolean") {
                var skipValue = !!forcedOptions.skipOrtho;
                startupChoice.skipOrthoState = {
                    checkboxValue: skipValue,
                    initialValue: !skipValue,
                    changed: true
                };
            }
        } else {
            startupChoice = showUnifiedDuctworkDialog(selectionContext, selectionContext.storedSettings);
        }
        if (startupChoice && typeof startupChoice.skipAllBranchSegments !== "undefined") {
            SKIP_ALL_BRANCH_ORTHO = !!startupChoice.skipAllBranchSegments;
        }
        if (startupChoice && typeof startupChoice.skipFinalRegisterSegment !== "undefined") {
            SKIP_FINAL_REGISTER_ORTHO = !!startupChoice.skipFinalRegisterSegment;
        }
        if (startupChoice && typeof startupChoice.skipRegisterRotation !== "undefined") {
            SKIP_REGISTER_ROTATION = !!startupChoice.skipRegisterRotation;
        }
        addDebug("[Register Rotation] SKIP_REGISTER_ROTATION=" + SKIP_REGISTER_ROTATION);

        // Carve-out options (default to false = disabled)
        var ENABLE_REGISTER_CARVE = false;
        var ENABLE_OVERLAP_CARVE = false;
        if (startupChoice && typeof startupChoice.enableRegisterCarve !== "undefined") {
            ENABLE_REGISTER_CARVE = !!startupChoice.enableRegisterCarve;
        }
        if (startupChoice && typeof startupChoice.enableOverlapCarve !== "undefined") {
            ENABLE_OVERLAP_CARVE = !!startupChoice.enableOverlapCarve;
        }
        addDebug("[Carve-Out] ENABLE_REGISTER_CARVE=" + ENABLE_REGISTER_CARVE + ", ENABLE_OVERLAP_CARVE=" + ENABLE_OVERLAP_CARVE);
        // Enforce mutual exclusivity: skipFinal takes precedence over skipAll
        if (SKIP_FINAL_REGISTER_ORTHO && SKIP_ALL_BRANCH_ORTHO) {
            addDebug("[Skip Ortho] WARNING: Both skip options were true! Setting SKIP_ALL_BRANCH_ORTHO=false");
            SKIP_ALL_BRANCH_ORTHO = false;
        }
        addDebug("[Skip Ortho Settings] SKIP_ALL_BRANCH_ORTHO=" + SKIP_ALL_BRANCH_ORTHO + ", SKIP_FINAL_REGISTER_ORTHO=" + SKIP_FINAL_REGISTER_ORTHO);
        if (!startupChoice || startupChoice.action === "cancel") {
            return;
        }
        if (startupChoice.action === "library") {
            try { registerMDUXExports(); } catch (eReg) { }
            return;
        }
        if (startupChoice.action === "flipOnly") {
            return;
        }
        if (startupChoice.action === "revert") {
            revertSelectionToLines(originalSelectionItems);
            return;
        }
        if (startupChoice.action === "hide") {
            toggleCenterlineVisibility(originalSelectionItems, false);
            return;
        }
        if (startupChoice.action === "show") {
            toggleCenterlineVisibility(originalSelectionItems, true);
            return;
        }

        if (startupChoice.branchWidthAssignments && startupChoice.branchWidthAssignments.centerlineIds && startupChoice.branchWidthAssignments.centerlineIds.length) {
            var branchAssign = startupChoice.branchWidthAssignments;
            var assignedWidth = parseFloat(branchAssign.width);
            if (!isFinite(assignedWidth) || assignedWidth <= 0) {
                assignedWidth = selectionContext && selectionContext.storedSettings && selectionContext.storedSettings.width ? selectionContext.storedSettings.width : 20;
                if (!isFinite(assignedWidth) || assignedWidth <= 0) assignedWidth = 20;
            }
            var uniqueAssignMap = {};
            var limitAssignMap = {};
            for (var bai = 0; bai < branchAssign.centerlineIds.length; bai++) {
                var assignId = branchAssign.centerlineIds[bai];
                if (!assignId || uniqueAssignMap[assignId]) continue;
                uniqueAssignMap[assignId] = true;
                limitAssignMap[assignId] = true;
                var targetPaths = findEmoryCenterlinesByIds([assignId]);
                for (var tp = 0; tp < targetPaths.length; tp++) {
                    var branchPath = targetPaths[tp];
                    if (!branchPath) continue;
                    setBranchStartWidthOnPath(branchPath, assignedWidth);
                    setBranchCustomStartWidth(assignId, assignedWidth);
                }
                var branchGroup = findEmoryGroupForCenterlineGlobal(assignId);
                if (branchGroup) {
                    var existingSettings = loadSettingsFromGroup(branchGroup) || { centerlineIds: [assignId] };
                    existingSettings.branchStart = assignedWidth;
                    existingSettings.branch = true;
                    if (!existingSettings.centerlineIds || existingSettings.centerlineIds.length === 0) {
                        existingSettings.centerlineIds = [assignId];
                    }
                    saveSettingsToGroup(branchGroup, existingSettings);
                }
            }
            // Check if limitAssignMap has any keys (ExtendScript doesn't support Object.keys)
            var hasLimitKeys = false;
            for (var key in limitAssignMap) {
                if (limitAssignMap.hasOwnProperty(key)) {
                    hasLimitKeys = true;
                    break;
                }
            }
            if (hasLimitKeys) {
                LIMIT_BRANCH_PROCESS_MAP = limitAssignMap;
            } else {
                LIMIT_BRANCH_PROCESS_MAP = null;
            }
        }

        var RUN_MODE = "normal"; // Always normal mode (Emory functionality removed)

        // Handle MD:NO_ORTHO note application/removal based on checkbox state
        // This MUST happen BEFORE any processing so the note is respected during orthogonalization
        if (startupChoice.skipOrthoState && startupChoice.skipOrthoState.changed) {
            var shouldAddNote = startupChoice.skipOrthoState.checkboxValue;
            forEachPathInItems(originalSelectionItems, function (pathItem) {
                if (!pathItem) return;
                var layerName = "";
                try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (e) { }
                if (layerName && isDuctworkLineLayer(layerName)) {
                    setNoOrthoNote(pathItem, shouldAddNote);
                }
            });
        }
        // If checkbox was not changed, do nothing - existing notes will be respected automatically

        function isPathInList(list, path) {
            if (!list || !path) return false;
            for (var i = 0; i < list.length; i++) {
                try {
                    // Validate both objects are still valid before comparing
                    if (list[i] && list[i].typename && path && path.typename) {
                        if (list[i] === path) return true;
                    }
                } catch (e) {
                    // Object is invalid (e.g., deleted by nuclear deletion), skip it
                    continue;
                }
            }
            return false;
        }

        function isPathSelected(path) {
            if (isPathInList(SELECTED_PATHS, path)) return true;
            // Also check if path's parent compound is in SELECTED_PATHS
            // (handles paths inside compound paths created by AUTO-CARVE)
            try {
                if (path && path.parent && path.parent.typename === 'CompoundPathItem') {
                    if (isPathInList(SELECTED_PATHS, path.parent)) return true;
                }
            } catch (e) { }
            return false;
        }

        function isPathCreated(path) {
            return isPathInList(CREATED_ANCHOR_PATHS, path);
        }

        function markCreatedPath(path) {
            if (!path) return;
            if (!isPathCreated(path)) CREATED_ANCHOR_PATHS.push(path);
        }

        function shouldProcessPath(path) {
            if (!path) return false;
            if (isPathSelected(path) || isPathCreated(path)) return true;
            return false;
        }

        function filterPathsToSelected(paths) {
            var result = [];
            if (!paths) return result;
            for (var i = 0; i < paths.length; i++) {
                var candidate = paths[i];
                if (!candidate) continue;
                if (isPathSelected(candidate)) {
                    result.push(paths[i]);
                }
            }
            return result;
        }

        function filterPathsToProcessable(paths) {
            var result = [];
            if (!paths) return result;
            for (var i = 0; i < paths.length; i++) {
                if (shouldProcessPath(paths[i])) result.push(paths[i]);
            }
            return result;
        }

        function filterPathsToCreated(paths) {
            var result = [];
            if (!paths) return result;
            for (var i = 0; i < paths.length; i++) {
                var candidate = paths[i];
                if (!candidate) continue;
                if (isPathCreated(candidate)) result.push(candidate);
            }
            return result;
        }

        // Removed: Emory style target tracking - not needed in normal mode
        function clearEmoryStyleTargetMarks() {
            // No-op: Emory functionality removed
            return;
        }

        function markEmoryTargetsFromSelection() {
            // No-op: Emory functionality removed
            return;
        }

        function isItemMarkedForEmoryStyle(item) {
            // Always false: Emory functionality removed
            return false;
        }

        function findExistingAnchorPath(layer, position, tolerance) {
            if (!layer || !position) return null;
            tolerance = (typeof tolerance === 'number' && tolerance >= 0) ? tolerance : CLOSE_DIST;
            var tol2 = tolerance * tolerance;
            try {
                var items = layer.pathItems;
                for (var i = 0; i < items.length; i++) {
                    var candidate = items[i];
                    if (!candidate || candidate.pathPoints.length !== 1) continue;
                    var anchor = candidate.pathPoints[0].anchor;
                    var dx = anchor[0] - position[0];
                    var dy = anchor[1] - position[1];
                    if (dx * dx + dy * dy <= tol2) return candidate;
                }
            } catch (e) { }
            return null;
        }

        function ensureAnchorTagged(layer, position, rotationOverride, tolerance) {
            var existing = findExistingAnchorPath(layer, position, tolerance);
            if (!existing) return null;
            if (typeof rotationOverride === 'number' && isFinite(rotationOverride)) {
                setPointRotation(existing, rotationOverride);
            } else {
                clearPointRotation(existing);
            }
            markCreatedPath(existing);
            return existing;
        }

        function computePlacedPrimaryAngle(item) {
            if (!item) return 0;
            try {
                var m = item.matrix;
                return Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
            } catch (e) {
                return 0;
            }
        }

        function ensurePlacedBaseRotation(item) {
            var base = getPlacedBaseRotation(item);
            if (base === null || !isFinite(base)) {
                base = computePlacedPrimaryAngle(item);
                setPlacedBaseRotation(item, base);
            }
            return base;
        }

        function getPlacedRotationDelta(item) {
            if (!item) return null;
            var tokens = readNoteTokens(item);
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_ROT_PREFIX) === 0) {
                    var raw = tokens[i].substring(PLACED_ROT_PREFIX.length);
                    var val = parseFloat(raw);
                    if (!isNaN(val)) return normalizeSignedAngleValue(val);
                }
            }
            return null;
        }

        function getPlacedRotation(item) {
            var delta = getPlacedRotationDelta(item);
            var base = getPlacedBaseRotation(item);
            if (delta === null) {
                if (base === null || !isFinite(base)) return null;
                return normalizeAngle(base);
            }
            if (base === null || !isFinite(base)) base = 0;
            return normalizeAngle(base + delta);
        }

        function setPlacedRotation(item, angleDeg) {
            if (!item) return;
            var base = ensurePlacedBaseRotation(item);
            if (base === null || !isFinite(base)) base = 0;
            var delta = normalizeSignedAngleValue(angleDeg - base);
            setPlacedRotationDelta(item, delta);
        }

        function clearPlacedRotation(item) {
            if (!item) return;
            var tokens = readNoteTokens(item);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_ROT_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(item, filtered);
        }

        function getPlacedScale(item) {
            if (!item) return null;
            var tokens = readNoteTokens(item);
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_SCALE_PREFIX) === 0) {
                    var raw = tokens[i].substring(PLACED_SCALE_PREFIX.length);
                    var val = parseFloat(raw);
                    if (!isNaN(val) && val > 0) return val;
                }
            }
            return null;
        }

        function setPlacedScale(item, scalePercent) {
            if (!item) return;
            var tokens = readNoteTokens(item);
            var tag = PLACED_SCALE_PREFIX + scalePercent.toFixed(2);
            var replaced = false;
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_SCALE_PREFIX) === 0) {
                    tokens[i] = tag;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) tokens.push(tag);
            writeNoteTokens(item, tokens);
        }

        function clearPlacedScale(item) {
            if (!item) return;
            var tokens = readNoteTokens(item);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_SCALE_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(item, filtered);
        }

        // MDUX_getMetadata and MDUX_setMetadata - redirect to early-defined _inner versions
        function MDUX_getMetadata(item) {
            return MDUX_getMetadata_inner(item);
        }

        function MDUX_setMetadata(item, metadata) {
            return MDUX_setMetadata_inner(item, metadata);
        }

        function getPlacedBaseRotation(item) {
            if (!item) return null;
            var tokens = readNoteTokens(item);
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_BASE_ROT_PREFIX) === 0) {
                    var raw = tokens[i].substring(PLACED_BASE_ROT_PREFIX.length);
                    var val = parseFloat(raw);
                    if (!isNaN(val)) return val;
                }
            }
            return null;
        }

        function setPlacedBaseRotation(item, angleDeg) {
            if (!item) return;
            var tokens = readNoteTokens(item);
            var tag = PLACED_BASE_ROT_PREFIX + angleDeg.toFixed(6);
            var replaced = false;
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_BASE_ROT_PREFIX) === 0) {
                    tokens[i] = tag;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) tokens.push(tag);
            writeNoteTokens(item, tokens);
        }

        function clearPlacedBaseRotation(item) {
            if (!item) return;
            var tokens = readNoteTokens(item);
            var filtered = [];
            for (var i = 0; i < tokens.length; i++) {
                if (tokens[i].toUpperCase().indexOf(PLACED_BASE_ROT_PREFIX) === 0) continue;
                filtered.push(tokens[i]);
            }
            writeNoteTokens(item, filtered);
        }

        // Handle rotation override from unified dialog (if provided)
        var GLOBAL_ROTATION_OVERRIDE = null;
        if (startupChoice.rotationOverride !== null && startupChoice.rotationOverride !== undefined) {
            var normalized = normalizeAngle(startupChoice.rotationOverride);
            GLOBAL_ROTATION_OVERRIDE = normalized;  // Store for use in anchor point collection
            addDebug("========================================");
            addDebug("[ROTATION OVERRIDE] GLOBAL: " + normalized + "°");
            addDebug("[ROTATION OVERRIDE] Applying to " + allPaths.length + " ductwork line paths");
            addDebug("========================================");
            var appliedCount = 0;
            for (var rIdx = 0; rIdx < allPaths.length; rIdx++) {
                if (allPaths[rIdx]) {
                    setRotationOverride(allPaths[rIdx], normalized);
                    clearOrthoLock(allPaths[rIdx]);
                    appliedCount++;
                }
            }
            addDebug("[ROTATION OVERRIDE] Applied to " + appliedCount + " ductwork line paths");
        } else {
            addDebug("========================================");
            addDebug("[ROTATION OVERRIDE] GLOBAL: None specified");
            addDebug("========================================");
        }

        // --- SEGMENT BUILDING ---
        function buildSegmentsForPaths(paths) {
            var segments = [];
            for (var i = 0; i < paths.length; i++) {
                var path = paths[i];
                var pts = path.pathPoints;
                if (pts.length < 2) continue;

                for (var k = 0; k < pts.length - 1; k++) {
                    var curr = pts[k], next = pts[k + 1];
                    var p0 = [curr.anchor[0], curr.anchor[1]];
                    var p3 = [next.anchor[0], next.anchor[1]];
                    var p1 = [curr.rightDirection[0], curr.rightDirection[1]];
                    var p2 = [next.leftDirection[0], next.leftDirection[1]];
                    var isStraight = almostEqualPoints(p0, p1) && almostEqualPoints(p2, p3);
                    if (isStraight) {
                        var bbox = makeBBox(p0, p3);
                        segments.push({
                            type: "line",
                            a: p0,
                            b: p3,
                            path: path,
                            bbox: expandBBox(bbox, SNAP_THRESHOLD)
                        });
                    } else {
                        var xMin = Math.min(p0[0], p1[0], p2[0], p3[0]);
                        var xMax = Math.max(p0[0], p1[0], p2[0], p3[0]);
                        var yMin = Math.min(p0[1], p1[1], p2[1], p3[1]);
                        var yMax = Math.max(p0[1], p1[1], p2[1], p3[1]);
                        var bbox = { minX: xMin, minY: yMin, maxX: xMax, maxY: yMax };
                        segments.push({
                            type: "cubic",
                            p0: p0, p1: p1, p2: p2, p3: p3,
                            path: path,
                            bbox: expandBBox(bbox, SNAP_THRESHOLD)
                        });
                    }
                }

                if (path.closed && pts.length > 1) {
                    var curr = pts[pts.length - 1], next = pts[0];
                    var p0 = [curr.anchor[0], curr.anchor[1]];
                    var p3 = [next.anchor[0], next.anchor[1]];
                    var p1 = [curr.rightDirection[0], curr.rightDirection[1]];
                    var p2 = [next.leftDirection[0], next.leftDirection[1]];
                    var isStraight = almostEqualPoints(p0, p1) && almostEqualPoints(p2, p3);
                    if (isStraight) {
                        var bbox = makeBBox(p0, p3);
                        segments.push({
                            type: "line",
                            a: p0,
                            b: p3,
                            path: path,
                            bbox: expandBBox(bbox, SNAP_THRESHOLD)
                        });
                    } else {
                        var xMin = Math.min(p0[0], p1[0], p2[0], p3[0]);
                        var xMax = Math.max(p0[0], p1[0], p2[0], p3[0]);
                        var yMin = Math.min(p0[1], p1[1], p2[1], p3[1]);
                        var yMax = Math.max(p0[1], p1[1], p2[1], p3[1]);
                        var bbox = { minX: xMin, minY: yMin, maxX: xMax, maxY: yMax };
                        segments.push({
                            type: "cubic",
                            p0: p0, p1: p1, p2: p2, p3: p3,
                            path: path,
                            bbox: expandBBox(bbox, SNAP_THRESHOLD)
                        });
                    }
                }
            }
            return segments;
        }

        // --- SNAP ANCHORS ---
        function snapAnchors(paths, allSegments) {
            var threshold2 = SNAP_THRESHOLD * SNAP_THRESHOLD;
            var changedAny = false;
            addDebug("[snapAnchors] Starting with " + paths.length + " paths and " + allSegments.length + " segments, threshold=" + SNAP_THRESHOLD + "px");
            for (var i = 0; i < paths.length; i++) {
                var path = paths[i];

                // Skip locked items or items on locked layers
                try {
                    if (path.locked) continue;
                    if (path.layer && path.layer.locked) continue;
                } catch (e) {
                    continue;
                }

                var pathLayerName = "";
                try { pathLayerName = path.layer ? path.layer.name : "unknown"; } catch (e) { pathLayerName = "unknown"; }

                var pts = path.pathPoints;
                if (pts.length === 0) continue;
                var buffer = [];
                for (var pi = 0; pi < pts.length; pi++) {
                    buffer[pi] = {
                        anchor: [pts[pi].anchor[0], pts[pi].anchor[1]],
                        left: [pts[pi].leftDirection[0], pts[pi].leftDirection[1]],
                        right: [pts[pi].rightDirection[0], pts[pi].rightDirection[1]]
                    };
                }

                var anyMoved = false;
                for (var pi = 0; pi < buffer.length; pi++) {
                    var point = buffer[pi].anchor;
                    var bestMatch = { dist2: Infinity, snapPt: null, targetSeg: null };

                    for (var j = 0; j < allSegments.length; j++) {
                        var seg = allSegments[j];
                        if (seg.path === path) continue;
                        if (!bboxContainsPoint(seg.bbox, point)) continue;

                        var candidate = null;
                        if (seg.type === "line") {
                            var res = closestPointOnSegment(seg.a, seg.b, point);
                            candidate = { pt: res.pt, dist2: dist2(res.pt, point) };
                        } else if (seg.type === "cubic") {
                            var best = closestPointOnCubic(seg.p0, seg.p1, seg.p2, seg.p3, point);
                            candidate = { pt: best.pt, dist2: best.dist2 };
                        }
                        if (candidate && candidate.dist2 < bestMatch.dist2) {
                            bestMatch.dist2 = candidate.dist2;
                            bestMatch.pt = candidate.pt;
                            bestMatch.targetSeg = seg;
                        }
                    }

                    if (bestMatch.snapPt === undefined && bestMatch.pt) {
                        if (bestMatch.dist2 <= threshold2 && bestMatch.dist2 > 1e-6) {
                            var old = buffer[pi].anchor;
                            var targetLayerName = "";
                            try { targetLayerName = bestMatch.targetSeg && bestMatch.targetSeg.path && bestMatch.targetSeg.path.layer ? bestMatch.targetSeg.path.layer.name : "unknown"; } catch (e) { targetLayerName = "unknown"; }
                            addDebug("[snapAnchors] SNAPPING point " + pi + " on '" + pathLayerName + "' FROM [" + old[0].toFixed(2) + ", " + old[1].toFixed(2) + "] TO [" + bestMatch.pt[0].toFixed(2) + ", " + bestMatch.pt[1].toFixed(2) + "] (dist=" + Math.sqrt(bestMatch.dist2).toFixed(2) + "px) -> target layer: '" + targetLayerName + "'");
                            buffer[pi].anchor = bestMatch.pt;
                            var delta = [bestMatch.pt[0] - old[0], bestMatch.pt[1] - old[1]];
                            buffer[pi].left = [buffer[pi].left[0] + delta[0], buffer[pi].left[1] + delta[1]];
                            buffer[pi].right = [buffer[pi].right[0] + delta[0], buffer[pi].right[1] + delta[1]];
                            anyMoved = true;
                        }
                    }
                }

                if (anyMoved) {
                    try {
                        for (var pi = 0; pi < pts.length; pi++) {
                            pts[pi].anchor = buffer[pi].anchor;
                            pts[pi].leftDirection = buffer[pi].left;
                            pts[pi].rightDirection = buffer[pi].right;
                        }
                        changedAny = true;
                    } catch (e) {
                        // Layer may have become locked during execution
                    }
                }
            }
            return changedAny;
        }

        // --- ORTHOGONALIZE ---
        function orthogonalizePath(pathItem, connectionPairs) {
            // Validate pathItem exists and is accessible
            if (!pathItem) {
                addDebug("[Orthogonalize] SKIP: pathItem is null/undefined");
                return false;
            }

            // Helper: check if a specific endpoint of this path connects to ANY other ductwork path
            // (same-layer blue-to-blue connections or cross-layer blue-to-green connections)
            function endpointHasDuctworkConnection(path, anchorIndex) {
                var pathName = "";
                try { pathName = path.layer ? path.layer.name : "(unknown layer)"; } catch (e) { pathName = "(error getting layer)"; }

                if (!connectionPairs || !connectionPairs.length) {
                    addDebug("[EndpointCheck] path '" + pathName + "' idx=" + anchorIndex + " -> NO connectionPairs available");
                    return false;
                }
                addDebug("[EndpointCheck] path '" + pathName + "' idx=" + anchorIndex + " -> checking " + connectionPairs.length + " pairs");

                for (var pi = 0; pi < connectionPairs.length; pi++) {
                    var pair = connectionPairs[pi];
                    if (!pair) continue;
                    // Check ALL connections - both same-layer (trunk) and cross-layer
                    // This ensures the main trunk is always orthogonalized
                    if (pair.a && pair.a.path === path && pair.a.index === anchorIndex) {
                        addDebug("[EndpointCheck] FOUND connection at pair " + pi + " via pair.a");
                        return true;
                    }
                    if (pair.b && pair.b.path === path && pair.b.index === anchorIndex) {
                        addDebug("[EndpointCheck] FOUND connection at pair " + pi + " via pair.b");
                        return true;
                    }
                }
                addDebug("[EndpointCheck] NO connection found for path '" + pathName + "' idx=" + anchorIndex);
                return false;
            }
            var earlyLayerName = "";
            try { earlyLayerName = pathItem.layer ? pathItem.layer.name : "unknown"; } catch (e) { earlyLayerName = "unknown"; }
            try {
                // Test if pathItem is still valid by accessing a property
                var test = pathItem.typename;
            } catch (e) {
                // pathItem is invalid (e.g., after copy/paste)
                addDebug("[Orthogonalize] SKIP: pathItem invalid (typename access failed) - layer: " + earlyLayerName);
                return false;
            }

            // Skip locked items or items on locked layers
            try {
                if (pathItem.locked) {
                    addDebug("[Orthogonalize] SKIP: pathItem is locked - layer: " + earlyLayerName);
                    return false;
                }
                if (pathItem.layer && pathItem.layer.locked) {
                    addDebug("[Orthogonalize] SKIP: layer is locked - layer: " + earlyLayerName);
                    return false;
                }
            } catch (e) {
                // Can't determine lock status, skip to be safe
                addDebug("[Orthogonalize] SKIP: can't determine lock status - layer: " + earlyLayerName);
                return false;
            }

            // Skip orthogonalizing paths that live on the Thermostat Lines layer
            // (case-insensitive match). Return false to signal no change.
            try {
                if (pathItem && pathItem.layer && (/^thermostat lines$/i).test(pathItem.layer.name)) {
                    addDebug("[Orthogonalize] SKIP: on Thermostat Lines layer");
                    return false;
                }
            } catch (e) {
                // Layer may be invalid after copy/paste
            }
            if (hasIgnoreNote(pathItem)) {
                addDebug("[Orthogonalize] SKIP: has ignore note - layer: " + earlyLayerName);
                return false;
            }
            // Skip orthogonalizing if path has MD:NO_ORTHO note
            if (hasNoOrthoNote(pathItem)) {
                addDebug("[Orthogonalize] SKIP: has MD:NO_ORTHO note - layer: " + earlyLayerName);
                return false;
            }

            var isBlueBranch = false;
            var layerName = "";
            try { layerName = pathItem.layer ? pathItem.layer.name : ""; } catch (eLayer) { }
            if (isBlueDuctworkLayerName(layerName)) {
                isBlueBranch = true;
            }
            var branchData = null;
            if (isBlueBranch) {
                try { branchData = pathItem.__blueRightAngleBranchData || null; } catch (eBranchData) { branchData = null; }
            }

            var rotationOverride = getRotationOverride(pathItem);
            var hasRotationOverride = rotationOverride !== null;
            // PERFORMANCE: Removed per-path logging
            if (!hasRotationOverride && branchData && branchData.mainPath) {
                try {
                    var mainPathItem = branchData.mainPath;
                    var mainPts = mainPathItem && mainPathItem.pathPoints ? mainPathItem.pathPoints : null;
                    var mainSegIndex = (typeof branchData.mainSegmentIndex === "number") ? branchData.mainSegmentIndex : null;
                    if (mainPts && mainPts.length > 1 && mainSegIndex !== null && mainSegIndex >= 0 && mainSegIndex < mainPts.length - 1) {
                        var mainStart = mainPts[mainSegIndex].anchor;
                        var mainEnd = mainPts[mainSegIndex + 1].anchor;
                        var mainDX = mainEnd[0] - mainStart[0];
                        var mainDY = mainEnd[1] - mainStart[1];
                        if (Math.abs(mainDX) > 1e-3 || Math.abs(mainDY) > 1e-3) {
                            rotationOverride = Math.atan2(mainDY, mainDX) * (180 / Math.PI);
                            hasRotationOverride = true;
                        }
                    }
                } catch (eDerivedRotation) { }
            }
            // Skip ORTHO_LOCK check when skip-final is enabled (paths may need re-processing
            // after shared endpoints are moved by other paths)
            if (!hasRotationOverride && !SKIP_FINAL_REGISTER_ORTHO && hasOrthoLock(pathItem)) {
                return false;
            }

            var pts = null;
            try {
                pts = pathItem.pathPoints;
            } catch (e) {
                // pathItem may be invalid after copy/paste
                return false;
            }
            if (!pts || pts.length < 2) return false;
            storePreOrthoGeometry(pathItem);
            var registerAnchorIndex = null;
            if (branchData && typeof branchData.branchAnchorIndex === "number") {
                var maxAnchorIndex = pts.length - 1;
                var anchorIdx = branchData.branchAnchorIndex;
                if (anchorIdx < 0) anchorIdx = 0;
                if (anchorIdx > maxAnchorIndex) anchorIdx = maxAnchorIndex;
                if (anchorIdx === 0) {
                    registerAnchorIndex = maxAnchorIndex;
                } else if (anchorIdx === maxAnchorIndex) {
                    registerAnchorIndex = 0;
                }
            }
            var registerSegmentIndex = null;
            if (registerAnchorIndex === 0) {
                registerSegmentIndex = 0;
            } else if (registerAnchorIndex === pts.length - 1) {
                registerSegmentIndex = pts.length - 2;
            }
            if (registerSegmentIndex === null && branchData && typeof branchData.branchSegmentIndex === "number") {
                var segIdx = branchData.branchSegmentIndex;
                if (segIdx >= 0 && segIdx < pts.length - 1) {
                    registerSegmentIndex = segIdx;
                }
            }
            var changed = false;
            var cosTheta = 1;
            var sinTheta = 0;
            var skipAllBranchOrtho = SKIP_ALL_BRANCH_ORTHO;
            var skipFinalBranchOrtho = SKIP_FINAL_REGISTER_ORTHO;
            if (hasRotationOverride) {
                var rotationRad = rotationOverride * (Math.PI / 180);
                cosTheta = Math.cos(rotationRad);
                sinTheta = Math.sin(rotationRad);
            }

            // Helper: check if an endpoint T-junctions onto another ductwork path (touches middle of path, not endpoint)
            function endpointTJunctionsOntoPath(path, anchorIndex) {
                var pathName = "";
                try { pathName = path.layer ? path.layer.name : "(unknown layer)"; } catch (e) { pathName = "(error getting layer)"; }

                var anchor = pts[anchorIndex].anchor;
                var TOLERANCE = 5; // pixels

                // Check all geometry paths for T-junction
                for (var gi = 0; gi < geometryPaths.length; gi++) {
                    var otherPath = geometryPaths[gi];
                    if (!otherPath || otherPath === path) continue;

                    try {
                        var otherPts = otherPath.pathPoints;
                        if (!otherPts || otherPts.length < 2) continue;

                        // Check if our endpoint is near any SEGMENT of the other path (not just endpoints)
                        for (var si = 0; si < otherPts.length - 1; si++) {
                            var segStart = otherPts[si].anchor;
                            var segEnd = otherPts[si + 1].anchor;

                            // Skip if this is an endpoint of the other path (that would be endpoint-to-endpoint, not T-junction)
                            if (si === 0 || si === otherPts.length - 2) {
                                // Check distance to segment endpoints
                                var distToStart = Math.sqrt(Math.pow(anchor[0] - segStart[0], 2) + Math.pow(anchor[1] - segStart[1], 2));
                                var distToEnd = Math.sqrt(Math.pow(anchor[0] - segEnd[0], 2) + Math.pow(anchor[1] - segEnd[1], 2));
                                if ((si === 0 && distToStart < TOLERANCE) || (si === otherPts.length - 2 && distToEnd < TOLERANCE)) {
                                    continue; // This is endpoint-to-endpoint, not T-junction
                                }
                            }

                            // Check if our anchor is near this segment (point-to-line distance)
                            var segDx = segEnd[0] - segStart[0];
                            var segDy = segEnd[1] - segStart[1];
                            var segLen = Math.sqrt(segDx * segDx + segDy * segDy);
                            if (segLen < 0.001) continue;

                            // Project anchor onto segment line
                            var t = ((anchor[0] - segStart[0]) * segDx + (anchor[1] - segStart[1]) * segDy) / (segLen * segLen);

                            // Check if projection is within segment (not at endpoints)
                            if (t > 0.05 && t < 0.95) {
                                var projX = segStart[0] + t * segDx;
                                var projY = segStart[1] + t * segDy;
                                var dist = Math.sqrt(Math.pow(anchor[0] - projX, 2) + Math.pow(anchor[1] - projY, 2));

                                if (dist < TOLERANCE) {
                                    var otherLayerName = "";
                                    try { otherLayerName = otherPath.layer ? otherPath.layer.name : "unknown"; } catch (e) { }
                                    addDebug("[T-Junction] path '" + pathName + "' idx=" + anchorIndex + " T-junctions onto '" + otherLayerName + "' at segment " + si);
                                    return true;
                                }
                            }
                        }
                    } catch (e) {
                        // Skip problematic paths
                    }
                }

                addDebug("[T-Junction] path '" + pathName + "' idx=" + anchorIndex + " - no T-junction found");
                return false;
            }

            // Determine if this entire path is a "branch" (no endpoint-to-endpoint connections)
            // vs "trunk" (has at least one endpoint-to-endpoint connection)
            var pathIsBranch = false;
            var registerEndIsFirst = false; // true if index 0 is register end, false if last index is register end

            if ((skipAllBranchOrtho || skipFinalBranchOrtho) && !pathItem.closed && pts.length >= 2) {
                var firstEndpointConnected = endpointHasDuctworkConnection(pathItem, 0);
                var lastEndpointConnected = endpointHasDuctworkConnection(pathItem, pts.length - 1);

                // If NEITHER endpoint has an endpoint-to-endpoint connection, it's a branch
                // (it T-junctions onto trunk or goes to registers at both ends)
                if (!firstEndpointConnected && !lastEndpointConnected) {
                    pathIsBranch = true;
                    addDebug("[BRANCH-DETECT] Path classified as BRANCH (neither endpoint connected). firstConnected=" + firstEndpointConnected + ", lastConnected=" + lastEndpointConnected);

                    // For skip-final, we need to know which end is the register end
                    // Check which endpoint T-junctions onto another path (that's the trunk connection)
                    var firstTJunctions = endpointTJunctionsOntoPath(pathItem, 0);
                    var lastTJunctions = endpointTJunctionsOntoPath(pathItem, pts.length - 1);

                    if (firstTJunctions && !lastTJunctions) {
                        registerEndIsFirst = false;
                    } else if (!firstTJunctions && lastTJunctions) {
                        registerEndIsFirst = true;
                    } else {
                        registerEndIsFirst = false;
                    }
                }
            }

            // FR-002: Check if register endpoint has an ignore anchor
            // If so, the final segment should still be orthogonalized
            // NOTE: Do a LIVE check of the Ignored layer since ignore anchors may be created
            // during processing (e.g., when internal anchors close to endpoints are cleaned up)
            var registerEndpointIgnored = false;
            var registerEndpointHasRegister = false; // Check if endpoint actually has a register nearby
            if (skipFinalBranchOrtho && pathIsBranch && pts.length >= 2) {
                var registerEndpoint = registerEndIsFirst ? pts[0].anchor : pts[pts.length - 1].anchor;
                var IGNORE_CHECK_DIST = 10; // tolerance for ignore anchor check
                addDebug("[SKIP-FINAL-CHECK] Path is branch, checking register endpoint at [" + registerEndpoint[0].toFixed(1) + "," + registerEndpoint[1].toFixed(1) + "], registerEndIsFirst=" + registerEndIsFirst);
                var REGISTER_CHECK_DIST = 25; // tolerance for register proximity check

                // First check pre-collected ignore anchors
                for (var igIdx = 0; igIdx < ORTHO_IGNORED_ANCHORS.length; igIdx++) {
                    var igPt = ORTHO_IGNORED_ANCHORS[igIdx];
                    var igDist = Math.sqrt(Math.pow(registerEndpoint[0] - igPt[0], 2) + Math.pow(registerEndpoint[1] - igPt[1], 2));
                    if (igDist <= IGNORE_CHECK_DIST) {
                        registerEndpointIgnored = true;
                        break;
                    }
                }

                // LIVE CHECK: Also check the Ignored layer directly for newly created ignore anchors
                if (!registerEndpointIgnored) {
                    try {
                        var ignoredLayerNames = ["Ignore", "Ignored", "ignore", "ignored"];
                        for (var ignLayerIdx = 0; ignLayerIdx < ignoredLayerNames.length && !registerEndpointIgnored; ignLayerIdx++) {
                            try {
                                var ignLayer = doc.layers.getByName(ignoredLayerNames[ignLayerIdx]);
                                if (ignLayer) {
                                    // Check all path items on this layer
                                    for (var ignPathIdx = 0; ignPathIdx < ignLayer.pathItems.length && !registerEndpointIgnored; ignPathIdx++) {
                                        var ignPath = ignLayer.pathItems[ignPathIdx];
                                        if (ignPath && ignPath.pathPoints && ignPath.pathPoints.length >= 1) {
                                            for (var ignPtIdx = 0; ignPtIdx < ignPath.pathPoints.length; ignPtIdx++) {
                                                var ignAnchor = ignPath.pathPoints[ignPtIdx].anchor;
                                                var ignDistLive = Math.sqrt(Math.pow(registerEndpoint[0] - ignAnchor[0], 2) + Math.pow(registerEndpoint[1] - ignAnchor[1], 2));
                                                if (ignDistLive <= IGNORE_CHECK_DIST) {
                                                    registerEndpointIgnored = true;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            } catch (eIgnLayer) { /* Layer doesn't exist */ }
                        }
                    } catch (eLiveCheck) { /* Ignore errors in live check */ }
                }

                // Check if there's actually a register near this endpoint
                // If no register exists, we should still orthogonalize (trunk without register at end)
                for (var regIdx = 0; regIdx < ORTHO_REGISTER_ANCHORS.length; regIdx++) {
                    var regPt = ORTHO_REGISTER_ANCHORS[regIdx];
                    var regDist = Math.sqrt(Math.pow(registerEndpoint[0] - regPt[0], 2) + Math.pow(registerEndpoint[1] - regPt[1], 2));
                    if (regDist <= REGISTER_CHECK_DIST) {
                        registerEndpointHasRegister = true;
                        break;
                    }
                }
                addDebug("[SKIP-FINAL-CHECK] Results: registerEndpointIgnored=" + registerEndpointIgnored + ", registerEndpointHasRegister=" + registerEndpointHasRegister + ", ORTHO_REGISTER_ANCHORS.length=" + ORTHO_REGISTER_ANCHORS.length);
            }

            // PERFORMANCE: Removed all addDebug calls from this hot function
            var shouldOrthogonalizeSegment = function (segmentIndex, totalSegments) {
                if (skipAllBranchOrtho) {
                    if (pathIsBranch) return false;
                    return true;
                }

                if (skipFinalBranchOrtho) {
                    if (!pathIsBranch) return true;
                    if (totalSegments === 1) return true;
                    var registerSegmentIndex = registerEndIsFirst ? 0 : (totalSegments - 1);
                    if (segmentIndex === registerSegmentIndex) {
                        // FR-002: If register endpoint is ignored, DON'T skip ortho
                        // (the final segment goes to an ignored point, not a ductwork part)
                        if (registerEndpointIgnored) {
                            addDebug("[SKIP-FINAL-DECISION] Segment " + segmentIndex + " is final segment but endpoint is IGNORED - WILL ortho");
                            return true; // Ortho this segment normally
                        }
                        // If no register exists at this endpoint, DON'T skip ortho
                        // (trunk/path without register at end should be orthogonalized)
                        if (!registerEndpointHasRegister) {
                            addDebug("[SKIP-FINAL-DECISION] Segment " + segmentIndex + " is final segment but NO REGISTER at endpoint - WILL ortho");
                            return true; // Ortho this segment normally
                        }
                        addDebug("[SKIP-FINAL-DECISION] Segment " + segmentIndex + " is final segment with REGISTER - SKIPPING ortho");
                        return false;
                    }
                    return true;
                }

                return true;
            };

            var totalSegments = pathItem.closed ? pts.length : pts.length - 1;

            for (var i = 0; i < pts.length; i++) {
                try {
                    var curr = pts[i];
                    var next = (i === pts.length - 1) ? (pathItem.closed ? pts[0] : null) : pts[i + 1];
                    if (!next) continue;

                    var dx = next.anchor[0] - curr.anchor[0];
                    var dy = next.anchor[1] - curr.anchor[1];

                    // Skip segments based on T-intersection logic
                    if (!shouldOrthogonalizeSegment(i, totalSegments)) {
                        continue;
                    }

                    // Removed: Emory-specific final segment handling
                    if (hasRotationOverride) {
                        var localX = dx * cosTheta + dy * sinTheta;
                        var localY = -dx * sinTheta + dy * cosTheta;
                        var localAngle = Math.atan2(localY, localX) * (180 / Math.PI);
                        var worldAngle = Math.atan2(dy, dx) * (180 / Math.PI);
                        addDebug("[Orthogonalize Seg " + i + "] World angle: " + worldAngle.toFixed(1) + "°, Local angle: " + localAngle.toFixed(1) + "°");

                        // Check if the angle is steep relative to the rotation override
                        var isLocalSteep = isSteepAngle(localAngle);
                        addDebug("[Orthogonalize Seg " + i + "] Local angle steep check: " + isLocalSteep);

                        if (!isLocalSteep) {
                            var targetX = localX;
                            var targetY = localY;
                            if (Math.abs(localX) >= Math.abs(localY)) {
                                targetY = 0;
                                addDebug("[Orthogonalize Seg " + i + "] Snapping to horizontal in rotated grid");
                            } else {
                                targetX = 0;
                                addDebug("[Orthogonalize Seg " + i + "] Snapping to vertical in rotated grid");
                            }

                            var newDX = targetX * cosTheta - targetY * sinTheta;
                            var newDY = targetX * sinTheta + targetY * cosTheta;
                            var newWorldAngle = Math.atan2(newDY, newDX) * (180 / Math.PI);
                            addDebug("[Orthogonalize Seg " + i + "] New world angle: " + newWorldAngle.toFixed(1) + "°");

                            var newX = curr.anchor[0] + newDX;
                            var newY = curr.anchor[1] + newDY;
                            if (!almostEqualPoints([newX, newY], next.anchor)) {
                                next.anchor = [newX, newY];
                                changed = true;
                            }

                            curr.rightDirection = curr.anchor.slice();
                            next.leftDirection = next.anchor.slice();
                            next.rightDirection = next.anchor.slice();
                            if (i > 0) {
                                var prevRot = pts[i - 1];
                                prevRot.rightDirection = prevRot.anchor.slice();
                            }
                            curr.leftDirection = curr.anchor.slice();
                            curr.rightDirection = curr.anchor.slice();
                        } else {
                            addDebug("[Orthogonalize Seg " + i + "] SKIPPED (steep angle relative to rotation override)");
                        }
                    } else {
                        var angle = Math.atan2(dy, dx) * (180 / Math.PI);
                        var isSteep = isSteepAngle(angle);
                        addDebug("[Orthogonalize Seg " + i + "] Angle: " + angle.toFixed(2) + "°, dx: " + dx.toFixed(2) + ", dy: " + dy.toFixed(2) + ", isSteep: " + isSteep);

                        // FIRST: Check if this segment connects an ignore marker to an endpoint
                        // If so, use the adjacent segment's direction instead of the segment's own angle
                        var ignoreMarkerInfo = null;
                        for (var imIdx = 0; imIdx < ORTHO_IGNORE_MARKER_PATHS.length; imIdx++) {
                            if (ORTHO_IGNORE_MARKER_PATHS[imIdx].path === pathItem) {
                                ignoreMarkerInfo = ORTHO_IGNORE_MARKER_PATHS[imIdx];
                                break;
                            }
                        }

                        var isIgnoreMarkerSegment = false;
                        var useAdjacentDirection = null; // "prev" or "next"
                        if (ignoreMarkerInfo) {
                            var markerIdx = ignoreMarkerInfo.ignoreMarkerIndex;
                            if (ignoreMarkerInfo.endpoint === "end") {
                                // Ignore marker is at markerIdx, endpoint is at pts.length-1
                                // The segment from marker to endpoint is segment index markerIdx
                                if (i === markerIdx) {
                                    isIgnoreMarkerSegment = true;
                                    useAdjacentDirection = "prev";
                                }
                            } else if (ignoreMarkerInfo.endpoint === "start") {
                                // Ignore marker is at markerIdx, endpoint is at 0
                                // The segment from endpoint to marker is segment index 0
                                if (i === 0) {
                                    isIgnoreMarkerSegment = true;
                                    useAdjacentDirection = "next";
                                }
                            }
                        }

                        if (isIgnoreMarkerSegment && useAdjacentDirection === "prev" && i > 0) {
                            // Segment connects ignore marker to END - use previous segment direction
                            var prevPt = pts[i - 1];
                            var prevDx = curr.anchor[0] - prevPt.anchor[0];
                            var prevDy = curr.anchor[1] - prevPt.anchor[1];
                            addDebug("[Orthogonalize Seg " + i + "] Ignore marker segment (to end) - using prev segment direction");

                            var newX = next.anchor[0];
                            var newY = next.anchor[1];
                            if (Math.abs(prevDx) > Math.abs(prevDy)) {
                                // Previous segment was horizontal, extend horizontally
                                newY = curr.anchor[1];
                                addDebug("[Orthogonalize Seg " + i + "] Extending horizontal from prev (newY = " + newY.toFixed(2) + ")");
                            } else {
                                // Previous segment was vertical, extend vertically
                                newX = curr.anchor[0];
                                addDebug("[Orthogonalize Seg " + i + "] Extending vertical from prev (newX = " + newX.toFixed(2) + ")");
                            }
                            var isAlmostEqual = almostEqualPoints([newX, newY], next.anchor);
                            if (!isAlmostEqual) {
                                next.anchor = [newX, newY];
                                changed = true;
                                addDebug("[Orthogonalize Seg " + i + "] CHANGED anchor (ignore marker segment)");
                            }
                            curr.rightDirection = curr.anchor.slice();
                            next.leftDirection = next.anchor.slice();
                            next.rightDirection = next.anchor.slice();
                        } else if (isIgnoreMarkerSegment && useAdjacentDirection === "next" && pts.length > 2) {
                            // Segment connects START to ignore marker - use next segment direction
                            var nextNext = pts[2];
                            var nextDx = nextNext.anchor[0] - next.anchor[0];
                            var nextDy = nextNext.anchor[1] - next.anchor[1];
                            addDebug("[Orthogonalize Seg " + i + "] Ignore marker segment (from start) - using next segment direction");

                            var newX = curr.anchor[0];
                            var newY = curr.anchor[1];
                            if (Math.abs(nextDx) > Math.abs(nextDy)) {
                                // Next segment is horizontal, move curr to match next's Y
                                newY = next.anchor[1];
                                addDebug("[Orthogonalize Seg " + i + "] Aligning to horizontal from next (newY = " + newY.toFixed(2) + ")");
                            } else {
                                // Next segment is vertical, move curr to match next's X
                                newX = next.anchor[0];
                                addDebug("[Orthogonalize Seg " + i + "] Aligning to vertical from next (newX = " + newX.toFixed(2) + ")");
                            }
                            var isAlmostEqual = almostEqualPoints([newX, newY], curr.anchor);
                            if (!isAlmostEqual) {
                                curr.anchor = [newX, newY];
                                changed = true;
                                addDebug("[Orthogonalize Seg " + i + "] CHANGED anchor (ignore marker segment)");
                            }
                            curr.leftDirection = curr.anchor.slice();
                            curr.rightDirection = curr.anchor.slice();
                            next.leftDirection = next.anchor.slice();
                        } else if (!isSteep) {
                            // Normal non-steep segment - orthogonalize based on segment's own angle
                            var newX = next.anchor[0];
                            var newY = next.anchor[1];
                            if (Math.abs(dx) > Math.abs(dy)) {
                                newY = curr.anchor[1];
                                addDebug("[Orthogonalize Seg " + i + "] Snapping to horizontal (newY = " + newY.toFixed(2) + ")");
                            } else {
                                newX = curr.anchor[0];
                                addDebug("[Orthogonalize Seg " + i + "] Snapping to vertical (newX = " + newX.toFixed(2) + ")");
                            }
                            var isAlmostEqual = almostEqualPoints([newX, newY], next.anchor);
                            addDebug("[Orthogonalize Seg " + i + "] almostEqualPoints: " + isAlmostEqual + ", old: [" + next.anchor[0].toFixed(2) + ", " + next.anchor[1].toFixed(2) + "], new: [" + newX.toFixed(2) + ", " + newY.toFixed(2) + "]");
                            if (!isAlmostEqual) {
                                next.anchor = [newX, newY];
                                changed = true;
                                addDebug("[Orthogonalize Seg " + i + "] CHANGED anchor");
                            }
                            curr.rightDirection = curr.anchor.slice();
                            next.leftDirection = next.anchor.slice();
                            next.rightDirection = next.anchor.slice();
                            if (i > 0) {
                                var prev = pts[i - 1];
                                prev.rightDirection = prev.anchor.slice();
                            }
                            curr.leftDirection = curr.anchor.slice();
                            curr.rightDirection = curr.anchor.slice();
                        } else {
                            // Steep angle - skip (ignore marker segments already handled above)
                            addDebug("[Orthogonalize Seg " + i + "] SKIPPED (steep angle)");
                        }
                    }
                } catch (e) {
                    // Layer may have become locked or item invalid during execution
                }
            }
            addDebug("[Orthogonalize] Path changed: " + changed);
            if (changed) {
                // Don't apply ORTHO_LOCK when skip-final is enabled, because shared endpoints
                // may be moved by other paths and we need to re-orthogonalize
                if (!SKIP_FINAL_REGISTER_ORTHO) {
                    flagOrthoLock(pathItem);
                } else {
                    addDebug("[Orthogonalize] Skip-final mode: NOT setting ORTHO_LOCK (allows re-processing)");
                }
            }
            return changed;
        }

        // --- ORIGINAL SOPHISTICATED CONNECTION DETECTION ---
        function pairKey(a, b) {
            return [a._id || a.pathPoints[0].anchor.join(","), b._id || b.pathPoints[0].anchor.join(",")].sort().join("|");
        }

        function vector(a, b) { return [b[0] - a[0], b[1] - a[1]]; }
        function length(v) { return Math.sqrt(v[0] * v[0] + v[1] * v[1]); }
        function normalize(v) { var l = length(v); return l === 0 ? [0, 0] : [v[0] / l, v[1] / l]; }
        function angleBetween(u, v) {
            var du = normalize(u), dv = normalize(v);
            var dotp = du[0] * dv[0] + du[1] * dv[1];
            dotp = Math.max(-1, Math.min(1, dotp));
            return Math.acos(dotp) * (180 / Math.PI);
        }
        function getAdjacentSegmentDirs(path, anchorIndex) {
            var pts = path.pathPoints;
            var dirs = [];
            if (anchorIndex > 0) {
                var prev = pts[anchorIndex - 1];
                var curr = pts[anchorIndex];
                dirs.push(vector(prev.anchor, curr.anchor));
            } else if (path.closed && pts.length > 1) {
                var prev = pts[pts.length - 1];
                var curr = pts[0];
                dirs.push(vector(prev.anchor, curr.anchor));
            }
            if (anchorIndex < pts.length - 1) {
                var curr = pts[anchorIndex];
                var next = pts[anchorIndex + 1];
                dirs.push(vector(curr.anchor, next.anchor));
            } else if (path.closed && pts.length > 1) {
                var curr = pts[pts.length - 1];
                var next = pts[0];
                dirs.push(vector(curr.anchor, next.anchor));
            }
            return dirs;
        }

        // =====================================================================
        // PYTHON-ACCELERATED CONNECTION DETECTION
        // =====================================================================
        // This wrapper function uses Python for 100-1000x faster processing
        // Falls back to ExtendScript if Python is unavailable
        // =====================================================================
        function findAllConnections(pathItems, maxDist, ignoredAnchorsOut, existingIgnoredAnchors) {
            ignoredAnchorsOut = ignoredAnchorsOut || [];
            existingIgnoredAnchors = existingIgnoredAnchors || [];

            // Minimum paths threshold - Python startup overhead (~7s) isn't worth it for small groups
            var MIN_PATHS_FOR_PYTHON = 20;

            // Try Python acceleration first (only for larger path sets)
            if ($.global.MDUX_USE_PYTHON && typeof PythonBridge !== 'undefined' && PythonBridge.isAvailable() && pathItems.length >= MIN_PATHS_FOR_PYTHON) {
                addDebug("[PYTHON] Attempting Python-accelerated connection detection for " + pathItems.length + " paths");
                var startTime = new Date().getTime();

                try {
                    var pyResult = PythonBridge.findConnections(pathItems, maxDist);

                    if (pyResult && pyResult.connections && !pyResult.error) {
                        // Map Python indices back to actual path objects
                        var connections = [];
                        for (var ci = 0; ci < pyResult.connections.length; ci++) {
                            var conn = pyResult.connections[ci];
                            var pathA = pathItems[conn.a];
                            var pathB = pathItems[conn.b];
                            if (pathA && pathB) {
                                connections.push([pathA, pathB]);
                            }
                        }

                        // Copy ignored anchors from Python result
                        if (pyResult.ignored_anchors) {
                            for (var ia = 0; ia < pyResult.ignored_anchors.length; ia++) {
                                ignoredAnchorsOut.push(pyResult.ignored_anchors[ia]);
                            }
                        }

                        var elapsed = new Date().getTime() - startTime;
                        addDebug("[PYTHON] SUCCESS: Found " + connections.length + " connections in " + elapsed + "ms (Python: " + (pyResult.time_ms || "?") + "ms)");
                        return connections;
                    } else {
                        addDebug("[PYTHON] No valid result, falling back to ExtendScript");
                    }
                } catch (ePython) {
                    addDebug("[PYTHON] Error: " + ePython + ", falling back to ExtendScript");
                }
            }

            // Fall back to original ExtendScript implementation
            return findAllConnectionsExtendScript(pathItems, maxDist, ignoredAnchorsOut, existingIgnoredAnchors);
        }

        // Original ExtendScript implementation (renamed from findAllConnections)
        function findAllConnectionsExtendScript(pathItems, maxDist, ignoredAnchorsOut, existingIgnoredAnchors) {
            var connections = [];
            var seen = {};
            var ANGLE_THRESHOLD_DEG = 20;
            var MIN_DIST = 0.5; // Minimum distance - paths closer than this are likely duplicates
            var T_JUNCTION_DIST = 3; // Tolerance for T-junction detection (point-to-segment) - real branches are drawn very close
            var PATH_ANCHOR_TOLERANCE = 10; // Distance threshold for path vertex at intersection check
            var DEBUG_CONNECTIONS = $.global.MDUX_DEBUG && $.global.MDUX_DEBUG.CONNECTIONS; // References global config
            ignoredAnchorsOut = ignoredAnchorsOut || []; // Array to collect intersection points to ignore
            existingIgnoredAnchors = existingIgnoredAnchors || []; // Array of existing ignored anchors to skip

            if (DEBUG_CONNECTIONS && pathItems.length > 0) {
                addDebug("[CONN-DEBUG] Checking " + pathItems.length + " paths with maxDist=" + maxDist);
            }

            // Helper to check if a path has a vertex (path point) near a given point
            function pathHasVertexNearPoint(pathPts, pt) {
                for (var v = 0; v < pathPts.length; v++) {
                    var vx = pathPts[v].anchor[0] - pt[0];
                    var vy = pathPts[v].anchor[1] - pt[1];
                    if (Math.sqrt(vx * vx + vy * vy) <= PATH_ANCHOR_TOLERANCE) {
                        return true;
                    }
                }
                return false;
            }

            // Helper to check if a point is near a crossover (segment intersection without vertex)
            // Returns true if the point is near where pathA and pathB intersect but neither has a vertex there
            function isNearCrossover(pt, ptsA, ptsB, tolerance) {
                var CROSSOVER_CHECK_DIST = tolerance || 15;
                for (var sA = 0; sA < ptsA.length - 1; sA++) {
                    for (var sB = 0; sB < ptsB.length - 1; sB++) {
                        var intPt = getSegmentIntersectionPoint(
                            ptsA[sA].anchor[0], ptsA[sA].anchor[1],
                            ptsA[sA + 1].anchor[0], ptsA[sA + 1].anchor[1],
                            ptsB[sB].anchor[0], ptsB[sB].anchor[1],
                            ptsB[sB + 1].anchor[0], ptsB[sB + 1].anchor[1]
                        );
                        if (intPt) {
                            // Check if pt is near this intersection
                            var distToInt = Math.sqrt((pt[0] - intPt[0]) * (pt[0] - intPt[0]) + (pt[1] - intPt[1]) * (pt[1] - intPt[1]));
                            if (distToInt <= CROSSOVER_CHECK_DIST) {
                                // Check if either path has a vertex at this intersection
                                var hasVertexA = pathHasVertexNearPoint(ptsA, intPt);
                                var hasVertexB = pathHasVertexNearPoint(ptsB, intPt);
                                if (!hasVertexA && !hasVertexB) {
                                    // This is a crossover - no vertex at intersection
                                    return true;
                                }
                            }
                        }
                    }
                }
                return false;
            }

            // Helper to check if a point is near an existing ignored anchor
            function isNearIgnoredAnchor(pt, ignoredAnchors, tolerance) {
                var IGNORED_TOLERANCE = tolerance || 4; // Default 4pt (matches IGNORED_DIST)
                for (var ig = 0; ig < ignoredAnchors.length; ig++) {
                    var dx = pt[0] - ignoredAnchors[ig][0];
                    var dy = pt[1] - ignoredAnchors[ig][1];
                    var d = Math.sqrt(dx * dx + dy * dy);
                    if (d <= IGNORED_TOLERANCE) {
                        return true;
                    }
                }
                return false;
            }

            // === SPATIAL HASHING FOR O(n) PERFORMANCE ===
            // Build a grid where each cell is T_JUNCTION_DIST sized
            // Only check paths in same or adjacent cells
            var CELL_SIZE = T_JUNCTION_DIST * 2; // Cell size covers max connection distance
            var spatialGrid = {};
            var pathToCells = []; // For each path, which cells it occupies

            // Phase 1: Build spatial index - put each path into grid cells based on its bounding box
            for (var si = 0; si < pathItems.length; si++) {
                var spath = pathItems[si];
                var spts = spath.pathPoints;
                if (!spts || spts.length === 0) {
                    pathToCells.push([]);
                    continue;
                }

                // Get bounding box of path
                var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
                for (var sp = 0; sp < spts.length; sp++) {
                    var anchor = spts[sp].anchor;
                    if (anchor[0] < minX) minX = anchor[0];
                    if (anchor[0] > maxX) maxX = anchor[0];
                    if (anchor[1] < minY) minY = anchor[1];
                    if (anchor[1] > maxY) maxY = anchor[1];
                }

                // Expand by T_JUNCTION_DIST to catch nearby paths
                minX -= T_JUNCTION_DIST; minY -= T_JUNCTION_DIST;
                maxX += T_JUNCTION_DIST; maxY += T_JUNCTION_DIST;

                // Find all cells this path occupies
                var cellMinX = Math.floor(minX / CELL_SIZE);
                var cellMaxX = Math.floor(maxX / CELL_SIZE);
                var cellMinY = Math.floor(minY / CELL_SIZE);
                var cellMaxY = Math.floor(maxY / CELL_SIZE);

                var cells = [];
                for (var cx = cellMinX; cx <= cellMaxX; cx++) {
                    for (var cy = cellMinY; cy <= cellMaxY; cy++) {
                        var cellKey = cx + "," + cy;
                        cells.push(cellKey);
                        if (!spatialGrid[cellKey]) spatialGrid[cellKey] = [];
                        spatialGrid[cellKey].push(si);
                    }
                }
                pathToCells.push(cells);
            }

            // Phase 2: Check only paths that share cells (spatial proximity)
            var checkedPairs = {};
            for (var i = 0; i < pathItems.length; i++) {
                var pathA = pathItems[i];
                var ptsA = pathA.pathPoints;
                if (!ptsA || ptsA.length === 0) continue;

                // Get candidate paths from shared cells
                var candidates = {};
                var myCells = pathToCells[i];
                for (var mc = 0; mc < myCells.length; mc++) {
                    var cellPaths = spatialGrid[myCells[mc]];
                    if (cellPaths) {
                        for (var cp = 0; cp < cellPaths.length; cp++) {
                            var candidateIdx = cellPaths[cp];
                            if (candidateIdx > i) { // Only check each pair once
                                candidates[candidateIdx] = true;
                            }
                        }
                    }
                }

                // Check only candidate paths (nearby in spatial grid)
                for (var j in candidates) {
                    if (!candidates.hasOwnProperty(j)) continue;
                    j = parseInt(j);
                    var checkKey = i + "_" + j;
                    if (checkedPairs[checkKey]) continue;
                    checkedPairs[checkKey] = true;

                    var pathB = pathItems[j];
                    var ptsB = pathB.pathPoints;
                    if (!ptsB || ptsB.length === 0) continue;
                    var connected = false;

                    // DUPLICATE CHECK: Skip paths that are likely duplicates (same geometry at same location)
                    // Two paths with same number of points AND all anchors at same positions are duplicates
                    if (ptsA.length === ptsB.length && ptsA.length > 0) {
                        var allAnchorsMatch = true;
                        for (var dupCheck = 0; dupCheck < ptsA.length && allAnchorsMatch; dupCheck++) {
                            var dupDx = ptsA[dupCheck].anchor[0] - ptsB[dupCheck].anchor[0];
                            var dupDy = ptsA[dupCheck].anchor[1] - ptsB[dupCheck].anchor[1];
                            if (Math.sqrt(dupDx * dupDx + dupDy * dupDy) >= MIN_DIST) {
                                allAnchorsMatch = false;
                            }
                        }
                        if (allAnchorsMatch) {
                            // These are duplicate paths - don't connect them
                            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Skipping duplicate paths " + i + " <-> " + j);
                            continue;
                        }
                    }

                    // Find closest anchor-to-anchor distance for debugging
                    var closestDist = Infinity;
                    for (var cai = 0; cai < ptsA.length; cai++) {
                        for (var cbi = 0; cbi < ptsB.length; cbi++) {
                            var cdx = ptsA[cai].anchor[0] - ptsB[cbi].anchor[0];
                            var cdy = ptsA[cai].anchor[1] - ptsB[cbi].anchor[1];
                            var cdist = Math.sqrt(cdx * cdx + cdy * cdy);
                            if (cdist < closestDist) closestDist = cdist;
                        }
                    }
                    if (DEBUG_CONNECTIONS) {
                        addDebug("[CONN-DEBUG] Paths " + i + " <-> " + j + " closest dist=" + closestDist.toFixed(2) + " (maxDist=" + maxDist + ")");
                    }

                    // 1. Anchor-to-anchor with directional alignment
                    for (var ai = 0; ai < ptsA.length && !connected; ai++) {
                        for (var bi = 0; bi < ptsB.length && !connected; bi++) {
                            var aPos = ptsA[ai].anchor;
                            var bPos = ptsB[bi].anchor;
                            var dx = aPos[0] - bPos[0];
                            var dy = aPos[1] - bPos[1];
                            var dist = Math.sqrt(dx * dx + dy * dy);
                            if (dist >= MIN_DIST && dist <= maxDist) {
                                var dirsA = getAdjacentSegmentDirs(pathA, ai);
                                var dirsB = getAdjacentSegmentDirs(pathB, bi);
                                var aligned = false;
                                for (var da = 0; da < dirsA.length && !aligned; da++) {
                                    for (var db = 0; db < dirsB.length && !aligned; db++) {
                                        if (angleBetween(dirsA[da], dirsB[db]) <= ANGLE_THRESHOLD_DEG) {
                                            aligned = true;
                                        }
                                    }
                                }
                                if (aligned) connected = true;
                            }
                        }
                    }

                    // 2. Point-to-segment (T-junction), projection inside segment
                    // Use larger T_JUNCTION_DIST tolerance for branch paths meeting trunks
                    if (!connected) {
                        var bestTJDist = Infinity;
                        var bestTJt = -1;
                        for (var ai = 0; ai < ptsA.length && !connected; ai++) {
                            for (var bi = 0; bi < ptsB.length - 1 && !connected; bi++) {
                                var res = closestPointOnSegment(
                                    [ptsB[bi].anchor[0], ptsB[bi].anchor[1]],
                                    [ptsB[bi + 1].anchor[0], ptsB[bi + 1].anchor[1]],
                                    [ptsA[ai].anchor[0], ptsA[ai].anchor[1]]
                                );
                                var dx = ptsA[ai].anchor[0] - res.pt[0];
                                var dy = ptsA[ai].anchor[1] - res.pt[1];
                                var dist = Math.sqrt(dx * dx + dy * dy);
                                if (dist < bestTJDist) { bestTJDist = dist; bestTJt = res.t; }
                                if (dist >= MIN_DIST && dist <= T_JUNCTION_DIST && res.t > 0 && res.t < 1) {
                                    // Skip T-junctions at exactly 4.25pt - this is the carve-out gap distance
                                    // and almost certainly indicates a false connection to a carved path endpoint
                                    var CARVE_GAP_DIST = 4.25;
                                    var CARVE_GAP_TOL = 0.5;
                                    if (Math.abs(dist - CARVE_GAP_DIST) < CARVE_GAP_TOL) {
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] T-junction SKIPPED (carve-gap distance): path " + i + " point -> path " + j + " segment, dist=" + dist.toFixed(2));
                                    // Check if this T-junction point is at a crossover (intersection without vertex)
                                    } else if (isNearCrossover(res.pt, ptsA, ptsB, T_JUNCTION_DIST)) {
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] T-junction SKIPPED (crossover): path " + i + " point -> path " + j + " segment, dist=" + dist.toFixed(2));
                                    } else {
                                        connected = true;
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] T-junction detected: path " + i + " point -> path " + j + " segment, dist=" + dist.toFixed(2));
                                    }
                                }
                            }
                        }
                        for (var bi = 0; bi < ptsB.length && !connected; bi++) {
                            for (var ai = 0; ai < ptsA.length - 1 && !connected; ai++) {
                                var res = closestPointOnSegment(
                                    [ptsA[ai].anchor[0], ptsA[ai].anchor[1]],
                                    [ptsA[ai + 1].anchor[0], ptsA[ai + 1].anchor[1]],
                                    [ptsB[bi].anchor[0], ptsB[bi].anchor[1]]
                                );
                                var dx = ptsB[bi].anchor[0] - res.pt[0];
                                var dy = ptsB[bi].anchor[1] - res.pt[1];
                                var dist = Math.sqrt(dx * dx + dy * dy);
                                if (dist < bestTJDist) { bestTJDist = dist; bestTJt = res.t; }
                                if (dist >= MIN_DIST && dist <= T_JUNCTION_DIST && res.t > 0 && res.t < 1) {
                                    // Skip T-junctions at exactly 4.25pt - this is the carve-out gap distance
                                    var CARVE_GAP_DIST2 = 4.25;
                                    var CARVE_GAP_TOL2 = 0.5;
                                    if (Math.abs(dist - CARVE_GAP_DIST2) < CARVE_GAP_TOL2) {
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] T-junction SKIPPED (carve-gap distance): path " + j + " point -> path " + i + " segment, dist=" + dist.toFixed(2));
                                    // Check if this T-junction point is at a crossover (intersection without vertex)
                                    } else if (isNearCrossover(res.pt, ptsA, ptsB, T_JUNCTION_DIST)) {
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] T-junction SKIPPED (crossover): path " + j + " point -> path " + i + " segment, dist=" + dist.toFixed(2));
                                    } else {
                                        connected = true;
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] T-junction detected: path " + j + " point -> path " + i + " segment, dist=" + dist.toFixed(2));
                                    }
                                }
                            }
                        }
                        if (DEBUG_CONNECTIONS && !connected && bestTJDist < Infinity) {
                            addDebug("[CONN-DEBUG] T-junction check: best dist=" + bestTJDist.toFixed(2) + ", t=" + bestTJt.toFixed(3) + " (needs t in 0-1, dist <= " + T_JUNCTION_DIST + ")");
                        }
                    }

                    // 2b. Extended anchor-to-anchor: endpoints within T_JUNCTION_DIST (for branches connecting to branches)
                    // This handles cases where two branch endpoints are near each other but not close enough for strict maxDist
                    // BUT skip if the endpoints are near a crossover (intersection without vertex)
                    if (!connected) {
                        var ENDPOINT_TOLERANCE = 15; // Allow endpoint connections up to 15pt apart
                        for (var ai = 0; ai < ptsA.length && !connected; ai++) {
                            // Only check actual endpoints (first or last point)
                            if (ai !== 0 && ai !== ptsA.length - 1) continue;
                            for (var bi = 0; bi < ptsB.length && !connected; bi++) {
                                // Only check actual endpoints (first or last point)
                                if (bi !== 0 && bi !== ptsB.length - 1) continue;
                                var aPos = ptsA[ai].anchor;
                                var bPos = ptsB[bi].anchor;
                                var dx = aPos[0] - bPos[0];
                                var dy = aPos[1] - bPos[1];
                                var dist = Math.sqrt(dx * dx + dy * dy);
                                if (dist >= MIN_DIST && dist <= ENDPOINT_TOLERANCE) {
                                    // Check if either endpoint is near an ignored anchor (user marked to skip)
                                    if (existingIgnoredAnchors.length > 0 && (isNearIgnoredAnchor(aPos, existingIgnoredAnchors) || isNearIgnoredAnchor(bPos, existingIgnoredAnchors))) {
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Endpoint-to-endpoint SKIPPED (ignored anchor): path " + i + " <-> path " + j + ", dist=" + dist.toFixed(2));
                                    // Check if these endpoints are near a crossover (intersection without vertex)
                                    } else {
                                        var midPt = [(aPos[0] + bPos[0]) / 2, (aPos[1] + bPos[1]) / 2];
                                        if (isNearCrossover(midPt, ptsA, ptsB, ENDPOINT_TOLERANCE)) {
                                            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Endpoint-to-endpoint SKIPPED (crossover): path " + i + " <-> path " + j + ", dist=" + dist.toFixed(2));
                                        } else {
                                            connected = true;
                                            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Extended endpoint-to-endpoint: path " + i + " <-> path " + j + ", dist=" + dist.toFixed(2));
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 3. Segment intersection (only connect if path has vertex at intersection)
                    // If two lines cross and one has a PATH VERTEX at the intersection → compound together
                    // If two lines cross but neither has a vertex there → separate runs (crossover)
                    if (!connected) {
                        for (var ai = 0; ai < ptsA.length - 1 && !connected; ai++) {
                            for (var bi = 0; bi < ptsB.length - 1 && !connected; bi++) {
                                var intersectPt = getSegmentIntersectionPoint(
                                    ptsA[ai].anchor[0], ptsA[ai].anchor[1],
                                    ptsA[ai + 1].anchor[0], ptsA[ai + 1].anchor[1],
                                    ptsB[bi].anchor[0], ptsB[bi].anchor[1],
                                    ptsB[bi + 1].anchor[0], ptsB[bi + 1].anchor[1]
                                );
                                if (intersectPt) {
                                    // Check if this intersection is near an existing ignored anchor
                                    if (existingIgnoredAnchors.length > 0 && isNearIgnoredAnchor(intersectPt, existingIgnoredAnchors)) {
                                        if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Intersection SKIPPED (ignored anchor) at [" + intersectPt[0].toFixed(1) + "," + intersectPt[1].toFixed(1) + "]");
                                    } else {
                                        // Check if either path has a vertex (path point) at this intersection
                                        var pathAHasVertex = pathHasVertexNearPoint(ptsA, intersectPt);
                                        var pathBHasVertex = pathHasVertexNearPoint(ptsB, intersectPt);
                                        if (pathAHasVertex || pathBHasVertex) {
                                            // Path vertex exists at intersection - these paths are connected here
                                            connected = true;
                                            // Add to ignored anchors so no component gets placed here
                                            ignoredAnchorsOut.push([intersectPt[0], intersectPt[1]]);
                                            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Connected at intersection [" + intersectPt[0].toFixed(1) + "," + intersectPt[1].toFixed(1) + "] - path vertex present, added to ignore list");
                                        } else {
                                            // No path vertex at intersection - these are separate runs (crossover)
                                            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Skipping intersection at [" + intersectPt[0].toFixed(1) + "," + intersectPt[1].toFixed(1) + "] - no path vertex (crossover)");
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (connected) {
                        var key = pairKey(pathA, pathB);
                        if (!seen[key]) {
                            connections.push([pathA, pathB]);
                            seen[key] = true;
                            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] CONNECTED paths " + i + " <-> " + j);
                        }
                    } else if (DEBUG_CONNECTIONS && closestDist <= maxDist * 2) {
                        // Log near-misses for debugging
                        addDebug("[CONN-DEBUG] NOT connected paths " + i + " <-> " + j + " (dist=" + closestDist.toFixed(2) + " within 2x range but no connection found)");
                    }
                }
            }
            if (DEBUG_CONNECTIONS) addDebug("[CONN-DEBUG] Total connections found: " + connections.length);
            return connections;
        }

        function findConnectedComponents(pathItems, connections) {
            var parent = [];
            for (var i = 0; i < pathItems.length; i++) parent[i] = i;
            function find(x) { return parent[x] === x ? x : (parent[x] = find(parent[x])); }
            function union(a, b) {
                var ra = find(a), rb = find(b);
                if (ra !== rb) parent[ra] = rb;
            }
            function indexOf(arr, v) {
                for (var i = 0; i < arr.length; i++) if (arr[i] === v) return i;
                return -1;
            }

            for (var i = 0; i < connections.length; i++) {
                var a = indexOf(pathItems, connections[i][0]);
                var b = indexOf(pathItems, connections[i][1]);
                if (a !== -1 && b !== -1) union(a, b);
            }

            var groups = {};
            for (var i = 0; i < pathItems.length; i++) {
                var r = find(i);
                if (!groups[r]) groups[r] = [];
                groups[r].push(pathItems[i]);
            }

            var result = [];
            for (var k in groups) result.push(groups[k]);
            return result;
        }

        function getSafePathPoint(path, index) {
            try {
                if (!path) return null;
                var pts = path.pathPoints;
                if (!pts || index < 0 || index >= pts.length) return null;
                return pts[index];
            } catch (e) {
                return null;
            }
        }

        function isOpenPathEndpoint(path, index) {
            if (!path || typeof index !== "number") return false;
            try {
                if (path.closed) return false;
                var pts = path.pathPoints;
                if (!pts || pts.length === 0) return false;
                return index === 0 || index === pts.length - 1;
            } catch (e) {
                return false;
            }
        }

        function getEndpointOrientationInfo(path, index) {
            if (!isOpenPathEndpoint(path, index)) return null;
            try {
                var pts = path.pathPoints;
                if (!pts || pts.length < 2) return null;
                var point = pts[index];
                if (!point) return null;
                var neighbor = null;
                if (index === 0) {
                    neighbor = pts[1];
                } else if (index === pts.length - 1) {
                    neighbor = pts[pts.length - 2];
                }
                if (!neighbor) return null;

                var dx = neighbor.anchor[0] - point.anchor[0];
                var dy = neighbor.anchor[1] - point.anchor[1];
                var rotationOverride = getRotationOverride(path);
                var localDX = dx;
                var localDY = dy;
                if (rotationOverride !== null) {
                    var theta = (-rotationOverride) * (Math.PI / 180);
                    var cosT = Math.cos(theta);
                    var sinT = Math.sin(theta);
                    var tmpX = dx * cosT - dy * sinT;
                    var tmpY = dx * sinT + dy * cosT;
                    localDX = tmpX;
                    localDY = tmpY;
                }

                var absX = Math.abs(localDX);
                var absY = Math.abs(localDY);
                var type = null;
                if (absX > absY + 0.01) {
                    type = "horizontal";
                } else if (absY > absX + 0.01) {
                    type = "vertical";
                }

                if (!type) {
                    return {
                        type: null,
                        pos: [point.anchor[0], point.anchor[1]],
                        rotation: rotationOverride,
                        direction: null
                    };
                }

                var baseAngle = rotationOverride !== null ? rotationOverride : 0;
                if (type === "vertical") {
                    baseAngle += 90;
                }
                var angleRad = baseAngle * (Math.PI / 180);
                var dir = [Math.cos(angleRad), Math.sin(angleRad)];
                var mag = Math.sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
                if (mag > 0) {
                    dir[0] /= mag;
                    dir[1] /= mag;
                } else {
                    dir = null;
                }

                return {
                    type: type,
                    pos: [point.anchor[0], point.anchor[1]],
                    rotation: rotationOverride,
                    direction: dir
                };
            } catch (e) {
                return null;
            }
        }

        function collectEndpointConnections(paths, tolerance) {
            var result = { pairs: [], endpointSegments: [] };
            if (!paths || paths.length === 0) return result;

            var tol = (typeof tolerance === "number" && tolerance >= 0) ? tolerance : CONNECTION_DIST;
            var tol2 = tol * tol;
            var endpointsByLayer = {};
            var segmentsByLayer = {};

            for (var i = 0; i < paths.length; i++) {
                var path = paths[i];
                if (!path) continue;

                var layerName = "";
                try {
                    layerName = (path.layer && path.layer.name) ? path.layer.name : "";
                } catch (e) {
                    layerName = "";
                }
                if (!isDuctworkLineLayer(layerName)) continue;

                var pts = null;
                try { pts = path.pathPoints; } catch (e) { pts = null; }
                if (!pts || pts.length === 0) continue;

                if (!segmentsByLayer[layerName]) segmentsByLayer[layerName] = [];
                var segList = segmentsByLayer[layerName];

                for (var s = 0; s < pts.length - 1; s++) {
                    segList.push({ path: path, index1: s, index2: s + 1 });
                }
                if (path.closed && pts.length > 1) {
                    segList.push({ path: path, index1: pts.length - 1, index2: 0 });
                }

                if (!path.closed && pts.length > 0) {
                    if (!endpointsByLayer[layerName]) endpointsByLayer[layerName] = [];
                    var endpointStore = endpointsByLayer[layerName];
                    endpointStore.push({
                        path: path,
                        index: 0,
                        pos: [pts[0].anchor[0], pts[0].anchor[1]]
                    });
                    if (pts.length > 1) {
                        var lastIdx = pts.length - 1;
                        endpointStore.push({
                            path: path,
                            index: lastIdx,
                            pos: [pts[lastIdx].anchor[0], pts[lastIdx].anchor[1]]
                        });
                    }
                }
            }

            // Same-layer endpoint-to-endpoint connections
            for (var layer in endpointsByLayer) {
                if (!endpointsByLayer.hasOwnProperty(layer)) continue;
                var endpoints = endpointsByLayer[layer];
                if (!endpoints || endpoints.length < 2) continue;

                for (var a = 0; a < endpoints.length; a++) {
                    var epA = endpoints[a];
                    if (!epA || !epA.path) continue;
                    for (var b = a + 1; b < endpoints.length; b++) {
                        var epB = endpoints[b];
                        if (!epB || !epB.path) continue;
                        if (epA.path === epB.path) continue;
                        try {
                            if (dist2(epA.pos, epB.pos) <= tol2) {
                                result.pairs.push({
                                    a: { path: epA.path, index: epA.index },
                                    b: { path: epB.path, index: epB.index },
                                    center: [
                                        (epA.pos[0] + epB.pos[0]) / 2,
                                        (epA.pos[1] + epB.pos[1]) / 2
                                    ]
                                });
                            }
                        } catch (e) {
                            // Ignore distance failures
                        }
                    }
                }
            }

            // CROSS-LAYER endpoint-to-endpoint connections (e.g., Blue to Green ductwork)
            var allLayerNames = [];
            for (var ln in endpointsByLayer) {
                if (endpointsByLayer.hasOwnProperty(ln)) allLayerNames.push(ln);
            }
            for (var li = 0; li < allLayerNames.length; li++) {
                for (var lj = li + 1; lj < allLayerNames.length; lj++) {
                    var layerA = allLayerNames[li];
                    var layerB = allLayerNames[lj];
                    var endpointsA = endpointsByLayer[layerA];
                    var endpointsB = endpointsByLayer[layerB];
                    if (!endpointsA || !endpointsB) continue;

                    for (var ai = 0; ai < endpointsA.length; ai++) {
                        var epA = endpointsA[ai];
                        if (!epA || !epA.path) continue;
                        for (var bi = 0; bi < endpointsB.length; bi++) {
                            var epB = endpointsB[bi];
                            if (!epB || !epB.path) continue;
                            try {
                                if (dist2(epA.pos, epB.pos) <= tol2) {
                                    addDebug("[collectEndpointConnections] Found CROSS-LAYER connection: '" + layerA + "' endpoint [" + epA.pos[0].toFixed(2) + ", " + epA.pos[1].toFixed(2) + "] <-> '" + layerB + "' endpoint [" + epB.pos[0].toFixed(2) + ", " + epB.pos[1].toFixed(2) + "]");
                                    result.pairs.push({
                                        a: { path: epA.path, index: epA.index },
                                        b: { path: epB.path, index: epB.index },
                                        center: [
                                            (epA.pos[0] + epB.pos[0]) / 2,
                                            (epA.pos[1] + epB.pos[1]) / 2
                                        ],
                                        crossLayer: true
                                    });
                                }
                            } catch (e) {
                                // Ignore distance failures
                            }
                        }
                    }
                }
            }

            for (var layerName in endpointsByLayer) {
                if (!endpointsByLayer.hasOwnProperty(layerName)) continue;
                var endpointsList = endpointsByLayer[layerName];
                if (!endpointsList || endpointsList.length === 0) continue;
                var segments = segmentsByLayer[layerName] || [];
                if (segments.length === 0) continue;

                for (var ei = 0; ei < endpointsList.length; ei++) {
                    var endpoint = endpointsList[ei];
                    if (!endpoint || !endpoint.path) continue;

                    var best = null;
                    for (var si = 0; si < segments.length; si++) {
                        var seg = segments[si];
                        if (!seg || !seg.path) continue;
                        if (seg.path === endpoint.path) continue;

                        var p1 = getSafePathPoint(seg.path, seg.index1);
                        var p2 = getSafePathPoint(seg.path, seg.index2);
                        if (!p1 || !p2) continue;

                        try {
                            if (dist2(endpoint.pos, p1.anchor) <= tol2 || dist2(endpoint.pos, p2.anchor) <= tol2) {
                                continue;
                            }
                        } catch (e) {
                            // Ignore coincidence checks that fail
                        }

                        var res = null;
                        try {
                            res = closestPointOnSegment(
                                [p1.anchor[0], p1.anchor[1]],
                                [p2.anchor[0], p2.anchor[1]],
                                [endpoint.pos[0], endpoint.pos[1]]
                            );
                        } catch (e) {
                            res = null;
                        }
                        if (!res || typeof res.t !== "number") continue;
                        if (res.t < -1e-6 || res.t > 1 + 1e-6) continue;

                        var dx = endpoint.pos[0] - res.pt[0];
                        var dy = endpoint.pos[1] - res.pt[1];
                        var d2 = dx * dx + dy * dy;
                        if (d2 > tol2) continue;

                        if (!best || d2 < best.dist2) {
                            best = {
                                dist2: d2,
                                point: [res.pt[0], res.pt[1]],
                                segment: seg
                            };
                        }
                    }

                    if (best) {
                        result.endpointSegments.push({
                            endpoint: { path: endpoint.path, index: endpoint.index },
                            segment: { path: best.segment.path, index1: best.segment.index1, index2: best.segment.index2 },
                            target: best.point.slice()
                        });
                    }
                }
            }

            return result;
        }

        function endpointParticipatesInPairs(endpointRef, pairs) {
            if (!endpointRef || !pairs) return false;
            for (var i = 0; i < pairs.length; i++) {
                var pair = pairs[i];
                if (!pair) continue;
                if (pair.a && pair.a.path === endpointRef.path && pair.a.index === endpointRef.index) return true;
                if (pair.b && pair.b.path === endpointRef.path && pair.b.index === endpointRef.index) return true;
            }
            return false;
        }

        function restoreEndpointConnections(connections) {
            if (!connections) return false;
            var moved = false;

            function midpoint(a, b) {
                return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
            }

            function projectPointOntoLine(point, linePoint, lineDir) {
                if (!lineDir || !linePoint) return point;
                var px = point[0] - linePoint[0];
                var py = point[1] - linePoint[1];
                var dot = px * lineDir[0] + py * lineDir[1];
                return [linePoint[0] + lineDir[0] * dot, linePoint[1] + lineDir[1] * dot];
            }

            function intersectLines(p0, d0, p1, d1) {
                if (!d0 || !d1) return null;
                var det = d0[0] * d1[1] - d0[1] * d1[0];
                if (Math.abs(det) < 1e-6) return null;
                var diffX = p1[0] - p0[0];
                var diffY = p1[1] - p0[1];
                var t = (diffX * d1[1] - diffY * d1[0]) / det;
                return [p0[0] + d0[0] * t, p0[1] + d0[1] * t];
            }

            var pairs = connections.pairs || [];
            for (var i = 0; i < pairs.length; i++) {
                var pair = pairs[i];
                if (!pair || !pair.a || !pair.b) continue;

                var ptA = getSafePathPoint(pair.a.path, pair.a.index);
                var ptB = getSafePathPoint(pair.b.path, pair.b.index);
                if (!ptA || !ptB) continue;

                var infoA = getEndpointOrientationInfo(pair.a.path, pair.a.index);
                var infoB = getEndpointOrientationInfo(pair.b.path, pair.b.index);

                var baseCenter = pair.center ? [pair.center[0], pair.center[1]] : midpoint(ptA.anchor, ptB.anchor);
                var finalPos = baseCenter;

                if (infoA && infoA.direction && infoB && infoB.direction) {
                    var intersect = intersectLines(infoA.pos, infoA.direction, infoB.pos, infoB.direction);
                    if (intersect) {
                        finalPos = intersect;
                    } else {
                        finalPos = projectPointOntoLine(finalPos, infoA.pos, infoA.direction);
                        finalPos = projectPointOntoLine(finalPos, infoB.pos, infoB.direction);
                    }
                } else {
                    if (infoA && infoA.direction) {
                        finalPos = projectPointOntoLine(finalPos, infoA.pos, infoA.direction);
                    }
                    if (infoB && infoB.direction) {
                        finalPos = projectPointOntoLine(finalPos, infoB.pos, infoB.direction);
                    }
                }

                if (!almostEqualPoints(ptA.anchor, finalPos)) {
                    ptA.anchor = finalPos.slice();
                    ptA.leftDirection = finalPos.slice();
                    ptA.rightDirection = finalPos.slice();
                    moved = true;
                }
                if (!almostEqualPoints(ptB.anchor, finalPos)) {
                    ptB.anchor = finalPos.slice();
                    ptB.leftDirection = finalPos.slice();
                    ptB.rightDirection = finalPos.slice();
                    moved = true;
                }
            }

            var segmentConnections = connections.endpointSegments || [];
            if (segmentConnections.length === 0) return moved;

            for (var j = 0; j < segmentConnections.length; j++) {
                var entry = segmentConnections[j];
                if (!entry || !entry.endpoint || !entry.segment) continue;
                if (endpointParticipatesInPairs(entry.endpoint, pairs)) continue;

                var endpointPoint = getSafePathPoint(entry.endpoint.path, entry.endpoint.index);
                var segPt1 = getSafePathPoint(entry.segment.path, entry.segment.index1);
                var segPt2 = getSafePathPoint(entry.segment.path, entry.segment.index2);
                if (!endpointPoint || !segPt1 || !segPt2) continue;

                var targetPt = null;
                var res = null;
                try {
                    res = closestPointOnSegment(
                        [segPt1.anchor[0], segPt1.anchor[1]],
                        [segPt2.anchor[0], segPt2.anchor[1]],
                        [endpointPoint.anchor[0], endpointPoint.anchor[1]]
                    );
                } catch (e) {
                    res = null;
                }
                if (res && typeof res.t === "number" && res.t >= -1e-4 && res.t <= 1 + 1e-4) {
                    targetPt = [res.pt[0], res.pt[1]];
                } else if (entry.target) {
                    targetPt = entry.target.slice();
                }
                if (!targetPt) continue;

                var endpointInfo = getEndpointOrientationInfo(entry.endpoint.path, entry.endpoint.index);
                if (endpointInfo && endpointInfo.direction) {
                    targetPt = projectPointOntoLine(targetPt, endpointInfo.pos, endpointInfo.direction);
                }

                if (!almostEqualPoints(endpointPoint.anchor, targetPt)) {
                    endpointPoint.anchor = targetPt.slice();
                    endpointPoint.leftDirection = targetPt.slice();
                    endpointPoint.rightDirection = targetPt.slice();
                    moved = true;
                }
            }

            return moved;
        }

        // --- FILTER PATHS BY LAYER (FIXED TO HANDLE INVALID PATHS) ---
        function getPathsOnLayer(allPaths, layerName) {
            var layerPaths = [];
            for (var i = 0; i < allPaths.length; i++) {
                var path = allPaths[i];
                try {
                    // Validate that the path is still valid
                    if (!path || !path.parent) continue;

                    var pathLayer = path.parent;
                    // Walk up to find the actual layer
                    while (pathLayer && pathLayer.typename !== "Layer") {
                        pathLayer = pathLayer.parent;
                    }
                    if (pathLayer && pathLayer.name === layerName) {
                        layerPaths.push(path);
                    }
                } catch (e) {
                    // Skip invalid paths silently
                    continue;
                }
            }
            return layerPaths;
        }

        // --- REBUILD PATHS ON LAYER (SAFER ALTERNATIVE) ---
        function getPathsOnLayerSafe(layerName) {
            var layerPaths = [];
            try {
                var targetLayer = findLayerByNameDeep(layerName);
                if (!targetLayer || targetLayer.locked || !targetLayer.visible) return [];

                function collectValidPaths(container) {
                    try {
                        if (container.typename !== 'Layer' && (container.locked || !container.visible)) {
                            return;
                        }
                        for (var i = 0; i < container.pathItems.length; i++) {
                            var path = container.pathItems[i];
                            if (!path.guides && !path.clipping) {
                                layerPaths.push(path);
                            }
                        }
                        for (var i = 0; i < container.groupItems.length; i++) {
                            collectValidPaths(container.groupItems[i]);
                        }
                        for (var i = 0; i < container.compoundPathItems.length; i++) {
                            var compound = container.compoundPathItems[i];
                            for (var j = 0; j < compound.pathItems.length; j++) {
                                if (!compound.pathItems[j].guides && !compound.pathItems[j].clipping) {
                                    layerPaths.push(compound.pathItems[j]);
                                }
                            }
                        }
                    } catch (e) {
                        // Skip problematic containers
                    }
                }
                collectValidPaths(targetLayer);
            } catch (e) {
                // Layer doesn't exist or other error
            }
            return layerPaths;
        }

        // --- LAYER FUNCTIONS FOR ANCHOR CREATION ---
        function ensureIgnoredLayer() {
            // Create "Ignored" layer if missing and place it just below the "Frame" layer (if present).
            try {
                var existing = findLayerByNameDeep("Ignored");
                if (existing) return existing;
            } catch (eExisting) { }

            try {
                var newLayer = doc.layers.add();
                newLayer.name = "Ignored";
                // Try to place it just below Frame
                try {
                    var frameLayer = findLayerByNameDeep("Frame");
                    if (frameLayer) newLayer.move(frameLayer, ElementPlacement.PLACEAFTER);
                } catch (e2) {
                    // Frame not found - leave new layer at its default position
                }
                return newLayer;
            } catch (e3) {
                // If creation fails, return null
                return null;
            }
        }

        function getOrCreateLayer(name) {
            // Robust layer creation: avoid creating layers named e.g. "Array" when callers accidentally pass arrays/objects.
            try {
                // If an array was passed, try to extract a sensible string name
                if (Object.prototype.toString.call(name) === '[object Array]') {
                    for (var ai = 0; ai < name.length; ai++) {
                        if (typeof name[ai] === 'string' && name[ai].length > 0) {
                            name = name[ai];
                            break;
                        }
                    }
                }

                // If name is not a non-empty string, fall back to the Ignored layer rather than creating an incorrect layer
                if (typeof name !== 'string' || name.length === 0) {
                    return ensureIgnoredLayer();
                }

                // Normal case: try to fetch existing layer
                var existingLayer = findLayerByNameDeep(name);
                if (existingLayer) return existingLayer;
            } catch (e) {
                // If layer does not exist and name is valid string, create it
                try {
                    if (typeof name === 'string' && name.length > 0) {
                        var newL = doc.layers.add();
                        newL.name = name;
                        return newL;
                    }
                } catch (e2) {
                    // ignore
                }
                // Fall back
                return ensureIgnoredLayer();
            }
        }

        function createAnchorPoint(layer, position, rotationOverride) {
            if (!layer || !position) return null;
            if (typeof rotationOverride === 'number' && isFinite(rotationOverride)) {
                rotationOverride = normalizeAngle(rotationOverride);
            } else {
                rotationOverride = null;
            }

            // Find the actual Layer object (walk up parents if a group or sublayer was passed)
            var layerObj = layer;
            try {
                while (layerObj && layerObj.typename && layerObj.typename !== 'Layer') {
                    if (!layerObj.parent) break;
                    layerObj = layerObj.parent;
                }
            } catch (e) {
                layerObj = layer; // fallback
            }

            // Record original visibility/locked state so we can restore it
            var prevLocked = null, prevVisible = null;
            try {
                if (layerObj && layerObj.typename === 'Layer') {
                    prevLocked = layerObj.locked;
                    prevVisible = layerObj.visible;
                    // Temporarily make editable so we can add points; restore later
                    try { layerObj.locked = false; } catch (e) { }
                    try { layerObj.visible = true; } catch (e) { }
                }
            } catch (e) {
                // ignore
            }

            function finalizeAnchor(pathItem) {
                if (!pathItem) return pathItem;
                if (rotationOverride !== null) {
                    setPointRotation(pathItem, rotationOverride);
                } else {
                    clearPointRotation(pathItem);
                }
                return pathItem;
            }

            try {
                var newPath = layer.pathItems.add();
                var p = newPath.pathPoints.add();
                try {
                    p.anchor = position;
                    p.leftDirection = position;
                    p.rightDirection = position;
                    p.pointType = PointType.CORNER;
                    newPath.stroked = false;
                    newPath.filled = false;
                    var finalized = finalizeAnchor(newPath);
                    markCreatedPath(finalized);
                    return finalized;
                } catch (innerErr) {
                    // Failed to write to the requested layer despite unlocking; fall through to fallback
                    try { newPath.remove(); } catch (e) { }
                    throw innerErr;
                }
            } catch (e) {
                // Fallback: try to add the anchor to the Ignored layer instead (safe fallback)
                try {
                    var alt = ensureIgnoredLayer();
                    if (alt && !alt.locked) {
                        var np = alt.pathItems.add();
                        var pp = np.pathPoints.add();
                        pp.anchor = position;
                        pp.leftDirection = position;
                        pp.rightDirection = position;
                        pp.pointType = PointType.CORNER;
                        np.stroked = false;
                        np.filled = false;
                        var finalizedAlt = finalizeAnchor(np);
                        markCreatedPath(finalizedAlt);
                        return finalizedAlt;
                    }
                } catch (e2) {
                    // give up
                }
                return null;
            } finally {
                // Restore previous layer state where possible
                try {
                    if (layerObj && layerObj.typename === 'Layer') {
                        if (prevLocked !== null) try { layerObj.locked = prevLocked; } catch (e) { }
                        if (prevVisible !== null) try { layerObj.visible = prevVisible; } catch (e) { }
                    }
                } catch (e) {
                    // ignore restore failures
                }
            }
        }

        function isAnchorPointPath(path) {
            if (!path) return false;
            try { if (path.guides || path.clipping) return false; } catch (eGuides) { return false; }
            try { if (path.pathPoints.length !== 1) return false; } catch (eLen) { return false; }
            try {
                if (path.filled) return false;
                if (path.stroked && path.strokeWidth > 0) return false;
            } catch (eStroke) { }
            return true;
        }

        function getPathsOnLayerAll(layerName) {
            var paths = [];

            function pushPath(path) {
                if (!path) return;
                for (var idx = 0; idx < paths.length; idx++) {
                    if (paths[idx] === path) return;
                }
                paths.push(path);
            }

            function collectPathsFrom(container) {
                if (!container) return;
                if (container.typename !== 'Layer' && (container.locked || !container.visible)) {
                    return;
                }
                for (var i = 0; i < container.pathItems.length; i++) {
                    pushPath(container.pathItems[i]);
                }
                for (var g = 0; g < container.groupItems.length; g++) {
                    collectPathsFrom(container.groupItems[g]);
                }
                for (var c = 0; c < container.compoundPathItems.length; c++) {
                    var compound = container.compoundPathItems[c];
                    for (var j = 0; j < compound.pathItems.length; j++) {
                        pushPath(compound.pathItems[j]);
                    }
                }
            }

            var candidateNames = [];
            var candidateSeen = {};

            function addCandidate(name) {
                if (!name) return;
                if (typeof name !== "string") name = "" + name;
                if (typeof trimString === "function") {
                    name = trimString(name);
                } else {
                    name = name.replace(/^\s+|\s+$/g, "");
                }
                if (!name) return;
                if (!candidateSeen[name]) {
                    candidateSeen[name] = true;
                    candidateNames.push(name);
                }
            }

            addCandidate(layerName);

            for (var c = 0; c < candidateNames.length; c++) {
                var candidateName = candidateNames[c];
                var layer = null;
                try { layer = findLayerByNameDeep(candidateName); } catch (eLayer) { layer = null; }
                if (!layer) continue;

                var prevLocked = null;
                var prevVisible = null;
                try {
                    prevLocked = layer.locked;
                    layer.locked = false;
                } catch (eLock) { }
                try {
                    prevVisible = layer.visible;
                    layer.visible = true;
                } catch (eVis) { }

                try {
                    collectPathsFrom(layer);
                } catch (eCollect) {
                    // ignore per-layer collection failures
                } finally {
                    try {
                        if (prevLocked !== null) layer.locked = prevLocked;
                    } catch (eRestoreLock) { }
                    try {
                        if (prevVisible !== null) layer.visible = prevVisible;
                    } catch (eRestoreVis) { }
                }
            }

            return paths;
        }

        function getPathsOnLayerSelected(layerName) {
            return filterPathsToSelected(getPathsOnLayerAll(layerName));
        }

        // --- NEW: GET IGNORED ANCHOR POINTS (DIRECT LAYER ITERATION APPROACH) ---
        function getIgnoredAnchorPoints() {
            var ignoredAnchors = [];
            var possibleLayerNames = ["Ignore", "Ignored", "ignore", "ignored"];
            var ignoredLayer = null;

            // Iterate through ALL layers to find ignore layer (bypasses visibility issues)
            for (var i = 0; i < doc.layers.length; i++) {
                var layer = doc.layers[i];
                for (var nameIdx = 0; nameIdx < possibleLayerNames.length; nameIdx++) {
                    if (layer.name === possibleLayerNames[nameIdx]) {
                        ignoredLayer = layer;
                        break;
                    }
                }
                if (ignoredLayer) break;
            }

            if (!ignoredLayer) {
                return []; // No ignore layer found
            }

            // Direct access to layer contents without visibility manipulation
            function collectAnchorsDirectly(container) {
                try {
                    // Direct iteration through pathItems array
                    for (var i = 0; i < container.pathItems.length; i++) {
                        try {
                            var path = container.pathItems[i];
                            // Direct access to pathPoints
                            for (var j = 0; j < path.pathPoints.length; j++) {
                                try {
                                    var anchor = path.pathPoints[j].anchor;
                                    ignoredAnchors.push([anchor[0], anchor[1]]);
                                    addDebug("[IGNORE-COLLECT] Found ignore anchor at [" + anchor[0].toFixed(1) + "," + anchor[1].toFixed(1) + "]");
                                } catch (e) {
                                    // Skip this anchor point
                                }
                            }
                        } catch (e) {
                            // Skip this path
                        }
                    }

                    // Direct iteration through groupItems
                    for (var k = 0; k < container.groupItems.length; k++) {
                        try {
                            collectAnchorsDirectly(container.groupItems[k]);
                        } catch (e) {
                            // Skip this group
                        }
                    }

                    // Direct iteration through compoundPathItems
                    for (var m = 0; m < container.compoundPathItems.length; m++) {
                        try {
                            var compound = container.compoundPathItems[m];
                            for (var n = 0; n < compound.pathItems.length; n++) {
                                try {
                                    var path = compound.pathItems[n];
                                    for (var p = 0; p < path.pathPoints.length; p++) {
                                        try {
                                            var anchor = path.pathPoints[p].anchor;
                                            ignoredAnchors.push([anchor[0], anchor[1]]);
                                        } catch (e) {
                                            // Skip this anchor point
                                        }
                                    }
                                } catch (e) {
                                    // Skip this path
                                }
                            }
                        } catch (e) {
                            // Skip this compound path
                        }
                    }

                    // Handle sublayers
                    if (container.layers) {
                        for (var s = 0; s < container.layers.length; s++) {
                            try {
                                collectAnchorsDirectly(container.layers[s]);
                            } catch (e) {
                                // Skip this sublayer
                            }
                        }
                    }
                } catch (e) {
                    // Skip this container entirely
                }
            }

            collectAnchorsDirectly(ignoredLayer);
            return ignoredAnchors;
        }

        // --- NEW: GET EXISTING ANCHOR POINTS FROM TARGET LAYERS ---
        function getExistingAnchorPoints(layerNames) {
            var existingAnchors = [];

            function collectExistingAnchors(container) {
                try {
                    for (var j = 0; j < container.pathItems.length; j++) {
                        try {
                            var path = container.pathItems[j];
                            for (var k = 0; k < path.pathPoints.length; k++) {
                                try {
                                    var anchor = path.pathPoints[k].anchor;
                                    existingAnchors.push([anchor[0], anchor[1]]);
                                } catch (e) {
                                    // Skip this anchor point
                                }
                            }
                        } catch (e) {
                            // Skip this path
                        }
                    }

                    // IMPORTANT: Also collect center positions from PlacedItems (Units, Registers, etc.)
                    // This prevents creating new anchor points when placed items already exist at those locations
                    for (var pi = 0; pi < container.placedItems.length; pi++) {
                        try {
                            var placed = container.placedItems[pi];
                            var gb = placed.geometricBounds;
                            var centerX = (gb[0] + gb[2]) / 2;
                            var centerY = (gb[1] + gb[3]) / 2;
                            existingAnchors.push([centerX, centerY]);
                        } catch (e) {
                            // Skip this placed item
                        }
                    }

                    for (var m = 0; m < container.groupItems.length; m++) {
                        try {
                            collectExistingAnchors(container.groupItems[m]);
                        } catch (e) {
                            // Skip this group
                        }
                    }

                    for (var n = 0; n < container.compoundPathItems.length; n++) {
                        try {
                            var compound = container.compoundPathItems[n];
                            for (var p = 0; p < compound.pathItems.length; p++) {
                                try {
                                    var compoundPath = compound.pathItems[p];
                                    for (var q = 0; q < compoundPath.pathPoints.length; q++) {
                                        try {
                                            var compoundAnchor = compoundPath.pathPoints[q].anchor;
                                            existingAnchors.push([compoundAnchor[0], compoundAnchor[1]]);
                                        } catch (e) {
                                            // Skip this anchor point
                                        }
                                    }
                                } catch (e) {
                                    // Skip this path
                                }
                            }
                        } catch (e) {
                            // Skip this compound path
                        }
                    }

                    if (container.layers) {
                        for (var s = 0; s < container.layers.length; s++) {
                            try {
                                collectExistingAnchors(container.layers[s]);
                            } catch (e) {
                                // Skip this sublayer
                            }
                        }
                    }
                } catch (e) {
                    // Skip this container entirely
                }
            }

            for (var layerIdx = 0; layerIdx < layerNames.length; layerIdx++) {
                var layerName = layerNames[layerIdx];
                var targetLayer = null;
                try { targetLayer = findLayerByNameDeep(layerName); } catch (eFind) { targetLayer = null; }
                if (!targetLayer) continue;
                collectExistingAnchors(targetLayer);
            }

            return existingAnchors;
        }

        // --- NEW: GET ONLY PLACED ITEM POSITIONS (NOT ANCHOR PATHS) ---
        // Used for skip-final-ortho check - only actual placed components should trigger skip
        function getExistingPlacedItemPositions(layerNames) {
            var placedPositions = [];

            function collectPlacedItems(container) {
                try {
                    // ONLY collect center positions from PlacedItems (actual components)
                    for (var pi = 0; pi < container.placedItems.length; pi++) {
                        try {
                            var placed = container.placedItems[pi];
                            var gb = placed.geometricBounds;
                            var centerX = (gb[0] + gb[2]) / 2;
                            var centerY = (gb[1] + gb[3]) / 2;
                            placedPositions.push([centerX, centerY]);
                        } catch (e) {
                            // Skip this placed item
                        }
                    }

                    for (var m = 0; m < container.groupItems.length; m++) {
                        try {
                            collectPlacedItems(container.groupItems[m]);
                        } catch (e) {
                            // Skip this group
                        }
                    }

                    if (container.layers) {
                        for (var s = 0; s < container.layers.length; s++) {
                            try {
                                collectPlacedItems(container.layers[s]);
                            } catch (e) {
                                // Skip this sublayer
                            }
                        }
                    }
                } catch (e) {
                    // Skip this container entirely
                }
            }

            for (var layerIdx = 0; layerIdx < layerNames.length; layerIdx++) {
                var layerName = layerNames[layerIdx];
                var targetLayer = null;
                try { targetLayer = findLayerByNameDeep(layerName); } catch (eFind) { targetLayer = null; }
                if (!targetLayer) continue;
                collectPlacedItems(targetLayer);
            }

            return placedPositions;
        }

        // --- NEW: CHECK IF POINT OVERLAPS WITH EXISTING ANCHORS ---
        function isPointAlreadyPlaced(position, existingAnchors, tolerance) {
            try {
                existingAnchors = existingAnchors || [];
                var threshold = (typeof tolerance === 'number' && tolerance >= 0) ? tolerance : CLOSE_DIST;
                for (var i = 0; i < existingAnchors.length; i++) {
                    if (dist(position, existingAnchors[i]) <= threshold) {
                        return true;
                    }
                }

                // Additionally ensure we respect any Circular Registers already placed on their layer.
                // This prevents the script from adding other ductwork pieces at the same spot.
                try {
                    var circAnchors = getExistingAnchorPoints(["Circular Registers"]);
                    for (var ci = 0; ci < circAnchors.length; ci++) {
                        if (dist(position, circAnchors[ci]) <= threshold) return true;
                    }
                } catch (e) {
                    // ignore failures collecting circular anchors
                }
            } catch (e) {
                // defensive: if anything goes wrong, assume not placed to avoid false positives
            }
            return false;
        }

        function mergeAnchorsOnLayer(layerName, tolerance) {
            var threshold = (typeof tolerance === 'number' && tolerance >= 0) ? tolerance : UNIT_MERGE_DIST;
            if (!layerName || !doc || threshold <= 0) return;

            var layer = null;
            try { layer = findLayerByNameDeep(layerName); } catch (e) { layer = null; }
            if (!layer) return;

            var prevLocked = null, prevVisible = null;
            try { prevLocked = layer.locked; layer.locked = false; } catch (e) { }
            try { prevVisible = layer.visible; layer.visible = true; } catch (e) { }

            try {
                var tol2 = threshold * threshold;
                var paths = filterPathsToCreated(getPathsOnLayerAll(layerName) || []);
                if (!paths || paths.length === 0) return;

                var entries = [];
                for (var i = 0; i < paths.length; i++) {
                    var pathItem = paths[i];
                    try {
                        if (!pathItem || pathItem.guides || pathItem.clipping) continue;
                        if (!pathItem.pathPoints || pathItem.pathPoints.length !== 1) continue;
                        if (pathItem.locked) continue;
                        var anchor = pathItem.pathPoints[0].anchor.slice();
                        if (!shouldProcessPath(pathItem)) continue;
                        entries.push({ path: pathItem, anchor: anchor });
                    } catch (e) {
                        continue;
                    }
                }

                if (entries.length <= 1) return;

                var clusters = [];
                for (var idx = 0; idx < entries.length; idx++) {
                    var entry = entries[idx];
                    var assigned = false;
                    for (var c = 0; c < clusters.length && !assigned; c++) {
                        var cluster = clusters[c];
                        var cx = cluster.sumX / cluster.count;
                        var cy = cluster.sumY / cluster.count;
                        var dx = entry.anchor[0] - cx;
                        var dy = entry.anchor[1] - cy;
                        if (dx * dx + dy * dy <= tol2) {
                            cluster.points.push(entry);
                            cluster.sumX += entry.anchor[0];
                            cluster.sumY += entry.anchor[1];
                            cluster.count++;
                            assigned = true;
                        }
                    }
                    if (!assigned) {
                        clusters.push({
                            points: [entry],
                            sumX: entry.anchor[0],
                            sumY: entry.anchor[1],
                            count: 1
                        });
                    }
                }

                for (var ci = 0; ci < clusters.length; ci++) {
                    var cluster = clusters[ci];
                    if (!cluster || cluster.count <= 1) continue;

                    var avg = [cluster.sumX / cluster.count, cluster.sumY / cluster.count];
                    for (var pi = 0; pi < cluster.points.length; pi++) {
                        var info = cluster.points[pi];
                        try {
                            var point = info.path.pathPoints[0];
                            point.anchor = avg;
                            point.leftDirection = avg;
                            point.rightDirection = avg;
                        } catch (e) { }
                    }

                    for (var ri = 1; ri < cluster.points.length; ri++) {
                        var redundant = cluster.points[ri];
                        try { if (redundant.path.locked) redundant.path.locked = false; } catch (e) { }
                        try { redundant.path.remove(); } catch (e) { }
                    }
                }
            } catch (e) {
                // swallow merge errors
            } finally {
                try { if (prevLocked !== null) layer.locked = prevLocked; } catch (e) { }
                try { if (prevVisible !== null) layer.visible = prevVisible; } catch (e) { }
            }
        }

        // --- *** FIX: ADDED MISSING isPointIgnored FUNCTION (uses IGNORED_DIST) ***
        function isPointIgnored(position, ignoredAnchors) {
            var threshold = (typeof IGNORED_DIST !== 'undefined') ? IGNORED_DIST : CLOSE_DIST;
            for (var i = 0; i < ignoredAnchors.length; i++) {
                if (dist(position, ignoredAnchors[i]) <= threshold) {
                    return true;
                }
            }
            return false;
        }


        // --- NEW: REMOVE PLACED ART AND ANCHORS THAT ALIGN WITH IGNORED POINTS ---
        function removeConflictingArtAndAnchors(ignoredAnchors) {
            var targetLayers = [
                "Thermostats", "Units", "Secondary Exhaust", "Exhaust Registers",
                "Orange Register", "Rectangular Registers", "Square Registers"
            ];

            for (var layerIdx = 0; layerIdx < targetLayers.length; layerIdx++) {
                var layerName = targetLayers[layerIdx];

                // Find the target layer
                var targetLayer = null;
                for (var i = 0; i < doc.layers.length; i++) {
                    if (doc.layers[i].name === layerName) {
                        targetLayer = doc.layers[i];
                        break;
                    }
                }

                if (!targetLayer) continue; // Layer doesn't exist

                // Store original layer state and make accessible
                var originalVisible = targetLayer.visible;
                var originalLocked = targetLayer.locked;
                targetLayer.visible = true;
                targetLayer.locked = false;

                function removeConflictingItems(container) {
                    try {
                        // Check PathItems for anchor points to remove
                        for (var i = container.pathItems.length - 1; i >= 0; i--) {
                            try {
                                var path = container.pathItems[i];
                                for (var j = path.pathPoints.length - 1; j >= 0; j--) {
                                    try {
                                        var anchor = [path.pathPoints[j].anchor[0], path.pathPoints[j].anchor[1]];
                                        // Check if this anchor aligns with any ignored point
                                        for (var k = 0; k < ignoredAnchors.length; k++) {
                                            if (dist(anchor, ignoredAnchors[k]) <= IGNORED_DIST) {
                                                // Remove the entire path if it only has this point, or just the point
                                                if (path.pathPoints.length === 1) {
                                                    try { path.remove(); } catch (e) { }
                                                } else {
                                                    try { path.pathPoints[j].remove(); } catch (e) { }
                                                }
                                                break;
                                            }
                                        }
                                    } catch (e) {
                                        // Skip this anchor point
                                    }
                                }
                            } catch (e) {
                                // Skip this path
                            }
                        }

                        // Check GroupItems (placed art) for center point alignment
                        for (var m = container.groupItems.length - 1; m >= 0; m--) {
                            try {
                                var group = container.groupItems[m];
                                var centerX = group.left + (group.width / 2);
                                var centerY = group.top - (group.height / 2);
                                var centerPoint = [centerX, centerY];

                                // Check if center aligns with any ignored point
                                for (var n = 0; n < ignoredAnchors.length; n++) {
                                    if (dist(centerPoint, ignoredAnchors[n]) <= IGNORED_DIST) {
                                        try { group.remove(); } catch (e) { }
                                        break;
                                    }
                                }
                            } catch (e) {
                                // Skip this group
                            }
                        }

                        // Check SymbolItems (placed symbols) for center point alignment
                        if (container.symbolItems) {
                            for (var p = container.symbolItems.length - 1; p >= 0; p--) {
                                try {
                                    var symbol = container.symbolItems[p];
                                    var centerX = symbol.left + (symbol.width / 2);
                                    var centerY = symbol.top - (symbol.height / 2);
                                    var centerPoint = [centerX, centerY];

                                    // Check if center aligns with any ignored point
                                    for (var q = 0; q < ignoredAnchors.length; q++) {
                                        if (dist(centerPoint, ignoredAnchors[q]) <= IGNORED_DIST) {
                                            try { symbol.remove(); } catch (e) { }
                                            break;
                                        }
                                    }
                                } catch (e) {
                                    // Skip this symbol
                                }
                            }
                        }

                        // Check TextFrames for center point alignment
                        if (container.textFrames) {
                            for (var r = container.textFrames.length - 1; r >= 0; r--) {
                                try {
                                    var text = container.textFrames[r];
                                    var centerX = text.left + (text.width / 2);
                                    var centerY = text.top - (text.height / 2);
                                    var centerPoint = [centerX, centerY];

                                    // Check if center aligns with any ignored point
                                    for (var s = 0; s < ignoredAnchors.length; s++) {
                                        if (dist(centerPoint, ignoredAnchors[s]) <= IGNORED_DIST) {
                                            try { text.remove(); } catch (e) { }
                                            break;
                                        }
                                    }
                                } catch (e) {
                                    // Skip this text frame
                                }
                            }
                        }

                        // Recursively process nested groups
                        for (var t = 0; t < container.groupItems.length; t++) {
                            try {
                                removeConflictingItems(container.groupItems[t]);
                            } catch (e) {
                                // Skip this nested group
                            }
                        }

                        // Handle sublayers
                        if (container.layers) {
                            for (var u = 0; u < container.layers.length; u++) {
                                try {
                                    removeConflictingItems(container.layers[u]);
                                } catch (e) {
                                    // Skip this sublayer
                                }
                            }
                        }
                    } catch (e) {
                        // Skip this container entirely
                    }
                }

                removeConflictingItems(targetLayer);

                // Restore original layer state
                targetLayer.visible = originalVisible;
                targetLayer.locked = originalLocked;
            }
        }

        function getEndpoints(paths) {
            var endpoints = [];
            for (var i = 0; i < paths.length; i++) {
                var item = paths[i];
                var rotationOverride = getRotationOverride(item);

                // Handle CompoundPathItems - iterate through child pathItems
                if (item.typename === "CompoundPathItem" && item.pathItems) {
                    for (var cp = 0; cp < item.pathItems.length; cp++) {
                        var childPath = item.pathItems[cp];
                        if (!childPath.closed && childPath.pathPoints && childPath.pathPoints.length > 0) {
                            endpoints.push({ path: childPath, index: 0, pos: childPath.pathPoints[0].anchor.slice(), rotationOverride: rotationOverride, parentCompound: item });
                            if (childPath.pathPoints.length > 1) {
                                var lastIndex = childPath.pathPoints.length - 1;
                                endpoints.push({ path: childPath, index: lastIndex, pos: childPath.pathPoints[lastIndex].anchor.slice(), rotationOverride: rotationOverride, parentCompound: item });
                            }
                        }
                    }
                }
                // Handle regular PathItems
                else if (item.pathPoints && !item.closed && item.pathPoints.length > 0) {
                    endpoints.push({ path: item, index: 0, pos: item.pathPoints[0].anchor.slice(), rotationOverride: rotationOverride });
                    if (item.pathPoints.length > 1) {
                        var lastIndex = item.pathPoints.length - 1;
                        endpoints.push({ path: item, index: lastIndex, pos: item.pathPoints[lastIndex].anchor.slice(), rotationOverride: rotationOverride });
                    }
                }
            }
            return endpoints;
        }

        function snapThermostatEndpointsToJunctions() {
            var threshold = (typeof THERMOSTAT_JUNCTION_DIST === 'number' && THERMOSTAT_JUNCTION_DIST >= 0) ? THERMOSTAT_JUNCTION_DIST : 0;
            if (threshold <= 0) return;

            var managedLayers = [];
            function prepareLayer(name) {
                var layer = null;
                try { layer = doc.layers.getByName(name); } catch (e) { layer = null; }
                if (!layer) return null;
                var state = { layer: layer, locked: null, visible: null };
                try { state.locked = layer.locked; layer.locked = false; } catch (e) { }
                try { state.visible = layer.visible; layer.visible = true; } catch (e) { }
                managedLayers.push(state);
                return layer;
            }

            var thermostatLayer = prepareLayer("Thermostat Lines");
            // Both Normal and Emory mode use the same ductwork layers
            var greenLayerName = "Green Ductwork";
            var blueLayerName = "Blue Ductwork";
            var greenLayer = prepareLayer(greenLayerName);
            var blueLayer = prepareLayer(blueLayerName);

            try {
                if (!thermostatLayer || !greenLayer || !blueLayer) return;

                var thermostatPaths = getPathsOnLayerSelected("Thermostat Lines") || [];
                if (thermostatPaths.length === 0) return;

                var greenPaths = getPathsOnLayerSelected(greenLayerName) || [];
                var bluePaths = getPathsOnLayerSelected(blueLayerName) || [];
                if (greenPaths.length === 0 || bluePaths.length === 0) return;

                var thermostatEndpoints = getEndpoints(thermostatPaths) || [];
                if (thermostatEndpoints.length === 0) return;
                var greenEndpoints = getEndpoints(greenPaths) || [];
                var blueEndpoints = getEndpoints(bluePaths) || [];
                if (greenEndpoints.length === 0 || blueEndpoints.length === 0) return;

                var tol2 = threshold * threshold;

                function findClosestEndpoint(basePos, endpoints) {
                    var closest = null;
                    var bestDist2 = tol2;
                    for (var i = 0; i < endpoints.length; i++) {
                        var candidate = endpoints[i];
                        if (!candidate || !candidate.pos) continue;
                        try {
                            var d2 = dist2(basePos, candidate.pos);
                            if (d2 <= bestDist2) {
                                bestDist2 = d2;
                                closest = candidate;
                            }
                        } catch (e) {
                            continue;
                        }
                    }
                    return closest;
                }

                for (var ti = 0; ti < thermostatEndpoints.length; ti++) {
                    var t = thermostatEndpoints[ti];
                    if (!t || !t.path || !t.path.pathPoints || t.index >= t.path.pathPoints.length) continue;

                    var greenMatch = findClosestEndpoint(t.pos, greenEndpoints);
                    var blueMatch = findClosestEndpoint(t.pos, blueEndpoints);
                    if (!greenMatch || !blueMatch) continue;

                    try {
                        if (dist2(greenMatch.pos, blueMatch.pos) > tol2) continue;
                    } catch (e) {
                        continue;
                    }

                    var snapPos = [
                        (greenMatch.pos[0] + blueMatch.pos[0]) / 2,
                        (greenMatch.pos[1] + blueMatch.pos[1]) / 2
                    ];

                    try {
                        var point = t.path.pathPoints[t.index];
                        point.anchor = snapPos;
                        point.leftDirection = snapPos;
                        point.rightDirection = snapPos;
                    } catch (e) {
                        continue;
                    }
                }
            } catch (e) {
                // swallow thermostat snapping errors
            } finally {
                for (var idx = managedLayers.length - 1; idx >= 0; idx--) {
                    var state = managedLayers[idx];
                    try { if (state.locked !== null) state.layer.locked = state.locked; } catch (e) { }
                    try { if (state.visible !== null) state.layer.visible = state.visible; } catch (e) { }
                }
            }
        }

        function duplicateIsolatedEndpointsFiltered(sourceLayerName, destLayerName, ignoredAnchors, existingAnchors, rectangularRegisterAnchors) {
            // Precompute thermostat endpoints so registers can avoid thermostat locations
            var thermostatEndpoints = [];
            try { thermostatEndpoints = getEndpoints(getPathsOnLayerSelected("Thermostat Lines")) || []; } catch (e) { thermostatEndpoints = []; }

            function isNearThermostat(pt) {
                for (var ti = 0; ti < thermostatEndpoints.length; ti++) {
                    try { if (dist(pt, thermostatEndpoints[ti].pos) <= IGNORED_DIST) return true; } catch (e) { continue; }
                }
                return false;
            }

            var sourcePaths = getPathsOnLayerSelected(sourceLayerName);
            if (sourcePaths.length === 0) return;

            var destLayer = getOrCreateLayer(destLayerName);

            var pointMap = {};
            var epsilon = 0.1;
            for (var i = 0; i < sourcePaths.length; i++) {
                var path = sourcePaths[i];
                for (var j = 0; j < path.pathPoints.length; j++) {
                    var pt = path.pathPoints[j];
                    var x = Math.round(pt.anchor[0] / epsilon) * epsilon;
                    var y = Math.round(pt.anchor[1] / epsilon) * epsilon;
                    var key = x + "," + y;
                    if (!pointMap[key]) pointMap[key] = 0;
                    pointMap[key]++;
                }
            }

            var candidateEndpoints = getEndpoints(sourcePaths);

            for (var i = 0; i < candidateEndpoints.length; i++) {
                var currentEndpoint = candidateEndpoints[i];

                var x = Math.round(currentEndpoint.pos[0] / epsilon) * epsilon;
                var y = Math.round(currentEndpoint.pos[1] / epsilon) * epsilon;
                var key = x + "," + y;
                if (pointMap[key] > 1) continue;

                var isTIntersection = false;
                for (var j = 0; j < sourcePaths.length; j++) {
                    var otherPath = sourcePaths[j];
                    for (var k = 0; k < otherPath.pathPoints.length - 1; k++) {
                        if (otherPath === currentEndpoint.path && (k === currentEndpoint.index || k === currentEndpoint.index - 1)) {
                            continue;
                        }
                        var p1 = otherPath.pathPoints[k].anchor;
                        var p2 = otherPath.pathPoints[k + 1].anchor;
                        var res = closestPointOnSegment(p1, p2, currentEndpoint.pos);
                        if (res.t > 1e-6 && res.t < 1 - 1e-6 && dist(currentEndpoint.pos, res.pt) <= CONNECTION_DIST) {
                            isTIntersection = true;
                            break;
                        }
                    }
                    if (isTIntersection) break;

                    if (otherPath.closed && otherPath.pathPoints.length > 1) {
                        var p1 = otherPath.pathPoints[otherPath.pathPoints.length - 1].anchor;
                        var p2 = otherPath.pathPoints[0].anchor;
                        var res = closestPointOnSegment(p1, p2, currentEndpoint.pos);
                        if (res.t > 1e-6 && res.t < 1 - 1e-6 && dist(currentEndpoint.pos, res.pt) <= CONNECTION_DIST) {
                            isTIntersection = true;
                            break;
                        }
                    }
                }

                if (isTIntersection) continue;

                // *** NEW: Skip if point overlaps with ignored anchors ***
                if (isPointIgnored(currentEndpoint.pos, ignoredAnchors)) continue;

                // *** NEW: Skip if point already exists on target layers (includes Units check now) ***
                if (isPointAlreadyPlaced(currentEndpoint.pos, existingAnchors)) {
                    ensureAnchorTagged(destLayer, currentEndpoint.pos, currentEndpoint.rotationOverride);
                    continue;
                }

                // *** NEW: For Square Registers, also skip if point exists on Rectangular Registers ***
                if (destLayerName === "Square Registers" && rectangularRegisterAnchors && isPointAlreadyPlaced(currentEndpoint.pos, rectangularRegisterAnchors)) {
                    ensureAnchorTagged(destLayer, currentEndpoint.pos, currentEndpoint.rotationOverride);
                    continue;
                }

                // Conservative fix: avoid creating Square Registers where Thermostat endpoints are nearby
                if (destLayerName === "Square Registers" && isNearThermostat(currentEndpoint.pos)) {
                    try { DIAG.skippedRegisterNearThermostat++; DIAG.registerSkipReasons.push('Square Register skipped at ' + currentEndpoint.pos + ' (near thermostat)'); } catch (e) { }
                    continue;
                }

                // Calculate ductwork angle at endpoint for rotation (if not disabled)
                var endpointRotation = currentEndpoint.rotationOverride;
                if (!SKIP_REGISTER_ROTATION && destLayerName === "Square Registers") {
                    try {
                        var epPath = currentEndpoint.path;
                        var epIdx = currentEndpoint.index;
                        if (epPath && epPath.pathPoints && epPath.pathPoints.length >= 2) {
                            var pt1, pt2;
                            if (epIdx === 0) {
                                // First point - angle from point 0 to point 1
                                pt1 = epPath.pathPoints[0].anchor;
                                pt2 = epPath.pathPoints[1].anchor;
                            } else {
                                // Last point - angle from second-to-last to last
                                var lastIdx = epPath.pathPoints.length - 1;
                                pt1 = epPath.pathPoints[lastIdx - 1].anchor;
                                pt2 = epPath.pathPoints[lastIdx].anchor;
                            }
                            var dxEp = pt2[0] - pt1[0];
                            var dyEp = pt2[1] - pt1[1];
                            var ductAngleEp = Math.atan2(dyEp, dxEp) * (180 / Math.PI);
                            endpointRotation = normalizeAngle(ductAngleEp);
                            addDebug("[ENDPOINT-ROTATION] Calculated angle " + ductAngleEp.toFixed(1) + " deg at endpoint [" + currentEndpoint.pos[0].toFixed(1) + "," + currentEndpoint.pos[1].toFixed(1) + "]");
                        }
                    } catch (eAngle) {
                        addDebug("[ENDPOINT-ROTATION] Error calculating angle: " + eAngle);
                    }
                }

                createAnchorPoint(destLayer, currentEndpoint.pos, endpointRotation);
                try { DIAG.createdRegisters++; } catch (e) { }
            }
        }

        function averageCloseEndpoints(layer1Name, layer2Name, destLayerName, ignoredAnchors, existingAnchors) {
            var paths1 = getPathsOnLayerSelected(layer1Name);
            var paths2 = getPathsOnLayerSelected(layer2Name);
            if (paths1.length === 0 || paths2.length === 0) return;

            var destLayer = getOrCreateLayer(destLayerName);
            var endpoints1 = getEndpoints(paths1);
            var endpoints2 = getEndpoints(paths2);

            existingAnchors = existingAnchors || [];
            var isUnitLayer = (typeof destLayerName === "string" && destLayerName.toLowerCase() === "units");
            var placementTolerance = isUnitLayer ? UNIT_MERGE_DIST : null;

            var used1 = [];
            var used2 = [];
            for (var k = 0; k < endpoints1.length; k++) used1[k] = false;
            for (var k = 0; k < endpoints2.length; k++) used2[k] = false;

            for (var i = 0; i < endpoints1.length; i++) {
                if (used1[i]) continue;
                for (var j = 0; j < endpoints2.length; j++) {
                    if (used2[j]) continue;

                    if (dist(endpoints1[i].pos, endpoints2[j].pos) <= CLOSE_DIST) {
                        var avgPos = [
                            (endpoints1[i].pos[0] + endpoints2[j].pos[0]) / 2,
                            (endpoints1[i].pos[1] + endpoints2[j].pos[1]) / 2
                        ];

                        // *** NEW: Skip if averaged point overlaps with ignored anchors ***
                        if (isPointIgnored(avgPos, ignoredAnchors)) {
                            used1[i] = true;
                            used2[j] = true;
                            break;
                        }

                        // *** NEW: Skip if averaged point already exists on target layers ***
                        if (isPointAlreadyPlaced(avgPos, existingAnchors, placementTolerance)) {
                            ensureAnchorTagged(destLayer, avgPos, endpoints1[i].rotationOverride !== null ? endpoints1[i].rotationOverride : endpoints2[j].rotationOverride, placementTolerance);
                            used1[i] = true;
                            used2[j] = true;
                            break;
                        }

                        var avgRotation = endpoints1[i].rotationOverride !== null ? endpoints1[i].rotationOverride : endpoints2[j].rotationOverride;
                        createAnchorPoint(destLayer, avgPos, avgRotation);
                        existingAnchors.push(avgPos);
                        used1[i] = true;
                        used2[j] = true;
                        break;
                    }
                }
            }
        }

        function createDistantThermostats(thermostatLinesLayerName, unitsLayerName, thermostatsLayerName, ignoredAnchors, existingAnchors) {
            var thermostatPaths = getPathsOnLayerSelected(thermostatLinesLayerName);
            var unitPaths = getPathsOnLayerAll(unitsLayerName);
            if (!thermostatPaths || thermostatPaths.length === 0) return;

            var thermostatLayer = getOrCreateLayer(thermostatsLayerName);
            var thermostatEndpoints = getEndpoints(thermostatPaths) || [];
            var unitAnchors = [];
            for (var i = 0; i < unitPaths.length; i++) {
                for (var j = 0; j < unitPaths[i].pathPoints.length; j++) {
                    unitAnchors.push(unitPaths[i].pathPoints[j].anchor);
                }
            }

            // Also get Blue Ductwork endpoints to avoid creating thermostats where Units should be
            var blueDuctworkEndpoints = [];
            try {
                var blueDuctworkPaths = getPathsOnLayerAll("Blue Ductwork");
                var blueEndpoints = getEndpoints(blueDuctworkPaths) || [];
                for (var i = 0; i < blueEndpoints.length; i++) {
                    blueDuctworkEndpoints.push(blueEndpoints[i].pos);
                }
            } catch (e) {
                blueDuctworkEndpoints = [];
            }

            // If there are no units, create thermostats at all endpoints that are not ignored/already placed/near blue ductwork
            if (unitAnchors.length === 0) {
                for (var i = 0; i < thermostatEndpoints.length; i++) {
                    try {
                        var pos = thermostatEndpoints[i].pos;
                        if (isPointIgnored(pos, ignoredAnchors)) { DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + pos + ' (ignored)'); } catch (e) { }; continue; }
                        if (existingAnchors && isPointAlreadyPlaced(pos, existingAnchors)) {
                            ensureAnchorTagged(thermostatLayer, pos, thermostatEndpoints[i].rotationOverride);
                            DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + pos + ' (already placed)'); } catch (e) { }; continue;
                        }

                        // Check if close to Blue Ductwork endpoints (should be Unit instead)
                        var isCloseToBlue = false;
                        for (var k = 0; k < blueDuctworkEndpoints.length; k++) {
                            try { if (dist(pos, blueDuctworkEndpoints[k]) <= CLOSE_DIST) { isCloseToBlue = true; break; } } catch (e) { continue; }
                        }
                        if (isCloseToBlue) { DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + pos + ' (close to blue ductwork - should be unit)'); } catch (e) { }; continue; }

                        createAnchorPoint(thermostatLayer, pos, thermostatEndpoints[i].rotationOverride);
                        try { DIAG.thermostatsCreated++; } catch (e) { }
                    } catch (e) { continue; }
                }
                return;
            }

            // With units present, only create thermostats away from units, blue ductwork endpoints, and not ignored / existing
            for (var i = 0; i < thermostatEndpoints.length; i++) {
                try {
                    var tstatPoint = thermostatEndpoints[i].pos;
                    var isCloseToUnit = false;
                    for (var j = 0; j < unitAnchors.length; j++) {
                        try { if (dist(tstatPoint, unitAnchors[j]) <= CLOSE_DIST) { isCloseToUnit = true; break; } } catch (e) { continue; }
                    }
                    if (isCloseToUnit) { DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + tstatPoint + ' (close to unit)'); } catch (e) { }; continue; }

                    // Check if close to Blue Ductwork endpoints (should be Unit instead)
                    var isCloseToBlue = false;
                    for (var k = 0; k < blueDuctworkEndpoints.length; k++) {
                        try { if (dist(tstatPoint, blueDuctworkEndpoints[k]) <= CLOSE_DIST) { isCloseToBlue = true; break; } } catch (e) { continue; }
                    }
                    if (isCloseToBlue) { DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + tstatPoint + ' (close to blue ductwork - should be unit)'); } catch (e) { }; continue; }

                    if (isPointIgnored(tstatPoint, ignoredAnchors)) { DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + tstatPoint + ' (ignored)'); } catch (e) { }; continue; }
                    if (existingAnchors && isPointAlreadyPlaced(tstatPoint, existingAnchors)) {
                        ensureAnchorTagged(thermostatLayer, tstatPoint, thermostatEndpoints[i].rotationOverride);
                        DIAG.thermostatsSkipped++; try { DIAG.thermostatSkipReasons.push('Skipped thermostat at ' + tstatPoint + ' (already placed)'); } catch (e) { }; continue;
                    }

                    createAnchorPoint(thermostatLayer, tstatPoint, thermostatEndpoints[i].rotationOverride);
                    try { DIAG.thermostatsCreated++; } catch (e) { }
                } catch (e) { continue; }
            }
        }

        // --- EARLY STROKE NORMALIZATION: Set all paths to smallest stroke width in selection ---
        function normalizeStrokesToSmallest(paths) {
            if (!paths || paths.length === 0) return;

            // Find the smallest stroke width in the selection
            var smallestStroke = null;
            for (var i = 0; i < paths.length; i++) {
                try {
                    var path = paths[i];
                    if (!path || path.typename !== "PathItem") continue;
                    if (path.stroked && path.strokeWidth > 0) {
                        if (smallestStroke === null || path.strokeWidth < smallestStroke) {
                            smallestStroke = path.strokeWidth;
                        }
                    }
                } catch (e) {
                    continue;
                }
            }

            if (smallestStroke === null || smallestStroke <= 0) {
                addDebug("[EARLY-NORMALIZE] No valid stroke widths found in selection");
                return;
            }

            addDebug("[EARLY-NORMALIZE] Normalizing all paths to smallest stroke: " + smallestStroke.toFixed(2) + "pt");

            // Apply the smallest stroke width to all paths
            var normalizedCount = 0;
            for (var j = 0; j < paths.length; j++) {
                try {
                    var normPath = paths[j];
                    if (!normPath || normPath.typename !== "PathItem") continue;
                    if (normPath.stroked && normPath.strokeWidth !== smallestStroke) {
                        normPath.strokeWidth = smallestStroke;
                        normalizedCount++;
                    }
                } catch (e) {
                    continue;
                }
            }

            addDebug("[EARLY-NORMALIZE] Normalized " + normalizedCount + " paths to " + smallestStroke.toFixed(2) + "pt");
        }

        // --- NEW: STROKE NORMALIZATION FUNCTION (ENHANCED WITH ERROR HANDLING) ---
        function normalizeStrokeProperties(paths) {
            for (var i = 0; i < paths.length; i++) {
                var path = paths[i];
                try {
                    // Validate that the path is still valid
                    if (!path || path.typename !== "PathItem") continue;

                    // Clear any graphic style associations first
                    try {
                        path.unapplyAll();
                    } catch (e) {
                        // Ignore if unapplyAll fails
                    }

                    // Reset stroke properties
                    path.stroked = true;
                    path.strokeWidth = 1;
                    try {
                        path.strokeColor = doc.swatches[0].color; // Use registration color temporarily
                    } catch (e) {
                        // If can't set color, create a basic black color
                        var blackColor = new RGBColor();
                        blackColor.red = 0;
                        blackColor.green = 0;
                        blackColor.blue = 0;
                        path.strokeColor = blackColor;
                    }
                    path.filled = false;
                } catch (e) {
                    // Continue processing other paths if one fails
                    continue;
                }
            }
        }

        function shouldProcessAppearanceItem(item) {
            if (!item) return false;

            if (item.typename === 'PathItem') {
                return shouldProcessPath(item);
            }
            if (item.typename === 'CompoundPathItem') {
                // Check if this is a carve-out compound (tracked in global array)
                if (typeof CARVE_OUT_COMPOUNDS !== 'undefined' && CARVE_OUT_COMPOUNDS.length > 0) {
                    for (var coIdx = 0; coIdx < CARVE_OUT_COMPOUNDS.length; coIdx++) {
                        try {
                            if (CARVE_OUT_COMPOUNDS[coIdx] === item) {
                                addDebug("[COMPOUND-CHECK] Found in CARVE_OUT_COMPOUNDS - processing");
                                return true;
                            }
                        } catch (e) { }
                    }
                }

                // Check if the compound path is currently selected
                var isSelected = false;
                try {
                    isSelected = item.selected;
                    addDebug("[COMPOUND-CHECK] item.selected = " + isSelected);
                    if (isSelected) return true;
                } catch (e) {
                    addDebug("[COMPOUND-CHECK] Error checking selected: " + e);
                }

                // Check if the compound path itself was marked
                if (isPathCreated(item)) return true;

                // Check if this compound path is in our tracking list
                if (typeof COMPOUND_PATHS_TO_STYLE !== 'undefined') {
                    for (var cpIdx = 0; cpIdx < COMPOUND_PATHS_TO_STYLE.length; cpIdx++) {
                        if (COMPOUND_PATHS_TO_STYLE[cpIdx] === item) {
                            return true;
                        }
                    }
                }

                // Otherwise check its sub-paths
                try {
                    for (var i = 0; i < item.pathItems.length; i++) {
                        var subPath = item.pathItems[i];
                        if (shouldProcessPath(subPath)) return true;
                    }
                } catch (e) { }
                return false;
            }
            return false;
        }

        // --- ENHANCED ROBUST GRAPHIC STYLE APPLICATION ---
        function applyAllDuctworkStylesRobust(selectedItems) {
            // TEMP DEBUG - remove after testing
            $.writeln("[ROBUST-STYLES] FUNCTION CALLED");
            addDebug("[ROBUST-STYLES] === applyAllDuctworkStylesRobust CALLED ===");
            addDebug("[ROBUST-STYLES] selectedItems count: " + (selectedItems ? selectedItems.length : "null"));
            // Always use base graphic styles for ductwork lines; Emory rectangles keep their styles separately
            var styleEmoryAppend = "";
            var styleMappings = [
                { layerName: "Green Ductwork", styleName: "Green Ductwork" + styleEmoryAppend },
                { layerName: "Light Green Ductwork", styleName: "Light Green Ductwork" + styleEmoryAppend },
                { layerName: "Blue Ductwork", styleName: "Blue Ductwork" + styleEmoryAppend },
                { layerName: "Orange Ductwork", styleName: "Orange Ductwork" + styleEmoryAppend },
                { layerName: "Light Orange Ductwork", styleName: "Light Orange Ductwork" + styleEmoryAppend },
                { layerName: "Thermostat Lines", styleName: "Thermostat Lines" }
            ];

            // *** PRE-CHECK: Auto-import graphic styles if not found ***
            var stylesFound = 0;
            var stylesToCheck = ["Blue Ductwork", "Green Ductwork", "Orange Ductwork"];
            for (var checkIdx = 0; checkIdx < stylesToCheck.length; checkIdx++) {
                try {
                    var checkStyle = doc.graphicStyles.getByName(stylesToCheck[checkIdx]);
                    if (checkStyle) stylesFound++;
                } catch (eCheck) { }
            }

            if (stylesFound === 0) {
                addDebug("[ROBUST-STYLES] No graphic styles found - auto-importing from DuctworkLines.ai");
                try {
                    // Import styles from the template file
                    var sourceFile = new File("E:/Work/Work/Floorplans/Ductwork Assets/DuctworkLines.ai");
                    if (sourceFile.exists) {
                        var destDoc = doc;
                        var sourceDoc = app.open(sourceFile);
                        app.activeDocument = sourceDoc;

                        // Unlock all layers
                        for (var sL = 0; sL < sourceDoc.layers.length; sL++) {
                            try { sourceDoc.layers[sL].locked = false; } catch (eLock) { }
                            try { sourceDoc.layers[sL].visible = true; } catch (eVis) { }
                        }

                        // Select and copy all items
                        var items = sourceDoc.pageItems;
                        for (var sI = 0; sI < items.length; sI++) {
                            try { items[sI].selected = true; } catch (eSel) { }
                        }
                        app.copy();
                        sourceDoc.close(SaveOptions.DONOTSAVECHANGES);

                        // Paste into destination document
                        app.activeDocument = destDoc;
                        destDoc.selection = null;

                        // Create or get the template lines layer
                        var tempLayerName = "__MDUX_STYLE_TEMPLATE_LINES__";
                        var tempLayer = null;
                        try { tempLayer = destDoc.layers.getByName(tempLayerName); } catch (eFind) {
                            tempLayer = destDoc.layers.add();
                            tempLayer.name = tempLayerName;
                        }
                        try { tempLayer.locked = false; } catch (eLock) { }
                        destDoc.activeLayer = tempLayer;
                        app.executeMenuCommand("pasteInPlace");

                        // Move pasted items far away
                        var pasted = destDoc.selection;
                        if (pasted) {
                            if (pasted.length === undefined) pasted = [pasted];
                            var FAR_AWAY_X = -50000;
                            var FAR_AWAY_Y = -50000;
                            for (var pIdx = 0; pIdx < pasted.length; pIdx++) {
                                try {
                                    var pItem = pasted[pIdx];
                                    var pBounds = pItem.geometricBounds;
                                    pItem.translate(FAR_AWAY_X - pBounds[0], FAR_AWAY_Y - pBounds[1]);
                                } catch (eMove) { }
                            }
                        }
                        destDoc.selection = null;
                        try { tempLayer.locked = true; } catch (eLock) { }
                        try { tempLayer.visible = false; } catch (eVis) { }

                        addDebug("[ROBUST-STYLES] Auto-imported graphic styles successfully");
                    } else {
                        addDebug("[ROBUST-STYLES] WARNING: DuctworkLines.ai not found - cannot auto-import styles");
                    }
                } catch (eAutoImport) {
                    addDebug("[ROBUST-STYLES] ERROR auto-importing styles: " + eAutoImport);
                }
            }

            // Build a set of items that were originally selected or created from them
            // Comprehensive item collection function for layer
            function getAllStylableItems(container) {
                var items = [];

                function collectItems(cont) {
                    try {
                        // Force unlock and make visible temporarily for processing
                        var wasLocked = false, wasVisible = true;
                        if (cont.typename === "Layer" || cont.typename === "GroupItem") {
                            wasLocked = cont.locked;
                            wasVisible = cont.visible;
                            cont.locked = false;
                            cont.visible = true;
                        }

                        // Collect PathItems
                        if (cont.pathItems) {
                            for (var i = cont.pathItems.length - 1; i >= 0; i--) {
                                try {
                                    var path = cont.pathItems[i];
                                    if (path && !path.guides && !path.clipping) {
                                        items.push(path);
                                    }
                                } catch (e) {
                                    continue;
                                }
                            }
                        }

                        // Collect CompoundPathItems
                        if (cont.compoundPathItems) {
                            for (var j = cont.compoundPathItems.length - 1; j >= 0; j--) {
                                try {
                                    var compound = cont.compoundPathItems[j];
                                    if (compound) {
                                        items.push(compound);
                                    }
                                } catch (e) {
                                    continue;
                                }
                            }
                        }

                        // Recursively process GroupItems
                        if (cont.groupItems) {
                            for (var k = cont.groupItems.length - 1; k >= 0; k--) {
                                try {
                                    collectItems(cont.groupItems[k]);
                                } catch (e) {
                                    continue;
                                }
                            }
                        }

                        // Recursively process Sublayers
                        if (cont.layers) {
                            for (var l = cont.layers.length - 1; l >= 0; l--) {
                                try {
                                    collectItems(cont.layers[l]);
                                } catch (e) {
                                    continue;
                                }
                            }
                        }

                        // Restore original state
                        if (cont.typename === "Layer" || cont.typename === "GroupItem") {
                            cont.locked = wasLocked;
                            cont.visible = wasVisible;
                        }
                    } catch (e) {
                        // Continue with other containers
                    }
                }

                collectItems(container);
                return items;
            }

            // Main loop for applying styles
            addDebug("[ROBUST-STYLES] Starting layer loop, " + styleMappings.length + " mappings");
            for (var i = 0; i < styleMappings.length; i++) {
                var mapping = styleMappings[i];
                var layerName = mapping.layerName;
                var styleName = mapping.styleName;
                var targetLayer;
                var graphicStyle;

                try {
                    targetLayer = doc.layers.getByName(layerName);
                    addDebug("[ROBUST-STYLES] Found layer: " + layerName);
                } catch (e) {
                    addDebug("[ROBUST-STYLES] Layer not found: " + layerName);
                    continue;
                }
                graphicStyle = getGraphicStyleByNameFlexible(styleName);
                if (!graphicStyle) {
                    addDebug("[ROBUST-STYLES] Style not found: " + styleName);
                    continue;
                }
                addDebug("[ROBUST-STYLES] Found style: " + styleName);

                try {
                    // Clear selection first
                    doc.selection = null;

                    // Store original layer state
                    var originalLayerLocked = targetLayer.locked;
                    var originalLayerVisible = targetLayer.visible;
                    targetLayer.locked = false;
                    targetLayer.visible = true;

                    // Get ALL items on this layer
                    var allItems = getAllStylableItems(targetLayer);

                    // Process each item individually - BUT ONLY IF IT WAS IN ORIGINAL SELECTION
                    var processedCount = 0;
                    var itemsToScale = []; // Collect items with their scale factors

                    // *** PRE-NORMALIZE STROKES TO 3pt BEFORE STYLE APPLICATION ***
                    // This ensures consistent baseline before graphic styles are applied
                    var PRE_NORMALIZE_STROKE = 3; // Pre-normalization stroke width in points
                    var TARGET_SCALE_PERCENT = 65; // Target scale percentage after style application

                    addDebug("[STYLE-APPLY] Processing " + allItems.length + " items on " + layerName);
                    addDebug("[STYLE-APPLY] Pre-normalizing strokes to " + PRE_NORMALIZE_STROKE + "pt, then applying " + TARGET_SCALE_PERCENT + "% scale");

                    for (var j = 0; j < allItems.length; j++) {
                        try {
                            var item = allItems[j];

                            // Skip if this item wasn't part of the selected/created paths
                            var shouldProcess = shouldProcessAppearanceItem(item);
                            addDebug("[STYLE-APPLY] Item " + j + " (" + item.typename + "): shouldProcess=" + shouldProcess);
                            if (!shouldProcess) continue;

                            // *** PRE-NORMALIZE: Set stroke width to 3pt before style application ***
                            try {
                                if (item.typename === "PathItem") {
                                    if (item.stroked) {
                                        item.strokeWidth = PRE_NORMALIZE_STROKE;
                                    }
                                } else if (item.typename === "CompoundPathItem") {
                                    // For compound paths, normalize child paths
                                    try {
                                        for (var preNormIdx = 0; preNormIdx < item.pathItems.length; preNormIdx++) {
                                            var preNormChild = item.pathItems[preNormIdx];
                                            if (preNormChild.stroked) {
                                                preNormChild.strokeWidth = PRE_NORMALIZE_STROKE;
                                            }
                                        }
                                    } catch (ePreNormChild) { }
                                    // Also set on compound path itself if it has stroke
                                    try {
                                        if (item.stroked) {
                                            item.strokeWidth = PRE_NORMALIZE_STROKE;
                                        }
                                    } catch (ePreNormComp) { }
                                }
                                addDebug("[PRE-NORMALIZE] Set stroke to " + PRE_NORMALIZE_STROKE + "pt on item " + j);
                            } catch (ePreNorm) {
                                addDebug("[PRE-NORMALIZE] Error on item " + j + ": " + ePreNorm);
                            }

                            // Use fixed target scale (65%) instead of detecting existing stroke
                            // This ensures consistent results: pre-normalize to 3pt, apply style, then scale to 65%
                            var itemScaleFactor = TARGET_SCALE_PERCENT;
                            addDebug("[SCALE-FIXED] Using fixed scale " + itemScaleFactor + "% for item " + j + " (" + item.typename + ")");

                            // Clear existing appearance
                            try {
                                item.unapplyAll();
                            } catch (e) { }

                            // Apply the graphic style directly
                            graphicStyle.applyTo(item);

                            // Add to items that need scaling with their calculated scale factor
                            itemsToScale.push({ item: item, scale: itemScaleFactor });
                            processedCount++;

                        } catch (e) {
                            // Continue with next item
                            continue;
                        }
                    }

                    // After applying styles, apply the preserved scale factor to strokes
                    // Each item has its own scale factor calculated from bounds difference before style was applied
                    for (var m = 0; m < itemsToScale.length; m++) {
                        var itemData = itemsToScale[m];
                        var it = itemData.item;
                        var itemScale = itemData.scale;
                        if (it.locked || it.hidden) continue;
                        try {
                            var isDuctworkLayer = (layerName === "Green Ductwork" || layerName === "Blue Ductwork" ||
                                layerName === "Orange Ductwork" || layerName === "Light Orange Ductwork" ||
                                layerName === "Green Ductwork Emory" || layerName === "Light Green Ductwork Emory" ||
                                layerName === "Blue Ductwork Emory" || layerName === "Orange Ductwork Emory" ||
                                layerName === "Light Orange Ductwork Emory" || layerName === "Light Green Ductwork");
                            if (isDuctworkLayer) {
                                // Apply scale if not default
                                if (itemScale !== 100 && itemScale > 0) {
                                    it.resize(100, 100, false, false, false, false, itemScale, Transformation.CENTER);
                                    addDebug("[STYLE-SCALE] Applied preserved scale " + itemScale + "% to strokes");
                                }
                                // Store scale in metadata for exact preservation on reprocess
                                // Round only when very close to a whole number (floating point precision fix)
                                // Note: Don't store strokeWidth separately - calculate from scale to avoid sync issues
                                var storedScale = itemScale;
                                if (Math.abs(itemScale - Math.round(itemScale)) < 0.0001) {
                                    storedScale = Math.round(itemScale);
                                }
                                try {
                                    var meta = MDUX_getMetadata(it) || {};
                                    meta.MDUX_CurrentScale = storedScale;
                                    // Remove any stale strokeWidth to prevent confusion
                                    if (meta.strokeWidth !== undefined) {
                                        delete meta.strokeWidth;
                                    }
                                    // Ensure MDUX_Original* fields exist so panel-bridge slider doesn't reset scale
                                    // These are needed for panel-bridge to know the item has been initialized
                                    if (meta.MDUX_OriginalWidth === undefined) {
                                        meta.MDUX_OriginalWidth = it.width;
                                    }
                                    if (meta.MDUX_OriginalHeight === undefined) {
                                        meta.MDUX_OriginalHeight = it.height;
                                    }
                                    if (meta.MDUX_OriginalStrokeWidth === undefined) {
                                        meta.MDUX_OriginalStrokeWidth = 1; // Default base stroke
                                    }
                                    if (meta.MDUX_CumulativeRotation === undefined) {
                                        meta.MDUX_CumulativeRotation = "0";
                                    }
                                    MDUX_setMetadata(it, meta);
                                    addDebug("[STYLE-SCALE] Stored MDUX_CurrentScale=" + storedScale + "% in metadata (with Original* fields)");
                                } catch (eMeta) { }
                            } else if (layerName === "Thermostat Lines" && itemScale !== 100 && itemScale > 0) {
                                // For Thermostat Lines, scale the stroke width directly
                                if (it.stroked && it.strokeWidth) {
                                    it.strokeWidth = it.strokeWidth * (itemScale / 100);
                                }
                            }
                        } catch (e) { }
                    }

                    // Restore layer state
                    targetLayer.locked = originalLayerLocked;
                    targetLayer.visible = originalLayerVisible;

                    // Optional: Alert for debugging (comment out for production)
                    // alert("Applied '" + styleName + "' to " + processedCount + " items on '" + layerName + "'");

                } catch (e) {
                    // Continue with next layer
                    continue;
                }
            }

            // Clear selection when done
            doc.selection = null;
        }

        // Ensure required layers exist in the correct order and have specified colors
        function ensureDuctworkLayersExist() {
            var desired = [
                { name: "Frame", color: [254, 56, 56] },
                { name: "Ignored", color: [255, 153, 204] },
                { name: "Thermostats", color: [48, 254, 116] },
                { name: "Units", color: [100, 254, 254] },
                { name: "Secondary Exhaust Registers", color: [255, 79, 255] },
                { name: "Thermostat Lines", color: [77, 254, 254] },
                { name: "Exhaust Registers", color: [254, 215, 61] },
                { name: "Rectangular Registers", color: [0, 0, 0] },
                { name: "Circular Registers", color: [200, 200, 200] },
                { name: "Orange Register", color: [128, 128, 128] },
                { name: "Square Registers", color: [254, 124, 0] },
                { name: "Light Orange Ductwork", color: [153, 51, 0] },
                { name: "Orange Ductwork", color: [243, 189, 141] },
                { name: "Blue Ductwork", color: [0, 199, 209] },
                { name: "Light Green Ductwork", color: [102, 255, 153] },
                { name: "Green Ductwork", color: [0, 89, 31] }
                // NOTE: No separate Emory layers - both Normal and Emory modes use the same ductwork layers
                // The difference is in the graphic styles applied and the Double Ductwork (rectangles/connectors)
            ];

            var prevLayer = null;
            for (var i = 0; i < desired.length; i++) {
                var entry = desired[i];
                var layer = null;
                try {
                    layer = doc.layers.getByName(entry.name);
                } catch (e) {
                    try {
                        layer = doc.layers.add();
                        layer.name = entry.name;
                    } catch (e2) {
                        layer = null;
                    }
                }

                if (!layer) continue;

                // Set color (best-effort)
                try {
                    var col = new RGBColor();
                    col.red = entry.color[0];
                    col.green = entry.color[1];
                    col.blue = entry.color[2];
                    layer.color = col;
                } catch (e) {
                    // ignore if unable to set color
                }

                // Reorder layers to match desired sequence: place first at top, subsequent after prevLayer
                try {
                    if (prevLayer === null) {
                        // place at top
                        try {
                            layer.move(doc.layers[0], ElementPlacement.PLACEBEFORE);
                        } catch (e) {
                            // fallback: if that fails, try placeatbeginning if supported
                            try { layer.move(doc.layers[doc.layers.length - 1], ElementPlacement.PLACEAFTER); } catch (e2) { }
                        }
                    } else {
                        try {
                            layer.move(prevLayer, ElementPlacement.PLACEAFTER);
                        } catch (e) {
                            // ignore reorder errors
                        }
                    }
                } catch (e) {
                    // ignore
                }

                prevLayer = layer;
            }
        }

        // Ensure layers and colors before doing heavy work
        try { ensureDuctworkLayersExist(); } catch (e) { /* non-fatal */ }

        // *** CLEANUP: Remove template lines layer from style import ***
        // These are the lines moved far away during graphic style import
        try {
            var templateLayerName = "__MDUX_STYLE_TEMPLATE_LINES__";
            var templateLayer = doc.layers.getByName(templateLayerName);
            if (templateLayer) {
                try { templateLayer.locked = false; } catch (eLock) { }
                templateLayer.remove();
                addDebug("[CLEANUP] Removed style template lines layer");
            }
        } catch (eTemplateCleanup) {
            // Layer doesn't exist - that's fine
        }

        // Pre-step: align thermostat endpoints to duct junctions before processing selection
        try { snapThermostatEndpointsToJunctions(); } catch (e) { }

        // FR-002: Collect ignored anchors EARLY for ortho phase
        // This allows skip-final-ortho to respect ignore anchors at endpoints
        try {
            ORTHO_IGNORED_ANCHORS = getIgnoredAnchorPoints();
            if (ORTHO_IGNORED_ANCHORS.length > 0) {
                addDebug("[ORTHO-IGNORE] Collected " + ORTHO_IGNORED_ANCHORS.length + " ignored anchor(s) for ortho phase");
            }
        } catch (eOrthoIgnore) {
            ORTHO_IGNORED_ANCHORS = [];
        }

        // Collect ONLY placed register items EARLY for ortho phase
        // This allows skip-final-ortho to check if endpoint actually has a placed register component
        // NOTE: We use getExistingPlacedItemPositions (not getExistingAnchorPoints) to ONLY detect
        // actual placed components, NOT anchor paths. Anchor paths that will receive ignore markers
        // should NOT trigger skip-final-ortho.
        try {
            ORTHO_REGISTER_ANCHORS = getExistingPlacedItemPositions([
                "Square Registers", "Rectangular Registers", "Circular Registers", "Exhaust Registers"
            ]);
            if (ORTHO_REGISTER_ANCHORS.length > 0) {
                addDebug("[ORTHO-REGISTERS] Collected " + ORTHO_REGISTER_ANCHORS.length + " placed register component(s) for ortho phase");
            }
        } catch (eOrthoRegs) {
            ORTHO_REGISTER_ANCHORS = [];
        }

        // FR-003: Auto-snap near-intersection anchors to vertex points
        // If an anchor is within 0.5pt of a line intersection but NOT a proper vertex,
        // DELETE that anchor and ADD a vertex point on one of the intersecting lines
        try {
            var SNAP_TO_INTERSECTION_DIST = 0.5; // tolerance in points
            var anchorsDeleted = 0;
            var verticesAdded = 0;

            // Only process blue ductwork paths for intersection snapping
            var blueLayerName = resolveDuctworkLayerForProcessing("Blue Ductwork");
            var blueSnapPaths = [];
            for (var bspIdx = 0; bspIdx < allPaths.length; bspIdx++) {
                try {
                    var bspPath = allPaths[bspIdx];
                    if (bspPath && bspPath.layer && bspPath.layer.name === blueLayerName) {
                        blueSnapPaths.push(bspPath);
                    }
                } catch (eBsp) { }
            }

            if (blueSnapPaths.length > 1) {
                addDebug("[FR-003] Checking " + blueSnapPaths.length + " blue paths for near-intersection anchors");

                // Collect all intersections and nearby misaligned anchors
                var intersectionsToFix = [];

                // Find all segment intersections between different paths
                for (var snapPathA = 0; snapPathA < blueSnapPaths.length; snapPathA++) {
                    var pathA = blueSnapPaths[snapPathA];
                    var ptsA = null;
                    try { ptsA = pathA.pathPoints; } catch (e) { continue; }
                    if (!ptsA || ptsA.length < 2) continue;

                    for (var snapPathB = snapPathA + 1; snapPathB < blueSnapPaths.length; snapPathB++) {
                        var pathB = blueSnapPaths[snapPathB];
                        var ptsB = null;
                        try { ptsB = pathB.pathPoints; } catch (e) { continue; }
                        if (!ptsB || ptsB.length < 2) continue;

                        // Check each segment pair for intersection
                        for (var segA = 0; segA < ptsA.length - 1; segA++) {
                            var a1 = ptsA[segA].anchor;
                            var a2 = ptsA[segA + 1].anchor;

                            for (var segB = 0; segB < ptsB.length - 1; segB++) {
                                var b1 = ptsB[segB].anchor;
                                var b2 = ptsB[segB + 1].anchor;

                                // Calculate intersection point
                                var intPt = getSegmentIntersectionPoint(
                                    a1[0], a1[1], a2[0], a2[1],
                                    b1[0], b1[1], b2[0], b2[1]
                                );

                                if (intPt) {
                                    // Check if there's already a proper vertex at this intersection
                                    var hasProperVertex = false;

                                    // Check pathA for vertex at intersection
                                    for (var vCheckA = 0; vCheckA < ptsA.length; vCheckA++) {
                                        var vAncA = ptsA[vCheckA].anchor;
                                        var vDistA = Math.sqrt(Math.pow(vAncA[0] - intPt[0], 2) + Math.pow(vAncA[1] - intPt[1], 2));
                                        if (vDistA <= 0.01) {
                                            hasProperVertex = true;
                                            break;
                                        }
                                    }

                                    // Check pathB for vertex at intersection
                                    if (!hasProperVertex) {
                                        for (var vCheckB = 0; vCheckB < ptsB.length; vCheckB++) {
                                            var vAncB = ptsB[vCheckB].anchor;
                                            var vDistB = Math.sqrt(Math.pow(vAncB[0] - intPt[0], 2) + Math.pow(vAncB[1] - intPt[1], 2));
                                            if (vDistB <= 0.01) {
                                                hasProperVertex = true;
                                                break;
                                            }
                                        }
                                    }

                                    // Find misaligned anchors (within tolerance but not exactly at intersection)
                                    var misalignedAnchors = [];

                                    for (var ancA = 0; ancA < ptsA.length; ancA++) {
                                        var anchorA = ptsA[ancA].anchor;
                                        var distA = Math.sqrt(Math.pow(anchorA[0] - intPt[0], 2) + Math.pow(anchorA[1] - intPt[1], 2));
                                        if (distA > 0.01 && distA <= SNAP_TO_INTERSECTION_DIST) {
                                            misalignedAnchors.push({ path: pathA, anchorIdx: ancA, dist: distA });
                                        }
                                    }

                                    for (var ancB = 0; ancB < ptsB.length; ancB++) {
                                        var anchorB = ptsB[ancB].anchor;
                                        var distB = Math.sqrt(Math.pow(anchorB[0] - intPt[0], 2) + Math.pow(anchorB[1] - intPt[1], 2));
                                        if (distB > 0.01 && distB <= SNAP_TO_INTERSECTION_DIST) {
                                            misalignedAnchors.push({ path: pathB, anchorIdx: ancB, dist: distB });
                                        }
                                    }

                                    if (misalignedAnchors.length > 0) {
                                        intersectionsToFix.push({
                                            intPt: intPt,
                                            pathA: pathA,
                                            pathB: pathB,
                                            segA: segA,
                                            segB: segB,
                                            hasProperVertex: hasProperVertex,
                                            misalignedAnchors: misalignedAnchors
                                        });
                                    }
                                }
                            }
                        }
                    }
                }

                // Process intersections: delete misaligned anchors and add proper vertices
                for (var fixIdx = 0; fixIdx < intersectionsToFix.length; fixIdx++) {
                    var fix = intersectionsToFix[fixIdx];

                    // Delete misaligned anchors (process in reverse order to maintain indices)
                    fix.misalignedAnchors.sort(function(a, b) { return b.anchorIdx - a.anchorIdx; });

                    for (var delIdx = 0; delIdx < fix.misalignedAnchors.length; delIdx++) {
                        var toDelete = fix.misalignedAnchors[delIdx];
                        try {
                            var delPts = toDelete.path.pathPoints;
                            var oldAnchor = delPts[toDelete.anchorIdx].anchor;
                            addDebug("[FR-003] Deleting misaligned anchor [" + oldAnchor[0].toFixed(2) + "," + oldAnchor[1].toFixed(2) +
                                     "] (dist=" + toDelete.dist.toFixed(3) + " from intersection)");
                            delPts[toDelete.anchorIdx].remove();
                            anchorsDeleted++;
                        } catch (eDelete) {
                            addDebug("[FR-003] Error deleting anchor: " + eDelete);
                        }
                    }

                    // Add vertex on pathA at the intersection point (if no proper vertex exists)
                    if (!fix.hasProperVertex) {
                        try {
                            // Add new point to pathA at the intersection
                            // Insert between segA and segA+1
                            var targetPath = fix.pathA;
                            var targetPts = targetPath.pathPoints;
                            var insertAfterIdx = fix.segA;

                            // Add a new point at the end, then we'll set its position
                            var newPt = targetPts.add();
                            newPt.anchor = [fix.intPt[0], fix.intPt[1]];
                            newPt.leftDirection = [fix.intPt[0], fix.intPt[1]];
                            newPt.rightDirection = [fix.intPt[0], fix.intPt[1]];
                            newPt.pointType = PointType.CORNER;

                            // Now we need to move it to the correct position in the path
                            // The new point was added at the end, we need to reorder
                            // We'll collect all points, remove them, and re-add in correct order
                            var allAnchors = [];
                            for (var collectIdx = 0; collectIdx < targetPts.length; collectIdx++) {
                                allAnchors.push({
                                    anchor: [targetPts[collectIdx].anchor[0], targetPts[collectIdx].anchor[1]],
                                    leftDir: [targetPts[collectIdx].leftDirection[0], targetPts[collectIdx].leftDirection[1]],
                                    rightDir: [targetPts[collectIdx].rightDirection[0], targetPts[collectIdx].rightDirection[1]],
                                    pointType: targetPts[collectIdx].pointType
                                });
                            }

                            // The new point is at the end, move it to insertAfterIdx + 1
                            var newPointData = allAnchors.pop(); // Remove from end
                            allAnchors.splice(insertAfterIdx + 1, 0, newPointData); // Insert at correct position

                            // Clear all points and re-add in correct order
                            while (targetPts.length > 0) {
                                targetPts[targetPts.length - 1].remove();
                            }

                            for (var readdIdx = 0; readdIdx < allAnchors.length; readdIdx++) {
                                var ptData = allAnchors[readdIdx];
                                var addedPt = targetPts.add();
                                addedPt.anchor = ptData.anchor;
                                addedPt.leftDirection = ptData.leftDir;
                                addedPt.rightDirection = ptData.rightDir;
                                try { addedPt.pointType = ptData.pointType; } catch (ePtType) { }
                            }

                            addDebug("[FR-003] Added vertex at intersection [" + fix.intPt[0].toFixed(2) + "," + fix.intPt[1].toFixed(2) + "]");
                            verticesAdded++;
                        } catch (eAddVertex) {
                            addDebug("[FR-003] Error adding vertex: " + eAddVertex);
                        }
                    }
                }

                if (anchorsDeleted > 0 || verticesAdded > 0) {
                    addDebug("[FR-003] Deleted " + anchorsDeleted + " misaligned anchor(s), added " + verticesAdded + " vertex point(s)");
                }
            }
        } catch (eFR003) {
            addDebug("[FR-003] Error in intersection snap: " + eFR003);
        }

        // PRE-STEP: Release any existing compound paths in ductwork layers
        // This ensures we work with clean individual paths and avoids issues with
        // persistent sibling detection incorrectly matching paths from different compounds
        try {
            addDebug("\n=== RELEASING EXISTING COMPOUND PATHS ===");
            var ductworkLayerNamesForRelease = [
                "Blue Ductwork", "Green Ductwork", "Orange Ductwork", "Light Orange Ductwork"
            ];
            var compoundsReleased = 0;

            // Get selection bounds for filtering
            var releaseBounds = computeItemsBounds(originalSelectionItems);

            for (var relLayerIdx = 0; relLayerIdx < ductworkLayerNamesForRelease.length; relLayerIdx++) {
                var relLayerName = ductworkLayerNamesForRelease[relLayerIdx];
                var relLayer = null;
                try { relLayer = findLayerByNameDeep(relLayerName); } catch (e) { continue; }
                if (!relLayer) continue;

                // Release compounds in this layer that overlap with selection
                for (var relCpIdx = relLayer.compoundPathItems.length - 1; relCpIdx >= 0; relCpIdx--) {
                    try {
                        var relCompound = relLayer.compoundPathItems[relCpIdx];
                        if (!relCompound) continue;

                        var cpBounds = null;
                        try { cpBounds = relCompound.geometricBounds; } catch (e) { continue; }
                        if (!cpBounds || cpBounds.length < 4) continue;

                        // Only release if it overlaps with selection (or if no bounds, release all)
                        if (!releaseBounds || boundsOverlap(cpBounds, releaseBounds)) {
                            relCompound.selected = true;
                            app.executeMenuCommand("releasePath");
                            compoundsReleased++;
                        }
                    } catch (eRelCp) { }
                }
            }

            if (compoundsReleased > 0) {
                addDebug("[COMPOUND-RELEASE] Released " + compoundsReleased + " compound path(s) before processing");
                // Re-collect paths from ductwork layers after release since structure changed
                var newAllPaths = [];
                for (var relLayerIdx2 = 0; relLayerIdx2 < ductworkLayerNamesForRelease.length; relLayerIdx2++) {
                    var relLayerName2 = ductworkLayerNamesForRelease[relLayerIdx2];
                    var relLayer2 = null;
                    try { relLayer2 = findLayerByNameDeep(relLayerName2); } catch (e) { continue; }
                    if (!relLayer2) continue;

                    // Collect all open paths from this layer that are within selection bounds
                    for (var relPathIdx = 0; relPathIdx < relLayer2.pathItems.length; relPathIdx++) {
                        try {
                            var relPath = relLayer2.pathItems[relPathIdx];
                            if (relPath && !relPath.closed) {
                                var pathBounds = null;
                                try { pathBounds = relPath.geometricBounds; } catch (e) { continue; }
                                if (pathBounds && (!releaseBounds || boundsOverlap(pathBounds, releaseBounds))) {
                                    newAllPaths.push(relPath);
                                }
                            }
                        } catch (e) { }
                    }
                }
                allPaths = newAllPaths;
                addDebug("[COMPOUND-RELEASE] Re-collected " + allPaths.length + " path(s) after release");
            }
        } catch (ePreRelease) {
            addDebug("[COMPOUND-RELEASE] Error: " + ePreRelease);
        }

        // STEP 1: Process selected paths (snap, orthogonalize)
        updateProgress("Orthogonalizing paths...");
        var geometryPaths = allPaths.slice();
        BLUE_BRANCH_CONNECTIONS = [];
        for (var gpClear = 0; gpClear < geometryPaths.length; gpClear++) {
            clearBlueRightAngleBranch(geometryPaths[gpClear]);
        }
        ACTIVE_CENTERLINES = geometryPaths.slice();

        // STEP 1.5: When rotation override is specified, DELETE selected ductwork parts BEFORE orthogonalization
        // This ensures we delete components before paths move, then regenerate them at correct angles
        if (GLOBAL_ROTATION_OVERRIDE !== null) {
            addDebug("");
            addDebug("========================================");
            addDebug("[ROTATION OVERRIDE] Deleting selected ductwork parts");
            addDebug("========================================");

            var totalDeletedPlacedItems = 0;
            var totalDeletedAnchors = 0;

            // Get current selection (includes both ductwork paths AND any selected components)
            var currentSelection = doc.selection;
            if (currentSelection && currentSelection.length > 0) {
                addDebug("[ROTATION OVERRIDE] Processing " + currentSelection.length + " selected items");

                // Delete selected ductwork parts (PlacedItems and anchor PathItems)
                for (var si = currentSelection.length - 1; si >= 0; si--) {
                    try {
                        var item = currentSelection[si];
                        if (!item || !item.typename) continue;

                        // Delete selected PlacedItems (registers, units, etc.)
                        if (item.typename === 'PlacedItem') {
                            var layerName = item.layer ? item.layer.name : null;
                            // Only delete if on a ductwork parts layer
                            var validLayers = ["Units", "Square Registers", "Rectangular Registers", "Circular Registers",
                                "Exhaust Registers", "Orange Register", "Secondary Exhaust Registers", "Thermostats"];
                            var isOnValidLayer = false;
                            for (var vl = 0; vl < validLayers.length; vl++) {
                                if (layerName === validLayers[vl]) {
                                    isOnValidLayer = true;
                                    break;
                                }
                            }
                            if (isOnValidLayer) {
                                item.remove();
                                totalDeletedPlacedItems++;
                            }
                        }
                        // Delete selected anchor PathItems (1-point paths on ductwork parts layers)
                        else if (item.typename === 'PathItem' && item.pathPoints && item.pathPoints.length === 1) {
                            var layerName = item.layer ? item.layer.name : null;
                            var validLayers = ["Units", "Square Registers", "Rectangular Registers", "Circular Registers",
                                "Exhaust Registers", "Orange Register", "Secondary Exhaust Registers", "Thermostats"];
                            var isOnValidLayer = false;
                            for (var vl = 0; vl < validLayers.length; vl++) {
                                if (layerName === validLayers[vl]) {
                                    isOnValidLayer = true;
                                    break;
                                }
                            }
                            if (isOnValidLayer) {
                                item.remove();
                                totalDeletedAnchors++;
                            }
                        }
                    } catch (eItem) {
                        addDebug("[ROTATION OVERRIDE] Error deleting item: " + eItem);
                    }
                }
            }

            addDebug("========================================");
            addDebug("[ROTATION OVERRIDE] Total deleted: " + totalDeletedPlacedItems + " items, " + totalDeletedAnchors + " anchors");
            addDebug("[ROTATION OVERRIDE] Will recreate with " + GLOBAL_ROTATION_OVERRIDE + "° rotation after orthogonalization");
            addDebug("========================================");
            addDebug("");

            // Restore selection to just the ductwork line paths (geometryPaths)
            // AND rebuild geometryPaths to only contain valid path references
            try {
                doc.selection = null;
                var validPaths = [];
                for (var restoreIdx = 0; restoreIdx < geometryPaths.length; restoreIdx++) {
                    try {
                        var gPath = geometryPaths[restoreIdx];
                        if (gPath && gPath.typename === "PathItem") {
                            validPaths.push(gPath);
                        }
                    } catch (eCheckPath) {
                        // Path reference is invalid, skip it
                    }
                }

                // Update geometryPaths to only contain valid paths
                geometryPaths = validPaths;
                addDebug("[ROTATION OVERRIDE] Rebuilt geometryPaths to " + geometryPaths.length + " valid paths");

                if (validPaths.length > 0) {
                    doc.selection = validPaths;
                    addDebug("[ROTATION OVERRIDE] Restored selection to " + validPaths.length + " ductwork line paths");
                }
            } catch (eRestoreSel) {
                addDebug("[ROTATION OVERRIDE] Error restoring selection: " + eRestoreSel);
            }
        }

        var preOrthoConnections = collectEndpointConnections(geometryPaths, RECONNECT_CAPTURE_DIST);

        // PERFORMANCE: Skip per-path logging - just log summary
        addDebug("[Orthogonalize] Processing " + geometryPaths.length + " paths");

        // *** EARLY CROSSOVER DETECTION AND SPLITTING ***
        // Must happen BEFORE orthogonalization so split paths get orthogonalized together at same Y level
        var EARLY_CROSSOVER_SEGMENTS = []; // Store for later ignore anchor placement
        var EARLY_SPLIT_PAIRS = []; // Store split path pairs for compounding phase (to include branches)
        var SMALL_SEG_MIN = 5;
        var SMALL_SEG_MAX = 10;  // Changed from 17 to 10pt per user request
        var CROSSOVER_NORMALIZE_DIST = 4.25;  // Distance from intersection to each anchor (4.25pt x 2 = 8.5pt apart)

        // Find Blue Ductwork paths in geometryPaths
        var bluePaths = [];
        var bluePathIndices = [];
        for (var bpIdx = 0; bpIdx < geometryPaths.length; bpIdx++) {
            try {
                var bpPath = geometryPaths[bpIdx];
                if (bpPath && bpPath.layer && bpPath.layer.name.toLowerCase().indexOf("blue") !== -1) {
                    bluePaths.push(bpPath);
                    bluePathIndices.push(bpIdx);
                }
            } catch (e) { }
        }

        if (bluePaths.length > 1) {
            addDebug("[EARLY-XOVER] Checking " + bluePaths.length + " blue paths for crossovers");

            var crossoversToSplit = [];

            // === SPATIAL HASHING FOR EARLY CROSSOVER DETECTION O(n) PERFORMANCE ===
            var EX_CELL_SIZE = 100;
            var exSpatialGrid = {};
            var exPathToCells = [];

            // Build spatial index
            for (var exBuildIdx = 0; exBuildIdx < bluePaths.length; exBuildIdx++) {
                var exBuildPath = bluePaths[exBuildIdx];
                var exBuildPts = exBuildPath ? exBuildPath.pathPoints : null;
                if (!exBuildPts || exBuildPts.length === 0) {
                    exPathToCells.push([]);
                    continue;
                }
                var exMinX = Infinity, exMinY = Infinity, exMaxX = -Infinity, exMaxY = -Infinity;
                for (var exBp = 0; exBp < exBuildPts.length; exBp++) {
                    var exAnchor = exBuildPts[exBp].anchor;
                    if (exAnchor[0] < exMinX) exMinX = exAnchor[0];
                    if (exAnchor[0] > exMaxX) exMaxX = exAnchor[0];
                    if (exAnchor[1] < exMinY) exMinY = exAnchor[1];
                    if (exAnchor[1] > exMaxY) exMaxY = exAnchor[1];
                }
                var exCellMinX = Math.floor(exMinX / EX_CELL_SIZE);
                var exCellMaxX = Math.floor(exMaxX / EX_CELL_SIZE);
                var exCellMinY = Math.floor(exMinY / EX_CELL_SIZE);
                var exCellMaxY = Math.floor(exMaxY / EX_CELL_SIZE);
                var exCells = [];
                for (var exCx = exCellMinX; exCx <= exCellMaxX; exCx++) {
                    for (var exCy = exCellMinY; exCy <= exCellMaxY; exCy++) {
                        var exCellKey = exCx + "," + exCy;
                        exCells.push(exCellKey);
                        if (!exSpatialGrid[exCellKey]) exSpatialGrid[exCellKey] = [];
                        exSpatialGrid[exCellKey].push(exBuildIdx);
                    }
                }
                exPathToCells.push(exCells);
            }

            for (var xpIdx = 0; xpIdx < bluePaths.length; xpIdx++) {
                var xPath = bluePaths[xpIdx];
                var xPts = xPath.pathPoints;

                // Get candidate paths from shared cells
                var exCandidates = {};
                var exMyCells = exPathToCells[xpIdx];
                for (var exMc = 0; exMc < exMyCells.length; exMc++) {
                    var exCellPaths = exSpatialGrid[exMyCells[exMc]];
                    if (exCellPaths) {
                        for (var exCp = 0; exCp < exCellPaths.length; exCp++) {
                            var exCandIdx = exCellPaths[exCp];
                            if (exCandIdx !== xpIdx) {
                                exCandidates[exCandIdx] = true;
                            }
                        }
                    }
                }

                // Check each internal segment (not first or last segment)
                for (var xSegIdx = 1; xSegIdx < xPts.length - 2; xSegIdx++) {
                    var xSegStart = xPts[xSegIdx].anchor;
                    var xSegEnd = xPts[xSegIdx + 1].anchor;
                    var xDx = xSegEnd[0] - xSegStart[0];
                    var xDy = xSegEnd[1] - xSegStart[1];
                    var xSegLen = Math.sqrt(xDx * xDx + xDy * xDy);

                    if (xSegLen >= SMALL_SEG_MIN && xSegLen <= SMALL_SEG_MAX) {
                        // Found a small internal segment - check only candidate paths
                        for (var xOtherIdx in exCandidates) {
                            if (!exCandidates.hasOwnProperty(xOtherIdx)) continue;
                            xOtherIdx = parseInt(xOtherIdx);
                            var xOtherPath = bluePaths[xOtherIdx];
                            var xOtherPts = xOtherPath.pathPoints;
                            var xFoundIntersection = false;

                            for (var xOtherSegIdx = 0; xOtherSegIdx < xOtherPts.length - 1 && !xFoundIntersection; xOtherSegIdx++) {
                                var xoSegStart = xOtherPts[xOtherSegIdx].anchor;
                                var xoSegEnd = xOtherPts[xOtherSegIdx + 1].anchor;

                                if (segmentsIntersect(
                                    xSegStart[0], xSegStart[1], xSegEnd[0], xSegEnd[1],
                                    xoSegStart[0], xoSegStart[1], xoSegEnd[0], xoSegEnd[1]
                                )) {
                                    xFoundIntersection = true;
                                    addDebug("[EARLY-XOVER] FOUND: Path " + xpIdx + " seg " + xSegIdx + " (" + xSegLen.toFixed(1) + "pt) crossed by path " + xOtherIdx + " seg " + xOtherSegIdx);
                                    crossoversToSplit.push({
                                        path: xPath,
                                        pathIdx: xpIdx,
                                        geometryIdx: bluePathIndices[xpIdx],
                                        segmentIdx: xSegIdx,
                                        segStart: [xSegStart[0], xSegStart[1]],
                                        segEnd: [xSegEnd[0], xSegEnd[1]],
                                        // Store crossing path info for intersection calculation after ortho
                                        crossingPath: xOtherPath,
                                        crossingSegIdx: xOtherSegIdx
                                    });
                                }
                            }
                        }
                    }
                }
            }

            addDebug("[EARLY-XOVER] Found " + crossoversToSplit.length + " crossover(s) - will calculate intersection points after ortho");

            // Store crossover info for post-ortho splitting with intersection calculation
            for (var forceIdx = 0; forceIdx < crossoversToSplit.length; forceIdx++) {
                EARLY_CROSSOVER_SEGMENTS.push(crossoversToSplit[forceIdx]);
            }
            addDebug("[EARLY-XOVER] Stored " + EARLY_CROSSOVER_SEGMENTS.length + " crossover(s) for post-ortho splitting");
        }

        // *** CLEANUP: REMOVE INTERNAL ANCHORS WITHIN 4PT OF ENDPOINTS ***
        // If an internal anchor on a blue ductwork path is within 4pt of an endpoint,
        // remove the internal anchor and mark that endpoint to be ignored (no part placed there)
        // Also mark the OPPOSITE endpoint for unit placement
        // NOTE: We store PATH REFERENCES, not coordinates, because orthogonalization will move endpoints
        addDebug("\n=== INTERNAL ANCHOR CLEANUP (near endpoints) ===");
        var ENDPOINT_INTERNAL_THRESHOLD = 4;
        var endpointPathRefs = []; // Store {path, endpoint: "start"|"end"} - will read coords AFTER ortho
        var oppositeEndpointPathRefs = []; // Store opposite endpoint refs for unit placement
        var endpointsToIgnore = []; // Will be populated AFTER ortho with actual coordinates

        addDebug("[CLEANUP] Checking " + bluePaths.length + " blue path(s) for internal anchors within " + ENDPOINT_INTERNAL_THRESHOLD + "pt of endpoints");

        for (var cleanIdx = 0; cleanIdx < bluePaths.length; cleanIdx++) {
            try {
                var cleanPath = bluePaths[cleanIdx];
                var cleanPts = cleanPath.pathPoints;
                addDebug("[CLEANUP] Path " + cleanIdx + ": " + cleanPts.length + " points");
                if (cleanPts.length < 3) {
                    addDebug("[CLEANUP] Path " + cleanIdx + ": Skipping - less than 3 points (no internal anchors)");
                    continue; // Need at least 3 points to have internal anchors
                }

                var startPt = cleanPts[0].anchor;
                var endPt = cleanPts[cleanPts.length - 1].anchor;

                // Check each internal anchor (not first or last)
                // Work backwards to avoid index shifting issues when removing
                for (var intIdx = cleanPts.length - 2; intIdx >= 1; intIdx--) {
                    var intAnchor = cleanPts[intIdx].anchor;
                    var distToStart = Math.sqrt(
                        Math.pow(intAnchor[0] - startPt[0], 2) +
                        Math.pow(intAnchor[1] - startPt[1], 2)
                    );
                    var distToEnd = Math.sqrt(
                        Math.pow(intAnchor[0] - endPt[0], 2) +
                        Math.pow(intAnchor[1] - endPt[1], 2)
                    );
                    addDebug("[CLEANUP] Path " + cleanIdx + " internal #" + intIdx + " at [" + intAnchor[0].toFixed(1) + "," + intAnchor[1].toFixed(1) + "]: distToStart=" + distToStart.toFixed(1) + "pt, distToEnd=" + distToEnd.toFixed(1) + "pt");

                    if (distToStart <= ENDPOINT_INTERNAL_THRESHOLD) {
                        addDebug("[CLEANUP] Path " + cleanIdx + ": Internal anchor at [" + intAnchor[0].toFixed(1) + "," + intAnchor[1].toFixed(1) + "] is " + distToStart.toFixed(1) + "pt from start - PRESERVING anchor, marking start endpoint to ignore");
                        // NOTE: We no longer remove internal anchors - they survive for reruns
                        // Store PATH REFERENCE instead of coordinates (will read after ortho)
                        endpointPathRefs.push({ path: cleanPath, endpoint: "start" });
                        // Add the OPPOSITE endpoint (end) for unit placement
                        oppositeEndpointPathRefs.push({ path: cleanPath, endpoint: "end" });
                        // Store for orthogonalization - ignore marker is at intIdx, endpoint is at index 0
                        ORTHO_IGNORE_MARKER_PATHS.push({ path: cleanPath, endpoint: "start", ignoreMarkerIndex: intIdx });
                        addDebug("[CLEANUP] Path " + cleanIdx + ": Will read endpoint coordinates AFTER orthogonalization");
                    } else if (distToEnd <= ENDPOINT_INTERNAL_THRESHOLD) {
                        addDebug("[CLEANUP] Path " + cleanIdx + ": Internal anchor at [" + intAnchor[0].toFixed(1) + "," + intAnchor[1].toFixed(1) + "] is " + distToEnd.toFixed(1) + "pt from end - PRESERVING anchor, marking end endpoint to ignore");
                        // NOTE: We no longer remove internal anchors - they survive for reruns
                        // Store PATH REFERENCE instead of coordinates (will read after ortho)
                        endpointPathRefs.push({ path: cleanPath, endpoint: "end" });
                        // Add the OPPOSITE endpoint (start) for unit placement
                        oppositeEndpointPathRefs.push({ path: cleanPath, endpoint: "start" });
                        // Store for orthogonalization - ignore marker is at intIdx, endpoint is at last index
                        ORTHO_IGNORE_MARKER_PATHS.push({ path: cleanPath, endpoint: "end", ignoreMarkerIndex: intIdx });
                        addDebug("[CLEANUP] Path " + cleanIdx + ": Will read endpoint coordinates AFTER orthogonalization");
                    }
                }
            } catch (eClean) {
                addDebug("[CLEANUP] Error processing path " + cleanIdx + ": " + eClean);
            }
        }

        // NOTE: DO NOT persist ignore anchors here - paths will be orthogonalized first
        // The actual coordinates will be read and persisted AFTER orthogonalization
        if (endpointPathRefs.length > 0) {
            addDebug("[CLEANUP] Deferred " + endpointPathRefs.length + " endpoint(s) for ignore - will persist AFTER orthogonalization");
        }
        if (oppositeEndpointPathRefs.length > 0) {
            addDebug("[CLEANUP] Deferred " + oppositeEndpointPathRefs.length + " opposite endpoint(s) for unit placement");
        }

        // =====================================================================
        // PYTHON-ACCELERATED ORTHOGONALIZATION
        // =====================================================================
        // Try Python for 30-50x faster orthogonalization
        // Falls back to ExtendScript if Python unavailable
        // =====================================================================

        var pythonOrthoSuccess = false;

        if ($.global.MDUX_USE_PYTHON && typeof PythonBridge !== 'undefined' && PythonBridge.isAvailable()) {
            addDebug("[PYTHON-ORTHO] Attempting Python-accelerated orthogonalization for " + geometryPaths.length + " paths");
            var orthoStartTime = new Date().getTime();

            // Build locked points list from ORTHO_IGNORE_MARKER_PATHS
            // These are endpoints with ignore markers that should NOT be snapped
            var lockedPointsForPython = [];
            for (var lpIdx = 0; lpIdx < ORTHO_IGNORE_MARKER_PATHS.length; lpIdx++) {
                var imInfo = ORTHO_IGNORE_MARKER_PATHS[lpIdx];
                // Find the path index in geometryPaths
                for (var gpIdx = 0; gpIdx < geometryPaths.length; gpIdx++) {
                    if (geometryPaths[gpIdx] === imInfo.path) {
                        var pointIdx = (imInfo.endpoint === "start") ? 0 : geometryPaths[gpIdx].pathPoints.length - 1;
                        lockedPointsForPython.push({ path_idx: gpIdx, point_idx: pointIdx });
                        addDebug("[PYTHON-ORTHO] Locking point: path " + gpIdx + ", point " + pointIdx + " (ignore marker endpoint)");
                        break;
                    }
                }
            }

            try {
                updateProgress("Orthogonalizing paths (Python)...");
                var pyOrthoResult = PythonBridge.orthogonalize(geometryPaths, SNAP_THRESHOLD, lockedPointsForPython);

                if (pyOrthoResult && pyOrthoResult.paths && !pyOrthoResult.error) {
                    // Apply Python results back to Illustrator paths
                    var appliedCount = 0;
                    for (var pyIdx = 0; pyIdx < pyOrthoResult.paths.length; pyIdx++) {
                        var pyPath = pyOrthoResult.paths[pyIdx];
                        var targetPath = geometryPaths[pyPath.id];

                        if (!targetPath || !targetPath.pathPoints) continue;

                        try {
                            var pts = targetPath.pathPoints;
                            var pyPts = pyPath.points;

                            // Only apply if point counts match
                            if (pts.length === pyPts.length) {
                                for (var ptIdx = 0; ptIdx < pts.length; ptIdx++) {
                                    var newX = pyPts[ptIdx].x;
                                    var newY = pyPts[ptIdx].y;
                                    var oldAnchor = pts[ptIdx].anchor;

                                    // Only update if position changed
                                    if (Math.abs(oldAnchor[0] - newX) > 0.01 || Math.abs(oldAnchor[1] - newY) > 0.01) {
                                        // Move anchor and handles together
                                        var dx = newX - oldAnchor[0];
                                        var dy = newY - oldAnchor[1];
                                        pts[ptIdx].anchor = [newX, newY];
                                        pts[ptIdx].leftDirection = [pts[ptIdx].leftDirection[0] + dx, pts[ptIdx].leftDirection[1] + dy];
                                        pts[ptIdx].rightDirection = [pts[ptIdx].rightDirection[0] + dx, pts[ptIdx].rightDirection[1] + dy];
                                    }
                                }
                                appliedCount++;
                            }
                        } catch (eApply) {
                            // Path may have become invalid
                        }
                    }

                    var orthoElapsed = new Date().getTime() - orthoStartTime;
                    addDebug("[PYTHON-ORTHO] SUCCESS: Applied " + appliedCount + " paths in " + orthoElapsed + "ms");
                    addDebug("[PYTHON-ORTHO] Python stats: " + pyOrthoResult.iterations + " iterations, " +
                             pyOrthoResult.total_snaps + " snaps, " + pyOrthoResult.total_ortho_changes + " ortho changes");
                    pythonOrthoSuccess = true;
                } else {
                    addDebug("[PYTHON-ORTHO] No valid result, falling back to ExtendScript");
                    if (pyOrthoResult && pyOrthoResult.error) {
                        addDebug("[PYTHON-ORTHO] Error: " + pyOrthoResult.error);
                    }
                }
            } catch (ePyOrtho) {
                addDebug("[PYTHON-ORTHO] Exception: " + ePyOrtho + ", falling back to ExtendScript");
            }
        }

        // Fall back to ExtendScript orthogonalization if Python didn't work
        if (!pythonOrthoSuccess) {
            // Check if fallback is disabled (for debugging)
            if (!$.global.MDUX_PYTHON_FALLBACK) {
                addDebug("[ORTHO] ERROR: Python orthogonalization failed and fallback is disabled");
                addDebug("[ORTHO] ABORTING - check debug log for [PYBRIDGE] errors");
                // Force flush the debug log NOW before showing alert
                try { flushDebugLog(); } catch (e) { }
                alert("Python acceleration failed - ABORTING.\n\nDebug log written. Check for [PYBRIDGE] messages.\n\nCommon issues:\n- Python not in system PATH\n- geometry_engine.py not found");
                // Close progress window and abort
                try { if (progressWin) progressWin.close(); } catch (e) { }
                return;  // EXIT IMMEDIATELY
            } else {
                addDebug("[ORTHO] Using ExtendScript orthogonalization (this may take several minutes)...");
                updateProgress("Orthogonalizing paths (ExtendScript)...");

                // BATCH PROCESSING: Process paths in chunks to prevent UI lockup
                var ORTHO_BATCH_SIZE = 25; // Process 25 paths at a time
                var iteration = 0;
                var changed = true;
                while (changed && iteration < MAX_ITER) {
                    iteration++;
                    changed = false;
                    addDebug("[Orthogonalize Iteration " + iteration + "] Starting with " + geometryPaths.length + " paths");
                    var allSegments = buildSegmentsForPaths(geometryPaths);
                    if (snapAnchors(geometryPaths, allSegments)) changed = true;

                    // Process in batches
                    for (var i = 0; i < geometryPaths.length; i++) {
                        if (orthogonalizePath(geometryPaths[i], preOrthoConnections.pairs)) changed = true;

                        // Update progress between batches (lightweight - no app.redraw)
                        if (i > 0 && i % ORTHO_BATCH_SIZE === 0) {
                            if (progressWin && progressLabel) {
                                try {
                                    progressLabel.text = "Orthogonalizing... " + i + "/" + geometryPaths.length;
                                    progressWin.update();
                                } catch (e) { }
                            }
                        }
                    }
                    if (restoreEndpointConnections(preOrthoConnections)) changed = true;
                }
            }
        }

        restoreEndpointConnections(collectEndpointConnections(geometryPaths, CONNECTION_DIST));

        // *** POST-ORTHO: READ ACTUAL ENDPOINT COORDINATES FROM PATH REFERENCES ***
        // Now that paths are orthogonalized, read the CURRENT endpoint positions
        if (endpointPathRefs.length > 0) {
            addDebug("\n=== POST-ORTHO: READING ENDPOINT COORDINATES ===");
            for (var epRefIdx = 0; epRefIdx < endpointPathRefs.length; epRefIdx++) {
                var epRef = endpointPathRefs[epRefIdx];
                try {
                    var epPath = epRef.path;
                    var epPts = epPath.pathPoints;
                    if (epPts && epPts.length > 0) {
                        var actualCoord;
                        if (epRef.endpoint === "start") {
                            actualCoord = [epPts[0].anchor[0], epPts[0].anchor[1]];
                        } else {
                            actualCoord = [epPts[epPts.length - 1].anchor[0], epPts[epPts.length - 1].anchor[1]];
                        }
                        endpointsToIgnore.push(actualCoord);
                        addDebug("[POST-ORTHO-IGNORE] Read " + epRef.endpoint + " endpoint: [" + actualCoord[0].toFixed(1) + "," + actualCoord[1].toFixed(1) + "]");
                    } else {
                        addDebug("[POST-ORTHO-IGNORE] WARNING: Path has no points, skipping");
                    }
                } catch (eReadEp) {
                    addDebug("[POST-ORTHO-IGNORE] ERROR reading endpoint: " + eReadEp);
                }
            }
            addDebug("[POST-ORTHO-IGNORE] Collected " + endpointsToIgnore.length + " endpoint(s) to ignore (using POST-ortho coordinates)");
        }

        // Also read opposite endpoints for unit placement
        var oppositeEndpointsForUnits = [];
        if (oppositeEndpointPathRefs.length > 0) {
            addDebug("[POST-ORTHO-UNITS] Reading opposite endpoint coordinates for unit placement");
            for (var oppRefIdx = 0; oppRefIdx < oppositeEndpointPathRefs.length; oppRefIdx++) {
                var oppRef = oppositeEndpointPathRefs[oppRefIdx];
                try {
                    var oppPath = oppRef.path;
                    var oppPts = oppPath.pathPoints;
                    if (oppPts && oppPts.length > 0) {
                        var oppCoord;
                        if (oppRef.endpoint === "start") {
                            oppCoord = [oppPts[0].anchor[0], oppPts[0].anchor[1]];
                        } else {
                            oppCoord = [oppPts[oppPts.length - 1].anchor[0], oppPts[oppPts.length - 1].anchor[1]];
                        }
                        oppositeEndpointsForUnits.push(oppCoord);
                        addDebug("[POST-ORTHO-UNITS] Read " + oppRef.endpoint + " endpoint for unit: [" + oppCoord[0].toFixed(1) + "," + oppCoord[1].toFixed(1) + "]");
                    }
                } catch (eReadOpp) {
                    addDebug("[POST-ORTHO-UNITS] ERROR reading opposite endpoint: " + eReadOpp);
                }
            }
            addDebug("[POST-ORTHO-UNITS] Collected " + oppositeEndpointsForUnits.length + " opposite endpoint(s) for unit placement");
        }

        // *** POST-ORTHO CROSSOVER SPLITTING ***
        // Now that paths are orthogonalized, split at crossover points with intersection alignment
        if (EARLY_CROSSOVER_SEGMENTS.length > 0) {
            addDebug("[POST-ORTHO-SPLIT] Splitting " + EARLY_CROSSOVER_SEGMENTS.length + " crossover path(s)");

            // *** FORCE-ORTHO CROSSOVER PATHS - REMOVED ***
            // This code was incorrectly flattening entire paths with multiple elbows
            // into single lines by aligning ALL points to the same coordinate.
            // The regular segment-by-segment orthogonalization already handles
            // crossover paths correctly - this extra step was destroying valid geometry.
            // Bug fixed: 2026-01-05
            addDebug("[POST-ORTHO-SPLIT] Skipping destructive FORCE-ORTHO (normal ortho already applied)");

            // Helper: Calculate intersection point of two line segments
            function getLineIntersection(x1, y1, x2, y2, x3, y3, x4, y4) {
                var denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
                if (Math.abs(denom) < 0.0001) return null; // Parallel lines

                var t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
                return [x1 + t * (x2 - x1), y1 + t * (y2 - y1)];
            }

            for (var postSplitIdx = EARLY_CROSSOVER_SEGMENTS.length - 1; postSplitIdx >= 0; postSplitIdx--) {
                var xoInfo = EARLY_CROSSOVER_SEGMENTS[postSplitIdx];
                var targetPath = xoInfo.path;
                var segIdx = xoInfo.segmentIdx;
                var crossingPath = xoInfo.crossingPath;
                var crossingSegIdx = xoInfo.crossingSegIdx;

                try {
                    var pts = targetPath.pathPoints;
                    var numPoints = pts.length;

                    if (segIdx > 0 && segIdx < numPoints - 2 && numPoints >= 4) {
                        addDebug("[POST-ORTHO-SPLIT] Splitting path at segment " + segIdx + " (has " + numPoints + " points)");

                        // *** CAPTURE STROKE WIDTH BEFORE SPLITTING ***
                        // This will be stored on the compound path for proper style application
                        var preCompoundScale = 100; // Default
                        try {
                            if (targetPath.stroked && targetPath.strokeWidth > 0) {
                                var baseStrokeWidth = 4; // Base stroke width for 100% scale
                                preCompoundScale = Math.round((targetPath.strokeWidth / baseStrokeWidth) * 100);
                                addDebug("[POST-ORTHO-SPLIT] Captured stroke: " + targetPath.strokeWidth.toFixed(2) + "pt = " + preCompoundScale + "% scale");
                            }
                        } catch (eStroke) {
                            addDebug("[POST-ORTHO-SPLIT] Could not read stroke width: " + eStroke);
                        }

                        // Get the crossover segment endpoints (AFTER ortho)
                        var segStartPt = pts[segIdx].anchor;
                        var segEndPt = pts[segIdx + 1].anchor;

                        // Calculate segment length and direction
                        var segDx = segEndPt[0] - segStartPt[0];
                        var segDy = segEndPt[1] - segStartPt[1];
                        var segLen = Math.sqrt(segDx * segDx + segDy * segDy);
                        // NORMALIZE: Use fixed 8.5pt distance from intersection instead of half segment length
                        var halfLen = CROSSOVER_NORMALIZE_DIST;

                        addDebug("[POST-ORTHO-SPLIT] Crossover segment: [" + segStartPt[0].toFixed(1) + "," + segStartPt[1].toFixed(1) + "] to [" + segEndPt[0].toFixed(1) + "," + segEndPt[1].toFixed(1) + "] len=" + segLen.toFixed(1) + " (normalizing to " + (halfLen * 2) + "pt)");

                        // *** FORCE ORTHOGONALIZATION OF ENTIRE PATH ***
                        // If the crossover segment was skipped during normal ortho (steep angle), force it now.
                        // IMPORTANT: Must orthogonalize ALL segments from crossover to end of path
                        // before splitting, otherwise the split halves will have misaligned endpoints.
                        var tolerance = 0.5; // Allow small tolerance for already-orthogonal segments
                        if (Math.abs(segDx) > tolerance && Math.abs(segDy) > tolerance) {
                            addDebug("[POST-ORTHO-SPLIT] Crossover segment is non-orthogonal (dx=" + segDx.toFixed(2) + ", dy=" + segDy.toFixed(2) + ") - forcing orthogonalization of entire path");

                            // Determine orientation based on dominant direction of crossover segment
                            var isHorizontal = Math.abs(segDx) > Math.abs(segDy);
                            var alignmentCoord = segStartPt[isHorizontal ? 1 : 0]; // Y for horizontal, X for vertical

                            addDebug("[POST-ORTHO-SPLIT] Forcing " + (isHorizontal ? "HORIZONTAL" : "VERTICAL") + " alignment at " + (isHorizontal ? "Y" : "X") + "=" + alignmentCoord.toFixed(2));

                            // Orthogonalize ALL points from crossover segment to end of path
                            for (var i = segIdx + 1; i < pts.length; i++) {
                                var oldAnchor = pts[i].anchor;
                                if (isHorizontal) {
                                    // Horizontal: set all Y coordinates to alignment Y
                                    pts[i].anchor = [oldAnchor[0], alignmentCoord];
                                    pts[i].leftDirection = [oldAnchor[0], alignmentCoord];
                                    pts[i].rightDirection = [oldAnchor[0], alignmentCoord];
                                    addDebug("[POST-ORTHO-SPLIT]   Point " + i + ": [" + oldAnchor[0].toFixed(1) + "," + oldAnchor[1].toFixed(1) + "] -> [" + oldAnchor[0].toFixed(1) + "," + alignmentCoord.toFixed(1) + "]");
                                } else {
                                    // Vertical: set all X coordinates to alignment X
                                    pts[i].anchor = [alignmentCoord, oldAnchor[1]];
                                    pts[i].leftDirection = [alignmentCoord, oldAnchor[1]];
                                    pts[i].rightDirection = [alignmentCoord, oldAnchor[1]];
                                    addDebug("[POST-ORTHO-SPLIT]   Point " + i + ": [" + oldAnchor[0].toFixed(1) + "," + oldAnchor[1].toFixed(1) + "] -> [" + alignmentCoord.toFixed(1) + "," + oldAnchor[1].toFixed(1) + "]");
                                }
                            }

                            // Update segment endpoint to the newly orthogonalized position
                            segEndPt = pts[segIdx + 1].anchor;

                            // Recalculate segment direction and length after orthogonalization
                            segDx = segEndPt[0] - segStartPt[0];
                            segDy = segEndPt[1] - segStartPt[1];
                            segLen = Math.sqrt(segDx * segDx + segDy * segDy);
                            // Keep using normalized 8.5pt distance (halfLen already set above)
                            addDebug("[POST-ORTHO-SPLIT] After forcing ortho: crossover seg dx=" + segDx.toFixed(2) + ", dy=" + segDy.toFixed(2) + ", len=" + segLen.toFixed(2));
                        }

                        // Calculate intersection point with crossing path (AFTER ortho)
                        var intersectionPt = null;
                        if (crossingPath && crossingPath.pathPoints && crossingPath.pathPoints.length > crossingSegIdx + 1) {
                            var crossPts = crossingPath.pathPoints;
                            var crossStart = crossPts[crossingSegIdx].anchor;
                            var crossEnd = crossPts[crossingSegIdx + 1].anchor;
                            addDebug("[POST-ORTHO-SPLIT] Crossing segment: [" + crossStart[0].toFixed(1) + "," + crossStart[1].toFixed(1) + "] to [" + crossEnd[0].toFixed(1) + "," + crossEnd[1].toFixed(1) + "]");

                            intersectionPt = getLineIntersection(
                                segStartPt[0], segStartPt[1], segEndPt[0], segEndPt[1],
                                crossStart[0], crossStart[1], crossEnd[0], crossEnd[1]
                            );
                            if (intersectionPt) {
                                addDebug("[POST-ORTHO-SPLIT] Intersection point: [" + intersectionPt[0].toFixed(2) + "," + intersectionPt[1].toFixed(2) + "]");
                            }
                        }

                        // CENTER the crossover segment around the intersection point
                        // This ensures both endpoints are equidistant from the crossing line
                        if (intersectionPt && segLen > 0.1) {
                            // Normalize segment direction
                            var normDx = segDx / segLen;
                            var normDy = segDy / segLen;

                            // Calculate new positions for B and C, centered on intersection
                            var newSegStart = [
                                intersectionPt[0] - normDx * halfLen,
                                intersectionPt[1] - normDy * halfLen
                            ];
                            var newSegEnd = [
                                intersectionPt[0] + normDx * halfLen,
                                intersectionPt[1] + normDy * halfLen
                            ];

                            addDebug("[POST-ORTHO-SPLIT] Centering segment on intersection:");
                            addDebug("[POST-ORTHO-SPLIT]   Old B: [" + segStartPt[0].toFixed(2) + "," + segStartPt[1].toFixed(2) + "] -> New B: [" + newSegStart[0].toFixed(2) + "," + newSegStart[1].toFixed(2) + "]");
                            addDebug("[POST-ORTHO-SPLIT]   Old C: [" + segEndPt[0].toFixed(2) + "," + segEndPt[1].toFixed(2) + "] -> New C: [" + newSegEnd[0].toFixed(2) + "," + newSegEnd[1].toFixed(2) + "]");

                            // Update the path points BEFORE splitting
                            // Also reset handles to make them corner points (no bezier curves)
                            pts[segIdx].anchor = [newSegStart[0], newSegStart[1]];
                            pts[segIdx].leftDirection = [newSegStart[0], newSegStart[1]];
                            pts[segIdx].rightDirection = [newSegStart[0], newSegStart[1]];
                            pts[segIdx + 1].anchor = [newSegEnd[0], newSegEnd[1]];
                            pts[segIdx + 1].leftDirection = [newSegEnd[0], newSegEnd[1]];
                            pts[segIdx + 1].rightDirection = [newSegEnd[0], newSegEnd[1]];

                            // Update our local copies
                            segStartPt = newSegStart;
                            segEndPt = newSegEnd;
                        }

                        // Store segment endpoints for ignore anchors (at the centered positions)
                        xoInfo.segStart = [segStartPt[0], segStartPt[1]];
                        xoInfo.segEnd = [segEndPt[0], segEndPt[1]];

                        // Create duplicate for second half (points from segIdx+1 to end)
                        var dupPath = targetPath.duplicate();
                        var dupPts = dupPath.pathPoints;

                        // Remove points 0 through segIdx from duplicate (keep segIdx+1 onwards)
                        for (var rmDup = 0; rmDup <= segIdx; rmDup++) {
                            dupPts[0].remove();
                        }

                        // Remove points segIdx+1 through end from original (keep 0 through segIdx)
                        var origPts = targetPath.pathPoints;
                        while (origPts.length > segIdx + 1) {
                            origPts[origPts.length - 1].remove();
                        }

                        // Ensure both paths have butt end caps (not round)
                        try { targetPath.strokeCap = StrokeCap.BUTTENDCAP; } catch (eCap1) { }
                        try { dupPath.strokeCap = StrokeCap.BUTTENDCAP; } catch (eCap2) { }

                        // Ensure split endpoints are corner points (no bezier handles)
                        try {
                            var lastPtIdx = targetPath.pathPoints.length - 1;
                            var lastPt = targetPath.pathPoints[lastPtIdx];
                            lastPt.leftDirection = lastPt.anchor;
                            lastPt.rightDirection = lastPt.anchor;
                        } catch (eHandle1) { }
                        try {
                            var firstPt = dupPath.pathPoints[0];
                            firstPt.leftDirection = firstPt.anchor;
                            firstPt.rightDirection = firstPt.anchor;
                        } catch (eHandle2) { }

                        addDebug("[POST-ORTHO-SPLIT] Original now " + targetPath.pathPoints.length + " pts ending at [" + targetPath.pathPoints[targetPath.pathPoints.length - 1].anchor[0].toFixed(1) + "," + targetPath.pathPoints[targetPath.pathPoints.length - 1].anchor[1].toFixed(1) + "]");
                        addDebug("[POST-ORTHO-SPLIT] New split has " + dupPath.pathPoints.length + " pts starting at [" + dupPath.pathPoints[0].anchor[0].toFixed(1) + "," + dupPath.pathPoints[0].anchor[1].toFixed(1) + "]");

                        // *** LOG ALL POINT POSITIONS AFTER SPLIT FOR DEBUGGING ***
                        addDebug("[POST-ORTHO-SPLIT] === LEFT HALF POINTS ===");
                        for (var lpIdx = 0; lpIdx < targetPath.pathPoints.length; lpIdx++) {
                            var lpt = targetPath.pathPoints[lpIdx].anchor;
                            addDebug("[POST-ORTHO-SPLIT]   Point " + lpIdx + ": [" + lpt[0].toFixed(2) + ", " + lpt[1].toFixed(2) + "]");
                        }
                        addDebug("[POST-ORTHO-SPLIT] === RIGHT HALF POINTS ===");
                        for (var rpIdx = 0; rpIdx < dupPath.pathPoints.length; rpIdx++) {
                            var rpt = dupPath.pathPoints[rpIdx].anchor;
                            addDebug("[POST-ORTHO-SPLIT]   Point " + rpIdx + ": [" + rpt[0].toFixed(2) + ", " + rpt[1].toFixed(2) + "]");
                        }

                        // *** DEFER COMPOUNDING TO LATER PHASE ***
                        // Don't compound here - store split pairs for COMPOUNDING CONNECTED PATHS phase
                        // This allows branches that connect to split halves to be included in the same compound
                        addDebug("[POST-ORTHO-SPLIT] Deferring compounding to later phase (to include branches)...");

                        // Store stroke width on individual paths for later use
                        try {
                            var splitMeta = { MDUX_PreCompoundScale: preCompoundScale, MDUX_EarlySplitPath: true };
                            MDUX_setMetadata(targetPath, splitMeta);
                            MDUX_setMetadata(dupPath, splitMeta);
                            addDebug("[POST-ORTHO-SPLIT] Stored preCompoundScale=" + preCompoundScale + " on both split paths");
                        } catch (eStoreMeta) {
                            addDebug("[POST-ORTHO-SPLIT] Failed to store metadata: " + eStoreMeta);
                        }

                        // Add dupPath to tracking arrays (targetPath is already in geometryPaths)
                        geometryPaths.push(dupPath);
                        SELECTED_PATHS.push(dupPath);
                        addDebug("[POST-ORTHO-SPLIT] Added split path to geometryPaths (now " + geometryPaths.length + " paths)");

                        // Store split pair for later forced connection
                        EARLY_SPLIT_PAIRS.push({
                            pathA: targetPath,
                            pathB: dupPath,
                            preCompoundScale: preCompoundScale
                        });
                        addDebug("[POST-ORTHO-SPLIT] Stored split pair for later compounding");

                        doc.selection = null;
                    } else {
                        addDebug("[POST-ORTHO-SPLIT] Skipping path - invalid segIdx or not enough points");
                    }
                } catch (eSplit) {
                    addDebug("[POST-ORTHO-SPLIT] ERROR: " + eSplit);
                }
            }
            addDebug("[POST-ORTHO-SPLIT] Done splitting crossover paths");
        }

        // STEP 2: Remove any art on target layers that conflicts with the 'Ignore' layer
        updateProgress("Cleaning ignored areas...");
        doc.selection = null;
        var ignoredAnchors = getIgnoredAnchorPoints();

        // Add endpoints that were identified during cleanup (internal anchors within 4pt of endpoints)
        // IMPORTANT: Also create physical PathItems on Ignored layer so they persist for re-runs
        if (endpointsToIgnore && endpointsToIgnore.length > 0) {
            addDebug("\n=== PERSISTING ENDPOINT IGNORE ANCHORS ===");
            addDebug("[CLEANUP-PERSIST] Need to persist " + endpointsToIgnore.length + " endpoint(s) to Ignored layer");

            // Get the Ignored layer and unlock it if needed
            var cleanupIgnLayer = null;
            try {
                cleanupIgnLayer = doc.layers.getByName("Ignored");
            } catch (eNoLayer) {
                addDebug("[CLEANUP-PERSIST] No 'Ignored' layer found, creating one");
                try {
                    cleanupIgnLayer = doc.layers.add();
                    cleanupIgnLayer.name = "Ignored";
                } catch (eCreate) {
                    addDebug("[CLEANUP-PERSIST] Could not create Ignored layer: " + eCreate);
                }
            }

            if (cleanupIgnLayer) {
                // Unlock the layer AND make it visible if needed
                try {
                    if (cleanupIgnLayer.locked) {
                        cleanupIgnLayer.locked = false;
                        addDebug("[CLEANUP-PERSIST] Unlocked Ignored layer");
                    }
                    if (!cleanupIgnLayer.visible) {
                        cleanupIgnLayer.visible = true;
                        addDebug("[CLEANUP-PERSIST] Made Ignored layer visible");
                    }
                    // Also check parent containers - unlock them too if locked
                    var parentContainer = cleanupIgnLayer.parent;
                    while (parentContainer) {
                        var pType = parentContainer.typename;
                        if (pType === "Layer" || pType === "GroupItem") {
                            try {
                                if (parentContainer.locked) {
                                    parentContainer.locked = false;
                                    addDebug("[CLEANUP-PERSIST] Unlocked parent " + pType + ": " + (parentContainer.name || "(unnamed)"));
                                }
                            } catch (ePLock) { }
                            try {
                                if (!parentContainer.visible) {
                                    parentContainer.visible = true;
                                    addDebug("[CLEANUP-PERSIST] Made parent " + pType + " visible: " + (parentContainer.name || "(unnamed)"));
                                }
                            } catch (ePVis) { }
                        }
                        if (pType !== "Layer" && pType !== "GroupItem") break;
                        parentContainer = parentContainer.parent;
                    }
                } catch (eUnlock) {
                    addDebug("[CLEANUP-PERSIST] Could not unlock/show layer: " + eUnlock);
                }

                // Now create physical PathItems for each endpoint
                for (var epIdx = 0; epIdx < endpointsToIgnore.length; epIdx++) {
                    var epPt = endpointsToIgnore[epIdx];
                    ignoredAnchors.push(epPt);

                    try {
                        var epIgnAnchor = cleanupIgnLayer.pathItems.add();
                        epIgnAnchor.setEntirePath([[epPt[0], epPt[1]]]);
                        epIgnAnchor.filled = false;
                        epIgnAnchor.stroked = false;
                        addDebug("[CLEANUP-PERSIST] Created ignore anchor at [" + epPt[0].toFixed(1) + "," + epPt[1].toFixed(1) + "]");
                    } catch (eAddAnchor) {
                        addDebug("[CLEANUP-PERSIST] ERROR creating anchor at [" + epPt[0].toFixed(1) + "," + epPt[1].toFixed(1) + "]: " + eAddAnchor);
                    }
                }
                addDebug("[CLEANUP-PERSIST] Persisted " + endpointsToIgnore.length + " endpoint(s) to Ignored layer");
            } else {
                // Fallback: just add to in-memory array if we couldn't get the layer
                for (var epIdx = 0; epIdx < endpointsToIgnore.length; epIdx++) {
                    ignoredAnchors.push(endpointsToIgnore[epIdx]);
                }
                addDebug("[CLEANUP] Added " + endpointsToIgnore.length + " endpoint(s) to ignored anchors (in-memory only - layer unavailable)");
            }
            addDebug("[CLEANUP-PERSIST] Total ignored anchors: " + ignoredAnchors.length);
        }

        if (ignoredAnchors.length > 0) {
            removeConflictingArtAndAnchors(ignoredAnchors);
        }

        // Consolidate any pre-existing Unit anchors before new placement
        try { mergeAnchorsOnLayer("Units", UNIT_MERGE_DIST); } catch (e) { }

        // STEP 3: Create Units FIRST from close endpoints between ductwork layers
        updateProgress("Creating units...");
        // *** Get existing anchor points from all target layers before Units creation ***
        var existingAnchorsForUnits = getExistingAnchorPoints(["Units", "Square Registers", "Exhaust Registers", "Thermostats"]);

        // *** Also check against Square Registers and Rectangular Registers for Units creation ***
        var registerAnchorsForUnits = getExistingAnchorPoints(["Square Registers", "Rectangular Registers"]);
        var allExistingForUnits = existingAnchorsForUnits.concat(registerAnchorsForUnits);

        // Use same ductwork layers for both normal and Emory mode
        var greenSourceLayer = resolveDuctworkLayerForProcessing("Green Ductwork");
        var lightGreenSourceLayer = resolveDuctworkLayerForProcessing("Light Green Ductwork");
        var blueSourceLayer = resolveDuctworkLayerForProcessing("Blue Ductwork");
        var orangeSourceLayer = resolveDuctworkLayerForProcessing("Orange Ductwork");
        var lightOrangeSourceLayer = resolveDuctworkLayerForProcessing("Light Orange Ductwork");

        // All 5 ductwork colors with their source layer names for iteration
        var ALL_DUCTWORK_SOURCES = [
            { layer: greenSourceLayer, name: "Green Ductwork" },
            { layer: lightGreenSourceLayer, name: "Light Green Ductwork" },
            { layer: blueSourceLayer, name: "Blue Ductwork" },
            { layer: orangeSourceLayer, name: "Orange Ductwork" },
            { layer: lightOrangeSourceLayer, name: "Light Orange Ductwork" }
        ];

        averageCloseEndpoints(greenSourceLayer, blueSourceLayer, "Units", ignoredAnchors, allExistingForUnits);
        averageCloseEndpoints(lightGreenSourceLayer, blueSourceLayer, "Units", ignoredAnchors, allExistingForUnits);
        averageCloseEndpoints(orangeSourceLayer, lightOrangeSourceLayer, "Units", ignoredAnchors, allExistingForUnits);
        averageCloseEndpoints("Thermostat Lines", blueSourceLayer, "Units", ignoredAnchors, allExistingForUnits);

        // *** CREATE UNITS AT OPPOSITE ENDPOINTS (from internal anchor cleanup) ***
        // When an internal anchor is removed near an endpoint, create a unit at the opposite end of that path
        if (oppositeEndpointsForUnits && oppositeEndpointsForUnits.length > 0) {
            addDebug("\n=== OPPOSITE ENDPOINT UNITS (from internal anchor cleanup) ===");
            var unitsLayer = getOrCreateLayer("Units");
            var unitsCreated = 0;

            // Refresh existing anchors to avoid duplicates
            var existingForOppositeUnits = getExistingAnchorPoints(["Units", "Square Registers", "Exhaust Registers", "Thermostats", "Rectangular Registers"]);

            for (var opIdx = 0; opIdx < oppositeEndpointsForUnits.length; opIdx++) {
                var opPos = oppositeEndpointsForUnits[opIdx];

                // Skip if this point is ignored
                if (isPointIgnored(opPos, ignoredAnchors)) {
                    addDebug("[OPPOSITE-UNITS] Skipping [" + opPos[0].toFixed(1) + "," + opPos[1].toFixed(1) + "] - in ignored anchors");
                    continue;
                }

                // Skip if already has a unit/register nearby
                if (isPointAlreadyPlaced(opPos, existingForOppositeUnits)) {
                    addDebug("[OPPOSITE-UNITS] Skipping [" + opPos[0].toFixed(1) + "," + opPos[1].toFixed(1) + "] - already placed");
                    continue;
                }

                // Create unit anchor at this position
                addDebug("[OPPOSITE-UNITS] Creating unit at [" + opPos[0].toFixed(1) + "," + opPos[1].toFixed(1) + "]");
                createAnchorPoint(unitsLayer, opPos, null);
                unitsCreated++;

                // Add to existing list to prevent duplicates
                existingForOppositeUnits.push(opPos);
            }

            addDebug("[OPPOSITE-UNITS] Created " + unitsCreated + " unit(s) at opposite endpoints");
        }

        // === MARKER-TRIGGERED INTERSECTION VERTICES ===
        // When a single-point "marker" exists near an intersection of two ductwork paths,
        // and neither path has a vertex at that intersection, insert a vertex into one of the
        // paths at the exact intersection point. This converts a "crossover" into a connection.
        addDebug("\n=== PROCESSING MARKER-TRIGGERED INTERSECTIONS ===");
        updateProgress("Processing marker intersections...");

        var MARKER_INTERSECTION_TOLERANCE = 25; // Distance from marker to intersection to trigger
        var VERTEX_INSERT_TOLERANCE = 2; // Distance to consider "already has vertex"

        // Helper: Insert a vertex into a path at a given segment index
        // Returns true if successful, false otherwise
        function insertVertexIntoPath(targetPath, segmentIdx, vertexPos) {
            try {
                var pts = targetPath.pathPoints;
                if (!pts || pts.length < 2) return false;
                if (segmentIdx < 0 || segmentIdx >= pts.length - 1) return false;

                // Collect all current anchor positions
                var newAnchors = [];
                for (var i = 0; i <= segmentIdx; i++) {
                    newAnchors.push([pts[i].anchor[0], pts[i].anchor[1]]);
                }
                // Insert the new vertex
                newAnchors.push([vertexPos[0], vertexPos[1]]);
                // Add remaining points
                for (var j = segmentIdx + 1; j < pts.length; j++) {
                    newAnchors.push([pts[j].anchor[0], pts[j].anchor[1]]);
                }

                // Rebuild the path with the new point
                targetPath.setEntirePath(newAnchors);
                return true;
            } catch (e) {
                addDebug("[MARKER-INTERSECT] ERROR inserting vertex: " + e);
                return false;
            }
        }

        // Collect single-point "marker" paths from selected paths on ductwork layers
        var soloMarkers = [];
        for (var mi = 0; mi < allPaths.length; mi++) {
            try {
                var mPath = allPaths[mi];
                if (!mPath || !mPath.pathPoints || mPath.pathPoints.length !== 1) continue;

                // Check if on a ductwork layer
                var mLayerName = mPath.layer ? mPath.layer.name : null;
                if (!mLayerName) continue;
                var lowerLayerName = mLayerName.toLowerCase();
                if (lowerLayerName.indexOf("ductwork") === -1) continue; // Skip non-ductwork layers

                var markerAnchor = mPath.pathPoints[0].anchor;
                soloMarkers.push({
                    path: mPath,
                    position: [markerAnchor[0], markerAnchor[1]],
                    layerName: mLayerName
                });
            } catch (e) { }
        }

        addDebug("[MARKER-INTERSECT] Found " + soloMarkers.length + " single-point marker(s) on ductwork layers");

        // For each marker, find nearby line intersections on the same layer
        var markersProcessed = 0;
        var markersToRemove = [];
        for (var si = 0; si < soloMarkers.length; si++) {
            var marker = soloMarkers[si];
            var markerPos = marker.position;

            // Get multi-point paths on the same layer
            var sameLayerPaths = [];
            for (var lpi = 0; lpi < allPaths.length; lpi++) {
                try {
                    var lPath = allPaths[lpi];
                    if (!lPath || !lPath.pathPoints || lPath.pathPoints.length < 2) continue;
                    var lpLayerName = lPath.layer ? lPath.layer.name : null;
                    if (lpLayerName === marker.layerName) {
                        sameLayerPaths.push(lPath);
                    }
                } catch (e) { }
            }

            if (sameLayerPaths.length < 2) continue; // Need at least 2 paths for an intersection

            // Check all path pairs for intersections near this marker
            var foundAndProcessed = false;
            for (var pA = 0; pA < sameLayerPaths.length && !foundAndProcessed; pA++) {
                var pathA = sameLayerPaths[pA];
                var ptsA = pathA.pathPoints;
                if (!ptsA || ptsA.length < 2) continue;

                for (var pB = pA + 1; pB < sameLayerPaths.length && !foundAndProcessed; pB++) {
                    var pathB = sameLayerPaths[pB];
                    var ptsB = pathB.pathPoints;
                    if (!ptsB || ptsB.length < 2) continue;

                    // Check all segment pairs for intersection
                    for (var sA = 0; sA < ptsA.length - 1 && !foundAndProcessed; sA++) {
                        for (var sB = 0; sB < ptsB.length - 1 && !foundAndProcessed; sB++) {
                            var intPt = getSegmentIntersectionPoint(
                                ptsA[sA].anchor[0], ptsA[sA].anchor[1],
                                ptsA[sA + 1].anchor[0], ptsA[sA + 1].anchor[1],
                                ptsB[sB].anchor[0], ptsB[sB].anchor[1],
                                ptsB[sB + 1].anchor[0], ptsB[sB + 1].anchor[1]
                            );

                            if (intPt) {
                                // Check if this intersection is near the marker
                                var dx = intPt[0] - markerPos[0];
                                var dy = intPt[1] - markerPos[1];
                                var distToMarker = Math.sqrt(dx * dx + dy * dy);

                                if (distToMarker <= MARKER_INTERSECTION_TOLERANCE) {
                                    addDebug("[MARKER-INTERSECT] Marker at [" + markerPos[0].toFixed(1) + "," + markerPos[1].toFixed(1) +
                                        "] near intersection at [" + intPt[0].toFixed(1) + "," + intPt[1].toFixed(1) + "] (dist=" + distToMarker.toFixed(1) + ")");

                                    // Check if either path already has a vertex at this intersection
                                    var hasVertexA = false, hasVertexB = false;
                                    for (var vA = 0; vA < ptsA.length; vA++) {
                                        var vdxA = ptsA[vA].anchor[0] - intPt[0];
                                        var vdyA = ptsA[vA].anchor[1] - intPt[1];
                                        if (Math.sqrt(vdxA * vdxA + vdyA * vdyA) <= VERTEX_INSERT_TOLERANCE) {
                                            hasVertexA = true;
                                            break;
                                        }
                                    }
                                    for (var vB = 0; vB < ptsB.length; vB++) {
                                        var vdxB = ptsB[vB].anchor[0] - intPt[0];
                                        var vdyB = ptsB[vB].anchor[1] - intPt[1];
                                        if (Math.sqrt(vdxB * vdxB + vdyB * vdyB) <= VERTEX_INSERT_TOLERANCE) {
                                            hasVertexB = true;
                                            break;
                                        }
                                    }

                                    if (!hasVertexA && !hasVertexB) {
                                        // Neither path has a vertex - insert one into pathA at segment sA
                                        addDebug("[MARKER-INTERSECT] Inserting vertex into path at segment " + sA);
                                        if (insertVertexIntoPath(pathA, sA, intPt)) {
                                            addDebug("[MARKER-INTERSECT] Successfully inserted vertex at [" + intPt[0].toFixed(1) + "," + intPt[1].toFixed(1) + "]");
                                            markersToRemove.push(marker.path);
                                            markersProcessed++;
                                            foundAndProcessed = true;
                                        }
                                    } else {
                                        addDebug("[MARKER-INTERSECT] Intersection already has vertex (A=" + hasVertexA + ", B=" + hasVertexB + ") - skipping");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Remove processed markers
        for (var rmIdx = 0; rmIdx < markersToRemove.length; rmIdx++) {
            try {
                markersToRemove[rmIdx].remove();
                addDebug("[MARKER-INTERSECT] Removed processed marker");
            } catch (e) { }
        }

        // Also remove processed markers from allPaths and SELECTED_PATHS
        if (markersToRemove.length > 0) {
            var removeSet = {};
            for (var rsIdx = 0; rsIdx < markersToRemove.length; rsIdx++) {
                try {
                    // Use a unique identifier - concatenate position coordinates
                    var rmPath = markersToRemove[rsIdx];
                    // Path is already removed, skip trying to access it
                } catch (e) { }
            }
            // Filter allPaths to remove any paths that no longer exist
            var filteredAllPaths = [];
            for (var fpi = 0; fpi < allPaths.length; fpi++) {
                try {
                    var fPath = allPaths[fpi];
                    // Test if path is still valid by accessing a property
                    var testTypename = fPath.typename;
                    filteredAllPaths.push(fPath);
                } catch (e) {
                    // Path no longer exists, skip it
                }
            }
            allPaths = filteredAllPaths;
        }

        addDebug("[MARKER-INTERSECT] Processed " + markersProcessed + " marker-triggered intersection(s)");

        // PRE-STEP 4: Detect ductwork line intersections where one path has a vertex at the crossing
        // These intersection points should be ignored for component placement (no registers)
        // Process each color separately (same-color intersections only)
        addDebug("\n=== DETECTING INTERSECTION VERTICES (ALL COLORS) ===");
        updateProgress("Detecting intersection vertices...");
        var INTERSECT_VERTEX_TOLERANCE = 10;
        var intersectionVertexIgnorePoints = [];

        for (var ivColorIdx = 0; ivColorIdx < ALL_DUCTWORK_SOURCES.length; ivColorIdx++) {
            var ivSource = ALL_DUCTWORK_SOURCES[ivColorIdx];
            var colorPathsForIntersect = filterPathsToProcessable(getPathsOnLayerSelected(ivSource.layer));
            if (colorPathsForIntersect.length < 2) continue;

            addDebug("[INTERSECT-VERTEX] Checking " + ivSource.name + ": " + colorPathsForIntersect.length + " paths");

            for (var intA = 0; intA < colorPathsForIntersect.length; intA++) {
                var pathIntA = colorPathsForIntersect[intA];
                var ptsIntA = pathIntA.pathPoints;
                if (!ptsIntA || ptsIntA.length < 2) continue;

                for (var intB = intA + 1; intB < colorPathsForIntersect.length; intB++) {
                    var pathIntB = colorPathsForIntersect[intB];
                    var ptsIntB = pathIntB.pathPoints;
                    if (!ptsIntB || ptsIntB.length < 2) continue;

                    // Check all segment pairs for intersection
                    for (var segA = 0; segA < ptsIntA.length - 1; segA++) {
                        for (var segB = 0; segB < ptsIntB.length - 1; segB++) {
                            var intPt = getSegmentIntersectionPoint(
                                ptsIntA[segA].anchor[0], ptsIntA[segA].anchor[1],
                                ptsIntA[segA + 1].anchor[0], ptsIntA[segA + 1].anchor[1],
                                ptsIntB[segB].anchor[0], ptsIntB[segB].anchor[1],
                                ptsIntB[segB + 1].anchor[0], ptsIntB[segB + 1].anchor[1]
                            );
                            if (intPt) {
                                // Check if either path has a vertex near this intersection
                                var hasVertexA = false, hasVertexB = false;
                                for (var vA = 0; vA < ptsIntA.length; vA++) {
                                    var vdxA = ptsIntA[vA].anchor[0] - intPt[0];
                                    var vdyA = ptsIntA[vA].anchor[1] - intPt[1];
                                    if (Math.sqrt(vdxA * vdxA + vdyA * vdyA) <= INTERSECT_VERTEX_TOLERANCE) {
                                        hasVertexA = true;
                                        break;
                                    }
                                }
                                for (var vB = 0; vB < ptsIntB.length; vB++) {
                                    var vdxB = ptsIntB[vB].anchor[0] - intPt[0];
                                    var vdyB = ptsIntB[vB].anchor[1] - intPt[1];
                                    if (Math.sqrt(vdxB * vdxB + vdyB * vdyB) <= INTERSECT_VERTEX_TOLERANCE) {
                                        hasVertexB = true;
                                        break;
                                    }
                                }
                                if (hasVertexA || hasVertexB) {
                                    // Add to ignore list - no component should be placed here
                                    intersectionVertexIgnorePoints.push([intPt[0], intPt[1]]);
                                    ignoredAnchors.push([intPt[0], intPt[1]]);
                                    addDebug("[INTERSECT-VERTEX] " + ivSource.name + ": Found intersection with vertex at [" + intPt[0].toFixed(1) + "," + intPt[1].toFixed(1) + "] - added to ignore list");
                                }
                            }
                        }
                    }
                }
            }
        }
        addDebug("[INTERSECT-VERTEX] Total intersection vertex ignore points: " + intersectionVertexIgnorePoints.length);

        // STEP 4: Create Registers, but skip endpoints that are close to existing Units, Ignored points, or existing points
        updateProgress("Creating registers...");
        // *** REFACTORED: Create a single, comprehensive list of all existing points to check against ***
        var allExistingRegisterPoints = getExistingAnchorPoints([
            "Units",
            "Square Registers",
            "Exhaust Registers",
            "Thermostats",
            "Rectangular Registers"
        ]);

        // Get anchor points from Rectangular Registers layer specifically for the Square/Rectangular duplicate check
        var rectangularRegisterAnchors = getExistingAnchorPoints(["Rectangular Registers"]);

        // Use same ductwork layers for both normal and Emory mode
        addDebug("=== REGISTER/UNIT CREATION (ALL COLORS) ===");

        // Log path counts for all 5 colors
        for (var regLogIdx = 0; regLogIdx < ALL_DUCTWORK_SOURCES.length; regLogIdx++) {
            var regLogSrc = ALL_DUCTWORK_SOURCES[regLogIdx];
            var regLogSelected = getPathsOnLayerSelected(regLogSrc.layer) || [];
            var regLogAll = getPathsOnLayerAll(regLogSrc.layer) || [];
            addDebug(regLogSrc.name + " (resolved: " + regLogSrc.layer + ") paths: " + regLogSelected.length + " selected, " + regLogAll.length + " total");
        }
        addDebug("");

        // Create registers for all 5 ductwork colors using the color-to-register mapping
        for (var regCreateIdx = 0; regCreateIdx < ALL_DUCTWORK_SOURCES.length; regCreateIdx++) {
            var regSrc = ALL_DUCTWORK_SOURCES[regCreateIdx];
            var registerLayer = getRegisterLayerForDuctwork(regSrc.name);
            // Blue uses rectangularRegisterAnchors for duplicate checking, others don't
            var rectAnchors = (regSrc.name === "Blue Ductwork") ? rectangularRegisterAnchors : null;
            duplicateIsolatedEndpointsFiltered(regSrc.layer, registerLayer, ignoredAnchors, allExistingRegisterPoints, rectAnchors);
        }


        // *** CARVE OUT DUCTWORK LINES THAT PASS THROUGH REGISTERS ***
        // After placing registers, check if OTHER ductwork lines pass through register areas
        // and carve out those segments to prevent overlap (each color processes its own register type)
        addDebug("\n=== REGISTER CARVE-OUT (ALL COLORS) ===");

        if (!ENABLE_REGISTER_CARVE) {
            addDebug("[REGISTER-CARVE] SKIPPED - checkbox not enabled");
            updateProgress("Register carve skipped...");
        } else {
            updateProgress("Carving register gaps...");
            var REGISTER_CARVE_HALF_WIDTH = 13.5; // 27pt total gap centered on register
            var REGISTER_DETECTION_THRESHOLD = 10; // How close a line segment must be to register center to trigger carve-out

            // Process each ductwork color with its own register layer
            for (var rcColorIdx = 0; rcColorIdx < ALL_DUCTWORK_SOURCES.length; rcColorIdx++) {
                var rcColorSrc = ALL_DUCTWORK_SOURCES[rcColorIdx];
                var rcRegisterLayerName = getRegisterLayerForDuctwork(rcColorSrc.name);
                var rcRegLayer = null;
                try { rcRegLayer = doc.layers.getByName(rcRegisterLayerName); } catch (e) { }

                if (!rcRegLayer) {
                    addDebug("[REGISTER-CARVE] " + rcColorSrc.name + ": No " + rcRegisterLayerName + " layer found, skipping");
                    continue;
                }

            // Collect all register center positions for this color's register layer
            var registerCenters = [];
            for (var sri = 0; sri < rcRegLayer.pathItems.length; sri++) {
                try {
                    var regPath = rcRegLayer.pathItems[sri];
                    if (regPath.pathPoints && regPath.pathPoints.length > 0) {
                        var regPt = regPath.pathPoints[0].anchor;
                        registerCenters.push(regPt);
                    }
                } catch (e) { }
            }
            if (registerCenters.length === 0) {
                addDebug("[REGISTER-CARVE] " + rcColorSrc.name + ": No registers in " + rcRegisterLayerName + ", skipping");
                continue;
            }
            addDebug("[REGISTER-CARVE] " + rcColorSrc.name + ": Found " + registerCenters.length + " " + rcRegisterLayerName + " position(s)");

            // Get SELECTED ductwork paths for this color only - do not touch unselected paths
            var colorPathsForCarveRaw = getPathsOnLayerSelected(rcColorSrc.layer) || [];

            // Extract paths from compound paths and include regular paths
            var colorPathsForCarve = [];
            for (var filterIdx = 0; filterIdx < colorPathsForCarveRaw.length; filterIdx++) {
                var filterPath = colorPathsForCarveRaw[filterIdx];
                if (!filterPath) continue;

                // For CompoundPathItems: extract child paths
                if (filterPath.typename === "CompoundPathItem") {
                    try {
                        for (var cpIdx = 0; cpIdx < filterPath.pathItems.length; cpIdx++) {
                            var childPath = filterPath.pathItems[cpIdx];
                            if (childPath && childPath.pathPoints && childPath.pathPoints.length >= 2) {
                                colorPathsForCarve.push(childPath);
                                addDebug("[CARVE-OUT] Including child path from CompoundPathItem");
                            }
                        }
                    } catch (e) {
                        addDebug("[CARVE-OUT] Error extracting compound children: " + e);
                    }
                    continue;
                }

                // Include regular paths (even if parent is CompoundPathItem - they're valid paths)
                colorPathsForCarve.push(filterPath);
            }
            addDebug("[REGISTER-CARVE] " + rcColorSrc.name + ": Checking " + colorPathsForCarve.length + " selected path(s) (filtered from " + colorPathsForCarveRaw.length + ")");
            var carveOutsPerformed = 0;
            var pathsToRemove = [];

            addDebug("[CARVE-OUT] Detection threshold: " + REGISTER_DETECTION_THRESHOLD + "pt");

            // SAVE THE BLUE PATHS that need to be restored to selection after carve-out
            // We use colorPathsForCarve since doc.selection may already be empty at this point
            var originalColorPaths = [];
            for (var origSelIdx = 0; origSelIdx < colorPathsForCarve.length; origSelIdx++) {
                originalColorPaths.push(colorPathsForCarve[origSelIdx]);
            }
            addDebug("[CARVE-OUT] Saved " + originalColorPaths.length + " blue paths for restoration");

            // Track new compound paths created during carve-out
            var newCompoundPaths = [];

            // === SPATIAL HASHING FOR REGISTER CARVE-OUT O(n) PERFORMANCE ===
            var RC_CELL_SIZE = 50; // Grid cell size for register carve-out
            var rcSpatialGrid = {};
            var rcPathToCells = [];

            // Build spatial index for paths
            for (var rcBuildIdx = 0; rcBuildIdx < colorPathsForCarve.length; rcBuildIdx++) {
                var rcBuildPath = colorPathsForCarve[rcBuildIdx];
                var rcBuildPts = rcBuildPath ? rcBuildPath.pathPoints : null;
                if (!rcBuildPts || rcBuildPts.length === 0) {
                    rcPathToCells.push([]);
                    continue;
                }
                var rcMinX = Infinity, rcMinY = Infinity, rcMaxX = -Infinity, rcMaxY = -Infinity;
                for (var rcBp = 0; rcBp < rcBuildPts.length; rcBp++) {
                    var rcAnchor = rcBuildPts[rcBp].anchor;
                    if (rcAnchor[0] < rcMinX) rcMinX = rcAnchor[0];
                    if (rcAnchor[0] > rcMaxX) rcMaxX = rcAnchor[0];
                    if (rcAnchor[1] < rcMinY) rcMinY = rcAnchor[1];
                    if (rcAnchor[1] > rcMaxY) rcMaxY = rcAnchor[1];
                }
                // Expand by detection threshold
                rcMinX -= REGISTER_DETECTION_THRESHOLD; rcMinY -= REGISTER_DETECTION_THRESHOLD;
                rcMaxX += REGISTER_DETECTION_THRESHOLD; rcMaxY += REGISTER_DETECTION_THRESHOLD;
                var rcCellMinX = Math.floor(rcMinX / RC_CELL_SIZE);
                var rcCellMaxX = Math.floor(rcMaxX / RC_CELL_SIZE);
                var rcCellMinY = Math.floor(rcMinY / RC_CELL_SIZE);
                var rcCellMaxY = Math.floor(rcMaxY / RC_CELL_SIZE);
                var rcCells = [];
                for (var rcCx = rcCellMinX; rcCx <= rcCellMaxX; rcCx++) {
                    for (var rcCy = rcCellMinY; rcCy <= rcCellMaxY; rcCy++) {
                        var rcCellKey = rcCx + "," + rcCy;
                        rcCells.push(rcCellKey);
                        if (!rcSpatialGrid[rcCellKey]) rcSpatialGrid[rcCellKey] = [];
                        rcSpatialGrid[rcCellKey].push(rcBuildIdx);
                    }
                }
                rcPathToCells.push(rcCells);
            }

            // For each register, check only nearby paths (spatial hash lookup)
            for (var rcIdx = 0; rcIdx < registerCenters.length; rcIdx++) {
                var regCenter = registerCenters[rcIdx];
                // if (rcIdx % 10 === 0) yieldToUI();

                // Find which cell this register is in
                var regCellX = Math.floor(regCenter[0] / RC_CELL_SIZE);
                var regCellY = Math.floor(regCenter[1] / RC_CELL_SIZE);

                // Get candidate paths from this cell and adjacent cells
                var rcCandidates = {};
                for (var adjX = regCellX - 1; adjX <= regCellX + 1; adjX++) {
                    for (var adjY = regCellY - 1; adjY <= regCellY + 1; adjY++) {
                        var adjKey = adjX + "," + adjY;
                        var adjPaths = rcSpatialGrid[adjKey];
                        if (adjPaths) {
                            for (var adjP = 0; adjP < adjPaths.length; adjP++) {
                                rcCandidates[adjPaths[adjP]] = true;
                            }
                        }
                    }
                }

                // Check only candidate paths (PERF: removed per-path logging)
                for (var bpIdx in rcCandidates) {
                    if (!rcCandidates.hasOwnProperty(bpIdx)) continue;
                    bpIdx = parseInt(bpIdx);
                    var colorPath = colorPathsForCarve[bpIdx];
                    if (!colorPath || !colorPath.pathPoints || colorPath.pathPoints.length < 2) continue;

                    var pts = colorPath.pathPoints;

                    // Skip if the register is at an endpoint of THIS path
                    var firstPt = pts[0].anchor;
                    var lastPt = pts[pts.length - 1].anchor;
                    var isRegisterOnThisPathEndpoint =
                        (Math.abs(firstPt[0] - regCenter[0]) < 5 && Math.abs(firstPt[1] - regCenter[1]) < 5) ||
                        (Math.abs(lastPt[0] - regCenter[0]) < 5 && Math.abs(lastPt[1] - regCenter[1]) < 5);

                    if (isRegisterOnThisPathEndpoint) continue;

                    // Check each segment for intersection with register area (PERF: removed per-segment logging)
                    for (var segIdx = 0; segIdx < pts.length - 1; segIdx++) {
                        var segStart = pts[segIdx].anchor;
                        var segEnd = pts[segIdx + 1].anchor;
                        var segDx = segEnd[0] - segStart[0];
                        var segDy = segEnd[1] - segStart[1];
                        var segLen = Math.sqrt(segDx * segDx + segDy * segDy);
                        if (segLen < 0.01) continue;

                        // Project register center onto the line
                        var t = ((regCenter[0] - segStart[0]) * segDx + (regCenter[1] - segStart[1]) * segDy) / (segLen * segLen);
                        if (t < 0.02 || t > 0.98) continue;

                        // Calculate closest point on segment to register center
                        var closestX = segStart[0] + t * segDx;
                        var closestY = segStart[1] + t * segDy;
                        var distToSeg = Math.sqrt(Math.pow(regCenter[0] - closestX, 2) + Math.pow(regCenter[1] - closestY, 2));

                        if (distToSeg > REGISTER_DETECTION_THRESHOLD) continue;

                        addDebug("[REGISTER-CARVE] " + rcColorSrc.name + ": Path segment passes through register at [" + regCenter[0].toFixed(1) + "," + regCenter[1].toFixed(1) + "]");

                        // Calculate cut points along the LINE (not from register center)
                        // Use the closest point on the segment as the center of the gap
                        var dirX = segDx / segLen;
                        var dirY = segDy / segLen;
                        var cutBefore = [closestX - REGISTER_CARVE_HALF_WIDTH * dirX, closestY - REGISTER_CARVE_HALF_WIDTH * dirY];
                        var cutAfter = [closestX + REGISTER_CARVE_HALF_WIDTH * dirX, closestY + REGISTER_CARVE_HALF_WIDTH * dirY];

                        try {
                            var firstHalf = colorPath.duplicate();
                            var firstPts = firstHalf.pathPoints;
                            for (var delIdx = firstPts.length - 1; delIdx > segIdx; delIdx--) {
                                firstPts[delIdx].remove();
                            }
                            var newEndPt = firstHalf.pathPoints.add();
                            newEndPt.anchor = cutBefore;
                            newEndPt.leftDirection = cutBefore;
                            newEndPt.rightDirection = cutBefore;

                            var secondHalf = colorPath.duplicate();
                            var secondPts = secondHalf.pathPoints;

                            // Set the segment start point (segIdx) to cutAfter position
                            // This keeps the path structure intact
                            secondPts[segIdx].anchor = cutAfter;
                            secondPts[segIdx].leftDirection = cutAfter;
                            secondPts[segIdx].rightDirection = cutAfter;

                            // Remove all points BEFORE segIdx (from end to avoid index shifting)
                            for (var delIdx2 = segIdx - 1; delIdx2 >= 0; delIdx2--) {
                                secondPts[delIdx2].remove();
                            }

                            // Select the new paths so they're included in subsequent processing
                            firstHalf.selected = true;
                            secondHalf.selected = true;

                            // Create ignore anchor markers on the Ignored layer
                            try {
                                var ignoredLayer = null;
                                try { ignoredLayer = doc.layers.getByName("Ignored"); } catch (eNoIgn) { }
                                if (!ignoredLayer) {
                                    ignoredLayer = doc.layers.add();
                                    ignoredLayer.name = "Ignored";
                                }
                                // Unlock the layer AND make it visible if needed
                                if (ignoredLayer) {
                                    try {
                                        if (ignoredLayer.locked) {
                                            ignoredLayer.locked = false;
                                            addDebug("[CARVE-OUT] Unlocked Ignored layer");
                                        }
                                        if (!ignoredLayer.visible) {
                                            ignoredLayer.visible = true;
                                            addDebug("[CARVE-OUT] Made Ignored layer visible");
                                        }
                                        // Also check parent containers - unlock them too if locked (Layers or GroupItems)
                                        var parentContainer = ignoredLayer.parent;
                                        while (parentContainer) {
                                            var pType = parentContainer.typename;
                                            if (pType === "Layer" || pType === "GroupItem") {
                                                try {
                                                    if (parentContainer.locked) {
                                                        parentContainer.locked = false;
                                                        addDebug("[CARVE-OUT] Unlocked parent " + pType + ": " + (parentContainer.name || "(unnamed)"));
                                                    }
                                                } catch (ePLock) { }
                                                try {
                                                    if (!parentContainer.visible) {
                                                        parentContainer.visible = true;
                                                        addDebug("[CARVE-OUT] Made parent " + pType + " visible: " + (parentContainer.name || "(unnamed)"));
                                                    }
                                                } catch (ePVis) { }
                                            }
                                            if (pType !== "Layer" && pType !== "GroupItem") break;
                                            parentContainer = parentContainer.parent;
                                        }
                                    } catch (eUnlock) {
                                        addDebug("[CARVE-OUT] Could not unlock/show layer: " + eUnlock);
                                    }
                                }
                                if (ignoredLayer) {
                                    addDebug("[CARVE-OUT] Layer state before add: locked=" + ignoredLayer.locked + ", visible=" + ignoredLayer.visible + ", parent=" + (ignoredLayer.parent ? ignoredLayer.parent.typename : "none"));
                                    // Create ignore anchor at cutBefore
                                    var ignoreAnchor1 = ignoredLayer.pathItems.add();
                                    ignoreAnchor1.setEntirePath([[cutBefore[0], cutBefore[1]]]);
                                    ignoreAnchor1.filled = false;
                                    ignoreAnchor1.stroked = false;
                                    addDebug("[CARVE-OUT] Placed ignore anchor at [" + cutBefore[0].toFixed(1) + "," + cutBefore[1].toFixed(1) + "]");
                                    // Add to ignoredAnchors array so component placement skips this position
                                    ignoredAnchors.push([cutBefore[0], cutBefore[1]]);

                                    // Create ignore anchor at cutAfter
                                    var ignoreAnchor2 = ignoredLayer.pathItems.add();
                                    ignoreAnchor2.setEntirePath([[cutAfter[0], cutAfter[1]]]);
                                    ignoreAnchor2.filled = false;
                                    ignoreAnchor2.stroked = false;
                                    addDebug("[CARVE-OUT] Placed ignore anchor at [" + cutAfter[0].toFixed(1) + "," + cutAfter[1].toFixed(1) + "]");
                                    // Add to ignoredAnchors array so component placement skips this position
                                    ignoredAnchors.push([cutAfter[0], cutAfter[1]]);

                                    addDebug("[CARVE-OUT] Added carve-out positions to ignoredAnchors array (total: " + ignoredAnchors.length + ")");
                                }
                            } catch (eIgnore) {
                                addDebug("[CARVE-OUT] Warning: Could not create ignore anchors: " + eIgnore + " (layer locked=" + (ignoredLayer ? ignoredLayer.locked : "null") + ", visible=" + (ignoredLayer ? ignoredLayer.visible : "null") + ")");
                            }

                            // Compound the two halves into a single compound path
                            // BUT skip if the original path was already part of a compound (to avoid "already part of compound" error)
                            var isAlreadyCompoundChild = false;
                            try {
                                if (colorPath.parent && colorPath.parent.typename === "CompoundPathItem") {
                                    isAlreadyCompoundChild = true;
                                    addDebug("[CARVE-OUT] Original path was part of compound - skipping re-compounding");
                                }
                            } catch (eCheckParent) { }

                            if (!isAlreadyCompoundChild) {
                                try {
                                    // Deselect everything
                                    doc.selection = null;
                                    // Select just the two halves
                                    firstHalf.selected = true;
                                    secondHalf.selected = true;
                                    // Create compound path
                                    if (doc.selection.length === 2) {
                                        app.executeMenuCommand("compoundPath");
                                        addDebug("[CARVE-OUT] Compounded split segments into compound path");

                                        // The compound path is now selected - capture it for later
                                        if (doc.selection.length === 1) {
                                            var newCompound = doc.selection[0];
                                            newCompoundPaths.push(newCompound);
                                            CARVE_OUT_COMPOUNDS.push(newCompound); // Also add to global tracking for styling
                                            SELECTED_PATHS.push(newCompound); // Also add to SELECTED_PATHS for component placement
                                            addDebug("[CARVE-OUT] New compound path captured (total: " + newCompoundPaths.length + ", global: " + CARVE_OUT_COMPOUNDS.length + ", SELECTED_PATHS: " + SELECTED_PATHS.length + ")");
                                        }
                                    }
                                } catch (eCompound) {
                                    addDebug("[CARVE-OUT] Warning: Could not compound paths: " + eCompound);
                                }
                            } else {
                                // Still add the split halves to SELECTED_PATHS for component placement
                                SELECTED_PATHS.push(firstHalf);
                                SELECTED_PATHS.push(secondHalf);
                                addDebug("[CARVE-OUT] Added split halves to SELECTED_PATHS without compounding");
                            }

                            pathsToRemove.push(colorPath);
                            ignoredAnchors.push(cutBefore);
                            ignoredAnchors.push(cutAfter);
                            carveOutsPerformed++;
                            addDebug("[CARVE-OUT] Created gap from [" + cutBefore[0].toFixed(1) + "," + cutBefore[1].toFixed(1) + "] to [" + cutAfter[0].toFixed(1) + "," + cutAfter[1].toFixed(1) + "]");
                            break;
                        } catch (eCarve) {
                            addDebug("[CARVE-OUT] Error: " + eCarve);
                        }
                    }
                }
            }

            for (var remIdx = 0; remIdx < pathsToRemove.length; remIdx++) {
                try {
                    var pathToRemove = pathsToRemove[remIdx];
                    // Remove from SELECTED_PATHS before deleting the path
                    for (var spIdx = SELECTED_PATHS.length - 1; spIdx >= 0; spIdx--) {
                        try {
                            if (SELECTED_PATHS[spIdx] === pathToRemove) {
                                SELECTED_PATHS.splice(spIdx, 1);
                                addDebug("[CARVE-OUT] Removed deleted path from SELECTED_PATHS (now " + SELECTED_PATHS.length + " paths)");
                            }
                        } catch (eSpCheck) { }
                    }
                    pathToRemove.remove();
                } catch (eRem) { }
            }
            addDebug("[CARVE-OUT] Performed " + carveOutsPerformed + " carve-out(s)");

            // RESTORE SELECTION: original blue paths (if still valid) plus new compound paths
            if (carveOutsPerformed > 0) {
                doc.selection = null; // Clear first
                var restoredCount = 0;

                // Restore original blue paths - try to select each one
                // If the item was removed, it will throw an error which we catch
                for (var restIdx = 0; restIdx < originalColorPaths.length; restIdx++) {
                    try {
                        var origItem = originalColorPaths[restIdx];
                        // Check if item is still valid by accessing a property
                        var testValid = origItem.typename;
                        origItem.selected = true;
                        restoredCount++;
                    } catch (eRest) {
                        // Item was removed or is invalid - skip it
                    }
                }

                // Add new compound paths to selection
                for (var cpIdx = 0; cpIdx < newCompoundPaths.length; cpIdx++) {
                    try {
                        newCompoundPaths[cpIdx].selected = true;
                        restoredCount++;
                    } catch (eCpSel) { }
                }

                addDebug("[CARVE-OUT] Restored selection: " + restoredCount + " items (including " + newCompoundPaths.length + " new compound path(s))");
            }
            } // end for (rcColorIdx - each ductwork color)
        } // end if (ENABLE_REGISTER_CARVE)

        // *** AUTOMATIC DUCTWORK PATH INTERSECTION CARVE-OUT ***
        // Detect where ductwork paths of the same color cross each other (without requiring small segments)
        // and create carve-outs at those intersection points (same-color only, no cross-color carving)
        updateProgress(!ENABLE_OVERLAP_CARVE ? "Overlap carve skipped..." : "Carving intersections...");
        addDebug("\n=== AUTOMATIC PATH INTERSECTION CARVE-OUT (ALL COLORS) ===");

        // Define variables outside conditional so they're always available
        var AUTO_CARVE_HALF_WIDTH = 4.25; // 8.5pt total gap, same as small segment crossovers
        var autoIntersections = [];
        var autoPathsToRemove = [];
        var autoNewCompoundPaths = [];
        // Track path pairs that should NOT be connected via T-junction
        // (carved halves should not connect to the crossing path that caused the carve)
        var CARVE_BLOCKED_CONNECTIONS = [];

        // Track split path pairs that SHOULD be connected (siblings from same carve-out)
        var AUTO_CARVE_SPLIT_PAIRS = [];

        // Helper function to save deleted segments to a "Deleted Segments" layer for safety/recovery
        function saveDeletedSegment(startPt, endPt, sourceLayer) {
            try {
                var deletedLayer = null;
                try { deletedLayer = doc.layers.getByName("Deleted Segments"); } catch (eNoLayer) { }
                if (!deletedLayer) {
                    deletedLayer = doc.layers.add();
                    deletedLayer.name = "Deleted Segments";
                    addDebug("[DELETED-SEG] Created 'Deleted Segments' layer");
                }
                // Unlock and show layer temporarily to add the path
                if (deletedLayer.locked) deletedLayer.locked = false;
                if (!deletedLayer.visible) deletedLayer.visible = true;

                // Create a path for the deleted segment
                var segPath = deletedLayer.pathItems.add();
                segPath.setEntirePath([[startPt[0], startPt[1]], [endPt[0], endPt[1]]]);
                segPath.filled = false;
                segPath.stroked = true;
                segPath.strokeWidth = 1;
                try {
                    // Try to match stroke color from source layer
                    var sourceLayerObj = null;
                    try { sourceLayerObj = doc.layers.getByName(sourceLayer); } catch (eNoSrc) { }
                    if (sourceLayerObj && sourceLayerObj.pathItems.length > 0) {
                        segPath.strokeColor = sourceLayerObj.pathItems[0].strokeColor;
                    }
                } catch (eColor) { }

                // Hide and lock the layer after adding the path
                deletedLayer.visible = false;
                deletedLayer.locked = true;

                addDebug("[DELETED-SEG] Saved segment from [" + startPt[0].toFixed(1) + "," + startPt[1].toFixed(1) + "] to [" + endPt[0].toFixed(1) + "," + endPt[1].toFixed(1) + "]");
                return segPath;
            } catch (eSave) {
                addDebug("[DELETED-SEG] ERROR saving segment: " + eSave);
                return null;
            }
        }

        if (!ENABLE_OVERLAP_CARVE) {
            addDebug("[AUTO-CARVE] SKIPPED - checkbox not enabled");
        } else {
            // Process each ductwork color separately (same-color intersections only)
            for (var acColorIdx = 0; acColorIdx < ALL_DUCTWORK_SOURCES.length; acColorIdx++) {
                var acColorSrc = ALL_DUCTWORK_SOURCES[acColorIdx];

                // Start of auto-carve processing for this color
                var colorPathsForAutoCarveRaw = getPathsOnLayerSelected(acColorSrc.layer) || [];
                if (colorPathsForAutoCarveRaw.length < 2) {
                    addDebug("[AUTO-CARVE] " + acColorSrc.name + ": Less than 2 paths, skipping");
                    continue; // Need at least 2 paths to have an intersection
                }

                // Extract paths from compound paths and include regular paths
                var colorPathsForAutoCarve = [];
                for (var acFilterIdx = 0; acFilterIdx < colorPathsForAutoCarveRaw.length; acFilterIdx++) {
                    var acFilterPath = colorPathsForAutoCarveRaw[acFilterIdx];
                    if (!acFilterPath) continue;

                    // For CompoundPathItems: extract child paths
                    if (acFilterPath.typename === "CompoundPathItem") {
                        try {
                            for (var acCpIdx = 0; acCpIdx < acFilterPath.pathItems.length; acCpIdx++) {
                                var acChildPath = acFilterPath.pathItems[acCpIdx];
                                if (acChildPath && acChildPath.pathPoints && acChildPath.pathPoints.length >= 2) {
                                    colorPathsForAutoCarve.push(acChildPath);
                                    addDebug("[AUTO-CARVE] " + acColorSrc.name + ": Including child path from CompoundPathItem");
                                }
                            }
                        } catch (e) {
                            addDebug("[AUTO-CARVE] " + acColorSrc.name + ": Error extracting compound children: " + e);
                        }
                        continue;
                    }

                    // Include regular paths (even if parent is CompoundPathItem - they're valid paths)
                    colorPathsForAutoCarve.push(acFilterPath);
                }
                addDebug("[AUTO-CARVE] " + acColorSrc.name + ": Checking " + colorPathsForAutoCarve.length + " selected path(s) for intersections (filtered from " + colorPathsForAutoCarveRaw.length + ")");

            // Build a spatial hash of already-handled crossover locations for O(1) lookup
            var handledCrossoverPoints = [];
            var handledCrossoverGrid = {};
            var HC_CELL_SIZE = 20; // Cell size for handled crossover spatial hash
            if (typeof EARLY_CROSSOVER_SEGMENTS !== 'undefined' && EARLY_CROSSOVER_SEGMENTS.length > 0) {
                for (var hcIdx = 0; hcIdx < EARLY_CROSSOVER_SEGMENTS.length; hcIdx++) {
                    var hcSeg = EARLY_CROSSOVER_SEGMENTS[hcIdx];
                    var hcMidX = (hcSeg.segStart[0] + hcSeg.segEnd[0]) / 2;
                    var hcMidY = (hcSeg.segStart[1] + hcSeg.segEnd[1]) / 2;
                    handledCrossoverPoints.push([hcMidX, hcMidY]);
                    // Add to spatial grid
                    var hcCellKey = Math.floor(hcMidX / HC_CELL_SIZE) + "," + Math.floor(hcMidY / HC_CELL_SIZE);
                    if (!handledCrossoverGrid[hcCellKey]) handledCrossoverGrid[hcCellKey] = [];
                    handledCrossoverGrid[hcCellKey].push([hcMidX, hcMidY]);
                }
                addDebug("[AUTO-CARVE] Found " + handledCrossoverPoints.length + " already-handled crossover point(s)");
            }

            // Helper function for O(1) handled crossover check
            function isHandledCrossover(pt) {
                var cellX = Math.floor(pt[0] / HC_CELL_SIZE);
                var cellY = Math.floor(pt[1] / HC_CELL_SIZE);
                for (var dx = -1; dx <= 1; dx++) {
                    for (var dy = -1; dy <= 1; dy++) {
                        var checkKey = (cellX + dx) + "," + (cellY + dy);
                        var cellPoints = handledCrossoverGrid[checkKey];
                        if (cellPoints) {
                            for (var cp = 0; cp < cellPoints.length; cp++) {
                                var dist = Math.sqrt(Math.pow(pt[0] - cellPoints[cp][0], 2) + Math.pow(pt[1] - cellPoints[cp][1], 2));
                                if (dist < 15) return true;
                            }
                        }
                    }
                }
                return false;
            }

            // Pre-compute path lengths for optimization (avoid recalculating in inner loops)
            var pathLengths = [];
            for (var plIdx = 0; plIdx < colorPathsForAutoCarve.length; plIdx++) {
                var plPath = colorPathsForAutoCarve[plIdx];
                var plLen = 0;
                if (plPath && plPath.pathPoints) {
                    var plPts = plPath.pathPoints;
                    for (var plSeg = 0; plSeg < plPts.length - 1; plSeg++) {
                        var plDx = plPts[plSeg + 1].anchor[0] - plPts[plSeg].anchor[0];
                        var plDy = plPts[plSeg + 1].anchor[1] - plPts[plSeg].anchor[1];
                        plLen += Math.sqrt(plDx * plDx + plDy * plDy);
                    }
                }
                pathLengths.push(plLen);
            }

            // === SEGMENT-LEVEL SPATIAL HASHING FOR O(n) INTERSECTION DETECTION ===
            // Instead of comparing all segments of candidate paths, we put individual SEGMENTS
            // into grid cells and only compare segments that share cells. This reduces
            // O(paths² × segments²) to O(segments × k) where k = avg segments per cell (~2-5)

            var SEG_CELL_SIZE = 50; // Smaller cells for segment-level precision
            var segmentGrid = {};   // cellKey -> [{pathIdx, segIdx, start, end}, ...]
            var totalSegments = 0;

            // Phase 1: Build segment-level spatial index
            addDebug("[AUTO-CARVE] Building SEGMENT-LEVEL spatial index...");
            for (var sgPathIdx = 0; sgPathIdx < colorPathsForAutoCarve.length; sgPathIdx++) {
                var sgPath = colorPathsForAutoCarve[sgPathIdx];
                if (!sgPath || !sgPath.pathPoints || sgPath.pathPoints.length < 2) continue;
                var sgPts = sgPath.pathPoints;

                for (var sgSegIdx = 0; sgSegIdx < sgPts.length - 1; sgSegIdx++) {
                    var sgStart = sgPts[sgSegIdx].anchor;
                    var sgEnd = sgPts[sgSegIdx + 1].anchor;

                    // Find all cells this segment passes through (not just bounding box)
                    var sgMinX = Math.min(sgStart[0], sgEnd[0]);
                    var sgMaxX = Math.max(sgStart[0], sgEnd[0]);
                    var sgMinY = Math.min(sgStart[1], sgEnd[1]);
                    var sgMaxY = Math.max(sgStart[1], sgEnd[1]);

                    var sgCellMinX = Math.floor(sgMinX / SEG_CELL_SIZE);
                    var sgCellMaxX = Math.floor(sgMaxX / SEG_CELL_SIZE);
                    var sgCellMinY = Math.floor(sgMinY / SEG_CELL_SIZE);
                    var sgCellMaxY = Math.floor(sgMaxY / SEG_CELL_SIZE);

                    // Add segment to all cells it touches
                    for (var sgCx = sgCellMinX; sgCx <= sgCellMaxX; sgCx++) {
                        for (var sgCy = sgCellMinY; sgCy <= sgCellMaxY; sgCy++) {
                            var sgCellKey = sgCx + "," + sgCy;
                            if (!segmentGrid[sgCellKey]) segmentGrid[sgCellKey] = [];
                            segmentGrid[sgCellKey].push({
                                pathIdx: sgPathIdx,
                                segIdx: sgSegIdx,
                                start: sgStart,
                                end: sgEnd
                            });
                        }
                    }
                    totalSegments++;
                }
            }
            addDebug("[AUTO-CARVE] Indexed " + totalSegments + " segments into grid");

            // Phase 2: Check intersections only between segments in the same cell
            var checkedSegPairs = {}; // "pathA_segA_pathB_segB" -> true
            var segCheckCount = 0;

            for (var cellKey in segmentGrid) {
                if (!segmentGrid.hasOwnProperty(cellKey)) continue;
                var cellSegs = segmentGrid[cellKey];
                if (cellSegs.length < 2) continue; // Need at least 2 segments to intersect

                // UI yield every 100 cells
                segCheckCount++;
                if (segCheckCount % 100 === 0) {
                    // yieldToUI();
                }

                // Check all segment pairs in this cell
                for (var csA = 0; csA < cellSegs.length; csA++) {
                    var segA = cellSegs[csA];

                    for (var csB = csA + 1; csB < cellSegs.length; csB++) {
                        var segB = cellSegs[csB];

                        // Skip if same path and adjacent segments (share endpoint)
                        if (segA.pathIdx === segB.pathIdx) {
                            var segDiff = Math.abs(segA.segIdx - segB.segIdx);
                            if (segDiff <= 1) continue; // Adjacent segments
                        }

                        // Create unique pair key (always smaller path/seg first)
                        var segPairKey;
                        if (segA.pathIdx < segB.pathIdx ||
                            (segA.pathIdx === segB.pathIdx && segA.segIdx < segB.segIdx)) {
                            segPairKey = segA.pathIdx + "_" + segA.segIdx + "_" + segB.pathIdx + "_" + segB.segIdx;
                        } else {
                            segPairKey = segB.pathIdx + "_" + segB.segIdx + "_" + segA.pathIdx + "_" + segA.segIdx;
                        }

                        // Skip if already checked this pair
                        if (checkedSegPairs[segPairKey]) continue;
                        checkedSegPairs[segPairKey] = true;

                        // Check for intersection
                        var intersection = computeSegmentIntersection(segA.start, segA.end, segB.start, segB.end, 1e-6);
                        if (!intersection) continue;

                        // Skip if intersection is at endpoints (t near 0 or 1)
                        if (intersection.t1 < 0.05 || intersection.t1 > 0.95 ||
                            intersection.t2 < 0.05 || intersection.t2 > 0.95) continue;

                        var intPt = [intersection.point.x, intersection.point.y];

                        // Use spatial hash for O(1) handled crossover check
                        if (isHandledCrossover(intPt)) continue;

                        // Determine which path to carve
                        var pathALen = pathLengths[segA.pathIdx];
                        var pathBLen = pathLengths[segB.pathIdx];
                        var pathA = colorPathsForAutoCarve[segA.pathIdx];
                        var pathB = colorPathsForAutoCarve[segB.pathIdx];

                        // For self-intersections (same path), carve the later segment
                        if (segA.pathIdx === segB.pathIdx) {
                            var laterSeg = segA.segIdx > segB.segIdx ? segA : segB;
                            var laterT = segA.segIdx > segB.segIdx ? intersection.t1 : intersection.t2;
                            autoIntersections.push({
                                intPoint: intPt,
                                pathToCarve: pathA,
                                pathToCarveIdx: segA.pathIdx,
                                carveSegIdx: laterSeg.segIdx,
                                carveT: laterT,
                                otherPath: pathA,
                                isSelfIntersection: true
                            });
                        } else {
                            // For cross-path intersections, carve the shorter path
                            autoIntersections.push({
                                intPoint: intPt,
                                pathToCarve: pathALen <= pathBLen ? pathA : pathB,
                                pathToCarveIdx: pathALen <= pathBLen ? segA.pathIdx : segB.pathIdx,
                                carveSegIdx: pathALen <= pathBLen ? segA.segIdx : segB.segIdx,
                                carveT: pathALen <= pathBLen ? intersection.t1 : intersection.t2,
                                otherPath: pathALen <= pathBLen ? pathB : pathA,
                                carvedPathLen: pathALen <= pathBLen ? pathALen : pathBLen,
                                otherPathLen: pathALen <= pathBLen ? pathBLen : pathALen
                            });
                        }
                    }
                }
            }

            addDebug("[AUTO-CARVE] Segment-level check complete, checked " + segCheckCount + " cells")

            addDebug("[AUTO-CARVE] Found " + autoIntersections.length + " new intersection(s) to carve out");

            // Limit carve-outs to prevent runaway processing on complex selections
            var MAX_CARVE_OUTS = 100;
            var CARVE_BATCH_SIZE = 10; // Update progress every 10 carve-outs
            var carveCount = 0;

            // Process carve-outs (work backwards to avoid index issues)
            for (var acIdx = autoIntersections.length - 1; acIdx >= 0 && carveCount < MAX_CARVE_OUTS; acIdx--) {
                // BATCH: Update progress between batches
                if (carveCount > 0 && carveCount % CARVE_BATCH_SIZE === 0) {
                    if (progressWin && progressLabel) {
                        try {
                            progressLabel.text = "Carving... " + carveCount + "/" + Math.min(autoIntersections.length, MAX_CARVE_OUTS);
                            progressWin.update();
                        } catch (e) { }
                    }
                }
                var autoInt = autoIntersections[acIdx];
                var carvePath = autoInt.pathToCarve;
                var carveSegIdx = autoInt.carveSegIdx;
                var intPt = autoInt.intPoint;

                try {
                    // ROBUST validity check - test multiple properties
                    var carvePathValid = false;
                    try {
                        if (carvePath && carvePath.typename === "PathItem") {
                            var testPts = carvePath.pathPoints;
                            if (testPts && testPts.length >= 2) {
                                // Test if we can actually access the points
                                var testAnchor = testPts[0].anchor;
                                if (testAnchor && typeof testAnchor[0] === "number") {
                                    carvePathValid = true;
                                }
                            }
                        }
                    } catch (eValidCheck) {
                        addDebug("[AUTO-CARVE] Path validity check failed: " + eValidCheck);
                        carvePathValid = false;
                    }

                    if (!carvePathValid) {
                        addDebug("[AUTO-CARVE] Skipping invalid path at intersection [" + intPt[0].toFixed(1) + "," + intPt[1].toFixed(1) + "]");
                        continue;
                    }

                    // Check if this path was already carved (for self-intersections with multiple crossings)
                    var alreadyCarved = false;
                    for (var acCheckIdx = 0; acCheckIdx < autoPathsToRemove.length; acCheckIdx++) {
                        try {
                            if (autoPathsToRemove[acCheckIdx] === carvePath) {
                                alreadyCarved = true;
                                break;
                            }
                        } catch (eCheckCarve) { }
                    }
                    if (alreadyCarved) {
                        addDebug("[AUTO-CARVE] Path already carved, skipping intersection at [" + intPt[0].toFixed(1) + "," + intPt[1].toFixed(1) + "]");
                        continue;
                    }
                    var carvePts = carvePath.pathPoints;

                    // Make sure segment index is still valid
                    if (carveSegIdx >= carvePts.length - 1) continue;

                    var cSegStart = carvePts[carveSegIdx].anchor;
                    var cSegEnd = carvePts[carveSegIdx + 1].anchor;
                    var cDx = cSegEnd[0] - cSegStart[0];
                    var cDy = cSegEnd[1] - cSegStart[1];
                    var cLen = Math.sqrt(cDx * cDx + cDy * cDy);
                    if (cLen < 0.01) continue;

                    // Calculate cut points centered on intersection
                    var cDirX = cDx / cLen;
                    var cDirY = cDy / cLen;
                    var cutBefore = [intPt[0] - AUTO_CARVE_HALF_WIDTH * cDirX, intPt[1] - AUTO_CARVE_HALF_WIDTH * cDirY];
                    var cutAfter = [intPt[0] + AUTO_CARVE_HALF_WIDTH * cDirX, intPt[1] + AUTO_CARVE_HALF_WIDTH * cDirY];

                    addDebug("[AUTO-CARVE] Creating carve-out at [" + intPt[0].toFixed(1) + "," + intPt[1].toFixed(1) + "] - gap from [" + cutBefore[0].toFixed(1) + "," + cutBefore[1].toFixed(1) + "] to [" + cutAfter[0].toFixed(1) + "," + cutAfter[1].toFixed(1) + "] (" + (autoInt.isSelfIntersection ? "SELF-INTERSECTION" : "CROSS-PATH") + ")");

                    // Save the deleted segment for recovery/audit purposes
                    saveDeletedSegment(cutBefore, cutAfter, acColorSrc.layer);

                    // Store original parent for later use (compounding requires same parent)
                    var carveParent = null;
                    try {
                        carveParent = carvePath.parent;
                    } catch (eGetParent) {
                        addDebug("[AUTO-CARVE] Could not get parent: " + eGetParent);
                    }

                    // Create first half (start of path to cutBefore)
                    var autoFirstHalf = null;
                    var autoSecondHalf = null;

                    try {
                        autoFirstHalf = carvePath.duplicate();
                    } catch (eDup1) {
                        addDebug("[AUTO-CARVE] Failed to duplicate for first half: " + eDup1);
                        continue;
                    }

                    // Safely remove points from first half
                    try {
                        var autoFirstPts = autoFirstHalf.pathPoints;
                        var firstPtCount = autoFirstPts.length;
                        // Remove points after the cut segment (backwards to preserve indices)
                        for (var afDelIdx = firstPtCount - 1; afDelIdx > carveSegIdx; afDelIdx--) {
                            try {
                                autoFirstPts[afDelIdx].remove();
                            } catch (ePtRem1) {
                                addDebug("[AUTO-CARVE] Warning: Could not remove point " + afDelIdx + " from first half");
                            }
                        }
                        // Add new endpoint at cut position
                        var autoNewEnd = autoFirstHalf.pathPoints.add();
                        autoNewEnd.anchor = cutBefore;
                        autoNewEnd.leftDirection = cutBefore;
                        autoNewEnd.rightDirection = cutBefore;
                    } catch (eFirst) {
                        addDebug("[AUTO-CARVE] Error creating first half: " + eFirst);
                        try { if (autoFirstHalf) autoFirstHalf.remove(); } catch (eRem1) { }
                        continue;
                    }

                    // Create second half (cutAfter to end of path)
                    try {
                        autoSecondHalf = carvePath.duplicate();
                    } catch (eDup2) {
                        addDebug("[AUTO-CARVE] Failed to duplicate for second half: " + eDup2);
                        try { if (autoFirstHalf) autoFirstHalf.remove(); } catch (eRem2) { }
                        continue;
                    }

                    // Safely modify second half
                    try {
                        var autoSecondPts = autoSecondHalf.pathPoints;
                        // Modify the cut point to become the new start
                        autoSecondPts[carveSegIdx].anchor = cutAfter;
                        autoSecondPts[carveSegIdx].leftDirection = cutAfter;
                        autoSecondPts[carveSegIdx].rightDirection = cutAfter;
                        // Remove points before the cut segment (backwards to preserve indices)
                        for (var asDelIdx = carveSegIdx - 1; asDelIdx >= 0; asDelIdx--) {
                            try {
                                autoSecondPts[asDelIdx].remove();
                            } catch (ePtRem2) {
                                addDebug("[AUTO-CARVE] Warning: Could not remove point " + asDelIdx + " from second half");
                            }
                        }
                    } catch (eSecond) {
                        addDebug("[AUTO-CARVE] Error creating second half: " + eSecond);
                        try { if (autoFirstHalf) autoFirstHalf.remove(); } catch (eRem3) { }
                        try { if (autoSecondHalf) autoSecondHalf.remove(); } catch (eRem4) { }
                        continue;
                    }

                    // Validate both halves were created successfully
                    var firstValid = false, secondValid = false;
                    try {
                        if (autoFirstHalf && autoFirstHalf.pathPoints && autoFirstHalf.pathPoints.length >= 2) firstValid = true;
                    } catch (eVal1) { }
                    try {
                        if (autoSecondHalf && autoSecondHalf.pathPoints && autoSecondHalf.pathPoints.length >= 2) secondValid = true;
                    } catch (eVal2) { }

                    if (!firstValid || !secondValid) {
                        addDebug("[AUTO-CARVE] Carve failed - first valid: " + firstValid + ", second valid: " + secondValid);
                        try { if (autoFirstHalf) autoFirstHalf.remove(); } catch (eRem5) { }
                        try { if (autoSecondHalf) autoSecondHalf.remove(); } catch (eRem6) { }
                        continue;
                    }

                    try {
                        autoFirstHalf.selected = true;
                        autoSecondHalf.selected = true;
                    } catch (eSel) { }

                    // CRITICAL: Track that these carved halves should NOT connect via T-junction
                    // to the crossing path that caused the carve (they're crossovers, not connections)
                    // NOTE: For self-intersections, otherPath is the same as pathToCarve, so we skip blocking
                    var crossingPath = autoInt.otherPath;
                    if (crossingPath && !autoInt.isSelfIntersection) {
                        try {
                            // Store endpoint coordinates for reliable comparison later
                            // (path references may not compare correctly in ExtendScript)
                            var crossPts = crossingPath.pathPoints;
                            var crossEndpoints = null;
                            if (crossPts && crossPts.length >= 2) {
                                crossEndpoints = {
                                    start: [crossPts[0].anchor[0], crossPts[0].anchor[1]],
                                    end: [crossPts[crossPts.length - 1].anchor[0], crossPts[crossPts.length - 1].anchor[1]]
                                };
                                addDebug("[AUTO-CARVE-BLOCK] crossingPath endpoints: [" + crossEndpoints.start[0].toFixed(1) + "," + crossEndpoints.start[1].toFixed(1) + "] to [" + crossEndpoints.end[0].toFixed(1) + "," + crossEndpoints.end[1].toFixed(1) + "]");
                                addDebug("[AUTO-CARVE-BLOCK] pathToCarve endpoints: [" + autoInt.pathToCarve.pathPoints[0].anchor[0].toFixed(1) + "," + autoInt.pathToCarve.pathPoints[0].anchor[1].toFixed(1) + "] to [" + autoInt.pathToCarve.pathPoints[autoInt.pathToCarve.pathPoints.length-1].anchor[0].toFixed(1) + "," + autoInt.pathToCarve.pathPoints[autoInt.pathToCarve.pathPoints.length-1].anchor[1].toFixed(1) + "]");
                            }
                            var firstPts = autoFirstHalf.pathPoints;
                            var firstEndpoints = null;
                            if (firstPts && firstPts.length >= 2) {
                                firstEndpoints = {
                                    start: [firstPts[0].anchor[0], firstPts[0].anchor[1]],
                                    end: [firstPts[firstPts.length - 1].anchor[0], firstPts[firstPts.length - 1].anchor[1]]
                                };
                            }
                            var secondPts = autoSecondHalf.pathPoints;
                            var secondEndpoints = null;
                            if (secondPts && secondPts.length >= 2) {
                                secondEndpoints = {
                                    start: [secondPts[0].anchor[0], secondPts[0].anchor[1]],
                                    end: [secondPts[secondPts.length - 1].anchor[0], secondPts[secondPts.length - 1].anchor[1]]
                                };
                            }
                            CARVE_BLOCKED_CONNECTIONS.push({
                                carvedPath: autoFirstHalf,
                                carvedEndpoints: firstEndpoints,
                                crossingPath: crossingPath,
                                crossEndpoints: crossEndpoints
                            });
                            CARVE_BLOCKED_CONNECTIONS.push({
                                carvedPath: autoSecondHalf,
                                carvedEndpoints: secondEndpoints,
                                crossingPath: crossingPath,
                                crossEndpoints: crossEndpoints
                            });
                            addDebug("[AUTO-CARVE] Blocked T-junction connections between carved halves and crossing path");
                        } catch (eBlock) { }
                    } else if (autoInt.isSelfIntersection) {
                        addDebug("[AUTO-CARVE] Skipping T-junction blocking for self-intersection (otherPath = same path)");
                    }

                    // Track split pair for FORCED connection during compounding
                    // (siblings must stay together in the same compound path)
                    try {
                        AUTO_CARVE_SPLIT_PAIRS.push({
                            pathA: autoFirstHalf,
                            pathB: autoSecondHalf
                        });
                        addDebug("[AUTO-CARVE] Tracked split pair for forced sibling connection");
                    } catch (eSplitPair) { }

                    // Create ignore anchors at cut points
                    try {
                        var autoIgnLayer = null;
                        try { autoIgnLayer = doc.layers.getByName("Ignored"); } catch (eNoIgn) { }
                        if (!autoIgnLayer) {
                            autoIgnLayer = doc.layers.add();
                            autoIgnLayer.name = "Ignored";
                        }
                        // Unlock the layer AND make it visible if needed
                        if (autoIgnLayer) {
                            try {
                                if (autoIgnLayer.locked) {
                                    autoIgnLayer.locked = false;
                                    addDebug("[AUTO-CARVE] Unlocked Ignored layer");
                                }
                                if (!autoIgnLayer.visible) {
                                    autoIgnLayer.visible = true;
                                    addDebug("[AUTO-CARVE] Made Ignored layer visible");
                                }
                                // Also check parent containers - unlock them too if locked (Layers or GroupItems)
                                var parentContainer = autoIgnLayer.parent;
                                while (parentContainer) {
                                    var pType = parentContainer.typename;
                                    if (pType === "Layer" || pType === "GroupItem") {
                                        try {
                                            if (parentContainer.locked) {
                                                parentContainer.locked = false;
                                                addDebug("[AUTO-CARVE] Unlocked parent " + pType + ": " + (parentContainer.name || "(unnamed)"));
                                            }
                                        } catch (ePLock) { }
                                        try {
                                            if (!parentContainer.visible) {
                                                parentContainer.visible = true;
                                                addDebug("[AUTO-CARVE] Made parent " + pType + " visible: " + (parentContainer.name || "(unnamed)"));
                                            }
                                        } catch (ePVis) { }
                                    }
                                    if (pType !== "Layer" && pType !== "GroupItem") break;
                                    parentContainer = parentContainer.parent;
                                }
                            } catch (eUnlock) {
                                addDebug("[AUTO-CARVE] Could not unlock/show layer: " + eUnlock);
                            }
                        }
                        if (autoIgnLayer) {
                            addDebug("[AUTO-CARVE] Layer state before add: locked=" + autoIgnLayer.locked + ", visible=" + autoIgnLayer.visible + ", parent=" + (autoIgnLayer.parent ? autoIgnLayer.parent.typename : "none"));
                            var autoIgn1 = autoIgnLayer.pathItems.add();
                            autoIgn1.setEntirePath([[cutBefore[0], cutBefore[1]]]);
                            autoIgn1.filled = false;
                            autoIgn1.stroked = false;
                            addDebug("[AUTO-CARVE] Placed ignore anchor at [" + cutBefore[0].toFixed(1) + "," + cutBefore[1].toFixed(1) + "]");
                            // Add to ignoredAnchors array so component placement skips this position
                            ignoredAnchors.push([cutBefore[0], cutBefore[1]]);

                            var autoIgn2 = autoIgnLayer.pathItems.add();
                            autoIgn2.setEntirePath([[cutAfter[0], cutAfter[1]]]);
                            autoIgn2.filled = false;
                            autoIgn2.stroked = false;
                            addDebug("[AUTO-CARVE] Placed ignore anchor at [" + cutAfter[0].toFixed(1) + "," + cutAfter[1].toFixed(1) + "]");
                            // Add to ignoredAnchors array so component placement skips this position
                            ignoredAnchors.push([cutAfter[0], cutAfter[1]]);

                            addDebug("[AUTO-CARVE] Added carve-out positions to ignoredAnchors array (total: " + ignoredAnchors.length + ")");
                        }
                    } catch (eAutoIgnore) {
                        addDebug("[AUTO-CARVE] Warning: Could not create ignore anchors: " + eAutoIgnore + " (layer locked=" + (autoIgnLayer ? autoIgnLayer.locked : "null") + ", visible=" + (autoIgnLayer ? autoIgnLayer.visible : "null") + ")");
                    }

                    // Compound the two halves together
                    // BUT skip if the original path was already part of a compound (to avoid "already part of compound" error)
                    var autoIsCompoundChild = false;
                    try {
                        if (carvePath.parent && carvePath.parent.typename === "CompoundPathItem") {
                            autoIsCompoundChild = true;
                            addDebug("[AUTO-CARVE] Original path was part of compound - skipping re-compounding");
                        }
                    } catch (eAutoCheckParent) { }

                    // Check if both halves have the same parent (required for compounding)
                    var canCompound = !autoIsCompoundChild;
                    var firstParent = null, secondParent = null;
                    try {
                        firstParent = autoFirstHalf.parent;
                        secondParent = autoSecondHalf.parent;
                        if (firstParent !== secondParent) {
                            addDebug("[AUTO-CARVE] Halves in different parents - first: " + (firstParent ? firstParent.typename : "null") + ", second: " + (secondParent ? secondParent.typename : "null"));
                            // Try to move second half to same parent as first
                            if (firstParent && firstParent.typename === "Layer") {
                                try {
                                    autoSecondHalf.move(firstParent, ElementPlacement.PLACEATEND);
                                    addDebug("[AUTO-CARVE] Moved second half to match first half's parent layer");
                                } catch (eMoveToParent) {
                                    addDebug("[AUTO-CARVE] Could not move to same parent: " + eMoveToParent);
                                    canCompound = false;
                                }
                            } else if (firstParent && firstParent.typename === "GroupItem") {
                                // If parent is a group, try moving to the same layer as the group
                                try {
                                    var groupParentLayer = firstParent.parent;
                                    while (groupParentLayer && groupParentLayer.typename !== "Layer") {
                                        groupParentLayer = groupParentLayer.parent;
                                    }
                                    if (groupParentLayer) {
                                        autoFirstHalf.move(groupParentLayer, ElementPlacement.PLACEATEND);
                                        autoSecondHalf.move(groupParentLayer, ElementPlacement.PLACEATEND);
                                        addDebug("[AUTO-CARVE] Moved both halves to parent layer of group");
                                    }
                                } catch (eMoveFromGroup) {
                                    addDebug("[AUTO-CARVE] Could not move from group: " + eMoveFromGroup);
                                    canCompound = false;
                                }
                            } else {
                                canCompound = false;
                            }
                        }
                    } catch (eCheckParents) {
                        addDebug("[AUTO-CARVE] Error checking parents: " + eCheckParents);
                        canCompound = false;
                    }

                    // Try to create compound path, fall back to separate paths on failure
                    var compoundSuccess = false;
                    if (canCompound) {
                        try {
                            // Validate both halves are still valid before attempting compound
                            var halfAValid = false, halfBValid = false;
                            try { halfAValid = autoFirstHalf && autoFirstHalf.pathPoints && autoFirstHalf.pathPoints.length >= 2; } catch (eValA) { }
                            try { halfBValid = autoSecondHalf && autoSecondHalf.pathPoints && autoSecondHalf.pathPoints.length >= 2; } catch (eValB) { }

                            if (halfAValid && halfBValid) {
                                // Deselect all, then select just the two halves
                                doc.selection = null;
                                autoFirstHalf.selected = true;
                                autoSecondHalf.selected = true;

                                if (doc.selection.length === 2) {
                                    app.executeMenuCommand("compoundPath");

                                    // Capture the new compound path
                                    if (doc.selection.length === 1 && doc.selection[0].typename === "CompoundPathItem") {
                                        var autoNewCompound = doc.selection[0];
                                        autoNewCompoundPaths.push(autoNewCompound);
                                        CARVE_OUT_COMPOUNDS.push(autoNewCompound);
                                        SELECTED_PATHS.push(autoNewCompound);
                                        compoundSuccess = true;
                                        carveCount++;
                                    }
                                }
                            }
                        } catch (eCompound) {
                            // Compounding failed - will fall back below
                        }
                    }

                    // Fallback: if compounding failed, add the halves as separate paths
                    if (!compoundSuccess) {
                        try {
                            var addedA = false, addedB = false;
                            try { if (autoFirstHalf && autoFirstHalf.pathPoints && autoFirstHalf.pathPoints.length >= 2) { SELECTED_PATHS.push(autoFirstHalf); addedA = true; } } catch (eA) { }
                            try { if (autoSecondHalf && autoSecondHalf.pathPoints && autoSecondHalf.pathPoints.length >= 2) { SELECTED_PATHS.push(autoSecondHalf); addedB = true; } } catch (eB) { }
                            if (addedA || addedB) carveCount++;
                        } catch (eAddHalves) { }
                    }

                    // Mark original path for removal (use loop since ExtendScript lacks indexOf)
                    var alreadyMarked = false;
                    for (var amIdx = 0; amIdx < autoPathsToRemove.length; amIdx++) {
                        if (autoPathsToRemove[amIdx] === carvePath) {
                            alreadyMarked = true;
                            break;
                        }
                    }
                    if (!alreadyMarked) {
                        autoPathsToRemove.push(carvePath);
                    }
                } catch (eAutoCarve) { }
            }

            // Log if limit was reached
            if (carveCount >= MAX_CARVE_OUTS && autoIntersections.length > MAX_CARVE_OUTS) {
                addDebug("[AUTO-CARVE] WARNING: Hit carve limit of " + MAX_CARVE_OUTS + " (had " + autoIntersections.length + " total intersections)");
            }

            // Remove original paths that were carved
            for (var arIdx = 0; arIdx < autoPathsToRemove.length; arIdx++) {
                try {
                    var autoPathToRemove = autoPathsToRemove[arIdx];
                    // Remove from SELECTED_PATHS before deleting
                    for (var aspIdx = SELECTED_PATHS.length - 1; aspIdx >= 0; aspIdx--) {
                        try {
                            if (SELECTED_PATHS[aspIdx] === autoPathToRemove) {
                                SELECTED_PATHS.splice(aspIdx, 1);
                            }
                        } catch (eSpCheck) { }
                    }
                    autoPathToRemove.remove();
                } catch (eAutoRem) { }
            }

            if (autoIntersections.length > 0) {
                addDebug("[AUTO-CARVE] " + acColorSrc.name + ": Performed " + autoIntersections.length + " automatic carve-out(s), created " + autoNewCompoundPaths.length + " compound path(s)");
            } else {
                addDebug("[AUTO-CARVE] " + acColorSrc.name + ": No new intersections found");
            }
            } // End for loop over ductwork colors (acColorIdx)
        } // End of ENABLE_OVERLAP_CARVE else block

        // *** PLACE SQUARE REGISTERS AT INTERNAL ANCHORS WITH NO DIRECTION CHANGE ***
        // For internal anchors on blue paths that are NOT crossovers and have no direction change
        // (incoming/outgoing segments are collinear), place a square register
        updateProgress("Internal anchor registers...");
        addDebug("\n=== INTERNAL ANCHOR REGISTERS (NO DIRECTION CHANGE) ===");

        // Helper: Check if three points are collinear (no direction change at middle point)
        function isAnchorCollinear(prevPt, anchorPt, nextPt, tolerance) {
            // Calculate vectors from prev->anchor and anchor->next
            var v1x = anchorPt[0] - prevPt[0];
            var v1y = anchorPt[1] - prevPt[1];
            var v2x = nextPt[0] - anchorPt[0];
            var v2y = nextPt[1] - anchorPt[1];

            // Normalize vectors
            var len1 = Math.sqrt(v1x * v1x + v1y * v1y);
            var len2 = Math.sqrt(v2x * v2x + v2y * v2y);

            if (len1 < 0.001 || len2 < 0.001) return false;

            v1x /= len1; v1y /= len1;
            v2x /= len2; v2y /= len2;

            // Dot product - if close to 1, vectors are in same direction (collinear continuation)
            var dot = v1x * v2x + v1y * v2y;

            // dot product of ~1 means same direction (path continues straight through anchor)
            return dot > (1 - tolerance);
        }

        // Process internal anchors for all ductwork colors
        addDebug("[INTERNAL-REGISTERS] Processing all ductwork colors for internal anchor registers");

        for (var irColorIdx = 0; irColorIdx < ALL_DUCTWORK_SOURCES.length; irColorIdx++) {
            var irColorSrc = ALL_DUCTWORK_SOURCES[irColorIdx];
            var irRegisterLayerName = getRegisterLayerForDuctwork(irColorSrc.name);
            var colorPathsForRegisters = getPathsOnLayerSelected(irColorSrc.layer) || [];
            if (colorPathsForRegisters.length === 0) {
                addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": No paths, skipping");
                continue;
            }
            addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": Processing " + colorPathsForRegisters.length + " paths");

        // Build set of crossover anchor indices per path (using path reference as key via index)
        // EARLY_CROSSOVER_SEGMENTS contains: { path, pathIdx, segmentIdx, ... }
        var crossoverAnchorsByPath = {};
        if (typeof EARLY_CROSSOVER_SEGMENTS !== 'undefined' && EARLY_CROSSOVER_SEGMENTS.length > 0) {
            for (var xoRegIdx = 0; xoRegIdx < EARLY_CROSSOVER_SEGMENTS.length; xoRegIdx++) {
                var xoReg = EARLY_CROSSOVER_SEGMENTS[xoRegIdx];
                // Mark both endpoints of the crossover segment as crossover anchors
                // Store the crossover segment midpoint for checking distance
                var xoMidX = (xoReg.segStart[0] + xoReg.segEnd[0]) / 2;
                var xoMidY = (xoReg.segStart[1] + xoReg.segEnd[1]) / 2;
                if (!crossoverAnchorsByPath[xoReg.pathIdx]) {
                    crossoverAnchorsByPath[xoReg.pathIdx] = [];
                }
                crossoverAnchorsByPath[xoReg.pathIdx].push({
                    segIdx: xoReg.segmentIdx,
                    midpoint: [xoMidX, xoMidY]
                });
            }
            addDebug("[INTERNAL-REGISTERS] Found " + EARLY_CROSSOVER_SEGMENTS.length + " crossover segment(s) to exclude");
        }

        // Refresh existing register points after endpoint registers were placed
        var updatedRegisterPoints = getExistingAnchorPoints([
            "Units", "Square Registers", "Exhaust Registers",
            "Thermostats", "Rectangular Registers"
        ]);

        var irRegisterLayer = getOrCreateLayer(irRegisterLayerName);
        var internalRegistersPlaced = 0;
        var internalRegistersSkipped = 0;

        // Determine component file based on register type
        var INTERNAL_REG_PATH = "E:/Work/Work/Floorplans/Ductwork Assets/";
        var irComponentFileName = (irRegisterLayerName === "Square Registers") ? "Square Register.ai" : "Exhaust Register.ai";
        var irComponentFile = new File(INTERNAL_REG_PATH + irComponentFileName);

        for (var irPathIdx = 0; irPathIdx < colorPathsForRegisters.length; irPathIdx++) {
            var irPath = colorPathsForRegisters[irPathIdx];
            try {
                // Validity check - skip if path was removed during carve-out
                if (!irPath || !irPath.typename) continue;
                var irPts = null;
                try { irPts = irPath.pathPoints; } catch (eGetPts) { continue; }
                if (!irPts || irPts.length < 3) continue; // Need at least 3 points for internal anchors

                // Get crossover info for this path (check by comparing anchor positions)
                var pathCrossovers = [];
                for (var pcKey in crossoverAnchorsByPath) {
                    if (crossoverAnchorsByPath.hasOwnProperty(pcKey)) {
                        var crossovers = crossoverAnchorsByPath[pcKey];
                        for (var pcIdx = 0; pcIdx < crossovers.length; pcIdx++) {
                            pathCrossovers.push(crossovers[pcIdx].midpoint);
                        }
                    }
                }

                // Check each internal anchor (not first or last point)
                for (var iaIdx = 1; iaIdx < irPts.length - 1; iaIdx++) {
                    var prevPt = irPts[iaIdx - 1].anchor;
                    var anchorPt = irPts[iaIdx].anchor;
                    var nextPt = irPts[iaIdx + 1].anchor;

                    // Check if this anchor is near a crossover midpoint
                    var isNearCrossover = false;
                    for (var ncIdx = 0; ncIdx < pathCrossovers.length; ncIdx++) {
                        var crossMid = pathCrossovers[ncIdx];
                        var distToCrossover = Math.sqrt(
                            Math.pow(anchorPt[0] - crossMid[0], 2) +
                            Math.pow(anchorPt[1] - crossMid[1], 2)
                        );
                        if (distToCrossover <= 15) { // Within 15pt of crossover midpoint
                            isNearCrossover = true;
                            break;
                        }
                    }

                    if (isNearCrossover) {
                        addDebug("[INTERNAL-REGISTERS] Skipping anchor at [" + anchorPt[0].toFixed(1) + "," + anchorPt[1].toFixed(1) + "] - near crossover");
                        internalRegistersSkipped++;
                        continue;
                    }

                    // Check if collinear (no direction change) - use tolerance of 0.02 (~8 degrees)
                    if (!isAnchorCollinear(prevPt, anchorPt, nextPt, 0.02)) {
                        continue; // Has direction change - don't place register
                    }

                    // Skip if this point is in ignored anchors
                    if (isPointIgnored(anchorPt, ignoredAnchors)) {
                        addDebug("[INTERNAL-REGISTERS] Skipping anchor at [" + anchorPt[0].toFixed(1) + "," + anchorPt[1].toFixed(1) + "] - ignored");
                        internalRegistersSkipped++;
                        continue;
                    }

                    // Skip if already has a register/unit nearby
                    if (isPointAlreadyPlaced(anchorPt, updatedRegisterPoints)) {
                        addDebug("[INTERNAL-REGISTERS] Skipping anchor at [" + anchorPt[0].toFixed(1) + "," + anchorPt[1].toFixed(1) + "] - already placed");
                        internalRegistersSkipped++;
                        continue;
                    }

                    // Place a register LINKED COMPONENT at this collinear internal anchor
                    addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": Placing " + irRegisterLayerName + " at internal anchor [" + anchorPt[0].toFixed(1) + "," + anchorPt[1].toFixed(1) + "] (path " + irPathIdx + ", anchor " + iaIdx + ")");

                    // Create anchor point for reference
                    createAnchorPoint(irRegisterLayer, anchorPt, null);

                    // Also directly place the linked register component
                    try {
                        if (irComponentFile.exists) {
                            var placed = irRegisterLayer.placedItems.add();
                            placed.file = irComponentFile;
                            try { placed.relink(irComponentFile); } catch (eRelink) { }
                            try { placed.update(); } catch (eUpdate) { }

                            // Center on anchor position
                            var bounds = placed.geometricBounds;
                            var w = bounds[2] - bounds[0];
                            var h = bounds[1] - bounds[3];
                            placed.position = [anchorPt[0] - w / 2, anchorPt[1] + h / 2];
                            placed.name = irRegisterLayerName.replace(" Registers", " Register") + " (Linked)";

                            // Apply default 50% scale
                            var DEFAULT_SCALE = 50;
                            placed.resize(DEFAULT_SCALE, DEFAULT_SCALE, true, true, true, true, DEFAULT_SCALE, Transformation.CENTER);

                            // Apply rotation to match ductwork angle (if not disabled)
                            if (!SKIP_REGISTER_ROTATION) {
                                var dxAngle = anchorPt[0] - prevPt[0];
                                var dyAngle = anchorPt[1] - prevPt[1];
                                var ductAngle = Math.atan2(dyAngle, dxAngle) * (180 / Math.PI);
                                placed.rotate(ductAngle, true, true, true, true, Transformation.CENTER);
                                // Store rotation metadata (same format as rotation slider)
                                setPlacedRotation(placed, normalizeAngle(ductAngle));
                                addDebug("[INTERNAL-REGISTERS] Applied rotation " + ductAngle.toFixed(1) + " deg to match ductwork");
                            }

                            // Re-center after scaling
                            bounds = placed.geometricBounds;
                            var cx = (bounds[0] + bounds[2]) / 2;
                            var cy = (bounds[1] + bounds[3]) / 2;
                            var dx = anchorPt[0] - cx;
                            var dy = anchorPt[1] - cy;
                            if (Math.abs(dx) > 0.01 || Math.abs(dy) > 0.01) {
                                placed.translate(dx, dy, true, true, true, true);
                            }

                            addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": Placed linked " + irRegisterLayerName + " component at [" + anchorPt[0].toFixed(1) + "," + anchorPt[1].toFixed(1) + "]");
                        } else {
                            addDebug("[INTERNAL-REGISTERS] WARNING: Component file not found: " + irComponentFile.fsName);
                        }
                    } catch (ePlaceReg) {
                        addDebug("[INTERNAL-REGISTERS] Error placing linked component: " + ePlaceReg);
                    }

                    internalRegistersPlaced++;

                    // Add to updated points to prevent duplicates
                    updatedRegisterPoints.push(anchorPt);
                }
            } catch (irErr) {
                addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": Error processing path " + irPathIdx + ": " + irErr);
            }
        }

        addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": Placed " + internalRegistersPlaced + " " + irRegisterLayerName + " at internal anchors with no direction change");
        addDebug("[INTERNAL-REGISTERS] " + irColorSrc.name + ": Skipped " + internalRegistersSkipped + " internal anchor(s)");
        } // End for loop over ductwork colors (irColorIdx) for internal registers

        // STEP 5: Create Thermostats from endpoints not near units, ignored points, or existing points
        updateProgress("Creating thermostats...");
        // *** Refresh existing anchor points after Registers creation ***
        var existingAnchorsForThermostats = getExistingAnchorPoints(["Units", "Square Registers", "Exhaust Registers", "Thermostats"]);

        // Correct parameter order: thermostatLinesLayer, unitsLayer, thermostatsLayer, ignoredAnchors, existingAnchors
        createDistantThermostats("Thermostat Lines", "Units", "Thermostats", ignoredAnchors, existingAnchorsForThermostats);

        // STEP 6: Compound connected paths PER LAYER within selection
        // Both Normal AND Emory mode use compounding to create proper centerlines
        // The Double Ductwork (rectangles/connectors) are generated separately AFTER compounding
        updateProgress("Compounding paths...");
        addDebug("\n=== COMPOUNDING CONNECTED PATHS ===");

        // Helper function to compare paths by endpoint coordinates (instead of object reference)
        // ExtendScript may return different wrapper objects for the same underlying path
        // Only compares ENDPOINTS, not internal point count (paths may have different point counts)
        function pathsAreSameByEndpoints(pathA, pathB) {
            if (!pathA || !pathB) return false;
            try {
                var ptsA = pathA.pathPoints;
                var ptsB = pathB.pathPoints;
                if (!ptsA || !ptsB) return false;
                if (ptsA.length < 2 || ptsB.length < 2) return false;

                // Compare first and last endpoints with small tolerance
                var tol = 0.5; // Half a point tolerance
                var a0 = ptsA[0].anchor;
                var aL = ptsA[ptsA.length - 1].anchor;
                var b0 = ptsB[0].anchor;
                var bL = ptsB[ptsB.length - 1].anchor;

                // Check if endpoints match (in either direction)
                var match1 = (Math.abs(a0[0] - b0[0]) < tol && Math.abs(a0[1] - b0[1]) < tol &&
                              Math.abs(aL[0] - bL[0]) < tol && Math.abs(aL[1] - bL[1]) < tol);
                var match2 = (Math.abs(a0[0] - bL[0]) < tol && Math.abs(a0[1] - bL[1]) < tol &&
                              Math.abs(aL[0] - b0[0]) < tol && Math.abs(aL[1] - b0[1]) < tol);
                return match1 || match2;
            } catch (e) {
                return false;
            }
        }

        var baseCompoundLayers = ["Green Ductwork", "Light Green Ductwork", "Blue Ductwork", "Orange Ductwork", "Light Orange Ductwork"];
        var layersToProcess = [];
        for (var baseIdx = 0; baseIdx < baseCompoundLayers.length; baseIdx++) {
            var baseLayerName = baseCompoundLayers[baseIdx];
            var resolvedLayerName = resolveDuctworkLayerForProcessing(baseLayerName);
            if (arrayIndexOf(layersToProcess, resolvedLayerName) === -1) layersToProcess.push(resolvedLayerName);
            if (resolvedLayerName !== baseLayerName && arrayIndexOf(layersToProcess, baseLayerName) === -1) {
                layersToProcess.push(baseLayerName);
            }
        }

        // Track compound paths created during compounding for styling
        var COMPOUND_PATHS_TO_STYLE = [];

        // Track crossover segment coordinates for post-processing (ignore anchors + segment deletion)
        var ALL_CROSSOVER_SEGMENTS = [];

        for (var layerIdx = 0; layerIdx < layersToProcess.length; layerIdx++) {
            var layerName = layersToProcess[layerIdx];
            addDebug("[COMPOUND] Processing layer: " + layerName);
            // Only process selected paths on this layer
            var layerPaths = filterPathsToProcessable(getPathsOnLayerSelected(layerName));
            addDebug("[COMPOUND] " + layerName + " has " + layerPaths.length + " selected paths");

            // Only compound open paths (centerlines), not closed paths
            // In Emory mode, this excludes the Double Ductwork rectangles/connectors from compounding
            var filteredPaths = [];
            for (var fp = 0; fp < layerPaths.length; fp++) {
                if (!layerPaths[fp].closed) filteredPaths.push(layerPaths[fp]);
            }
            addDebug("[COMPOUND] " + layerName + " after filter: " + filteredPaths.length + " open paths");

            // Build list of forced connections for paths in the same carve-out compound
            // This ensures carved halves stay together when compounded with other paths
            var carveOutForcedConnections = [];

            // FIRST: Add forced connections from AUTO_CARVE_SPLIT_PAIRS (direct tracking)
            // This is the most reliable method - uses actual path references stored at carve time
            if (typeof AUTO_CARVE_SPLIT_PAIRS !== 'undefined' && AUTO_CARVE_SPLIT_PAIRS.length > 0) {
                for (var acspIdx = 0; acspIdx < AUTO_CARVE_SPLIT_PAIRS.length; acspIdx++) {
                    try {
                        var acsp = AUTO_CARVE_SPLIT_PAIRS[acspIdx];
                        if (acsp && acsp.pathA && acsp.pathB) {
                            carveOutForcedConnections.push([acsp.pathA, acsp.pathB]);
                            addDebug("[COMPOUND] Forced connection from AUTO_CARVE_SPLIT_PAIRS: pathA <-> pathB");
                        }
                    } catch (eAcsp) { }
                }
                if (carveOutForcedConnections.length > 0) {
                    addDebug("[COMPOUND] Added " + carveOutForcedConnections.length + " forced connections from AUTO_CARVE_SPLIT_PAIRS");
                }
            }

            if (typeof CARVE_OUT_COMPOUNDS !== 'undefined' && CARVE_OUT_COMPOUNDS.length > 0) {
                for (var cocIdx = 0; cocIdx < CARVE_OUT_COMPOUNDS.length; cocIdx++) {
                    try {
                        var coc = CARVE_OUT_COMPOUNDS[cocIdx];
                        if (!coc || coc.typename !== 'CompoundPathItem') continue;
                        var cocChildren = [];
                        for (var cocChild = 0; cocChild < coc.pathItems.length; cocChild++) {
                            var childPath = coc.pathItems[cocChild];
                            // Check if this child is in filteredPaths
                            for (var fpCheck = 0; fpCheck < filteredPaths.length; fpCheck++) {
                                if (filteredPaths[fpCheck] === childPath) {
                                    cocChildren.push(childPath);
                                    break;
                                }
                            }
                        }
                        // Add forced connections between all children of this compound
                        for (var cocA = 0; cocA < cocChildren.length; cocA++) {
                            for (var cocB = cocA + 1; cocB < cocChildren.length; cocB++) {
                                carveOutForcedConnections.push([cocChildren[cocA], cocChildren[cocB]]);
                            }
                        }
                    } catch (eCoc) { }
                }
                if (carveOutForcedConnections.length > 0) {
                    addDebug("[COMPOUND] Added " + carveOutForcedConnections.length + " forced connections for carve-out compound siblings");
                }
            }

            // PERSISTENT CARVE-OUT SIBLINGS: On reruns, CARVE_OUT_COMPOUNDS is empty but we need to
            // detect existing compound paths that contain carve-out siblings (paths with endpoints
            // near carve-out gap pairs). Force connect children of such compounds.
            if (carveOutForcedConnections.length === 0 && ignoredAnchors && ignoredAnchors.length >= 2) {
                var CARVE_GAP_MIN = 7;
                var CARVE_GAP_MAX = 10;
                var CARVE_EP_TOL = 5;

                // Find carve-out gap pairs (ignored anchor pairs ~8.5pt apart)
                var carveGapPairs = [];
                for (var gp1 = 0; gp1 < ignoredAnchors.length - 1; gp1++) {
                    for (var gp2 = gp1 + 1; gp2 < ignoredAnchors.length; gp2++) {
                        var gpDist = Math.sqrt(Math.pow(ignoredAnchors[gp1][0] - ignoredAnchors[gp2][0], 2) + Math.pow(ignoredAnchors[gp1][1] - ignoredAnchors[gp2][1], 2));
                        if (gpDist >= CARVE_GAP_MIN && gpDist <= CARVE_GAP_MAX) {
                            carveGapPairs.push([ignoredAnchors[gp1], ignoredAnchors[gp2]]);
                        }
                    }
                }

                if (carveGapPairs.length > 0) {
                    addDebug("[COMPOUND] Found " + carveGapPairs.length + " carve-out gap pair(s) for persistent sibling detection");

                    // Check each existing compound path on this layer
                    try {
                        var layer = doc.layers.getByName(layerName);
                        for (var cpIdx = 0; cpIdx < layer.compoundPathItems.length; cpIdx++) {
                            try {
                                var cp = layer.compoundPathItems[cpIdx];
                                if (!cp || cp.pathItems.length < 2) continue;

                                // Get children that are in filteredPaths
                                var cpChildren = [];
                                for (var cpChild = 0; cpChild < cp.pathItems.length; cpChild++) {
                                    var childPath = cp.pathItems[cpChild];
                                    for (var fpIdx = 0; fpIdx < filteredPaths.length; fpIdx++) {
                                        if (filteredPaths[fpIdx] === childPath) {
                                            cpChildren.push(childPath);
                                            break;
                                        }
                                    }
                                }

                                // Check if children have endpoints near carve-out gap pairs
                                for (var cgpIdx = 0; cgpIdx < carveGapPairs.length; cgpIdx++) {
                                    var gapPt1 = carveGapPairs[cgpIdx][0];
                                    var gapPt2 = carveGapPairs[cgpIdx][1];

                                    // Find children with endpoints near each gap point
                                    var childNearPt1 = null, childNearPt2 = null;
                                    for (var chIdx = 0; chIdx < cpChildren.length; chIdx++) {
                                        var chPath = cpChildren[chIdx];
                                        var chPts = chPath.pathPoints;
                                        if (!chPts || chPts.length < 2) continue;

                                        var ep1 = [chPts[0].anchor[0], chPts[0].anchor[1]];
                                        var ep2 = [chPts[chPts.length - 1].anchor[0], chPts[chPts.length - 1].anchor[1]];

                                        var d1a = Math.sqrt(Math.pow(ep1[0] - gapPt1[0], 2) + Math.pow(ep1[1] - gapPt1[1], 2));
                                        var d1b = Math.sqrt(Math.pow(ep2[0] - gapPt1[0], 2) + Math.pow(ep2[1] - gapPt1[1], 2));
                                        var d2a = Math.sqrt(Math.pow(ep1[0] - gapPt2[0], 2) + Math.pow(ep1[1] - gapPt2[1], 2));
                                        var d2b = Math.sqrt(Math.pow(ep2[0] - gapPt2[0], 2) + Math.pow(ep2[1] - gapPt2[1], 2));

                                        if (d1a <= CARVE_EP_TOL || d1b <= CARVE_EP_TOL) {
                                            childNearPt1 = chPath;
                                        }
                                        if (d2a <= CARVE_EP_TOL || d2b <= CARVE_EP_TOL) {
                                            childNearPt2 = chPath;
                                        }
                                    }

                                    // If two different children are near the gap pair endpoints, force connect them
                                    if (childNearPt1 && childNearPt2 && childNearPt1 !== childNearPt2) {
                                        carveOutForcedConnections.push([childNearPt1, childNearPt2]);
                                        addDebug("[COMPOUND] Detected persistent carve-out siblings, forcing connection");
                                    }
                                }
                            } catch (eCpCheck) { }
                        }
                    } catch (eLayerCheck) { }
                }
            }

            layerPaths = filteredPaths;

            // CROSSOVER DETECTION: Find small internal segments (5-17pt) where another path intersects
            // These represent crossover points between separate ductwork runs that should NOT be merged
            var crossoverInfo = [];
            if (layerName.toLowerCase().indexOf("blue") !== -1 && layerPaths.length > 1) {
                var SMALL_SEG_MIN = 5;
                var SMALL_SEG_MAX = 17;
                var INTERSECT_DIST = 5; // Max distance to consider an intersection

                // === SPATIAL HASHING FOR CROSSOVER DETECTION O(n) PERFORMANCE ===
                var XO_CELL_SIZE = 100;
                var xoSpatialGrid = {};
                var xoPathToCells = [];

                // Build spatial index
                for (var xoBuildIdx = 0; xoBuildIdx < layerPaths.length; xoBuildIdx++) {
                    var xoBuildPath = layerPaths[xoBuildIdx];
                    var xoBuildPts = xoBuildPath ? xoBuildPath.pathPoints : null;
                    if (!xoBuildPts || xoBuildPts.length === 0) {
                        xoPathToCells.push([]);
                        continue;
                    }
                    var xoMinX = Infinity, xoMinY = Infinity, xoMaxX = -Infinity, xoMaxY = -Infinity;
                    for (var xoBp = 0; xoBp < xoBuildPts.length; xoBp++) {
                        var xoAnchor = xoBuildPts[xoBp].anchor;
                        if (xoAnchor[0] < xoMinX) xoMinX = xoAnchor[0];
                        if (xoAnchor[0] > xoMaxX) xoMaxX = xoAnchor[0];
                        if (xoAnchor[1] < xoMinY) xoMinY = xoAnchor[1];
                        if (xoAnchor[1] > xoMaxY) xoMaxY = xoAnchor[1];
                    }
                    var xoCellMinX = Math.floor(xoMinX / XO_CELL_SIZE);
                    var xoCellMaxX = Math.floor(xoMaxX / XO_CELL_SIZE);
                    var xoCellMinY = Math.floor(xoMinY / XO_CELL_SIZE);
                    var xoCellMaxY = Math.floor(xoMaxY / XO_CELL_SIZE);
                    var xoCells = [];
                    for (var xoCx = xoCellMinX; xoCx <= xoCellMaxX; xoCx++) {
                        for (var xoCy = xoCellMinY; xoCy <= xoCellMaxY; xoCy++) {
                            var xoCellKey = xoCx + "," + xoCy;
                            xoCells.push(xoCellKey);
                            if (!xoSpatialGrid[xoCellKey]) xoSpatialGrid[xoCellKey] = [];
                            xoSpatialGrid[xoCellKey].push(xoBuildIdx);
                        }
                    }
                    xoPathToCells.push(xoCells);
                }

                for (var pathIdx = 0; pathIdx < layerPaths.length; pathIdx++) {
                    var path = layerPaths[pathIdx];
                    var pts = path.pathPoints;
                    // UI yield with app.redraw
                    if (pathIdx % 10 === 0) {
                        // yieldToUI();
                    }

                    // Get candidate paths from shared cells
                    var xoCandidates = {};
                    var xoMyCells = xoPathToCells[pathIdx];
                    for (var xoMc = 0; xoMc < xoMyCells.length; xoMc++) {
                        var xoCellPaths = xoSpatialGrid[xoMyCells[xoMc]];
                        if (xoCellPaths) {
                            for (var xoCp = 0; xoCp < xoCellPaths.length; xoCp++) {
                                var xoCandIdx = xoCellPaths[xoCp];
                                if (xoCandIdx !== pathIdx) {
                                    xoCandidates[xoCandIdx] = true;
                                }
                            }
                        }
                    }

                    // Check each internal segment (not first or last segment)
                    for (var segIdx = 1; segIdx < pts.length - 2; segIdx++) {
                        var segStart = pts[segIdx].anchor;
                        var segEnd = pts[segIdx + 1].anchor;
                        var dx = segEnd[0] - segStart[0];
                        var dy = segEnd[1] - segStart[1];
                        var segLen = Math.sqrt(dx * dx + dy * dy);

                        if (segLen >= SMALL_SEG_MIN && segLen <= SMALL_SEG_MAX) {
                            // Found a small internal segment - check only candidate paths
                            for (var otherIdx in xoCandidates) {
                                if (!xoCandidates.hasOwnProperty(otherIdx)) continue;
                                otherIdx = parseInt(otherIdx);
                                var otherPath = layerPaths[otherIdx];
                                var otherPts = otherPath.pathPoints;
                                var foundIntersection = false;

                                // Check if any segment of otherPath intersects this small segment
                                for (var otherSegIdx = 0; otherSegIdx < otherPts.length - 1 && !foundIntersection; otherSegIdx++) {
                                    var oSegStart = otherPts[otherSegIdx].anchor;
                                    var oSegEnd = otherPts[otherSegIdx + 1].anchor;

                                    // Use segment intersection test (already exists in codebase)
                                    if (segmentsIntersect(
                                        segStart[0], segStart[1], segEnd[0], segEnd[1],
                                        oSegStart[0], oSegStart[1], oSegEnd[0], oSegEnd[1]
                                    )) {
                                        foundIntersection = true;
                                        addDebug("[XOVER] FOUND: P" + pathIdx + "S" + segIdx + " (" + segLen.toFixed(1) + "pt) crossed by P" + otherIdx + "S" + otherSegIdx);
                                        var xoverData = {
                                            pathWithSmallSeg: path,
                                            pathWithSmallSegIdx: pathIdx,
                                            intersectingPath: otherPath,
                                            intersectingPathIdx: otherIdx,
                                            segmentIdx: segIdx,
                                            segStart: [segStart[0], segStart[1]],
                                            segEnd: [segEnd[0], segEnd[1]],
                                            layerName: layerName
                                        };
                                        crossoverInfo.push(xoverData);
                                        ALL_CROSSOVER_SEGMENTS.push(xoverData);
                                    }
                                }
                            }
                        }
                    }
                }
                addDebug("[XOVER] " + layerPaths.length + " paths, found " + crossoverInfo.length + " crossover(s)");

                // FORCE ORTHOGONALIZATION for paths with crossover segments BEFORE splitting
                // This ensures the segments are properly aligned before we split and compound
                if (crossoverInfo.length > 0) {
                    addDebug("[XOVER-ORTHO] Forcing orthogonalization on " + crossoverInfo.length + " crossover path(s)");

                    // Temporarily disable skip-final so ALL segments get orthogonalized
                    var savedSkipFinal = SKIP_FINAL_REGISTER_ORTHO;
                    SKIP_FINAL_REGISTER_ORTHO = false;
                    addDebug("[XOVER-ORTHO] Temporarily disabled SKIP_FINAL_REGISTER_ORTHO (was " + savedSkipFinal + ")");

                    var pathsToOrtho = [];
                    for (var orthoIdx = 0; orthoIdx < crossoverInfo.length; orthoIdx++) {
                        var orthoPath = crossoverInfo[orthoIdx].pathWithSmallSeg;
                        // Clear ortho lock so it can be re-processed
                        try { clearOrthoLock(orthoPath); } catch (e) { }
                        // Add to list if not already included
                        var alreadyIn = false;
                        for (var chk = 0; chk < pathsToOrtho.length; chk++) {
                            if (pathsToOrtho[chk] === orthoPath) { alreadyIn = true; break; }
                        }
                        if (!alreadyIn) pathsToOrtho.push(orthoPath);
                    }
                    // Run orthogonalization on these paths
                    var orthoConnections = collectEndpointConnections(pathsToOrtho, RECONNECT_CAPTURE_DIST);
                    for (var orthoRunIdx = 0; orthoRunIdx < pathsToOrtho.length; orthoRunIdx++) {
                        addDebug("[XOVER-ORTHO] Orthogonalizing crossover path " + orthoRunIdx);
                        try {
                            orthogonalizePath(pathsToOrtho[orthoRunIdx], orthoConnections.pairs);
                        } catch (eOrtho) {
                            addDebug("[XOVER-ORTHO] ERROR: " + eOrtho);
                        }
                    }

                    // Restore skip-final setting
                    SKIP_FINAL_REGISTER_ORTHO = savedSkipFinal;
                    addDebug("[XOVER-ORTHO] Restored SKIP_FINAL_REGISTER_ORTHO to " + savedSkipFinal);
                    addDebug("[XOVER-ORTHO] Done orthogonalizing crossover paths");
                }

                // CROSSOVER SPLITTING: Split paths at crossover segments BEFORE compounding
                // This must happen early so the new split paths are included in layerPaths
                var splitPathPairs = []; // Track pairs of split paths that should be connected
                var splitPathCrossoverBlocks = []; // Track split halves that should NOT connect to the intersecting path
                for (var xoSplitIdx = 0; xoSplitIdx < crossoverInfo.length; xoSplitIdx++) {
                    var xoSplit = crossoverInfo[xoSplitIdx];
                    var targetPath = xoSplit.pathWithSmallSeg;
                    var segIdx = xoSplit.segmentIdx;

                    try {
                        if (targetPath && targetPath.pathPoints && targetPath.pathPoints.length > segIdx + 1) {
                            var pts = targetPath.pathPoints;
                            var numPoints = pts.length;

                            if (segIdx > 0 && segIdx < numPoints - 2) {
                                addDebug("[XOVER-SPLIT] Splitting P" + xoSplit.pathWithSmallSegIdx + " at segment " + segIdx + " (has " + numPoints + " points)");

                                // Create duplicate for second half
                                var dupPath = targetPath.duplicate();
                                var dupPts = dupPath.pathPoints;

                                // From original: keep [0..segIdx], remove rest
                                for (var delIdx = numPoints - 1; delIdx > segIdx; delIdx--) {
                                    pts[delIdx].remove();
                                }

                                // From duplicate: keep [segIdx+1..end], remove start
                                for (var delIdx2 = 0; delIdx2 <= segIdx; delIdx2++) {
                                    dupPts[0].remove();
                                }

                                addDebug("[XOVER-SPLIT] Original now " + targetPath.pathPoints.length + " pts, new split has " + dupPath.pathPoints.length + " pts");

                                // Add the new split path to layerPaths so it gets processed
                                layerPaths.push(dupPath);

                                // Also add to SELECTED_PATHS so endpoint collection includes this path's endpoints
                                SELECTED_PATHS.push(dupPath);

                                // Track this pair so we can force a connection between them
                                splitPathPairs.push({ pathA: targetPath, pathB: dupPath });

                                // CRITICAL: Track that BOTH halves should NOT connect to the intersecting path
                                // The original crossoverInfo only has targetPath, but dupPath also crosses over!
                                if (xoSplit.intersectingPath) {
                                    splitPathCrossoverBlocks.push({
                                        splitPath: dupPath,
                                        crossoverPath: xoSplit.intersectingPath
                                    });
                                    addDebug("[XOVER-SPLIT] Blocking dupPath <-> intersectingPath connection");
                                }

                                // Mark both as created paths for styling
                                markCreatedPath(targetPath);
                                markCreatedPath(dupPath);
                            }
                        }
                    } catch (eSplit) {
                        addDebug("[XOVER-SPLIT] ERROR: " + eSplit);
                    }
                }
            }

            if (layerPaths.length > 1) {
                // Array to collect intersection points where paths connect (should not get components)
                var intersectionIgnorePoints = [];
                var connections = findAllConnections(layerPaths, CONNECTION_DIST, intersectionIgnorePoints, ignoredAnchors);

                // Add intersection ignore points to main ignoredAnchors list
                if (intersectionIgnorePoints.length > 0) {
                    for (var iipIdx = 0; iipIdx < intersectionIgnorePoints.length; iipIdx++) {
                        ignoredAnchors.push(intersectionIgnorePoints[iipIdx]);
                    }
                    addDebug("[COMPOUND] Added " + intersectionIgnorePoints.length + " intersection points to ignore list");
                }

                // Add forced connections for split path pairs (they should stay connected)
                if (typeof splitPathPairs !== 'undefined' && splitPathPairs.length > 0) {
                    for (var spIdx = 0; spIdx < splitPathPairs.length; spIdx++) {
                        connections.push([splitPathPairs[spIdx].pathA, splitPathPairs[spIdx].pathB]);
                        addDebug("[XOVER] Added forced connection between split path halves");
                    }
                }

                // Add forced connections for EARLY split path pairs (from POST-ORTHO-SPLIT)
                // This ensures split halves and any branches that connect to them end up in the same compound
                if (typeof EARLY_SPLIT_PAIRS !== 'undefined' && EARLY_SPLIT_PAIRS.length > 0) {
                    for (var espIdx = 0; espIdx < EARLY_SPLIT_PAIRS.length; espIdx++) {
                        try {
                            var espPair = EARLY_SPLIT_PAIRS[espIdx];
                            if (!espPair || !espPair.pathA || !espPair.pathB) continue;
                            // Validity check - skip if paths were removed
                            try { var testA = espPair.pathA.typename; } catch (eTestA) { continue; }
                            try { var testB = espPair.pathB.typename; } catch (eTestB) { continue; }
                            // Only add if both paths are in layerPaths (same layer)
                            var pathAInLayer = false, pathBInLayer = false;
                            for (var lpCheck = 0; lpCheck < layerPaths.length; lpCheck++) {
                                try {
                                    if (layerPaths[lpCheck] === espPair.pathA) pathAInLayer = true;
                                    if (layerPaths[lpCheck] === espPair.pathB) pathBInLayer = true;
                                } catch (eLpCheck) { }
                            }
                            if (pathAInLayer && pathBInLayer) {
                                connections.push([espPair.pathA, espPair.pathB]);
                            }
                        } catch (eEspPair) { }
                    }
                }

                // Add forced connections for carve-out compound siblings (carved halves must stay together)
                if (carveOutForcedConnections.length > 0) {
                    for (var cofcIdx = 0; cofcIdx < carveOutForcedConnections.length; cofcIdx++) {
                        connections.push(carveOutForcedConnections[cofcIdx]);
                    }
                    addDebug("[COMPOUND] Added " + carveOutForcedConnections.length + " carve-out sibling connections");
                }

                // CROSSOVER FILTER: Remove connections between paths that are linked by a crossover segment
                // Also filter AUTO-CARVE blocked connections (carved halves should not connect to crossing path)
                // Also filter PERSISTENT crossovers (on rerun) by checking path endpoints near ignored anchors
                if (crossoverInfo.length > 0 || (typeof splitPathCrossoverBlocks !== 'undefined' && splitPathCrossoverBlocks.length > 0) || (typeof CARVE_BLOCKED_CONNECTIONS !== 'undefined' && CARVE_BLOCKED_CONNECTIONS.length > 0) || (ignoredAnchors && ignoredAnchors.length > 0)) {
                    var originalConnCount = connections.length;
                    var filteredConnections = [];
                    for (var connIdx = 0; connIdx < connections.length; connIdx++) {
                        try {
                            var connPathA = connections[connIdx][0];
                            var connPathB = connections[connIdx][1];
                            // Validity check
                            if (!connPathA || !connPathB) continue;
                            try { var testCA = connPathA.typename; } catch (eCA) { continue; }
                            try { var testCB = connPathB.typename; } catch (eCB) { continue; }

                            var isCrossoverConnection = false;

                            // Check against original crossoverInfo (pathWithSmallSeg <-> intersectingPath)
                            for (var xoIdx = 0; xoIdx < crossoverInfo.length; xoIdx++) {
                                try {
                                    var xo = crossoverInfo[xoIdx];
                                    if (!xo || !xo.pathWithSmallSeg || !xo.intersectingPath) continue;
                                    if ((connPathA === xo.pathWithSmallSeg && connPathB === xo.intersectingPath) ||
                                        (connPathB === xo.pathWithSmallSeg && connPathA === xo.intersectingPath)) {
                                        isCrossoverConnection = true;
                                        addDebug("[XOVER-FILTER] Blocking original crossover connection");
                                        break;
                                    }
                                } catch (eXo) { }
                            }

                            // ALSO check against split path crossover blocks (dupPath <-> intersectingPath)
                            // This catches connections between the NEW split half and the intersecting path
                            if (!isCrossoverConnection && typeof splitPathCrossoverBlocks !== 'undefined') {
                                for (var spxoIdx = 0; spxoIdx < splitPathCrossoverBlocks.length; spxoIdx++) {
                                    try {
                                        var spxo = splitPathCrossoverBlocks[spxoIdx];
                                        if (!spxo || !spxo.splitPath || !spxo.crossoverPath) continue;
                                        if ((connPathA === spxo.splitPath && connPathB === spxo.crossoverPath) ||
                                            (connPathB === spxo.splitPath && connPathA === spxo.crossoverPath)) {
                                            isCrossoverConnection = true;
                                            addDebug("[XOVER-FILTER] Blocking split-half crossover connection");
                                            break;
                                        }
                                    } catch (eSpXo) { }
                                }
                            }

                            // ALSO check against CARVE_BLOCKED_CONNECTIONS (AUTO-CARVE blocked pairs)
                            // This blocks T-junction connections between carved halves and crossing paths
                            // Use stored endpoint coordinates for reliable comparison
                            if (!isCrossoverConnection && typeof CARVE_BLOCKED_CONNECTIONS !== 'undefined' && CARVE_BLOCKED_CONNECTIONS.length > 0) {
                                addDebug("[CARVE-BLOCK-CHECK] Checking connection against " + CARVE_BLOCKED_CONNECTIONS.length + " blocked pairs");

                                // Helper to compare path endpoints with stored coordinates
                                function pathMatchesStoredEndpoints(path, storedEndpoints) {
                                    if (!path || !storedEndpoints) return false;
                                    try {
                                        var pts = path.pathPoints;
                                        if (!pts || pts.length < 2) return false;
                                        var tol = 1.0; // 1pt tolerance
                                        var p0 = pts[0].anchor;
                                        var pL = pts[pts.length - 1].anchor;
                                        var s0 = storedEndpoints.start;
                                        var sL = storedEndpoints.end;
                                        // Check forward match
                                        var fwd = (Math.abs(p0[0] - s0[0]) < tol && Math.abs(p0[1] - s0[1]) < tol &&
                                                   Math.abs(pL[0] - sL[0]) < tol && Math.abs(pL[1] - sL[1]) < tol);
                                        // Check reverse match
                                        var rev = (Math.abs(p0[0] - sL[0]) < tol && Math.abs(p0[1] - sL[1]) < tol &&
                                                   Math.abs(pL[0] - s0[0]) < tol && Math.abs(pL[1] - s0[1]) < tol);
                                        return fwd || rev;
                                    } catch (e) { return false; }
                                }

                                // Debug: log endpoints of connPathA and connPathB
                                try {
                                    var dbgPtsA = connPathA.pathPoints;
                                    var dbgPtsB = connPathB.pathPoints;
                                    addDebug("[CARVE-BLOCK-CHECK] connPathA endpoints: [" + dbgPtsA[0].anchor[0].toFixed(1) + "," + dbgPtsA[0].anchor[1].toFixed(1) + "] to [" + dbgPtsA[dbgPtsA.length-1].anchor[0].toFixed(1) + "," + dbgPtsA[dbgPtsA.length-1].anchor[1].toFixed(1) + "]");
                                    addDebug("[CARVE-BLOCK-CHECK] connPathB endpoints: [" + dbgPtsB[0].anchor[0].toFixed(1) + "," + dbgPtsB[0].anchor[1].toFixed(1) + "] to [" + dbgPtsB[dbgPtsB.length-1].anchor[0].toFixed(1) + "," + dbgPtsB[dbgPtsB.length-1].anchor[1].toFixed(1) + "]");
                                } catch (eDbg) {}
                                for (var cbcIdx = 0; cbcIdx < CARVE_BLOCKED_CONNECTIONS.length; cbcIdx++) {
                                    try {
                                        var cbc = CARVE_BLOCKED_CONNECTIONS[cbcIdx];
                                        if (!cbc) continue;
                                        // Debug: log stored endpoint coordinates
                                        try {
                                            if (cbc.carvedEndpoints) {
                                                addDebug("[CARVE-BLOCK-CHECK] cbc[" + cbcIdx + "] stored carvedEndpoints: [" + cbc.carvedEndpoints.start[0].toFixed(1) + "," + cbc.carvedEndpoints.start[1].toFixed(1) + "] to [" + cbc.carvedEndpoints.end[0].toFixed(1) + "," + cbc.carvedEndpoints.end[1].toFixed(1) + "]");
                                            }
                                            if (cbc.crossEndpoints) {
                                                addDebug("[CARVE-BLOCK-CHECK] cbc[" + cbcIdx + "] stored crossEndpoints: [" + cbc.crossEndpoints.start[0].toFixed(1) + "," + cbc.crossEndpoints.start[1].toFixed(1) + "] to [" + cbc.crossEndpoints.end[0].toFixed(1) + "," + cbc.crossEndpoints.end[1].toFixed(1) + "]");
                                            }
                                        } catch (eDbg2) { }
                                        // Use stored coordinates for comparison (more reliable than path references)
                                        var carvedMatchA = cbc.carvedEndpoints ? pathMatchesStoredEndpoints(connPathA, cbc.carvedEndpoints) : false;
                                        var carvedMatchB = cbc.carvedEndpoints ? pathMatchesStoredEndpoints(connPathB, cbc.carvedEndpoints) : false;
                                        var crossMatchA = cbc.crossEndpoints ? pathMatchesStoredEndpoints(connPathA, cbc.crossEndpoints) : false;
                                        var crossMatchB = cbc.crossEndpoints ? pathMatchesStoredEndpoints(connPathB, cbc.crossEndpoints) : false;
                                        addDebug("[CARVE-BLOCK-CHECK] cbc[" + cbcIdx + "]: carvedMatchA=" + carvedMatchA + ", carvedMatchB=" + carvedMatchB + ", crossMatchA=" + crossMatchA + ", crossMatchB=" + crossMatchB);
                                        if ((carvedMatchA && crossMatchB) || (carvedMatchB && crossMatchA)) {
                                            isCrossoverConnection = true;
                                            addDebug("[XOVER-FILTER] Blocking carve-out crossover connection (coordinate match)");
                                            break;
                                        }
                                    } catch (eCbc) {
                                        addDebug("[CARVE-BLOCK-CHECK] Error: " + eCbc);
                                    }
                                }
                            }

                            // PERSISTENT CROSSOVER CHECK: On reruns, CARVE_BLOCKED_CONNECTIONS is empty
                            // but we can detect carve-out crossovers by looking for PAIRS of ignored anchors
                            // that are ~8.5pt apart (indicating a carve-out gap). Only block connections where:
                            // 1. There's a PAIR of ignored anchors ~8.5pt apart (carve-out gap signature)
                            // 2. One path has an endpoint near one of these paired anchors
                            // 3. The other path passes through the gap (near the midpoint between the pair)
                            // IMPORTANT: Do NOT block if BOTH paths have endpoints near the SAME gap pair
                            // (those are siblings from the same carve-out, not crossovers)
                            if (!isCrossoverConnection && ignoredAnchors && ignoredAnchors.length >= 2) {
                                var CARVE_GAP_MIN = 7; // Minimum distance between carve-out anchor pairs
                                var CARVE_GAP_MAX = 10; // Maximum distance between carve-out anchor pairs (8.5pt nominal)
                                var CARVE_ENDPOINT_TOLERANCE = 3; // How close endpoint must be to an anchor
                                try {
                                    var ptsA = connPathA.pathPoints;
                                    var ptsB = connPathB.pathPoints;
                                    if (ptsA && ptsB && ptsA.length >= 2 && ptsB.length >= 2) {
                                        // Get endpoints of both paths
                                        var endpointsA = [[ptsA[0].anchor[0], ptsA[0].anchor[1]], [ptsA[ptsA.length - 1].anchor[0], ptsA[ptsA.length - 1].anchor[1]]];
                                        var endpointsB = [[ptsB[0].anchor[0], ptsB[0].anchor[1]], [ptsB[ptsB.length - 1].anchor[0], ptsB[ptsB.length - 1].anchor[1]]];

                                        // Find pairs of ignored anchors that are ~8.5pt apart (carve-out gaps)
                                        for (var ignIdx1 = 0; ignIdx1 < ignoredAnchors.length - 1 && !isCrossoverConnection; ignIdx1++) {
                                            for (var ignIdx2 = ignIdx1 + 1; ignIdx2 < ignoredAnchors.length && !isCrossoverConnection; ignIdx2++) {
                                                var ign1 = ignoredAnchors[ignIdx1];
                                                var ign2 = ignoredAnchors[ignIdx2];
                                                var pairDx = ign1[0] - ign2[0];
                                                var pairDy = ign1[1] - ign2[1];
                                                var pairDist = Math.sqrt(pairDx * pairDx + pairDy * pairDy);

                                                // Only consider pairs that are ~8.5pt apart (carve-out gap signature)
                                                if (pairDist >= CARVE_GAP_MIN && pairDist <= CARVE_GAP_MAX) {
                                                    var gapMidpoint = [(ign1[0] + ign2[0]) / 2, (ign1[1] + ign2[1]) / 2];

                                                    // Check if pathA has an endpoint near either anchor of this pair
                                                    var pathAEndpointNearPair = false;
                                                    var pathANearWhichAnchor = 0; // 1 or 2
                                                    for (var epAIdx = 0; epAIdx < endpointsA.length && !pathAEndpointNearPair; epAIdx++) {
                                                        var d1 = Math.sqrt(Math.pow(endpointsA[epAIdx][0] - ign1[0], 2) + Math.pow(endpointsA[epAIdx][1] - ign1[1], 2));
                                                        var d2 = Math.sqrt(Math.pow(endpointsA[epAIdx][0] - ign2[0], 2) + Math.pow(endpointsA[epAIdx][1] - ign2[1], 2));
                                                        if (d1 <= CARVE_ENDPOINT_TOLERANCE) {
                                                            pathAEndpointNearPair = true;
                                                            pathANearWhichAnchor = 1;
                                                        } else if (d2 <= CARVE_ENDPOINT_TOLERANCE) {
                                                            pathAEndpointNearPair = true;
                                                            pathANearWhichAnchor = 2;
                                                        }
                                                    }

                                                    // SIBLING CHECK: Before checking if B passes through gap,
                                                    // check if B ALSO has an endpoint near this gap pair (sibling)
                                                    var pathBEndpointNearPair = false;
                                                    if (pathAEndpointNearPair) {
                                                        for (var epBSibIdx = 0; epBSibIdx < endpointsB.length && !pathBEndpointNearPair; epBSibIdx++) {
                                                            var db1 = Math.sqrt(Math.pow(endpointsB[epBSibIdx][0] - ign1[0], 2) + Math.pow(endpointsB[epBSibIdx][1] - ign1[1], 2));
                                                            var db2 = Math.sqrt(Math.pow(endpointsB[epBSibIdx][0] - ign2[0], 2) + Math.pow(endpointsB[epBSibIdx][1] - ign2[1], 2));
                                                            if (db1 <= CARVE_ENDPOINT_TOLERANCE || db2 <= CARVE_ENDPOINT_TOLERANCE) {
                                                                pathBEndpointNearPair = true;
                                                            }
                                                        }
                                                    }

                                                    // If BOTH paths have endpoints near this gap pair, they're SIBLINGS - don't block!
                                                    if (pathAEndpointNearPair && pathBEndpointNearPair) {
                                                        // Skip this gap pair - these are siblings, not crossovers
                                                        continue;
                                                    }

                                                    // Check if pathB passes through the gap (near the midpoint)
                                                    var pathBPassesThroughGap = false;
                                                    if (pathAEndpointNearPair) {
                                                        for (var segBIdx = 0; segBIdx < ptsB.length - 1 && !pathBPassesThroughGap; segBIdx++) {
                                                            var segStart = [ptsB[segBIdx].anchor[0], ptsB[segBIdx].anchor[1]];
                                                            var segEnd = [ptsB[segBIdx + 1].anchor[0], ptsB[segBIdx + 1].anchor[1]];
                                                            var res = closestPointOnSegment(segStart, segEnd, gapMidpoint);
                                                            if (res.t > 0.01 && res.t < 0.99) {
                                                                var distToMid = Math.sqrt(Math.pow(gapMidpoint[0] - res.pt[0], 2) + Math.pow(gapMidpoint[1] - res.pt[1], 2));
                                                                if (distToMid <= CARVE_GAP_MAX / 2) {
                                                                    pathBPassesThroughGap = true;
                                                                }
                                                            }
                                                        }
                                                    }

                                                    if (pathAEndpointNearPair && pathBPassesThroughGap) {
                                                        isCrossoverConnection = true;
                                                        addDebug("[XOVER-FILTER] Blocking persistent carve-out crossover (pathA endpoint near gap pair)");
                                                    }

                                                    // Also check reverse: pathB endpoint near pair, pathA passes through gap
                                                    // Note: pathBEndpointNearPair may already be set from sibling check above
                                                    // If it was set, and pathAEndpointNearPair was also set, we already skipped this pair
                                                    // So if we get here, either pathB wasn't near the pair, or we need to check if A passes through
                                                    if (!isCrossoverConnection && !pathBEndpointNearPair) {
                                                        // Re-check if pathB has endpoint near pair (wasn't checked in sibling check if pathA wasn't near)
                                                        for (var epBIdx = 0; epBIdx < endpointsB.length && !pathBEndpointNearPair; epBIdx++) {
                                                            var dbRev1 = Math.sqrt(Math.pow(endpointsB[epBIdx][0] - ign1[0], 2) + Math.pow(endpointsB[epBIdx][1] - ign1[1], 2));
                                                            var dbRev2 = Math.sqrt(Math.pow(endpointsB[epBIdx][0] - ign2[0], 2) + Math.pow(endpointsB[epBIdx][1] - ign2[1], 2));
                                                            if (dbRev1 <= CARVE_ENDPOINT_TOLERANCE || dbRev2 <= CARVE_ENDPOINT_TOLERANCE) {
                                                                pathBEndpointNearPair = true;
                                                            }
                                                        }
                                                    }

                                                    if (!isCrossoverConnection && pathBEndpointNearPair && !pathAEndpointNearPair) {
                                                        // PathB is near gap but pathA isn't - check if A passes through gap
                                                        var pathAPassesThroughGap = false;
                                                        for (var segAIdx = 0; segAIdx < ptsA.length - 1 && !pathAPassesThroughGap; segAIdx++) {
                                                            var segStartA = [ptsA[segAIdx].anchor[0], ptsA[segAIdx].anchor[1]];
                                                            var segEndA = [ptsA[segAIdx + 1].anchor[0], ptsA[segAIdx + 1].anchor[1]];
                                                            var resA = closestPointOnSegment(segStartA, segEndA, gapMidpoint);
                                                            if (resA.t > 0.01 && resA.t < 0.99) {
                                                                var distToMidA = Math.sqrt(Math.pow(gapMidpoint[0] - resA.pt[0], 2) + Math.pow(gapMidpoint[1] - resA.pt[1], 2));
                                                                if (distToMidA <= CARVE_GAP_MAX / 2) {
                                                                    pathAPassesThroughGap = true;
                                                                }
                                                            }
                                                        }

                                                        if (pathAPassesThroughGap) {
                                                            isCrossoverConnection = true;
                                                            addDebug("[XOVER-FILTER] Blocking persistent carve-out crossover (pathB endpoint near gap pair)");
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } catch (eIgnoreCheck) {
                                    // Silently continue if check fails
                                }
                            }

                            if (!isCrossoverConnection) {
                                filteredConnections.push(connections[connIdx]);
                            }
                        } catch (eConnFilter) { }
                    }
                    var blockedCount = originalConnCount - filteredConnections.length;
                    if (blockedCount > 0) {
                        addDebug("[XOVER-FILTER] Blocked " + blockedCount + " crossover connection(s)");
                    }
                    connections = filteredConnections;
                }

                var components = findConnectedComponents(layerPaths, connections);
                var COMPOUND_BATCH_SIZE = 10; // Update progress every 10 components

                // DEBUG: Log connections and components for blue ductwork (compact)
                if (layerName.toLowerCase().indexOf("blue") !== -1) {
                    addDebug("[XOVER] " + connections.length + " connections -> " + components.length + " components");
                }

                for (var i = 0; i < components.length; i++) {
                    // BATCH: Update progress between batches
                    if (i > 0 && i % COMPOUND_BATCH_SIZE === 0) {
                        if (progressWin && progressLabel) {
                            try {
                                progressLabel.text = "Compounding... " + i + "/" + components.length;
                                progressWin.update();
                            } catch (e) { }
                        }
                    }
                    var comp = components[i];
                    if (comp.length <= 1) continue;
                    var componentIsTargeted = false;

                    // Check if any path in component was selected
                    for (var normalIdx = 0; normalIdx < comp.length; normalIdx++) {
                        if (isPathSelected(comp[normalIdx])) {
                            componentIsTargeted = true;
                            break;
                        }
                    }

                    // *** CAPTURE STROKE WIDTH BEFORE NORMALIZING OR COMPOUNDING ***
                    // This MUST happen first because normalizeStrokeProperties resets strokeWidth to 1pt
                    // And after executeMenuCommand("compoundPath"), stroke detection also fails
                    var preCompoundStrokeWidth = null;
                    var preCompoundScale = null;
                    addDebug("[COMPOUND] Checking " + comp.length + " paths for stroke width BEFORE normalization");

                    // First, check if any path has MDUX_PreCompoundScale metadata (from EARLY_SPLIT paths)
                    for (var metaIdx = 0; metaIdx < comp.length && preCompoundScale === null; metaIdx++) {
                        try {
                            var metaPath = comp[metaIdx];
                            var pathMeta = MDUX_getMetadata(metaPath);
                            if (pathMeta && pathMeta.MDUX_PreCompoundScale) {
                                preCompoundScale = pathMeta.MDUX_PreCompoundScale;
                                preCompoundStrokeWidth = (preCompoundScale / 100) * 4; // Convert back to pt
                                addDebug("[COMPOUND] Found MDUX_PreCompoundScale=" + preCompoundScale + "% from EARLY_SPLIT path metadata");
                            }
                        } catch (eMeta) { }
                    }

                    // If no metadata found, check actual strokeWidth values
                    if (preCompoundScale === null) {
                        for (var swIdx = 0; swIdx < comp.length; swIdx++) {
                            try {
                                var swPath = comp[swIdx];
                                var swStroked = swPath.stroked;
                                var swWidth = swPath.strokeWidth;
                                addDebug("[COMPOUND] Path " + swIdx + ": stroked=" + swStroked + ", strokeWidth=" + (swWidth || "undefined"));
                                // Check strokeWidth even if stroked is false (appearance-based strokes)
                                if (swWidth > 0.1) {
                                    if (preCompoundStrokeWidth === null || swWidth > preCompoundStrokeWidth) {
                                        preCompoundStrokeWidth = swWidth;
                                    }
                                }
                            } catch (eSW) {
                                addDebug("[COMPOUND] Path " + swIdx + " error: " + eSW);
                            }
                        }
                        if (preCompoundStrokeWidth !== null) {
                            // Calculate scale based on 4pt base stroke
                            preCompoundScale = Math.round((preCompoundStrokeWidth / 4) * 100);
                            addDebug("[COMPOUND] Captured pre-compound stroke: " + preCompoundStrokeWidth.toFixed(2) + "pt = " + preCompoundScale + "% scale");
                        } else {
                            addDebug("[COMPOUND] WARNING: Could not capture stroke width from any path");
                        }
                    }

                    // *** NORMALIZE STROKE PROPERTIES BEFORE COMPOUNDING ***
                    // This resets stroke to 1pt which is why we capture above first
                    normalizeStrokeProperties(comp);

                    // *** PARENT VALIDATION - Ensure all paths are in same container ***
                    // Compounding fails silently if paths are in different parent groups
                    var compoundParentLayer = null;
                    var needsParentReconciliation = false;
                    var firstValidParent = null;

                    for (var pvi = 0; pvi < comp.length; pvi++) {
                        try {
                            var pvPath = comp[pvi];
                            if (!pvPath) continue;
                            var pvParent = pvPath.parent;
                            if (!pvParent) continue;

                            // Find the layer (walk up if in group)
                            var pvLayer = pvParent;
                            while (pvLayer && pvLayer.typename !== "Layer") {
                                pvLayer = pvLayer.parent;
                            }
                            if (pvLayer && !compoundParentLayer) {
                                compoundParentLayer = pvLayer;
                            }

                            // Check if parent differs from first
                            if (!firstValidParent) {
                                firstValidParent = pvParent;
                            } else if (pvParent !== firstValidParent) {
                                needsParentReconciliation = true;
                            }
                        } catch (ePv) { }
                    }

                    // Move all paths to the common layer if they're in different parents
                    if (needsParentReconciliation && compoundParentLayer) {
                        addDebug("[COMPOUND] Parent mismatch detected - moving " + comp.length + " paths to layer");
                        for (var mvIdx = 0; mvIdx < comp.length; mvIdx++) {
                            try {
                                var mvPath = comp[mvIdx];
                                if (mvPath && mvPath.parent !== compoundParentLayer) {
                                    mvPath.move(compoundParentLayer, ElementPlacement.PLACEATEND);
                                }
                            } catch (eMv) { }
                        }
                    }

                    doc.selection = null;
                    var validPathCount = 0;
                    for (var j = 0; j < comp.length; j++) {
                        try {
                            comp[j].selected = true;
                            validPathCount++;
                        } catch (e) {
                            // Skip if path is invalid
                        }
                    }

                    var originalInteractionLevel = app.userInteractionLevel;
                    var compoundSuccess = false;
                    try {
                        if (doc.selection.length > 1) {
                            app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
                            app.executeMenuCommand("compoundPath");
                            // Check if compound was created
                            if (doc.selection.length === 1 && doc.selection[0].typename === "CompoundPathItem") {
                                compoundSuccess = true;
                            }
                        }
                    } catch (e) {
                        addDebug("[COMPOUND] ERROR: " + e);
                    }

                    // Log if compounding failed on a multi-path component
                    if (!compoundSuccess && validPathCount > 1) {
                        addDebug("[COMPOUND] WARNING: Compounding failed for " + validPathCount + " paths (may be in different containers)");
                    }

                    // Restore interaction level and process result
                    try {
                        // Mark compound paths for styling (both modes)
                        if (componentIsTargeted) {
                            try {
                                var compoundSelection = doc.selection;
                                for (var csIdx = 0; compoundSelection && csIdx < compoundSelection.length; csIdx++) {
                                    var compoundItem = compoundSelection[csIdx];
                                    // Track for styling in normal mode
                                    markCreatedPath(compoundItem);
                                    COMPOUND_PATHS_TO_STYLE.push(compoundItem);

                                    // *** FIX: Add compound to SELECTED_PATHS for component placement ***
                                    // The original paths are now part of the compound and invalid
                                    SELECTED_PATHS.push(compoundItem);

                                    // *** FIX: Remove original paths from SELECTED_PATHS ***
                                    // They are now invalid (part of the compound)
                                    for (var remIdx = 0; remIdx < comp.length; remIdx++) {
                                        for (var spRemIdx = SELECTED_PATHS.length - 1; spRemIdx >= 0; spRemIdx--) {
                                            try {
                                                if (SELECTED_PATHS[spRemIdx] === comp[remIdx]) {
                                                    SELECTED_PATHS.splice(spRemIdx, 1);
                                                }
                                            } catch (eRem) { }
                                        }
                                    }

                                    // Copy rotation override from child paths to compound path metadata
                                    try {
                                        addDebug("[COMPOUND] Processing compound path, typename=" + compoundItem.typename);
                                        if (compoundItem.typename === "CompoundPathItem" && compoundItem.pathItems) {
                                            addDebug("[COMPOUND] Compound path has " + compoundItem.pathItems.length + " child paths");
                                            addDebug("[COMPOUND] Compound path note BEFORE rotation copy: " + (compoundItem.note || "(empty)"));

                                            var foundRotation = null;
                                            for (var childIdx = 0; childIdx < compoundItem.pathItems.length; childIdx++) {
                                                var childPath = compoundItem.pathItems[childIdx];
                                                addDebug("[COMPOUND] Child " + childIdx + " note: " + (childPath.note || "(empty)"));
                                                var childRot = getRotationOverride(childPath);
                                                addDebug("[COMPOUND] Child " + childIdx + " rotation override: " + childRot);
                                                if (childRot !== null && childRot !== undefined && isFinite(childRot)) {
                                                    foundRotation = childRot;
                                                    addDebug("[COMPOUND] Found rotation: " + foundRotation);
                                                    break; // Found rotation, use it
                                                }
                                            }
                                            // Store rotation override AND pre-compound scale in compound path's MDUX_META
                                            var compoundMeta = MDUX_getMetadata(compoundItem);
                                            if (!compoundMeta) compoundMeta = {};
                                            var metaChanged = false;

                                            if (foundRotation !== null) {
                                                addDebug("[COMPOUND] Storing rotation " + foundRotation + " in compound path metadata");
                                                compoundMeta.MDUX_RotationOverride = foundRotation;
                                                metaChanged = true;
                                            } else {
                                                addDebug("[COMPOUND] No rotation override found in child paths");
                                            }

                                            // Store pre-compound scale so styling can use it
                                            if (preCompoundScale !== null) {
                                                addDebug("[COMPOUND] Storing pre-compound scale " + preCompoundScale + "% in metadata");
                                                compoundMeta.MDUX_PreCompoundScale = preCompoundScale;
                                                metaChanged = true;
                                            }

                                            if (metaChanged) {
                                                addDebug("[COMPOUND] Writing metadata: " + JSON.stringify(compoundMeta));
                                                MDUX_setMetadata(compoundItem, compoundMeta);
                                                addDebug("[COMPOUND] Metadata written. Compound path note AFTER: " + (compoundItem.note || "(empty)"));
                                            }
                                        }
                                    } catch (eRotCopy) {
                                        addDebug("[COMPOUND] ERROR copying rotation: " + eRotCopy);
                                    }
                                }
                            } catch (eMarkCompound) { }
                        }
                    } catch (eCompoundProcess) {
                        addDebug("[COMPOUND] Error in post-compound processing: " + eCompoundProcess);
                    } finally {
                        app.userInteractionLevel = originalInteractionLevel;
                    }
                }
            }
        }

        // CROSSOVER POST-PROCESSING: Place ignore anchors at split endpoints
        // Combine early crossover segments with any found during compounding
        var combinedCrossoverSegments = [];
        for (var ecIdx = 0; ecIdx < EARLY_CROSSOVER_SEGMENTS.length; ecIdx++) {
            combinedCrossoverSegments.push(EARLY_CROSSOVER_SEGMENTS[ecIdx]);
        }
        for (var acIdx = 0; acIdx < ALL_CROSSOVER_SEGMENTS.length; acIdx++) {
            combinedCrossoverSegments.push(ALL_CROSSOVER_SEGMENTS[acIdx]);
        }
        addDebug("[XOVER-POST] EARLY_CROSSOVER_SEGMENTS: " + EARLY_CROSSOVER_SEGMENTS.length + ", ALL_CROSSOVER_SEGMENTS: " + ALL_CROSSOVER_SEGMENTS.length + ", combined: " + combinedCrossoverSegments.length);

        if (combinedCrossoverSegments.length > 0) {
            addDebug("[XOVER-POST] Processing " + combinedCrossoverSegments.length + " crossover segment(s)");

            // Get or create Ignored layer for placing ignore anchors
            var ignoredLayer = null;
            var ignoredLayerWasLocked = false;
            var ignoredLayerWasHidden = false;
            try {
                ignoredLayer = doc.layers.getByName("Ignored");
                addDebug("[XOVER-POST] Found 'Ignored' layer. locked=" + ignoredLayer.locked + ", visible=" + ignoredLayer.visible);

                // Unlock if locked so we can place anchors
                if (ignoredLayer.locked) {
                    ignoredLayerWasLocked = true;
                    ignoredLayer.locked = false;
                    addDebug("[XOVER-POST] Unlocked 'Ignored' layer");
                }
                // Make visible if hidden (can't add to hidden layers in some cases)
                if (!ignoredLayer.visible) {
                    ignoredLayerWasHidden = true;
                    ignoredLayer.visible = true;
                    addDebug("[XOVER-POST] Made 'Ignored' layer visible");
                }

                // Also check parent layer if it exists
                if (ignoredLayer.parent && ignoredLayer.parent.typename === "Layer") {
                    var parentLayer = ignoredLayer.parent;
                    if (parentLayer.locked) {
                        parentLayer.locked = false;
                        addDebug("[XOVER-POST] Unlocked parent layer: " + parentLayer.name);
                    }
                }
            } catch (e) {
                addDebug("[XOVER-POST] Creating new 'Ignored' layer. Error was: " + e);
                ignoredLayer = doc.layers.add();
                ignoredLayer.name = "Ignored";
            }

            for (var xoPostIdx = 0; xoPostIdx < combinedCrossoverSegments.length; xoPostIdx++) {
                var xoSeg = combinedCrossoverSegments[xoPostIdx];
                addDebug("[XOVER-POST] Crossover " + xoPostIdx + ": seg from [" + xoSeg.segStart[0].toFixed(1) + "," + xoSeg.segStart[1].toFixed(1) + "] to [" + xoSeg.segEnd[0].toFixed(1) + "," + xoSeg.segEnd[1].toFixed(1) + "]");

                // Place ignore anchors at both endpoints of the crossover segment
                try {
                    // Create single-point path at segStart as ignore anchor
                    var ignoreAnchor1 = ignoredLayer.pathItems.add();
                    ignoreAnchor1.setEntirePath([[xoSeg.segStart[0], xoSeg.segStart[1]]]);
                    ignoreAnchor1.filled = false;
                    ignoreAnchor1.stroked = false;
                    addDebug("[XOVER-POST] Placed ignore anchor at [" + xoSeg.segStart[0].toFixed(1) + "," + xoSeg.segStart[1].toFixed(1) + "]");
                    // Add to ignoredAnchors array so component placement skips this position
                    ignoredAnchors.push([xoSeg.segStart[0], xoSeg.segStart[1]]);

                    // Create single-point path at segEnd as ignore anchor
                    var ignoreAnchor2 = ignoredLayer.pathItems.add();
                    ignoreAnchor2.setEntirePath([[xoSeg.segEnd[0], xoSeg.segEnd[1]]]);
                    ignoreAnchor2.filled = false;
                    ignoreAnchor2.stroked = false;
                    addDebug("[XOVER-POST] Placed ignore anchor at [" + xoSeg.segEnd[0].toFixed(1) + "," + xoSeg.segEnd[1].toFixed(1) + "]");
                    // Add to ignoredAnchors array so component placement skips this position
                    ignoredAnchors.push([xoSeg.segEnd[0], xoSeg.segEnd[1]]);

                    addDebug("[XOVER-POST] Added crossover positions to ignoredAnchors array (total: " + ignoredAnchors.length + ")");
                } catch (eIgnore) {
                    addDebug("[XOVER-POST] ERROR placing ignore anchors: " + eIgnore);
                }
                // Note: Path splitting now happens BEFORE compounding (see pre-compounding crossover split section)
                // This ensures both halves get merged into the same compound path
            }

            // Restore Ignored layer state
            if (ignoredLayer) {
                try {
                    if (ignoredLayerWasHidden) {
                        ignoredLayer.visible = false;
                        addDebug("[XOVER-POST] Re-hid 'Ignored' layer");
                    }
                    if (ignoredLayerWasLocked) {
                        ignoredLayer.locked = true;
                        addDebug("[XOVER-POST] Re-locked 'Ignored' layer");
                    }
                } catch (eRestore) { }
            }
        }

        doc.selection = null;

        // STEP 7: Call the next script in the workflow (embedded version of 03 - Place Ductwork at Points.jsx)
        updateProgress("Placing components...");
        try {
            // Embedded to avoid external file dependency. Runs the Place Ductwork routine using the current document.
            (function placeDuctworkAtPoints_embedded(doc) {
                // --- BEGIN embedded 03 - Place Ductwork at Points.jsx ---

                // Helper: read current global scale % from Change Scale script
                function getCurrentScaleFactor_local(docParam) {
                    var layerName = "Scale Factor Container Layer",
                        boxName = "ScaleFactorBox";
                    try {
                        var container = docParam.layers.getByName(layerName);
                        for (var i = 0; i < container.pathItems.length; i++) {
                            var pi = container.pathItems[i];
                            if (pi.name === boxName) {
                                var val = parseFloat(pi.note);
                                if (!isNaN(val)) return val;
                            }
                        }
                    } catch (e) { }
                    return 100;
                }

                // Base folder and component definitions (kept as in original)
                var COMPONENT_FILES_PATH = "E:/Work/Work/Floorplans/Ductwork Assets/";
                var USE_EMORY_ASSETS = false; // Always use normal assets (Emory mode removed)
                function componentFile(name) {
                    return name + ".ai";
                }
                var COMPONENT_TYPES = [
                    { name: "Square Register", layer: "Square Registers", file: componentFile("Square Register") },
                    { name: "Rectangular Register", layer: "Rectangular Registers", file: componentFile("Rectangular Register") },
                    { name: "Circular Register", layer: "Circular Registers", file: componentFile("Circular Register") },
                    { name: "Exhaust Register", layer: "Exhaust Registers", file: componentFile("Exhaust Register") },
                    { name: "Secondary Exhaust Register", layer: "Secondary Exhaust Registers", file: componentFile("Secondary Exhaust Register") },
                    { name: "Orange Register", layer: "Orange Register", file: componentFile("Orange Register") },
                    { name: "Thermostat", layer: "Thermostats", file: componentFile("Thermostat") },
                    { name: "Unit", layer: "Units", file: componentFile("Unit") }
                ];

                // GLOBAL DEBUG ARRAY FOR UNIT PLACEMENT
                var UNIT_DEBUG = [];

                function embeddedMain() {
                    if (!doc) { alert("No document provided to embedded Place Ductwork routine."); return; }
                    try { doc.rulerOrigin = [0, 0]; } catch (e) { }

                    // Clear debug array
                    UNIT_DEBUG = [];

                    // Skipped cleanupLayers_local to preserve unselected artwork

                    var frameLayer = getLayerByName_local(doc, "Frame");
                    if (frameLayer && !frameLayer.locked) {
                        drawFrame_local(doc, frameLayer, doc.artboards[0]);
                    }

                    // Re-collect selected paths from ductwork layers RIGHT BEFORE component placement
                    // This ensures paths created by carve-out splits are included in proximity filter
                    var selectedPathsToUse = [];
                    var ductworkLayerNamesForPlacement = ["Blue Ductwork", "Green Ductwork", "Orange Ductwork", "Light Orange Ductwork"];
                    for (var dpLayerIdx = 0; dpLayerIdx < ductworkLayerNamesForPlacement.length; dpLayerIdx++) {
                        try {
                            var dpLayer = doc.layers.getByName(ductworkLayerNamesForPlacement[dpLayerIdx]);
                            if (!dpLayer) continue;
                            // Collect selected PathItems
                            for (var dpPathIdx = 0; dpPathIdx < dpLayer.pathItems.length; dpPathIdx++) {
                                var dpPath = dpLayer.pathItems[dpPathIdx];
                                if (dpPath.selected && !dpPath.guides && !dpPath.clipping) {
                                    selectedPathsToUse.push(dpPath);
                                }
                            }
                            // Collect selected CompoundPathItems
                            for (var dpCpIdx = 0; dpCpIdx < dpLayer.compoundPathItems.length; dpCpIdx++) {
                                var dpCp = dpLayer.compoundPathItems[dpCpIdx];
                                if (dpCp.selected) {
                                    selectedPathsToUse.push(dpCp);
                                }
                            }
                        } catch (eDpLayer) { }
                    }
                    addDebug("[COMPONENT PLACEMENT] Re-collected " + selectedPathsToUse.length + " selected path(s) from ductwork layers");
                    if (selectedPathsToUse.length === 0) selectedPathsToUse = null;
                    placeLinkedComponents_local(doc, selectedPathsToUse);

                    // DEBUG OUTPUT COMMENTED OUT - uncomment to show unit placement debug info
                    /*
                    // Show debug output in copyable dialog window
                    var debugText = "UNIT DEBUG OUTPUT\n";
                    var separator = "";
                    for (var i = 0; i < 80; i++) { separator += "="; }
                    debugText += separator + "\n";
                    if (UNIT_DEBUG.length > 0) {
                        debugText += UNIT_DEBUG.join("\n");
                    } else {
                        debugText += "No Unit anchors were processed.\n";
                        debugText += "This might mean:\n";
                        debugText += "- No Units layer exists\n";
                        debugText += "- Units layer is locked\n";
                        debugText += "- No unit anchor points were found\n";
                    }
                    debugText += "\n" + separator;
    
                    try {
                        var w = new Window("dialog", "Unit Debug Output");
                        w.preferredSize = [900, 700];
    
                        var debugTextbox = w.add("edittext", undefined, "", {multiline: true, scrolling: true});
                        debugTextbox.preferredSize = [880, 650];
                        debugTextbox.text = debugText;
    
                        var btnGroup = w.add("group");
                        btnGroup.alignment = "center";
                        var closeBtn = btnGroup.add("button", undefined, "Close", {name: "ok"});
    
                        w.show();
                    } catch (e) {
                        alert("Debug dialog error: " + e + "\n\nOutput:\n\n" + debugText.substring(0, 1000));
                    }
                    */
                }

                function cleanupLayers_local(docParam) {
                    var layers = ["Thermostats", "Units", "Secondary Exhaust Registers", "Exhaust Registers", "Orange Register", "Rectangular Registers", "Circular Registers", "Square Registers"];
                    for (var i = 0; i < layers.length; i++) {
                        var layer = getLayerByName_local(docParam, layers[i]);
                        if (!layer || layer.locked) continue;
                        for (var j = layer.compoundPathItems.length - 1; j >= 0; j--) {
                            try { layer.compoundPathItems[j].release(); } catch (e) { }
                        }
                        for (var k = layer.groupItems.length - 1; k >= 0; k--) {
                            try { if (layer.groupItems[k].clipped) layer.groupItems[k].release(); } catch (e) { }
                        }
                        var allPoints = [];
                        for (var m = 0; m < layer.pathItems.length; m++) {
                            var path = layer.pathItems[m];
                            if (!path.locked && path.pathPoints.length > 1) {
                                for (var n = 0; n < path.pathPoints.length; n++) {
                                    allPoints.push([path.pathPoints[n].anchor[0], path.pathPoints[n].anchor[1]]);
                                }
                            }
                        }
                        for (var p = layer.pathItems.length - 1; p >= 0; p--) {
                            if (!layer.pathItems[p].locked && layer.pathItems[p].pathPoints.length > 1) {
                                try { layer.pathItems[p].remove(); } catch (e) { }
                            }
                        }
                        for (var q = 0; q < allPoints.length; q++) {
                            try {
                                var pt = layer.pathItems.add();
                                pt.setEntirePath([allPoints[q]]);
                                pt.filled = false;
                                pt.stroked = false;
                            } catch (e) { }
                        }
                    }
                }

                function drawFrame_local(docParam, frameLayerParam, artboard) {
                    if (!frameLayerParam || frameLayerParam.locked) return;
                    var R = artboard.artboardRect,
                        left = R[0], top = R[1],
                        width = R[2] - R[0],
                        height = R[1] - R[3];
                    var existing = null;
                    for (var i = 0; i < docParam.pathItems.length; i++) {
                        var pi = docParam.pathItems[i];
                        try {
                            if (pi.layer === frameLayerParam && pi.name === "Frame") { existing = pi; break; }
                        } catch (e) {
                            // Item may be invalid after copy/paste
                        }
                    }
                    if (existing && !existing.locked) {
                        try {
                            existing.setEntirePath([
                                [left, top],
                                [left + width, top],
                                [left + width, top - height],
                                [left, top - height]
                            ]);
                        } catch (e) { }
                    } else if (!existing) {
                        try {
                            var rect = docParam.pathItems.rectangle(top, left, width, height);
                            rect.stroked = false;
                            rect.filled = false;
                            rect.name = "Frame";
                            rect.note = "Frame";
                            rect.move(frameLayerParam, ElementPlacement.PLACEATBEGINNING);
                        } catch (e) { }
                    }
                }

                function inferGlobalScaleFallback_local(docParam) {
                    var counts = {};
                    var bestAnyScale = null, bestAnyCount = 0;
                    var bestNonDefaultScale = null, bestNonDefaultCount = 0;

                    function registerScale(val) {
                        if (typeof val !== 'number' || !isFinite(val) || val <= 0) return;
                        var rounded = Math.round(val * 1000) / 1000;
                        var key = rounded.toFixed(3);
                        var cnt = counts[key] ? counts[key] + 1 : 1;
                        counts[key] = cnt;
                        if (cnt > bestAnyCount) {
                            bestAnyCount = cnt;
                            bestAnyScale = rounded;
                        }
                        if (Math.abs(rounded - 100) > 0.01 && cnt > bestNonDefaultCount) {
                            bestNonDefaultCount = cnt;
                            bestNonDefaultScale = rounded;
                        }
                    }

                    try {
                        for (var pi = 0; pi < docParam.placedItems.length; pi++) {
                            registerScale(getPlacedScale(docParam.placedItems[pi]));
                        }
                    } catch (e) { }

                    try {
                        for (var key in initialPlacedTransforms) {
                            if (!initialPlacedTransforms.hasOwnProperty(key)) continue;
                            var info = initialPlacedTransforms[key];
                            if (info && typeof info.scale === 'number' && isFinite(info.scale)) {
                                registerScale(info.scale);
                            }
                        }
                    } catch (e) { }

                    if (bestNonDefaultScale !== null) return bestNonDefaultScale;
                    return bestAnyScale;
                }

                function placeLinkedComponents_local(docParam, selectedPaths) {
                    // *** DEFAULT SCALE FOR NEW DUCTWORK PARTS: 50% ***
                    var DEFAULT_PLACEMENT_SCALE = 50;

                    var globalScale = getCurrentScaleFactor_local(docParam);
                    if (!isFinite(globalScale) || globalScale <= 0) globalScale = DEFAULT_PLACEMENT_SCALE;

                    var inferredScale = inferGlobalScaleFallback_local(docParam);
                    if (inferredScale !== null) {
                        // Only override if we detected a valid non-default scale from existing items
                        if (Math.abs(globalScale - DEFAULT_PLACEMENT_SCALE) < 0.01 && Math.abs(inferredScale - globalScale) > 0.5) {
                            if (inferredScale > globalScale) {
                                globalScale = inferredScale;
                            }
                        }
                    }

                    // If globalScale is still at default 100% (from old documents), use new default of 50%
                    if (Math.abs(globalScale - 100) < 0.01) {
                        globalScale = DEFAULT_PLACEMENT_SCALE;
                        addDebug("[SCALE] Overriding legacy 100% scale with new default: " + DEFAULT_PLACEMENT_SCALE + "%");
                    }

                    addDebug("");
                    addDebug("=== DUCTWORK COMPONENT PLACEMENT ===");
                    addDebug("Component Files Path: " + COMPONENT_FILES_PATH);
                    addDebug("Default Placement Scale: " + DEFAULT_PLACEMENT_SCALE + "%");
                    addDebug("Global Scale: " + globalScale);
                    addDebug("Use Emory Assets: " + USE_EMORY_ASSETS);
                    addDebug("");

                    // PERF: Cache ignored anchors ONCE before processing all component types (Gemini optimization)
                    var CACHED_IGNORED_ANCHORS = [];
                    var possibleLayerNames = ["Ignore", "Ignored", "ignore", "ignored"];
                    var ignoredLayers = [];
                    for (var layerIdx = 0; layerIdx < docParam.layers.length; layerIdx++) {
                        var checkLayer = docParam.layers[layerIdx];
                        for (var nameIdx = 0; nameIdx < possibleLayerNames.length; nameIdx++) {
                            if (checkLayer.name === possibleLayerNames[nameIdx]) {
                                ignoredLayers.push(checkLayer);
                                break;
                            }
                        }
                    }
                    if (ignoredLayers.length > 0) {
                        function collectIgnoredAnchors_cached(container) {
                            try {
                                for (var i = 0; i < container.pathItems.length; i++) {
                                    try {
                                        var path = container.pathItems[i];
                                        for (var j = 0; j < path.pathPoints.length; j++) {
                                            try {
                                                var anchor = path.pathPoints[j].anchor;
                                                CACHED_IGNORED_ANCHORS.push([anchor[0], anchor[1]]);
                                            } catch (e) { }
                                        }
                                    } catch (e) { }
                                }
                                for (var p = 0; p < container.placedItems.length; p++) {
                                    try {
                                        var placed = container.placedItems[p];
                                        var gb = placed.geometricBounds;
                                        CACHED_IGNORED_ANCHORS.push([(gb[0] + gb[2]) / 2, (gb[1] + gb[3]) / 2]);
                                    } catch (e) { }
                                }
                                for (var k = 0; k < container.groupItems.length; k++) {
                                    try { collectIgnoredAnchors_cached(container.groupItems[k]); } catch (e) { }
                                }
                                if (container.layers) {
                                    for (var s = 0; s < container.layers.length; s++) {
                                        try { collectIgnoredAnchors_cached(container.layers[s]); } catch (e) { }
                                    }
                                }
                            } catch (e) { }
                        }
                        for (var igIdx = 0; igIdx < ignoredLayers.length; igIdx++) {
                            collectIgnoredAnchors_cached(ignoredLayers[igIdx]);
                        }
                        addDebug("[PERF] Cached " + CACHED_IGNORED_ANCHORS.length + " ignored anchors (collected once, reused 8x)");
                    }

                    for (var i = 0; i < COMPONENT_TYPES.length; i++) {
                        placeComponentAtAnchorPoints_local(docParam, COMPONENT_TYPES[i], globalScale, selectedPaths, CACHED_IGNORED_ANCHORS);
                    }
                }

                function placeComponentAtAnchorPoints_local(docParam, type, globalScale, selectedPaths, cachedIgnoredAnchors) {
                    var layer = getLayerByName_local(docParam, type.layer);
                    if (!layer || layer.locked) {
                        addDebug("[" + type.name + "] Layer '" + type.layer + "' not found or locked");
                        return;
                    }
                    var file = new File(COMPONENT_FILES_PATH + type.file);
                    if (!file.exists) {
                        addDebug("[" + type.name + "] File NOT FOUND: " + type.file);
                        return;
                    }

                    addDebug("");
                    addDebug("========== " + type.name + " ==========");
                    addDebug("[" + type.name + "] Collecting anchors from layer: " + type.layer);
                    var anchorPts = collectAnchorPoints_local(docParam, layer, selectedPaths);
                    addDebug("[" + type.name + "] Collected " + anchorPts.length + " anchor points");

                    // PERF: Use cached ignored anchors instead of re-collecting (Gemini optimization)
                    var ignoredAnchors = cachedIgnoredAnchors || [];
                    var IGNORED_DIST_LOCAL = 4;

                    var keySet = {};
                    for (var aIdx = 0; aIdx < anchorPts.length; aIdx++) {
                        var ap = anchorPts[aIdx].pos;
                        keySet[ap[0].toFixed(2) + "_" + ap[1].toFixed(2)] = true;
                    }

                    if (!layer.locked) {
                        var removedCount = 0;
                        for (var pi = docParam.placedItems.length - 1; pi >= 0; pi--) {
                            var itm = docParam.placedItems[pi];
                            try {
                                if (itm.layer === layer && !itm.locked) {
                                    var gb = itm.geometricBounds;
                                    var centerX = (gb[0] + gb[2]) / 2;
                                    var centerY = (gb[1] + gb[3]) / 2;
                                    var centerPos = [centerX, centerY];

                                    // Remove if near ignored anchor
                                    var isIgnored = false;
                                    for (var ignIdx = 0; ignIdx < ignoredAnchors.length; ignIdx++) {
                                        var dx = centerPos[0] - ignoredAnchors[ignIdx][0];
                                        var dy = centerPos[1] - ignoredAnchors[ignIdx][1];
                                        if (Math.sqrt(dx * dx + dy * dy) <= IGNORED_DIST_LOCAL) {
                                            isIgnored = true;
                                            break;
                                        }
                                    }
                                    if (isIgnored) {
                                        addDebug("[" + type.name + "] Removing PlacedItem at ignored location: " + centerX.toFixed(2) + "_" + centerY.toFixed(2));
                                        try {
                                            itm.remove();
                                            removedCount++;
                                        } catch (e) { }
                                        continue;
                                    }

                                    var ck = centerX.toFixed(2) + "_" + centerY.toFixed(2);
                                    if (!keySet[ck]) continue;
                                    if (itm.name.indexOf(type.name) === -1) {
                                        try { itm.remove(); } catch (e) { }
                                    }
                                }
                            } catch (e) {
                                // Item may be invalid after copy/paste
                            }
                        }
                        if (removedCount > 0) {
                            addDebug("[" + type.name + "] Removed " + removedCount + " PlacedItems at ignored locations");
                        }
                    }

                    function getItemScale_local(item) {
                        try {
                            var m = item.matrix;
                            return Math.sqrt((m.mValueA * m.mValueA) + (m.mValueB * m.mValueB)) * 100;
                        } catch (e) {
                            return 100;
                        }
                    }

                    function normalizeSignedAngle_local(angle) {
                        while (angle > 180) angle -= 360;
                        while (angle < -180) angle += 360;
                        return angle;
                    }

                    function rotatePageItemToAbsolute(item, targetAngle, baseAngle) {
                        if (!item) return;
                        if (typeof targetAngle !== 'number' || !isFinite(targetAngle)) targetAngle = 0;
                        if (typeof baseAngle !== 'number' || !isFinite(baseAngle)) baseAngle = 0;

                        var targetAbsolute = normalizeSignedAngle_local(targetAngle);
                        var baseAbsolute = normalizeSignedAngle_local(baseAngle);

                        var storedAbsoluteRaw = null;
                        var storedAbsoluteSigned = null;
                        try {
                            storedAbsoluteRaw = getPlacedRotation(item);
                        } catch (eStored) {
                            storedAbsoluteRaw = null;
                        }
                        if (storedAbsoluteRaw !== null && isFinite(storedAbsoluteRaw)) {
                            storedAbsoluteSigned = normalizeSignedAngle_local(storedAbsoluteRaw);
                        }

                        var matrixAbsolute = null;
                        try {
                            var m = item.matrix;
                            matrixAbsolute = Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
                        } catch (eMatrix) {
                            matrixAbsolute = null;
                        }

                        var appliedAbsolute = storedAbsoluteSigned;
                        if (appliedAbsolute === null || !isFinite(appliedAbsolute)) {
                            if (matrixAbsolute !== null && isFinite(matrixAbsolute)) {
                                appliedAbsolute = normalizeSignedAngle_local(matrixAbsolute);
                            } else {
                                appliedAbsolute = baseAbsolute;
                            }
                        }

                        var delta = normalizeSignedAngle_local(targetAbsolute - appliedAbsolute);
                        if (Math.abs(delta) > 0.1) {
                            try { item.rotate(delta, true, true, true, true, Transformation.CENTER); } catch (eRotate) { }
                        }

                        var absoluteUnsigned = normalizeAngle(targetAbsolute);
                        setPlacedRotation(item, absoluteUnsigned);

                        function formatAngle(val) {
                            return (typeof val === 'number' && isFinite(val)) ? (Math.round(val * 1000) / 1000) : "null";
                        }
                        var nameForLog = "";
                        try {
                            nameForLog = item.name || item.typename || "";
                        } catch (eName) {
                            nameForLog = "";
                        }
                        logDebug("[RotatePlacedItem] name=" + nameForLog +
                            " base=" + formatAngle(baseAbsolute) +
                            " target=" + formatAngle(targetAbsolute) +
                            " stored=" + formatAngle(storedAbsoluteSigned) +
                            " matrix=" + formatAngle(matrixAbsolute) +
                            " applied=" + formatAngle(appliedAbsolute) +
                            " delta=" + formatAngle(delta));
                    }

                    function centerPageItemAt_local(item, anchor) {
                        if (!item) return;
                        try {
                            var b = item.geometricBounds;
                            var cx = (b[0] + b[2]) / 2;
                            var cy = (b[1] + b[3]) / 2;
                            var dx = anchor[0] - cx;
                            var dy = anchor[1] - cy;
                            if (Math.abs(dx) > 0.01 || Math.abs(dy) > 0.01) {
                                item.translate(dx, dy, true, true, true, true);
                            }
                        } catch (e) { }
                    }

                    var customTransforms = {}; // Store custom scale and rotation overrides
                    var existingItems = {}; // Map of key -> existing placed item

                    // First pass: collect anchor point data to get expected rotations
                    var anchorRotations = {};
                    for (var ai = 0; ai < anchorPts.length; ai++) {
                        var aInfo = anchorPts[ai];
                        var aKey = aInfo.pos[0].toFixed(2) + "_" + aInfo.pos[1].toFixed(2);
                        anchorRotations[aKey] = aInfo.rotation;
                    }

                    // Build list of existing items with their positions (tolerance-based matching)
                    var POSITION_TOLERANCE = 10.0; // pixels - tolerance for matching existing items to anchor points (MUST match CLOSE_DIST so units behave like registers)
                    for (var i = 0; i < docParam.placedItems.length; i++) {
                        var itm = docParam.placedItems[i];
                        try {
                            if (itm.layer === layer && !itm.locked && itm.name.indexOf(type.name) !== -1) {
                                var boundsExisting = itm.geometricBounds;
                                var centerXExisting = (boundsExisting[0] + boundsExisting[2]) / 2;
                                var centerYExisting = (boundsExisting[1] + boundsExisting[3]) / 2;

                                // Find nearest anchor point within tolerance
                                var nearestKey = null;
                                var nearestDist = POSITION_TOLERANCE;
                                for (var ak = 0; ak < anchorPts.length; ak++) {
                                    var anchorPos = anchorPts[ak].pos;
                                    var dx = anchorPos[0] - centerXExisting;
                                    var dy = anchorPos[1] - centerYExisting;
                                    var dist = Math.sqrt(dx * dx + dy * dy);
                                    if (dist < nearestDist) {
                                        nearestDist = dist;
                                        nearestKey = anchorPos[0].toFixed(2) + "_" + anchorPos[1].toFixed(2);
                                    }
                                }

                                // Only add if we found a nearby anchor point
                                if (nearestKey && !existingItems[nearestKey]) {
                                    existingItems[nearestKey] = itm;
                                }

                                var key = nearestKey;
                                if (!key) continue;
                                if (!customTransforms[key]) customTransforms[key] = {};
                                var entry = customTransforms[key];

                                // Get rotations and scales
                                var actualRot = null;
                                try {
                                    var m = itm.matrix;
                                    actualRot = Math.atan2(m.mValueB, m.mValueA) * (180 / Math.PI);
                                } catch (e) { }

                                var actualScale = getItemScale_local(itm);
                                var storedRotAbsolute = getPlacedRotation(itm);  // ABSOLUTE rotation target
                                var storedBaseRot = getPlacedBaseRotation(itm);
                                var storedScale = getPlacedScale(itm);

                                var rawNote = itm.note || "";
                                logDebug("RAW NOTE at " + key + ": " + rawNote);

                                var hasInitialTransform = initialPlacedTransforms[key] !== undefined;
                                var initialRot = hasInitialTransform ? initialPlacedTransforms[key].rotation : null;
                                var initialScale = hasInitialTransform ? initialPlacedTransforms[key].scale : null;

                                var anchorRotForKey = anchorRotations.hasOwnProperty(key) ? anchorRotations[key] : null;

                                var absoluteRotation = null;
                                if (storedRotAbsolute !== null && isFinite(storedRotAbsolute)) {
                                    absoluteRotation = storedRotAbsolute;
                                } else if (initialRot !== null && isFinite(initialRot)) {
                                    absoluteRotation = initialRot;
                                } else if (actualRot !== null && isFinite(actualRot)) {
                                    absoluteRotation = normalizeAngle(actualRot);
                                }
                                entry.absoluteRotation = absoluteRotation;

                                var baseRotation = storedBaseRot;
                                if (baseRotation === null || !isFinite(baseRotation)) {
                                    if (anchorRotForKey !== null && isFinite(anchorRotForKey) && actualRot !== null && isFinite(actualRot)) {
                                        baseRotation = normalizeSignedAngle_local(actualRot - anchorRotForKey);
                                    } else if (absoluteRotation !== null && isFinite(absoluteRotation) && actualRot !== null && isFinite(actualRot)) {
                                        baseRotation = normalizeSignedAngle_local(actualRot - absoluteRotation);
                                    } else if (absoluteRotation !== null && isFinite(absoluteRotation) && anchorRotForKey !== null && isFinite(anchorRotForKey)) {
                                        baseRotation = normalizeSignedAngle_local(absoluteRotation - anchorRotForKey);
                                    } else if (actualRot !== null && isFinite(actualRot)) {
                                        baseRotation = normalizeSignedAngle_local(actualRot);
                                    } else if (initialRot !== null && isFinite(initialRot)) {
                                        baseRotation = normalizeSignedAngle_local(initialRot);
                                    } else {
                                        baseRotation = 0;
                                    }
                                } else {
                                    baseRotation = normalizeSignedAngle_local(baseRotation);
                                }

                                entry.baseRotation = baseRotation;
                                if (storedBaseRot === null || !isFinite(storedBaseRot)) {
                                    try { setPlacedBaseRotation(itm, baseRotation); } catch (e) { }
                                }

                                if (storedScale !== null && isFinite(storedScale)) {
                                    entry.scale = storedScale;
                                } else if (initialScale !== null && isFinite(initialScale)) {
                                    entry.scale = initialScale;
                                } else if (actualScale !== null && isFinite(actualScale)) {
                                    entry.scale = actualScale;
                                }

                                logDebug("CAPTURE at " + key + ": actualRot=" + actualRot +
                                    " anchorRot=" + anchorRotForKey +
                                    " absoluteRotation=" + absoluteRotation +
                                    " baseRotation=" + baseRotation);
                            }
                        } catch (e) {
                            // Item may be invalid after copy/paste
                        }
                    }

                    var PLACEMENT_BATCH_SIZE = 15; // Update progress every 15 placements
                    for (var j = 0; j < anchorPts.length; j++) {
                        // BATCH: Update progress between batches
                        if (j > 0 && j % PLACEMENT_BATCH_SIZE === 0) {
                            if (progressWin && progressLabel) {
                                try {
                                    progressLabel.text = type.name + "... " + j + "/" + anchorPts.length;
                                    progressWin.update();
                                } catch (e) { }
                            }
                        }
                        var info = anchorPts[j];
                        var a = info.pos;
                        var rotation = info.rotation;
                        var key = a[0].toFixed(2) + "_" + a[1].toFixed(2);

                        // Check if this location has custom transforms
                        var customScale = null;
                        var baseRotation = null;
                        var fallbackRotation = null;
                        if (customTransforms[key]) {
                            customScale = customTransforms[key].scale;
                            baseRotation = customTransforms[key].baseRotation;
                            fallbackRotation = customTransforms[key].absoluteRotation;
                        }

                        var targetItem = null;
                        var createdNew = false;

                        // DEBUG OUTPUT FOR UNITS - COMMENTED OUT FOR PERFORMANCE
                        /*
                        if (type.layer === "Units") {
                            UNIT_DEBUG.push("---");
                            UNIT_DEBUG.push("Anchor: " + key);
                            UNIT_DEBUG.push("Position: [" + a[0].toFixed(2) + ", " + a[1].toFixed(2) + "]");
                            UNIT_DEBUG.push("existingItems has key? " + (existingItems[key] ? "YES" : "NO"));
                            UNIT_DEBUG.push("POSITION_TOLERANCE: " + POSITION_TOLERANCE + "px");
    
                            // Count existing units near this position
                            var nearbyCount = 0;
                            for (var pi = 0; pi < docParam.placedItems.length; pi++) {
                                var checkItem = docParam.placedItems[pi];
                                try {
                                    if (checkItem.layer === layer && !checkItem.locked && checkItem.name.indexOf("Unit") !== -1) {
                                        var gb = checkItem.geometricBounds;
                                        var cx = (gb[0] + gb[2]) / 2;
                                        var cy = (gb[1] + gb[3]) / 2;
                                        var dx = a[0] - cx;
                                        var dy = a[1] - cy;
                                        var dist = Math.sqrt(dx * dx + dy * dy);
                                        if (dist < 20) {  // Show all units within 20px
                                            nearbyCount++;
                                            var scaleInfo = getItemScale_local(checkItem);
                                            UNIT_DEBUG.push("  Nearby Unit #" + nearbyCount + ": dist=" + dist.toFixed(2) + "px at [" + cx.toFixed(2) + "," + cy.toFixed(2) + "] scale=" + (scaleInfo ? scaleInfo.toFixed(1) : "?") + "%");
                                        }
                                    }
                                } catch (e) {}
                            }
                            UNIT_DEBUG.push("Total nearby units: " + nearbyCount);
                        }
                        */

                        // Check if an item already exists at this location
                        if (existingItems[key]) {
                            targetItem = existingItems[key];
                            createdNew = false;

                            // Update file link - this preserves custom rotations and scaling (works like manual relink)
                            try {
                                targetItem.file = file;
                                addDebug("[" + type.name + "] Updated file link at " + key + " (preserving custom transforms)");
                                if (type.layer === "Units") {
                                    UNIT_DEBUG.push("ACTION: Updated existing unit - preserving transforms");
                                }
                            } catch (e) {
                                addDebug("[" + type.name + "] Failed to update file link at " + key + ": " + e);
                                if (type.layer === "Units") {
                                    UNIT_DEBUG.push("ERROR: Failed to update file link: " + e);
                                }
                            }
                        } else if (!layer.locked) {
                            // Create new placed item
                            try {
                                var placed = layer.placedItems.add();
                                placed.file = file;
                                try { placed.relink(file); } catch (e) { }
                                try { placed.update(); } catch (e) { }
                                var b0 = placed.geometricBounds;
                                var w0 = b0[2] - b0[0], h0 = b0[1] - b0[3];
                                placed.position = [a[0] - w0 / 2, a[1] + h0 / 2];
                                placed.name = type.name + " (Linked)";

                                targetItem = placed;
                                createdNew = true;
                                addDebug("[" + type.name + "] Created new item at " + key);
                                if (type.layer === "Units") {
                                    UNIT_DEBUG.push("ACTION: Created NEW unit (will apply full size + default rotation)");
                                }
                            } catch (e) {
                                if (type.layer === "Units") {
                                    UNIT_DEBUG.push("ERROR: Failed to create new unit: " + e);
                                }
                            }
                        }

                        // Only apply rotation and scale to NEW items
                        if (targetItem && !targetItem.locked && createdNew) {
                            if (typeof baseRotation !== 'number' || !isFinite(baseRotation)) {
                                try {
                                    var mBase = targetItem.matrix;
                                    baseRotation = Math.atan2(mBase.mValueB, mBase.mValueA) * (180 / Math.PI);
                                } catch (e) {
                                    baseRotation = 0;
                                }
                                baseRotation = normalizeSignedAngle_local(baseRotation || 0);
                            }

                            if (!customTransforms[key]) customTransforms[key] = {};
                            customTransforms[key].baseRotation = baseRotation;

                            try { setPlacedBaseRotation(targetItem, baseRotation); } catch (e) { }

                            var desiredRotation = null;
                            if (typeof rotation === 'number' && isFinite(rotation)) {
                                desiredRotation = rotation;
                            } else if (typeof fallbackRotation === 'number' && isFinite(fallbackRotation)) {
                                desiredRotation = fallbackRotation;
                            } else if (typeof baseRotation === 'number' && isFinite(baseRotation)) {
                                desiredRotation = normalizeAngle(baseRotation);
                            }

                            // Debug logging
                            addDebug("[PLACE " + type.name + " at " + key + "]");
                            addDebug("  rotation (from anchor) = " + (rotation !== null ? rotation + "°" : "null"));
                            addDebug("  fallbackRotation = " + (fallbackRotation !== null ? fallbackRotation + "°" : "null"));
                            addDebug("  baseRotation = " + (baseRotation !== null ? baseRotation + "°" : "null"));
                            addDebug("  desiredRotation (FINAL) = " + (desiredRotation !== null ? desiredRotation + "°" : "null"));

                            // Apply rotation and scale ONLY for NEW items to preserve custom transforms on existing items
                            if (createdNew) {
                                // Apply rotation for NEW items
                                // Skip rotation for Square Registers if SKIP_REGISTER_ROTATION is enabled
                                addDebug("  [SKIP-ROT-CHECK] SKIP_REGISTER_ROTATION=" + SKIP_REGISTER_ROTATION + ", type.layer='" + type.layer + "'");
                                var shouldSkipRotation = (SKIP_REGISTER_ROTATION && type.layer === "Square Registers");
                                addDebug("  [SKIP-ROT-CHECK] shouldSkipRotation=" + shouldSkipRotation);
                                if (type.layer !== "Thermostats" && desiredRotation !== null && !shouldSkipRotation) {
                                    addDebug("  APPLYING rotation: " + desiredRotation + "° (base: " + baseRotation + "°)");
                                    rotatePageItemToAbsolute(targetItem, desiredRotation, baseRotation);
                                    customTransforms[key].absoluteRotation = desiredRotation;

                                    // Fix bounding box orientation by re-assigning the file reference
                                    try {
                                        var currentFile = targetItem.file;
                                        targetItem.file = currentFile;
                                    } catch (e) { }
                                } else if (shouldSkipRotation) {
                                    addDebug("  SKIPPING rotation for " + type.name + " (No Register Rotation option enabled)");
                                    desiredRotation = 0; // Set to 0 so metadata stores 0 instead of the ductwork angle
                                }

                                // Apply scale for NEW items
                                var scaleToApply = globalScale;
                                if (typeof customScale === 'number' && isFinite(customScale)) {
                                    scaleToApply = customScale;
                                }

                                if (scaleToApply !== 100) {
                                    try {
                                        targetItem.resize(scaleToApply, scaleToApply, true, true, true, true, scaleToApply, Transformation.CENTER);
                                        setPlacedScale(targetItem, scaleToApply);
                                        addDebug("  APPLIED scale: " + scaleToApply + "%");
                                    } catch (e) { }
                                }

                                centerPageItemAt_local(targetItem, a);
                            } else {
                                addDebug("  EXISTING item - preserving custom transforms");
                            }

                            // PERF: Removed targetItem.update() - may cause per-item redraws (Gemini optimization)

                            // Store rotation metadata AFTER all item operations (file, scale, center)
                            // to prevent operations from wiping the .note property
                            // Write metadata for BOTH new and existing items if a rotation override was specified
                            if (type.layer !== "Thermostats" && desiredRotation !== null) {
                                try {
                                    // PERF: Removed verbose logging and verification read (Gemini optimization)
                                    var placedMeta2 = MDUX_getMetadata(targetItem) || {};
                                    placedMeta2.MDUX_RotationOverride = desiredRotation;
                                    MDUX_setMetadata(targetItem, placedMeta2);
                                } catch (eStoreMeta) {
                                    addDebug("  ERROR storing rotation metadata: " + eStoreMeta);
                                }
                            }
                        }
                    }
                }

                function collectAnchorPoints_local(docParam, layer, selectedPaths) {
                    var pts = [], seen = {};
                    if (!layer || layer.locked) return pts;

                    // Collect ignored anchors first (same logic as main script)
                    var ignoredAnchors = [];
                    var IGNORED_DIST_LOCAL = 4; // Same threshold as main script
                    var possibleLayerNames = ["Ignore", "Ignored", "ignore", "ignored"];
                    var ignoredLayers = [];

                    // Collect ALL layers that match any of the possible names
                    for (var layerIdx = 0; layerIdx < docParam.layers.length; layerIdx++) {
                        var checkLayer = docParam.layers[layerIdx];
                        for (var nameIdx = 0; nameIdx < possibleLayerNames.length; nameIdx++) {
                            if (checkLayer.name === possibleLayerNames[nameIdx]) {
                                ignoredLayers.push(checkLayer);
                                break;
                            }
                        }
                    }

                    if (ignoredLayers.length > 0) {
                        // PERF: Removed per-item logging (Gemini optimization)
                        function collectIgnoredAnchors(container) {
                            try {
                                // Collect anchor points from PathItems (no logging)
                                for (var i = 0; i < container.pathItems.length; i++) {
                                    try {
                                        var path = container.pathItems[i];
                                        for (var j = 0; j < path.pathPoints.length; j++) {
                                            try {
                                                var anchor = path.pathPoints[j].anchor;
                                                ignoredAnchors.push([anchor[0], anchor[1]]);
                                            } catch (e) { }
                                        }
                                    } catch (e) { }
                                }
                                // Collect center points from PlacedItems
                                for (var p = 0; p < container.placedItems.length; p++) {
                                    try {
                                        var placed = container.placedItems[p];
                                        var gb = placed.geometricBounds;
                                        var centerX = (gb[0] + gb[2]) / 2;
                                        var centerY = (gb[1] + gb[3]) / 2;
                                        ignoredAnchors.push([centerX, centerY]);
                                    } catch (e) { }
                                }
                                for (var k = 0; k < container.groupItems.length; k++) {
                                    try { collectIgnoredAnchors(container.groupItems[k]); } catch (e) { }
                                }
                                if (container.layers) {
                                    for (var s = 0; s < container.layers.length; s++) {
                                        try { collectIgnoredAnchors(container.layers[s]); } catch (e) { }
                                    }
                                }
                            } catch (e) { }
                        }
                        for (var igIdx = 0; igIdx < ignoredLayers.length; igIdx++) {
                            collectIgnoredAnchors(ignoredLayers[igIdx]);
                        }
                        addDebug("[ANCHOR COLLECTION] Found " + ignoredAnchors.length + " ignored anchors across " + ignoredLayers.length + " layers");
                    }

                    // Helper to check if point is ignored (within IGNORED_DIST of any ignored anchor)
                    function isPointIgnored_local(pos) {
                        var closestDist = Infinity;
                        var closestIgnored = null;
                        for (var i = 0; i < ignoredAnchors.length; i++) {
                            var dx = pos[0] - ignoredAnchors[i][0];
                            var dy = pos[1] - ignoredAnchors[i][1];
                            var dist = Math.sqrt(dx * dx + dy * dy);
                            if (dist < closestDist) {
                                closestDist = dist;
                                closestIgnored = ignoredAnchors[i];
                            }
                            if (dist <= IGNORED_DIST_LOCAL) {
                                addDebug("[IGNORE CHECK] Point " + pos[0].toFixed(2) + "," + pos[1].toFixed(2) + " is " + dist.toFixed(2) + "px from ignored " + ignoredAnchors[i][0].toFixed(2) + "," + ignoredAnchors[i][1].toFixed(2) + " - FILTERED");
                                return true;
                            }
                        }
                        if (closestIgnored && closestDist < 20) {
                            addDebug("[IGNORE CHECK] Point " + pos[0].toFixed(2) + "," + pos[1].toFixed(2) + " closest ignored is " + closestDist.toFixed(2) + "px away at " + closestIgnored[0].toFixed(2) + "," + closestIgnored[1].toFixed(2) + " - NOT filtered (threshold is " + IGNORED_DIST_LOCAL + "px)");
                        }
                        return false;
                    }

                    // Build list of endpoints from selected paths for proximity filtering
                    var selectedEndpoints = [];
                    var useProximityFilter = selectedPaths && selectedPaths.length > 0;
                    if (useProximityFilter) {
                        addDebug("[ANCHOR COLLECTION] Proximity filter active with " + selectedPaths.length + " selected paths");
                        for (var sp = 0; sp < selectedPaths.length; sp++) {
                            try {
                                var item = selectedPaths[sp];
                                // Validate item is still valid by accessing typename
                                if (!item) {
                                    addDebug("[ANCHOR COLLECTION] Path " + sp + " is null/undefined - skipping");
                                    continue;
                                }
                                var itemType = item.typename; // This will throw if item is invalid/deleted

                                // Handle CompoundPathItems - iterate through child pathItems
                                // Include ALL path vertices, not just endpoints (so T-junctions and internal anchors match)
                                if (itemType === "CompoundPathItem" && item.pathItems) {
                                    addDebug("[ANCHOR COLLECTION] Path " + sp + " is CompoundPathItem with " + item.pathItems.length + " sub-paths");
                                    for (var cpIdx = 0; cpIdx < item.pathItems.length; cpIdx++) {
                                        var childPath = item.pathItems[cpIdx];
                                        if (childPath && childPath.pathPoints && childPath.pathPoints.length > 0) {
                                            // Add ALL vertices from this sub-path
                                            for (var cpPtIdx = 0; cpPtIdx < childPath.pathPoints.length; cpPtIdx++) {
                                                var cpPt = childPath.pathPoints[cpPtIdx].anchor;
                                                selectedEndpoints.push([cpPt[0], cpPt[1]]);
                                            }
                                        }
                                    }
                                }
                                // Handle regular PathItems
                                else if (itemType === "PathItem" && item.pathPoints && item.pathPoints.length > 0) {
                                    // Add ALL vertices from this path (not just endpoints)
                                    // This ensures T-junctions, internal anchors, etc. are matched
                                    for (var ptIdx = 0; ptIdx < item.pathPoints.length; ptIdx++) {
                                        var pt = item.pathPoints[ptIdx].anchor;
                                        selectedEndpoints.push([pt[0], pt[1]]);
                                    }
                                }
                            } catch (ePathAccess) {
                                // Path was deleted or is invalid - skip it
                                addDebug("[ANCHOR COLLECTION] Path " + sp + " is invalid/deleted - skipping: " + ePathAccess.message);
                            }
                        }
                        addDebug("[ANCHOR COLLECTION] Built " + selectedEndpoints.length + " vertices from selected paths");
                    } else {
                        addDebug("[ANCHOR COLLECTION] No proximity filter - collecting all anchors from layer");
                    }

                    // Helper to check if a point is near any selected path endpoint
                    function isNearSelectedPath(pos) {
                        if (!useProximityFilter) return true; // No filter active
                        var PROXIMITY_THRESHOLD = 10; // CLOSE_DIST constant
                        for (var i = 0; i < selectedEndpoints.length; i++) {
                            var ep = selectedEndpoints[i];
                            var dx = pos[0] - ep[0];
                            var dy = pos[1] - ep[1];
                            var distSq = dx * dx + dy * dy;
                            if (distSq <= PROXIMITY_THRESHOLD * PROXIMITY_THRESHOLD) {
                                return true;
                            }
                        }
                        return false;
                    }

                    function processPath(p) {
                        if (!p || p.guides || p.clipping) return;

                        var process = shouldProcessPath(p);
                        if (!process) {
                            if (!isAnchorPointPath(p)) return;
                            markCreatedPath(p);
                            process = true;
                        }

                        if (p.locked) return;

                        // Priority: global rotation override > path rotation override > stored point rotation
                        // This ensures re-processing with a new angle override updates existing parts correctly
                        var rotation = null;
                        var rotationSource = "";
                        if (typeof GLOBAL_ROTATION_OVERRIDE === 'number' && GLOBAL_ROTATION_OVERRIDE !== null) {
                            rotation = GLOBAL_ROTATION_OVERRIDE;
                            rotationSource = "GLOBAL_OVERRIDE";
                        } else {
                            var pathOverride = getRotationOverride(p);
                            var pointRotation = getPointRotation(p);
                            if (pathOverride !== null) {
                                rotation = pathOverride;
                                rotationSource = "PATH_OVERRIDE";
                            } else if (pointRotation !== null) {
                                rotation = pointRotation;
                                rotationSource = "POINT_METADATA";
                            } else {
                                rotationSource = "NONE";
                            }
                        }

                        for (var j = 0; j < p.pathPoints.length; j++) {
                            var a = p.pathPoints[j].anchor;
                            var key = a[0].toFixed(2) + "_" + a[1].toFixed(2);
                            if (!seen[key]) {
                                var anchorPos = [a[0], a[1]];
                                // Skip if anchor is ignored (within IGNORED_DIST of any ignored anchor)
                                if (isPointIgnored_local(anchorPos)) {
                                    addDebug("[ANCHOR " + key + "] SKIPPED (on Ignored layer)");
                                    continue;
                                }
                                // Only include anchor if it's near selected paths (or no filter active)
                                if (isNearSelectedPath(anchorPos)) {
                                    seen[key] = true;
                                    pts.push({ pos: anchorPos, rotation: rotation });
                                    addDebug("[ANCHOR " + key + "] Rotation: " + (rotation !== null ? rotation + "° from " + rotationSource : "null"));
                                }
                            }
                        }
                    }

                    function walkContainer(container) {
                        if (!container) return;
                        var type = container.typename;

                        if (type === "PathItem") {
                            processPath(container);
                            return;
                        }

                        if (type !== "Layer" && (container.locked || !container.visible)) {
                            return;
                        }

                        if (container.pathItems) {
                            for (var i = 0; i < container.pathItems.length; i++) {
                                processPath(container.pathItems[i]);
                            }
                        }

                        // CRITICAL FIX: Also collect existing PlacedItems (Units/Registers) so their file links can be updated
                        // This ensures existing scaled-down Units preserve their custom transforms when reprocessing
                        if (container.placedItems) {
                            for (var pi = 0; pi < container.placedItems.length; pi++) {
                                try {
                                    var placed = container.placedItems[pi];
                                    if (placed.locked) continue;

                                    var gb = placed.geometricBounds;
                                    var centerX = (gb[0] + gb[2]) / 2;
                                    var centerY = (gb[1] + gb[3]) / 2;
                                    var centerPos = [centerX, centerY];
                                    var key = centerX.toFixed(2) + "_" + centerY.toFixed(2);

                                    // Skip if already seen
                                    if (seen[key]) continue;

                                    // Skip if anchor is ignored
                                    if (isPointIgnored_local(centerPos)) {
                                        addDebug("[PLACED " + key + "] SKIPPED (on Ignored layer)");
                                        continue;
                                    }

                                    // Only include if near selected paths (or no filter active)
                                    if (isNearSelectedPath(centerPos)) {
                                        seen[key] = true;
                                        // PlacedItems don't have rotation metadata on their anchor points, rotation will be determined later
                                        pts.push({ pos: centerPos, rotation: null });
                                        addDebug("[PLACED " + key + "] Collected existing PlacedItem for file link update");
                                    }
                                } catch (e) {
                                    // Skip this placed item
                                }
                            }
                        }

                        if (container.groupItems) {
                            for (var g = 0; g < container.groupItems.length; g++) {
                                walkContainer(container.groupItems[g]);
                            }
                        }

                        if (container.compoundPathItems) {
                            for (var c = 0; c < container.compoundPathItems.length; c++) {
                                var compound = container.compoundPathItems[c];
                                for (var pIndex = 0; pIndex < compound.pathItems.length; pIndex++) {
                                    walkContainer(compound.pathItems[pIndex]);
                                }
                            }
                        }

                        if (container.layers) {
                            for (var s = 0; s < container.layers.length; s++) {
                                walkContainer(container.layers[s]);
                            }
                        }
                    }

                    // Walk component layer to find existing anchor marker paths
                    walkContainer(layer);
                    return pts;
                }

                function getLayerByName_local(docParam, name) {
                    var direct = null;
                    try { direct = findLayerByNameDeepInContainer(docParam, name); } catch (e) { direct = null; }
                    if (direct) return direct;
                    if (name === "Frame") {
                        try {
                            var l = docParam.layers.add();
                            l.name = "Frame";
                            return l;
                        } catch (e) {
                            return null;
                        }
                    }
                    return null;
                }

                // Run the embedded main
                try { embeddedMain(); } catch (e) { /* swallow errors from embedded routine to match previous behavior */ }

                // NOTE: Final cleanup pass removed - was causing 30+ second freeze by processing
                // ALL existing anchors instead of just selection-related ones.
                // If orphaned anchors are a problem, user can run Process again on those items.

                // *** DUPLICATE PATH CLEANUP ***
                // Remove duplicate/overlapping path segments on Blue Ductwork layer
                try {
                    addDebug("[CLEANUP] Starting duplicate path removal...");
                    var blueLayer = null;
                    try { blueLayer = doc.layers.getByName("Blue Ductwork"); } catch (e) { }
                    if (!blueLayer) {
                        try { blueLayer = doc.layers.getByName("blue ductwork"); } catch (e) { }
                    }

                    if (blueLayer) {
                        var duplicatesRemoved = 0;
                        var allBluePaths = [];

                        // Collect all paths on Blue Ductwork layer
                        for (var bpi = 0; bpi < blueLayer.pathItems.length; bpi++) {
                            try {
                                var bPath = blueLayer.pathItems[bpi];
                                if (bPath && bPath.pathPoints && bPath.pathPoints.length >= 2) {
                                    allBluePaths.push(bPath);
                                }
                            } catch (e) { }
                        }

                        addDebug("[CLEANUP] Checking " + allBluePaths.length + " paths for duplicates");

                        // Check each pair of paths for duplicates
                        var DUPE_TOLERANCE = 1.0; // Points within 1pt are considered same
                        var pathsToRemove = [];

                        // ES3 compatible indexOf helper
                        function arrayContains(arr, item) {
                            for (var idx = 0; idx < arr.length; idx++) {
                                if (arr[idx] === item) return true;
                            }
                            return false;
                        }

                        for (var dpA = 0; dpA < allBluePaths.length; dpA++) {
                            var pathA = allBluePaths[dpA];
                            if (!pathA || arrayContains(pathsToRemove, pathA)) continue;

                            var ptsA = pathA.pathPoints;
                            var aStart = ptsA[0].anchor;
                            var aEnd = ptsA[ptsA.length - 1].anchor;

                            for (var dpB = dpA + 1; dpB < allBluePaths.length; dpB++) {
                                var pathB = allBluePaths[dpB];
                                if (!pathB || arrayContains(pathsToRemove, pathB)) continue;

                                var ptsB = pathB.pathPoints;
                                var bStart = ptsB[0].anchor;
                                var bEnd = ptsB[ptsB.length - 1].anchor;

                                // Check if endpoints match (either direction)
                                var sameDir = (Math.abs(aStart[0] - bStart[0]) < DUPE_TOLERANCE &&
                                               Math.abs(aStart[1] - bStart[1]) < DUPE_TOLERANCE &&
                                               Math.abs(aEnd[0] - bEnd[0]) < DUPE_TOLERANCE &&
                                               Math.abs(aEnd[1] - bEnd[1]) < DUPE_TOLERANCE);
                                var revDir = (Math.abs(aStart[0] - bEnd[0]) < DUPE_TOLERANCE &&
                                              Math.abs(aStart[1] - bEnd[1]) < DUPE_TOLERANCE &&
                                              Math.abs(aEnd[0] - bStart[0]) < DUPE_TOLERANCE &&
                                              Math.abs(aEnd[1] - bStart[1]) < DUPE_TOLERANCE);

                                if (sameDir || revDir) {
                                    // Paths are duplicates - remove the shorter one (or B if same length)
                                    var lenA = pathA.length;
                                    var lenB = pathB.length;
                                    if (lenB <= lenA) {
                                        pathsToRemove.push(pathB);
                                    } else {
                                        pathsToRemove.push(pathA);
                                        break; // pathA marked for removal, stop checking against it
                                    }
                                }
                            }
                        }

                        // Remove duplicate paths
                        for (var remDupe = 0; remDupe < pathsToRemove.length; remDupe++) {
                            try {
                                pathsToRemove[remDupe].remove();
                                duplicatesRemoved++;
                            } catch (e) { }
                        }

                        addDebug("[CLEANUP] Removed " + duplicatesRemoved + " duplicate path(s)");
                    }
                } catch (eDupe) {
                    addDebug("[CLEANUP] Error in duplicate removal: " + eDupe);
                }

                // --- END embedded 03 - Place Ductwork at Points.jsx ---
            })(doc);
        } catch (e) {
            alert("Main processing complete, but the embedded 'Place Ductwork at Points' routine failed: " + e);
        }

        // STEP 8: Apply graphic styles AFTER everything else is completely done
        // *** USING ENHANCED ROBUST VERSION ***
        updateProgress("Applying styles...");
        addDebug("[STEP8] About to call applyAllDuctworkStylesRobust");
        try {
            applyAllDuctworkStylesRobust(originalSelectionItems);
            addDebug("[STEP8] applyAllDuctworkStylesRobust completed");
        } catch (eStep8) {
            addDebug("[STEP8] ERROR: " + eStep8);
        }

        // Ensure Thermostat Lines explicitly gets its graphic style applied to every path (defensive)
        try {
            // NOTE: Naming inconsistency is intentional for backward compatibility with existing templates:
            // - Graphic style is "Thermostat Line" (SINGULAR)
            // - Layer is "Thermostat Lines" (PLURAL)
            var _thermostatStyle = null;
            try { _thermostatStyle = doc.graphicStyles.getByName("Thermostat Line"); } catch (e) { _thermostatStyle = null; }
            var _thermostatLayer = null;
            try { _thermostatLayer = doc.layers.getByName("Thermostat Lines"); } catch (e) { _thermostatLayer = null; }

            addDebug("[Thermostat Style] Style found: " + !!_thermostatStyle + ", Layer found: " + !!_thermostatLayer);

            if (_thermostatStyle && _thermostatLayer) {
                var _thermostatPathCount = 0;
                var _thermostatCompoundCount = 0;

                function _applyThermostatStyleToContainer(cont) {
                    try {
                        // Apply to PathItems
                        if (cont.pathItems) {
                            for (var pi = 0; pi < cont.pathItems.length; pi++) {
                                try {
                                    var p = cont.pathItems[pi];
                                    try { p.unapplyAll(); } catch (e) { }
                                    try { _thermostatStyle.applyTo(p); _thermostatPathCount++; } catch (e) { }
                                } catch (e) { }
                            }
                        }

                        // Apply to CompoundPathItems
                        if (cont.compoundPathItems) {
                            for (var ci = 0; ci < cont.compoundPathItems.length; ci++) {
                                try {
                                    var cp = cont.compoundPathItems[ci];
                                    try { cp.unapplyAll(); } catch (e) { }
                                    try { _thermostatStyle.applyTo(cp); _thermostatCompoundCount++; } catch (e) { }
                                } catch (e) { }
                            }
                        }

                        // Recurse into groups and sublayers
                        if (cont.groupItems) {
                            for (var gi = 0; gi < cont.groupItems.length; gi++) {
                                try { _applyThermostatStyleToContainer(cont.groupItems[gi]); } catch (e) { }
                            }
                        }
                        if (cont.layers) {
                            for (var li = 0; li < cont.layers.length; li++) {
                                try { _applyThermostatStyleToContainer(cont.layers[li]); } catch (e) { }
                            }
                        }
                    } catch (e) {
                        // non-fatal
                    }
                }

                try {
                    // Temporarily ensure layer is editable for styling
                    var _prevLocked = _thermostatLayer.locked;
                    var _prevVisible = _thermostatLayer.visible;
                    try { _thermostatLayer.locked = false; } catch (e) { }
                    try { _thermostatLayer.visible = true; } catch (e) { }

                    _applyThermostatStyleToContainer(_thermostatLayer);

                    addDebug("[Thermostat Style] Applied to " + _thermostatPathCount + " paths and " + _thermostatCompoundCount + " compound paths");

                    try { _thermostatLayer.locked = _prevLocked; } catch (e) { }
                    try { _thermostatLayer.visible = _prevVisible; } catch (e) { }
                } catch (e) {
                    // swallow
                }
            } else {
                if (!_thermostatStyle) {
                    addDebug("[Thermostat Style] ERROR: 'Thermostat Line' graphic style not found. Import graphic styles first.");
                }
                if (!_thermostatLayer) {
                    addDebug("[Thermostat Style] ERROR: 'Thermostat Lines' layer not found.");
                }
            }
        } catch (e) {
            // non-fatal
            addDebug("[Thermostat Style] Exception: " + e);
        }

        applyManualCenterlineStrokeStyles();
        clearEmoryStyleTargetMarks();

        // Ensure the 'Ignored' layer is hidden at the end of the script (safe, non-creating)
        try {
            var _ignoreNames = ["Ignored", "Ignore", "ignored", "ignore"];
            for (var _in = 0; _in < _ignoreNames.length; _in++) {
                try {
                    var _l = doc.layers.getByName(_ignoreNames[_in]);
                    if (_l) {
                        try { _l.visible = false; } catch (e) { /* ignore if cannot set */ }
                        break;
                    }
                } catch (e) { /* layer not present - continue */ }
            }
        } catch (e) {
            /* non-fatal */
        }

        doc.selection = null;
        // alert("Processing complete!");

        // Close progress dialog
        closeProgress();

        // Write debug log
        writeDebugLog();

        // Ensure the desired block of ductwork layers exist and are in the exact order shown in the UI
        function ensureFinalLayerBlockOrder() {
            var desired = [
                "Scale Factor Container Layer",
                "Frame",
                "Ignored",
                "Thermostats",
                "Units",
                "Secondary Exhaust Registers",
                "Thermostat Lines",
                "Exhaust Registers",
                "Rectangular Registers",
                "Circular Registers",
                "Orange Register",
                "Square Registers",
                "Light Orange Ductwork",
                "Orange Ductwork",
                "Blue Ductwork",
                "Green Ductwork"
            ];

            // Create any missing layers (best-effort) without altering other layers otherwise
            for (var i = 0; i < desired.length; i++) {
                try {
                    doc.layers.getByName(desired[i]);
                } catch (e) {
                    try {
                        var nl = doc.layers.add();
                        nl.name = desired[i];
                    } catch (e2) {
                        // ignore create failures
                    }
                }
            }

            // Helper: unlock a layer and its parent chain; return array of {obj, prevLocked}
            function unlockChain(root) {
                var changed = [];
                try {
                    var cur = root;
                    while (cur) {
                        try {
                            if (cur.locked !== undefined) {
                                changed.push({ obj: cur, prevLocked: cur.locked });
                                try { cur.locked = false; } catch (e) { }
                            }
                        } catch (e) { }
                        try { cur = cur.parent; } catch (e) { break; }
                        // Stop if we've reached the document level
                        if (!cur || cur.typename === 'Document') break;
                    }
                } catch (e) { }
                return changed;
            }

            function restoreChain(changes) {
                if (!changes) return;
                for (var i = 0; i < changes.length; i++) {
                    try { changes[i].obj.locked = changes[i].prevLocked; } catch (e) { }
                }
            }

            // Find the insertion anchor: first existing desired-layer index in the current stack
            var firstDesiredIndex = -1;
            for (var li = 0; li < doc.layers.length; li++) {
                try {
                    if (desired.indexOf(doc.layers[li].name) !== -1) { firstDesiredIndex = li; break; }
                } catch (e) { }
            }

            // Determine prevLayer reference: we will place the block starting after prevLayer (or at top if null)
            var prevLayer = null;
            if (firstDesiredIndex === -1) {
                prevLayer = null; // nothing found, place block at top
            } else {
                prevLayer = (firstDesiredIndex > 0) ? doc.layers[firstDesiredIndex - 1] : null;
            }

            // Move each desired layer in order to form a contiguous block
            for (var di = 0; di < desired.length; di++) {
                try {
                    var name = desired[di];
                    var layerObj = null;
                    try { layerObj = doc.layers.getByName(name); } catch (e) { layerObj = null; }
                    if (!layerObj) continue;

                    // Unlock involved layers (layerObj and prevLayer chain) before moving
                    var unlockA = unlockChain(layerObj);
                    var unlockB = null;
                    // Only unlock prevLayer if different from layerObj
                    if (prevLayer && prevLayer !== layerObj) unlockB = unlockChain(prevLayer);

                    if (prevLayer === null) {
                        // place at top
                        try {
                            layerObj.move(doc.layers[0], ElementPlacement.PLACEBEFORE);
                        } catch (e) {
                            // fallback: try placing after last layer
                            try { layerObj.move(doc.layers[doc.layers.length - 1], ElementPlacement.PLACEAFTER); } catch (e) { }
                        }
                    } else {
                        try {
                            layerObj.move(prevLayer, ElementPlacement.PLACEAFTER);
                        } catch (e) {
                            // ignore move failure for this layer
                        }
                    }

                    // Restore locks for both chains
                    try { restoreChain(unlockA); } catch (e) { }
                    try { restoreChain(unlockB); } catch (e) { }

                    // update prevLayer to the layer we just positioned
                    prevLayer = layerObj;
                } catch (e) {
                    // non-fatal per-layer
                }
            }
        }

        // Run final ordering pass (best-effort, non-fatal)
        try { ensureFinalLayerBlockOrder(); } catch (e) { /* swallow */ }

        try {
            registerMDUXExports();
        } catch (e) { }

        addDebug("=== MAGIC DUCTWORK COMPLETE ===");

    } catch (scriptError) {
        // Catch ANY error from the entire script
        addDebug("");
        addDebug("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        addDebug("FATAL ERROR: " + scriptError);
        addDebug("Stack: " + (scriptError.stack || "N/A"));
        addDebug("Line: " + (scriptError.line || "N/A"));
        addDebug("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        alert("Error: " + scriptError);
    } finally {
        // Debug dialog - uncomment to show debug output at end of script
        // try { showDebugDialog(); } catch (e) {}
    }
})();
