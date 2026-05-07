#target illustrator

// Define debug log early so we can use it
if (typeof MDUX === "undefined") {
    var MDUX = {};
}

// Session ID to detect stale MDUX namespace from previous Illustrator sessions
// If MDUX._bridgeSessionId doesn't match, we need to reload magic-final.jsx
$.global.MDUX_BRIDGE_SESSION_ID = new Date().getTime().toString() + "_" + Math.random().toString(36).substr(2, 9);

function MDUX_debugLog_Early(message) {
    try {
        var f = new File($.fileName);
        var folder = f.parent.parent; // Extension root
        var logFile = new File(folder.fsName + "/debug.log");
        logFile.open("a");
        logFile.writeln("[EARLY] " + message);
        logFile.close();
    } catch (e) { }
}

MDUX_debugLog_Early("panel-bridge.jsx is loading...");

#include "./register-ignore.jsx"
#include "./export-utils.jsx"
#include "./gap-tools.jsx"

MDUX_debugLog_Early("Includes finished. MDUX_performExport type: " + typeof MDUX_performExport);

// Store the jsx folder path when this file is first evaluated
// ALWAYS use $.global.MDUX_LAST_BRIDGE_PATH (set by panel.js) if available
// because $.fileName is unreliable and may point to system32 or other wrong locations
// Force update every time if MDUX_LAST_BRIDGE_PATH is set to override any stale cached values
if ($.global.MDUX_LAST_BRIDGE_PATH) {
    $.global.MDUX_JSX_FOLDER = File($.global.MDUX_LAST_BRIDGE_PATH).parent;
    try { $.writeln("[MDUX] Updated MDUX_JSX_FOLDER from MDUX_LAST_BRIDGE_PATH: " + $.global.MDUX_JSX_FOLDER); } catch (e) { }
} else if (typeof $.global.MDUX_JSX_FOLDER === "undefined") {
    $.global.MDUX_JSX_FOLDER = File($.fileName).parent;
    try { $.writeln("[MDUX] Initial setup from $.fileName: " + $.global.MDUX_JSX_FOLDER); } catch (e) { }
}

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
                .replace(/[\b]/g, '\\b') + '"';
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

function MDUX_joinPath(folderObj, filename) {
    var base = "";
    try {
        base = folderObj.fsName;
    } catch (eFs) {
        try {
            base = folderObj.toString();
        } catch (eStr) {
            base = "";
        }
    }
    if (!base) return filename;
    base = base.replace(/\\/g, "/");
    if (base.charAt(base.length - 1) === "/") {
        return base + filename;
    }
    return base + "/" + filename;
}

// Ductwork line layers (line-only layers that should not be orthogonalized, etc.)
var DUCTWORK_LINES = [
    "Green Ductwork",
    "Light Green Ductwork",
    "Blue Ductwork",
    "Orange Ductwork",
    "Light Orange Ductwork",
    "Thermostat Lines"
];

// Ductwork piece layers (used for placing pieces and anchor checks)
var DUCTWORK_PARTS = [
    "Thermostats",
    "Units",
    "Secondary Exhaust Registers",
    "Exhaust Registers",
    "Orange Register",
    "Rectangular Registers",
    "Square Registers",
    "Circular Registers"
];

function MDUX_isDuctworkLine(item) {
    try {
        if (!item || !item.layer) return false;
        var layerName = item.layer.name;
        for (var i = 0; i < DUCTWORK_LINES.length; i++) {
            if (layerName === DUCTWORK_LINES[i]) return true;
        }
    } catch (e) { }
    return false;
}

function MDUX_isDuctworkPart(item) {
    try {
        if (!item || !item.layer) return false;
        var layerName = item.layer.name;
        for (var i = 0; i < DUCTWORK_PARTS.length; i++) {
            if (layerName === DUCTWORK_PARTS[i]) return true;
        }
    } catch (e) { }
    return false;
}

// Metadata storage using item.note (more reliable than tags with undo/redo)
// PERFORMANCE: Removed verbose logging - these functions are called frequently
function MDUX_getMetadata(item) {
    try {
        var note = item.note || "";
        if (!note) return null;

        // Find MDUX_META: anywhere in the note (may have other prefixes before it)
        var metaStart = note.indexOf("MDUX_META:");
        if (metaStart === -1) return null;

        // Extract from after "MDUX_META:" prefix
        var jsonStart = metaStart + 10;
        var remaining = note.substring(jsonStart);

        // Find the FIRST complete JSON object - match braces
        var braceCount = 0;
        var jsonEnd = -1;
        for (var i = 0; i < remaining.length; i++) {
            var ch = remaining.charAt(i);
            if (ch === "{") braceCount++;
            else if (ch === "}") {
                braceCount--;
                if (braceCount === 0) {
                    jsonEnd = i;
                    break;
                }
            }
        }

        if (jsonEnd === -1) return null;

        var jsonStr = remaining.substring(0, jsonEnd + 1);
        return JSON.parse(jsonStr);
    } catch (e) {
        return null;
    }
}

function MDUX_setMetadata(item, metadata) {
    try {
        var jsonStr = JSON.stringify(metadata);
        var note = item.note || "";

        // Preserve any MD: tags from magic-final.jsx (they use | separator)
        var mdTags = [];
        var parts = note.split("|");
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i];
            // Keep MD: tags but not MDUX_META: entries
            if (part.indexOf("MD:") === 0) {
                mdTags.push(part);
            }
        }

        // Build new note: MDUX_META first, then preserved MD: tags
        var newNote = "MDUX_META:" + jsonStr;
        if (mdTags.length > 0) {
            newNote += "|" + mdTags.join("|");
        }

        item.note = newNote;
    } catch (e) {
        // Silent fail - logging here would be expensive
    }
}

function MDUX_getTag(item, key) {
    var meta = MDUX_getMetadata(item);
    return meta && meta[key] !== undefined ? String(meta[key]) : null;
}

function MDUX_setTag(item, key, value) {
    var meta = MDUX_getMetadata(item) || {};
    meta[key] = value;
    MDUX_setMetadata(item, meta);
}

function MDUX_removeTag(item, key) {
    var meta = MDUX_getMetadata(item);
    if (!meta) return;
    delete meta[key];
    MDUX_setMetadata(item, meta);
}

// ========================================
// GLOBAL DOCUMENT SCALE MANAGEMENT
// Uses Document Tags for invisible, undeletable storage
// No undo/redo pollution, completely hidden from UI
// ========================================

var MDUX_SCALE_TAG_NAME = "MDUX_ScaleFactor";

/**
 * Gets the scale factor from document tags.
 * Returns null if no tag exists yet.
 */
function MDUX_getScaleFactorTag(doc) {
    try {
        for (var i = 0; i < doc.tags.length; i++) {
            if (doc.tags[i].name === MDUX_SCALE_TAG_NAME) {
                return parseFloat(doc.tags[i].value) || 100;
            }
        }
    } catch (e) { }
    return null;
}

/**
 * Sets the scale factor in document tags.
 * Creates the tag if it doesn't exist.
 * This is completely invisible and won't pollute undo/redo.
 */
function MDUX_setScaleFactorTag(doc, value) {
    try {
        // Look for existing tag
        for (var i = 0; i < doc.tags.length; i++) {
            if (doc.tags[i].name === MDUX_SCALE_TAG_NAME) {
                doc.tags[i].value = String(value);
                return true;
            }
        }
        // Create new tag
        var tag = doc.tags.add();
        tag.name = MDUX_SCALE_TAG_NAME;
        tag.value = String(value);
        return true;
    } catch (e) {
        return false;
    }
}

/**
 * LEGACY COMPATIBILITY: Gets the Scale Factor Box if it exists.
 * Used for migrating old documents to tag-based storage.
 */
function MDUX_getScaleFactorBox(doc) {
    var layerName = "Scale Factor Container Layer";
    var boxName = "ScaleFactorBox";
    try {
        var container = doc.layers.getByName(layerName);
        for (var i = 0; i < container.pathItems.length; i++) {
            var pi = container.pathItems[i];
            if (pi.name === boxName) {
                return pi;
            }
        }
    } catch (e) { }
    return null;
}

/**
 * Gets the document scale from tags (preferred) or legacy box (fallback).
 * Returns "100" if no scale has been set yet.
 * Automatically migrates legacy box storage to tags.
 */
function MDUX_getDocumentScale() {
    try {
        if (app.documents.length === 0) return "100";
        var doc = app.activeDocument;

        // First, check document tags (new method)
        var tagValue = MDUX_getScaleFactorTag(doc);
        if (tagValue !== null) {
            return String(tagValue);
        }

        // Fallback: check legacy box and migrate to tags
        var box = MDUX_getScaleFactorBox(doc);
        if (box && box.note) {
            var legacyValue = parseFloat(box.note) || 100;
            // Migrate to tags
            MDUX_setScaleFactorTag(doc, legacyValue);
            return String(legacyValue);
        }

        return "100"; // Default if nothing exists yet
    } catch (e) {
        return "100";
    }
}

/**
 * Sets the document scale using document tags.
 * No visible objects, no undo pollution.
 */
function MDUX_setDocumentScale(percent) {
    try {
        if (app.documents.length === 0) return "ERROR:No document";
        var doc = app.activeDocument;

        if (MDUX_setScaleFactorTag(doc, percent)) {
            return "OK";
        } else {
            return "ERROR:Failed to set scale tag";
        }
    } catch (e) {
        return "ERROR:" + e;
    }
}

// ========================================
// GAP DEFINITIONS - VISUAL EDITOR OVERLAY
// Stores gap data in document tags, uses temporary layer for editing
// ========================================

var MDUX_GAP_DATA_TAG_NAME = "MDUX_GapDefinitions";
var MDUX_GAP_EDITOR_LAYER_NAME = "__MDUX_GapEditor_Temp";

/**
 * Gets gap definitions from document tags.
 * Returns array of gap objects or empty array.
 */
function MDUX_getGapDataTag(doc) {
    try {
        for (var i = 0; i < doc.tags.length; i++) {
            if (doc.tags[i].name === MDUX_GAP_DATA_TAG_NAME) {
                var val = doc.tags[i].value;
                if (val && val.length > 0) {
                    return JSON.parse(val);
                }
            }
        }
    } catch (e) { }
    return [];
}

/**
 * Sets gap definitions in document tags.
 * Data is an array of gap objects: [{x, y, gapSize, dirX, dirY, sourceLayer}, ...]
 */
function MDUX_setGapDataTag(doc, data) {
    try {
        var jsonStr = JSON.stringify(data || []);
        // Look for existing tag
        for (var i = 0; i < doc.tags.length; i++) {
            if (doc.tags[i].name === MDUX_GAP_DATA_TAG_NAME) {
                doc.tags[i].value = jsonStr;
                return true;
            }
        }
        // Create new tag
        var tag = doc.tags.add();
        tag.name = MDUX_GAP_DATA_TAG_NAME;
        tag.value = jsonStr;
        return true;
    } catch (e) {
        return false;
    }
}

/**
 * Migrates gap data from the old "Gap Definitions" layer to document tags.
 * Called once when entering edit mode if no tag data exists.
 */
function MDUX_migrateGapLayerToTags(doc) {
    var gaps = [];
    try {
        var oldLayer = doc.layers.getByName("Gap Definitions");
        for (var i = 0; i < oldLayer.pathItems.length; i++) {
            var item = oldLayer.pathItems[i];
            if (!item.note) continue;

            // Parse MDUX_GAP: format
            if (item.note.indexOf("MDUX_GAP:") === 0) {
                try {
                    var meta = JSON.parse(item.note.substring(9));
                    gaps.push({
                        x: meta.x,
                        y: meta.y,
                        gapSize: meta.gapSize || 8.5,
                        dirX: meta.dirX || 0,
                        dirY: meta.dirY || 1,
                        sourceLayer: meta.sourceLayer || ""
                    });
                } catch (e) { }
            }
            // Parse MDUX_PATCH format (simpler markers)
            else if (item.note.indexOf("MDUX_PATCH") === 0) {
                if (item.pathPoints && item.pathPoints.length > 0) {
                    var pt = item.pathPoints[0].anchor;
                    gaps.push({
                        x: pt[0],
                        y: pt[1],
                        gapSize: 8.5,
                        dirX: 0,
                        dirY: 1,
                        sourceLayer: ""
                    });
                }
            }
        }
    } catch (e) { }
    return gaps;
}

/**
 * Enters gap editor mode:
 * 1. Reads gap data from document tags (or migrates from old layer)
 * 2. Creates temporary editor layer with visual markers
 * 3. Returns status for UI
 */
function MDUX_enterGapEditMode() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open" });
        }
        var doc = app.activeDocument;

        // Check if already in edit mode
        var existingLayer = null;
        try { existingLayer = doc.layers.getByName(MDUX_GAP_EDITOR_LAYER_NAME); } catch (e) { }
        if (existingLayer) {
            return JSON.stringify({ ok: false, message: "Already in gap edit mode. Save or cancel first." });
        }

        // Get gap data from tags, or migrate from old layer
        var gaps = MDUX_getGapDataTag(doc);
        if (gaps.length === 0) {
            gaps = MDUX_migrateGapLayerToTags(doc);
            if (gaps.length > 0) {
                MDUX_setGapDataTag(doc, gaps);
            }
        }

        // Create temporary editor layer
        var editorLayer = doc.layers.add();
        editorLayer.name = MDUX_GAP_EDITOR_LAYER_NAME;
        editorLayer.visible = true;
        editorLayer.locked = false;

        // Set layer color to magenta for visibility
        var layerColor = new RGBColor();
        layerColor.red = 255;
        layerColor.green = 0;
        layerColor.blue = 255;
        editorLayer.color = layerColor;

        // Create visual markers for each gap
        var markerColor = new RGBColor();
        markerColor.red = 255;
        markerColor.green = 0;
        markerColor.blue = 255;

        for (var i = 0; i < gaps.length; i++) {
            var gap = gaps[i];
            var marker = editorLayer.pathItems.add();
            marker.stroked = true;
            marker.filled = false;
            marker.strokeColor = markerColor;
            marker.strokeWidth = 2;

            // Create a small cross or line at the gap position
            var halfSize = gap.gapSize || 8.5;
            var x = gap.x;
            var y = gap.y;
            var dirX = gap.dirX || 0;
            var dirY = gap.dirY || 1;

            // Draw line perpendicular to gap direction to show gap size
            var p1 = [x - dirX * halfSize, y - dirY * halfSize];
            var p2 = [x + dirX * halfSize, y + dirY * halfSize];
            marker.setEntirePath([p1, p2]);

            // Store gap metadata in note for retrieval on save
            marker.note = "MDUX_GAP_EDIT:" + JSON.stringify({
                x: x,
                y: y,
                gapSize: halfSize,
                dirX: dirX,
                dirY: dirY,
                sourceLayer: gap.sourceLayer || ""
            });
        }

        // Move layer to top for visibility
        try { editorLayer.move(doc.layers[0], ElementPlacement.PLACEBEFORE); } catch (e) { }

        app.redraw();

        return JSON.stringify({
            ok: true,
            message: "Gap edit mode active. " + gaps.length + " gap(s) loaded.",
            gapCount: gaps.length
        });

    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e });
    }
}

/**
 * Saves gap editor changes and exits edit mode:
 * 1. Reads marker positions from temp layer
 * 2. Saves to document tags
 * 3. Deletes temp layer
 */
function MDUX_saveGapEditMode() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open" });
        }
        var doc = app.activeDocument;

        // Find editor layer
        var editorLayer = null;
        try { editorLayer = doc.layers.getByName(MDUX_GAP_EDITOR_LAYER_NAME); } catch (e) { }
        if (!editorLayer) {
            return JSON.stringify({ ok: false, message: "Not in gap edit mode." });
        }

        // Collect gap data from markers
        var gaps = [];
        for (var i = 0; i < editorLayer.pathItems.length; i++) {
            var marker = editorLayer.pathItems[i];

            // Try to read from note first (preserves original metadata)
            if (marker.note && marker.note.indexOf("MDUX_GAP_EDIT:") === 0) {
                try {
                    var meta = JSON.parse(marker.note.substring(14));
                    // Update position from current marker location
                    if (marker.pathPoints && marker.pathPoints.length >= 2) {
                        var p1 = marker.pathPoints[0].anchor;
                        var p2 = marker.pathPoints[1].anchor;
                        meta.x = (p1[0] + p2[0]) / 2;
                        meta.y = (p1[1] + p2[1]) / 2;
                        // Recalculate direction from marker orientation
                        var dx = p2[0] - p1[0];
                        var dy = p2[1] - p1[1];
                        var len = Math.sqrt(dx * dx + dy * dy);
                        if (len > 0) {
                            meta.dirX = dx / len;
                            meta.dirY = dy / len;
                            meta.gapSize = len / 2;
                        }
                    }
                    gaps.push(meta);
                } catch (e) { }
            }
            // Fallback: read position from path geometry
            else if (marker.pathPoints && marker.pathPoints.length >= 1) {
                var pt = marker.pathPoints[0].anchor;
                var gapSize = 8.5;
                var dirX = 0, dirY = 1;

                if (marker.pathPoints.length >= 2) {
                    var p1 = marker.pathPoints[0].anchor;
                    var p2 = marker.pathPoints[1].anchor;
                    pt = [(p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2];
                    var dx = p2[0] - p1[0];
                    var dy = p2[1] - p1[1];
                    var len = Math.sqrt(dx * dx + dy * dy);
                    if (len > 0) {
                        dirX = dx / len;
                        dirY = dy / len;
                        gapSize = len / 2;
                    }
                }

                gaps.push({
                    x: pt[0],
                    y: pt[1],
                    gapSize: gapSize,
                    dirX: dirX,
                    dirY: dirY,
                    sourceLayer: ""
                });
            }
        }

        // Save to document tags
        MDUX_setGapDataTag(doc, gaps);

        // Delete the editor layer
        try {
            editorLayer.locked = false;
            editorLayer.remove();
        } catch (e) { }

        app.redraw();

        return JSON.stringify({
            ok: true,
            message: "Saved " + gaps.length + " gap(s) and exited edit mode.",
            gapCount: gaps.length
        });

    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e });
    }
}

/**
 * Cancels gap editor mode without saving:
 * Simply deletes the temp layer.
 */
function MDUX_cancelGapEditMode() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open" });
        }
        var doc = app.activeDocument;

        // Find and delete editor layer
        var editorLayer = null;
        try { editorLayer = doc.layers.getByName(MDUX_GAP_EDITOR_LAYER_NAME); } catch (e) { }
        if (!editorLayer) {
            return JSON.stringify({ ok: false, message: "Not in gap edit mode." });
        }

        try {
            editorLayer.locked = false;
            editorLayer.remove();
        } catch (e) { }

        app.redraw();

        return JSON.stringify({ ok: true, message: "Cancelled gap edit mode." });

    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e });
    }
}

/**
 * Checks if currently in gap edit mode.
 */
function MDUX_isInGapEditMode() {
    try {
        if (app.documents.length === 0) return "false";
        var doc = app.activeDocument;
        var editorLayer = null;
        try { editorLayer = doc.layers.getByName(MDUX_GAP_EDITOR_LAYER_NAME); } catch (e) { }
        return editorLayer ? "true" : "false";
    } catch (e) {
        return "false";
    }
}

/**
 * Adds a new gap at the specified position (or center of selection).
 * Must be in gap edit mode.
 */
function MDUX_addGapMarker(x, y) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open" });
        }
        var doc = app.activeDocument;

        var editorLayer = null;
        try { editorLayer = doc.layers.getByName(MDUX_GAP_EDITOR_LAYER_NAME); } catch (e) { }
        if (!editorLayer) {
            return JSON.stringify({ ok: false, message: "Enter gap edit mode first." });
        }

        // If no position specified, use center of selection or document
        if (typeof x !== "number" || typeof y !== "number") {
            if (doc.selection && doc.selection.length > 0) {
                var bounds = doc.selection[0].geometricBounds;
                x = (bounds[0] + bounds[2]) / 2;
                y = (bounds[1] + bounds[3]) / 2;
            } else {
                var ab = doc.artboards[doc.artboards.getActiveArtboardIndex()].artboardRect;
                x = (ab[0] + ab[2]) / 2;
                y = (ab[1] + ab[3]) / 2;
            }
        }

        // Create marker
        var markerColor = new RGBColor();
        markerColor.red = 255;
        markerColor.green = 0;
        markerColor.blue = 255;

        var marker = editorLayer.pathItems.add();
        marker.stroked = true;
        marker.filled = false;
        marker.strokeColor = markerColor;
        marker.strokeWidth = 2;

        var halfSize = 8.5;
        marker.setEntirePath([
            [x, y - halfSize],
            [x, y + halfSize]
        ]);

        marker.note = "MDUX_GAP_EDIT:" + JSON.stringify({
            x: x,
            y: y,
            gapSize: halfSize,
            dirX: 0,
            dirY: 1,
            sourceLayer: ""
        });

        marker.selected = true;
        app.redraw();

        return JSON.stringify({ ok: true, message: "Added gap marker at " + x.toFixed(1) + ", " + y.toFixed(1) });

    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e });
    }
}

/**
 * LEGACY: Full document scaling disabled in UI to prevent desync issues.
 * Kept for reference - do not expose in panel UI.
 *
 * Ported logic from old script: Portions the document into Geometry (Resized)
 * and Strokes (StrokeWidth Scaled).
 */
function MDUX_applyScaleToFullDocument(targetPercent) {
    try {
        if (app.documents.length === 0) return JSON.stringify({ ok: false, message: "No document open" });
        var doc = app.activeDocument;

        var currentScaleStr = MDUX_getDocumentScale();
        var currentPercent = parseFloat(currentScaleStr) || 100;

        if (Math.abs(targetPercent - currentPercent) < 0.001) {
            return JSON.stringify({ ok: true, message: "Document is already at " + targetPercent + "%" });
        }

        var ratio = targetPercent / currentPercent;
        var stats = { parts: 0, lines: 0 };

        // 1. Scale Parts (Geometry & Stroke)
        for (var i = 0; i < DUCTWORK_PARTS.length; i++) {
            try {
                var layer = doc.layers.getByName(DUCTWORK_PARTS[i]);
                if (layer.locked || !layer.visible) continue;

                for (var j = 0; j < layer.pageItems.length; j++) {
                    var item = layer.pageItems[j];
                    if (item.locked || item.hidden) continue;
                    try {
                        // Resizing parts includes stroke scaling usually
                        item.resize(ratio * 100, ratio * 100, true, true, true, true, ratio * 100, Transformation.CENTER);
                        stats.parts++;
                    } catch (e) { }
                }
            } catch (eLayer) { }
        }

        // 2. Scale Lines (Stroke Only)
        // Note: For lines, we typically want to scale the stroke width but not the geometry lengths
        // unless they are also being repositioned. The old script's applyFullLayerScaling
        // distinguishes between scaleGeometryOnly and scaleStrokesOnly.

        for (var k = 0; k < DUCTWORK_LINES.length; k++) {
            try {
                var lLayer = doc.layers.getByName(DUCTWORK_LINES[k]);
                if (lLayer.locked || !lLayer.visible) continue;

                for (var m = 0; m < lLayer.pageItems.length; m++) {
                    var lItem = lLayer.pageItems[m];
                    if (lItem.locked || lItem.hidden) continue;

                    // Recursive stroke scaling logic
                    MDUX_scaleStrokeRecursive(lItem, ratio);
                    stats.lines++;
                }
            } catch (eLayerLine) { }
        }

        // Update the box
        MDUX_setDocumentScale(targetPercent);

        // SYNC TAGS: Important to keep Selection Transform UI in sync
        for (var n = 0; n < doc.pageItems.length; n++) {
            var pi = doc.pageItems[n];
            // Only update if it already has ductwork metadata to avoid tagging every single thing in the doc
            if (MDUX_getTag(pi, "MDUX_OriginalWidth") !== null) {
                MDUX_setTag(pi, "MDUX_CurrentScale", targetPercent);
            }
        }

        return JSON.stringify({ ok: true, message: "Scaled document from " + currentPercent + "% to " + targetPercent + "%", stats: stats });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e });
    }
}

function MDUX_scaleStrokeRecursive(item, ratio) {
    var percent = ratio * 100;
    try {
        if (item.typename === "GroupItem") {
            for (var i = 0; i < item.pageItems.length; i++) {
                MDUX_scaleStrokeRecursive(item.pageItems[i], ratio);
            }
        } else if (item.typename === "CompoundPathItem" || item.typename === "PathItem") {
            // Use resize(100, 100, false, false, false, false, percent) 
            // to scale ONLY the stroke width/patterns.
            // Ported from old script's scaleStrokeProperties logic.
            item.resize(100, 100, false, false, false, false, percent, Transformation.CENTER);
        }
    } catch (e) { }
}

function MDUX_resetTransforms(targetPercent) {
    try {
        if (app.documents.length === 0) return JSON.stringify({ ok: false, message: "No document open." });
        var sel = app.selection;
        if (!sel || sel.length === 0) return JSON.stringify({ ok: false, message: "Nothing selected." });

        var count = 0;
        for (var i = 0; i < sel.length; i++) {
            var item = sel[i];

            // Determine item type - ductwork lines should NOT have geometry resized
            var isDuctLine = MDUX_isDuctworkLine(item);
            var isDuctPart = MDUX_isDuctworkPart(item);

            // 1. Restore Rotation (around center) - only for ductwork parts
            // Uses same comprehensive logic as MDUX_resetDuctworkPartsRotation
            if (isDuctPart) {
                var meta = MDUX_getMetadata(item);

                // Check for MD:PLACED_ROT tag in the note (used by Quick Rotate)
                var placedRot = 0;
                var note = "";
                try { note = item.note || ""; } catch (e) {}
                var placedRotMatch = note.match(/MD:PLACED_ROT=([0-9.\-]+)/);
                if (placedRotMatch) {
                    placedRot = parseFloat(placedRotMatch[1]) || 0;
                }

                // Get cumulative rotation from metadata - check ALL possible sources
                var r = 0;
                if (meta && meta.MDUX_CumulativeRotation !== undefined) {
                    r = parseFloat(meta.MDUX_CumulativeRotation) || 0;
                } else if (meta && meta.MDUX_RotationOverride !== undefined) {
                    r = parseFloat(meta.MDUX_RotationOverride) || 0;
                } else if (meta && typeof meta.rotation === "number") {
                    r = meta.rotation;
                } else if (meta && meta.tagRotation !== undefined) {
                    r = parseFloat(meta.tagRotation) || 0;
                } else if (placedRot !== 0) {
                    // Fall back to MD:PLACED_ROT from magic-final.jsx
                    r = placedRot;
                }

                if (Math.abs(r) > 0.001) {
                    // Capture center position before rotation
                    var bounds = item.geometricBounds;
                    var centerX = (bounds[0] + bounds[2]) / 2;
                    var centerY = (bounds[1] + bounds[3]) / 2;

                    // Rotate back to 0 around center
                    item.rotate(-r, true, true, true, true, Transformation.CENTER);

                    // Verify center stayed in place, translate back if needed
                    var newBounds = item.geometricBounds;
                    var newCenterX = (newBounds[0] + newBounds[2]) / 2;
                    var newCenterY = (newBounds[1] + newBounds[3]) / 2;
                    if (Math.abs(newCenterX - centerX) > 0.1 || Math.abs(newCenterY - centerY) > 0.1) {
                        item.translate(centerX - newCenterX, centerY - newCenterY);
                    }
                }

                // Clear ALL rotation-related fields in metadata
                if (!meta) meta = {};
                meta.MDUX_CumulativeRotation = "0";
                meta.MDUX_RotationOverride = 0;
                meta.tagRotation = 0;
                if (typeof meta.rotation === "number") {
                    meta.rotation = 0;
                }
                MDUX_setMetadata(item, meta);

                // Also clear MD:PLACED_ROT from note if present
                if (placedRot !== 0 && note) {
                    try {
                        var newNote = note.replace(/MD:PLACED_ROT=[0-9.\-]+;?/g, "");
                        item.note = newNote;
                    } catch (eNote) {}
                }

                // Relink PlacedItem to refresh bounding box after rotation reset
                if (item.typename === "PlacedItem") {
                    try {
                        var linkedFile = item.file;
                        if (linkedFile && linkedFile.exists) {
                            // Save note before relink - relink wipes metadata!
                            var savedNote = item.note || "";
                            item.file = linkedFile;
                            try { item.relink(linkedFile); } catch (eRl) { }
                            try { item.update(); } catch (eUp) { }
                            // Restore the note after relink
                            if (savedNote) {
                                item.note = savedNote;
                            }
                        }
                    } catch (eRelink) { }
                }
            }

            // 2. Restore Scale (around center) - only for ductwork PARTS, not lines
            if (isDuctPart) {
                var origW = MDUX_getTag(item, "MDUX_OriginalWidth");
                var origH = MDUX_getTag(item, "MDUX_OriginalHeight");
                if (origW !== null && origH !== null) {
                    var targetW = parseFloat(origW);
                    var targetH = parseFloat(origH);
                    var currentW = item.width;
                    var currentH = item.height;

                    // Only resize if dimensions actually changed
                    if (Math.abs(currentW - targetW) > 0.01 || Math.abs(currentH - targetH) > 0.01) {
                        // Calculate scale percentages
                        var scaleX = (targetW / currentW) * 100;
                        var scaleY = (targetH / currentH) * 100;

                        // Use resize with Transformation.CENTER to scale around center point
                        // Keep stroke at 100% since we restore it separately
                        item.resize(scaleX, scaleY, true, true, true, true, 100, Transformation.CENTER);
                    }

                    MDUX_removeTag(item, "MDUX_OriginalWidth");
                    MDUX_removeTag(item, "MDUX_OriginalHeight");
                }
            }

            // 3. Restore appearance - different behavior for parts vs lines
            if (isDuctPart) {
                // For ductwork parts: restore stroke width
                var origStroke = MDUX_getTag(item, "MDUX_OriginalStrokeWidth");
                if (origStroke !== null) {
                    try {
                        item.strokeWidth = parseFloat(origStroke);
                    } catch (e) {}
                    MDUX_removeTag(item, "MDUX_OriginalStrokeWidth");
                }
            } else if (isDuctLine) {
                // For ductwork lines: reapply the default graphic style based on layer
                try {
                    var layerName = item.layer.name;
                    // Graphic style names match layer names for ductwork
                    var graphicStyle = app.activeDocument.graphicStyles.getByName(layerName);
                    if (graphicStyle) {
                        graphicStyle.applyTo(item);
                    }
                } catch (eStyle) {
                    // Style not found or failed to apply - ignore
                }
            }

            // 4. Restore Selection Transform Tag
            MDUX_removeTag(item, "MDUX_CurrentScale");

            count++;
        }
        return JSON.stringify({ ok: true, message: "Reset " + count + " items." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e.message });
    }
}

if (typeof MDUX === "undefined") {
    var MDUX = {};
}

// Debug logging to file
// NOTE: Debug flags are now centralized in magic-final.jsx at $.global.MDUX_DEBUG
// This helper checks the global config, defaulting to true if not yet loaded
function MDUX_isDebugEnabled() {
    if (typeof $.global.MDUX_DEBUG !== "undefined" && $.global.MDUX_DEBUG) {
        return $.global.MDUX_DEBUG.ENABLED;
    }
    return true; // Default to enabled if config not loaded yet
}

function MDUX_setDebugEnabledBridge(enabled) {
    if (typeof $.global.MDUX_DEBUG === "undefined" || !$.global.MDUX_DEBUG) {
        $.global.MDUX_DEBUG = {};
    }
    $.global.MDUX_DEBUG.ENABLED = !!enabled;
    return $.global.MDUX_DEBUG.ENABLED ? "true" : "false";
}

function MDUX_getDebugEnabledBridge() {
    return MDUX_isDebugEnabled() ? "true" : "false";
}

