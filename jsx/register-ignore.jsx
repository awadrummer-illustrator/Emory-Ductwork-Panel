if (typeof MDUX === "undefined") {
    var MDUX = {};
}

MDUX.ignoreLayerNames = ["Ignore", "Ignored", "ignore", "ignored"];

function MDUX_log(msg) {
    try {
        $.writeln("[Magic-Ductwork] " + msg);
    } catch (e) {}
}

function MDUX_isEligibleIgnorePath(pathItem) {
    if (!pathItem) return false;
    try {
        if (!(pathItem instanceof PathItem)) return false;
    } catch (e) {
        try {
            if (pathItem.typename !== "PathItem") return false;
        } catch (e2) {
            return false;
        }
    }
    if (pathItem.closed) return false;
    try {
        if (pathItem.pathPoints.length < 2) return false;
    } catch (eLen) {
        return false;
    }
    try {
        if (pathItem.guides) return false;
    } catch (eGuides) {}
    try {
        if (pathItem.layer && pathItem.layer.name === "__MDUX_RegisterPreview") return false;
    } catch (eLayerName) {}
    try {
        if (pathItem.locked) return false;
    } catch (eLocked) {}
    try {
        if (pathItem.hidden) return false;
    } catch (eHidden) {}
    try {
        if (pathItem.layer) {
            if (pathItem.layer.locked) return false;
            if (pathItem.layer.visible === false) return false;
        }
    } catch (eLayerState) {}
    return true;
}

function MDUX_collectOpenPathsFromItem(item, results, seen) {
    if (!item) return;
    if (!results) results = [];
    if (!seen) seen = {};

    var key = null;
    try {
        if (item.uuid) key = "uuid:" + item.uuid;
    } catch (eUuid) {
        key = null;
    }
    if (!key) {
        key = "ref:" + item;
    }
    if (seen[key]) return;
    seen[key] = true;

    try {
        if (item instanceof PathItem) {
            if (MDUX_isEligibleIgnorePath(item)) results.push(item);
            return;
        }
    } catch (eInstance) {
        try {
            if (item.typename === "PathItem" && MDUX_isEligibleIgnorePath(item)) {
                results.push(item);
                return;
            }
        } catch (eTypename) {}
    }

    var typename = "";
    try { typename = item.typename; } catch (eType) { typename = ""; }

    if (typename === "CompoundPathItem") {
        try {
            for (var ci = 0; ci < item.pathItems.length; ci++) {
                MDUX_collectOpenPathsFromItem(item.pathItems[ci], results, seen);
            }
        } catch (eCompound) {}
        return;
    }

    if (item.pageItems) {
        try {
            for (var pi = 0; pi < item.pageItems.length; pi++) {
                MDUX_collectOpenPathsFromItem(item.pageItems[pi], results, seen);
            }
        } catch (ePage) {}
    }

    if (item.pathItems && !item.pageItems) {
        try {
            for (var pi2 = 0; pi2 < item.pathItems.length; pi2++) {
                MDUX_collectOpenPathsFromItem(item.pathItems[pi2], results, seen);
            }
        } catch (ePaths) {}
    }

    if (item.layers) {
        try {
            for (var li = 0; li < item.layers.length; li++) {
                MDUX_collectOpenPathsFromItem(item.layers[li], results, seen);
            }
        } catch (eLayers) {}
    }

    if (item.groupItems && !item.pageItems) {
        try {
            for (var gi = 0; gi < item.groupItems.length; gi++) {
                MDUX_collectOpenPathsFromItem(item.groupItems[gi], results, seen);
            }
        } catch (eGroups) {}
    }
}

function MDUX_collectSelectedOpenPaths(doc) {
    var results = [];
    if (!doc) return results;
    var sel = null;
    try {
        sel = doc.selection;
    } catch (eSel) {
        sel = null;
    }
    if (!sel) return results;

    if (sel.length === undefined && sel.typename) {
        MDUX_collectOpenPathsFromItem(sel, results, {});
        return results;
    }
    for (var i = 0; i < sel.length; i++) {
        MDUX_collectOpenPathsFromItem(sel[i], results, {});
    }
    return results;
}

function MDUX_getIgnoreLayer(doc, createIfMissing) {
    if (!doc) return null;
    var layer = null;
    try {
        for (var i = 0; i < doc.layers.length; i++) {
            var candidate = doc.layers[i];
            for (var j = 0; j < MDUX.ignoreLayerNames.length; j++) {
                if (candidate.name === MDUX.ignoreLayerNames[j]) {
                    layer = candidate;
                    break;
                }
            }
            if (layer) break;
        }
    } catch (eSearch) {}

    if (!layer && createIfMissing) {
        try {
            layer = doc.layers.add();
            layer.name = "Ignore";
            layer.zOrder(ZOrderMethod.SENDTOFRONT);
        } catch (eCreate) {
            layer = null;
        }
    }

    if (layer) {
        try { layer.locked = false; } catch (eLocked) {}
        try { layer.visible = true; } catch (eVisible) {}
    }
    return layer;
}

