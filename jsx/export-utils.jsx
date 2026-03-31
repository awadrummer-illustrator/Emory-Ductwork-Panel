
/**
 * Export Utilities for Emory Ductwork
 */

// ========================================
// CONFIGURATION
// ========================================

var EXPORT_LAYERS_DUCTWORK = [
    // Ductwork Pieces
    "Thermostats",
    "Units",
    "Secondary Exhaust Registers",
    "Exhaust Registers",
    "Orange Register",
    "Rectangular Registers",
    "Square Registers",
    "Circular Registers",
    // Ductwork Lines
    "Green Ductwork",
    "Light Green Ductwork",
    "Blue Ductwork",
    "Orange Ductwork",
    "Light Orange Ductwork",
    "Thermostat Lines",
    // Frame
    "Frame"
];

var EXPORT_LAYERS_FLOORPLAN = [
    "Render",      // Also matches "Render 01", "Render 02", etc. via prefix matching
    "Floorplan",   // Legacy layer name
    "Layer 1",     // Default layer name (backwards compatibility)
    "Frame"
];

// ========================================
// HELPER FUNCTIONS
// ========================================

var MDUX_logBuffer = [];

function MDUX_log(msg) {
    MDUX_logBuffer.push(msg);
}

function MDUX_getLog() {
    return MDUX_logBuffer.join("\n");
}

function MDUX_getFrameBounds(doc) {
    // Get the bounds of the Frame layer's content (the crop area)
    try {
        var frameLayer = doc.layers.getByName("Frame");
        if (!frameLayer) {
            MDUX_log("[getFrameBounds] Frame layer not found");
            return null;
        }

        // Find the largest path item on the Frame layer (the frame rectangle)
        var maxArea = 0;
        var frameBounds = null;

        for (var i = 0; i < frameLayer.pathItems.length; i++) {
            var item = frameLayer.pathItems[i];
            try {
                var bounds = item.geometricBounds;
                var area = (bounds[2] - bounds[0]) * (bounds[1] - bounds[3]);
                if (area > maxArea) {
                    maxArea = area;
                    frameBounds = bounds;
                }
            } catch (e) { }
        }

        if (frameBounds) {
            MDUX_log("[getFrameBounds] Found frame bounds: " + frameBounds.join(", "));
            return frameBounds;
        }

        // Fallback: try pageItems if no pathItems found
        for (var j = 0; j < frameLayer.pageItems.length; j++) {
            var pageItem = frameLayer.pageItems[j];
            try {
                var bounds = pageItem.geometricBounds;
                var area = (bounds[2] - bounds[0]) * (bounds[1] - bounds[3]);
                if (area > maxArea) {
                    maxArea = area;
                    frameBounds = bounds;
                }
            } catch (e) { }
        }

        if (frameBounds) {
            MDUX_log("[getFrameBounds] Found frame bounds from pageItems: " + frameBounds.join(", "));
        } else {
            MDUX_log("[getFrameBounds] No frame bounds found");
        }
        return frameBounds;
    } catch (e) {
        MDUX_log("[getFrameBounds] Error: " + e.message);
        return null;
    }
}