function MDUX_resetSessionStateBridge() {
    var cleared = [];

    function clearKey(key, value) {
        try {
            if (typeof $.global[key] !== "undefined") {
                if (typeof value === "undefined") {
                    try { delete $.global[key]; } catch (eDel) { $.global[key] = null; }
                } else {
                    $.global[key] = value;
                }
                cleared.push(key);
            }
        } catch (e) { }
    }

    clearKey("MDUX_SELECTION_BOUNDS");
    clearKey("MDUX_NAME_COUNTERS");
    clearKey("MDUX_PROGRESS_WIN", null);
    clearKey("MDUX_PROGRESS_CANCELLED", false);

    try {
        if ($.global.MDUX && $.global.MDUX.forcedOptions) {
            delete $.global.MDUX.forcedOptions;
            cleared.push("MDUX.forcedOptions");
        }
    } catch (eForced) { }

    try {
        if (typeof PythonBridge !== "undefined" && PythonBridge.resetServerStatus) {
            PythonBridge.resetServerStatus();
            cleared.push("PythonBridge.resetServerStatus");
        }
    } catch (ePy) { }

    return "Session reset: " + (cleared.length ? cleared.join(", ") : "nothing to clear");
}

// Document change cleanup - call this when switching between documents to prevent stale state
// This is lighter than a full session reset and designed to be called frequently
function MDUX_onDocumentChange() {
    var actions = [];

    try {
        // Truncate debug buffer if it's grown too large from previous processing
        // This prevents memory issues and potential slowdowns from large array operations
        if (typeof $.global.MDUX_debugBuffer !== "undefined" && $.global.MDUX_debugBuffer.length > 500) {
            var oldLen = $.global.MDUX_debugBuffer.length;
            // Keep only the last 200 entries to maintain some context
            $.global.MDUX_debugBuffer = $.global.MDUX_debugBuffer.slice(-200);
            actions.push("truncated debug buffer from " + oldLen + " to " + $.global.MDUX_debugBuffer.length);
        }

        // Clear selection-related cached state that could be stale
        if (typeof $.global.MDUX_SELECTION_BOUNDS !== "undefined") {
            delete $.global.MDUX_SELECTION_BOUNDS;
            actions.push("cleared MDUX_SELECTION_BOUNDS");
        }

        // Reset progress window state to prevent stale references
        if ($.global.MDUX_PROGRESS_WIN !== undefined && $.global.MDUX_PROGRESS_WIN !== null) {
            try {
                if ($.global.MDUX_PROGRESS_WIN.close) {
                    $.global.MDUX_PROGRESS_WIN.close();
                }
            } catch (eClose) { }
            $.global.MDUX_PROGRESS_WIN = null;
            actions.push("closed stale progress window");
        }

        $.global.MDUX_PROGRESS_CANCELLED = false;

    } catch (e) {
        actions.push("error: " + e);
    }

    return actions.length > 0 ? actions.join("; ") : "no cleanup needed";
}

// In-memory debug log buffer
if (typeof $.global.MDUX_debugBuffer === "undefined") {
    $.global.MDUX_debugBuffer = [];
}

var MDUX_LIVE_SELECTION_PATH_LIMIT = 300;
var MDUX_METADATA_NOTE_LIMIT = 500;

function MDUX_selectionExceedsPathLimit(selection, limit) {
    try {
        if (!selection) return false;
        var max = (typeof limit === "number" && isFinite(limit)) ? limit : MDUX_LIVE_SELECTION_PATH_LIMIT;
        var count = 0;

        function add(n) {
            count += n;
            return count > max;
        }

        function visit(item) {
            if (!item) return false;
            var type = item.typename;
            if (type === "PathItem") {
                return add(1);
            }
            if (type === "CompoundPathItem" && item.pathItems) {
                return add(item.pathItems.length);
            }
            if (type === "GroupItem" && item.pageItems) {
                for (var g = 0; g < item.pageItems.length; g++) {
                    if (visit(item.pageItems[g])) return true;
                }
                return false;
            }
            return add(1);
        }

        if (selection.length === undefined && selection.typename) {
            return visit(selection);
        }
        for (var i = 0; i < selection.length; i++) {
            if (visit(selection[i])) return true;
        }
    } catch (e) { }
    return false;
}

function MDUX_selectionHasPathItems(selection) {
    try {
        if (!selection) return false;
        function visit(item) {
            if (!item) return false;
            var type = item.typename;
            if (type === "PathItem") return true;
            if (type === "CompoundPathItem" && item.pathItems && item.pathItems.length) return true;
            if (type === "GroupItem" && item.pageItems) {
                for (var g = 0; g < item.pageItems.length; g++) {
                    if (visit(item.pageItems[g])) return true;
                }
            }
            return false;
        }
        if (selection.length === undefined && selection.typename) {
            return visit(selection);
        }
        for (var i = 0; i < selection.length; i++) {
            if (visit(selection[i])) return true;
        }
    } catch (e) { }
    return false;
}

function MDUX_selectionHasPlacedItems(selection) {
    try {
        if (!selection) return false;
        function visit(item) {
            if (!item) return false;
            if (item.typename === "PlacedItem") return true;
            if (item.typename === "GroupItem" && item.pageItems) {
                for (var g = 0; g < item.pageItems.length; g++) {
                    if (visit(item.pageItems[g])) return true;
                }
            }
            return false;
        }
        if (selection.length === undefined && selection.typename) {
            return visit(selection);
        }
        for (var i = 0; i < selection.length; i++) {
            if (visit(selection[i])) return true;
        }
    } catch (e) { }
    return false;
}

function MDUX_noteIsHeavy(item) {
    try {
        if (!item || typeof item.note !== "string") return false;
        return item.note.length > MDUX_METADATA_NOTE_LIMIT;
    } catch (e) { }
    return false;
}

function MDUX_collectPlacedRotationSummary(selection) {
    var summary = { available: true, reason: null, rotations: [], formatted: "", count: 0 };
    var rotationMap = {};
    var rotationList = [];

    function addRotation(value) {
        if (typeof value !== "number" || !isFinite(value)) return;
        var normalized = value;
        var key = normalized.toFixed(2);
        if (!rotationMap.hasOwnProperty(key)) {
            rotationMap[key] = normalized;
            rotationList.push(normalized);
        }
    }

    function visit(item) {
        if (!item) return;
        if (item.typename === "PlacedItem") {
            if (MDUX_noteIsHeavy(item)) return;
            var meta = MDUX_getMetadata(item);
            if (meta && meta.MDUX_RotationOverride !== undefined && meta.MDUX_RotationOverride !== null) {
                var rot = parseFloat(meta.MDUX_RotationOverride);
                if (isFinite(rot)) addRotation(rot);
            }
            return;
        }
        if (item.typename === "GroupItem" && item.pageItems) {
            for (var g = 0; g < item.pageItems.length; g++) visit(item.pageItems[g]);
        }
    }

    if (selection.length === undefined && selection.typename) {
        visit(selection);
    } else {
        for (var i = 0; i < selection.length; i++) visit(selection[i]);
    }

    rotationList.sort(function (a, b) { return a - b; });
    var formattedList = [];
    for (var ri = 0; ri < rotationList.length; ri++) {
        var rounded = Math.round(rotationList[ri] * 100) / 100;
        formattedList.push(String(rounded));
    }
    summary.rotations = rotationList;
    summary.formatted = formattedList.join(", ");
    summary.count = rotationList.length;
    return summary;
}

function MDUX_debugLog(message) {
    // PERFORMANCE: Skip all logging if debug mode is off
    if (!MDUX_isDebugEnabled()) return;

    try {
        // ExtendScript doesn't have toISOString(), use toString() instead
        var timestamp = new Date().toString();
        var logEntry = "[" + timestamp + "] " + message;
        $.global.MDUX_debugBuffer.push(logEntry);

        // PERFORMANCE: Batch truncation instead of shift() on every call
        // shift() is O(n) and calling it repeatedly is very slow
        // Let buffer grow to 2500, then truncate to 2000 with slice() (single operation)
        if ($.global.MDUX_debugBuffer.length > 2500) {
            $.global.MDUX_debugBuffer = $.global.MDUX_debugBuffer.slice(-2000);
        }
    } catch (e) {
        // Log the error to help debug
        $.global.MDUX_debugBuffer.push("[ERROR in MDUX_debugLog] " + e.toString());
    }
}

function MDUX_isCepSuspended() {
    try {
        var flagFile = new File(Folder.userData.fsName + "/Adobe/CEP/extensions/Emory-Ductwork-Panel/md_cep_suspend.flag");
        return flagFile.exists;
    } catch (e) {
        return false;
    }
}

// Log that bridge is loading
MDUX_debugLog("=== BRIDGE LOADING - panel-bridge.jsx ===");

function MDUX_extensionRoot() {
    // Use the stored jsx folder path, or fall back to calculating it
    // from $.global.MDUX_LAST_BRIDGE_PATH (set by panel.js) or $.fileName
    if (typeof $.global.MDUX_JSX_FOLDER !== "undefined" && $.global.MDUX_JSX_FOLDER) {
        return $.global.MDUX_JSX_FOLDER;
    }
    var bridgePath = $.global.MDUX_LAST_BRIDGE_PATH || $.fileName;
    if (bridgePath) {
        var file = File(bridgePath);
        if (file.parent && file.parent.exists) {
            return file.parent;
        }
    }
    // Last resort: try to get it from $.fileName
    if ($.fileName) {
        return File($.fileName).parent;
    }
    return null;
}

function MDUX_runMagicDuctwork() {
    try {
        MDUX_debugLog("=== MDUX_runMagicDuctwork called ===");
        MDUX_debugLog("$.fileName: " + $.fileName);
        MDUX_debugLog("$.global.MDUX_LAST_BRIDGE_PATH: " + $.global.MDUX_LAST_BRIDGE_PATH);
        MDUX_debugLog("$.global.MDUX_JSX_FOLDER: " + $.global.MDUX_JSX_FOLDER);

        var root = MDUX_extensionRoot();
        if (!root) {
            MDUX_debugLog("ERROR: root is null/undefined");
            return "ERROR:Could not determine extension root folder";
        }
        var rootPath = "";
        try {
            rootPath = root.fsName || root.toString();
        } catch (e) {
            rootPath = String(root);
        }
        MDUX_debugLog("root folder: " + rootPath);
        try { $.writeln("[MDUX] root folder: " + rootPath); } catch (logRoot) { }

        var scriptFile = File(MDUX_joinPath(root, "magic-final.jsx"));
        var scriptPath = "";
        try {
            scriptPath = scriptFile.fsName || scriptFile.toString();
        } catch (e) {
            scriptPath = String(scriptFile);
        }
        MDUX_debugLog("looking for: " + scriptPath);
        MDUX_debugLog("file exists: " + scriptFile.exists);
        try { $.writeln("[MDUX] looking for: " + scriptPath); } catch (logPath) { }
        try { $.writeln("[MDUX] file exists: " + scriptFile.exists); } catch (logExists) { }

        if (!scriptFile.exists) {
            MDUX_debugLog("ERROR: magic-final missing at " + scriptPath);
            try { $.writeln("[MDUX] magic-final missing at " + scriptPath); } catch (logMiss) { }
            return "ERROR:magic-final.jsx not found at: " + scriptPath;
        }
        MDUX_debugLog("SUCCESS: About to evalFile");
        $.evalFile(scriptFile);
        MDUX_debugLog("SUCCESS: evalFile completed");
        return "OK";
    } catch (e) {
        MDUX_debugLog("EXCEPTION: " + e);
        return "ERROR:" + e;
    }
}

function MDUX_requireMagicFinal() {
    try {
        // Check if MDUX exists AND is from this session (not stale from previous Illustrator run)
        // Stale MDUX has broken closures that cause "undefined is not an object" errors
        var currentSessionId = $.global.MDUX_BRIDGE_SESSION_ID || "";
        if (typeof MDUX !== "undefined" && MDUX.rotateSelection && MDUX._bridgeSessionId === currentSessionId) {
            return true;
        }
        var root = MDUX_extensionRoot();
        if (!root) {
            try { $.writeln("[MDUX] requireMagicFinal: could not determine root"); } catch (logErr) { }
            return false;
        }
        var scriptFile = File(MDUX_joinPath(root, "magic-final.jsx"));
        var scriptPath = "";
        try {
            scriptPath = scriptFile.fsName || scriptFile.toString();
        } catch (e) {
            scriptPath = String(scriptFile);
        }
        try { $.writeln("[MDUX] requireMagicFinal: looking for " + scriptPath); } catch (logPath) { }
        if (!scriptFile.exists) {
            try { $.writeln("[MDUX] requireMagicFinal: file not found at " + scriptPath); } catch (logMiss) { }
            return false;
        }
        var previousForced = null;
        if ($.global.MDUX && $.global.MDUX.forcedOptions) {
            previousForced = $.global.MDUX.forcedOptions;
        }
        $.global.MDUX = $.global.MDUX || {};
        $.global.MDUX.forcedOptions = { action: "library" };
        try { $.writeln("[MDUX] requireMagicFinal: loading magic-final.jsx"); } catch (logErr) { }
        $.evalFile(scriptFile);
        try {
            if ($.global.MDUX) {
                MDUX = $.global.MDUX;
            }
        } catch (eAssign) { }
        if (previousForced) {
            $.global.MDUX.forcedOptions = previousForced;
        } else if ($.global.MDUX && $.global.MDUX.hasOwnProperty("forcedOptions")) {
            delete $.global.MDUX.forcedOptions;
        }
        var ns = (typeof MDUX !== "undefined" && MDUX.rotateSelection) ? MDUX : ($.global.MDUX || null);
        return !!(ns && (ns.rotateSelection || ns.createStandardLayerBlock || ns.importDuctworkGraphicStyles));
    } catch (e) {
        try { $.writeln("[MDUX] requireMagicFinal error: " + e); } catch (logErr2) { }
        return false;
    }
}