function MDUX_calculateEndpointInfo(pathItem) {
    if (!pathItem || pathItem.closed) {
        return null;
    }
    var pts = null;
    try { pts = pathItem.pathPoints; } catch (ePoints) { pts = null; }
    if (!pts || pts.length < 2) {
        return null;
    }
    var last = pts[pts.length - 1];
    var prev = pts[pts.length - 2];
    var endpoint = [last.anchor[0], last.anchor[1]];
    var dx = endpoint[0] - prev.anchor[0];
    var dy = endpoint[1] - prev.anchor[1];
    var length = Math.sqrt((dx * dx) + (dy * dy));
    if (length === 0) {
        dx = 1;
        dy = 0;
        length = 1;
    }
    return {
        endpoint: endpoint,
        direction: [dx / length, dy / length],
        length: length
    };
}

function MDUX_applyIgnoreToSelection() {
    var stats = {
        total: 0,
        added: 0,
        skipped: 0,
        moved: 0
    };

    if (app.documents.length === 0) {
        stats.error = "No Illustrator document is open.";
        return stats;
    }

    var doc = app.activeDocument;
    var selection = null;
    try { selection = doc.selection; } catch (e) { selection = null; }
    if (!selection || selection.length === 0) {
        stats.reason = "no-selection";
        return stats;
    }

    // Ductwork piece layer names
    var ductworkPieceLayers = [
        "Thermostats", "Units", "Secondary Exhaust", "Exhaust Registers",
        "Orange Register", "Rectangular Registers", "Square Registers", "Circular Registers"
    ];

    var ignoreLayer = MDUX_getIgnoreLayer(doc, true);
    if (!ignoreLayer) {
        stats.error = "Unable to create or access the Ignore layer.";
        return stats;
    }

    // Process open paths (ductwork lines)
    var targets = MDUX_collectSelectedOpenPaths(doc);
    stats.total = targets.length;

    for (var i = 0; i < targets.length; i++) {
        var pathItem = targets[i];
        if (!MDUX_isEligibleIgnorePath(pathItem)) {
            stats.skipped++;
            continue;
        }
        var endpointInfo = MDUX_calculateEndpointInfo(pathItem);
        if (!endpointInfo) {
            stats.skipped++;
            continue;
        }
        try {
            var ignorePath = ignoreLayer.pathItems.add();
            ignorePath.stroked = false;
            ignorePath.filled = false;
            ignorePath.closed = false;
            ignorePath.name = "__MDUX_ignore_point";
            ignorePath.setEntirePath([endpointInfo.endpoint, endpointInfo.endpoint]);
            ignorePath.opacity = 0;
            stats.added++;
        } catch (eAdd) {
            stats.skipped++;
        }
    }

    // Process ductwork parts (PlacedItems) - move them and their anchors to Ignored layer
    var ductworkParts = [];
    for (var s = 0; s < selection.length; s++) {
        var item = selection[s];
        try {
            if (item.typename === "PlacedItem" && item.layer) {
                var layerName = item.layer.name;
                for (var dl = 0; dl < ductworkPieceLayers.length; dl++) {
                    if (layerName === ductworkPieceLayers[dl]) {
                        ductworkParts.push(item);
                        break;
                    }
                }
            }
        } catch (e) {}
    }

    stats.total += ductworkParts.length;

    for (var p = 0; p < ductworkParts.length; p++) {
        var part = ductworkParts[p];
        try {
            // Get center position before moving
            var bounds = part.geometricBounds;
            var centerX = (bounds[0] + bounds[2]) / 2;
            var centerY = (bounds[1] + bounds[3]) / 2;

            // Move the part to the Ignore layer
            part.move(ignoreLayer, ElementPlacement.PLACEATEND);
            stats.moved++;

            // Create an invisible anchor point at the center position
            try {
                var anchorPath = ignoreLayer.pathItems.add();
                anchorPath.stroked = false;
                anchorPath.filled = false;
                anchorPath.closed = false;
                anchorPath.name = "__MDUX_ignore_point";
                anchorPath.setEntirePath([[centerX, centerY], [centerX, centerY]]);
                anchorPath.opacity = 0;
                stats.added++;
            } catch (eAnchor) {}
        } catch (eMove) {
            stats.skipped++;
        }
    }

    try { app.redraw(); } catch (eRedraw) {}
    return stats;
}

function MDUX_applyIgnoreToCurrent() {
    return MDUX_applyIgnoreToSelection();
}

function MDUX_toggleIgnoreMode() {
    return "IGNORE_MODE_DISABLED";
}

function MDUX_setIgnoreMode() {
    return "false";
}

function MDUX_getIgnoreModeStatus() {
    return "inactive";
}

function MDUX_cleanup() {
    // No-op -- hover preview removed.
}