function MDUX_getFloorplanItem(doc) {
    // Helper to get filename from embedded RasterItem (if available)
    function getEmbeddedImageName(rasterItem) {
        try {
            // Try various properties that might contain the original filename
            if (rasterItem.file && rasterItem.file.name) {
                return rasterItem.file.name;
            }
        } catch(e) {}

        try {
            // Try sourceArt property
            if (rasterItem.sourceArt && rasterItem.sourceArt.file) {
                return rasterItem.sourceArt.file.name;
            }
        } catch(e) {}

        try {
            // Check if there's embedded file info via the item's internal name
            // Illustrator sometimes stores original filename in the item name
            if (rasterItem.name && rasterItem.name.length > 0) {
                return rasterItem.name;
            }
        } catch(e) {}

        return null;
    }

    // Helper to find best item in a specific layer (supports both placed and embedded)
    function getBestFromLayer(layerName, requirePNG) {
        try {
            var layer = doc.layers.getByName(layerName);
            var best = null;
            var maxArea = 0;
            MDUX_log("[getFloorplanItem] Checking layer '" + layerName + "' - found " + layer.placedItems.length + " placed items");
            for (var i = 0; i < layer.placedItems.length; i++) {
                var item = layer.placedItems[i];
                try {
                    if (!item.file) {
                        MDUX_log("[getFloorplanItem]   Item " + i + ": no linked file (embedded?)");
                        continue;
                    }

                    var fName = item.file.name;
                    MDUX_log("[getFloorplanItem]   Item " + i + ": " + fName);

                    // Check for PNG if required
                    if (requirePNG) {
                        if (fName.toLowerCase().slice(-4) !== ".png") {
                            MDUX_log("[getFloorplanItem]     Skipped (not PNG)");
                            continue;
                        }
                    }

                    var area = item.width * item.height;
                    // Pick the largest one. If equal, keeps the first one found (since > not >=).
                    if (area > maxArea) {
                        maxArea = area;
                        best = item;
                        MDUX_log("[getFloorplanItem]     Selected as best (area=" + Math.round(area) + ")");
                    }
                } catch(e) {
                    MDUX_log("[getFloorplanItem]   Item " + i + " error: " + e.message);
                }
            }
            return best;
        } catch(e) {
            MDUX_log("[getFloorplanItem] Layer '" + layerName + "' not found or error: " + e.message);
            return null;
        }
    }

    // Priority 1: Render Layer (Highest priority for naming) - MUST be PNG
    MDUX_log("[getFloorplanItem] Priority 1: Checking Render layer for linked items...");
    var item = getBestFromLayer("Render", true);
    if (item) {
        MDUX_log("[getFloorplanItem] Using Render layer linked item: " + item.file.name);
        return item;
    }

    // Priority 1b: Check Render layer for embedded rasterItems
    MDUX_log("[getFloorplanItem] Priority 1b: Checking Render layer for embedded images...");
    try {
        var renderLayer = doc.layers.getByName("Render");
        if (renderLayer.rasterItems.length > 0) {
            var bestRaster = null;
            var maxRasterArea = 0;
            var bestRasterName = null;

            for (var ri = 0; ri < renderLayer.rasterItems.length; ri++) {
                var rItem = renderLayer.rasterItems[ri];
                var rasterName = getEmbeddedImageName(rItem);
                var area = rItem.width * rItem.height;

                MDUX_log("[getFloorplanItem]   RasterItem " + ri + ": name=" + (rasterName || "(none)") + ", area=" + Math.round(area));

                // Check if it's a PNG (if we can determine the name)
                if (rasterName) {
                    if (rasterName.toLowerCase().slice(-4) !== ".png") {
                        MDUX_log("[getFloorplanItem]     Skipped (not PNG)");
                        continue;
                    }
                }

                if (area > maxRasterArea) {
                    maxRasterArea = area;
                    bestRaster = rItem;
                    bestRasterName = rasterName;
                    MDUX_log("[getFloorplanItem]     Selected as best");
                }
            }

            if (bestRaster) {
                // If we couldn't get the embedded name, try using the document name
                // (when PNG is opened directly, doc takes that name, then saved as .ai)
                if (!bestRasterName && doc.name) {
                    var docBaseName = doc.name.replace(/\.[^\.]+$/, ""); // Remove extension
                    bestRasterName = docBaseName + ".png";
                    MDUX_log("[getFloorplanItem]   Using document name as fallback: " + bestRasterName);
                }

                if (bestRasterName) {
                    MDUX_log("[getFloorplanItem] Using Render layer embedded image: " + bestRasterName);

                    // Determine the correct parent folder for export
                    // If doc.path ends with "2K, 4K and 8K", use its parent instead
                    var docFolder = new Folder(doc.path);
                    var exportParentFolder = docFolder;
                    MDUX_log("[getFloorplanItem]   Doc folder: " + docFolder.fsName + ", folder name: " + docFolder.name);

                    if (docFolder.name === "2K, 4K and 8K" && docFolder.parent) {
                        exportParentFolder = docFolder.parent;
                        MDUX_log("[getFloorplanItem]   Doc is in export subfolder, using parent: " + exportParentFolder.fsName);
                    }

                    // Return a wrapper object that mimics placedItem structure
                    return {
                        _isEmbedded: true,
                        _embeddedName: bestRasterName,
                        width: bestRaster.width,
                        height: bestRaster.height,
                        geometricBounds: bestRaster.geometricBounds,
                        file: {
                            name: bestRasterName,
                            parent: exportParentFolder  // This is a Folder object
                        }
                    };
                }
            }
        }
    } catch(e) {
        MDUX_log("[getFloorplanItem] Error checking rasterItems: " + e.message);
    }

    // Priority 2: Floorplan Layer
    MDUX_log("[getFloorplanItem] Priority 2: Checking Floorplan layer...");
    item = getBestFromLayer("Floorplan", false);
    if (item) {
        MDUX_log("[getFloorplanItem] Using Floorplan layer item: " + item.file.name);
        return item;
    }

    // Priority 3: Ductwork Reference Layer
    MDUX_log("[getFloorplanItem] Priority 3: Checking Ductwork Reference layer...");
    item = getBestFromLayer("Ductwork Reference", false);
    if (item) {
        MDUX_log("[getFloorplanItem] Using Ductwork Reference layer item: " + item.file.name);
        return item;
    }

    // Fallback: Largest in document
    MDUX_log("[getFloorplanItem] Fallback: Searching all placed items...");
    var bestItem = null;
    var maxArea = 0;
    for (var i = 0; i < doc.placedItems.length; i++) {
        var docItem = doc.placedItems[i];
        try {
            if (!docItem.file) continue;
            var area = docItem.width * docItem.height;
            if (area > maxArea) {
                maxArea = area;
                bestItem = docItem;
            }
        } catch(e) {}
    }
    if (bestItem) {
        MDUX_log("[getFloorplanItem] Using fallback item: " + bestItem.file.name);
    } else {
        MDUX_log("[getFloorplanItem] No suitable item found!");
    }
    return bestItem;
}