function MDUX_toggleIgnoreModeBridge() {
    try {
        return MDUX_toggleIgnoreMode();
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_setIgnoreModeBridge(state) {
    try {
        return MDUX_setIgnoreMode(state);
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_ignoreModeStatusBridge() {
    try {
        return MDUX_getIgnoreModeStatus();
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_skipOrthoStateBridge() {
    try {
        if (!MDUX_requireMagicFinal()) {
            return JSON.stringify({ available: false, error: "bridge-load" });
        }
        if (app.documents.length === 0) {
            return JSON.stringify({ available: false, reason: "no-document" });
        }
        var doc = app.activeDocument;
        var sel = null;
        try { sel = doc.selection; } catch (eSel) { sel = null; }
        if (!sel || sel.length === 0) {
            return JSON.stringify({ available: false, reason: "no-selection" });
        }
        if (!MDUX_selectionHasPathItems(sel)) {
            return JSON.stringify({ available: true, hasNote: false, mixed: false, reason: "no-lines" });
        }
        if (MDUX_selectionExceedsPathLimit(sel, MDUX_LIVE_SELECTION_PATH_LIMIT)) {
            return JSON.stringify({
                available: true,
                hasNote: false,
                mixed: true,
                reason: "large-selection"
            });
        }
        var ns = (typeof MDUX !== "undefined" && MDUX.checkSkipOrthoState) ? MDUX : ($.global.MDUX || null);
        if (!ns || !ns.checkSkipOrthoState) {
            return "ERROR:Skip ortho state function unavailable";
        }
        var state = ns.checkSkipOrthoState(sel);
        return JSON.stringify({
            available: true,
            hasNote: !!(state && state.hasNote),
            mixed: !!(state && state.mixed)
        });
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_applyIgnoreBridge() {
    try {
        var stats = MDUX_applyIgnoreToCurrent ? MDUX_applyIgnoreToCurrent() : null;
        if (stats === null || typeof stats === "undefined") {
            stats = {};
        }
        var encodeJson = (typeof JSON !== "undefined" && JSON.stringify) ? JSON.stringify : null;
        if (!encodeJson) {
            try {
                encodeJson = function (obj) {
                    return obj ? obj.toSource() : "{}";
                };
            } catch (eSource) {
                encodeJson = function () { return "{}"; };
            }
        }
        return encodeJson(stats);
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_cleanupBridge() {
    try {
        MDUX_cleanup();
        return "OK";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_revertPreOrthoBridge() {
    try {
        if (app.documents.length === 0) {
            return "ERROR:No Illustrator document is open.";
        }
        var doc = app.activeDocument;
        var sel = null;
        try { sel = doc.selection; } catch (eSel) { sel = null; }
        if (!sel || sel.length === 0) {
            return JSON.stringify({ total: 0, reverted: 0, skipped: 0, reason: "no-selection" });
        }
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Revert function unavailable";
        }
        if (typeof MDUX !== "undefined" && MDUX.revertSelectionToPreOrtho) {
            var stats = MDUX.revertSelectionToPreOrtho(sel);
            return JSON.stringify(stats);
        }
        return "ERROR:Revert function unavailable";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_clearRotationMetadataBridge() {
    try {
        if (app.documents.length === 0) {
            return "ERROR:No Illustrator document is open.";
        }
        var doc = app.activeDocument;
        var sel = null;
        try { sel = doc.selection; } catch (eSel) { sel = null; }
        if (!sel || sel.length === 0) {
            return "0";
        }
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Clear rotation metadata function unavailable";
        }
        if (typeof MDUX === "undefined" || !MDUX.clearRotationOverride || !MDUX.getAllPathItemsInGroup) {
            return "ERROR:Clear rotation metadata function unavailable";
        }

        var count = 0;
        for (var i = 0; i < sel.length; i++) {
            var item = sel[i];
            if (item.typename === "PathItem") {
                MDUX.clearRotationOverride(item);
                count++;
            } else if (item.typename === "CompoundPathItem") {
                // Clear rotation override from child paths (MD:ROT= tokens)
                if (item.pathItems) {
                    for (var cpi = 0; cpi < item.pathItems.length; cpi++) {
                        MDUX.clearRotationOverride(item.pathItems[cpi]);
                        count++;
                    }
                }
                // Also clear rotation override from compound path's MDUX_META
                try {
                    var compoundMeta = MDUX_getMetadata(item);
                    if (compoundMeta && compoundMeta.MDUX_RotationOverride !== undefined) {
                        delete compoundMeta.MDUX_RotationOverride;
                        MDUX_setMetadata(item, compoundMeta);
                    }
                } catch (eCompoundMeta) {
                    // Ignore metadata errors
                }
            } else if (item.typename === "GroupItem") {
                var paths = MDUX.getAllPathItemsInGroup(item);
                for (var j = 0; j < paths.length; j++) {
                    MDUX.clearRotationOverride(paths[j]);
                    count++;
                }
            }
        }

        return String(count);
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_prepareProcessBridge(optionsJSON) {
    try {
        var opts = null;
        if (optionsJSON && optionsJSON.length) {
            opts = JSON.parse(optionsJSON);
        }
        if (!opts) opts = {};
        $.global.MDUX = $.global.MDUX || {};
        $.global.MDUX.forcedOptions = {
            action: opts.action || "process",
            skipOrtho: (typeof opts.skipOrtho === "boolean") ? opts.skipOrtho : undefined,
            rotationOverride: (typeof opts.rotationOverride === "number" && isFinite(opts.rotationOverride)) ? opts.rotationOverride : null,
            skipAllBranchSegments: !!opts.skipAllBranchSegments,
            skipFinalRegisterSegment: !!opts.skipFinalRegisterSegment,
            skipRegisterRotation: !!opts.skipRegisterRotation,
            enableRegisterCarve: !!opts.enableRegisterCarve,
            enableOverlapCarve: !!opts.enableOverlapCarve
        };
        MDUX_debugLog("[BRIDGE] skipRegisterRotation option received: " + opts.skipRegisterRotation + " -> forcedOptions.skipRegisterRotation=" + $.global.MDUX.forcedOptions.skipRegisterRotation);
        MDUX_debugLog("[BRIDGE] enableRegisterCarve=" + opts.enableRegisterCarve + ", enableOverlapCarve=" + opts.enableOverlapCarve);
        return "OK";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_rotateSelectionBridge(angle) {
    try {
        if (typeof angle !== "number") angle = parseFloat(angle);
        if (!isFinite(angle)) return "ERROR:Invalid rotation value";
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Rotate function unavailable";
        }
        // Use absolute rotation so entering 45Â° sets to 45Â° (not adds 45Â°)
        // Entering 0Â° resets to original/base orientation
        if (typeof MDUX !== "undefined" && MDUX.rotateSelectionAbsolute) {
            var stats = MDUX.rotateSelectionAbsolute(angle);
            return JSON.stringify(stats);
        }
        // Fallback to legacy relative rotation if absolute unavailable
        if (typeof MDUX !== "undefined" && MDUX.rotateSelection) {
            var stats = MDUX.rotateSelection(angle);
            return JSON.stringify(stats);
        }
        return "ERROR:Rotate function unavailable";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_scaleSelectionBridge(requestedPercent) {
    try {
        var percent = requestedPercent;
        if (typeof percent !== "number") percent = parseFloat(percent);
        if (!isFinite(percent)) return "ERROR:Invalid scale value";
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Scale function unavailable";
        }
        if (typeof MDUX !== "undefined" && MDUX.scaleSelectionAbsolute) {
            // Anchor-based scaling: scale relative to document's anchor
            var anchor = parseFloat(MDUX_getDocumentScale()) || 100;
            var targetPercent = percent * (anchor / 100);

            var stats = MDUX.scaleSelectionAbsolute(targetPercent);

            // Note: We do NOT update the Document Scale anchor here, 
            // as this is a selection-only transform.

            return JSON.stringify(stats);
        }
        return "ERROR:Scale function unavailable";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_resetScaleBridge() {
    try {
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Reset scale function unavailable";
        }
        if (typeof MDUX !== "undefined" && MDUX.resetSelectionScale) {
            var stats = MDUX.resetSelectionScale();

            // Sync with Scale Factor Box if successful
            if (stats && stats.reset > 0) {
                MDUX_setDocumentScale(100);
            }

            return JSON.stringify(stats);
        }
        return "ERROR:Reset scale function unavailable";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_rotationStateBridge() {
    if (MDUX_isCepSuspended()) {
        return JSON.stringify({ available: false, reason: "suspended", count: 0 });
    }
    try {
        MDUX_debugLog("[ROT-BRIDGE] MDUX_rotationStateBridge called");
        if (!MDUX_selectionHasPathItems(app.activeDocument.selection) && MDUX_selectionHasPlacedItems(app.activeDocument.selection)) {
            return JSON.stringify(MDUX_collectPlacedRotationSummary(app.activeDocument.selection));
        }
        if (!MDUX_requireMagicFinal()) {
            MDUX_debugLog("[ROT-BRIDGE] MDUX_requireMagicFinal returned false");
            return "ERROR:Rotation function unavailable";
        }
        if (app.documents.length && MDUX_selectionExceedsPathLimit(app.activeDocument.selection, MDUX_LIVE_SELECTION_PATH_LIMIT)) {
            return JSON.stringify({
                available: true,
                reason: "large-selection",
                rotations: [],
                formatted: "",
                count: 2
            });
        }
        MDUX_debugLog("[ROT-BRIDGE] MDUX_requireMagicFinal returned true, checking MDUX.getRotationOverrideSummary...");
        if (typeof MDUX !== "undefined" && MDUX.getRotationOverrideSummary) {
            MDUX_debugLog("[ROT-BRIDGE] Calling MDUX.getRotationOverrideSummary()...");
            var summary = MDUX.getRotationOverrideSummary();
            MDUX_debugLog("[ROT-BRIDGE] Got summary: " + JSON.stringify(summary).substring(0, 200));
            return JSON.stringify(summary);
        }
        MDUX_debugLog("[ROT-BRIDGE] MDUX.getRotationOverrideSummary not available");
        return "ERROR:Rotation function unavailable";
    } catch (e) {
        MDUX_debugLog("[ROT-BRIDGE] ERROR: " + e);
        return "ERROR:" + e;
    }
}

function MDUX_isolatePartsBridge() {
    try {
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Isolate parts function unavailable";
        }
        if (typeof MDUX !== "undefined" && MDUX.isolateDuctworkParts) {
            return MDUX.isolateDuctworkParts();
        }
        return "ERROR:Isolate parts function unavailable";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_isolateDuctworkBridge() {
    try {
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Isolate ductwork function unavailable";
        }
        if (typeof MDUX !== "undefined" && MDUX.isolateDuctworkLines) {
            return MDUX.isolateDuctworkLines();
        }
        return "ERROR:Isolate ductwork function unavailable";
    } catch (e) {
        return "ERROR:" + e;
    }
}

function MDUX_unlockDuctworkBridge() {
    try {
        if (!MDUX_requireMagicFinal()) {
            return "ERROR:Unlock function unavailable (bridge load failed)";
        }
        var ns = (typeof MDUX !== "undefined" && MDUX.unlockAllDuctworkLayers) ? MDUX : ($.global.MDUX || null);
        if (ns && ns.unlockAllDuctworkLayers) {
            return ns.unlockAllDuctworkLayers();
        }
        return "ERROR:Unlock function unavailable (namespace missing)";
    } catch (e) {
        return "ERROR:Unlock (exception): " + e;
    }
}

function MDUX_importGraphicStylesBridge() {
    try {
        if (app.documents.length === 0) {
            return "ERROR:No Illustrator document is open.";
        }
        var destDoc = app.activeDocument;
        var sourceFile = new File("E:/Work/Work/Floorplans/Ductwork Assets/DuctworkLines.ai");
        if (!sourceFile.exists) {
            return "ERROR:DuctworkLines.ai not found.";
        }

        var sourceDoc = null;
        try { $.writeln("[MDUX] Import styles: opening " + sourceFile.fsName); } catch (logOpen) { }
        try {
            sourceDoc = app.open(sourceFile);
            app.activeDocument = sourceDoc;
            try {
                for (var L = 0; L < sourceDoc.layers.length; L++) {
                    try { sourceDoc.layers[L].locked = false; } catch (eLock) { }
                    try { sourceDoc.layers[L].visible = true; } catch (eVis) { }
                }
            } catch (eIter) { }

            var items = null;
            try { items = sourceDoc.pageItems; } catch (eItems) { items = null; }
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
        } catch (copyErr) {
            try {
                if (sourceDoc) sourceDoc.close(SaveOptions.DONOTSAVECHANGES);
            } catch (closeErr) { }
            app.activeDocument = destDoc;
            try { $.writeln("[MDUX] Import styles error: " + copyErr); } catch (logCopy) { }
            return "ERROR:" + copyErr;
        }

        app.activeDocument = destDoc;
        destDoc.selection = null;

        // Use a specific layer name to hold template lines until Process Ductwork
        var tempLayerName = "__MDUX_STYLE_TEMPLATE_LINES__";
        var tempLayer = null;
        try {
            tempLayer = destDoc.layers.getByName(tempLayerName);
        } catch (findErr) {
            tempLayer = destDoc.layers.add();
            tempLayer.name = tempLayerName;
        }
        try { tempLayer.locked = false; } catch (lockErr) { }
        destDoc.activeLayer = tempLayer;
        app.executeMenuCommand("pasteInPlace");

        var pasted = null;
        try { pasted = destDoc.selection; } catch (selErr) { pasted = null; }
        if (pasted) {
            if (pasted.length === undefined) pasted = [pasted];

            // Position items at bottom-left of artboard, 200pt to the left of artboard edge
            // Right edge of items should be 200pt left of artboard's left edge
            var artboardIndex = destDoc.artboards.getActiveArtboardIndex();
            var artboardRect = destDoc.artboards[artboardIndex].artboardRect;
            // artboardRect = [left, top, right, bottom] (top > bottom in AI coordinates)
            var abLeft = artboardRect[0];
            var abTop = artboardRect[1];
            var abRight = artboardRect[2];
            var abBottom = artboardRect[3];

            // Calculate bounding box of all pasted items
            var allLeft = Infinity, allTop = -Infinity, allRight = -Infinity, allBottom = Infinity;
            for (var p = 0; p < pasted.length; p++) {
                try {
                    var bounds = pasted[p].geometricBounds;
                    if (bounds[0] < allLeft) allLeft = bounds[0];
                    if (bounds[1] > allTop) allTop = bounds[1];
                    if (bounds[2] > allRight) allRight = bounds[2];
                    if (bounds[3] < allBottom) allBottom = bounds[3];
                } catch (boundsErr) { }
            }

            var itemsWidth = allRight - allLeft;
            var itemsHeight = allTop - allBottom;

            // Target position: right edge of items at (artboard left - 200pt)
            // So items left edge should be at: (artboard left - 200 - itemsWidth)
            var targetX = abLeft - 200 - itemsWidth;
            // Target Y: items bottom at artboard bottom
            var targetY = abBottom;

            // Calculate translation needed
            var deltaX = targetX - allLeft;
            var deltaY = targetY - allBottom;

            for (var p = 0; p < pasted.length; p++) {
                try {
                    pasted[p].translate(deltaX, deltaY);
                } catch (moveErr) {
                    try { $.writeln("[MDUX] Error moving template item: " + moveErr); } catch (logMove) { }
                }
            }
            try { $.writeln("[MDUX] Positioned " + pasted.length + " template line(s) at bottom-left of artboard (200pt offset)"); } catch (logMoved) { }
        }
        destDoc.selection = null;

        // Delete the empty temp layer (items went to their respective layers)
        try {
            tempLayer.locked = false;
            tempLayer.remove();
            $.writeln("[MDUX] Removed empty temp layer __MDUX_STYLE_TEMPLATE_LINES__");
        } catch (removeErr) {
            try { $.writeln("[MDUX] Could not remove temp layer: " + removeErr); } catch (logRemove) { }
        }
        try { app.redraw(); } catch (redErr) { }
        try { $.writeln("[MDUX] Import styles completed successfully"); } catch (logDone) { }
        return "Graphic styles imported.";
    } catch (e) {
        try { $.writeln("[MDUX] Import styles exception: " + e); } catch (logFinal) { }
        return "ERROR:Import styles (exception): " + e;
    }
}

// ============================================
// V3 LAYER MANAGEMENT FUNCTIONS
// ============================================

/**
 * Migrate legacy "Scale Factor Container Layer" to document tags
 * This is a one-time migration for old documents
 */
function MDUX_migrateLegacyScaleFactor() {
    MDUX_debugLog("[V3] MDUX_migrateLegacyScaleFactor called");
    try {
        var doc = app.activeDocument;

        // Check if legacy layer exists
        var legacyLayer = null;
        try {
            legacyLayer = doc.layers.getByName("Scale Factor Container Layer");
        } catch (e) {
            return "No legacy Scale Factor Container Layer found";
        }

        if (!legacyLayer) {
            return "No legacy Scale Factor Container Layer found";
        }

        // Try to extract scale factor from the layer
        var scaleValue = null;

        // Check layer name for embedded scale info
        if (legacyLayer.name.indexOf("=") !== -1) {
            var parts = legacyLayer.name.split("=");
            if (parts.length > 1) {
                var parsed = parseFloat(parts[1]);
                if (!isNaN(parsed) && parsed > 0) {
                    scaleValue = parsed;
                }
            }
        }

        // Check for text items with scale value
        if (!scaleValue && legacyLayer.textFrames && legacyLayer.textFrames.length > 0) {
            for (var i = 0; i < legacyLayer.textFrames.length; i++) {
                var text = legacyLayer.textFrames[i].contents || "";
                var parsed = parseFloat(text);
                if (!isNaN(parsed) && parsed > 0 && parsed < 1000) {
                    scaleValue = parsed;
                    break;
                }
            }
        }

        // Default to 100 if no scale found
        if (!scaleValue) scaleValue = 100;

        // Save to document tags (new system)
        MDUX_setScaleFactorTag(doc, scaleValue);

        // Optionally remove or hide the legacy layer
        try {
            legacyLayer.visible = false;
            legacyLayer.locked = true;
        } catch (e) {}

        MDUX_debugLog("[LEGACY-MIGRATION] Migrated scale factor " + scaleValue + " to document tags");
        return "Migrated scale factor (" + scaleValue + "%) to document tags";

    } catch (e) {
        MDUX_debugLog("[LEGACY-MIGRATION] Error: " + e);
        return "Error: " + e.message;
    }
}

/**
 * Rename "Floorplan" or "Layer 1" to "Render" (with increment on conflict)
 */
function MDUX_renameFloorplanToRender() {
    MDUX_debugLog("[V3] MDUX_renameFloorplanToRender called");
    try {
        var doc = app.activeDocument;

        // Find the layer to rename
        var sourceLayer = null;
        var sourceNames = ["Floorplan", "Layer 1", "Layer1"];

        for (var i = 0; i < sourceNames.length; i++) {
            try {
                sourceLayer = doc.layers.getByName(sourceNames[i]);
                if (sourceLayer) break;
            } catch (e) {}
        }

        if (!sourceLayer) {
            return "No Floorplan or Layer 1 found to rename";
        }

        // Determine target name (Render, Render 2, Render 3, etc.)
        var targetName = "Render";
        var suffix = 1;

        while (true) {
            var testName = suffix === 1 ? "Render" : "Render " + suffix;
            var exists = false;

            try {
                var existing = doc.layers.getByName(testName);
                if (existing && existing !== sourceLayer) {
                    exists = true;
                }
            } catch (e) {
                // Layer doesn't exist, we can use this name
            }

            if (!exists) {
                targetName = testName;
                break;
            }

            suffix++;
            if (suffix > 100) {
                return "Too many Render layers exist";
            }
        }

        // Rename the layer
        var oldName = sourceLayer.name;
        sourceLayer.name = targetName;

        MDUX_debugLog("[RENAME-LAYER] Renamed '" + oldName + "' to '" + targetName + "'");
        return "Renamed '" + oldName + "' to '" + targetName + "'";

    } catch (e) {
        MDUX_debugLog("[RENAME-LAYER] Error: " + e);
        return "Error: " + e.message;
    }
}

/**
 * Organize layers - create standard layers and apply correct colors
 */
function MDUX_organizeLayers() {
    MDUX_debugLog("[V3] MDUX_organizeLayers called");

    // Run legacy migration first
    MDUX_migrateLegacyScaleFactor();

    // Then create/ensure standard layers
    return MDUX_createLayersBridge();
}

function MDUX_createLayersBridge() {
    try {
        if (app.documents.length === 0) {
            return "ERROR:No Illustrator document is open.";
        }
        var doc = app.activeDocument;

        // Layer definitions with names and colors [R, G, B]
        var desired = [
            { name: "Scale Factor Container Layer", color: [128, 128, 128] },
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
        ];

        // First pass: Create missing layers and set colors on ALL layers
        for (var i = 0; i < desired.length; i++) {
            var entry = desired[i];
            var layer = null;
            try { layer = doc.layers.getByName(entry.name); } catch (eFind) {
                try {
                    layer = doc.layers.add();
                    layer.name = entry.name;
                    try { $.writeln("[MDUX] Created missing layer: " + entry.name); } catch (logCreate) { }
                } catch (eCreate) {
                    layer = null;
                }
            }

            // Set color on ALL layers (existing or newly created)
            if (layer) {
                try {
                    var col = new RGBColor();
                    col.red = entry.color[0];
                    col.green = entry.color[1];
                    col.blue = entry.color[2];
                    layer.color = col;
                } catch (eColor) {
                    // Ignore color setting errors
                }
            }
        }

        // Second pass: Reorder layers to match desired sequence
        for (var idx = desired.length - 1; idx >= 0; idx--) {
            try {
                var target = doc.layers.getByName(desired[idx].name);
                if (!target) continue;
                var prevLocked = null;
                try { prevLocked = target.locked; target.locked = false; } catch (eLock) { }
                try { target.visible = true; } catch (eVisible) { }
                try {
                    target.move(doc.layers[0], ElementPlacement.PLACEBEFORE);
                } catch (eMove) { }
                try {
                    if (prevLocked !== null) target.locked = prevLocked;
                } catch (eRestore) { }
            } catch (eTarget) { }
        }

        try { app.redraw(); } catch (eRedraw) { }
        try { $.writeln("[MDUX] Ensure standard layers completed with colors"); } catch (logDone) { }
        return "Standard ductwork layers ensured with colors.";
    } catch (e) {
        try { $.writeln("[MDUX] Create layers exception: " + e); } catch (logErr) { }
        return "ERROR:Create layers (exception): " + e;
    }
}

/**
 * Helper function to set the proper name for a PlacedItem based on its file.
 * Extracts the piece name from the file path and sets it as "PieceName (Linked)"
 * This ensures the item can be found by existingItems filters in magic-final.jsx
 */
function MDUX_setPlacedItemName(item, file) {
    try {
        if (!item || !file) return;

        // Get file name without extension
        var fileName = file.name || "";
        var baseName = fileName.replace(/\.[^.]*$/, ""); // Remove extension

        // Remove " Emory" suffix if present (so "Unit Emory" becomes "Unit")
        baseName = baseName.replace(/ Emory$/, "");

        // Set name in format "PieceName (Linked)" to match magic-final.jsx convention
        item.name = baseName + " (Linked)";

        $.writeln("[NAME] Set PlacedItem name to: " + item.name);
    } catch (e) {
        $.writeln("[NAME] Error setting PlacedItem name: " + e);
    }
}

function MDUX_runEmoryDuctwork(createRegisterWires) {
    try {
        if (!app.documents.length) {
            return "ERROR:No document open";
        }

        var doc = app.activeDocument;
        var REGISTER_WIRE_TAG = "MD:REGISTER_WIRE";
        var WIRE_CONNECTION_TOLERANCE = 50; // Increased tolerance to 50px
        var swappedCount = 0;
        var wireCount = 0;

        // Default to false if not specified
        if (typeof createRegisterWires === 'undefined') {
            createRegisterWires = false;
        }

        $.writeln("[EMORY] Starting Emory ductwork processing");

        // Step 1: Swap all placed items to Emory versions
        var allItems = doc.pageItems;
        for (var i = 0; i < allItems.length; i++) {
            var item = allItems[i];
            if (item.typename === "PlacedItem") {
                try {
                    var currentFile = item.file;
                    if (currentFile) {
                        var currentPath = currentFile.fsName;
                        if (currentPath.indexOf(" Emory") === -1) {
                            var emoryPath = currentPath.replace(/(\.[^.]+)$/, " Emory$1");
                            var emoryFile = new File(emoryPath);
                            if (emoryFile.exists) {
                                item.file = emoryFile;
                                MDUX_setPlacedItemName(item, emoryFile);
                                swappedCount++;
                            }
                        }
                    }
                } catch (e) { }
            }
        }

        // Step 1.5: Place/swap register images at anchors based on layer
        $.writeln("[EMORY] Placing register images at anchors based on layer");
        var registerLayerMap = {
            "Square Registers": "Square Register Emory",
            "Rectangular Registers": "Rectangular Register Emory",
            "Exhaust Registers": "Exhaust Register",
            "Secondary Exhaust Registers": "Secondary Exhaust Register"
        };

        for (var layerName in registerLayerMap) {
            var targetImageName = registerLayerMap[layerName];
            var registerLayer = null;

            // Find the layer
            try {
                for (var li = 0; li < doc.layers.length; li++) {
                    if (doc.layers[li].name === layerName) {
                        registerLayer = doc.layers[li];
                        break;
                    }
                }
            } catch (e) { }

            if (!registerLayer || !registerLayer.pathItems) continue;

            // Get all anchor points on this layer
            for (var pi = 0; pi < registerLayer.pathItems.length; pi++) {
                var regPath = registerLayer.pathItems[pi];
                if (!regPath || !regPath.pathPoints || regPath.pathPoints.length === 0) continue;

                var anchor = regPath.pathPoints[0].anchor;
                var anchorPos = { x: anchor[0], y: anchor[1] };

                // Check if there's already a PlacedItem near this anchor
                var foundPlacedItem = null;
                var searchTolerance = 5; // 5px tolerance

                for (var ai = 0; ai < allItems.length; ai++) {
                    var checkItem = allItems[ai];
                    if (checkItem.typename !== "PlacedItem") continue;

                    try {
                        var itemPos = { x: checkItem.position[0], y: checkItem.position[1] };
                        var dx = itemPos.x - anchorPos.x;
                        var dy = itemPos.y - anchorPos.y;
                        var dist = Math.sqrt(dx * dx + dy * dy);

                        if (dist < searchTolerance) {
                            foundPlacedItem = checkItem;
                            break;
                        }
                    } catch (e) { }
                }

                // Swap or place the correct Emory register image
                try {
                    // Find the Emory register file based on layer
                    var basePath = "E:\\Work\\Work\\Floorplans\\Custom Sketchup, Illustrator and Photoshop Scripts and Extensions\\Illustrator\\";
                    var emoryRegisterPath = basePath + "Ductwork Pieces Emory\\" + targetImageName + ".ai";
                    var emoryRegisterFile = new File(emoryRegisterPath);

                    if (!emoryRegisterFile.exists) {
                        // Try alternate path
                        emoryRegisterPath = basePath + "Ductwork Pieces\\" + targetImageName + ".ai";
                        emoryRegisterFile = new File(emoryRegisterPath);
                    }

                    if (emoryRegisterFile.exists) {
                        if (foundPlacedItem) {
                            // Swap existing placed item to correct Emory register
                            var currentFileName = foundPlacedItem.file ? foundPlacedItem.file.name : "";
                            if (currentFileName !== targetImageName + ".ai") {
                                foundPlacedItem.file = emoryRegisterFile;
                                MDUX_setPlacedItemName(foundPlacedItem, emoryRegisterFile);
                                swappedCount++;
                                $.writeln("[EMORY] Swapped register at [" + anchorPos.x.toFixed(1) + "," + anchorPos.y.toFixed(1) + "] to " + targetImageName);
                            }
                        } else {
                            // Place new Emory register at anchor
                            var newPlaced = registerLayer.placedItems.add();
                            newPlaced.file = emoryRegisterFile;
                            MDUX_setPlacedItemName(newPlaced, emoryRegisterFile);
                            newPlaced.position = [anchorPos.x, anchorPos.y];
                            swappedCount++;
                            $.writeln("[EMORY] Placed " + targetImageName + " at [" + anchorPos.x.toFixed(1) + "," + anchorPos.y.toFixed(1) + "]");
                        }
                    } else {
                        $.writeln("[EMORY] Could not find Emory register file: " + emoryRegisterPath);
                    }
                } catch (e) {
                    $.writeln("[EMORY] Error placing/swapping register: " + e);
                }
            }
        }

        // Step 2: Generate register wires (only if enabled)
        if (createRegisterWires) {
            $.writeln("[EMORY] Register wire creation enabled");
            var ductworkPaths = [];
            var registerPoints = [];
            var ignoreAnchors = [];

            $.writeln("[EMORY] Scanning for ductwork paths, registers, and ignore anchors");

            // Find ductwork paths
            for (var i = 0; i < allItems.length; i++) {
                var item = allItems[i];
                if (item.typename === "PathItem") {
                    var layerName = "";
                    try { layerName = item.layer.name; } catch (e) { }
                    if (layerName && (layerName.indexOf("Ductwork") !== -1 || layerName.indexOf("ductwork") !== -1)) {
                        ductworkPaths.push(item);
                    }
                }
            }

            // Find register anchor points on register layers
            var registerLayerNames = ["Square Registers", "Rectangular Registers", "Exhaust Registers", "Secondary Exhaust Registers", "Registers"];
            for (var layerIdx = 0; layerIdx < registerLayerNames.length; layerIdx++) {
                var layerName = registerLayerNames[layerIdx];
                var registerLayer = null;
                try {
                    for (var li = 0; li < doc.layers.length; li++) {
                        if (doc.layers[li].name === layerName) {
                            registerLayer = doc.layers[li];
                            break;
                        }
                    }
                } catch (e) { }

                if (registerLayer && registerLayer.pathItems) {
                    for (var pi = 0; pi < registerLayer.pathItems.length; pi++) {
                        var regPath = registerLayer.pathItems[pi];
                        if (regPath && regPath.pathPoints && regPath.pathPoints.length > 0) {
                            // Use first anchor point as register location
                            var anchor = regPath.pathPoints[0].anchor;
                            registerPoints.push({ x: anchor[0], y: anchor[1] });
                        }
                    }
                }
            }

            // Find ignore anchors on the ignore layer
            var ignoreLayer = null;
            try {
                for (var li = 0; li < doc.layers.length; li++) {
                    if (doc.layers[li].name === "Ignore" || doc.layers[li].name === "ignore") {
                        ignoreLayer = doc.layers[li];
                        break;
                    }
                }
            } catch (e) { }

            if (ignoreLayer && ignoreLayer.pathItems) {
                for (var pi = 0; pi < ignoreLayer.pathItems.length; pi++) {
                    var ignorePath = ignoreLayer.pathItems[pi];
                    if (ignorePath && ignorePath.pathPoints) {
                        for (var pp = 0; pp < ignorePath.pathPoints.length; pp++) {
                            var anchor = ignorePath.pathPoints[pp].anchor;
                            ignoreAnchors.push({ x: anchor[0], y: anchor[1] });
                        }
                    }
                }
            }

            $.writeln("[EMORY] Found " + ductworkPaths.length + " ductwork paths");
            $.writeln("[EMORY] Found " + registerPoints.length + " register points");
            $.writeln("[EMORY] Found " + ignoreAnchors.length + " ignore anchors");

            for (var i = 0; i < ductworkPaths.length; i++) {
                var path = ductworkPaths[i];
                if (!path.pathPoints || path.pathPoints.length < 2) continue; // Need at least 2 points
                var layer = path.layer;
                if (!layer) continue;

                // Check both ends of the ductwork path
                var endpoints = [
                    { endIdx: 0, prevIdx: 1, name: "start" },
                    { endIdx: path.pathPoints.length - 1, prevIdx: path.pathPoints.length - 2, name: "end" }
                ];

                for (var j = 0; j < endpoints.length; j++) {
                    var epInfo = endpoints[j];
                    if (epInfo.prevIdx >= path.pathPoints.length || epInfo.prevIdx < 0) continue;

                    var endAnchor = path.pathPoints[epInfo.endIdx].anchor;
                    var prevAnchor = path.pathPoints[epInfo.prevIdx].anchor;
                    var endPoint = { x: endAnchor[0], y: endAnchor[1] };
                    var prevPoint = { x: prevAnchor[0], y: prevAnchor[1] };

                    // Check if endpoint is near an ignore anchor (if so, SKIP it)
                    var nearIgnore = false;
                    for (var k = 0; k < ignoreAnchors.length; k++) {
                        var ignoreAnchor = ignoreAnchors[k];
                        var dx = ignoreAnchor.x - endPoint.x;
                        var dy = ignoreAnchor.y - endPoint.y;
                        var dist = Math.sqrt(dx * dx + dy * dy);
                        if (dist < 5) { // Within 5px of ignore anchor
                            nearIgnore = true;
                            $.writeln("[EMORY] Skipping endpoint at [" + endPoint.x.toFixed(1) + "," + endPoint.y.toFixed(1) + "] - near ignore anchor (dist=" + dist.toFixed(1) + ")");
                            break;
                        }
                    }

                    if (nearIgnore) continue;

                    // Check if endpoint is near a register
                    var nearRegister = false;
                    for (var k = 0; k < registerPoints.length; k++) {
                        var register = registerPoints[k];
                        var dx = register.x - endPoint.x;
                        var dy = register.y - endPoint.y;
                        var dist = Math.sqrt(dx * dx + dy * dy);
                        if (dist < WIRE_CONNECTION_TOLERANCE) {
                            nearRegister = true;
                            $.writeln("[EMORY] Endpoint at [" + endPoint.x.toFixed(1) + "," + endPoint.y.toFixed(1) + "] is near register at [" + register.x.toFixed(1) + "," + register.y.toFixed(1) + "] (dist=" + dist.toFixed(1) + ")");
                            break;
                        }
                    }

                    if (nearRegister) {
                        try {
                            $.writeln("[EMORY] ===== WIRE CREATION DEBUG =====");
                            $.writeln("[EMORY] Path has " + path.pathPoints.length + " points");
                            $.writeln("[EMORY] Endpoint info: name=" + epInfo.name + ", endIdx=" + epInfo.endIdx + ", prevIdx=" + epInfo.prevIdx);
                            $.writeln("[EMORY] prevPoint: [" + prevPoint.x.toFixed(1) + "," + prevPoint.y.toFixed(1) + "]");
                            $.writeln("[EMORY] endPoint: [" + endPoint.x.toFixed(1) + "," + endPoint.y.toFixed(1) + "]");

                            // Create wire from previous point to endpoint (last segment of ductwork)
                            var wireDX = endPoint.x - prevPoint.x;
                            var wireDY = endPoint.y - prevPoint.y;
                            var wireLen = Math.sqrt(wireDX * wireDX + wireDY * wireDY);

                            $.writeln("[EMORY] Wire vector: dx=" + wireDX.toFixed(2) + ", dy=" + wireDY.toFixed(2) + ", length=" + wireLen.toFixed(2));

                            if (wireLen < 5) {
                                $.writeln("[EMORY] Skipping wire - too short (< 5px): " + wireLen.toFixed(2));
                                continue;
                            }

                            // Create wire path from previous point to endpoint
                            var wirePath = layer.pathItems.add();
                            wirePath.setEntirePath([[prevPoint.x, prevPoint.y], [endPoint.x, endPoint.y]]);

                            $.writeln("[EMORY] Wire path created, checking points...");
                            if (wirePath.pathPoints && wirePath.pathPoints.length === 2) {
                                var wp0 = wirePath.pathPoints[0].anchor;
                                var wp1 = wirePath.pathPoints[1].anchor;
                                $.writeln("[EMORY] Wire point 0: [" + wp0[0].toFixed(1) + "," + wp0[1].toFixed(1) + "]");
                                $.writeln("[EMORY] Wire point 1: [" + wp1[0].toFixed(1) + "," + wp1[1].toFixed(1) + "]");
                                var actualDX = wp1[0] - wp0[0];
                                var actualDY = wp1[1] - wp0[1];
                                var actualLen = Math.sqrt(actualDX * actualDX + actualDY * actualDY);
                                $.writeln("[EMORY] Actual wire length: " + actualLen.toFixed(2) + "px");
                            } else {
                                $.writeln("[EMORY] ERROR: Wire has " + (wirePath.pathPoints ? wirePath.pathPoints.length : "NO") + " points!");
                            }

                            // Style the wire: stroke-only (explicitly clear fills) - from Emory script
                            wirePath.closed = false;

                            // Explicitly remove any graphic style that might be applied
                            try {
                                wirePath.unapplyAll();
                            } catch (e) { }

                            wirePath.stroked = true;
                            wirePath.strokeWidth = 3;
                            try {
                                var wireColor = new RGBColor();
                                wireColor.red = 0;
                                wireColor.green = 0;
                                wireColor.blue = 255;
                                wirePath.strokeColor = wireColor;
                            } catch (eWireColor) { }
                            wirePath.strokeCap = StrokeCap.ROUNDENDCAP;
                            wirePath.strokeJoin = StrokeJoin.ROUNDENDJOIN;

                            wirePath.filled = false;
                            try {
                                var noColor = new NoColor();
                                wirePath.fillColor = noColor;
                            } catch (e) { }

                            // Add curved handles - using Emory script logic
                            var wirePoints = wirePath.pathPoints;
                            if (wirePoints && wirePoints.length === 2) {
                                // Get segment direction (from previous segment if possible)
                                var segDirection = { x: wireDX / wireLen, y: wireDY / wireLen };

                                // Try to get the previous segment's direction for smoother curve
                                if (path.pathPoints.length > 2 && epInfo.prevIdx > 0 && epInfo.prevIdx < path.pathPoints.length - 1) {
                                    var prevPrevAnchor = path.pathPoints[epInfo.prevIdx - (epInfo.endIdx > epInfo.prevIdx ? -1 : 1)].anchor;
                                    var prevPrevPoint = { x: prevPrevAnchor[0], y: prevPrevAnchor[1] };
                                    var prevSegDX = prevPoint.x - prevPrevPoint.x;
                                    var prevSegDY = prevPoint.y - prevPrevPoint.y;
                                    var prevSegLen = Math.sqrt(prevSegDX * prevSegDX + prevSegDY * prevSegDY);
                                    if (prevSegLen > 0) {
                                        segDirection = { x: prevSegDX / prevSegLen, y: prevSegDY / prevSegLen };
                                    }
                                }

                                // Scale handles based on wire length - from Emory script
                                var handleLen;
                                if (wireLen < 10) {
                                    handleLen = wireLen * 0.05;
                                } else if (wireLen < 20) {
                                    var t = (wireLen - 10) / 10;
                                    handleLen = wireLen * (0.05 + t * 0.05);
                                } else if (wireLen < 30) {
                                    var t = (wireLen - 20) / 10;
                                    handleLen = wireLen * (0.10 + t * 0.05);
                                } else if (wireLen < 50) {
                                    var t = (wireLen - 30) / 20;
                                    handleLen = wireLen * (0.15 + t * 0.10);
                                } else {
                                    handleLen = Math.min(wireLen * 0.30, 30);
                                }
                                handleLen = Math.max(handleLen, 0.5);

                                var startPointInfo = wirePoints[0];
                                var endPointInfo = wirePoints[1];

                                // Start handle: extend along the previous segment direction
                                if (startPointInfo) {
                                    startPointInfo.leftDirection = [prevPoint.x, prevPoint.y];
                                    startPointInfo.rightDirection = [
                                        prevPoint.x + segDirection.x * handleLen,
                                        prevPoint.y + segDirection.y * handleLen
                                    ];
                                    try { startPointInfo.pointType = PointType.SMOOTH; } catch (eWireStartType) { }
                                }

                                // End handle: point downward (inverted)
                                if (endPointInfo) {
                                    endPointInfo.leftDirection = [
                                        endPoint.x,
                                        endPoint.y + handleLen * 0.6
                                    ];
                                    endPointInfo.rightDirection = [endPoint.x, endPoint.y];
                                    try { endPointInfo.pointType = PointType.SMOOTH; } catch (eWireEndType) { }
                                }
                            }

                            wirePath.name = "Register Wire";
                            wirePath.note = REGISTER_WIRE_TAG;
                            wireCount++;
                            $.writeln("[EMORY] Wire created successfully");
                        } catch (eWire) {
                            $.writeln("[EMORY] Wire creation error: " + eWire);
                        }
                    }
                }
            }
        } else {
            $.writeln("[EMORY] Register wire creation skipped (disabled)");
        }

        var message = "Swapped " + swappedCount + " items to Emory versions";
        if (createRegisterWires) {
            message += " and created " + wireCount + " register wires";
        }
        message += ".";

        $.writeln("[EMORY] Complete: " + message);
        return message;
    } catch (e) {
        $.writeln("[EMORY] Error: " + e);
        return "ERROR:" + e.toString();
    }
}

function MDUX_moveToLayerBridge(optionsJSON) {
    try {
        $.writeln("[MOVE] MDUX_moveToLayerBridge called");
        $.writeln("[MOVE] Input JSON: " + optionsJSON);

        if (!app.documents.length) {
            $.writeln("[MOVE] ERROR: No documents open");
            return JSON.stringify({ itemsMoved: 0, anchorsMoved: 0, reason: 'no-document' });
        }

        var doc = app.activeDocument;

        // Clean up any leftover temp layers from previous errors
        try {
            for (var i = doc.layers.length - 1; i >= 0; i--) {
                var layerName = doc.layers[i].name;
                if (layerName.indexOf('MDUX_TEMP_') === 0 || layerName === 'Scale Factor Container Layer') {
                    $.writeln("[MOVE] Cleaning up temp layer: " + layerName);
                    doc.layers[i].remove();
                }
            }
        } catch (e) {
            $.writeln("[MOVE] Error cleaning temp layers: " + e);
        }

        var selection = doc.selection;
        $.writeln("[MOVE] Selection length: " + (selection ? selection.length : 0));

        if (!selection || selection.length === 0) {
            $.writeln("[MOVE] ERROR: No selection");
            return JSON.stringify({ itemsMoved: 0, anchorsMoved: 0, reason: 'no-selection' });
        }

        // Get accurate selected anchor positions from C++ SDK (handles Direct Selection properly)
        var cppSelectedAnchors = [];
        try {
            var cppResult = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", "action=get-selected-anchors");
            if (cppResult) {
                var cppData = JSON.parse(cppResult);
                if (cppData && cppData.ok && cppData.points && cppData.points.length > 0) {
                    cppSelectedAnchors = cppData.points;
                    $.writeln("[MOVE] C++ detected " + cppSelectedAnchors.length + " selected anchor point(s)");
                }
            }
        } catch (eCpp) {
            $.writeln("[MOVE] C++ anchor detection unavailable: " + eCpp);
        }

        var options = JSON.parse(optionsJSON);
        var targetLayerName = options.layerName;
        var fileBaseName = options.fileBaseName;
        $.writeln("[MOVE] Target layer: " + targetLayerName);
        $.writeln("[MOVE] File base name: " + fileBaseName);

        // Get or create target layer
        var targetLayer = null;
        try {
            targetLayer = doc.layers.getByName(targetLayerName);
            $.writeln("[MOVE] Found existing layer: " + targetLayerName);
        } catch (e) {
            targetLayer = doc.layers.add();
            targetLayer.name = targetLayerName;
            $.writeln("[MOVE] Created new layer: " + targetLayerName);
        }

        var isIgnoreLayer = (targetLayerName === 'Ignore' || targetLayerName === 'Ignored');
        var wasLocked = targetLayer.locked;
        var wasVisible = targetLayer.visible;
        $.writeln("[MOVE] Layer locked: " + wasLocked + ", visible: " + wasVisible);

        // Unlock and show target layer temporarily
        if (isIgnoreLayer || targetLayer.locked) {
            targetLayer.locked = false;
            $.writeln("[MOVE] Unlocked target layer");
        }
        if (!targetLayer.visible) {
            targetLayer.visible = true;
            $.writeln("[MOVE] Made target layer visible");
        }

        var itemsMoved = 0;
        var anchorsMoved = 0;
        var itemsSkipped = 0;
        var filePath = null;
        var artPlacedPositions = []; // Track positions where art has been placed to avoid duplicates
        var newlyCreatedItems = []; // Track all items created during move for re-selection
        var movedPositions = []; // Track positions where items were moved/created

        // Helper to check if art was already placed at a position (within 5px tolerance)
        function wasArtPlacedNear(x, y) {
            for (var ap = 0; ap < artPlacedPositions.length; ap++) {
                var dx = artPlacedPositions[ap].x - x;
                var dy = artPlacedPositions[ap].y - y;
                if (Math.sqrt(dx * dx + dy * dy) < 5) {
                    return true;
                }
            }
            return false;
        }

        if (fileBaseName) {
            var COMPONENT_FILES_PATH = 'E:/Work/Work/Floorplans/Ductwork Assets/';
            filePath = new File(COMPONENT_FILES_PATH + fileBaseName);
            $.writeln("[MOVE] File path: " + filePath.fsName);
            if (!filePath.exists) {
                $.writeln("[MOVE] WARNING: File does not exist");
                filePath = null;
            } else {
                $.writeln("[MOVE] File exists");
            }
        }

        // Define valid ductwork parts layers (where anchors/images go)
        var validDuctworkLayers = [
            'Units',
            'Square Registers',
            'Rectangular Registers',
            'Circular Registers',
            'Exhaust Registers',
            'Secondary Exhaust Registers',
            'Thermostats',
            'Ignore',
            'Ignored'
        ];

        // Define ductwork COLOR layers (lines, NOT parts - should not be moved)
        var ductworkColorLayers = [
            'Green Ductwork',
            'Light Green Ductwork',
            'Blue Ductwork',
            'Orange Ductwork',
            'Light Orange Ductwork',
            'Thermostat Lines'
        ];

        // Helper function to check if layer is valid ductwork parts layer
        function isValidDuctworkLayer(layerName) {
            for (var i = 0; i < validDuctworkLayers.length; i++) {
                if (validDuctworkLayers[i] === layerName) {
                    return true;
                }
            }
            return false;
        }

        // Helper function to check if layer is a ductwork color/line layer (NOT parts)
        // Case-insensitive and handles Emory variants
        function isDuctworkColorLayer(layerName) {
            if (!layerName) return false;
            // ES3 doesn't have trim(), use regex instead
            var lowerName = layerName.toLowerCase().replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');
            // Check for standard names and Emory variants
            var patterns = [
                'green ductwork', 'light green ductwork', 'blue ductwork',
                'orange ductwork', 'light orange ductwork', 'thermostat lines',
                'green ductwork emory', 'light green ductwork emory', 'blue ductwork emory',
                'orange ductwork emory', 'light orange ductwork emory', 'thermostat lines emory'
            ];
            for (var i = 0; i < patterns.length; i++) {
                if (lowerName === patterns[i] || lowerName.indexOf(patterns[i]) === 0) {
                    return true;
                }
            }
            // Also check if it contains "ductwork" but NOT register/unit keywords
            if (lowerName.indexOf('ductwork') !== -1 &&
                lowerName.indexOf('register') === -1 &&
                lowerName.indexOf('unit') === -1 &&
                lowerName.indexOf('exhaust') === -1 &&
                lowerName.indexOf('thermostat') === -1) {
                return true;
            }
            return false;
        }

        function isReplacementTargetItem(item, layerName) {
            if (!item || !layerName) return false;
            try {
                if (item.typename === 'PlacedItem') {
                    return isValidDuctworkLayer(layerName);
                }
                if (item.typename === 'PathItem') {
                    return item.pathPoints && item.pathPoints.length === 1 && isValidDuctworkLayer(layerName);
                }
            } catch (eReplacementTarget) {}
            return false;
        }

        var prioritizePartReplacement = false;
        for (var selIdx = 0; selIdx < selection.length; selIdx++) {
            var selectedItem = selection[selIdx];
            var selectedLayerName = null;
            try { selectedLayerName = selectedItem && selectedItem.layer ? selectedItem.layer.name : null; } catch (eSelectedLayer) {}
            if (isReplacementTargetItem(selectedItem, selectedLayerName)) {
                prioritizePartReplacement = true;
                break;
            }
        }
        $.writeln("[MOVE] prioritizePartReplacement=" + (prioritizePartReplacement ? 1 : 0));

        // Helper function to find and remove existing anchors at a position from ALL ductwork parts layers
        // Returns the removed anchor's layer name if one was found and removed, null otherwise
        function removeExistingAnchorAtPosition(x, y, tolerance, excludeLayer) {
            var removedFrom = null;
            tolerance = tolerance || 5;
            for (var layerIdx = 0; layerIdx < doc.layers.length; layerIdx++) {
                var layer = doc.layers[layerIdx];
                var layerName = layer.name;
                // Only check ductwork parts layers
                if (!isValidDuctworkLayer(layerName)) continue;
                // Skip the exclude layer (typically the target layer)
                if (excludeLayer && layerName === excludeLayer) continue;

                try {
                    for (var pathIdx = layer.pathItems.length - 1; pathIdx >= 0; pathIdx--) {
                        var path = layer.pathItems[pathIdx];
                        if (path.pathPoints && path.pathPoints.length === 1) {
                            var anchorPt = path.pathPoints[0].anchor;
                            var dx = anchorPt[0] - x;
                            var dy = anchorPt[1] - y;
                            var dist = Math.sqrt(dx * dx + dy * dy);
                            if (dist <= tolerance) {
                                removedFrom = layerName;
                                path.remove();
                                $.writeln("[MOVE] Removed existing anchor at [" + x.toFixed(2) + ", " + y.toFixed(2) + "] from layer '" + layerName + "'");
                                // Continue checking in case there are duplicates on other layers
                            }
                        }
                    }
                } catch (eRemove) {
                    $.writeln("[MOVE] Error removing anchor from layer '" + layerName + "': " + eRemove);
                }
            }
            return removedFrom;
        }

        // Helper function to check if an anchor already exists at position on target layer
        function anchorExistsOnLayer(layer, x, y, tolerance) {
            tolerance = tolerance || 5;
            try {
                for (var pathIdx = 0; pathIdx < layer.pathItems.length; pathIdx++) {
                    var path = layer.pathItems[pathIdx];
                    if (path.pathPoints && path.pathPoints.length === 1) {
                        var anchorPt = path.pathPoints[0].anchor;
                        var dx = anchorPt[0] - x;
                        var dy = anchorPt[1] - y;
                        var dist = Math.sqrt(dx * dx + dy * dy);
                        if (dist <= tolerance) {
                            return true;
                        }
                    }
                }
            } catch (e) {}
            return false;
        }

        // Helper function to check if PlacedItem (art) exists at position on a layer
        function artExistsOnLayer(layer, x, y, tolerance) {
            tolerance = tolerance || 10;
            try {
                for (var i = 0; i < layer.placedItems.length; i++) {
                    var item = layer.placedItems[i];
                    var bounds = item.geometricBounds;
                    var centerX = (bounds[0] + bounds[2]) / 2;
                    var centerY = (bounds[1] + bounds[3]) / 2;
                    var dx = centerX - x;
                    var dy = centerY - y;
                    var dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist <= tolerance) {
                        return true;
                    }
                }
            } catch (e) {}
            return false;
        }

        // Helper function to remove existing PlacedItems (art) at position from ALL ductwork parts layers
        // Skips art at positions where new art was just placed in this batch (artPlacedPositions)
        function removeArtFromAllDuctworkLayers(x, y, tolerance) {
            tolerance = tolerance || 10;
            var totalRemoved = 0;
            for (var layerIdx = 0; layerIdx < doc.layers.length; layerIdx++) {
                var layer = doc.layers[layerIdx];
                var layerName = layer.name;
                // Only check ductwork parts layers
                if (!isValidDuctworkLayer(layerName)) continue;

                try {
                    for (var i = layer.placedItems.length - 1; i >= 0; i--) {
                        var item = layer.placedItems[i];
                        var bounds = item.geometricBounds;
                        var centerX = (bounds[0] + bounds[2]) / 2;
                        var centerY = (bounds[1] + bounds[3]) / 2;
                        var dx = centerX - x;
                        var dy = centerY - y;
                        var dist = Math.sqrt(dx * dx + dy * dy);
                        if (dist <= tolerance) {
                            // Don't remove art that was JUST placed in this batch
                            var justPlaced = false;
                            for (var jpIdx = 0; jpIdx < artPlacedPositions.length; jpIdx++) {
                                var jpDx = centerX - artPlacedPositions[jpIdx].x;
                                var jpDy = centerY - artPlacedPositions[jpIdx].y;
                                if (Math.sqrt(jpDx * jpDx + jpDy * jpDy) <= 5) {
                                    justPlaced = true;
                                    break;
                                }
                            }
                            if (justPlaced) {
                                $.writeln("[MOVE] Preserving just-placed art at [" + centerX.toFixed(2) + ", " + centerY.toFixed(2) + "] on '" + layerName + "'");
                                continue;
                            }
                            item.remove();
                            totalRemoved++;
                            $.writeln("[MOVE] Removed existing art at [" + x.toFixed(2) + ", " + y.toFixed(2) + "] from layer '" + layerName + "'");
                        }
                    }
                } catch (e) {
                    $.writeln("[MOVE] Error removing art from layer '" + layerName + "': " + e);
                }
            }
            return totalRemoved;
        }

        // Helper function to remove existing PlacedItems (art) at position from a layer
        function removeArtAtPosition(layer, x, y, tolerance) {
            tolerance = tolerance || 10;
            var removed = 0;
            try {
                for (var i = layer.placedItems.length - 1; i >= 0; i--) {
                    var item = layer.placedItems[i];
                    var bounds = item.geometricBounds;
                    var centerX = (bounds[0] + bounds[2]) / 2;
                    var centerY = (bounds[1] + bounds[3]) / 2;
                    var dx = centerX - x;
                    var dy = centerY - y;
                    var dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist <= tolerance) {
                        item.remove();
                        removed++;
                        $.writeln("[MOVE] Removed existing art at [" + x.toFixed(2) + ", " + y.toFixed(2) + "] from layer '" + layer.name + "'");
                    }
                }
            } catch (e) {
                $.writeln("[MOVE] Error removing art: " + e);
            }
            return removed;
        }

        // Helper function to get scale of a PlacedItem using matrix
        function getItemScale(item) {
            try {
                if (item.typename !== 'PlacedItem') return 100;

                // Get the transformation matrix
                var matrix = item.matrix;

                // Calculate scale from matrix
                // mValueA is horizontal scale, mValueD is vertical scale
                var scaleX = Math.sqrt(matrix.mValueA * matrix.mValueA + matrix.mValueB * matrix.mValueB);
                var scaleY = Math.sqrt(matrix.mValueC * matrix.mValueC + matrix.mValueD * matrix.mValueD);

                // Use average of X and Y scale
                var scale = (scaleX + scaleY) / 2;

                return scale * 100;
            } catch (e) {
                $.writeln("[MOVE] Error getting scale: " + e);
                return 100;
            }
        }

        // Find smallest scale on target layer (if not Ignore layer)
        var smallestScale = 100;
        if (!isIgnoreLayer && filePath) {
            $.writeln("[MOVE] Scanning target layer for smallest scale...");
            try {
                var layerItems = targetLayer.pageItems;
                for (var i = 0; i < layerItems.length; i++) {
                    if (layerItems[i].typename === 'PlacedItem') {
                        var scale = getItemScale(layerItems[i]);
                        if (scale < smallestScale) {
                            smallestScale = scale;
                        }
                    }
                }
                $.writeln("[MOVE] Smallest scale found: " + smallestScale + "%");
            } catch (e) {
                $.writeln("[MOVE] Error scanning for scale: " + e);
                smallestScale = 100;
            }
        }

        // Build map of anchor positions (PathItems with 1 point)
        var anchors = [];
        for (var i = 0; i < selection.length; i++) {
            if (selection[i].typename === 'PathItem' && selection[i].pathPoints.length === 1) {
                var anchor = selection[i].pathPoints[0].anchor;
                anchors.push({ x: anchor[0], y: anchor[1] });
            }
        }
        $.writeln("[MOVE] Found " + anchors.length + " anchors in selection");

        // Helper function to find anchors at PlacedItem center
        function findAnchorsAtCenter(centerX, centerY, tolerance) {
            var foundAnchors = [];
            var searchLog = [];
            $.writeln("[MOVE] Searching for anchors at center [" + centerX + ", " + centerY + "] with tolerance " + tolerance);
            searchLog.push("Searching for anchors at center [" + centerX.toFixed(2) + ", " + centerY.toFixed(2) + "] with tolerance " + tolerance);

            // Search all ductwork parts layers for anchors
            for (var layerIdx = 0; layerIdx < doc.layers.length; layerIdx++) {
                var layer = doc.layers[layerIdx];
                if (!isValidDuctworkLayer(layer.name)) continue;

                searchLog.push("Checking layer: " + layer.name + " (" + layer.pathItems.length + " paths)");

                try {
                    // Check all PathItems on this layer
                    for (var pathIdx = 0; pathIdx < layer.pathItems.length; pathIdx++) {
                        var path = layer.pathItems[pathIdx];

                        // Only consider 1-point paths (anchors)
                        if (path.pathPoints && path.pathPoints.length === 1) {
                            var anchorPt = path.pathPoints[0].anchor;
                            var dx = anchorPt[0] - centerX;
                            var dy = anchorPt[1] - centerY;
                            var dist = Math.sqrt(dx * dx + dy * dy);

                            searchLog.push("  Found 1-pt anchor at [" + anchorPt[0].toFixed(2) + ", " + anchorPt[1].toFixed(2) + "] - distance: " + dist.toFixed(2) + "px");

                            if (dist <= tolerance) {
                                $.writeln("[MOVE]   Found anchor at [" + anchorPt[0] + ", " + anchorPt[1] + "] on layer '" + layer.name + "' (distance: " + dist.toFixed(2) + ")");
                                searchLog.push("    MATCH! Adding to foundAnchors");
                                foundAnchors.push(path);
                            }
                        }
                    }
                } catch (eLayerSearch) {
                    $.writeln("[MOVE]   Error searching layer '" + layer.name + "': " + eLayerSearch);
                    searchLog.push("  Error: " + eLayerSearch);
                }
            }

            $.writeln("[MOVE] Found " + foundAnchors.length + " anchors at center");
            searchLog.push("Found " + foundAnchors.length + " anchors at center");

            // Write search log to file
            try {
                var debugLogPath = "C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Emory-Ductwork-Panel/anchor-search-debug.log";
                var debugFile = new File(debugLogPath);
                if (debugFile.open("w")) {
                    for (var i = 0; i < searchLog.length; i++) {
                        debugFile.writeln(searchLog[i]);
                    }
                    debugFile.close();
                }
            } catch (eDebug) {
                $.writeln("[MOVE] Error writing anchor search debug log: " + eDebug);
            }

            return foundAnchors;
        }

        // Move selection to target layer
        $.writeln("[MOVE] Processing " + selection.length + " selected items...");

        // Write detailed debug log to file
        var moveLogLines = [];
        moveLogLines.push("=== MOVE DEBUG LOG ===");
        moveLogLines.push("Timestamp: " + new Date().toString());
        moveLogLines.push("Selection length: " + selection.length);
        moveLogLines.push("Target layer: " + targetLayerName);
        moveLogLines.push("File path: " + (filePath ? filePath.fsName : "null"));
        moveLogLines.push("isIgnoreLayer: " + isIgnoreLayer);
        moveLogLines.push("");

        for (var i = 0; i < selection.length; i++) {
            var item = selection[i];

            // Validate item is still accessible (previous iterations may have deleted it)
            try {
                var testType = item.typename;
            } catch (eInvalid) {
                $.writeln("[MOVE] Item " + i + " is invalid/deleted - skipping");
                continue;
            }

            var parentType = "none";
            try { parentType = item.parent ? item.parent.typename : "none"; } catch(eP) {}
            $.writeln("[MOVE] Item " + i + " typename: " + item.typename + " parent: " + parentType);
            moveLogLines.push("Item " + i + ": typename=" + item.typename + ", layer=" + (item.layer ? item.layer.name : "null") + ", parent=" + parentType);
            // Log all point selection states
            try {
                if (item.typename === 'PathItem' && item.pathPoints) {
                    var ptStates = [];
                    for (var logPi = 0; logPi < item.pathPoints.length; logPi++) {
                        ptStates.push("[" + item.pathPoints[logPi].anchor[0].toFixed(1) + "," + item.pathPoints[logPi].anchor[1].toFixed(1) + "]=" + item.pathPoints[logPi].selected);
                    }
                    moveLogLines.push("  PointStates: " + ptStates.join(", "));
                }
            } catch(ePtLog) {}

            // Check if item is on a valid source layer
            var itemLayerName = item.layer ? item.layer.name : null;
            $.writeln("[MOVE]   Item layer: " + itemLayerName);

            var isOnDuctworkParts = isValidDuctworkLayer(itemLayerName);
            var isOnIgnoreLayer = (itemLayerName === 'Ignore' || itemLayerName === 'Ignored');
            var isReplacementTarget = isReplacementTargetItem(item, itemLayerName);

            // Accept items from ANY layer per user request
            $.writeln("[MOVE]   Item accepted from layer: " + itemLayerName);

            if (prioritizePartReplacement && !isReplacementTarget) {
                $.writeln("[MOVE]   Ignored non-part item during mixed-selection replacement");
                moveLogLines.push("  Ignored in mixed-selection replacement mode");
                continue;
            }

            try {
                moveLogLines.push("  Checking typename: " + item.typename);
                // Move PlacedItems and replace with fresh file centered on anchor
                if (item.typename === 'PlacedItem') {
                    moveLogLines.push("  ENTERED: PlacedItem block");
                    $.writeln("[MOVE]   Processing PlacedItem...");

                    // If we have a file to replace with, reload centered on anchor with smallest scale
                    if (filePath && !isIgnoreLayer) {
                        try {
                            $.writeln("[MOVE]   Replacing with fresh file...");

                            // Get item's geometric center
                            var bounds = item.geometricBounds;
                            var centerX = (bounds[0] + bounds[2]) / 2;
                            var centerY = (bounds[1] + bounds[3]) / 2;
                            $.writeln("[MOVE]   Item center: " + centerX + ", " + centerY);

                            // Find nearest anchor that's actually CLOSE to this PlacedItem
                            // (not an anchor from a completely different position in the selection)
                            var nearestAnchor = null;
                            var minDist = 999999;
                            var MAX_ANCHOR_MATCH_DIST = 20; // Only match anchors within 20pt of PlacedItem center
                            for (var a = 0; a < anchors.length; a++) {
                                var dx = anchors[a].x - centerX;
                                var dy = anchors[a].y - centerY;
                                var dist = Math.sqrt(dx * dx + dy * dy);
                                if (dist < minDist && dist <= MAX_ANCHOR_MATCH_DIST) {
                                    minDist = dist;
                                    nearestAnchor = anchors[a];
                                }
                            }

                            // Use anchor position if found, otherwise use item center
                            var targetX = nearestAnchor ? nearestAnchor.x : centerX;
                            var targetY = nearestAnchor ? nearestAnchor.y : centerY;
                            $.writeln("[MOVE]   Target center (anchor): " + targetX + ", " + targetY);

                            // Read old item's metadata BEFORE deleting it
                            var oldItemMeta = null;
                            var oldStoredRotation = 0;
                            var oldStoredScale = null;
                            try {
                                oldItemMeta = MDUX_getMetadata(item);
                                if (oldItemMeta) {
                                    // Check for rotation in multiple places
                                    if (oldItemMeta.tagRotation !== undefined) {
                                        oldStoredRotation = parseFloat(oldItemMeta.tagRotation) || 0;
                                    }
                                    if (oldItemMeta.MDUX_CumulativeRotation !== undefined) {
                                        var cumRot = parseFloat(oldItemMeta.MDUX_CumulativeRotation);
                                        if (isFinite(cumRot) && Math.abs(cumRot) > Math.abs(oldStoredRotation)) {
                                            oldStoredRotation = cumRot;
                                        }
                                    }
                                    // Check for scale
                                    if (oldItemMeta.tagScale !== undefined) {
                                        oldStoredScale = parseFloat(oldItemMeta.tagScale);
                                        if (!isFinite(oldStoredScale)) oldStoredScale = null;
                                    }
                                    if (oldStoredScale === null && oldItemMeta.MDUX_CurrentScale !== undefined) {
                                        oldStoredScale = parseFloat(oldItemMeta.MDUX_CurrentScale);
                                        if (!isFinite(oldStoredScale)) oldStoredScale = null;
                                    }
                                }
                                $.writeln("[MOVE]   Old item metadata - Rotation: " + oldStoredRotation + ", Scale: " + oldStoredScale);
                            } catch (eOldMeta) {
                                $.writeln("[MOVE]   Warning: Could not read old item metadata: " + eOldMeta);
                            }

                            // Also check MD:PLACED_ROT= in notes using magic-final functions
                            var oldPlacedRot = null;
                            try {
                                if (typeof getPlacedRotation === "function") {
                                    oldPlacedRot = getPlacedRotation(item);
                                    if (oldPlacedRot !== null && isFinite(oldPlacedRot) && Math.abs(oldPlacedRot) > 0.1) {
                                        oldStoredRotation = oldPlacedRot;
                                        $.writeln("[MOVE]   Found MD:PLACED_ROT=" + oldPlacedRot);
                                    }
                                }
                            } catch (ePlacedRot) {}

                            // Delete the old item
                            item.remove();
                            $.writeln("[MOVE]   Old item removed");

                            // Place fresh item from file
                            var newItem = targetLayer.placedItems.add();
                            newItem.file = filePath;
                            MDUX_setPlacedItemName(newItem, filePath);

                            // Get new item bounds to calculate center offset
                            var newBounds = newItem.geometricBounds;
                            var newWidth = Math.abs(newBounds[2] - newBounds[0]);
                            var newHeight = Math.abs(newBounds[1] - newBounds[3]);

                            // Scale FIRST if needed
                            if (smallestScale !== 100) {
                                newItem.resize(smallestScale, smallestScale, true, false, false, false, 100, Transformation.CENTER);
                                $.writeln("[MOVE]   Scaled to " + smallestScale + "%");

                                // Recalculate bounds after scaling
                                newBounds = newItem.geometricBounds;
                                newWidth = Math.abs(newBounds[2] - newBounds[0]);
                                newHeight = Math.abs(newBounds[1] - newBounds[3]);
                            }

                            // Position so center aligns with target anchor
                            newItem.position = [targetX - newWidth / 2, targetY + newHeight / 2];
                            $.writeln("[MOVE]   Item centered on anchor at " + targetX + ", " + targetY);

                            // Apply stored rotation from old item (if any)
                            if (oldStoredRotation !== 0 && Math.abs(oldStoredRotation) > 0.1) {
                                try {
                                    newItem.rotate(oldStoredRotation, true, true, true, true, Transformation.CENTER);
                                    $.writeln("[MOVE]   Applied stored rotation: " + oldStoredRotation + " deg");

                                    // Re-center after rotation
                                    var rotBounds = newItem.geometricBounds;
                                    var rotCx = (rotBounds[0] + rotBounds[2]) / 2;
                                    var rotCy = (rotBounds[1] + rotBounds[3]) / 2;
                                    var rotDx = targetX - rotCx;
                                    var rotDy = targetY - rotCy;
                                    if (Math.abs(rotDx) > 0.01 || Math.abs(rotDy) > 0.01) {
                                        newItem.translate(rotDx, rotDy);
                                    }
                                } catch (eApplyRot) {
                                    $.writeln("[MOVE]   Warning: Failed to apply rotation: " + eApplyRot);
                                }
                            }

                            // Record this position to prevent duplicate art placement
                            artPlacedPositions.push({ x: targetX, y: targetY });

                            // Write complete metadata for the placed item
                            try {
                                // Get actual dimensions after placement
                                var actualWidth = newItem.width;
                                var actualHeight = newItem.height;
                                var actualStrokeWidth = 1;
                                try { actualStrokeWidth = newItem.strokeWidth || 1; } catch (e) { }

                                // Check if there's a rotation override from the nearest anchor
                                var rotationOverride = null;
                                if (nearestAnchor && nearestAnchor.anchor) {
                                    try {
                                        var anchorMeta = MDUX_getMetadata(nearestAnchor.anchor);
                                        if (anchorMeta && anchorMeta.MDUX_PointRotation !== undefined && anchorMeta.MDUX_PointRotation !== null) {
                                            rotationOverride = parseFloat(anchorMeta.MDUX_PointRotation);
                                            if (!isFinite(rotationOverride)) rotationOverride = null;
                                        }
                                    } catch (eAnchorMeta) {
                                        rotationOverride = null;
                                    }
                                }

                                // Use old stored values if available, otherwise defaults
                                var finalRotation = oldStoredRotation || 0;
                                var finalScale = oldStoredScale || smallestScale;

                                var metadata = {
                                    MDUX_OriginalWidth: actualWidth,
                                    MDUX_OriginalHeight: actualHeight,
                                    MDUX_OriginalStrokeWidth: actualStrokeWidth,
                                    MDUX_OriginalRotation: String(finalRotation),
                                    MDUX_CumulativeRotation: String(finalRotation),
                                    MDUX_CurrentScale: String(finalScale),
                                    tagScale: finalScale,
                                    tagRotation: finalRotation
                                };

                                // Store rotation override if available
                                if (rotationOverride !== null) {
                                    metadata.MDUX_RotationOverride = rotationOverride;
                                }

                                MDUX_setMetadata(newItem, metadata);
                                $.writeln("[MOVE]   Wrote complete metadata: scale=" + finalScale + ", rotation=" + finalRotation + (rotationOverride !== null ? ", rotOverride=" + rotationOverride : "") + ", width=" + actualWidth);
                            } catch (eMetadata) {
                                $.writeln("[MOVE]   Warning: Failed to write metadata: " + eMetadata);
                            }

                            itemsMoved++;

                            // Ensure only one anchor exists at this location
                            // Remove anchors at BOTH the target position AND the original PlacedItem center
                            // (the anchor might be at the original center, not the target position)
                            removeExistingAnchorAtPosition(targetX, targetY, 10, targetLayerName);
                            removeExistingAnchorAtPosition(centerX, centerY, 10, targetLayerName);

                            // Check if anchor already exists on target layer
                            if (!anchorExistsOnLayer(targetLayer, targetX, targetY, 5)) {
                                // Create a new anchor on the target layer
                                var newAnchor = targetLayer.pathItems.add();
                                newAnchor.setEntirePath([[targetX, targetY]]);
                                newAnchor.filled = false;
                                newAnchor.stroked = false;
                                anchorsMoved++;
                                $.writeln("[MOVE]   Created anchor at [" + targetX.toFixed(2) + ", " + targetY.toFixed(2) + "]");
                            } else {
                                $.writeln("[MOVE]   Anchor already exists on target layer - no new anchor created");
                            }
                        } catch (eReplace) {
                            $.writeln("[MOVE]   ERROR replacing item: " + eReplace);
                            // If replacement failed and item still exists, just move it
                            try {
                                if (item && !item.removed) {
                                    item.move(targetLayer, ElementPlacement.PLACEATBEGINNING);
                                    itemsMoved++;
                                }
                            } catch (e) { }
                        }
                    } else {
                        // No file to replace with, just move the item
                        $.writeln("[MOVE]   Moving PlacedItem without replacement...");

                        // Get item center BEFORE moving it
                        var bounds = item.geometricBounds;
                        var centerX = (bounds[0] + bounds[2]) / 2;
                        var centerY = (bounds[1] + bounds[3]) / 2;

                        item.move(targetLayer, ElementPlacement.PLACEATBEGINNING);
                        itemsMoved++;
                        $.writeln("[MOVE]   PlacedItem moved successfully");

                        // Ensure only one anchor exists at this location
                        // First remove any existing anchors from OTHER ductwork parts layers
                        removeExistingAnchorAtPosition(centerX, centerY, 5, targetLayerName);

                        // Check if anchor already exists on target layer
                        if (!anchorExistsOnLayer(targetLayer, centerX, centerY, 5)) {
                            // Create a new anchor on the target layer
                            var newAnchor = targetLayer.pathItems.add();
                            newAnchor.setEntirePath([[centerX, centerY]]);
                            newAnchor.filled = false;
                            newAnchor.stroked = false;
                            anchorsMoved++;
                            $.writeln("[MOVE]   Created anchor at [" + centerX.toFixed(2) + ", " + centerY.toFixed(2) + "]");
                        } else {
                            $.writeln("[MOVE]   Anchor already exists on target layer - no new anchor created");
                        }
                    }
                }
                // Move PathItems (anchors or multi-point paths)
                else if (item.typename === 'PathItem') {
                    moveLogLines.push("  ENTERED: PathItem block");
                    var numPoints = item.pathPoints.length;
                    var isFromDuctworkColorLayer = isDuctworkColorLayer(itemLayerName);
                    $.writeln("[MOVE]   Processing PathItem with " + numPoints + " points from layer '" + itemLayerName + "' (ductwork line: " + isFromDuctworkColorLayer + ")");
                    moveLogLines.push("  PathItem: numPoints=" + numPoints + ", isFromDuctworkColorLayer=" + isFromDuctworkColorLayer);

                    var anchorPositions = [];

                    if (numPoints === 1) {
                        // Single-point path (anchor) - always use it
                        var pos = item.pathPoints[0].anchor;
                        anchorPositions.push({ x: pos[0], y: pos[1] });
                        $.writeln("[MOVE]   Single-point path (anchor)");
                    } else {
                        // Multi-point path - use C++ SDK anchor positions if available,
                        // fall back to ExtendScript PathPointSelection check
                        var hasSelectedPoints = false;

                        // First try C++ detected positions (accurate for Direct Selection)
                        if (cppSelectedAnchors.length > 0) {
                            var CPP_MATCH_TOL = 2;
                            for (var pi = 0; pi < numPoints; pi++) {
                                var ptAnchor = item.pathPoints[pi].anchor;
                                for (var cpi = 0; cpi < cppSelectedAnchors.length; cpi++) {
                                    var cdx = ptAnchor[0] - cppSelectedAnchors[cpi].x;
                                    var cdy = ptAnchor[1] - cppSelectedAnchors[cpi].y;
                                    if (Math.sqrt(cdx * cdx + cdy * cdy) <= CPP_MATCH_TOL) {
                                        anchorPositions.push({ x: ptAnchor[0], y: ptAnchor[1] });
                                        hasSelectedPoints = true;
                                        $.writeln("[MOVE]   C++ matched anchor at [" + ptAnchor[0].toFixed(2) + ", " + ptAnchor[1].toFixed(2) + "]");
                                        break;
                                    }
                                }
                            }
                        }

                        // Fallback to ExtendScript selection check
                        if (!hasSelectedPoints) {
                            for (var pi2 = 0; pi2 < numPoints; pi2++) {
                                var pt = item.pathPoints[pi2];
                                if (pt.selected === PathPointSelection.ANCHORPOINT || pt.selected === PathPointSelection.LEFTRIGHTPOINT) {
                                    var pos = pt.anchor;
                                    anchorPositions.push({ x: pos[0], y: pos[1] });
                                    hasSelectedPoints = true;
                                }
                            }
                        }

                        if (!hasSelectedPoints) {
                            $.writeln("[MOVE]   SKIPPED: no selected points detected (C++ or ExtendScript)");
                            itemsSkipped++;
                            continue;
                        }
                        // For ductwork color layer multi-point paths:
                        // - ALL points selected = object selection (Selection tool) â†’ SKIP
                        //   The PlacedItem and 1-point anchor in the selection handle conversion.
                        // - SOME points selected = direct selection â†’ use those specific points
                        // - NO points selected = check C++ for accurate detection
                        if (isFromDuctworkColorLayer && numPoints > 1 && anchorPositions.length === numPoints) {
                            $.writeln("[MOVE]   SKIPPED: object-selected ductwork line (all " + numPoints + " points selected)");
                            itemsSkipped++;
                            continue;
                        }

                        // Filter connected endpoints for partially-selected ductwork lines
                        if (isFromDuctworkColorLayer && anchorPositions.length === numPoints && numPoints > 1) {
                            $.writeln("[MOVE]   All " + numPoints + " points selected on ductwork line - filtering connected endpoints");
                            var filteredPositions = [];
                            var JUNCTION_DIST = 5;
                            for (var fpIdx = 0; fpIdx < anchorPositions.length; fpIdx++) {
                                var fpX = anchorPositions[fpIdx].x;
                                var fpY = anchorPositions[fpIdx].y;
                                var isAtJunction = false;
                                // Check against all ductwork paths in the document
                                for (var lIdx = 0; lIdx < doc.layers.length && !isAtJunction; lIdx++) {
                                    var chkLayerName = doc.layers[lIdx].name;
                                    if (!isDuctworkColorLayer(chkLayerName)) continue;
                                    try {
                                        for (var pIdx = 0; pIdx < doc.layers[lIdx].pathItems.length && !isAtJunction; pIdx++) {
                                            var otherPath = doc.layers[lIdx].pathItems[pIdx];
                                            if (otherPath === item) continue; // skip self
                                            if (!otherPath.pathPoints || otherPath.pathPoints.length < 2) continue;
                                            // Check endpoints of other path
                                            for (var opIdx = 0; opIdx < otherPath.pathPoints.length; opIdx++) {
                                                var op = otherPath.pathPoints[opIdx].anchor;
                                                var jdx = fpX - op[0];
                                                var jdy = fpY - op[1];
                                                if (Math.sqrt(jdx * jdx + jdy * jdy) <= JUNCTION_DIST) {
                                                    isAtJunction = true;
                                                    break;
                                                }
                                            }
                                        }
                                    } catch (eLookup) {}
                                }
                                if (!isAtJunction) {
                                    filteredPositions.push(anchorPositions[fpIdx]);
                                    $.writeln("[MOVE]   Kept free endpoint at [" + fpX.toFixed(2) + ", " + fpY.toFixed(2) + "]");
                                } else {
                                    $.writeln("[MOVE]   Filtered junction endpoint at [" + fpX.toFixed(2) + ", " + fpY.toFixed(2) + "]");
                                }
                            }
                            anchorPositions = filteredPositions.length > 0 ? filteredPositions : anchorPositions;
                        }
                        $.writeln("[MOVE]   Using " + anchorPositions.length + " anchor position(s)");
                    }

                    // Handle paths with art placement (not Ignore layer, and we have a file)
                    if (filePath && !isIgnoreLayer && anchorPositions.length > 0) {

                        $.writeln("[MOVE]   Processing " + anchorPositions.length + " anchor(s) with art placement");

                        // For each anchor position, create an anchor point and place art (if not already placed)
                        for (var ai = 0; ai < anchorPositions.length; ai++) {
                            var anchorX = anchorPositions[ai].x;
                            var anchorY = anchorPositions[ai].y;
                            $.writeln("[MOVE]   Processing anchor " + (ai + 1) + "/" + anchorPositions.length + " at " + anchorX.toFixed(2) + ", " + anchorY.toFixed(2));

                            // Skip if a ductwork part or ignore anchor already exists at this location
                            // (on ANY part layer or ignore layer, not just the target)
                            var alreadyHasPart = false;
                            try {
                                var PART_CHECK_TOL = 10;
                                for (var chkIdx = 0; chkIdx < doc.layers.length && !alreadyHasPart; chkIdx++) {
                                    var chkLayer = doc.layers[chkIdx];
                                    var chkName = chkLayer.name;
                                    if (!isValidDuctworkLayer(chkName) && chkName !== 'Ignore' && chkName !== 'Ignored') continue;
                                    // Check placed items (art)
                                    for (var chkPi = 0; chkPi < chkLayer.placedItems.length && !alreadyHasPart; chkPi++) {
                                        try {
                                            var chkItem = chkLayer.placedItems[chkPi];
                                            var chkBounds = chkItem.geometricBounds;
                                            var chkCx = (chkBounds[0] + chkBounds[2]) / 2;
                                            var chkCy = (chkBounds[1] + chkBounds[3]) / 2;
                                            var chkDx = chkCx - anchorX;
                                            var chkDy = chkCy - anchorY;
                                            if (Math.sqrt(chkDx * chkDx + chkDy * chkDy) <= PART_CHECK_TOL) {
                                                alreadyHasPart = true;
                                            }
                                        } catch (eChk) {}
                                    }
                                    // Check single-point anchors
                                    for (var chkAi = 0; chkAi < chkLayer.pathItems.length && !alreadyHasPart; chkAi++) {
                                        try {
                                            var chkPath = chkLayer.pathItems[chkAi];
                                            if (chkPath.pathPoints && chkPath.pathPoints.length === 1) {
                                                var chkPt = chkPath.pathPoints[0].anchor;
                                                var chkAdx = chkPt[0] - anchorX;
                                                var chkAdy = chkPt[1] - anchorY;
                                                if (Math.sqrt(chkAdx * chkAdx + chkAdy * chkAdy) <= PART_CHECK_TOL) {
                                                    alreadyHasPart = true;
                                                }
                                            }
                                        } catch (eChkA) {}
                                    }
                                }
                            } catch (ePartCheck) {}
                            if (alreadyHasPart) {
                                $.writeln("[MOVE]   SKIPPED anchor at [" + anchorX.toFixed(2) + ", " + anchorY.toFixed(2) + "] - ductwork part/ignore already exists");
                                continue;
                            }

                            // Find the nearest existing PlacedItem near this anchor to preserve its rotation/scale
                            var nearestArtScale = smallestScale;
                            var nearestArtRotation = 0;
                            try {
                                var ART_SEARCH_TOLERANCE = 20; // wider search for art near anchor
                                for (var layerIdx = 0; layerIdx < doc.layers.length; layerIdx++) {
                                    var searchLayer = doc.layers[layerIdx];
                                    if (!isValidDuctworkLayer(searchLayer.name)) continue;
                                    for (var piIdx = 0; piIdx < searchLayer.placedItems.length; piIdx++) {
                                        try {
                                            var pItem = searchLayer.placedItems[piIdx];
                                            var pBounds = pItem.geometricBounds;
                                            var pcx = (pBounds[0] + pBounds[2]) / 2;
                                            var pcy = (pBounds[1] + pBounds[3]) / 2;
                                            var pdx = pcx - anchorX;
                                            var pdy = pcy - anchorY;
                                            if (Math.sqrt(pdx * pdx + pdy * pdy) <= ART_SEARCH_TOLERANCE) {
                                                // Preserve scale from existing art
                                                var existingScale = getItemScale(pItem);
                                                if (existingScale < nearestArtScale) {
                                                    nearestArtScale = existingScale;
                                                }
                                                // Try to get rotation from metadata
                                                try {
                                                    var existingMeta = MDUX_getMetadata(pItem);
                                                    if (existingMeta && existingMeta.MDUX_CumulativeRotation !== undefined) {
                                                        nearestArtRotation = parseFloat(existingMeta.MDUX_CumulativeRotation) || 0;
                                                    } else if (existingMeta && existingMeta.tagRotation !== undefined) {
                                                        nearestArtRotation = parseFloat(existingMeta.tagRotation) || 0;
                                                    }
                                                } catch (eRotMeta) {}
                                                // Also check MD:PLACED_ROT
                                                try {
                                                    if (typeof getPlacedRotation === "function") {
                                                        var pRot = getPlacedRotation(pItem);
                                                        if (pRot !== null && isFinite(pRot) && Math.abs(pRot) > 0.1) {
                                                            nearestArtRotation = pRot;
                                                        }
                                                    }
                                                } catch (ePRot) {}
                                                $.writeln("[MOVE]   Found nearby art on '" + searchLayer.name + "' - scale=" + existingScale.toFixed(1) + "%, rotation=" + nearestArtRotation);
                                                break;
                                            }
                                        } catch (ePi) {}
                                    }
                                }
                            } catch (eArtSearch) {
                                $.writeln("[MOVE]   Error searching for nearby art: " + eArtSearch);
                            }

                            // CRITICAL: Remove existing ductwork parts from ALL layers at this position
                            // Use wider tolerance to catch slightly offset art
                            removeArtFromAllDuctworkLayers(anchorX, anchorY, 20);
                            removeExistingAnchorAtPosition(anchorX, anchorY, 5, targetLayerName);

                            // Check if anchor already exists on target layer
                            if (anchorExistsOnLayer(targetLayer, anchorX, anchorY, 5)) {
                                $.writeln("[MOVE]   Anchor already exists on target layer - using existing anchor");
                            } else {
                                // Create a new single-point anchor on the target layer
                                var newAnchor = targetLayer.pathItems.add();
                                newAnchor.setEntirePath([[anchorX, anchorY]]);
                                newAnchor.filled = false;
                                newAnchor.stroked = false;
                                anchorsMoved++;
                                $.writeln("[MOVE]   Created anchor at [" + anchorX.toFixed(2) + ", " + anchorY.toFixed(2) + "]");
                            }

                            // Check if art was already placed at this position DURING THIS OPERATION (avoid duplicates in same batch)
                            if (wasArtPlacedNear(anchorX, anchorY)) {
                                $.writeln("[MOVE]   SKIPPED art placement - art already placed in this batch");
                                continue;
                            }

                            // Place art at this anchor
                            try {
                                var newItem = targetLayer.placedItems.add();
                                newItem.file = filePath;
                                MDUX_setPlacedItemName(newItem, filePath);

                                // Get new item bounds
                                var newBounds = newItem.geometricBounds;
                                var newWidth = Math.abs(newBounds[2] - newBounds[0]);
                                var newHeight = Math.abs(newBounds[1] - newBounds[3]);

                                // Scale using preserved scale from old art, or smallest on layer
                                var useScale = nearestArtScale;
                                if (useScale !== 100) {
                                    newItem.resize(useScale, useScale, true, false, false, false, 100, Transformation.CENTER);
                                    newBounds = newItem.geometricBounds;
                                    newWidth = Math.abs(newBounds[2] - newBounds[0]);
                                    newHeight = Math.abs(newBounds[1] - newBounds[3]);
                                }

                                // Position centered on anchor
                                newItem.position = [anchorX - newWidth / 2, anchorY + newHeight / 2];

                                // Apply rotation from old art if any
                                if (nearestArtRotation !== 0 && Math.abs(nearestArtRotation) > 0.1) {
                                    try {
                                        newItem.rotate(nearestArtRotation, true, true, true, true, Transformation.CENTER);
                                        // Re-center after rotation
                                        var rotBounds = newItem.geometricBounds;
                                        var rotCx = (rotBounds[0] + rotBounds[2]) / 2;
                                        var rotCy = (rotBounds[1] + rotBounds[3]) / 2;
                                        var rotDx = anchorX - rotCx;
                                        var rotDy = anchorY - rotCy;
                                        if (Math.abs(rotDx) > 0.01 || Math.abs(rotDy) > 0.01) {
                                            newItem.translate(rotDx, rotDy);
                                        }
                                        $.writeln("[MOVE]   Applied rotation: " + nearestArtRotation + " deg");
                                    } catch (eApplyRot) {
                                        $.writeln("[MOVE]   Warning: Failed to apply rotation: " + eApplyRot);
                                    }
                                }

                                // Record this position as having art placed
                                artPlacedPositions.push({ x: anchorX, y: anchorY });

                                // Write metadata
                                try {
                                    var metadata = {
                                        MDUX_OriginalWidth: newItem.width,
                                        MDUX_OriginalHeight: newItem.height,
                                        MDUX_OriginalStrokeWidth: 1,
                                        MDUX_OriginalRotation: String(nearestArtRotation),
                                        MDUX_CumulativeRotation: String(nearestArtRotation),
                                        MDUX_CurrentScale: String(useScale),
                                        tagScale: useScale,
                                        tagRotation: nearestArtRotation
                                    };
                                    MDUX_setMetadata(newItem, metadata);
                                } catch (eMetadata) {}

                                itemsMoved++;
                                $.writeln("[MOVE]   Art placed at anchor " + (ai + 1));
                            } catch (eArtPlace) {
                                $.writeln("[MOVE]   ERROR placing art at anchor: " + eArtPlace);
                                moveLogLines.push("  ERROR placing art: " + eArtPlace.toString());
                            }
                        }

                    } else if (anchorPositions.length > 0) {
                        // Create anchors without art (Ignore layer or no file)
                        $.writeln("[MOVE]   Creating " + anchorPositions.length + " anchors without art");

                        // Keep the original path - just extract selected anchor positions
                        $.writeln("[MOVE]   Keeping original path intact");

                        // Create individual anchors
                        for (var ai = 0; ai < anchorPositions.length; ai++) {
                            var anchorX = anchorPositions[ai].x;
                            var anchorY = anchorPositions[ai].y;

                            // Remove existing art AND anchors from ALL ductwork parts layers at this position
                            removeArtFromAllDuctworkLayers(anchorX, anchorY, 20);
                            removeExistingAnchorAtPosition(anchorX, anchorY, 5, targetLayerName);

                            // Check if anchor already exists on target layer - skip if so
                            if (anchorExistsOnLayer(targetLayer, anchorX, anchorY, 5)) {
                                $.writeln("[MOVE]   Anchor already exists on target layer at [" + anchorX.toFixed(2) + ", " + anchorY.toFixed(2) + "] - skipping");
                                continue;
                            }

                            var newAnchor = targetLayer.pathItems.add();
                            newAnchor.setEntirePath([[anchorX, anchorY]]);
                            newAnchor.filled = false;
                            newAnchor.stroked = false;
                            anchorsMoved++;
                            $.writeln("[MOVE]   Created anchor " + (ai + 1) + " at [" + anchorX.toFixed(2) + ", " + anchorY.toFixed(2) + "]");
                        }
                    } else {
                        // No selected points and no anchors extracted - shouldn't happen but handle it
                        $.writeln("[MOVE]   No anchors to extract - skipping");
                        itemsSkipped++;
                    }
                }
                // Handle CompoundPathItems (ductwork lines might be compound paths)
                else if (item.typename === 'CompoundPathItem') {
                    moveLogLines.push("  ENTERED: CompoundPathItem block");
                    $.writeln("[MOVE]   Processing CompoundPathItem from layer '" + itemLayerName + "'");
                    $.writeln("[MOVE]   isDuctworkColorLayer result: " + isDuctworkColorLayer(itemLayerName));
                    moveLogLines.push("  CompoundPathItem: isDuctworkColorLayer=" + isDuctworkColorLayer(itemLayerName));
                    $.writeln("[MOVE]   filePath: " + (filePath ? filePath.fsName : 'null'));
                    $.writeln("[MOVE]   isIgnoreLayer: " + isIgnoreLayer);

                    // Extract endpoints from compound path
                    var compoundEndpoints = [];
                    try {
                        // Try to get paths from the compound
                        var pathCount = item.pathItems ? item.pathItems.length : 0;
                        $.writeln("[MOVE]   CompoundPath has " + pathCount + " pathItems");

                        if (pathCount > 0) {
                            var firstPath = item.pathItems[0];
                            var pointCount = firstPath.pathPoints ? firstPath.pathPoints.length : 0;
                            $.writeln("[MOVE]   First path has " + pointCount + " points");

                            if (pointCount > 0) {
                                var firstPt = firstPath.pathPoints[0].anchor;
                                var lastPt = firstPath.pathPoints[pointCount - 1].anchor;
                                $.writeln("[MOVE]   First endpoint: [" + firstPt[0].toFixed(2) + ", " + firstPt[1].toFixed(2) + "]");
                                $.writeln("[MOVE]   Last endpoint: [" + lastPt[0].toFixed(2) + ", " + lastPt[1].toFixed(2) + "]");

                                compoundEndpoints.push({ x: firstPt[0], y: firstPt[1] });
                                var dx = lastPt[0] - firstPt[0];
                                var dy = lastPt[1] - firstPt[1];
                                if (Math.sqrt(dx * dx + dy * dy) > 5) {
                                    compoundEndpoints.push({ x: lastPt[0], y: lastPt[1] });
                                }
                            }
                        }
                    } catch (eCompExtract) {
                        $.writeln("[MOVE]   Error extracting from CompoundPath: " + eCompExtract);
                    }

                    $.writeln("[MOVE]   Extracted " + compoundEndpoints.length + " endpoints from CompoundPath");

                    // Create ductwork parts at extracted endpoints (if we have a file)
                    if (filePath && !isIgnoreLayer && compoundEndpoints.length > 0) {
                        $.writeln("[MOVE]   Creating ductwork parts at " + compoundEndpoints.length + " endpoint(s)");

                        for (var cei = 0; cei < compoundEndpoints.length; cei++) {
                            var cex = compoundEndpoints[cei].x;
                            var cey = compoundEndpoints[cei].y;
                            $.writeln("[MOVE]   Processing endpoint " + (cei + 1) + " at [" + cex.toFixed(2) + ", " + cey.toFixed(2) + "]");

                            // CRITICAL: Remove existing ductwork parts from ALL layers at this position
                            // Only ONE ductwork part can exist at any location across all ductwork parts layers
                            removeArtFromAllDuctworkLayers(cex, cey, 10);
                            removeExistingAnchorAtPosition(cex, cey, 5, targetLayerName);

                            // Create anchor if needed
                            if (!anchorExistsOnLayer(targetLayer, cex, cey, 5)) {
                                $.writeln("[MOVE]   Creating anchor at endpoint");
                                var newAnchor = targetLayer.pathItems.add();
                                newAnchor.setEntirePath([[cex, cey]]);
                                newAnchor.filled = false;
                                newAnchor.stroked = false;
                                anchorsMoved++;
                            }

                            // Place art if not already placed in this batch
                            if (!wasArtPlacedNear(cex, cey)) {
                                $.writeln("[MOVE]   Placing art at endpoint");
                                try {
                                    var newItem = targetLayer.placedItems.add();
                                    newItem.file = filePath;
                                    MDUX_setPlacedItemName(newItem, filePath);
                                    var newBounds = newItem.geometricBounds;
                                    var newWidth = Math.abs(newBounds[2] - newBounds[0]);
                                    var newHeight = Math.abs(newBounds[1] - newBounds[3]);
                                    if (smallestScale !== 100) {
                                        newItem.resize(smallestScale, smallestScale, true, false, false, false, 100, Transformation.CENTER);
                                        newBounds = newItem.geometricBounds;
                                        newWidth = Math.abs(newBounds[2] - newBounds[0]);
                                        newHeight = Math.abs(newBounds[1] - newBounds[3]);
                                    }
                                    newItem.position = [cex - newWidth / 2, cey + newHeight / 2];
                                    artPlacedPositions.push({ x: cex, y: cey });
                                    itemsMoved++;
                                    $.writeln("[MOVE]   Art placed successfully");
                                } catch (ePlaceArt) {
                                    $.writeln("[MOVE]   Error placing art: " + ePlaceArt);
                                }
                            } else {
                                $.writeln("[MOVE]   Art already placed nearby in this batch");
                            }
                        }
                    } else {
                        $.writeln("[MOVE]   Cannot create parts: filePath=" + !!filePath + ", isIgnoreLayer=" + isIgnoreLayer + ", endpoints=" + compoundEndpoints.length);
                        if (compoundEndpoints.length === 0) {
                            itemsSkipped++;
                        }
                    }
                    // Never move the compound path itself
                    continue;
                }
                // Move GroupItems
                else if (item.typename === 'GroupItem') {
                    moveLogLines.push("  ENTERED: GroupItem block");
                    $.writeln("[MOVE]   Moving GroupItem...");
                    item.move(targetLayer, ElementPlacement.PLACEATBEGINNING);
                    itemsMoved++;
                    $.writeln("[MOVE]   GroupItem moved");
                }
                // Move other items - but NEVER ductwork color layer items
                else {
                    moveLogLines.push("  ENTERED: else (fallback) block for typename=" + item.typename);
                    // CRITICAL SAFEGUARD: Never move items from ductwork color layers
                    if (isDuctworkColorLayer(itemLayerName)) {
                        $.writeln("[MOVE]   BLOCKED: Refusing to move " + item.typename + " from ductwork color layer '" + itemLayerName + "'");
                        itemsSkipped++;
                        continue;
                    }
                    $.writeln("[MOVE]   Moving " + item.typename + "...");
                    item.move(targetLayer, ElementPlacement.PLACEATBEGINNING);
                    itemsMoved++;
                    $.writeln("[MOVE]   Item moved");
                }
            } catch (eMove) {
                $.writeln("[MOVE]   ERROR moving item: " + eMove);
                moveLogLines.push("  ERROR: " + eMove.toString());
            }
        }

        $.writeln("[MOVE] Moved " + itemsMoved + " items, " + anchorsMoved + " anchors, skipped " + itemsSkipped + " items");

        // Write debug log to file
        try {
            var debugLogPath = "C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Emory-Ductwork-Panel/move-debug.log";
            var debugFile = new File(debugLogPath);
            if (debugFile.open("w")) {
                // Write detailed log lines
                for (var logIdx = 0; logIdx < moveLogLines.length; logIdx++) {
                    debugFile.writeln(moveLogLines[logIdx]);
                }
                debugFile.writeln("");
                debugFile.writeln("=== SUMMARY ===");
                debugFile.writeln("Items moved: " + itemsMoved);
                debugFile.writeln("Anchors moved: " + anchorsMoved);
                debugFile.writeln("Items skipped: " + itemsSkipped);
                debugFile.writeln("Target layer: " + targetLayerName);
                debugFile.close();
            }
        } catch (eDebug) {
            $.writeln("[MOVE] Error writing debug log: " + eDebug);
        }

        // Re-select items on the target layer that match positions where we moved/created items
        try {
            if (!isIgnoreLayer && artPlacedPositions.length > 0) {
                doc.selection = null;
                var reselCount = 0;
                var RESEL_TOL = 15;
                for (var rpi = 0; rpi < targetLayer.pageItems.length; rpi++) {
                    try {
                        var rItem = targetLayer.pageItems[rpi];
                        if (!rItem || rItem.locked || rItem.hidden) continue;
                        // Get item center
                        var rBounds = rItem.geometricBounds;
                        var rcx = (rBounds[0] + rBounds[2]) / 2;
                        var rcy = (rBounds[1] + rBounds[3]) / 2;
                        // Check if near any moved position
                        for (var mp = 0; mp < artPlacedPositions.length; mp++) {
                            var mdx = rcx - artPlacedPositions[mp].x;
                            var mdy = rcy - artPlacedPositions[mp].y;
                            if (Math.sqrt(mdx * mdx + mdy * mdy) <= RESEL_TOL) {
                                rItem.selected = true;
                                reselCount++;
                                break;
                            }
                        }
                    } catch (eReSel) {}
                }
                // Also select 1-point anchors near moved positions
                for (var api = 0; api < targetLayer.pathItems.length; api++) {
                    try {
                        var aItem = targetLayer.pathItems[api];
                        if (!aItem || aItem.locked || aItem.pathPoints.length !== 1) continue;
                        var apt = aItem.pathPoints[0].anchor;
                        for (var mp2 = 0; mp2 < artPlacedPositions.length; mp2++) {
                            var adx = apt[0] - artPlacedPositions[mp2].x;
                            var ady = apt[1] - artPlacedPositions[mp2].y;
                            if (Math.sqrt(adx * adx + ady * ady) <= RESEL_TOL) {
                                aItem.selected = true;
                                reselCount++;
                                break;
                            }
                        }
                    } catch (eReSel2) {}
                }
                $.writeln("[MOVE] Re-selected " + reselCount + " items near " + artPlacedPositions.length + " moved positions");
            } else if (isIgnoreLayer) {
                doc.selection = null;
            }
        } catch (eReselect) {
            $.writeln("[MOVE] Error re-selecting: " + eReselect);
        }

        // NOW restore layer state (lock/hide)
        if (isIgnoreLayer) {
            targetLayer.locked = true;
            targetLayer.visible = false;
            $.writeln("[MOVE] Locked and hid Ignore layer");
        } else {
            targetLayer.locked = wasLocked;
            targetLayer.visible = wasVisible;
            $.writeln("[MOVE] Restored layer state");
        }

        var result = JSON.stringify({
            itemsMoved: itemsMoved,
            anchorsMoved: anchorsMoved,
            itemsSkipped: itemsSkipped
        });
        $.writeln("[MOVE] Returning result: " + result);
        return result;
    } catch (e) {
        $.writeln("[MOVE] EXCEPTION: " + e.toString());
        $.writeln("[MOVE] Stack: " + e.line);
        return "ERROR:" + e.toString();
    }
}

function MDUX_scaleStrokeRecursively(item, scalePercent, stats) {
    try {
        if (item.locked || item.hidden) {
            stats.locked++;
            return;
        }

        if (item.typename === "GroupItem") {
            stats.groups++;
            var children = item.pageItems;
            for (var i = 0; i < children.length; i++) {
                MDUX_scaleStrokeRecursively(children[i], scalePercent, stats);
            }
        } else if (item.typename === "CompoundPathItem") {
            stats.compoundPaths++;

            // Try to scale the compound path itself first (if it has stroke properties)
            var compoundStroked = false;
            try {
                // Some AI versions expose stroke on the CompoundPathItem
                if (item.stroked) {
                    item.strokeWidth = item.strokeWidth * (scalePercent / 100.0);
                    stats.stroked++;
                    compoundStroked = true;
                }
            } catch (e) { }

            // Also recurse into children (pathItems)
            // If the compound path itself was stroked, the children might inherit or duplicate.
            // But usually, if the compound path is stroked, the children's stroke properties are ignored or synced.
            // If we didn't successfully scale the compound path, we MUST scale the children.
            // If we DID scale the compound path, scaling children might be redundant but harmless (unless they have independent strokes).

            var paths = item.pathItems;
            for (var j = 0; j < paths.length; j++) {
                MDUX_scaleStrokeRecursively(paths[j], scalePercent, stats);
            }

        } else if (item.typename === "PathItem") {
            stats.paths++;
            var strokesScaled = 0;

            // Scale all strokes from Appearance panel (multiple strokes)
            try {
                if (item.strokes && item.strokes.length > 0) {
                    for (var s = 0; s < item.strokes.length; s++) {
                        var stroke = item.strokes[s];
                        if (stroke.visible) {
                            stroke.weight = stroke.weight * (scalePercent / 100.0);
                            strokesScaled++;
                        }
                    }
                }
            } catch (e) {
                // Fallback: strokes collection not available, use basic strokeWidth
            }

            // Also scale basic strokeWidth if stroked (covers single-stroke case)
            if (item.stroked && strokesScaled === 0) {
                item.strokeWidth = item.strokeWidth * (scalePercent / 100.0);
                strokesScaled++;
            }

            if (strokesScaled > 0) {
                stats.stroked++;
            } else if (item.filled) {
                stats.filled++;
            } else {
                stats.unstroked++;
            }
        } else {
            stats.others++;
            stats.otherTypes.push(item.typename);
        }
    } catch (e) {
        stats.errors++;
    }
}

function MDUX_transformEach(scale, rotation, undoPrevious) {
    MDUX_debugLog("[TRANSFORM-EACH] === Called with scale=" + scale + ", rotation=" + rotation + ", undoPrevious=" + undoPrevious + " ===");
    try {
        if (app.documents.length === 0) return JSON.stringify({ ok: false, message: "No document open." });

        var sel = app.selection;
        if (!sel || sel.length === 0) return JSON.stringify({ ok: false, message: "Nothing selected." });

        var len = sel.length;
        var targetScale = Number(scale);        // ABSOLUTE target scale (e.g., 120 = 120%)
        var targetRotation = Number(rotation);  // ABSOLUTE target rotation (e.g., 45 = 45Â°)
        MDUX_debugLog("[TRANSFORM-EACH] Selection: " + len + " items, targetScale=" + targetScale + ", targetRotation=" + targetRotation);
        var anchor = Number(MDUX_getDocumentScale()) || 100;
        var targetPercent = targetScale * (anchor / 100);

        // Handle Undo (legacy, not used with absolute values)
        if (undoPrevious === true || undoPrevious === "true") {
            app.executeMenuCommand('undo');
            sel = app.selection;
            if (!sel || sel.length === 0) return JSON.stringify({ ok: false, message: "Selection lost after undo." });
            len = sel.length;
        }

        var transformedCount = 0;
        var errors = 0;

        for (var i = 0; i < len; i++) {
            var item = sel[i];
            try {
                var isDuctLine = MDUX_isDuctworkLine(item);
                var isDuctPart = MDUX_isDuctworkPart(item);

                // OPTIMIZATION: Get metadata once, modify, set once (instead of multiple get/set calls)
                var meta = MDUX_getMetadata(item) || {};

                // Initialize metadata if needed
                if (meta.MDUX_OriginalWidth === undefined) {
                    var sWidth = 1;
                    try { sWidth = item.strokeWidth || 1; } catch (e) { }
                    meta.MDUX_OriginalWidth = item.width;
                    meta.MDUX_OriginalHeight = item.height;
                    meta.MDUX_OriginalStrokeWidth = sWidth;
                    meta.MDUX_OriginalRotation = "0";
                    meta.MDUX_CumulativeRotation = "0";
                    meta.MDUX_CurrentScale = "100";
                    meta.MDUX_RotationOverride = 0;
                }

                var currentScale = parseFloat(meta.MDUX_CurrentScale || "100");
                var currentRotation = parseFloat(meta.MDUX_CumulativeRotation || "0");

                MDUX_debugLog("[TRANSFORM-EACH] Item " + i + ": isDuctPart=" + isDuctPart + ", isDuctLine=" + isDuctLine);
                MDUX_debugLog("[TRANSFORM-EACH] currentRotation=" + currentRotation + ", targetRotation=" + targetRotation);

                // --- ROTATION (ABSOLUTE) ---
                // Calculate delta to reach target rotation from current rotation
                if (isDuctPart) {
                    var rotationDelta = targetRotation - currentRotation;
                    MDUX_debugLog("[TRANSFORM-EACH] rotationDelta=" + rotationDelta);
                    if (Math.abs(rotationDelta) > 0.001) {
                        MDUX_debugLog("[TRANSFORM-EACH] Applying rotation: " + rotationDelta + "Â°");
                        item.rotate(rotationDelta, true, true, true, true, Transformation.CENTER);
                        meta.MDUX_CumulativeRotation = String(targetRotation);
                        // Also sync MDUX_RotationOverride so rotation text box reads correct value
                        meta.MDUX_RotationOverride = targetRotation;
                        MDUX_debugLog("[TRANSFORM-EACH] Updated metadata: MDUX_CumulativeRotation=" + targetRotation);

                        // For PlacedItems, refresh the link to fix the bounding box after rotation
                        // (Same logic as rotateSelectionAbsolute in magic-final.jsx)
                        if (item.typename === "PlacedItem") {
                            try {
                                var linkedFile = item.file;
                                if (linkedFile && linkedFile.exists) {
                                    // Save note before relink - relink wipes metadata!
                                    var savedNote = item.note || "";
                                    item.file = linkedFile;
                                    try { item.relink(linkedFile); } catch (eRl) { }
                                    try { item.update(); } catch (eUp) { }
                                    // Restore the note after relink
                                    if (savedNote) {
                                        item.note = savedNote;
                                    }
                                    MDUX_debugLog("[TRANSFORM-EACH] Refreshed PlacedItem link to fix bounding box");
                                }
                            } catch (eRelink) {
                                MDUX_debugLog("[TRANSFORM-EACH] Could not refresh link: " + eRelink);
                            }
                        }
                    } else {
                        MDUX_debugLog("[TRANSFORM-EACH] Skipping rotation (delta too small)");
                    }
                } else {
                    MDUX_debugLog("[TRANSFORM-EACH] Skipping rotation (not a duct part)");
                }

                // --- SCALING (ABSOLUTE) ---
                if (Math.abs(currentScale - targetPercent) > 0.001) {
                    var resizeFactor = (targetPercent / currentScale) * 100;

                    if (isDuctLine) {
                        // Lines: scale stroke only
                        item.resize(100, 100, true, true, true, true, resizeFactor, Transformation.CENTER);
                    } else {
                        // Parts: scale geometry and stroke
                        item.resize(resizeFactor, resizeFactor, true, true, true, true, resizeFactor, Transformation.CENTER);
                    }

                    meta.MDUX_CurrentScale = String(targetPercent);
                }

                // OPTIMIZATION: Set metadata once after all changes
                MDUX_setMetadata(item, meta);
                transformedCount++;
            } catch (eItem) {
                errors++;
            }
        }

        return JSON.stringify({ ok: true, message: "Transformed " + transformedCount + "/" + len });

    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e.message });
    }
}

function MDUX_cppTransformEach(scale, rotation) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=transform;scale=" + scale + ";rotation=" + rotation;
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ transform error: " + e });
    }
}

function MDUX_cppTransformEachLive(scale, rotation) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=transform;scale=" + scale + ";rotation=" + rotation + ";live=1";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ live transform error: " + e });
    }
}

function MDUX_cppGetSelectedLineAngleBridge() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=get-angle";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ get angle error: " + e });
    }
}

function MDUX_cppSetRotationOverride(value) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=set-override;value=" + value;
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ set override error: " + e });
    }
}

function MDUX_cppClearRotationOverride() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=clear-override";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ clear override error: " + e });
    }
}

function MDUX_cppResetStrokes() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=reset-strokes";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ reset strokes error: " + e });
    }
}

function MDUX_cppResetScale() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=reset-scale";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ reset scale error: " + e });
    }
}

function MDUX_cppResetRotation() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=reset-rotation";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ reset rotation error: " + e });
    }
}

function MDUX_cppResetOriginal() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=reset-original";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ reset original error: " + e });
    }
}

function MDUX_cppQuickRotate(value) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=quick-rotate;value=" + value;
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ quick rotate error: " + e });
    }
}

function MDUX_cppProcessPlacedApi(payloadOverride) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }

        var payload = "action=process-placed-api";
        if (payloadOverride && typeof payloadOverride === "string") {
            payload = payloadOverride;
        }
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);

        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ process placed error: " + e });
    }
}

function MDUX_cppToggleSelectedEmoryConnector() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=toggle-connector-style";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ toggle connector error: " + e });
    }
}

function MDUX_cppMarkSelectedEmoryConnectorSeparate() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=mark-overlap-separate";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ mark separate connector error: " + e });
    }
}

function MDUX_cppGetSelectedEmorySegmentState() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: true, available: false, reason: "no-document" });
        }
        var payload = "action=get-emory-selection-state";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, available: false, message: "C++ Emory selection state error: " + e });
    }
}