function MDUX_setLayerVisibility(doc, layerNamesToKeepVisible) {
    var originalStates = {};

    // Build a lookup set for exact matches
    var exactMatches = {};
    for (var k = 0; k < layerNamesToKeepVisible.length; k++) {
        exactMatches[layerNamesToKeepVisible[k]] = true;
    }

    // Check if "Render" is in the list (for prefix matching "Render 01", "Render 02", etc.)
    var matchRenderPrefix = exactMatches["Render"] === true;

    // Store original states and hide all, but show matching layers
    for (var i = 0; i < doc.layers.length; i++) {
        var layer = doc.layers[i];
        originalStates[layer.name] = layer.visible;

        // Check if this layer should be visible
        var shouldShow = false;

        // Exact match check
        if (exactMatches[layer.name]) {
            shouldShow = true;
        }
        // Prefix match for "Render" variants (Render 01, Render 02, etc.)
        else if (matchRenderPrefix && layer.name.indexOf("Render") === 0) {
            shouldShow = true;
        }

        layer.visible = shouldShow;
    }

    return originalStates;
}

function MDUX_restoreLayerVisibility(doc, originalStates) {
    for (var name in originalStates) {
        if (originalStates.hasOwnProperty(name)) {
            try {
                var layer = doc.layers.getByName(name);
                layer.visible = originalStates[name];
            } catch (e) {}
        }
    }
}

function MDUX_ensureFolderExists(folderPath) {
    var folder = new Folder(folderPath);
    if (!folder.exists) {
        folder.create();
    }
    return folder;
}

function MDUX_exportPNG(doc, file, scalePercent, artboardRect) {
    var options = new ExportOptionsPNG24();
    options.antiAliasing = true;
    options.transparency = true;
    options.artBoardClipping = true;
    options.horizontalScale = scalePercent;
    options.verticalScale = scalePercent;

    // If we have a specific rect, we might need to adjust the artboard
    // But for now, let's assume the user wants the active artboard or we set it
    
    doc.exportFile(file, ExportType.PNG24, options);
}