function MDUX_cppSetSelectedEmoryStartSegment() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=set-emory-start-segment";
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ set Emory start error: " + e });
    }
}

function MDUX_cppApplySelectedEmorySegmentWidth(width) {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }
        var payload = "action=apply-emory-segment-width;width=" + width;
        var result = app.sendScriptMessage("EmoryDuctwork", "EmoryDuctworkPanel", payload);
        return result || JSON.stringify({ ok: false, message: "No response from C++ panel." });
    } catch (e) {
        return JSON.stringify({ ok: false, message: "C++ apply Emory width error: " + e });
    }
}

// Collect selected single-point anchor paths on target layers (Units, Registers, Thermostats, etc.)
function MDUX_collectSelectedTargetAnchors() {
    var anchors = [];
    try {
        var doc = app.activeDocument;
        var sel = doc.selection;
        if (!sel || sel.length === 0) return anchors;

        var TARGET_LAYERS = {
            "Units": "Unit.ai",
            "Square Registers": "Square Register.ai",
            "Rectangular Registers": "Rectangular Register.ai",
            "Circular Registers": "Circular Register.ai",
            "Exhaust Registers": "Exhaust Register.ai",
            "Secondary Exhaust Registers": "Secondary Exhaust Register.ai",
            "Orange Register": "Orange Register.ai",
            "Thermostats": "Thermostat.ai"
        };

        for (var i = 0; i < sel.length; i++) {
            try {
                var item = sel[i];
                if (item.typename !== "PathItem") continue;
                if (!item.pathPoints || item.pathPoints.length !== 1) continue;
                var layerName = item.layer ? item.layer.name : "";
                if (!TARGET_LAYERS[layerName]) continue;
                var anchor = item.pathPoints[0].anchor;
                anchors.push({
                    x: anchor[0],
                    y: anchor[1],
                    layer: layerName,
                    file: TARGET_LAYERS[layerName]
                });
            } catch (eItem) {}
        }
    } catch (e) {}
    return anchors;
}

// Place art at target-layer anchor positions that don't already have a PlacedItem nearby
function MDUX_placeArtAtTargetAnchors(anchors) {
    try {
        var doc = app.activeDocument;
        var COMPONENT_FILES_PATH = "E:/Work/Work/Floorplans/Ductwork Assets/";
        var ART_TOLERANCE = 15; // distance to consider art "already placed"

        for (var i = 0; i < anchors.length; i++) {
            var anchor = anchors[i];
            var ax = anchor.x;
            var ay = anchor.y;
            var layerName = anchor.layer;
            var fileName = anchor.file;

            // Get the target layer
            var layer = null;
            try { layer = doc.layers.getByName(layerName); } catch (e) { continue; }
            if (!layer || layer.locked) continue;

            // Check if art already exists near this position on the target layer
            var hasArt = false;
            try {
                for (var pi = 0; pi < layer.placedItems.length; pi++) {
                    var pItem = layer.placedItems[pi];
                    var bounds = pItem.geometricBounds;
                    var cx = (bounds[0] + bounds[2]) / 2;
                    var cy = (bounds[1] + bounds[3]) / 2;
                    var dx = cx - ax;
                    var dy = cy - ay;
                    if (Math.sqrt(dx * dx + dy * dy) <= ART_TOLERANCE) {
                        hasArt = true;
                        break;
                    }
                }
            } catch (eCheck) {}

            if (hasArt) continue;

            // Place art file
            var file = new File(COMPONENT_FILES_PATH + fileName);
            if (!file.exists) continue;

            // Determine scale from existing placed items on the layer
            var scale = 100;
            try {
                for (var si = 0; si < layer.placedItems.length; si++) {
                    try {
                        var sItem = layer.placedItems[si];
                        var matrix = sItem.matrix;
                        var scaleX = Math.sqrt(matrix.mValueA * matrix.mValueA + matrix.mValueB * matrix.mValueB);
                        var scaleY = Math.sqrt(matrix.mValueC * matrix.mValueC + matrix.mValueD * matrix.mValueD);
                        var itemScale = ((scaleX + scaleY) / 2) * 100;
                        if (itemScale < scale) scale = itemScale;
                    } catch (eScale) {}
                }
            } catch (eScaleScan) {}

            try {
                var newItem = layer.placedItems.add();
                newItem.file = file;
                try { MDUX_setPlacedItemName(newItem, file); } catch (eName) {}

                var newBounds = newItem.geometricBounds;
                var newWidth = Math.abs(newBounds[2] - newBounds[0]);
                var newHeight = Math.abs(newBounds[1] - newBounds[3]);

                if (scale !== 100) {
                    newItem.resize(scale, scale, true, false, false, false, 100, Transformation.CENTER);
                    newBounds = newItem.geometricBounds;
                    newWidth = Math.abs(newBounds[2] - newBounds[0]);
                    newHeight = Math.abs(newBounds[1] - newBounds[3]);
                }

                // Center on anchor
                newItem.position = [ax - newWidth / 2, ay + newHeight / 2];

                // Write metadata
                try {
                    var metadata = {
                        MDUX_OriginalWidth: newItem.width,
                        MDUX_OriginalHeight: newItem.height,
                        MDUX_OriginalStrokeWidth: 1,
                        MDUX_OriginalRotation: "0",
                        MDUX_CumulativeRotation: "0",
                        MDUX_CurrentScale: String(scale),
                        tagScale: scale,
                        tagRotation: 0
                    };
                    MDUX_setMetadata(newItem, metadata);
                } catch (eMeta) {}

                $.writeln("[POST-PROCESS] Placed " + fileName + " at [" + ax.toFixed(1) + "," + ay.toFixed(1) + "] on " + layerName + " (scale=" + scale.toFixed(1) + "%)");
            } catch (ePlace) {
                $.writeln("[POST-PROCESS] ERROR placing art: " + ePlace);
            }
        }
    } catch (e) {
        $.writeln("[POST-PROCESS] ERROR in MDUX_placeArtAtTargetAnchors: " + e);
    }
}

function MDUX_getSelectedLineAngleBridge() {
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false, message: "No document open." });
        }

        var sel = app.selection;
        if (!sel || sel.length === 0) {
            return JSON.stringify({ ok: false, message: "Please select a line." });
        }

        // Recursively collect all PathItems from selection (including groups and compound paths)
        function collectPaths(item, paths) {
            if (item.typename === "PathItem") {
                paths.push(item);
            } else if (item.typename === "GroupItem") {
                for (var i = 0; i < item.pageItems.length; i++) {
                    collectPaths(item.pageItems[i], paths);
                }
            } else if (item.typename === "CompoundPathItem") {
                for (var j = 0; j < item.pathItems.length; j++) {
                    paths.push(item.pathItems[j]);
                }
            }
        }

        var allPaths = [];
        for (var s = 0; s < sel.length; s++) {
            collectPaths(sel[s], allPaths);
        }

        if (allPaths.length === 0) {
            return JSON.stringify({ ok: false, message: "Please select a path or line." });
        }

        // Find the longest segment across all paths
        var longestLength = 0;
        var longestAngle = 0;
        var segmentCount = 0;

        for (var p = 0; p < allPaths.length; p++) {
            var pathItem = allPaths[p];
            if (!pathItem.pathPoints || pathItem.pathPoints.length < 2) continue;

            // Check each segment in the path
            for (var i = 0; i < pathItem.pathPoints.length - 1; i++) {
                var point1 = pathItem.pathPoints[i].anchor;
                var point2 = pathItem.pathPoints[i + 1].anchor;

                var dx = point2[0] - point1[0];
                var dy = point2[1] - point1[1];
                var segmentLength = Math.sqrt(dx * dx + dy * dy);
                segmentCount++;

                if (segmentLength > longestLength) {
                    longestLength = segmentLength;
                    // Calculate angle using Illustrator's rotation convention
                    var angleRadians = Math.atan2(dy, dx);
                    longestAngle = angleRadians * (180 / Math.PI);
                }
            }
        }

        if (longestLength === 0) {
            return JSON.stringify({ ok: false, message: "No valid line segments found." });
        }

        // Round to 1 decimal place
        longestAngle = Math.round(longestAngle * 10) / 10;

        return JSON.stringify({
            ok: true,
            angle: longestAngle,
            message: "Angle: " + longestAngle + "Â° (longest of " + segmentCount + " segments)"
        });

    } catch (e) {
        return JSON.stringify({ ok: false, message: "Error: " + e.message });
    }
}
// Test function to verify note property works
function MDUX_testNoteProperty() {
    try {
        if (app.documents.length === 0) return "ERROR: No document";
        var sel = app.selection;
        if (!sel || sel.length === 0) return "ERROR: Nothing selected";

        var item = sel[0];
        MDUX_debugLog("[TEST] Testing note property on " + item.typename);

        // Test 1: Set a simple note
        item.note = "TEST123";
        var readBack = item.note;
        MDUX_debugLog("[TEST] Set 'TEST123', read back: '" + readBack + "'");

        if (readBack !== "TEST123") {
            return "ERROR: Note property doesn't work on " + item.typename;
        }

        // Test 2: Set metadata
        MDUX_setMetadata(item, { testKey: "testValue", number: 42 });

        // Test 3: Read it back
        var meta = MDUX_getMetadata(item);
        if (!meta) {
            return "ERROR: Metadata not readable";
        }

        MDUX_debugLog("[TEST] Metadata read back: " + JSON.stringify(meta));
        return "SUCCESS: Note property works! Meta=" + JSON.stringify(meta);
    } catch (e) {
        return "ERROR: " + e;
    }
}

// Function to get debug log contents from memory buffer
function MDUX_getDebugLog() {
    // PERFORMANCE: Skip expensive operations if debug mode is off
    if (!MDUX_isDebugEnabled()) {
        return "Debug mode is disabled. Enable Debug Logging in the panel or set $.global.MDUX_DEBUG.ENABLED = true.";
    }

    try {
        // Add a test message to prove the buffer works
        MDUX_debugLog("MDUX_getDebugLog() was called at " + new Date().toString());

        var bufferInfo = "Buffer type: " + (typeof $.global.MDUX_debugBuffer) +
                        ", Is array: " + ($.global.MDUX_debugBuffer instanceof Array) +
                        ", Length: " + ($.global.MDUX_debugBuffer ? $.global.MDUX_debugBuffer.length : "N/A");

        if (typeof $.global.MDUX_debugBuffer === "undefined") {
            return "DEBUG BUFFER IS UNDEFINED!\n" + bufferInfo;
        }

        if ($.global.MDUX_debugBuffer.length === 0) {
            return "DEBUG BUFFER IS EMPTY (but exists)\n" + bufferInfo + "\nThis means no debug messages have been logged yet.";
        }

        // Write to file in Debug folder BEFORE constructing logContent
        var fileWriteStatus = "";
        try {
            var debugFolderPath = "C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Emory-Ductwork-Panel/Debug";
            var debugFolder = new Folder(debugFolderPath);
            fileWriteStatus += "Folder exists: " + debugFolder.exists + "; ";
            if (!debugFolder.exists) {
                var created = debugFolder.create();
                fileWriteStatus += "Created: " + created + "; ";
            }
            var timestamp = new Date();
            var dateStr = timestamp.getFullYear() + "-" +
                         ("0" + (timestamp.getMonth() + 1)).slice(-2) + "-" +
                         ("0" + timestamp.getDate()).slice(-2) + "_" +
                         ("0" + timestamp.getHours()).slice(-2) + "-" +
                         ("0" + timestamp.getMinutes()).slice(-2) + "-" +
                         ("0" + timestamp.getSeconds()).slice(-2);
            var debugFilePath = debugFolderPath + "/debug-" + dateStr + ".log";
            var debugFile = new File(debugFilePath);
            fileWriteStatus += "File: " + debugFilePath + "; ";
            var opened = debugFile.open("w");
            fileWriteStatus += "Opened: " + opened + "; ";
            if (opened) {
                debugFile.encoding = "UTF-8";
                var tempContent = "=== DEBUG LOG ===\n" + $.global.MDUX_debugBuffer.join("\n");
                debugFile.write(tempContent);
                debugFile.close();
                fileWriteStatus += "Wrote " + tempContent.length + " chars";
            } else {
                fileWriteStatus += "FAILED - error: " + debugFile.error;
            }
        } catch (fileErr) {
            fileWriteStatus += "EXCEPTION: " + fileErr;
        }

        // Add file write status to the buffer so it shows in clipboard
        $.global.MDUX_debugBuffer.push("[DEBUG-FILE] " + fileWriteStatus);

        var logContent = "=== DEBUG LOG ===\n" + $.global.MDUX_debugBuffer.join("\n");
        return logContent;
    } catch (e) {
        return "ERROR reading debug buffer: " + e;
    }
}

// Function to clear debug log buffer
function MDUX_clearDebugLog() {
    try {
        $.global.MDUX_debugBuffer = [];
        // Also reset any stuck state flags that could block processing
        $.global.MDUX_PROGRESS_CANCELLED = false;
        $.global.MDUX_PROGRESS_WIN = null;
        return "Debug buffer and state flags cleared";
    } catch (e) {
        return "ERROR clearing debug buffer: " + e;
    }
}

// Performance threshold - skip expensive metadata reads for large selections
var MDUX_LARGE_SELECTION_THRESHOLD = 50;

function MDUX_getSelectionTransformState() {
    if (MDUX_isCepSuspended()) {
        return JSON.stringify({ ok: false, reason: "suspended" });
    }
    try {
        if (app.documents.length === 0) {
            return JSON.stringify({ ok: false });
        }

        var sel = app.selection;
        if (!sel || sel.length === 0) {
            return JSON.stringify({ ok: false });
        }

        if (!MDUX_selectionHasPathItems(sel) && MDUX_selectionHasPlacedItems(sel)) {
            return JSON.stringify({
                ok: true,
                scale: null,
                rotation: null,
                mixedScale: true,
                mixedRotation: true,
                count: sel.length,
                reason: "placed-only"
            });
        }

        if (MDUX_selectionExceedsPathLimit(sel, MDUX_LIVE_SELECTION_PATH_LIMIT)) {
            MDUX_debugLog("[SELECT-STATE] Large selection (pathItems) - returning mixed to avoid lockup");
            return JSON.stringify({
                ok: true,
                scale: null,
                rotation: null,
                mixedScale: true,
                mixedRotation: true,
                count: sel.length,
                largeSelection: true
            });
        }

        // PERFORMANCE: For large selections, return "mixed" immediately without scanning metadata
        // This prevents Illustrator lockups when many items are selected
        if (sel.length > MDUX_LARGE_SELECTION_THRESHOLD) {
            MDUX_debugLog("[SELECT-STATE] Large selection (" + sel.length + " items) - returning mixed to avoid lockup");
            return JSON.stringify({
                ok: true,
                scale: null,
                rotation: null,
                mixedScale: true,
                mixedRotation: true,
                count: sel.length,
                largeSelection: true
            });
        }

        var anchor = parseFloat(MDUX_getDocumentScale()) || 100;
        var count = 0;

        var firstScale = null;
        var firstRot = null;
        var mixedScale = false;
        var mixedRot = false;

        for (var i = 0; i < sel.length; i++) {
            try {
                var item = sel[i];

                // Skip anchor points (single-point PathItems)
                if (item.typename === "PathItem") {
                    try {
                        if (item.pathPoints && item.pathPoints.length === 1) {
                            continue;
                        }
                    } catch (eAnchor) {}
                }

                // Get metadata once per item (optimization: single parse instead of multiple)
                var meta = MDUX_getMetadata(item);
                var tagScale = meta && meta.MDUX_CurrentScale !== undefined ? String(meta.MDUX_CurrentScale) : null;
                var tagRot = meta && meta.MDUX_CumulativeRotation !== undefined ? String(meta.MDUX_CumulativeRotation) : null;

                // Fallback for rotation on placed items
                if ((tagRot === null || tagRot === undefined) && item.typename === "PlacedItem") {
                    if (meta && meta.MDUX_RotationOverride !== undefined && meta.MDUX_RotationOverride !== null) {
                        tagRot = String(meta.MDUX_RotationOverride);
                    }
                }

                var absScale = tagScale !== null ? parseFloat(tagScale) : anchor;
                var rot = parseFloat(tagRot || "0");

                // Normalize scale relative to anchor for UI
                var uiScale = (absScale / (anchor / 100));
                uiScale = Math.round(uiScale * 100) / 100;
                rot = Math.round(rot * 100) / 100;

                if (firstScale === null) {
                    firstScale = uiScale;
                    firstRot = rot;
                } else {
                    if (Math.abs(uiScale - firstScale) > 0.1) mixedScale = true;
                    if (Math.abs(rot - firstRot) > 0.1) mixedRot = true;
                }
                count++;

                // PERFORMANCE: If we already know it's mixed, no need to keep checking
                if (mixedScale && mixedRot) break;
            } catch (e) {
                // Silent fail on individual items
            }
        }

        return JSON.stringify({
            ok: true,
            scale: mixedScale ? null : firstScale,
            rotation: mixedRot ? null : firstRot,
            mixedScale: mixedScale,
            mixedRotation: mixedRot,
            count: count
        });
    } catch (e) {
        return JSON.stringify({ ok: false, message: e.message });
    }
}

// --- RESET STROKES: Apply appropriate graphic styles to selected ductwork lines based on layer ---
function MDUX_resetStrokes() {
    // Outer safety wrapper
    try {
        if (typeof MDUX_debugLog !== "function") {
            return "Error: MDUX_debugLog not defined";
        }
        MDUX_debugLog("[RESET-STROKES] ========== FUNCTION ENTRY ==========");
    } catch (initErr) {
        return "Error at init: " + initErr;
    }

    try {
        MDUX_debugLog("[RESET-STROKES] Checking for active document...");
        var doc = null;
        try {
            doc = app.activeDocument;
        } catch (docErr) {
            MDUX_debugLog("[RESET-STROKES] Error getting activeDocument: " + docErr);
            return "Error: Cannot access active document";
        }

        if (!doc) {
            MDUX_debugLog("[RESET-STROKES] No document open");
            return "No document open";
        }
        MDUX_debugLog("[RESET-STROKES] Document: " + doc.name);

        MDUX_debugLog("[RESET-STROKES] Getting selection...");
        var sel = null;
        try {
            sel = doc.selection;
        } catch (selErr) {
            MDUX_debugLog("[RESET-STROKES] Error getting selection: " + selErr);
            return "Error: Cannot access selection";
        }

        if (!sel || sel.length === 0) {
            MDUX_debugLog("[RESET-STROKES] No selection");
            return "No selection";
        }
        MDUX_debugLog("[RESET-STROKES] Selection has " + sel.length + " items");

        // Layer to style mappings - ductwork lines
        var LAYER_STYLE_MAP = {
            "Green Ductwork": "Green Ductwork",
            "Light Green Ductwork": "Light Green Ductwork",
            "Blue Ductwork": "Blue Ductwork",
            "Orange Ductwork": "Orange Ductwork",
            "Light Orange Ductwork": "Light Orange Ductwork",
            "Thermostat Lines": "Thermostat Lines"
        };

        var count = 0;
        var errors = [];
        var styleCache = {};

        // Get graphic style by name
        function getStyle(styleName) {
            if (styleCache[styleName] !== undefined) return styleCache[styleName];
            try {
                MDUX_debugLog("[RESET-STROKES] Looking for style: " + styleName);
                var styleCount = doc.graphicStyles.length;
                MDUX_debugLog("[RESET-STROKES] Document has " + styleCount + " graphic styles");
                for (var i = 0; i < styleCount; i++) {
                    try {
                        var gs = doc.graphicStyles[i];
                        if (gs && gs.name === styleName) {
                            styleCache[styleName] = gs;
                            MDUX_debugLog("[RESET-STROKES] Found style: " + styleName);
                            return gs;
                        }
                    } catch (styleItemErr) {
                        MDUX_debugLog("[RESET-STROKES] Error accessing style " + i + ": " + styleItemErr);
                    }
                }
            } catch (e) {
                MDUX_debugLog("[RESET-STROKES] Error in getStyle: " + e);
            }
            MDUX_debugLog("[RESET-STROKES] Style NOT found: " + styleName);
            styleCache[styleName] = null;
            return null;
        }

        function applyStyleToItem(item) {
            try {
                var layerName = null;
                try {
                    layerName = item.layer ? item.layer.name : null;
                } catch (layerErr) {
                    MDUX_debugLog("[RESET-STROKES] Error getting layer: " + layerErr);
                    return;
                }

                MDUX_debugLog("[RESET-STROKES] Item type=" + item.typename + ", layer=" + layerName);
                if (!layerName) return;

                var styleName = LAYER_STYLE_MAP[layerName];
                if (!styleName) {
                    MDUX_debugLog("[RESET-STROKES] No style mapping for layer: " + layerName);
                    return;
                }

                var style = getStyle(styleName);
                if (!style) {
                    MDUX_debugLog("[RESET-STROKES] Style not available: " + styleName);
                    return;
                }

                MDUX_debugLog("[RESET-STROKES] Applying style " + styleName + " to item...");
                try {
                    style.applyTo(item);
                    MDUX_debugLog("[RESET-STROKES] Applied style successfully");
                    count++;
                } catch (applyErr) {
                    MDUX_debugLog("[RESET-STROKES] Error applying style: " + applyErr);
                    errors.push("Apply error: " + applyErr);
                }
            } catch (e) {
                MDUX_debugLog("[RESET-STROKES] Error in applyStyleToItem: " + e);
                errors.push("Item error: " + e);
            }
        }

        function walkItems(item) {
            if (!item) return;
            try {
                var typeName = item.typename;
                MDUX_debugLog("[RESET-STROKES] Walking item type: " + typeName);

                if (typeName === "GroupItem") {
                    try {
                        var itemCount = item.pageItems.length;
                        for (var i = 0; i < itemCount; i++) {
                            walkItems(item.pageItems[i]);
                        }
                    } catch (groupErr) {
                        MDUX_debugLog("[RESET-STROKES] Error walking group: " + groupErr);
                    }
                } else if (typeName === "CompoundPathItem") {
                    applyStyleToItem(item);
                } else if (typeName === "PathItem") {
                    try {
                        if (!item.guides && !item.clipping) {
                            applyStyleToItem(item);
                        }
                    } catch (pathErr) {
                        MDUX_debugLog("[RESET-STROKES] Error checking path properties: " + pathErr);
                    }
                }
            } catch (walkErr) {
                MDUX_debugLog("[RESET-STROKES] Error in walkItems: " + walkErr);
            }
        }

        MDUX_debugLog("[RESET-STROKES] Starting to walk " + sel.length + " selected items...");
        for (var i = 0; i < sel.length; i++) {
            try {
                walkItems(sel[i]);
            } catch (loopErr) {
                MDUX_debugLog("[RESET-STROKES] Error processing item " + i + ": " + loopErr);
            }
        }

        MDUX_debugLog("[RESET-STROKES] ========== COMPLETE ==========");
        MDUX_debugLog("[RESET-STROKES] Applied styles to " + count + " items, " + errors.length + " errors");

        if (errors.length > 0) {
            return "Applied to " + count + " item(s), " + errors.length + " error(s)";
        }
        return "Applied styles to " + count + " item(s)";
    } catch (e) {
        try {
            MDUX_debugLog("[RESET-STROKES] FATAL ERROR: " + e + " (line: " + e.line + ")");
        } catch (logErr) {}
        return "Error: " + (e.message || e.toString());
    }
}

// --- RESET DUCTWORK PARTS SCALE: Reset ductwork parts to original scale while keeping rotation ---
function MDUX_resetDuctworkPartsScale() {
    // Outer safety wrapper
    try {
        if (typeof MDUX_debugLog !== "function") {
            return "Error: MDUX_debugLog not defined";
        }
        MDUX_debugLog("[RESET-PARTS-SCALE] ========== FUNCTION ENTRY ==========");
    } catch (initErr) {
        return "Error at init: " + initErr;
    }

    try {
        MDUX_debugLog("[RESET-PARTS-SCALE] Checking for active document...");
        var doc = null;
        try {
            doc = app.activeDocument;
        } catch (docErr) {
            MDUX_debugLog("[RESET-PARTS-SCALE] Error getting activeDocument: " + docErr);
            return "Error: Cannot access active document";
        }

        if (!doc) {
            MDUX_debugLog("[RESET-PARTS-SCALE] No document open");
            return "No document open";
        }
        MDUX_debugLog("[RESET-PARTS-SCALE] Document: " + doc.name);

        MDUX_debugLog("[RESET-PARTS-SCALE] Getting selection...");
        var sel = null;
        try {
            sel = doc.selection;
        } catch (selErr) {
            MDUX_debugLog("[RESET-PARTS-SCALE] Error getting selection: " + selErr);
            return "Error: Cannot access selection";
        }

        if (!sel || sel.length === 0) {
            MDUX_debugLog("[RESET-PARTS-SCALE] No selection");
            return "No selection";
        }
        MDUX_debugLog("[RESET-PARTS-SCALE] Selection has " + sel.length + " items");

        var count = 0;
        var skipped = 0;
        var errors = [];

        function resetPartScale(item) {
            try {
                MDUX_debugLog("[RESET-PARTS-SCALE] Processing " + item.typename);

                // Get current scale from metadata
                var currentScaleTag = null;
                try {
                    currentScaleTag = MDUX_getTag(item, "MDUX_CurrentScale");
                } catch (tagErr) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] Error getting tag: " + tagErr);
                    skipped++;
                    return;
                }
                MDUX_debugLog("[RESET-PARTS-SCALE] MDUX_CurrentScale tag = " + currentScaleTag);

                if (currentScaleTag === null) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] No scale metadata, skipping");
                    skipped++;
                    return;
                }

                var currentScale = parseFloat(currentScaleTag);
                if (isNaN(currentScale) || currentScale <= 0) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] Invalid scale value: " + currentScaleTag);
                    skipped++;
                    return;
                }

                // If already at 100%, skip
                if (Math.abs(currentScale - 100) < 1) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] Already at 100%, skipping");
                    return;
                }

                // Calculate the scale factor needed to return to 100%
                // If current is 50%, we need to scale by 200% (100/50 * 100)
                var resetScalePercent = (100 / currentScale) * 100;
                MDUX_debugLog("[RESET-PARTS-SCALE] Current: " + currentScale + "%, reset factor: " + resetScalePercent + "%");

                // Apply uniform scale to reset to original size
                try {
                    var matrix = app.getScaleMatrix(resetScalePercent, resetScalePercent);
                    // Use simpler transform call that works with more item types
                    item.transform(matrix);
                    MDUX_debugLog("[RESET-PARTS-SCALE] Transform applied successfully");
                } catch (transformErr) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] Transform error: " + transformErr);
                    errors.push("Transform error: " + transformErr);
                    skipped++;
                    return;
                }

                // Update metadata
                try {
                    MDUX_setTag(item, "MDUX_CurrentScale", "100");
                    MDUX_debugLog("[RESET-PARTS-SCALE] Set MDUX_CurrentScale to 100");
                } catch (setTagErr) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] Error setting tag: " + setTagErr);
                }

                count++;
            } catch (e) {
                MDUX_debugLog("[RESET-PARTS-SCALE] Error in resetPartScale: " + e);
                errors.push("Reset error: " + e);
                skipped++;
            }
        }

        function walkItems(item) {
            if (!item) return;
            try {
                var typeName = item.typename;
                MDUX_debugLog("[RESET-PARTS-SCALE] Walking item type: " + typeName);

                // Check if item has MDUX_CurrentScale metadata
                var hasScaleTag = false;
                try {
                    hasScaleTag = MDUX_getTag(item, "MDUX_CurrentScale") !== null;
                } catch (tagCheckErr) {
                    MDUX_debugLog("[RESET-PARTS-SCALE] Error checking tag: " + tagCheckErr);
                }

                // Handle PlacedItem (linked/embedded images or symbols)
                if (typeName === "PlacedItem" || typeName === "SymbolItem") {
                    if (hasScaleTag) {
                        resetPartScale(item);
                    } else {
                        MDUX_debugLog("[RESET-PARTS-SCALE] " + typeName + " has no scale metadata");
                        skipped++;
                    }
                } else if (typeName === "GroupItem") {
                    // Groups with MDUX metadata are ductwork parts (Units, Registers, etc.)
                    if (hasScaleTag) {
                        resetPartScale(item);
                    } else {
                        // Walk into group to find nested items
                        try {
                            var itemCount = item.pageItems.length;
                            for (var i = 0; i < itemCount; i++) {
                                walkItems(item.pageItems[i]);
                            }
                        } catch (groupErr) {
                            MDUX_debugLog("[RESET-PARTS-SCALE] Error walking group: " + groupErr);
                        }
                    }
                } else if (typeName === "CompoundPathItem" || typeName === "PathItem") {
                    // Check if this item has scale metadata stored
                    if (hasScaleTag) {
                        resetPartScale(item);
                    }
                }
            } catch (walkErr) {
                MDUX_debugLog("[RESET-PARTS-SCALE] Error in walkItems: " + walkErr);
            }
        }

        MDUX_debugLog("[RESET-PARTS-SCALE] Starting to walk " + sel.length + " selected items...");
        for (var i = 0; i < sel.length; i++) {
            try {
                walkItems(sel[i]);
            } catch (loopErr) {
                MDUX_debugLog("[RESET-PARTS-SCALE] Error processing item " + i + ": " + loopErr);
            }
        }

        MDUX_debugLog("[RESET-PARTS-SCALE] ========== COMPLETE ==========");
        MDUX_debugLog("[RESET-PARTS-SCALE] Reset " + count + " parts, skipped " + skipped + ", errors " + errors.length);

        if (count === 0 && skipped > 0) {
            return "No parts with scale metadata found (checked " + skipped + " items)";
        }
        return "Reset scale on " + count + " part(s) to 100%" + (skipped > 0 ? ", skipped " + skipped : "");
    } catch (e) {
        try {
            MDUX_debugLog("[RESET-PARTS-SCALE] FATAL ERROR: " + e + " (line: " + e.line + ")");
        } catch (logErr) {}
        return "Error: " + e.message;
    }
}

// ============================================================================
// MERGE PATHS AT ENDPOINTS
// Joins selected path(s) with adjacent paths that share endpoints
// Works with single path selection - automatically finds neighbors on same layer
// ============================================================================
function MDUX_mergePathsAtEndpoints() {
    try {
        MDUX_debugLog("[MERGE-PATHS] ========== STARTING ==========");

        var doc = app.activeDocument;
        if (!doc) {
            MDUX_debugLog("[MERGE-PATHS] No document open");
            return "No document open";
        }

        var sel = doc.selection;
        if (!sel || sel.length < 1) {
            MDUX_debugLog("[MERGE-PATHS] No selection");
            return "Select at least one path";
        }

        MDUX_debugLog("[MERGE-PATHS] Selection has " + sel.length + " items");

        // Collect selected PathItems and their layers
        var selectedPaths = [];
        var layersToSearch = {};

        for (var i = 0; i < sel.length; i++) {
            var item = sel[i];
            MDUX_debugLog("[MERGE-PATHS] Item " + i + ": " + item.typename);

            if (item.typename === "PathItem") {
                if (!item.closed && item.pathPoints && item.pathPoints.length >= 2) {
                    selectedPaths.push(item);
                    try {
                        var layerName = item.layer.name;
                        layersToSearch[layerName] = item.layer;
                        MDUX_debugLog("[MERGE-PATHS] Added path from layer: " + layerName);
                    } catch (e) { }
                }
            } else if (item.typename === "CompoundPathItem" && item.pathItems) {
                MDUX_debugLog("[MERGE-PATHS] CompoundPathItem with " + item.pathItems.length + " children");
                for (var j = 0; j < item.pathItems.length; j++) {
                    var subPath = item.pathItems[j];
                    if (!subPath.closed && subPath.pathPoints && subPath.pathPoints.length >= 2) {
                        selectedPaths.push(subPath);
                        try {
                            var layerName2 = item.layer.name;
                            layersToSearch[layerName2] = item.layer;
                        } catch (e) { }
                    }
                }
            }
        }

        MDUX_debugLog("[MERGE-PATHS] Found " + selectedPaths.length + " open path(s) in selection");

        if (selectedPaths.length < 1) {
            return "No open paths in selection";
        }

        // Collect all candidate paths - selected paths plus any on the same layer(s)
        var allPaths = selectedPaths.slice();

        MDUX_debugLog("[MERGE-PATHS] Searching for adjacent paths on same layer(s)...");

        for (var layerName in layersToSearch) {
            var layer = layersToSearch[layerName];
            MDUX_debugLog("[MERGE-PATHS] Searching layer: " + layerName + " (" + layer.pathItems.length + " paths)");

            for (var pi = 0; pi < layer.pathItems.length; pi++) {
                var layerPath = layer.pathItems[pi];
                if (!layerPath.closed && layerPath.pathPoints && layerPath.pathPoints.length >= 2) {
                    var alreadyAdded = false;
                    for (var ap = 0; ap < allPaths.length; ap++) {
                        if (allPaths[ap] === layerPath) { alreadyAdded = true; break; }
                    }
                    if (!alreadyAdded) allPaths.push(layerPath);
                }
            }

            // Also check compound paths on layer
            for (var ci = 0; ci < layer.compoundPathItems.length; ci++) {
                var compound = layer.compoundPathItems[ci];
                for (var cpi = 0; cpi < compound.pathItems.length; cpi++) {
                    var cPath = compound.pathItems[cpi];
                    if (!cPath.closed && cPath.pathPoints && cPath.pathPoints.length >= 2) {
                        var alreadyAdded2 = false;
                        for (var ap2 = 0; ap2 < allPaths.length; ap2++) {
                            if (allPaths[ap2] === cPath) { alreadyAdded2 = true; break; }
                        }
                        if (!alreadyAdded2) allPaths.push(cPath);
                    }
                }
            }
        }

        MDUX_debugLog("[MERGE-PATHS] Total paths to check: " + allPaths.length);

        if (allPaths.length < 2) {
            return "Need at least 2 paths to merge";
        }

        var TOLERANCE = 5.0; // Generous tolerance for matching endpoints
        var mergeCount = 0;
        var removedPaths = [];

        // Helper: Get endpoint coordinates
        function getEndpoints(path) {
            try {
                var pts = path.pathPoints;
                if (!pts || pts.length < 2) return null;
                return {
                    start: [pts[0].anchor[0], pts[0].anchor[1]],
                    end: [pts[pts.length - 1].anchor[0], pts[pts.length - 1].anchor[1]]
                };
            } catch (e) { return null; }
        }

        // Helper: Distance between points
        function pointDistance(p1, p2) {
            var dx = p1[0] - p2[0];
            var dy = p1[1] - p2[1];
            return Math.sqrt(dx * dx + dy * dy);
        }

        // Helper: Check if path is still valid
        function isPathValid(path) {
            try { return path && path.pathPoints && path.pathPoints.length >= 2; }
            catch (e) { return false; }
        }

        // Helper: Check if path was removed
        function wasRemoved(path) {
            for (var r = 0; r < removedPaths.length; r++) {
                if (removedPaths[r] === path) return true;
            }
            return false;
        }

        // Helper: Merge paths by creating a NEW path (avoids setEntirePath reference bug)
        // Returns: { newPath: PathItem, pathsToRemove: [pathA, pathB] }
        function mergePaths(pathA, pathB, matchType, targetLayer) {
            var ptsA = pathA.pathPoints;
            var ptsB = pathB.pathPoints;

            MDUX_debugLog("[MERGE-PATHS] Merging: " + matchType + " (A:" + ptsA.length + "pts, B:" + ptsB.length + "pts)");

            var allPoints = [];

            // Skip the junction anchors on BOTH paths to create a clean seamless join
            if (matchType === "A_end_B_start") {
                // A's end connects to B's start - skip A's last point AND B's first point
                for (var i = 0; i < ptsA.length - 1; i++) allPoints.push([ptsA[i].anchor[0], ptsA[i].anchor[1]]);
                for (var i = 1; i < ptsB.length; i++) allPoints.push([ptsB[i].anchor[0], ptsB[i].anchor[1]]);
            } else if (matchType === "A_end_B_end") {
                // A's end connects to B's end - skip A's last point AND B's last point
                for (var i = 0; i < ptsA.length - 1; i++) allPoints.push([ptsA[i].anchor[0], ptsA[i].anchor[1]]);
                for (var i = ptsB.length - 2; i >= 0; i--) allPoints.push([ptsB[i].anchor[0], ptsB[i].anchor[1]]);
            } else if (matchType === "A_start_B_end") {
                // A's start connects to B's end - skip B's last point AND A's first point
                for (var i = 0; i < ptsB.length - 1; i++) allPoints.push([ptsB[i].anchor[0], ptsB[i].anchor[1]]);
                for (var i = 1; i < ptsA.length; i++) allPoints.push([ptsA[i].anchor[0], ptsA[i].anchor[1]]);
            } else if (matchType === "A_start_B_start") {
                // A's start connects to B's start - skip B's first point AND A's first point
                for (var i = ptsB.length - 1; i >= 1; i--) allPoints.push([ptsB[i].anchor[0], ptsB[i].anchor[1]]);
                for (var i = 1; i < ptsA.length; i++) allPoints.push([ptsA[i].anchor[0], ptsA[i].anchor[1]]);
            }

            MDUX_debugLog("[MERGE-PATHS] Creating NEW path with " + allPoints.length + " points");

            // Create a NEW path on the target layer (avoids setEntirePath reference invalidation bug)
            var newPath = targetLayer.pathItems.add();
            newPath.setEntirePath(allPoints);

            // Copy styling from pathA
            try { newPath.strokeColor = pathA.strokeColor; } catch (e) {}
            try { newPath.strokeWidth = pathA.strokeWidth; } catch (e) {}
            try { newPath.fillColor = pathA.fillColor; } catch (e) {}
            try { newPath.filled = pathA.filled; } catch (e) {}
            try { newPath.stroked = pathA.stroked; } catch (e) {}
            try { newPath.strokeCap = pathA.strokeCap; } catch (e) {}
            try { newPath.strokeJoin = pathA.strokeJoin; } catch (e) {}

            return { newPath: newPath, pathsToRemove: [pathA, pathB] };
        }

        // Merge loop - keep merging until no more matches
        var keepMerging = true;
        var iterations = 0;
        var maxIterations = allPaths.length * 5; // Increased since we're creating new paths
        var mergedPaths = []; // Track newly created merged paths for final selection

        while (keepMerging && iterations < maxIterations) {
            keepMerging = false;
            iterations++;
            MDUX_debugLog("[MERGE-PATHS] === Iteration " + iterations + " ===");

            for (var i = 0; i < selectedPaths.length; i++) {
                var pathA = selectedPaths[i];
                if (!isPathValid(pathA) || wasRemoved(pathA)) continue;

                var endpointsA = getEndpoints(pathA);
                if (!endpointsA) continue;

                MDUX_debugLog("[MERGE-PATHS] Checking pathA[" + i + "] start=[" + endpointsA.start[0].toFixed(1) + "," + endpointsA.start[1].toFixed(1) + "] end=[" + endpointsA.end[0].toFixed(1) + "," + endpointsA.end[1].toFixed(1) + "]");

                for (var j = 0; j < allPaths.length; j++) {
                    var pathB = allPaths[j];
                    if (pathB === pathA || !isPathValid(pathB) || wasRemoved(pathB)) continue;

                    var endpointsB = getEndpoints(pathB);
                    if (!endpointsB) continue;

                    // Find closest matching endpoints
                    var matchType = null;
                    var minDist = TOLERANCE;

                    var d1 = pointDistance(endpointsA.end, endpointsB.start);
                    var d2 = pointDistance(endpointsA.end, endpointsB.end);
                    var d3 = pointDistance(endpointsA.start, endpointsB.end);
                    var d4 = pointDistance(endpointsA.start, endpointsB.start);

                    if (d1 < minDist) { matchType = "A_end_B_start"; minDist = d1; }
                    if (d2 < minDist) { matchType = "A_end_B_end"; minDist = d2; }
                    if (d3 < minDist) { matchType = "A_start_B_end"; minDist = d3; }
                    if (d4 < minDist) { matchType = "A_start_B_start"; minDist = d4; }

                    if (matchType) {
                        MDUX_debugLog("[MERGE-PATHS] MATCH! dist=" + minDist.toFixed(2) + " type=" + matchType);
                        try {
                            // Get target layer from pathA
                            var targetLayer = pathA.layer;

                            // Create new merged path
                            var result = mergePaths(pathA, pathB, matchType, targetLayer);
                            var newPath = result.newPath;

                            // Remove BOTH old paths from document
                            try { pathA.remove(); } catch (e) { MDUX_debugLog("[MERGE-PATHS] Failed to remove pathA: " + e); }
                            try { pathB.remove(); } catch (e) { MDUX_debugLog("[MERGE-PATHS] Failed to remove pathB: " + e); }

                            // Mark both as removed
                            removedPaths.push(pathA);
                            removedPaths.push(pathB);

                            // Add new path to arrays so it can be merged further
                            selectedPaths.push(newPath);
                            allPaths.push(newPath);
                            mergedPaths.push(newPath);

                            mergeCount++;
                            keepMerging = true;
                            MDUX_debugLog("[MERGE-PATHS] Merge successful! New path added to arrays.");
                        } catch (mergeErr) {
                            MDUX_debugLog("[MERGE-PATHS] Merge error: " + mergeErr);
                        }
                        break;
                    }
                }
                if (keepMerging) break; // Restart outer loop
            }
        }

        MDUX_debugLog("[MERGE-PATHS] Merge loop done after " + iterations + " iterations");

        // Select the merged path(s)
        try {
            doc.selection = null;
            // Select newly created merged paths
            for (var mp = 0; mp < mergedPaths.length; mp++) {
                try {
                    var mergedP = mergedPaths[mp];
                    if (mergedP && isPathValid(mergedP) && !wasRemoved(mergedP)) {
                        mergedP.selected = true;
                    }
                } catch (selErr) { /* path error, skip */ }
            }
            // Also select any original selected paths that weren't merged
            for (var i = 0; i < selectedPaths.length; i++) {
                try {
                    var p = selectedPaths[i];
                    if (p && !wasRemoved(p) && isPathValid(p)) {
                        p.selected = true;
                    }
                } catch (selErr) { /* path no longer valid, skip */ }
            }
        } catch (e) { /* selection error, ignore */ }

        MDUX_debugLog("[MERGE-PATHS] ========== COMPLETE: " + mergeCount + " merge(s) ==========");

        if (mergeCount === 0) {
            return "No adjacent endpoints found (within " + TOLERANCE + "pt)";
        }
        return "Merged " + mergeCount + " path(s)";

    } catch (e) {
        MDUX_debugLog("[MERGE-PATHS] FATAL ERROR: " + e + " (line: " + e.line + ")");
        return "Error: " + e.message;
    }
}