function MDUX_launchCaesiumWithFiles(files) {
    try {
        // Define the path to Caesium executable
        var caesiumPath = "C:\\Program Files\\Caesium Image Compressor\\caesiumclt.exe";
        var caesiumApp = new File(caesiumPath);
        
        if (!caesiumApp.exists) {
            MDUX_log("Caesium application not found at: " + caesiumPath);
            // Try common locations
            var commonPaths = [
                "C:/Program Files (x86)/Caesium/caesiumclt.exe",
                "C:/Program Files/Caesium Image Compressor/caesiumclt.exe",
                "C:/Program Files (x86)/Caesium Image Compressor/caesiumclt.exe"
            ];
            for (var i = 0; i < commonPaths.length; i++) {
                caesiumApp = new File(commonPaths[i]);
                if (caesiumApp.exists) {
                    caesiumPath = commonPaths[i];
                    MDUX_log("Found Caesium at: " + caesiumPath);
                    break;
                }
            }
        }
        
        if (!caesiumApp.exists) {
            // Fallback to system PATH
            caesiumPath = "caesiumclt.exe";
            MDUX_log("Using system PATH to locate Caesium");
        }
        
        // Verify all input files exist
        var validFiles = [];
        for (var i = 0; i < files.length; i++) {
            if (files[i].exists) {
                validFiles.push(files[i]);
            } else {
                MDUX_log("Input file for compression does not exist: " + files[i].fsName);
            }
        }
        
        if (validFiles.length === 0) return false;

        // Build command using a temporary batch file to avoid quoting/escaping issues
        var tempFolder = Folder.temp;
        var batchFile = new File(tempFolder.fsName + "/caesium_compress.bat");

        // Build batch file content - run minimized and detached (async)
        var batchContent = '@echo off\r\n';
        // Use "start /min /b" to run minimized and detached from console
        batchContent += 'start /min /b "" "' + caesiumPath + '" -q 95 --output "' + validFiles[0].parent.fsName + '"';
        for (var i = 0; i < validFiles.length; i++) {
            batchContent += ' "' + validFiles[i].fsName + '"';
        }
        batchContent += '\r\n';

        // Write batch file
        batchFile.open("w");
        batchFile.write(batchContent);
        batchFile.close();

        MDUX_log("Batch file created at: " + batchFile.fsName);
        MDUX_log("Batch content: " + batchContent);

        // Execute the batch file (fire-and-forget, no waiting)
        var result = batchFile.execute();

        if (!result) {
            MDUX_log("Caesium batch file failed to launch");
            return false;
        }

        MDUX_log("Caesium compression launched asynchronously (running in background)");
        return true;
    } catch (e) {
        MDUX_log("Error launching Caesium: " + e);
        return false;
    }
}

// ========================================
// MAIN EXPORT FUNCTION
// ========================================