// ============================================
// V3 HELPER FUNCTIONS
// ============================================

// Get duct role (trunk/branch) from path metadata
function MDUX_getDuctRoleForPath(pathItem) {
    if (!pathItem) return null;
    try {
        var meta = MDUX_getMetadata(pathItem);
        if (meta && (meta.ductRole === "trunk" || meta.ductRole === "branch")) {
            return meta.ductRole;
        }
    } catch (e) {}
    return null;
}

// Collect selected paths that are valid ductwork lines
function MDUX_collectSelectedDuctworkPaths() {
    var paths = [];
    try {
        var doc = app.activeDocument;
        var sel = doc.selection;
        if (!sel || sel.length === 0) return paths;

        for (var i = 0; i < sel.length; i++) {
            var item = sel[i];
            try {
                if (item.typename === "PathItem" && !item.guides && !item.clipping) {
                    if (item.pathPoints && item.pathPoints.length >= 2) {
                        paths.push(item);
                    }
                } else if (item.typename === "CompoundPathItem") {
                    for (var j = 0; j < item.pathItems.length; j++) {
                        var subPath = item.pathItems[j];
                        if (subPath.pathPoints && subPath.pathPoints.length >= 2) {
                            paths.push(subPath);
                        }
                    }
                }
            } catch (e) {}
        }
    } catch (e) {
        MDUX_debugLog("[V3-HELPER] Error collecting paths: " + e);
    }
    return paths;
}

// Filter paths by duct role
function MDUX_filterPathsByRole(paths, targetRole) {
    var filtered = [];
    for (var i = 0; i < paths.length; i++) {
        var role = MDUX_getDuctRoleForPath(paths[i]);
        if (role === targetRole) {
            filtered.push(paths[i]);
        }
    }
    return filtered;
}

// Store pre-ortho geometry for a path (backup before orthogonalization)
function MDUX_storePreOrthoGeometry(pathItem) {
    if (!pathItem || !pathItem.pathPoints) return;
    try {
        var points = [];
        for (var i = 0; i < pathItem.pathPoints.length; i++) {
            var pt = pathItem.pathPoints[i];
            points.push({
                anchor: [pt.anchor[0], pt.anchor[1]],
                left: [pt.leftDirection[0], pt.leftDirection[1]],
                right: [pt.rightDirection[0], pt.rightDirection[1]]
            });
        }
        var encoded = JSON.stringify(points);
        var note = pathItem.note || "";
        var PREFIX = "MD:PREORTHO=";
        // Remove existing pre-ortho data
        var tokens = note.split(";");
        var filtered = [];
        for (var t = 0; t < tokens.length; t++) {
            if (tokens[t].indexOf(PREFIX) !== 0) {
                filtered.push(tokens[t]);
            }
        }
        filtered.push(PREFIX + encoded);
        pathItem.note = filtered.join(";");
    } catch (e) {
        MDUX_debugLog("[PRE-ORTHO] Error storing geometry: " + e);
    }
}

// ============================================
// T-JUNCTION DETECTION AND RESTORATION
// Preserves endpoint-to-segment connections during orthogonalization
// ============================================

// Detect T-junctions: endpoints that project onto the middle of other path segments
// Returns array of T-junction objects with path references and connection info
function MDUX_detectTJunctions(paths) {
    var tJunctions = [];
    var T_JUNCTION_TOLERANCE = 3; // Distance tolerance in points

    MDUX_debugLog("[PRE-ORTHO-TJ] Detecting T-junctions for " + paths.length + " paths");

    for (var pathAIdx = 0; pathAIdx < paths.length; pathAIdx++) {
        var pathA = paths[pathAIdx];

        // Validate path
        try {
            var ptsA = pathA.pathPoints;
            if (!ptsA || ptsA.length < 2) continue;
        } catch (e) {
            continue; // Path is invalid
        }

        var ptsA = pathA.pathPoints;

        // Check each endpoint of pathA (index 0 and length-1)
        for (var epIdx = 0; epIdx < 2; epIdx++) {
            var endpointIndex = (epIdx === 0) ? 0 : ptsA.length - 1;
            var endpoint = ptsA[endpointIndex].anchor;

            // Check against all OTHER paths' segments
            for (var pathBIdx = 0; pathBIdx < paths.length; pathBIdx++) {
                if (pathBIdx === pathAIdx) continue; // Skip same path

                var pathB = paths[pathBIdx];

                // Validate pathB
                try {
                    var ptsB = pathB.pathPoints;
                    if (!ptsB || ptsB.length < 2) continue;
                } catch (e) {
                    continue;
                }

                var ptsB = pathB.pathPoints;

                // Check each segment of pathB
                for (var segIdx = 0; segIdx < ptsB.length - 1; segIdx++) {
                    var segStart = ptsB[segIdx].anchor;
                    var segEnd = ptsB[segIdx + 1].anchor;

                    // Calculate closest point on segment to endpoint using parametric projection
                    var segDx = segEnd[0] - segStart[0];
                    var segDy = segEnd[1] - segStart[1];
                    var segLenSq = segDx * segDx + segDy * segDy;

                    if (segLenSq < 0.001) continue; // Skip zero-length segments

                    // Calculate t parameter (0-1 for points on segment)
                    var t = ((endpoint[0] - segStart[0]) * segDx + (endpoint[1] - segStart[1]) * segDy) / segLenSq;

                    // Only consider points that project onto the MIDDLE of segment (not endpoints)
                    // t between 0.001 and 0.999 means it's not at the endpoints
                    if (t > 0.001 && t < 0.999) {
                        var closestX = segStart[0] + t * segDx;
                        var closestY = segStart[1] + t * segDy;

                        var dx = endpoint[0] - closestX;
                        var dy = endpoint[1] - closestY;
                        var dist = Math.sqrt(dx * dx + dy * dy);

                        if (dist <= T_JUNCTION_TOLERANCE) {
                            // Found a T-junction
                            tJunctions.push({
                                endpointPath: pathA,
                                endpointPathIndex: pathAIdx,
                                endpointIndex: endpointIndex,
                                segmentPath: pathB,
                                segmentPathIndex: pathBIdx,
                                segmentIndex: segIdx,
                                originalDist: dist,
                                originalT: t
                            });

                            MDUX_debugLog("[PRE-ORTHO-TJ] Found T-junction: endpoint at [" +
                                endpoint[0].toFixed(1) + "," + endpoint[1].toFixed(1) +
                                "] -> segment (dist=" + dist.toFixed(2) + "pt, t=" + t.toFixed(3) + ")");
                        }
                    }
                }
            }
        }
    }

    MDUX_debugLog("[PRE-ORTHO-TJ] Found " + tJunctions.length + " T-junction(s)");
    return tJunctions;
}

// Restore T-junctions after orthogonalization
// Snaps endpoints back to segments to maintain connections
function MDUX_restoreTJunctions(tJunctions) {
    if (!tJunctions || tJunctions.length === 0) return 0;

    MDUX_debugLog("\n=== POST-ORTHO: RESTORING T-JUNCTION CONNECTIONS ===");
    var restoredCount = 0;

    for (var i = 0; i < tJunctions.length; i++) {
        var tj = tJunctions[i];

        try {
            var endpointPath = tj.endpointPath;
            var segmentPath = tj.segmentPath;

            // Get CURRENT positions after orthogonalization
            var epPts = endpointPath.pathPoints;
            var segPts = segmentPath.pathPoints;

            if (!epPts || epPts.length <= tj.endpointIndex) continue;
            if (!segPts || segPts.length <= tj.segmentIndex + 1) continue;

            var currentEndpoint = epPts[tj.endpointIndex].anchor;
            var currentSegStart = segPts[tj.segmentIndex].anchor;
            var currentSegEnd = segPts[tj.segmentIndex + 1].anchor;

            // Calculate closest point on CURRENT segment
            var segDx = currentSegEnd[0] - currentSegStart[0];
            var segDy = currentSegEnd[1] - currentSegStart[1];
            var segLenSq = segDx * segDx + segDy * segDy;

            if (segLenSq < 0.001) continue; // Skip zero-length segments

            var t = ((currentEndpoint[0] - currentSegStart[0]) * segDx +
                     (currentEndpoint[1] - currentSegStart[1]) * segDy) / segLenSq;

            // Clamp t to [0, 1] to stay on segment
            if (t < 0) t = 0;
            if (t > 1) t = 1;

            var snapX = currentSegStart[0] + t * segDx;
            var snapY = currentSegStart[1] + t * segDy;

            // Calculate current distance
            var currentDx = currentEndpoint[0] - snapX;
            var currentDy = currentEndpoint[1] - snapY;
            var currentDist = Math.sqrt(currentDx * currentDx + currentDy * currentDy);

            // Only snap if distance is reasonable (within 10pt)
            // This prevents snapping to wrong segments if paths moved significantly
            if (currentDist <= 10) {
                // Apply the snap
                epPts[tj.endpointIndex].anchor = [snapX, snapY];
                epPts[tj.endpointIndex].leftDirection = [snapX, snapY];
                epPts[tj.endpointIndex].rightDirection = [snapX, snapY];
                restoredCount++;

                MDUX_debugLog("[POST-ORTHO-TJ] Snapped endpoint from [" +
                    currentEndpoint[0].toFixed(1) + "," + currentEndpoint[1].toFixed(1) +
                    "] to [" + snapX.toFixed(1) + "," + snapY.toFixed(1) +
                    "] (was " + currentDist.toFixed(2) + "pt away)");
            } else {
                MDUX_debugLog("[POST-ORTHO-TJ] SKIPPED snapping (distance " +
                    currentDist.toFixed(2) + "pt too large, paths moved significantly)");
            }
        } catch (e) {
            MDUX_debugLog("[POST-ORTHO-TJ] Error restoring T-junction: " + e);
        }
    }

    MDUX_debugLog("[POST-ORTHO-TJ] Restored " + restoredCount + " of " + tJunctions.length + " T-junction(s)");
    return restoredCount;
}

// Apply orthogonalization using Python bridge
function MDUX_applyPythonOrtho(paths, snapThreshold) {
    if (!paths || paths.length === 0) {
        return { success: false, message: "No paths to orthogonalize" };
    }

    MDUX_debugLog("[V3-ORTHO] Starting Python orthogonalization for " + paths.length + " paths");

    // Check if Python bridge is available
    if (typeof PythonBridge === "undefined" || !PythonBridge.isAvailable()) {
        MDUX_debugLog("[V3-ORTHO] Python bridge not available, using fallback");
        return MDUX_applyExtendScriptOrtho(paths, snapThreshold);
    }

    try {
        // Store pre-ortho geometry for all paths
        for (var i = 0; i < paths.length; i++) {
            MDUX_storePreOrthoGeometry(paths[i]);
        }

        // *** DETECT T-JUNCTIONS BEFORE ORTHOGONALIZATION ***
        // Store endpoint-to-segment connections so we can restore them after ortho
        var preOrthoTJunctions = MDUX_detectTJunctions(paths);

        // Call Python orthogonalization
        var result = PythonBridge.orthogonalize(paths, snapThreshold || 5, []);

        if (result && result.paths && !result.error) {
            // Apply results back to Illustrator paths
            var appliedCount = 0;
            for (var j = 0; j < result.paths.length; j++) {
                var pyPath = result.paths[j];
                var targetPath = paths[pyPath.id];

                try {
                    var pts = targetPath.pathPoints;
                    var pyPts = pyPath.points;

                    if (pts.length === pyPts.length) {
                        for (var k = 0; k < pts.length; k++) {
                            var newX = pyPts[k].x;
                            var newY = pyPts[k].y;
                            pts[k].anchor = [newX, newY];
                            pts[k].leftDirection = [newX, newY];
                            pts[k].rightDirection = [newX, newY];
                        }
                        appliedCount++;
                    }
                } catch (e) {
                    MDUX_debugLog("[V3-ORTHO] Error applying to path " + j + ": " + e);
                }
            }

            // *** RESTORE T-JUNCTIONS AFTER ORTHOGONALIZATION ***
            // Snap endpoints back to segments to maintain connections
            var restoredTJ = MDUX_restoreTJunctions(preOrthoTJunctions);

            MDUX_debugLog("[V3-ORTHO] Applied orthogonalization to " + appliedCount + " paths");
            return {
                success: true,
                message: "Orthogonalized " + appliedCount + " path(s)" + (restoredTJ > 0 ? ", restored " + restoredTJ + " T-junction(s)" : ""),
                count: appliedCount,
                iterations: result.iterations || 0,
                snaps: result.total_snaps || 0,
                tJunctionsRestored: restoredTJ
            };
        } else {
            var errMsg = result && result.error ? result.error : "Unknown error";
            MDUX_debugLog("[V3-ORTHO] Python returned error: " + errMsg);
            return { success: false, message: "Python error: " + errMsg };
        }
    } catch (e) {
        MDUX_debugLog("[V3-ORTHO] Exception: " + e);
        return { success: false, message: "Error: " + e.message };
    }
}

// Fallback ExtendScript orthogonalization (slower)
function MDUX_applyExtendScriptOrtho(paths, snapThreshold) {
    var SNAP = snapThreshold || 5;
    var changed = 0;

    // Store pre-ortho geometry for all paths first
    for (var i = 0; i < paths.length; i++) {
        MDUX_storePreOrthoGeometry(paths[i]);
    }

    // *** DETECT T-JUNCTIONS BEFORE ORTHOGONALIZATION ***
    var preOrthoTJunctions = MDUX_detectTJunctions(paths);

    // Perform orthogonalization
    for (var i = 0; i < paths.length; i++) {
        var path = paths[i];
        try {
            var pts = path.pathPoints;
            if (!pts || pts.length < 2) continue;

            // Simple orthogonalization: snap segment angles to 0/90/180/270
            for (var j = 0; j < pts.length - 1; j++) {
                var p1 = pts[j].anchor;
                var p2 = pts[j + 1].anchor;
                var dx = p2[0] - p1[0];
                var dy = p2[1] - p1[1];
                var angle = Math.atan2(dy, dx) * 180 / Math.PI;

                // Snap to nearest 90 degree increment
                var snappedAngle = Math.round(angle / 90) * 90;
                var angleDiff = Math.abs(angle - snappedAngle);

                // Only snap if close enough (within steep threshold)
                if (angleDiff < 17 || angleDiff > 73) {
                    var len = Math.sqrt(dx * dx + dy * dy);
                    var newRad = snappedAngle * Math.PI / 180;
                    var newX = p1[0] + Math.cos(newRad) * len;
                    var newY = p1[1] + Math.sin(newRad) * len;
                    pts[j + 1].anchor = [newX, newY];
                    pts[j + 1].leftDirection = [newX, newY];
                    pts[j + 1].rightDirection = [newX, newY];
                }
            }
            changed++;
        } catch (e) {
            MDUX_debugLog("[ES-ORTHO] Error on path " + i + ": " + e);
        }
    }

    // *** RESTORE T-JUNCTIONS AFTER ORTHOGONALIZATION ***
    var restoredTJ = MDUX_restoreTJunctions(preOrthoTJunctions);

    return {
        success: true,
        message: "Orthogonalized " + changed + " path(s) (ExtendScript fallback)" + (restoredTJ > 0 ? ", restored " + restoredTJ + " T-junction(s)" : ""),
        count: changed,
        tJunctionsRestored: restoredTJ
    };
}

// ============================================
// V3 FULL FUNCTIONS - Process Ductwork Panel
// ============================================