function MDUX_performExport(exportType, overwrite, versionSuffix) {
    MDUX_logBuffer = []; // Clear buffer at start
    MDUX_log("MDUX_performExport called with: " + exportType + ", overwrite=" + overwrite + ", version=" + versionSuffix);
    
    try {
        if (app.documents.length === 0) {
            MDUX_log("No document open.");
            return JSON.stringify({ ok: false, message: "No document open.", log: MDUX_getLog() });
        }
        
        var doc = app.activeDocument;
        var floorplanItem = MDUX_getFloorplanItem(doc);
        
        if (!floorplanItem) {
            MDUX_log("Could not find Floorplan reference image (must be a linked PlacedItem).");
            return JSON.stringify({ ok: false, message: "Could not find Floorplan reference image (must be a linked PlacedItem).", log: MDUX_getLog() });
        }

        // Get reference file info
        var refFile = null;
        try {
            refFile = floorplanItem.file;
        } catch (e) {
            MDUX_log("Floorplan item is not linked to a file: " + e.message);
            return JSON.stringify({ ok: false, message: "Floorplan item is not linked to a file.", log: MDUX_getLog() });
        }

        if (!refFile) {
            MDUX_log("Floorplan item has no source file.");
            return JSON.stringify({ ok: false, message: "Floorplan item has no source file.", log: MDUX_getLog() });
        }

        MDUX_log("Found floorplan file: " + (refFile.fsName || refFile.name));

        var refFileName = decodeURI(refFile.name);
        
        // Debug: log char codes to identify weird dashes
        var charCodes = [];
        for(var i=0; i<refFileName.length; i++) charCodes.push(refFileName.charCodeAt(i));
        MDUX_log("Filename char codes: " + charCodes.join(","));

        var refNameBase = refFileName.substring(0, refFileName.lastIndexOf("."));
        
        // Ensure it's a string
        refNameBase = String(refNameBase);

        // Clean up the filename:
        // 1. Replace dashes (hyphen, en-dash, em-dash, and other variants) with spaces.
        
        // Nuclear option: Rebuild string character by character to ensure no regex/split weirdness
        var cleanName = "";
        // Map of characters to replace with space
        var replaceWithSpaceMap = {
            45: true,    // Hyphen-Minus (-)
            95: false,   // Underscore (_) - KEEP IT
            8208: true,  // Hyphen (â€)
            8209: true,  // Non-breaking Hyphen (â€‘)
            8210: true,  // Figure Dash (â€’)
            8211: true,  // En Dash (â€“)
            8212: true,  // Em Dash (â€”)
            8213: true,  // Horizontal Bar (â€•)
            8722: true,  // Minus Sign (âˆ’)
            65112: true, // Small Em Dash (FE58)
            65123: true, // Small Hyphen-Minus (FE63)
            65293: true  // Fullwidth Hyphen-Minus (FF0D)
        };

        MDUX_log("Starting cleanup on: '" + refNameBase + "'");

        for (var i = 0; i < refNameBase.length; i++) {
            var cc = refNameBase.charCodeAt(i);
            if (replaceWithSpaceMap[cc] === true) {
                cleanName += " ";
            } else {
                cleanName += refNameBase.charAt(i);
            }
        }
        refNameBase = cleanName;
        
        MDUX_log("After dash removal: '" + refNameBase + "'");

        // Remove existing resolution suffixes at end (checking for both space and underscore)
        // We use a simple regex that works reliably in ES3
        refNameBase = refNameBase.replace(new RegExp("[ _]4K$", "i"), "");
        refNameBase = refNameBase.replace(new RegExp("[ _]2K$", "i"), "");
        refNameBase = refNameBase.replace(new RegExp("[ _]8K$", "i"), "");
        refNameBase = refNameBase.replace(new RegExp("[ _]1080p$", "i"), "");
        refNameBase = refNameBase.replace(new RegExp("[ _]720p$", "i"), "");
        // Normalize multiple spaces to single space and trim
        while (refNameBase.indexOf("  ") >= 0) {
            refNameBase = refNameBase.replace("  ", " ");
        }
        // Trim leading/trailing spaces
        while (refNameBase.charAt(0) === " ") {
            refNameBase = refNameBase.substring(1);
        }
        while (refNameBase.charAt(refNameBase.length - 1) === " ") {
            refNameBase = refNameBase.substring(0, refNameBase.length - 1);
        }

        MDUX_log("Cleaned filename base: " + refNameBase);

        var parentFolder = refFile.parent;

        // Get Frame bounds early for dimension calculation
        var earlyFrameBounds = MDUX_getFrameBounds(doc);
        if (!earlyFrameBounds) {
            MDUX_log("ERROR: Could not find Frame layer bounds.");
            return JSON.stringify({ ok: false, message: "Could not find Frame layer bounds. Ensure a Frame layer exists with a rectangle.", log: MDUX_getLog() });
        }

        // Calculate Dimensions and Scale based on Frame bounds
        var itemWidth = earlyFrameBounds[2] - earlyFrameBounds[0]; // right - left = width
        var itemHeight = earlyFrameBounds[1] - earlyFrameBounds[3]; // top - bottom = height (AI coords)

        MDUX_log("Frame dimensions: " + itemWidth + "x" + itemHeight);

        // Target dimensions
        // If Width > Height: Width = 4000px
        // If Height > Width: Height = 2160px
        
        var targetWidth4K, targetHeight4K;
        var scaleFactor4K;

        if (itemWidth > itemHeight) {
            targetWidth4K = 4000;
            scaleFactor4K = (targetWidth4K / itemWidth) * 100; // Percentage
        } else {
            targetHeight4K = 2160;
            scaleFactor4K = (targetHeight4K / itemHeight) * 100; // Percentage
        }

        var scaleFactor2K = scaleFactor4K / 2;
        var scaleFactor8K = scaleFactor4K * 2; // 8K is double the 4K size

        MDUX_log("Scale Factor 4K: " + scaleFactor4K + "%, 8K: " + scaleFactor8K + "%");

        // Define Suffixes and Layers
        var typeSuffix = "";
        var layersToExport = [];
        var exportTypeUpper = exportType.toUpperCase();

        if (exportTypeUpper === "DUCTWORK") {
            typeSuffix = " Ductwork";
            layersToExport = EXPORT_LAYERS_DUCTWORK.slice(); // Copy array
        } else if (exportTypeUpper === "FLOORPLAN") {
            typeSuffix = " Floorplan"; 
            layersToExport = EXPORT_LAYERS_FLOORPLAN.slice(); // Copy array
        }

        // Versioning
        var versionStr = "";
        if (versionSuffix) {
            versionStr = " V" + versionSuffix;
        }

        // Construct Filenames (Desired Final Names)
        var finalName8K = refNameBase + typeSuffix + versionStr + "_8K.png";
        var finalName4K = refNameBase + typeSuffix + versionStr + "_4K.png";
        var finalName2K = refNameBase + typeSuffix + versionStr + "_2K.png";

        // Construct Safe Filenames for Export (replace spaces with underscores to avoid Illustrator auto-dashing)
        // We will export to these safe names, then rename the files to the final names.
        var safeName8K = finalName8K.split(" ").join("_");
        var safeName4K = finalName4K.split(" ").join("_");
        var safeName2K = finalName2K.split(" ").join("_");

        // File objects for checking existence (Final Names)
        var finalFile8K = new File(parentFolder.fsName + "/" + finalName8K);
        var finalFile4K = new File(parentFolder.fsName + "/" + finalName4K);
        var finalFile2K = new File(parentFolder.fsName + "/" + finalName2K);

        // File objects for Exporting (Safe Names)
        var exportFile8K = new File(parentFolder.fsName + "/" + safeName8K);
        var exportFile4K = new File(parentFolder.fsName + "/" + safeName4K);
        var exportFile2K = new File(parentFolder.fsName + "/" + safeName2K);

        MDUX_log("Target 8K file (Final): " + finalFile8K.fsName);
        MDUX_log("Target 4K file (Final): " + finalFile4K.fsName);

        // Check existence (Check against FINAL names)
        if (!overwrite && !versionSuffix) {
            if (finalFile8K.exists || finalFile4K.exists || finalFile2K.exists) {
                MDUX_log("Files exist, asking for confirmation.");
                return JSON.stringify({
                    ok: true,
                    status: "CONFIRM_OVERWRITE",
                    message: "Files already exist. Overwrite?",
                    existingFiles: [finalFile8K.fsName, finalFile4K.fsName, finalFile2K.fsName],
                    log: MDUX_getLog()
                });
            }
        }

        // Prepare Artboard
        // We want to export exactly the area of the Frame layer.
        // Best way: Resize active artboard to match Frame bounds, then restore.
        var activeArtboardIndex = doc.artboards.getActiveArtboardIndex();
        var originalArtboardRect = doc.artboards[activeArtboardIndex].artboardRect;

        // Set artboard to Frame bounds (already retrieved earlier)
        // Rect is [left, top, right, bottom]
        doc.artboards[activeArtboardIndex].artboardRect = earlyFrameBounds;

        // Set Visibility
        var originalVis = MDUX_setLayerVisibility(doc, layersToExport);

        try {
            // Export 8K (to Safe Name)
            MDUX_log("Exporting 8K to " + exportFile8K.name + "...");
            MDUX_exportPNG(doc, exportFile8K, scaleFactor8K);

            // Rename 8K to Final Name
            if (exportFile8K.exists) {
                if (finalFile8K.exists) finalFile8K.remove(); // Ensure target is clear
                var renameResult8K = exportFile8K.rename(finalName8K);
                MDUX_log("Renamed 8K to " + finalName8K + ": " + renameResult8K);
            } else {
                MDUX_log("Error: Exported 8K file not found!");
            }

            // Export 4K (to Safe Name)
            MDUX_log("Exporting 4K to " + exportFile4K.name + "...");
            MDUX_exportPNG(doc, exportFile4K, scaleFactor4K);

            // Rename 4K to Final Name
            if (exportFile4K.exists) {
                if (finalFile4K.exists) finalFile4K.remove(); // Ensure target is clear
                var renameResult4K = exportFile4K.rename(finalName4K);
                MDUX_log("Renamed 4K to " + finalName4K + ": " + renameResult4K);
            } else {
                MDUX_log("Error: Exported 4K file not found!");
            }

            // Export 2K (to Safe Name)
            MDUX_log("Exporting 2K to " + exportFile2K.name + "...");
            MDUX_exportPNG(doc, exportFile2K, scaleFactor2K);

            // Rename 2K to Final Name
            if (exportFile2K.exists) {
                if (finalFile2K.exists) finalFile2K.remove(); // Ensure target is clear
                var renameResult2K = exportFile2K.rename(finalName2K);
                MDUX_log("Renamed 2K to " + finalName2K + ": " + renameResult2K);
            } else {
                MDUX_log("Error: Exported 2K file not found!");
            }

            // Compression - Create FRESH File objects to avoid ExtendScript caching issues
            var filesToCompress = [];
            var compressFile8K = new File(parentFolder.fsName + "/" + finalName8K);
            var compressFile4K = new File(parentFolder.fsName + "/" + finalName4K);
            var compressFile2K = new File(parentFolder.fsName + "/" + finalName2K);

            MDUX_log("Checking for compression - 8K exists: " + compressFile8K.exists);
            MDUX_log("Checking for compression - 4K exists: " + compressFile4K.exists);
            MDUX_log("Checking for compression - 2K exists: " + compressFile2K.exists);

            if (compressFile8K.exists) filesToCompress.push(compressFile8K);
            if (compressFile4K.exists) filesToCompress.push(compressFile4K);
            if (compressFile2K.exists) filesToCompress.push(compressFile2K);
            
            if (filesToCompress.length > 0) {
                MDUX_log("Calling compression with " + filesToCompress.length + " files");
                var compressionResult = MDUX_launchCaesiumWithFiles(filesToCompress);
                MDUX_log("Compression result: " + compressionResult);
            } else {
                MDUX_log("No files found for compression!");
            }

        } catch (e) {
            MDUX_log("Export failed: " + e.message);
            return JSON.stringify({ ok: false, message: "Export failed: " + e.message, log: MDUX_getLog() });
        } finally {
            // Restore
            MDUX_restoreLayerVisibility(doc, originalVis);
            doc.artboards[activeArtboardIndex].artboardRect = originalArtboardRect;
        }

        MDUX_log("Export successful.");
        return JSON.stringify({ ok: true, status: "SUCCESS", message: "Exported successfully!\nSource: " + refFileName + "\nFolder: " + parentFolder.fsName, log: MDUX_getLog() });

    } catch (globalErr) {
        MDUX_log("CRITICAL ERROR: " + globalErr.message);
        return JSON.stringify({ ok: false, message: "Critical Error: " + globalErr.message, log: MDUX_getLog() });
    }
}