function MDUX_rotateRegistersOnly() {
    MDUX_debugLog("[V3] MDUX_rotateRegistersOnly called");
    try {
        var doc = app.activeDocument;

        // Find register layers
        var registerLayers = ["Square Registers", "Rectangular Registers", "Circular Registers", "Exhaust Registers"];
        var rotatedCount = 0;

        for (var layerIdx = 0; layerIdx < registerLayers.length; layerIdx++) {
            var layerName = registerLayers[layerIdx];
            var layer = null;

            try {
                layer = doc.layers.getByName(layerName);
            } catch (e) {
                continue; // Layer doesn't exist
            }

            if (!layer) continue;

            // Process placed items (register graphics) on this layer
            try {
                for (var i = 0; i < layer.placedItems.length; i++) {
                    var item = layer.placedItems[i];
                    var meta = MDUX_getMetadata(item);

                    if (meta && typeof meta.targetRotation === "number") {
                        var currentRotation = meta.rotation || 0;
                        var targetRotation = meta.targetRotation;
                        var delta = targetRotation - currentRotation;

                        if (Math.abs(delta) > 0.1) {
                            var rotMatrix = app.getRotationMatrix(delta);
                            item.transform(rotMatrix, true, true, true, true, true);
                            meta.rotation = targetRotation;
                            MDUX_setMetadata(item, meta);
                            rotatedCount++;
                        }
                    }
                }
            } catch (e) {
                MDUX_debugLog("[ROTATE-REG] Error on layer " + layerName + ": " + e);
            }
        }

        return "Rotated " + rotatedCount + " register(s)";
    } catch (e) {
        MDUX_debugLog("[ROTATE-REG] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_carveForRegistersOnly() {
    MDUX_debugLog("[V3] MDUX_carveForRegistersOnly called");
    try {
        var doc = app.activeDocument;
        var REGISTER_CARVE_HALF_WIDTH = 13.5; // 27pt total gap
        var carvedCount = 0;

        // Get register positions from register layers
        var registerLayers = ["Square Registers", "Rectangular Registers", "Exhaust Registers", "Secondary Exhaust Registers"];
        var registerCenters = [];

        for (var layerIdx = 0; layerIdx < registerLayers.length; layerIdx++) {
            try {
                var layer = doc.layers.getByName(registerLayers[layerIdx]);
                for (var i = 0; i < layer.placedItems.length; i++) {
                    var item = layer.placedItems[i];
                    var bounds = item.geometricBounds;
                    var cx = (bounds[0] + bounds[2]) / 2;
                    var cy = (bounds[1] + bounds[3]) / 2;
                    registerCenters.push([cx, cy]);
                }
            } catch (e) {}
        }

        if (registerCenters.length === 0) {
            return "No registers found to carve around";
        }

        MDUX_debugLog("[CARVE-REG] Found " + registerCenters.length + " register center(s)");

        // Get selected ductwork paths
        var paths = MDUX_collectSelectedDuctworkPaths();
        if (paths.length === 0) {
            return "No ductwork paths selected";
        }

        // For each path, check if it passes through any register
        for (var pathIdx = 0; pathIdx < paths.length; pathIdx++) {
            var path = paths[pathIdx];
            try {
                var pts = path.pathPoints;
                if (!pts || pts.length < 2) continue;

                // Check each segment
                for (var segIdx = 0; segIdx < pts.length - 1; segIdx++) {
                    var p1 = pts[segIdx].anchor;
                    var p2 = pts[segIdx + 1].anchor;

                    for (var regIdx = 0; regIdx < registerCenters.length; regIdx++) {
                        var regCenter = registerCenters[regIdx];

                        // Check if register center is near segment
                        var dx = p2[0] - p1[0];
                        var dy = p2[1] - p1[1];
                        var segLen = Math.sqrt(dx * dx + dy * dy);
                        if (segLen < 1) continue;

                        // Project register center onto segment
                        var t = ((regCenter[0] - p1[0]) * dx + (regCenter[1] - p1[1]) * dy) / (segLen * segLen);

                        if (t > 0.05 && t < 0.95) {
                            var projX = p1[0] + t * dx;
                            var projY = p1[1] + t * dy;
                            var dist = Math.sqrt(Math.pow(regCenter[0] - projX, 2) + Math.pow(regCenter[1] - projY, 2));

                            if (dist < 50) { // Within threshold
                                // Mark this path as needing carve (actual carve happens in main processing)
                                MDUX_debugLog("[CARVE-REG] Path segment near register at [" + regCenter[0].toFixed(1) + "," + regCenter[1].toFixed(1) + "]");
                                carvedCount++;
                            }
                        }
                    }
                }
            } catch (e) {}
        }

        return "Found " + carvedCount + " segment(s) near registers (run full process to carve)";
    } catch (e) {
        MDUX_debugLog("[CARVE-REG] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_carveOverlapsOnly() {
    MDUX_debugLog("[V3] MDUX_carveOverlapsOnly called");
    try {
        var paths = MDUX_collectSelectedDuctworkPaths();
        if (paths.length === 0) {
            return "No ductwork paths selected";
        }

        // Use Python bridge to detect intersections
        if (typeof PythonBridge !== "undefined" && PythonBridge.isAvailable()) {
            var result = PythonBridge.detectIntersections(paths);

            if (result && result.intersections) {
                var intersectionCount = result.intersections.length;
                MDUX_debugLog("[CARVE-OVERLAP] Found " + intersectionCount + " intersection(s)");
                return "Found " + intersectionCount + " intersection(s) (run full process to carve)";
            }
        }

        return "Overlap detection requires Python bridge (run full process)";
    } catch (e) {
        MDUX_debugLog("[CARVE-OVERLAP] Error: " + e);
        return "Error: " + e.message;
    }
}

// ============================================
// V3 FULL FUNCTIONS - Orthogonalize Panel
// ============================================

// Track whether pre-ortho backup has been created for this document
// Uses document tag to persist across sessions
var PRE_ORTHO_BACKUP_TAG = "MD:PRE_ORTHO_BACKUP_CREATED";

function MDUX_hasPreOrthoBackup(doc) {
    try {
        var tags = doc.tags;
        for (var i = 0; i < tags.length; i++) {
            if (tags[i].name === PRE_ORTHO_BACKUP_TAG) {
                return true;
            }
        }
    } catch (e) {}
    return false;
}

function MDUX_setPreOrthoBackupCreated(doc) {
    try {
        var tag = doc.tags.add();
        tag.name = PRE_ORTHO_BACKUP_TAG;
        tag.value = new Date().toISOString();
    } catch (e) {
        MDUX_debugLog("[PRE-ORTHO-BACKUP] Error setting tag: " + e);
    }
}

function MDUX_createPreOrthoBackup() {
    MDUX_debugLog("[V3] MDUX_createPreOrthoBackup called");
    try {
        var doc = app.activeDocument;

        // Check if backup already exists for this document
        if (MDUX_hasPreOrthoBackup(doc)) {
            MDUX_debugLog("[PRE-ORTHO-BACKUP] Backup already created for this document");
            return { created: false, message: "Pre-Ortho backup already exists" };
        }

        // Get the current file path
        var docPath = null;
        try {
            docPath = doc.fullName;
        } catch (e) {
            // Document hasn't been saved yet
            MDUX_debugLog("[PRE-ORTHO-BACKUP] Document not saved - cannot create backup");
            return { created: false, message: "Save document first to enable Pre-Ortho backup" };
        }

        if (!docPath || !docPath.exists) {
            return { created: false, message: "Save document first to enable Pre-Ortho backup" };
        }

        // Create backup filename
        var originalPath = docPath.fsName;
        var ext = ".ai";
        var baseName = originalPath;

        // Remove extension
        if (originalPath.toLowerCase().indexOf(".ai") === originalPath.length - 3) {
            baseName = originalPath.substring(0, originalPath.length - 3);
            ext = ".ai";
        }

        var backupPath = baseName + " - Pre-Ortho" + ext;
        var backupFile = new File(backupPath);

        // Save a copy
        MDUX_debugLog("[PRE-ORTHO-BACKUP] Creating backup: " + backupPath);

        // Save current document to backup location
        var saveOptions = new IllustratorSaveOptions();
        saveOptions.compatibility = Compatibility.ILLUSTRATOR17; // CS3+
        saveOptions.flattenOutput = OutputFlattening.PRESERVEAPPEARANCE;
        saveOptions.pdfCompatible = true;

        doc.saveAs(backupFile, saveOptions);

        // Re-open original (saveAs changes the current document)
        var originalFile = new File(originalPath);
        app.open(originalFile);

        // Mark backup as created on the reopened document
        MDUX_setPreOrthoBackupCreated(app.activeDocument);

        MDUX_debugLog("[PRE-ORTHO-BACKUP] Backup created successfully");
        return { created: true, message: "Pre-Ortho backup created: " + backupFile.name };

    } catch (e) {
        MDUX_debugLog("[PRE-ORTHO-BACKUP] Error: " + e);
        return { created: false, message: "Backup error: " + e.message };
    }
}

// Wrapper function that creates backup if needed before orthogonalization
function MDUX_ensurePreOrthoBackupAndOrtho(orthoFunc) {
    try {
        var doc = app.activeDocument;

        // Create backup if not already done
        if (!MDUX_hasPreOrthoBackup(doc)) {
            var backupResult = MDUX_createPreOrthoBackup();
            if (backupResult.created) {
                MDUX_debugLog("[PRE-ORTHO] Created backup before orthogonalization");
            }
        }

        // Now run the orthogonalization
        return orthoFunc();
    } catch (e) {
        return "Error: " + e.message;
    }
}

function MDUX_orthoTrunkOnly() {
    MDUX_debugLog("[V3] MDUX_orthoTrunkOnly called");
    try {
        var allPaths = MDUX_collectSelectedDuctworkPaths();
        if (allPaths.length === 0) {
            return "No ductwork paths selected";
        }

        // Filter to trunk paths only
        var trunkPaths = MDUX_filterPathsByRole(allPaths, "trunk");

        if (trunkPaths.length === 0) {
            // No paths with trunk metadata - classify by connection pattern
            // Paths with endpoint-to-endpoint connections are trunks
            for (var i = 0; i < allPaths.length; i++) {
                var path = allPaths[i];
                var meta = MDUX_getMetadata(path);
                if (!meta || !meta.ductRole) {
                    // Default classification: paths with more than 2 points are likely trunk
                    if (path.pathPoints && path.pathPoints.length > 2) {
                        trunkPaths.push(path);
                    }
                }
            }
        }

        if (trunkPaths.length === 0) {
            return "No trunk paths found in selection";
        }

        MDUX_debugLog("[V3-ORTHO] Orthogonalizing " + trunkPaths.length + " trunk path(s)");
        var result = MDUX_applyPythonOrtho(trunkPaths, 5);
        return result.message;
    } catch (e) {
        MDUX_debugLog("[V3-ORTHO-TRUNK] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_orthoBranchesOnly() {
    MDUX_debugLog("[V3] MDUX_orthoBranchesOnly called");
    try {
        var allPaths = MDUX_collectSelectedDuctworkPaths();
        if (allPaths.length === 0) {
            return "No ductwork paths selected";
        }

        // Filter to branch paths only
        var branchPaths = MDUX_filterPathsByRole(allPaths, "branch");

        if (branchPaths.length === 0) {
            // No paths with branch metadata - classify by connection pattern
            // Open paths with 2-3 points that aren't marked as trunk are likely branches
            for (var i = 0; i < allPaths.length; i++) {
                var path = allPaths[i];
                if (!path.closed && path.pathPoints && path.pathPoints.length >= 2 && path.pathPoints.length <= 4) {
                    var role = MDUX_getDuctRoleForPath(path);
                    if (!role) {
                        branchPaths.push(path);
                    }
                }
            }
        }

        if (branchPaths.length === 0) {
            return "No branch paths found in selection";
        }

        MDUX_debugLog("[V3-ORTHO] Orthogonalizing " + branchPaths.length + " branch path(s)");
        var result = MDUX_applyPythonOrtho(branchPaths, 5);
        return result.message;
    } catch (e) {
        MDUX_debugLog("[V3-ORTHO-BRANCH] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_orthoFinalOnly() {
    MDUX_debugLog("[V3] MDUX_orthoFinalOnly called");
    try {
        var allPaths = MDUX_collectSelectedDuctworkPaths();
        if (allPaths.length === 0) {
            return "No ductwork paths selected";
        }

        var doc = app.activeDocument;

        // Get register positions to identify final segments
        var registerLayers = ["Square Registers", "Rectangular Registers", "Circular Registers", "Exhaust Registers", "Secondary Exhaust Registers"];
        var registerPositions = [];

        for (var layerIdx = 0; layerIdx < registerLayers.length; layerIdx++) {
            try {
                var layer = doc.layers.getByName(registerLayers[layerIdx]);
                for (var i = 0; i < layer.placedItems.length; i++) {
                    var item = layer.placedItems[i];
                    var bounds = item.geometricBounds;
                    registerPositions.push([(bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2]);
                }
            } catch (e) {}
        }

        // Final segments are paths that have an endpoint near a register
        var REGISTER_THRESHOLD = 50; // 50pt threshold
        var finalPaths = [];

        for (var pathIdx = 0; pathIdx < allPaths.length; pathIdx++) {
            var path = allPaths[pathIdx];
            try {
                var pts = path.pathPoints;
                if (!pts || pts.length < 2) continue;

                var firstPt = pts[0].anchor;
                var lastPt = pts[pts.length - 1].anchor;

                // Check if either endpoint is near a register
                for (var regIdx = 0; regIdx < registerPositions.length; regIdx++) {
                    var regPos = registerPositions[regIdx];
                    var d1 = Math.sqrt(Math.pow(firstPt[0] - regPos[0], 2) + Math.pow(firstPt[1] - regPos[1], 2));
                    var d2 = Math.sqrt(Math.pow(lastPt[0] - regPos[0], 2) + Math.pow(lastPt[1] - regPos[1], 2));

                    if (d1 < REGISTER_THRESHOLD || d2 < REGISTER_THRESHOLD) {
                        finalPaths.push(path);
                        break;
                    }
                }
            } catch (e) {}
        }

        if (finalPaths.length === 0) {
            return "No final segments found (paths connecting to registers)";
        }

        MDUX_debugLog("[V3-ORTHO] Orthogonalizing " + finalPaths.length + " final segment(s)");
        var result = MDUX_applyPythonOrtho(finalPaths, 5);
        return result.message;
    } catch (e) {
        MDUX_debugLog("[V3-ORTHO-FINAL] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_orthoAll() {
    MDUX_debugLog("[V3] MDUX_orthoAll called");
    try {
        var allPaths = MDUX_collectSelectedDuctworkPaths();
        if (allPaths.length === 0) {
            return "No ductwork paths selected";
        }

        MDUX_debugLog("[V3-ORTHO] Orthogonalizing all " + allPaths.length + " path(s)");
        var result = MDUX_applyPythonOrtho(allPaths, 5);
        return result.message;
    } catch (e) {
        MDUX_debugLog("[V3-ORTHO-ALL] Error: " + e);
        return "Error: " + e.message;
    }
}

// ============================================
// V3 FULL FUNCTIONS - Ductwork Parts Panel
// ============================================

// Path to ductwork component assets
var DUCTWORK_ASSETS_PATH = "E:/Work/Work/Floorplans/Ductwork Assets/";

// Ductwork color to register layer mapping
var V3_DUCTWORK_COLOR_TO_REGISTER = {
    "Blue Ductwork": "Square Registers",
    "Green Ductwork": "Exhaust Registers",
    "Light Green Ductwork": "Exhaust Registers",
    "Orange Ductwork": "Exhaust Registers",
    "Light Orange Ductwork": "Exhaust Registers",
    "Thermostat Lines": "Thermostats"
};

// Layer to component file mapping - covers ALL ductwork parts layers
var V3_REGISTER_TO_FILE = {
    "Thermostats": "Thermostat.ai",
    "Units": "Unit.ai",
    "Secondary Exhaust Registers": "Secondary Exhaust Register.ai",
    "Exhaust Registers": "Exhaust Register.ai",
    "Orange Register": "Orange Register.ai",
    "Rectangular Registers": "Rectangular Register.ai",
    "Square Registers": "Square Register.ai",
    "Circular Registers": "Circular Register.ai"
};

// Helper: Get or create a layer by name
function V3_getOrCreateLayer(doc, layerName) {
    try {
        return doc.layers.getByName(layerName);
    } catch (e) {
        var newLayer = doc.layers.add();
        newLayer.name = layerName;
        return newLayer;
    }
}

// Helper: Check if a point already has an anchor nearby
function V3_isPointAlreadyPlaced(pt, existingPoints, tolerance) {
    tolerance = tolerance || 3;
    for (var i = 0; i < existingPoints.length; i++) {
        var ex = existingPoints[i];
        var dx = pt[0] - ex[0];
        var dy = pt[1] - ex[1];
        if (Math.sqrt(dx * dx + dy * dy) < tolerance) return true;
    }
    return false;
}

// Helper: Create an anchor point with rotation metadata
function V3_createAnchorPoint(layer, position, rotationDeg) {
    try {
        var newPath = layer.pathItems.add();
        var p = newPath.pathPoints.add();
        p.anchor = position;
        p.leftDirection = position;
        p.rightDirection = position;
        p.pointType = PointType.CORNER;
        newPath.stroked = false;
        newPath.filled = false;

        // Store rotation in note
        if (typeof rotationDeg === 'number' && isFinite(rotationDeg)) {
            newPath.note = "MD:POINT_ROT=" + rotationDeg.toFixed(2);
        }
        return newPath;
    } catch (e) {
        MDUX_debugLog("[V3-ANCHOR] Error creating anchor: " + e);
        return null;
    }
}

// Helper: Get rotation from anchor point note
function V3_getAnchorRotation(anchorPath) {
    try {
        var note = anchorPath.note || "";
        var match = note.match(/MD:POINT_ROT=([\-0-9.]+)/);
        if (match) return parseFloat(match[1]);
    } catch (e) {}
    return 0;
}

// Helper: Compute segment angle at a point
function V3_computeSegmentAngle(prevPt, currentPt, nextPt) {
    // Use incoming direction if available, else outgoing
    var dx, dy;
    if (prevPt) {
        dx = currentPt[0] - prevPt[0];
        dy = currentPt[1] - prevPt[1];
    } else if (nextPt) {
        dx = nextPt[0] - currentPt[0];
        dy = nextPt[1] - currentPt[1];
    } else {
        return 0;
    }
    return Math.atan2(dy, dx) * (180 / Math.PI);
}

// Helper: Normalize angle to 0-360 range
function V3_normalizeAngle(angleDeg) {
    while (angleDeg < 0) angleDeg += 360;
    while (angleDeg >= 360) angleDeg -= 360;
    return angleDeg;
}

function MDUX_createUpdateDuctworkParts() {
    MDUX_debugLog("[V3] MDUX_createUpdateDuctworkParts called");
    try {
        var doc = app.activeDocument;
        var paths = MDUX_collectSelectedDuctworkPaths();

        if (paths.length === 0) {
            return "No ductwork paths selected";
        }

        MDUX_debugLog("[V3] Processing " + paths.length + " selected ductwork path(s)");

        var DEFAULT_SCALE = 50;
        var anchorCount = 0;
        var graphicCount = 0;
        var existingPositions = [];

        // Group paths by their ductwork color layer
        var pathsByColor = {};
        for (var i = 0; i < paths.length; i++) {
            var path = paths[i];
            var layerName = "";
            try { layerName = path.layer.name; } catch (e) { continue; }

            if (!pathsByColor[layerName]) {
                pathsByColor[layerName] = [];
            }
            pathsByColor[layerName].push(path);
        }

        // Process each ductwork color group
        for (var colorLayer in pathsByColor) {
            if (!pathsByColor.hasOwnProperty(colorLayer)) continue;

            var colorPaths = pathsByColor[colorLayer];
            var registerLayerName = V3_DUCTWORK_COLOR_TO_REGISTER[colorLayer];

            if (!registerLayerName) {
                MDUX_debugLog("[V3] Skipping unknown ductwork color: " + colorLayer);
                continue;
            }

            var componentFileName = V3_REGISTER_TO_FILE[registerLayerName];
            var componentFile = new File(DUCTWORK_ASSETS_PATH + componentFileName);

            if (!componentFile.exists) {
                MDUX_debugLog("[V3] WARNING: Component file not found: " + componentFile.fsName);
                continue;
            }

            var registerLayer = V3_getOrCreateLayer(doc, registerLayerName);

            MDUX_debugLog("[V3] Processing " + colorPaths.length + " paths on " + colorLayer + " -> " + registerLayerName);

            // Collect all endpoints from this color's paths
            for (var pathIdx = 0; pathIdx < colorPaths.length; pathIdx++) {
                var path = colorPaths[pathIdx];
                try {
                    var pts = path.pathPoints;
                    if (!pts || pts.length < 2) continue;

                    // Process start endpoint
                    var startPt = pts[0].anchor;
                    var nextPt = pts[1].anchor;
                    var startAngle = V3_computeSegmentAngle(null, startPt, nextPt);

                    if (!V3_isPointAlreadyPlaced(startPt, existingPositions, 3)) {
                        // Create anchor
                        V3_createAnchorPoint(registerLayer, startPt, startAngle);
                        anchorCount++;

                        // Place linked graphic
                        try {
                            var placed = registerLayer.placedItems.add();
                            placed.file = componentFile;
                            try { placed.relink(componentFile); } catch (eRelink) {}
                            try { placed.update(); } catch (eUpdate) {}

                            // Center on anchor position
                            var bounds = placed.geometricBounds;
                            var w = bounds[2] - bounds[0];
                            var h = bounds[1] - bounds[3];
                            placed.position = [startPt[0] - w / 2, startPt[1] + h / 2];
                            placed.name = registerLayerName.replace(" Registers", " Register") + " (Linked)";

                            // Apply default 50% scale
                            placed.resize(DEFAULT_SCALE, DEFAULT_SCALE, true, true, true, true, DEFAULT_SCALE, Transformation.CENTER);

                            // Apply rotation to match ductwork angle
                            placed.rotate(startAngle, true, true, true, true, Transformation.CENTER);

                            // Re-center after scaling
                            bounds = placed.geometricBounds;
                            var cx = (bounds[0] + bounds[2]) / 2;
                            var cy = (bounds[1] + bounds[3]) / 2;
                            var dx = startPt[0] - cx;
                            var dy = startPt[1] - cy;
                            if (Math.abs(dx) > 0.01 || Math.abs(dy) > 0.01) {
                                placed.translate(dx, dy, true, true, true, true);
                            }

                            // Store metadata
                            var meta = {
                                MDUX_RotationOverride: V3_normalizeAngle(startAngle),
                                MDUX_CumulativeRotation: String(V3_normalizeAngle(startAngle))
                            };
                            MDUX_setMetadata(placed, meta);

                            graphicCount++;
                        } catch (ePlaceStart) {
                            MDUX_debugLog("[V3] Error placing start graphic: " + ePlaceStart);
                        }

                        existingPositions.push(startPt);
                    }

                    // Process end endpoint
                    var endPt = pts[pts.length - 1].anchor;
                    var prevPt = pts[pts.length - 2].anchor;
                    var endAngle = V3_computeSegmentAngle(prevPt, endPt, null);

                    if (!V3_isPointAlreadyPlaced(endPt, existingPositions, 3)) {
                        // Create anchor
                        V3_createAnchorPoint(registerLayer, endPt, endAngle);
                        anchorCount++;

                        // Place linked graphic
                        try {
                            var placedEnd = registerLayer.placedItems.add();
                            placedEnd.file = componentFile;
                            try { placedEnd.relink(componentFile); } catch (eRelink2) {}
                            try { placedEnd.update(); } catch (eUpdate2) {}

                            // Center on anchor position
                            var boundsEnd = placedEnd.geometricBounds;
                            var wEnd = boundsEnd[2] - boundsEnd[0];
                            var hEnd = boundsEnd[1] - boundsEnd[3];
                            placedEnd.position = [endPt[0] - wEnd / 2, endPt[1] + hEnd / 2];
                            placedEnd.name = registerLayerName.replace(" Registers", " Register") + " (Linked)";

                            // Apply default 50% scale
                            placedEnd.resize(DEFAULT_SCALE, DEFAULT_SCALE, true, true, true, true, DEFAULT_SCALE, Transformation.CENTER);

                            // Apply rotation to match ductwork angle
                            placedEnd.rotate(endAngle, true, true, true, true, Transformation.CENTER);

                            // Re-center after scaling
                            boundsEnd = placedEnd.geometricBounds;
                            var cxEnd = (boundsEnd[0] + boundsEnd[2]) / 2;
                            var cyEnd = (boundsEnd[1] + boundsEnd[3]) / 2;
                            var dxEnd = endPt[0] - cxEnd;
                            var dyEnd = endPt[1] - cyEnd;
                            if (Math.abs(dxEnd) > 0.01 || Math.abs(dyEnd) > 0.01) {
                                placedEnd.translate(dxEnd, dyEnd, true, true, true, true);
                            }

                            // Store metadata
                            var metaEnd = {
                                MDUX_RotationOverride: V3_normalizeAngle(endAngle),
                                MDUX_CumulativeRotation: String(V3_normalizeAngle(endAngle))
                            };
                            MDUX_setMetadata(placedEnd, metaEnd);

                            graphicCount++;
                        } catch (ePlaceEnd) {
                            MDUX_debugLog("[V3] Error placing end graphic: " + ePlaceEnd);
                        }

                        existingPositions.push(endPt);
                    }

                    // Process internal anchors (collinear points)
                    for (var ptIdx = 1; ptIdx < pts.length - 1; ptIdx++) {
                        var internalPt = pts[ptIdx].anchor;
                        var prevInternal = pts[ptIdx - 1].anchor;
                        var nextInternal = pts[ptIdx + 1].anchor;

                        // Skip if already placed
                        if (V3_isPointAlreadyPlaced(internalPt, existingPositions, 3)) continue;

                        var internalAngle = V3_computeSegmentAngle(prevInternal, internalPt, null);

                        // Create anchor
                        V3_createAnchorPoint(registerLayer, internalPt, internalAngle);
                        anchorCount++;

                        // Place linked graphic
                        try {
                            var placedInt = registerLayer.placedItems.add();
                            placedInt.file = componentFile;
                            try { placedInt.relink(componentFile); } catch (eRelink3) {}
                            try { placedInt.update(); } catch (eUpdate3) {}

                            var boundsInt = placedInt.geometricBounds;
                            var wInt = boundsInt[2] - boundsInt[0];
                            var hInt = boundsInt[1] - boundsInt[3];
                            placedInt.position = [internalPt[0] - wInt / 2, internalPt[1] + hInt / 2];
                            placedInt.name = registerLayerName.replace(" Registers", " Register") + " (Linked)";

                            placedInt.resize(DEFAULT_SCALE, DEFAULT_SCALE, true, true, true, true, DEFAULT_SCALE, Transformation.CENTER);
                            placedInt.rotate(internalAngle, true, true, true, true, Transformation.CENTER);

                            boundsInt = placedInt.geometricBounds;
                            var cxInt = (boundsInt[0] + boundsInt[2]) / 2;
                            var cyInt = (boundsInt[1] + boundsInt[3]) / 2;
                            var dxInt = internalPt[0] - cxInt;
                            var dyInt = internalPt[1] - cyInt;
                            if (Math.abs(dxInt) > 0.01 || Math.abs(dyInt) > 0.01) {
                                placedInt.translate(dxInt, dyInt, true, true, true, true);
                            }

                            var metaInt = {
                                MDUX_RotationOverride: V3_normalizeAngle(internalAngle),
                                MDUX_CumulativeRotation: String(V3_normalizeAngle(internalAngle))
                            };
                            MDUX_setMetadata(placedInt, metaInt);

                            graphicCount++;
                        } catch (ePlaceInt) {
                            MDUX_debugLog("[V3] Error placing internal graphic: " + ePlaceInt);
                        }

                        existingPositions.push(internalPt);
                    }
                } catch (ePath) {
                    MDUX_debugLog("[V3] Error processing path: " + ePath);
                }
            }
        }

        return "Created " + anchorCount + " anchor(s) and " + graphicCount + " graphic(s)";
    } catch (e) {
        MDUX_debugLog("[CREATE-PARTS] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_createPartAnchorsOnly() {
    MDUX_debugLog("[V3] MDUX_createPartAnchorsOnly called");
    try {
        var doc = app.activeDocument;
        var paths = MDUX_collectSelectedDuctworkPaths();

        if (paths.length === 0) {
            return "No ductwork paths selected";
        }

        MDUX_debugLog("[V3] Creating anchors only for " + paths.length + " path(s)");

        var anchorCount = 0;
        var existingPositions = [];

        // Group paths by their ductwork color layer
        var pathsByColor = {};
        for (var i = 0; i < paths.length; i++) {
            var path = paths[i];
            var layerName = "";
            try { layerName = path.layer.name; } catch (e) { continue; }

            if (!pathsByColor[layerName]) {
                pathsByColor[layerName] = [];
            }
            pathsByColor[layerName].push(path);
        }

        // Process each ductwork color group
        for (var colorLayer in pathsByColor) {
            if (!pathsByColor.hasOwnProperty(colorLayer)) continue;

            var colorPaths = pathsByColor[colorLayer];
            var registerLayerName = V3_DUCTWORK_COLOR_TO_REGISTER[colorLayer];

            if (!registerLayerName) {
                MDUX_debugLog("[V3] Skipping unknown ductwork color: " + colorLayer);
                continue;
            }

            var registerLayer = V3_getOrCreateLayer(doc, registerLayerName);

            for (var pathIdx = 0; pathIdx < colorPaths.length; pathIdx++) {
                var path = colorPaths[pathIdx];
                try {
                    var pts = path.pathPoints;
                    if (!pts || pts.length < 2) continue;

                    // Process all anchor points
                    for (var ptIdx = 0; ptIdx < pts.length; ptIdx++) {
                        var pt = pts[ptIdx].anchor;

                        if (V3_isPointAlreadyPlaced(pt, existingPositions, 3)) continue;

                        var prevPt = ptIdx > 0 ? pts[ptIdx - 1].anchor : null;
                        var nextPt = ptIdx < pts.length - 1 ? pts[ptIdx + 1].anchor : null;
                        var angle = V3_computeSegmentAngle(prevPt, pt, nextPt);

                        V3_createAnchorPoint(registerLayer, pt, angle);
                        anchorCount++;
                        existingPositions.push(pt);
                    }
                } catch (ePath) {
                    MDUX_debugLog("[V3] Error processing path: " + ePath);
                }
            }
        }

        return "Created " + anchorCount + " anchor point(s)";
    } catch (e) {
        MDUX_debugLog("[CREATE-ANCHORS] Error: " + e);
        return "Error: " + e.message;
    }
}

function MDUX_selectDuctworkParts() {
    MDUX_debugLog("[V3] MDUX_selectDuctworkParts called");
    try {
        var doc = app.activeDocument;
        doc.selection = null;

        // Build lookup hash for target layers
        var partsLayerLookup = {};
        for (var k = 0; k < DUCTWORK_PARTS.length; k++) {
            partsLayerLookup[DUCTWORK_PARTS[k]] = true;
        }

        // Iterate through all placed items and select those on target layers
        var placed = doc.placedItems;
        var len = placed.length;
        var count = 0;

        for (var i = 0; i < len; i++) {
            var item = placed[i];
            if (partsLayerLookup[item.layer.name]) {
                item.selected = true;
                count++;
            }
        }

        return count > 0 ? "Selected " + count + " ductwork part(s)" : "No ductwork parts found";
    } catch (e) {
        return "Error: " + e.message;
    }
}

function MDUX_selectDuctworkAnchors() {
    MDUX_debugLog("[V3] MDUX_selectDuctworkAnchors called");
    try {
        var doc = app.activeDocument;
        doc.selection = null;

        // Build lookup hash for target layers
        var partsLayerLookup = {};
        for (var k = 0; k < DUCTWORK_PARTS.length; k++) {
            partsLayerLookup[DUCTWORK_PARTS[k]] = true;
        }

        // Iterate through all path items and select anchors on target layers
        var paths = doc.pathItems;
        var len = paths.length;
        var count = 0;

        for (var i = 0; i < len; i++) {
            var item = paths[i];
            // Anchors are single-point paths with no stroke/fill on ductwork layers
            if (partsLayerLookup[item.layer.name] &&
                !item.stroked && !item.filled &&
                item.pathPoints.length === 1) {
                item.selected = true;
                count++;
            }
        }

        return count > 0 ? "Selected " + count + " anchor(s)" : "No ductwork anchors found";
    } catch (e) {
        return "Error: " + e.message;
    }
}

function MDUX_deleteSelectedAnchors() {
    MDUX_debugLog("[V3] MDUX_deleteSelectedAnchors called");
    try {
        var doc = app.activeDocument;
        var sel = doc.selection;
        if (!sel || sel.length === 0) return "No selection";

        var deletedCount = 0;

        for (var i = sel.length - 1; i >= 0; i--) {
            try {
                var item = sel[i];
                // Delete single-point paths (anchors)
                if (item.typename === "PathItem" && item.pathPoints && item.pathPoints.length === 1) {
                    if (!item.stroked && !item.filled) {
                        item.remove();
                        deletedCount++;
                    }
                }
            } catch (e) {}
        }

        return "Deleted " + deletedCount + " anchor(s)";
    } catch (e) {
        return "Error: " + e.message;
    }
}

function MDUX_placeDuctworkPartGraphics() {
    MDUX_debugLog("[V3] MDUX_placeDuctworkPartGraphics called");
    try {
        var doc = app.activeDocument;
        var placedCount = 0;

        // Search all ductwork parts layers for anchors without nearby graphics
        for (var layerIdx = 0; layerIdx < DUCTWORK_PARTS.length; layerIdx++) {
            var layerName = DUCTWORK_PARTS[layerIdx];

            // Determine component file for this layer
            var componentFileName = V3_REGISTER_TO_FILE[layerName];
            if (!componentFileName) continue;

            var componentFile = new File(DUCTWORK_ASSETS_PATH + componentFileName);
            if (!componentFile.exists) {
                MDUX_debugLog("[V3] Component file not found: " + componentFile.fsName);
                continue;
            }

            try {
                var layer = doc.layers.getByName(layerName);
                if (!layer) continue;

                // Collect existing graphic positions
                var existingGraphicPositions = [];
                for (var g = 0; g < layer.placedItems.length; g++) {
                    var graphic = layer.placedItems[g];
                    try {
                        var gBounds = graphic.geometricBounds;
                        var gCx = (gBounds[0] + gBounds[2]) / 2;
                        var gCy = (gBounds[1] + gBounds[3]) / 2;
                        existingGraphicPositions.push([gCx, gCy]);
                    } catch (e) {}
                }

                // Find anchors without graphics
                for (var p = 0; p < layer.pathItems.length; p++) {
                    var path = layer.pathItems[p];
                    try {
                        if (path.pathPoints.length !== 1 || path.stroked || path.filled) continue;

                        var anchorPt = path.pathPoints[0].anchor;

                        // Check if graphic already exists at this position
                        if (V3_isPointAlreadyPlaced(anchorPt, existingGraphicPositions, 5)) continue;

                        // Place new graphic at 100% size, no rotation
                        var placed = layer.placedItems.add();
                        placed.file = componentFile;
                        try { placed.relink(componentFile); } catch (eRelink) {}
                        try { placed.update(); } catch (eUpdate) {}

                        // Center the placed item on the anchor point
                        var bounds = placed.geometricBounds;
                        var w = bounds[2] - bounds[0];
                        var h = bounds[1] - bounds[3];
                        placed.position = [anchorPt[0] - w / 2, anchorPt[1] + h / 2];
                        placed.name = layerName.replace(" Registers", " Register") + " (Linked)";

                        // No resize (100% size) and no rotation

                        // Store metadata with 0 rotation
                        var meta = {
                            MDUX_RotationOverride: 0,
                            MDUX_CumulativeRotation: "0"
                        };
                        MDUX_setMetadata(placed, meta);

                        placedCount++;
                    } catch (ePlacePath) {
                        MDUX_debugLog("[V3] Error placing graphic: " + ePlacePath);
                    }
                }
            } catch (eLayer) {
                MDUX_debugLog("[V3] Error processing layer " + layerName + ": " + eLayer);
            }
        }

        if (placedCount === 0) {
            return "No anchors need graphics (all anchors already have graphics nearby)";
        }

        return "Placed " + placedCount + " graphic(s) at anchor positions";
    } catch (e) {
        MDUX_debugLog("[PLACE-GRAPHICS] Error: " + e);
        return "Error: " + e.message;
    }
}

// ============================================
// V3 FULL FUNCTIONS - Transform Panel
// ============================================

function MDUX_resetDuctworkPartsRotation() {
    MDUX_debugLog("[V3] MDUX_resetDuctworkPartsRotation called");
    try {
        var doc = app.activeDocument;
        var sel = doc.selection;
        if (!sel || sel.length === 0) return "No selection";

        MDUX_debugLog("[RESET-ROT] Selection has " + sel.length + " items");
        var resetCount = 0;
        var skippedNotPart = 0;
        var skippedNoRotation = 0;

        for (var i = 0; i < sel.length; i++) {
            try {
                var item = sel[i];
                var layerName = "unknown";
                try { layerName = item.layer ? item.layer.name : "no layer"; } catch (e) {}

                // Check if it's a ductwork part (on any ductwork parts layer)
                var isPart = MDUX_isDuctworkPart(item);
                MDUX_debugLog("[RESET-ROT] Item " + i + " layer='" + layerName + "' isDuctworkPart=" + isPart);

                if (!isPart) {
                    skippedNotPart++;
                    continue;
                }

                var meta = MDUX_getMetadata(item);
                MDUX_debugLog("[RESET-ROT] Item " + i + " meta=" + (meta ? JSON.stringify(meta) : "null"));

                // Also check for MD:PLACED_ROT tag in the note
                var placedRot = 0;
                var note = "";
                try { note = item.note || ""; } catch (e) {}
                var placedRotMatch = note.match(/MD:PLACED_ROT=([0-9.\-]+)/);
                if (placedRotMatch) {
                    placedRot = parseFloat(placedRotMatch[1]) || 0;
                }

                // Get cumulative rotation from metadata - check multiple possible sources
                var cumRot = 0;
                if (meta && meta.MDUX_CumulativeRotation !== undefined) {
                    cumRot = parseFloat(meta.MDUX_CumulativeRotation) || 0;
                } else if (meta && meta.MDUX_RotationOverride !== undefined) {
                    // Old format - rotation override
                    cumRot = parseFloat(meta.MDUX_RotationOverride) || 0;
                } else if (meta && typeof meta.rotation === "number") {
                    cumRot = meta.rotation;
                } else if (meta && meta.tagRotation !== undefined) {
                    cumRot = parseFloat(meta.tagRotation) || 0;
                } else if (placedRot !== 0) {
                    // Fall back to MD:PLACED_ROT from magic-final.jsx
                    cumRot = placedRot;
                }

                MDUX_debugLog("[RESET-ROT] Item " + i + " cumRot=" + cumRot + " (placedRot=" + placedRot + ")");

                if (Math.abs(cumRot) > 0.001) {
                    // Capture center position before rotation
                    var bounds = item.geometricBounds;
                    var centerX = (bounds[0] + bounds[2]) / 2;
                    var centerY = (bounds[1] + bounds[3]) / 2;

                    // Rotate back to 0 around center
                    item.rotate(-cumRot, true, true, true, true, Transformation.CENTER);

                    // Verify center stayed in place, translate back if needed
                    var newBounds = item.geometricBounds;
                    var newCenterX = (newBounds[0] + newBounds[2]) / 2;
                    var newCenterY = (newBounds[1] + newBounds[3]) / 2;
                    if (Math.abs(newCenterX - centerX) > 0.1 || Math.abs(newCenterY - centerY) > 0.1) {
                        item.translate(centerX - newCenterX, centerY - newCenterY);
                    }

                    // Update metadata - clear all rotation-related fields
                    if (!meta) meta = {};
                    meta.MDUX_CumulativeRotation = "0";
                    meta.MDUX_RotationOverride = 0;
                    meta.tagRotation = 0;
                    if (typeof meta.rotation === "number") {
                        meta.rotation = 0;
                    }
                    MDUX_setMetadata(item, meta);

                    // Also clear MD:PLACED_ROT in the note
                    try {
                        var currentNote = item.note || "";
                        currentNote = currentNote.replace(/MD:PLACED_ROT=[0-9.\-]+/g, "MD:PLACED_ROT=0");
                        item.note = currentNote;
                    } catch (e) {}

                    // Relink PlacedItem to refresh bounding box (like rotation slider does)
                    try {
                        if (item.typename === "PlacedItem" && item.file) {
                            var linkedFile = item.file;
                            if (linkedFile && linkedFile.exists) {
                                // Save note before relink - relink wipes metadata!
                                var savedNote = item.note || "";
                                MDUX_debugLog("[RESET-ROT] Saved note before relink: " + savedNote.substring(0, 80));

                                // Three-step refresh like rotation slider: file assignment + relink + update
                                item.file = linkedFile;
                                try { item.relink(linkedFile); } catch (eRl) { }
                                try { item.update(); } catch (eUp) { }

                                // Restore the note after relink
                                if (savedNote) {
                                    item.note = savedNote;
                                    MDUX_debugLog("[RESET-ROT] Restored note after relink");
                                }
                                MDUX_debugLog("[RESET-ROT] Refreshed link (file+relink+update) to fix bounding box");
                            }
                        }
                    } catch (eRelink) {
                        MDUX_debugLog("[RESET-ROT] Relink error (non-fatal): " + eRelink);
                    }

                    resetCount++;

                    MDUX_debugLog("[RESET-ROT] Reset item " + i + " from " + cumRot + " to 0");
                } else {
                    skippedNoRotation++;
                    MDUX_debugLog("[RESET-ROT] Item " + i + " skipped - no rotation to reset (cumRot=" + cumRot + ")");
                }
            } catch (e) {
                MDUX_debugLog("[RESET-ROT] Error on item " + i + ": " + e);
            }
        }

        MDUX_debugLog("[RESET-ROT] Summary: reset=" + resetCount + ", skippedNotPart=" + skippedNotPart + ", skippedNoRotation=" + skippedNoRotation);

        // Return detailed message
        if (resetCount === 0 && skippedNotPart > 0) {
            return "No ductwork parts in selection (skipped " + skippedNotPart + " non-part items)";
        } else if (resetCount === 0 && skippedNoRotation > 0) {
            return "Parts have no rotation metadata - use rotation slider first";
        }
        return "Reset rotation on " + resetCount + " part(s)";
    } catch (e) {
        return "Error: " + e.message;
    }
}

