(function () {
    'use strict';

    const csInterface = (typeof CSInterface !== 'undefined') ? new CSInterface() : {
        evalScript: function () {},
        getSystemPath: function () { return ''; }
    };
    const ENABLE_LEGACY_PROCESS_BUTTON = false;
    const EMORY_PROCESS_READY = true;
    let panelLogPath = null;
    const fallbackLogPath = 'C:/Users/Chris/AppData/Roaming/Adobe/CEP/extensions/Emory-Ductwork-Panel/panel-ui.log';
    function getExtensionRoot() {
        try {
            if (csInterface && csInterface.getSystemPath) {
                return csInterface.getSystemPath(CSInterface.SystemPath.EXTENSION);
            }
        } catch (e) {}
        try {
            const href = window.location && window.location.href ? window.location.href : '';
            const match = href.match(/^file:\/\/\/(.+?)\/index\.html/i);
            if (match && match[1]) {
                return decodeURIComponent(match[1]);
            }
        } catch (e) {}
        return '';
    }
    function panelFileLog(message) {
        try {
            const text = String(message || '');
            if (!panelLogPath) {
                const root = getExtensionRoot();
                panelLogPath = root ? (root + '/panel-ui.log') : fallbackLogPath;
            }
            if (!panelLogPath) {
                return;
            }
            if (typeof require === 'function') {
                const fs = require('fs');
                fs.appendFileSync(panelLogPath, text + '\n');
                return;
            }
            if (window.cep && window.cep.fs) {
                const read = window.cep.fs.readFile(panelLogPath);
                const prev = read && read.err === 0 ? read.data : '';
                window.cep.fs.writeFile(panelLogPath, prev + text + '\n');
            }
        } catch (e) {}
    }
    function htmlLog(message) {
        try {
            if (window.MDUX_htmlLog) {
                window.MDUX_htmlLog(String(message || ''));
            }
        } catch (e) {}
    }
    window.MDUX_PANEL_JS_LOADED = true;
    panelFileLog('[BOOT] panel.js loaded; hasCSInterface=' + (typeof CSInterface !== 'undefined'));
    htmlLog('[BOOT] panel.js loaded; hasCSInterface=' + (typeof CSInterface !== 'undefined'));
    window.addEventListener('error', (event) => {
        try {
            const msg = String(event && event.message ? event.message : event);
            panelFileLog('[JS ERROR] ' + msg);
            if (debugStatus) {
                debugStatus.textContent = '[JS ERROR] ' + msg;
            }
            csInterface.evalScript('MDUX_debugLog("[PANEL-ERROR] ' + msg.replace(/'/g, "\\'") + '")', function() {});
        } catch (e) {}
    });
    let processBtn = document.getElementById('process-btn');
    let processPlacedBtn = document.getElementById('process-placed-btn');
    let toggleConnectorStyleBtn = document.getElementById('toggle-connector-style-btn');
    let markOverlapSeparateBtn = document.getElementById('mark-overlap-separate-btn');
    let setEmoryStartBtn = document.getElementById('set-emory-start-btn');
    let emoryWidthSlider = document.getElementById('emory-width-slider');
    let emoryWidthInput = document.getElementById('emory-width-input');
    let emorySelectionStatus = document.getElementById('emory-selection-status');
    let emoryWidthStatus = document.getElementById('emory-width-status');
    let processEmoryBtn = document.getElementById('process-emory-btn');
    let processStatus = document.getElementById('process-status');
    let revertBtn = document.getElementById('revert-ortho-btn');
    let revertStatus = document.getElementById('revert-status');
    let clearRotationMetadataBtn = document.getElementById('clear-rotation-metadata-btn');
    let clearRotationMetadataStatus = document.getElementById('clear-rotation-metadata-status');
    // const applyIgnoreBtn = document.getElementById('apply-ignore-btn'); // Removed - button no longer exists
    // const ignoreStatus = document.getElementById('ignore-status'); // Removed - status no longer exists
    let reloadBtn = document.getElementById('reload-btn');
    const extensionId = 'com.chris.emoryductwork.panel';
    function reloadExtensionView() {
        try {
            const base = window.location.href.split('?')[0];
            window.location.href = base + '?v=' + Date.now();
            return;
        } catch (e) {
            console.error('View reload failed:', e);
        }
        window.location.reload();
    }
    function refreshDomRefs() {
        processBtn = document.getElementById('process-btn');
        processPlacedBtn = document.getElementById('process-placed-btn');
        toggleConnectorStyleBtn = document.getElementById('toggle-connector-style-btn');
        markOverlapSeparateBtn = document.getElementById('mark-overlap-separate-btn');
        setEmoryStartBtn = document.getElementById('set-emory-start-btn');
        emoryWidthSlider = document.getElementById('emory-width-slider');
        emoryWidthInput = document.getElementById('emory-width-input');
        emorySelectionStatus = document.getElementById('emory-selection-status');
        emoryWidthStatus = document.getElementById('emory-width-status');
        processEmoryBtn = document.getElementById('process-emory-btn');
        processStatus = document.getElementById('process-status');
        revertBtn = document.getElementById('revert-ortho-btn');
        revertStatus = document.getElementById('revert-status');
        clearRotationMetadataBtn = document.getElementById('clear-rotation-metadata-btn');
        clearRotationMetadataStatus = document.getElementById('clear-rotation-metadata-status');
        reloadBtn = document.getElementById('reload-btn');
        debugStatus = document.getElementById('debug-status');
        debugLoggingOption = document.getElementById('debug-logging-option');
        devModeOption = document.getElementById('dev-mode-option');
        yieldToUiOption = document.getElementById('yield-to-ui-option');
        resetSessionBtn = document.getElementById('reset-session-btn');
        skipOrthoOption = document.getElementById('skip-ortho-option');
        processSkipOrthoOption = document.getElementById('process-skip-ortho-option');
        rotationInput = document.getElementById('rotation-input');
        getAngleBtn = document.getElementById('get-angle-btn');
        clearRotationBtn = document.getElementById('clear-rotation-btn');
        skipAllBranchesOption = document.getElementById('skip-all-branches-option');
        skipFinalOption = document.getElementById('skip-final-option');
        processSkipAllBranchesOption = document.getElementById('process-skip-all-branches-option');
        processSkipFinalOption = document.getElementById('process-skip-final-option');
        skipRegisterRotationOption = document.getElementById('skip-register-rotation-option');
        createRegisterWiresOption = document.getElementById('create-register-wires-option');
        rotate90Btn = document.getElementById('rotate-90-btn');
        rotate45Btn = document.getElementById('rotate-45-btn');
        rotate180Btn = document.getElementById('rotate-180-btn');
        rotateCustomBtn = document.getElementById('rotate-custom-btn');
        customRotationInput = document.getElementById('custom-rotation-input');
        scaleSlider = document.getElementById('scale-slider');
        scaleLabel = document.getElementById('scale-label');
        scaleInput = document.getElementById('scale-input');
        applyScaleBtn = document.getElementById('apply-scale-btn');
        resetScaleBtn = document.getElementById('reset-scale-btn');
        selectionStatus = document.getElementById('selection-status');
        isolatePartsBtn = document.getElementById('isolate-parts-btn');
        isolateLinesBtn = document.getElementById('isolate-lines-btn');
        unlockDuctworkBtn = document.getElementById('unlock-ductwork-btn');
        createLayersBtn = document.getElementById('create-layers-btn');
        layerStatus = document.getElementById('layer-status');
        importStylesBtn = document.getElementById('import-styles-btn');
        importStatus = document.getElementById('import-status');
        exportDuctworkBtn = document.getElementById('export-ductwork-btn');
        reexportFloorplanBtn = document.getElementById('reexport-floorplan-btn');
        exportStatus = document.getElementById('export-status');
        healGapsBtn = document.getElementById('heal-gaps-btn');
        recutGapsBtn = document.getElementById('recut-gaps-btn');
        mergePathsBtn = document.getElementById('merge-paths-btn');
        docScaleInput = document.getElementById('doc-scale-input');
        getDocScaleBtn = document.getElementById('get-doc-scale-btn');
    }
    let debugStatus = document.getElementById('debug-status');
    let debugLoggingOption = document.getElementById('debug-logging-option');
    let devModeOption = document.getElementById('dev-mode-option');
    let yieldToUiOption = document.getElementById('yield-to-ui-option');
    let resetSessionBtn = document.getElementById('reset-session-btn');
    let skipOrthoOption = document.getElementById('skip-ortho-option');
    let processSkipOrthoOption = document.getElementById('process-skip-ortho-option');
    let rotationInput = document.getElementById('rotation-input');
    let getAngleBtn = document.getElementById('get-angle-btn');
    let clearRotationBtn = document.getElementById('clear-rotation-btn');
    let skipAllBranchesOption = document.getElementById('skip-all-branches-option');
    let skipFinalOption = document.getElementById('skip-final-option');
    let processSkipAllBranchesOption = document.getElementById('process-skip-all-branches-option');
    let processSkipFinalOption = document.getElementById('process-skip-final-option');
    let skipRegisterRotationOption = document.getElementById('skip-register-rotation-option');
    let createRegisterWiresOption = document.getElementById('create-register-wires-option');
    let rotate90Btn = document.getElementById('rotate-90-btn');
    let rotate45Btn = document.getElementById('rotate-45-btn');
    let rotate180Btn = document.getElementById('rotate-180-btn');
    let rotateCustomBtn = document.getElementById('rotate-custom-btn');
    let customRotationInput = document.getElementById('custom-rotation-input');
    let scaleSlider = document.getElementById('scale-slider');
    let scaleLabel = document.getElementById('scale-label');
    let scaleInput = document.getElementById('scale-input');
    let applyScaleBtn = document.getElementById('apply-scale-btn');
    let resetScaleBtn = document.getElementById('reset-scale-btn');
    let selectionStatus = document.getElementById('selection-status');
    let isolatePartsBtn = document.getElementById('isolate-parts-btn');
    let isolateLinesBtn = document.getElementById('isolate-lines-btn');
    let unlockDuctworkBtn = document.getElementById('unlock-ductwork-btn');
    let createLayersBtn = document.getElementById('create-layers-btn');
    let layerStatus = document.getElementById('layer-status');
    let importStylesBtn = document.getElementById('import-styles-btn');
    let importStatus = document.getElementById('import-status');
    let exportDuctworkBtn = document.getElementById('export-ductwork-btn');
    let reexportFloorplanBtn = document.getElementById('reexport-floorplan-btn');
    let exportStatus = document.getElementById('export-status');
    let healGapsBtn = document.getElementById('heal-gaps-btn');
    let recutGapsBtn = document.getElementById('recut-gaps-btn');
    let mergePathsBtn = document.getElementById('merge-paths-btn');

    // Document Scale Controls (read-only anchor display)
    let docScaleInput = document.getElementById('doc-scale-input');
    let getDocScaleBtn = document.getElementById('get-doc-scale-btn');
    // Legacy: Document scaling controls disabled to prevent desync issues
    // const setDocScale100Btn = document.getElementById('set-doc-scale-100-btn');
    // const applyDocScaleBtn = document.getElementById('apply-doc-scale-btn');

    // Transform Each Controls
    let teScaleInput = document.getElementById('te-scale');
    let teRotateInput = document.getElementById('te-rotate');
    let teScaleSlider = document.getElementById('te-scale-slider');
    let teRotateSlider = document.getElementById('te-rotate-slider');
    let transformEachBtn = document.getElementById('transform-each-btn');
    let teResetOriginalBtn = document.getElementById('te-reset-original-btn');
    let teLiveOption = document.getElementById('te-live-option');

    // Reset/Normalize Controls
    const resetStrokesBtn = document.getElementById('reset-strokes-btn');
    const resetPartsScaleBtn = document.getElementById('reset-parts-scale-btn');
    const resetPartsRotationBtn = document.getElementById('reset-parts-rotation-btn');

    // New V3 Process Ductwork Panel Controls
    const rotateRegistersBtn = document.getElementById('rotate-registers-btn');
    const carveRegistersBtn = document.getElementById('carve-registers-btn');
    const carveOverlapsBtn = document.getElementById('carve-overlaps-btn');

    // New V3 Orthogonalize Panel Controls
    const orthoTrunkBtn = document.getElementById('ortho-trunk-btn');
    const orthoBranchesBtn = document.getElementById('ortho-branches-btn');
    const orthoFinalBtn = document.getElementById('ortho-final-btn');
    const orthoAllBtn = document.getElementById('ortho-all-btn');
    const orthoStatus = document.getElementById('ortho-status');

    // New V3 Ductwork Parts Panel Controls
    const createUpdatePartsBtn = document.getElementById('create-update-parts-btn');
    const createAnchorsBtn = document.getElementById('create-anchors-btn');
    const selectPartsBtn = document.getElementById('select-parts-btn');
    const selectAnchorsBtn = document.getElementById('select-anchors-btn');
    const deleteAnchorsBtn = document.getElementById('delete-anchors-btn');
    const placeGraphicsBtn = document.getElementById('place-graphics-btn');
    const partsStatus = document.getElementById('parts-status');

    // New V3 Quick Rotate Controls
    const rotateNeg45Btn = document.getElementById('rotate-neg45-btn');
    const rotateNeg90Btn = document.getElementById('rotate-neg90-btn');
    const quickRotateStatus = document.getElementById('quick-rotate-status');

    // New V3 Export Controls
    const exportFloorplanBtn = document.getElementById('export-floorplan-btn');

    // Collapsible Section Controls
    const docScaleToggle = document.getElementById('doc-scale-toggle');
    const docScaleSection = document.getElementById('doc-scale-section');

    // Gap Editor Overlay Controls
    const gapEditorNormal = document.getElementById('gap-editor-normal');
    const gapEditorActive = document.getElementById('gap-editor-active');
    const editGapsBtn = document.getElementById('edit-gaps-btn');
    const saveGapsBtn = document.getElementById('save-gaps-btn');
    const cancelGapsBtn = document.getElementById('cancel-gaps-btn');
    const addGapBtn = document.getElementById('add-gap-btn');
    const protectionStatus = document.getElementById('protection-status');

    // Gap editor state
    let isInGapEditMode = false;

    let scaleDebounce = null;
    let bridgeReloaded = false;
    let skipOrthoRefreshTimer = null;
    const bridgePath = (function () {
        var root = csInterface.getSystemPath(CSInterface.SystemPath.EXTENSION);
        return root.replace(/\\/g, '/') + '/jsx/panel-bridge.jsx';
    })();;
    window.MDUX_SUSPEND_POLL = false;
    const suspendFlagPath = (function () {
        var base = csInterface.getSystemPath(CSInterface.SystemPath.USER_DATA);
        return base.replace(/\\/g, '/') + '/Adobe/CEP/extensions/Emory-Ductwork-Panel/md_cep_suspend.flag';
    })();

    function isCepSuspended() {
        if (window.MDUX_SUSPEND_POLL) return true;
        try {
            if (window.cep && window.cep.fs && window.cep.fs.exists) {
                return window.cep.fs.exists(suspendFlagPath) === true;
            }
        } catch (e) { }
        return false;
    }

    window.MDUX_setSuspendPoll = function (flag) {
        window.MDUX_SUSPEND_POLL = !!flag;
    };

    const AUTO_SELECTION_REFRESH_ENABLED = true;
    const AUTO_SELECTION_REFRESH_MODE = 'all';
    const AUTO_SELECTION_EVENTS_ENABLED = true;
    const AUTO_SELECTION_POLL_ENABLED = false;
    let cepRuntimeSuspended = false;
    let pollInterval = null;
    let suspendMonitor = null;
    let selectionMonitor = null;
    let pollInProgress = false;
    let lastSelectionHash = '';
    let skipSelectionRefresh = false;
    let emorySelectionState = null;
    let emoryWidthRefreshInFlight = false;
    let emoryWidthApplyTimer = null;
    let emoryWidthDragActive = false;
    let emoryWidthApplyInFlight = false;
    let emoryWidthLastApplied = null;
    let emoryWidthRefreshPending = false;
    let emorySelectionRefreshTimer = null;

    function onAfterSelectionChanged() {
        if (isCepSuspended()) return;
        if (!AUTO_SELECTION_REFRESH_ENABLED) return;
        updateSkipSelectionRefresh().then(() => {
            if (skipSelectionRefresh) return;
            scheduleEmorySelectionRefresh(false);
            scheduleSkipOrthoRefresh();
        }).catch(() => {});
    }

    function onDocumentAfterActivate() {
        if (isCepSuspended()) return;
        if (!AUTO_SELECTION_REFRESH_ENABLED) return;
        updateSkipSelectionRefresh().then(() => {
            if (!skipSelectionRefresh) {
                scheduleEmorySelectionRefresh(true);
                scheduleSkipOrthoRefresh();
            }
        }).catch(() => {});
        resetTransformControls(true);
        lastSelectionHash = '';
        evalScript('MDUX_onDocumentChange()').then(result => {
            console.log('[JS] Document change cleanup:', result);
        }).catch(() => {});
    }

    function onDocumentChanged() {
        if (isCepSuspended()) return;
        if (!AUTO_SELECTION_REFRESH_ENABLED) return;
        updateSkipSelectionRefresh().then(() => {
            if (skipSelectionRefresh) return;
            scheduleEmorySelectionRefresh(false);
            scheduleSkipOrthoRefresh();
        }).catch(() => {});
    }

    function startCepRuntime() {
        if (!AUTO_SELECTION_REFRESH_ENABLED && !AUTO_SELECTION_POLL_ENABLED) return;
        if (cepRuntimeSuspended || isCepSuspended()) return;
        if (AUTO_SELECTION_EVENTS_ENABLED) {
            csInterface.addEventListener('afterSelectionChanged', onAfterSelectionChanged);
            csInterface.addEventListener('documentAfterActivate', onDocumentAfterActivate);
            csInterface.addEventListener('documentChanged', onDocumentChanged);
        }
        if (AUTO_SELECTION_POLL_ENABLED && !pollInterval) {
            pollInterval = setInterval(function() {
                if (pollInProgress || isCepSuspended()) return;
                pollInProgress = true;
                if (skipSelectionRefresh) {
                    updateSkipSelectionRefresh().then(() => {
                        pollInProgress = false;
                    }).catch(() => {
                        pollInProgress = false;
                    });
                    return;
                }
                evalScript('(function(){try{var s=app.activeDocument.selection;if(!s||s.length===0)return"empty";var pos=s[0].position||[0,0];return s.length+"|"+(s[0].typename||"")+"|"+Math.round(pos[0])+","+Math.round(pos[1]);}catch(e){return"nodoc";}})()').then(function(hash) {
                    if (hash === lastSelectionHash) {
                        pollInProgress = false;
                        return;
                    }
                    lastSelectionHash = hash;
                    if (!AUTO_SELECTION_REFRESH_ENABLED) {
                        return;
                    }
                    return Promise.all([
                        refreshSelectionTransformState().catch(function() {}),
                        refreshRotationOverrideState().catch(function() {})
                    ]);
                }).catch(function() {
                }).finally(function() {
                    pollInProgress = false;
                });
            }, 1000);
        }
        cepRuntimeSuspended = false;
    }

    function stopCepRuntime() {
        if (cepRuntimeSuspended) return;
        if (AUTO_SELECTION_EVENTS_ENABLED) {
            csInterface.removeEventListener('afterSelectionChanged', onAfterSelectionChanged);
            csInterface.removeEventListener('documentAfterActivate', onDocumentAfterActivate);
            csInterface.removeEventListener('documentChanged', onDocumentChanged);
        }
        if (pollInterval) {
            clearInterval(pollInterval);
            pollInterval = null;
        }
        cepRuntimeSuspended = true;
    }

    function ensureSuspendMonitor() {
        if (suspendMonitor) return;
        suspendMonitor = setInterval(() => {
            if (isCepSuspended()) {
                stopCepRuntime();
            } else {
                startCepRuntime();
            }
        }, 1000);
    }

    async function selectionHasPlacedItems() {
        try {
            const res = await evalScript('(function(){try{var s=app.selection;if(!s||s.length===0)return\"no\";function hasPlaced(item){if(!item)return false;if(item.typename===\"PlacedItem\")return true;if(item.typename===\"GroupItem\"&&item.pageItems){for(var i=0;i<item.pageItems.length;i++){if(hasPlaced(item.pageItems[i]))return true;}}return false;}if(s.length===undefined&&s.typename){return hasPlaced(s)?\"yes\":\"no\";}for(var i=0;i<s.length;i++){if(hasPlaced(s[i]))return\"yes\";}return\"no\";}catch(e){return\"no\";}})()');
            return res === 'yes';
        } catch (e) {
            return false;
        }
    }

    function ensureSelectionMonitor() {
        if (!AUTO_SELECTION_REFRESH_ENABLED) return;
        if (selectionMonitor) return;
        selectionMonitor = setInterval(async () => {
            if (isCepSuspended()) return;
            const hasPlaced = await selectionHasPlacedItems();
            if (hasPlaced) {
                skipSelectionRefresh = true;
                stopCepRuntime();
                return;
            }
            skipSelectionRefresh = false;
            if (cepRuntimeSuspended) {
                startCepRuntime();
            }
        }, 750);
    }

    /**
     * Normalize angle to range -90 to 90 (acute angle from axis)
     * This gives the meaningful "tilt" for ductwork rotation override.
     * e.g., 170 -> -10 (10Â° from horizontal), 100 -> -80, 270 -> 0
     */
    function normalizeAngle(angle) {
        // First, reduce to 0-360 range
        angle = angle % 360;
        // Handle negative results from modulo
        if (angle < 0) angle += 360;
        // Convert to -180 to 180 range first
        if (angle > 180) angle -= 360;
        // Now convert to -90 to 90 range (acute angle)
        if (angle > 90) angle -= 180;
        else if (angle < -90) angle += 180;
        // Round to avoid floating point noise
        return Math.round(angle * 100) / 100;
    }

    function evalScript(script) {
        return new Promise((resolve, reject) => {
            csInterface.evalScript(script, (result) => {
                if (typeof result === 'undefined' || result === null) {
                    resolve('');
                } else {
                    resolve(result);
                }
            });
        });
    }

    /**
     * Run orthogonalization using async JavaScript-based Python communication
     * This is NON-BLOCKING - the UI remains responsive during Python processing
     */
    async function runAsyncOrtho() {
        console.log('[ASYNC-ORTHO] Starting async orthogonalization...');
        setProcessStatus('Starting async ortho...');

        // Check if PythonClient is available
        if (typeof window.PythonClient === 'undefined') {
            setProcessStatus('PythonClient not loaded - Node.js may not be enabled', true);
            return { success: false, error: 'PythonClient not available' };
        }

        // Check if server is running
        if (!window.PythonClient.isServerRunning()) {
            setProcessStatus('Python server not running. Starting it...', false);
            // Try to start via geometry bridge
            if (window.GeometryBridge && window.GeometryBridge.startServer) {
                const started = await window.GeometryBridge.startServer();
                if (!started) {
                    setProcessStatus('Failed to start Python server', true);
                    return { success: false, error: 'Server failed to start' };
                }
            } else {
                setProcessStatus('Cannot start Python server - run Start_Geometry_Server.bat', true);
                return { success: false, error: 'No way to start server' };
            }
        }

        try {
            // Step 1: Get paths from ExtendScript (quick sync call)
            setProcessStatus('Getting selected paths...');
            const pathsJson = await evalScript('getSelectedPathsForAsync()');
            const pathsData = JSON.parse(pathsJson);

            if (pathsData.error) {
                setProcessStatus('Error: ' + pathsData.error, true);
                return { success: false, error: pathsData.error };
            }

            if (!pathsData.paths || pathsData.paths.length === 0) {
                setProcessStatus('No paths to process', true);
                return { success: false, error: 'No paths' };
            }

            console.log('[ASYNC-ORTHO] Got ' + pathsData.paths.length + ' paths from ExtendScript');
            setProcessStatus('Processing ' + pathsData.paths.length + ' paths (async)...');

            // Step 2: Send to Python ASYNCHRONOUSLY (non-blocking!)
            const callbacks = {
                onProgress: (elapsed, max) => {
                    const pct = Math.round((elapsed / max) * 100);
                    setProcessStatus('Python processing... ' + pct + '%');
                },
                onComplete: (result) => {
                    console.log('[ASYNC-ORTHO] Python completed:', result);
                },
                onError: (err) => {
                    console.error('[ASYNC-ORTHO] Python error:', err);
                }
            };

            const result = await window.PythonClient.orthogonalizeAsync(
                pathsData.paths,
                5,  // snap threshold
                callbacks
            );

            console.log('[ASYNC-ORTHO] Python result:', result);

            // Step 3: Apply results back to Illustrator (quick sync call)
            setProcessStatus('Applying results...');
            const applyJson = await evalScript("applyAsyncOrthoResult('" +
                JSON.stringify(result).replace(/\\/g, '\\\\').replace(/'/g, "\\'") + "')");
            const applyResult = JSON.parse(applyJson);

            if (applyResult.error) {
                setProcessStatus('Error applying: ' + applyResult.error, true);
                return { success: false, error: applyResult.error };
            }

            setProcessStatus('Async ortho complete! Applied ' + applyResult.applied + ' paths');
            console.log('[ASYNC-ORTHO] Applied ' + applyResult.applied + ' paths, ' + applyResult.errors + ' errors');

            return { success: true, applied: applyResult.applied, stats: applyResult.stats };

        } catch (e) {
            console.error('[ASYNC-ORTHO] Error:', e);
            setProcessStatus('Async ortho failed: ' + e.message, true);
            // Cleanup stored paths on error
            await evalScript('clearAsyncPaths()');
            return { success: false, error: e.message };
        }
    }

    // Expose for testing from console
    window.runAsyncOrtho = runAsyncOrtho;

    function setProcessStatus(message, isError) {
        if (!processStatus) return;
        processStatus.textContent = message || '';
        processStatus.classList.toggle('error', !!isError);
    }

    function setIgnoreStatus(message, isError) {
        // ignoreStatus element removed - function disabled
    }

    function setRevertStatus(message, isError) {
        if (!revertStatus) return;
        revertStatus.textContent = message || '';
        revertStatus.classList.toggle('error', !!isError);
    }

    function setClearRotationMetadataStatus(message, isError) {
        if (!clearRotationMetadataStatus) return;
        clearRotationMetadataStatus.textContent = message || '';
        clearRotationMetadataStatus.classList.toggle('error', !!isError);
    }

    function setSelectionStatus(message, isError) {
        const el = document.getElementById('selection-status');
        const debugEl = document.getElementById('debug-status');

        if (el) {
            el.textContent = message || '';
            el.classList.toggle('error', !!isError);
            el.style.display = 'block'; // Force display
        }

        if (debugEl) {
            // Also show in footer for redundancy
            debugEl.textContent = (isError ? '[ERR] ' : '') + (message || '');
            debugEl.title = message || ''; // Tooltip for full text
        }
    }

    function setLayerStatus(message, isError) {
        if (!layerStatus) return;
        layerStatus.textContent = message || '';
        layerStatus.classList.toggle('error', !!isError);
    }

    function setImportStatus(message, isError) {
        if (!importStatus) return;
        importStatus.textContent = message || '';
        importStatus.classList.toggle('error', !!isError);
    }

    function setExportStatus(message, isError) {
        if (!exportStatus) return;
        exportStatus.textContent = message || '';
        exportStatus.classList.toggle('error', !!isError);
    }

    function normaliseResult(value) {
        if (!value) return { ok: true, value: '' };
        if (typeof value === 'string' && value.indexOf('ERROR:') === 0) {
            return { ok: false, value: value.substring(6) };
        }
        return { ok: true, value: value };
    }

    function parseBridgeJsonResult(value) {
        const normalized = normaliseResult(value);
        if (!normalized.ok) {
            return { ok: false, message: normalized.value || 'Bridge error.' };
        }
        if (typeof normalized.value === 'object' && normalized.value) {
            if (typeof normalized.value.ok !== 'boolean') {
                normalized.value.ok = true;
            }
            return normalized.value;
        }
        if (typeof normalized.value === 'string' && normalized.value) {
            try {
                const parsed = JSON.parse(normalized.value);
                if (parsed && typeof parsed.ok !== 'boolean') {
                    parsed.ok = true;
                }
                return parsed || { ok: true, message: normalized.value };
            } catch (e) {
                return { ok: true, message: normalized.value, value: normalized.value };
            }
        }
        return { ok: true, message: '' };
    }

    function setEmorySelectionStatus(message, isError) {
        if (!emorySelectionStatus) return;
        emorySelectionStatus.textContent = message || '';
        emorySelectionStatus.classList.toggle('error', !!isError);
    }

    function setEmoryWidthStatus(message, isError) {
        if (!emoryWidthStatus) return;
        emoryWidthStatus.textContent = message || '';
        emoryWidthStatus.classList.toggle('error', !!isError);
    }

    function getEmoryWidthSliderConfig(width) {
        const normalized = Math.max(0.25, Number(width) || 0.25);
        let step = 0.1;
        if (normalized >= 40) {
            step = 0.25;
        }
        if (normalized >= 120) {
            step = 0.5;
        }
        return {
            min: 0.25,
            max: Math.max(240, normalized * 4, normalized + 80),
            step: step
        };
    }

    function syncEmoryWidthControls(width, preserveInputValue) {
        if (!emoryWidthSlider || !emoryWidthInput || !isFinite(width) || width <= 0) return;
        const normalized = Math.max(0.25, width);
        const sliderConfig = getEmoryWidthSliderConfig(normalized);
        const inputMax = Math.max(240, Math.ceil(sliderConfig.max * 2));
        emoryWidthSlider.min = String(sliderConfig.min);
        emoryWidthSlider.max = String(sliderConfig.max);
        emoryWidthSlider.step = String(sliderConfig.step);
        emoryWidthInput.min = '0.25';
        emoryWidthInput.step = String(sliderConfig.step);
        emoryWidthInput.max = String(inputMax);
        emoryWidthSlider.value = String(normalized);
        if (!preserveInputValue) {
            emoryWidthInput.value = normalized.toFixed(2).replace(/\.00$/, '');
        }
    }

    function flushPendingEmoryWidthRefresh() {
        if (!emoryWidthRefreshPending) return;
        emoryWidthRefreshPending = false;
        refreshEmorySelectionState(true).catch(function () {});
    }

    function finishEmoryWidthDrag(commitFinalWidth) {
        if (!emoryWidthDragActive) return;
        emoryWidthDragActive = false;
        if (emoryWidthApplyTimer) {
            clearTimeout(emoryWidthApplyTimer);
            emoryWidthApplyTimer = null;
        }

        const numericWidth = emoryWidthSlider ? Number(emoryWidthSlider.value) : NaN;
        if (commitFinalWidth && isFinite(numericWidth) && numericWidth > 0) {
            queueEmoryWidthApply(numericWidth, true);
            return;
        }

        flushPendingEmoryWidthRefresh();
    }

    function setEmoryControlsEnabled(enabled) {
        if (setEmoryStartBtn) setEmoryStartBtn.disabled = !enabled;
        if (emoryWidthSlider) emoryWidthSlider.disabled = !enabled;
        if (emoryWidthInput) emoryWidthInput.disabled = !enabled;
    }

    async function refreshEmorySelectionState(force) {
        if (!setEmoryStartBtn || !emoryWidthSlider || !emoryWidthInput) return;
        if (isCepSuspended()) return;
        if (emoryWidthRefreshInFlight) return;
        if (!force) {
            if (emoryWidthApplyInFlight || emoryWidthDragActive) return;
            if (document.activeElement === emoryWidthInput) return;
        }

        emoryWidthRefreshInFlight = true;
        try {
            await ensureBridgeLoaded();
            const state = parseBridgeJsonResult(await evalScript('MDUX_cppGetSelectedEmorySegmentState()'));
            emorySelectionState = state;

            if (!state || state.ok === false) {
                setEmoryControlsEnabled(false);
                setEmorySelectionStatus('');
                setEmoryWidthStatus((state && state.message) ? state.message : 'Unable to read Emory selection.', true);
                return;
            }

            if (!state.available) {
                setEmoryControlsEnabled(false);
                if (state.reason === 'multiple-runs') {
                    setEmorySelectionStatus('Select segments from only one Emory run at a time.', true);
                } else if (state.reason === 'no-segment-selection') {
                    setEmorySelectionStatus('Select a generated Emory duct segment to edit width.', false);
                } else {
                    setEmorySelectionStatus('');
                }
                setEmoryWidthStatus('');
                return;
            }

            const selectedCount = Number(state.selectedCount || 0);
            const runCount = Number(state.runCount || 0);
            const segmentCount = Number(state.segmentCount || 0);
            const startSegmentIndex = Number(state.startSegmentIndex || 0);
            const selectedSegmentIndex = Number(state.selectedSegmentIndex);
            const startLabel = 'Start: segment ' + (startSegmentIndex + 1) + ' of ' + segmentCount;

            if (runCount > 1) {
                let multiRunMessage = selectedCount + ' segments selected across ' + runCount + ' runs';
                if (state.mixedWidths) {
                    multiRunMessage += ' | Mixed widths selected';
                }
                setEmorySelectionStatus(multiRunMessage, false);
            } else if (selectedCount === 1 && isFinite(selectedSegmentIndex) && selectedSegmentIndex >= 0) {
                const selectedLabel = 'Selected: segment ' + (selectedSegmentIndex + 1) + ' of ' + segmentCount;
                setEmorySelectionStatus(selectedLabel + ' | ' + startLabel, false);
            } else {
                let msg = selectedCount + ' segments selected | ' + startLabel;
                if (state.mixedWidths) {
                    msg += ' | Mixed widths selected';
                }
                setEmorySelectionStatus(msg, false);
            }

            const canApplyWidth = !!state.canApplyWidth;
            if (setEmoryStartBtn) {
                setEmoryStartBtn.disabled = selectedCount !== 1;
            }
            if (emoryWidthSlider) emoryWidthSlider.disabled = !canApplyWidth;
            if (emoryWidthInput) emoryWidthInput.disabled = !canApplyWidth;

            if (canApplyWidth && runCount > 1) {
                const referenceWidth = Number(state.sharedWidth || state.referenceWidth || 0);
                if (referenceWidth > 0) {
                    syncEmoryWidthControls(referenceWidth, false);
                }
                setEmoryWidthStatus(state.mixedWidths
                    ? 'Mixed widths selected across multiple runs. Dragging the slider will set all selected segments to one width and cascade each run away from its marked start.'
                    : 'Dragging the slider will set all selected segments to one width and cascade each run away from its marked start.', false);
            } else if (canApplyWidth && selectedCount === 1) {
                const selectedWidth = Number(state.selectedWidth || 0);
                if (selectedWidth > 0) {
                    syncEmoryWidthControls(selectedWidth, false);
                    emoryWidthLastApplied = selectedWidth;
                }
                setEmoryWidthStatus(state.isStartSegment
                    ? 'Width changes will cascade away from this start segment in both directions.'
                    : 'Width changes will cascade away from the marked start on this branch.', false);
            } else if (canApplyWidth && selectedCount > 1) {
                const referenceWidth = Number(state.sharedWidth || state.referenceWidth || 0);
                if (referenceWidth > 0) {
                    syncEmoryWidthControls(referenceWidth, false);
                }
                setEmoryWidthStatus(state.mixedWidths
                    ? 'Mixed widths selected. Dragging the slider will set all selected segments to one width and cascade each selected branch outward.'
                    : 'Dragging the slider will set all selected segments to one width and cascade each selected branch outward.', false);
            } else {
                setEmoryWidthStatus('', false);
            }
        } catch (e) {
            setEmoryControlsEnabled(false);
            setEmorySelectionStatus('');
            setEmoryWidthStatus('Unable to refresh Emory selection state: ' + e.message, true);
        } finally {
            emoryWidthRefreshInFlight = false;
        }
    }

    async function applySelectedEmorySegmentWidth(width, options) {
        const opts = options || {};
        const numericWidth = Number(width);
        if (!isFinite(numericWidth) || numericWidth <= 0) {
            setEmoryWidthStatus('Width must be greater than zero.', true);
            return;
        }
        if (emoryWidthApplyInFlight) {
            return;
        }

        emoryWidthApplyInFlight = true;
        try {
            await ensureBridgeLoaded();
            if (!opts.silent) {
                setEmoryWidthStatus('Applying width...', false);
            }
            const payloadWidth = Math.max(0.25, numericWidth);
            const result = parseBridgeJsonResult(await evalScript('MDUX_cppApplySelectedEmorySegmentWidth(' + payloadWidth + ')'));
            if (!result || result.ok === false) {
                setEmoryWidthStatus((result && result.message) ? result.message : 'Failed to apply width.', true);
                return;
            }
            emoryWidthLastApplied = payloadWidth;
            setEmoryWidthStatus(result.message || 'Width updated.', false);
            scheduleSkipOrthoRefresh();
            if (emoryWidthDragActive && !opts.forceRefresh) {
                emoryWidthRefreshPending = true;
            } else {
                emoryWidthRefreshPending = false;
                await refreshEmorySelectionState(true);
            }
        } catch (e) {
            setEmoryWidthStatus('Failed to apply width: ' + e.message, true);
        } finally {
            emoryWidthApplyInFlight = false;
            if (!emoryWidthDragActive) {
                flushPendingEmoryWidthRefresh();
            }
        }
    }

    function queueEmoryWidthApply(width, immediate) {
        if (emoryWidthApplyTimer) {
            clearTimeout(emoryWidthApplyTimer);
            emoryWidthApplyTimer = null;
        }
        const fn = function () {
            applySelectedEmorySegmentWidth(width, { silent: false }).catch(function () {});
        };
        if (immediate) {
            fn();
            return;
        }
        emoryWidthApplyTimer = setTimeout(fn, 90);
    }

    function escapeForExtendScript(str) {
        return str.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
    }

    function buildProcessPlacedPayload(options) {
        const parts = ['action=process-placed-api'];
        if (typeof options.skipOrtho === 'boolean') {
            parts.push('skipOrtho=' + (options.skipOrtho ? '1' : '0'));
        }
        if (typeof options.skipAllBranchSegments === 'boolean') {
            parts.push('skipAllBranchSegments=' + (options.skipAllBranchSegments ? '1' : '0'));
        }
        if (typeof options.skipFinalRegisterSegment === 'boolean') {
            parts.push('skipFinalRegisterSegment=' + (options.skipFinalRegisterSegment ? '1' : '0'));
        }
        if (typeof options.skipRegisterRotation === 'boolean') {
            parts.push('skipRegisterRotation=' + (options.skipRegisterRotation ? '1' : '0'));
        }
        if (typeof options.enableRegisterCarve === 'boolean') {
            parts.push('enableRegisterCarve=' + (options.enableRegisterCarve ? '1' : '0'));
        }
        if (typeof options.enableOverlapCarve === 'boolean') {
            parts.push('enableOverlapCarve=' + (options.enableOverlapCarve ? '1' : '0'));
        }
        return parts.join(';');
    }

    async function ensureBridgeLoaded() {
        htmlLog('[BRIDGE] ensureBridgeLoaded enter');
        // DEV MODE: always reload JSX if enabled
        const devMode = devModeOption && devModeOption.checked;
        if (devMode) {
            htmlLog('[BRIDGE] devMode reload');
            await forceReloadScripts();
            return;
        }
        // Normal mode: only load once per session
        if (bridgeReloaded) {
            htmlLog('[BRIDGE] already loaded');
            return;
        }
        const escapedPath = escapeForExtendScript(bridgePath);
        const loadScript = '(function(){' +
            'delete $.global.MDUX_JSX_FOLDER;' +  // Force clear stale cached folder path
            'delete $.global.MDUX;' +  // Force clear stale MDUX namespace to ensure fresh initialization
            '$.global.MDUX_LAST_BRIDGE_PATH = "' + escapedPath + '";' +
            'try { $.evalFile("' + escapedPath + '"); return "OK"; } ' +
            'catch (e) { $.global.MDUX_LAST_BRIDGE_ERROR = e.toString(); return "ERROR:" + e; }' +
            '})()';
        const loadResult = await evalScript(loadScript);
        if (typeof loadResult === 'string' && loadResult.indexOf('ERROR:') === 0) {
            const msg = loadResult.substring(6);
            debugStatus.textContent = 'Bridge load failed: ' + msg;
            htmlLog('[BRIDGE] load failed: ' + msg);
            throw new Error(msg);
        }
        bridgeReloaded = true;
        debugStatus.textContent = 'Bridge ready: ' + bridgePath.replace(/\\/g, '/');
        htmlLog('[BRIDGE] load OK');
    }

    async function forceReloadScripts() {
        bridgeReloaded = false;
        const root = csInterface.getSystemPath(CSInterface.SystemPath.EXTENSION).replace(/\\/g, '/');
        const bridge = escapeForExtendScript(root + '/jsx/panel-bridge.jsx');
        const gapTools = escapeForExtendScript(root + '/jsx/gap-tools.jsx');
        const clearScript = '(function(){' +
            'var cleared = [];' +
            'for (var key in $.global) {' +
            '  if (key.indexOf("MDUX") === 0) {' +
            '    delete $.global[key];' +
            '    cleared.push(key);' +
            '  }' +
            '}' +
            'return cleared.length;' +
            '})()';
        await evalScript(clearScript);
        const loadScript = '(function(){' +
            'try {' +
            ' $.evalFile("' + gapTools + '");' +
            ' $.evalFile("' + bridge + '");' +
            ' return "OK";' +
            ' } catch (e) {' +
            ' return "ERROR:" + e;' +
            ' }' +
            '})()';
        const result = await evalScript(loadScript);
        if (typeof result === 'string' && result.indexOf('ERROR:') === 0) {
            console.error('Script reload failed:', result);
            debugStatus.textContent = 'Script reload failed: ' + result.substring(6);
        } else {
            bridgeReloaded = true;
            debugStatus.textContent = 'JSX reloaded @ ' + new Date().toLocaleTimeString();
        }
        return result;
    }

    function scheduleSkipOrthoRefresh() {
        console.log('[JS] scheduleSkipOrthoRefresh called');
        // PERF: Removed debug logging to prevent blocking ExtendScript calls
        if (!AUTO_SELECTION_REFRESH_ENABLED) return;
        if (isCepSuspended()) return;
        if (skipOrthoRefreshTimer) clearTimeout(skipOrthoRefreshTimer);
        skipOrthoRefreshTimer = setTimeout(async () => {
            console.log('[JS] scheduleSkipOrthoRefresh timeout fired, calling refresh functions');
            // PERF: Removed debug logging to prevent blocking ExtendScript calls
            if (skipSelectionRefresh) return;
            if (AUTO_SELECTION_REFRESH_MODE === 'skip-ortho') {
                refreshSkipOrthoState().catch(() => { });
                return;
            }
            if (AUTO_SELECTION_REFRESH_MODE === 'skip-ortho+rotation') {
                refreshSkipOrthoState().catch(() => { });
                refreshRotationOverrideState().catch(() => { });
                return;
            }
            refreshSkipOrthoState().catch(() => { });
            refreshRotationOverrideState().catch(() => { });
            refreshSelectionTransformState().catch(() => { });
        }, 150);
    }

    function scheduleEmorySelectionRefresh(force) {
        if (!AUTO_SELECTION_REFRESH_ENABLED) return;
        if (isCepSuspended()) return;
        if (emorySelectionRefreshTimer) {
            clearTimeout(emorySelectionRefreshTimer);
            emorySelectionRefreshTimer = null;
        }
        emorySelectionRefreshTimer = setTimeout(function () {
            emorySelectionRefreshTimer = null;
            if (skipSelectionRefresh) return;
            refreshEmorySelectionState(!!force).catch(function () { });
        }, force ? 0 : 20);
    }

    async function updateSkipSelectionRefresh() {
        try {
            const res = await evalScript('(function(){try{var s=app.selection;if(!s||s.length===0)return\"no\";function hasPlaced(item){if(!item)return false;if(item.typename===\"PlacedItem\")return true;if(item.typename===\"GroupItem\"&&item.pageItems){for(var i=0;i<item.pageItems.length;i++){if(hasPlaced(item.pageItems[i]))return true;}}return false;}if(s.length===undefined&&s.typename){return hasPlaced(s)?\"yes\":\"no\";}for(var i=0;i<s.length;i++){if(hasPlaced(s[i]))return\"yes\";}return\"no\";}catch(e){return\"no\";}})()');
            skipSelectionRefresh = res === 'yes';
        } catch (e) {
            skipSelectionRefresh = false;
        }
        return skipSelectionRefresh;
    }

    async function refreshRotationOverrideState() {
        if (!rotationInput) return;
        if (isCepSuspended()) return;

        // NEVER update if user is typing in the input
        var isFocused = document.activeElement === rotationInput;
        if (isFocused) return;

        try {
            await ensureBridgeLoaded();
        } catch (e) {
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_rotationStateBridge()'));
        if (!result.ok) {
            if (result.value) {
                debugStatus.textContent = 'Rotation state error: ' + result.value;
            }
            return;
        }
        var summary = null;
        try { summary = result.value ? JSON.parse(result.value) : null; } catch (eParse) { summary = null; }
        if (!summary) return;

        var nextPlaceholder = 'Leave blank to skip';

        if (summary.reason === 'no-document') {
            rotationInput.value = '';
            rotationInput.dataset.autoValue = '';
            rotationInput.dataset.multi = 'false';
            rotationInput.placeholder = 'No document';
            return;
        }

        if (summary.reason === 'no-selection') {
            rotationInput.value = '';
            rotationInput.dataset.autoValue = '';
            rotationInput.dataset.multi = 'false';
            rotationInput.placeholder = nextPlaceholder;
            return;
        }

        if (!summary.available) {
            return;
        }

        var formatted = summary.formatted || '';
        // Normalize the angle if it's a valid number
        var numVal = parseFloat(formatted);
        if (!isNaN(numVal)) {
            var normalized = normalizeAngle(numVal);
            formatted = normalized.toString();
        }
        rotationInput.value = formatted;
        rotationInput.dataset.autoValue = formatted;

        if (summary.count > 1) {
            rotationInput.dataset.multi = 'true';
            nextPlaceholder = 'Mixed rotations';
        } else {
            rotationInput.dataset.multi = 'false';
        }

        rotationInput.placeholder = nextPlaceholder;
    }

    async function refreshDebugLoggingState() {
        if (!debugLoggingOption) return;
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_getDebugEnabledBridge()'));
        if (!result.ok) return;
        debugLoggingOption.checked = String(result.value).toLowerCase() === 'true';
    }

    async function refreshYieldToUiState() {
        if (!yieldToUiOption) return;
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_getYieldToUIBridge()'));
        if (!result.ok) return;
        yieldToUiOption.checked = String(result.value).toLowerCase() === 'true';
    }

    async function setDebugLoggingState(enabled) {
        if (!debugLoggingOption) return;
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_setDebugEnabledBridge(' + (enabled ? 'true' : 'false') + ')'));
        if (!result.ok && debugStatus) {
            debugStatus.textContent = 'Debug toggle error: ' + result.value;
        }
    }

    async function setYieldToUiState(enabled) {
        if (!yieldToUiOption) return;
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_setYieldToUIBridge(' + (enabled ? 'true' : 'false') + ')'));
        if (!result.ok && debugStatus) {
            debugStatus.textContent = 'Yield toggle error: ' + result.value;
        }
    }

    async function refreshSkipOrthoState() {
        if (!skipOrthoOption) return;
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            skipOrthoOption.indeterminate = false;
            skipOrthoOption.checked = false;
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_skipOrthoStateBridge()'));
        if (!result.ok) {
            skipOrthoOption.indeterminate = false;
            skipOrthoOption.checked = false;
            debugStatus.textContent = 'Skip-ortho state error: ' + result.value;
            return;
        }
        var state = null;
        try { state = result.value ? JSON.parse(result.value) : null; } catch (eParse) { state = null; }
        if (!state || state.available === false) {
            skipOrthoOption.indeterminate = false;
            skipOrthoOption.checked = false;
            skipOrthoOption.disabled = !!(state && state.reason === 'no-document');
            return;
        }
        skipOrthoOption.disabled = false;
        if (state.mixed) {
            skipOrthoOption.indeterminate = true;
            skipOrthoOption.checked = false;
        } else if (state.hasNote) {
            skipOrthoOption.indeterminate = false;
            skipOrthoOption.checked = true;
        } else {
            skipOrthoOption.indeterminate = false;
            skipOrthoOption.checked = false;
        }
    }

    async function handleProcessClick() {
        processBtn.disabled = true;
        revertBtn.disabled = true;
        setProcessStatus('Preparing ductwork runâ€¦');
        setRevertStatus('');

        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setProcessStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            processBtn.disabled = false;
            revertBtn.disabled = false;
            return;
        }

        // PYTHON MODE: Keep Python ENABLED - it's much faster than ExtendScript
        // The $.sleep() blocking is minimal compared to slow ExtendScript operations
        console.log('[PROCESS] Python mode: keeping Python enabled for fast geometry operations');

        // Wrap entire processing in try/finally to ensure buttons always get re-enabled
        try {
        let rotationValue = null;
        const rotationText = rotationInput.value.trim();
        const autoValue = (rotationInput.dataset.autoValue || '').trim();
        if (rotationText) {
            const isNumeric = /^-?\d+(\.\d+)?$/.test(rotationText);
            if (!isNumeric) {
                if (rotationText !== autoValue) {
                    setProcessStatus('Rotation override must be a valid number.', true);
                    processBtn.disabled = false;
                    revertBtn.disabled = false;
                    return;
                }
            } else {
                rotationValue = parseFloat(rotationText);
            }
        }

        // Use new visible checkboxes for process options (all off by default)
        const processRotateRegistersOption = document.getElementById('process-rotate-registers-option');
        const processCarveRegistersOption = document.getElementById('process-carve-registers-option');
        const processCarveOverlapsOption = document.getElementById('process-carve-overlaps-option');
        const options = {
            action: 'process',
            skipAllBranchSegments: !!skipAllBranchesOption.checked,
            skipFinalRegisterSegment: !!skipFinalOption.checked,
            // skipRegisterRotation is TRUE to skip rotation (checkbox NOT checked = skip)
            skipRegisterRotation: !(processRotateRegistersOption && processRotateRegistersOption.checked),
            enableRegisterCarve: !!(processCarveRegistersOption && processCarveRegistersOption.checked),
            enableOverlapCarve: !!(processCarveOverlapsOption && processCarveOverlapsOption.checked)
        };

        if (!skipOrthoOption.indeterminate) {
            options.skipOrtho = !!skipOrthoOption.checked;
        }
        if (rotationValue !== null) {
            options.rotationOverride = rotationValue;
        }

        const payload = JSON.stringify(options).replace(/\\/g, '\\\\').replace(/'/g, "\\'");
        const prep = normaliseResult(await evalScript("MDUX_prepareProcessBridge('" + payload + "')"));
        if (!prep.ok) {
            setProcessStatus('Error: ' + prep.value, true);
            processBtn.disabled = false;
            revertBtn.disabled = false;
            return;
        }

        setProcessStatus('Running ductwork script (Python accelerated)...');
        const result = normaliseResult(await evalScript('MDUX_runMagicDuctwork()'));

        if (result.ok) {
            setProcessStatus('Emory Ductwork completed.');
            debugStatus.textContent = 'Process completed';
        } else {
            setProcessStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Process failed: ' + result.value;
        }
        scheduleSkipOrthoRefresh();

        // Write debug log to file after processing (clipboard copy disabled)
        if (!debugLoggingOption || debugLoggingOption.checked) {
            try {
                console.log('[PANEL] Calling MDUX_getDebugLog()...');
                const debugLog = await evalScript('MDUX_getDebugLog()');
                console.log('[PANEL] MDUX_getDebugLog returned ' + (debugLog ? debugLog.length : 0) + ' chars');
                // NOTE: Clipboard copy disabled per user request
                // Debug log is written to file by MDUX_getDebugLog()
            } catch (clipErr) {
                console.log('[PANEL] Failed to get debug log:', clipErr);
            }
        }

        } catch (processingError) {
            // Catch any unexpected errors during processing
            console.error('[PANEL] Processing error:', processingError);
            setProcessStatus('Unexpected error: ' + (processingError.message || processingError), true);
        } finally {
            // ALWAYS re-enable buttons, even if an error occurred
            processBtn.disabled = false;
            revertBtn.disabled = false;
        }
    }

    async function handleProcessPlacedApiClick() {
        if (!processPlacedBtn) return;
        if (!EMORY_PROCESS_READY) {
            setProcessStatus('Emory geometry engine scaffolded. Shared Illustrator tools are copied over, but the Emory duct generation port is not wired yet.', true);
            if (debugStatus) {
                debugStatus.textContent = 'Emory scaffold only';
            }
            return;
        }
        processPlacedBtn.disabled = true;
        if (debugStatus) {
            debugStatus.textContent = 'Process Ductwork: starting';
        }
        panelFileLog('[PANEL] ProcessPlaced click');
        htmlLog('[PANEL] ProcessPlaced click');
        setProcessStatus('Processing ductwork (Placed)...');

        try {
            htmlLog('[PANEL] ensureBridgeLoaded...');
            await ensureBridgeLoaded();
            htmlLog('[PANEL] ensureBridgeLoaded OK');
        } catch (e) {
            setProcessStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            if (debugStatus) {
                debugStatus.textContent = 'Process Ductwork: bridge load failed';
            }
            htmlLog('[PANEL] ensureBridgeLoaded error: ' + (e && e.message ? e.message : e));
            processPlacedBtn.disabled = false;
            return;
        }

        try {
            const processRotateRegistersOption = document.getElementById('process-rotate-registers-option');
            const processCarveRegistersOption = document.getElementById('process-carve-registers-option');
            const processCarveOverlapsOption = document.getElementById('process-carve-overlaps-option');
            const skipOrtho = processSkipOrthoOption ? !!processSkipOrthoOption.checked : (skipOrthoOption ? !!skipOrthoOption.checked : false);
            const skipAllBranchSegments = processSkipAllBranchesOption ? !!processSkipAllBranchesOption.checked : (skipAllBranchesOption ? !!skipAllBranchesOption.checked : false);
            const skipFinalRegisterSegment = processSkipFinalOption ? !!processSkipFinalOption.checked : (skipFinalOption ? !!skipFinalOption.checked : false);
            const payload = buildProcessPlacedPayload({
                skipOrtho,
                skipAllBranchSegments,
                skipFinalRegisterSegment,
                skipRegisterRotation: !(processRotateRegistersOption && processRotateRegistersOption.checked),
                enableRegisterCarve: !!(processCarveRegistersOption && processCarveRegistersOption.checked),
                enableOverlapCarve: !!(processCarveOverlapsOption && processCarveOverlapsOption.checked)
            });
            const escaped = escapeForExtendScript(payload);
            htmlLog('[PANEL] calling MDUX_cppProcessPlacedApi');
            const result = normaliseResult(await evalScript('MDUX_cppProcessPlacedApi("' + escaped + '")'));
            htmlLog('[PANEL] MDUX_cppProcessPlacedApi result ok=' + (result && result.ok ? 'true' : 'false'));
            if (result.ok) {
                setProcessStatus('Ready.');
                debugStatus.textContent = 'Process placed completed';
            } else {
                setProcessStatus('Error: ' + result.value, true);
                debugStatus.textContent = 'Process placed failed: ' + result.value;
            }
        } catch (e) {
            setProcessStatus('Error: ' + e.message, true);
            if (debugStatus) {
                debugStatus.textContent = 'Process Ductwork: exception';
            }
        } finally {
            processPlacedBtn.disabled = false;
        }
    }

    async function handleProcessEmoryClick() {
        return handleProcessPlacedApiClick();
    }

    async function handleToggleConnectorStyleClick() {
        if (!toggleConnectorStyleBtn) return;
        toggleConnectorStyleBtn.disabled = true;
        setProcessStatus('Toggling selected connector style...');
        try {
            await ensureBridgeLoaded();
            const result = parseBridgeJsonResult(await evalScript('MDUX_cppToggleSelectedEmoryConnector()'));
            if (result && result.ok !== false) {
                setProcessStatus(result.message || 'Connector style updated.');
            } else {
                setProcessStatus('Error: ' + (result && result.message ? result.message : 'Unable to update connector style.'), true);
            }
        } catch (e) {
            setProcessStatus('Error: ' + e.message, true);
        } finally {
            toggleConnectorStyleBtn.disabled = false;
            scheduleSkipOrthoRefresh();
        }
    }

    async function handleMarkOverlapSeparateClick() {
        if (!markOverlapSeparateBtn) return;
        markOverlapSeparateBtn.disabled = true;
        setProcessStatus('Marking selected connector as separate runs...');
        try {
            await ensureBridgeLoaded();
            const result = parseBridgeJsonResult(await evalScript('MDUX_cppMarkSelectedEmoryConnectorSeparate()'));
            if (result && result.ok !== false) {
                setProcessStatus(result.message || 'Connector marked as separate runs.');
            } else {
                setProcessStatus('Error: ' + (result && result.message ? result.message : 'Unable to mark connector as separate.'), true);
            }
        } catch (e) {
            setProcessStatus('Error: ' + e.message, true);
        } finally {
            markOverlapSeparateBtn.disabled = false;
            scheduleEmorySelectionRefresh(true);
            scheduleSkipOrthoRefresh();
        }
    }

    async function handleSetEmoryStartClick() {
        if (!setEmoryStartBtn) return;
        setEmoryStartBtn.disabled = true;
        setEmoryWidthStatus('Marking selected segment as the cascade start...', false);
        try {
            await ensureBridgeLoaded();
            const result = parseBridgeJsonResult(await evalScript('MDUX_cppSetSelectedEmoryStartSegment()'));
            if (result && result.ok !== false) {
                setEmoryWidthStatus(result.message || 'Cascade start updated.', false);
            } else {
                setEmoryWidthStatus((result && result.message) ? result.message : 'Unable to mark the cascade start.', true);
            }
        } catch (e) {
            setEmoryWidthStatus('Unable to mark the cascade start: ' + e.message, true);
        } finally {
            scheduleEmorySelectionRefresh(true);
            scheduleSkipOrthoRefresh();
            setEmoryStartBtn.disabled = false;
        }
    }

    async function rotateSelection(angle) {
        if (!isFinite(angle)) {
            setSelectionStatus('Rotation value must be numeric.', true);
            return;
        }
        setSelectionStatus('Rotating selection ' + angle + 'Â°â€¦');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setSelectionStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_cppQuickRotate(' + angle + ')'));
        if (!result.ok) {
            setSelectionStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Rotate failed: ' + result.value;
            return;
        }
        setSelectionStatus(result.value || 'Rotation complete.');
        debugStatus.textContent = 'Rotate result: ' + (result.value || 'OK');
        scheduleSkipOrthoRefresh();
    }

    async function applyScale(percent) {
        setSelectionStatus('Scaling selection to ' + percent + '%â€¦');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setSelectionStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            return;
        }

        // First, reset graphic styles on ductwork lines to ensure proper scaling
        try {
            await evalScript('MDUX.resetDuctworkLineStyles()');
        } catch (e) {
            // Non-fatal - continue with scaling even if reset fails
        }

        const result = normaliseResult(await evalScript('MDUX_scaleSelectionBridge(' + percent + ')'));
        if (!result.ok) {
            setSelectionStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Scale failed: ' + result.value;
            return;
        }
        let stats = null;
        try { stats = result.value ? JSON.parse(result.value) : null; } catch (e) { stats = null; }
        if (stats && typeof stats.total === 'number') {
            if (stats.total === 0) {
                setSelectionStatus('Select ductwork, registers, units, or thermostats to scale.', true);
                debugStatus.textContent = 'Scale: no eligible items';
            } else if (stats.scaled > 0) {
                setSelectionStatus('Scaled ' + stats.scaled + ' item(s).' + (stats.skipped ? ' Skipped ' + stats.skipped + '.' : ''));
                debugStatus.textContent = 'Scale result: scaled ' + stats.scaled + ', skipped ' + (stats.skipped || 0);
            } else {
                setSelectionStatus('No eligible items were scaled.', true);
                debugStatus.textContent = 'Scale: no items scaled';
            }
        } else {
            setSelectionStatus(result.value || 'Scaling complete.');
            debugStatus.textContent = 'Scale result: ' + (result.value || 'OK');
        }
        scheduleSkipOrthoRefresh();
    }

    async function resetScale() {
        setSelectionStatus('Resetting selection scaleâ€¦');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setSelectionStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_resetScaleBridge()'));
        if (!result.ok) {
            setSelectionStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Reset scale failed: ' + result.value;
            return;
        }
        let stats = null;
        try { stats = result.value ? JSON.parse(result.value) : null; } catch (e) { stats = null; }
        if (stats && typeof stats.total === 'number') {
            if (stats.total === 0) {
                setSelectionStatus('No eligible items selected to reset.', true);
                debugStatus.textContent = 'Reset scale: no eligible items';
            } else if (stats.reset > 0) {
                setSelectionStatus('Restored scale on ' + stats.reset + ' item(s).' + (stats.skipped ? ' Skipped ' + stats.skipped + '.' : ''));
                debugStatus.textContent = 'Reset scale result: reset ' + stats.reset + ', skipped ' + (stats.skipped || 0);
            } else {
                setSelectionStatus('Unable to restore scale for the selection.', true);
                debugStatus.textContent = 'Reset scale: nothing reset';
            }
        } else {
            setSelectionStatus(result.value || 'Scale reset complete.');
            debugStatus.textContent = 'Reset scale result: ' + (result.value || 'OK');
        }
        scheduleSkipOrthoRefresh();
    }

    function onScaleInput() {
        if (!scaleSlider || !scaleLabel) return;
        var value = parseFloat(scaleSlider.value);
        if (!isFinite(value)) return;
        scaleLabel.textContent = value + '%';
        if (scaleDebounce) {
            clearTimeout(scaleDebounce);
        }
        scaleDebounce = setTimeout(function () {
            applyScale(value);
        }, 400);
    }

    async function isolate(action) {
        let cmd;
        let startMessage = 'Applying layer isolationâ€¦';
        if (action === 'parts') cmd = 'MDUX_isolatePartsBridge()';
        else if (action === 'lines') cmd = 'MDUX_isolateDuctworkBridge()';
        else if (action === 'unlock') cmd = 'MDUX_unlockDuctworkBridge()';
        else if (action === 'create') {
            cmd = 'MDUX_createLayersBridge()';
            startMessage = 'Ensuring standard ductwork layersâ€¦';
        } else return;

        setLayerStatus(startMessage);
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setLayerStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            return;
        }
        const result = normaliseResult(await evalScript(cmd));
        if (!result.ok) {
            setLayerStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Layer command failed: ' + result.value;
        } else {
            setLayerStatus(result.value || 'Layer operation completed.');
            debugStatus.textContent = 'Layer command result: ' + (result.value || 'OK');
        }
        scheduleSkipOrthoRefresh();
    }

    async function importGraphicStyles() {
        setImportStatus('Importing graphic stylesâ€¦');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setImportStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            debugStatus.textContent = 'Bridge load failed: ' + (e && e.message ? e.message : e);
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_importGraphicStylesBridge()'));
        if (!result.ok) {
            setImportStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Import styles failed: ' + result.value;
            return;
        }
        setImportStatus(result.value || 'Graphic styles imported.');
        debugStatus.textContent = 'Import styles result: ' + (result.value || 'OK');
    }

    async function applyIgnore() {
        // applyIgnoreBtn.disabled = true; // Button removed
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setIgnoreStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            // applyIgnoreBtn.disabled = false; // Button removed
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_applyIgnoreBridge()'));
        if (!result.ok) {
            setIgnoreStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Ignore apply failed: ' + result.value;
        } else {
            let stats = null;
            try { stats = result.value ? JSON.parse(result.value) : null; } catch (e) { stats = null; }
            if (stats && typeof stats.total === 'number') {
                if (stats.error) {
                    setIgnoreStatus(stats.error, true);
                    debugStatus.textContent = 'Ignore apply error: ' + stats.error;
                } else if (stats.total === 0) {
                    if (stats.reason === 'no-selection') {
                        setIgnoreStatus('Select duct lines or ductwork parts to add to ignored layer.', true);
                        debugStatus.textContent = 'Ignore apply: no selection';
                    } else {
                        setIgnoreStatus('No eligible items found in the selection.', true);
                        debugStatus.textContent = 'Ignore apply: no eligible items';
                    }
                } else if (stats.added > 0 || stats.moved > 0) {
                    const parts = [];
                    if (stats.added > 0) parts.push(stats.added + ' anchor(s)');
                    if (stats.moved > 0) parts.push(stats.moved + ' part(s) moved');
                    const skippedFragment = stats.skipped ? ' Skipped ' + stats.skipped + '.' : '';
                    setIgnoreStatus('Ignored ' + parts.join(', ') + '.' + skippedFragment);
                    debugStatus.textContent = 'Ignore apply result: added ' + (stats.added || 0) + ', moved ' + (stats.moved || 0) + ', skipped ' + (stats.skipped || 0);
                } else {
                    setIgnoreStatus('Unable to process selection.', true);
                    debugStatus.textContent = 'Ignore apply: unable to process';
                }
            } else {
                setIgnoreStatus(result.value || 'Ignore markers created.');
                debugStatus.textContent = 'Ignore apply result: ' + (result.value || 'OK');
            }
        }
        // applyIgnoreBtn.disabled = false; // Button removed
        scheduleSkipOrthoRefresh();
    }

    async function handleRevertPreOrtho() {
        revertBtn.disabled = true;
        setRevertStatus('Restoring original geometryâ€¦');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setRevertStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            revertBtn.disabled = false;
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_revertPreOrthoBridge()'));
        if (!result.ok) {
            setRevertStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Revert failed: ' + result.value;
        } else {
            var stats = null;
            try {
                stats = result.value ? JSON.parse(result.value) : null;
            } catch (e) {
                stats = null;
            }
            if (stats && typeof stats.total === 'number') {
                if (stats.total === 0) {
                    if (stats.reason === 'no-selection') {
                        setRevertStatus('Select one or more duct lines to revert.', true);
                        debugStatus.textContent = 'Revert: no selection';
                    } else {
                        setRevertStatus('No stored pre-orthogonalization data found on the selection.', true);
                        debugStatus.textContent = 'Revert: no stored data';
                    }
                } else if (stats.reverted > 0) {
                    setRevertStatus('Reverted ' + stats.reverted + ' of ' + stats.total + ' path(s) to their pre-orthogonalized state.');
                    debugStatus.textContent = 'Revert result: reverted ' + stats.reverted + ' of ' + stats.total;
                } else {
                    setRevertStatus('Selection contains no stored pre-orthogonalization data.', true);
                    debugStatus.textContent = 'Revert: nothing reverted';
                }
            } else {
                setRevertStatus(result.value || 'Revert completed.');
                debugStatus.textContent = 'Revert result: ' + (result.value || 'OK');
            }
        }
        revertBtn.disabled = false;
        scheduleSkipOrthoRefresh();
    }

    async function handleClearRotationMetadata() {
        clearRotationMetadataBtn.disabled = true;
        setClearRotationMetadataStatus('Clearing rotation metadataâ€¦');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setClearRotationMetadataStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            clearRotationMetadataBtn.disabled = false;
            return;
        }
        const result = normaliseResult(await evalScript('MDUX_clearRotationMetadataBridge()'));
        if (!result.ok) {
            setClearRotationMetadataStatus('Error: ' + result.value, true);
            debugStatus.textContent = 'Clear rotation metadata failed: ' + result.value;
        } else {
            var count = 0;
            try {
                count = parseInt(result.value, 10);
            } catch (e) {
                count = 0;
            }
            if (count === 0) {
                setClearRotationMetadataStatus('No paths selected or no rotation metadata found.', true);
                debugStatus.textContent = 'Clear rotation metadata: nothing cleared';
            } else {
                setClearRotationMetadataStatus('Cleared rotation metadata from ' + count + ' path(s).');
                debugStatus.textContent = 'Clear rotation metadata: cleared ' + count + ' paths';
            }
        }
        clearRotationMetadataBtn.disabled = false;
        scheduleSkipOrthoRefresh();
    }
    async function handleGetAngle() {
        setProcessStatus('Getting angle from selected line.');
        try {
            await ensureBridgeLoaded();
        } catch (e) {
            setProcessStatus('Bridge load failed: ' + (e && e.message ? e.message : e), true);
            return;
        }

        const resultStr = await evalScript('MDUX_cppGetSelectedLineAngleBridge()');
        const result = normaliseResult(resultStr);
        let data = null;
        if (result.ok) {
            try { data = JSON.parse(result.value); } catch (eParse) { data = null; }
        }

        const legacyStr = await evalScript('MDUX_getSelectedLineAngleBridge()');
        const legacyResult = normaliseResult(legacyStr);
        let legacy = null;
        if (legacyResult.ok) {
            try { legacy = JSON.parse(legacyResult.value); } catch (eLegacy) { legacy = null; }
        }

        const cppOk = !!(data && data.ok && typeof data.angle === 'number' && !isNaN(data.angle));
        const legacyOk = !!(legacy && legacy.ok && typeof legacy.angle === 'number' && !isNaN(legacy.angle));

        let finalAngle = null;
        let finalMessage = '';
        if (cppOk && legacyOk && Math.abs(data.angle - legacy.angle) > 0.1) {
            finalAngle = legacy.angle;
            finalMessage = legacy.message || 'Angle captured (legacy fallback).';
        } else if (cppOk) {
            finalAngle = data.angle;
            finalMessage = data.message || 'Angle captured.';
        } else if (legacyOk) {
            finalAngle = legacy.angle;
            finalMessage = legacy.message || 'Angle captured (legacy fallback).';
        }

        if (finalAngle != null) {
            const normalized = normalizeAngle(finalAngle);
            rotationInput.value = normalized.toString();
            rotationInput.dataset.autoValue = '';
            rotationInput.dataset.multi = 'false';
            try {
                await evalScript(`MDUX_cppSetRotationOverride(${normalized})`);
            } catch (e) {
                // Ignore bridge failures here; UI still reflects the value.
            }
            setProcessStatus(finalMessage || ('Angle set to ' + normalized + 'Â°'));
            debugStatus.textContent = 'Angle retrieved: ' + normalized + 'Â°';
        } else {
            const errorMsg = (data && data.message) || (legacy && legacy.message) || 'Failed to get angle';
            setProcessStatus(errorMsg, true);
            debugStatus.textContent = 'Get angle failed: ' + errorMsg;
        }
    }

    async function refreshDocScale() {
        try {
            if (!docScaleInput) return; // Element was removed from UI
            await ensureBridgeLoaded();
            const scale = await evalScript('MDUX_getDocumentScale()');
            if (scale && isFinite(parseFloat(scale))) {
                docScaleInput.value = scale;
            }
        } catch (e) {
            console.error("Failed to refresh doc scale:", e);
        }
    }

    async function handleSetDocScale100() {
        try {
            await ensureBridgeLoaded();
            const result = await evalScript('MDUX_setDocumentScale(100)');
            if (result === 'OK') {
                docScaleInput.value = '100';
                setSelectionStatus('Baseline set to 100%.');
            } else {
                setSelectionStatus('Error: ' + result, true);
            }
        } catch (e) {
            setSelectionStatus('Error: ' + e.message, true);
        }
    }

    async function handleApplyDocScale() {
        const targetPercent = parseFloat(docScaleInput.value);
        if (isNaN(targetPercent) || targetPercent <= 0) {
            setSelectionStatus('Enter a valid positive scale percentage.', true);
            return;
        }
        setSelectionStatus('Scaling entire document to ' + targetPercent + '%...');
        try {
            await ensureBridgeLoaded();
            const resultStr = await evalScript('MDUX_applyScaleToFullDocument(' + targetPercent + ')');
            const result = JSON.parse(resultStr);
            if (result.ok) {
                setSelectionStatus(result.message);
                if (result.stats) {
                    debugStatus.textContent = 'Doc scale: ' + result.stats.parts + ' parts, ' + result.stats.lines + ' lines';
                }
            } else {
                setSelectionStatus('Error: ' + result.message, true);
            }
        } catch (e) {
            setSelectionStatus('Error: ' + e.message, true);
        }
    }

    async function handleExport(type) {
        setExportStatus('Exporting...', false);
        console.log("Starting export for type: " + type);

        // Convert to uppercase to match export-utils.jsx expectations
        const exportType = type.toUpperCase();

        try {
            // Initial attempt (overwrite=false, version=null)
            let resultStr = await evalScript(`MDUX_performExport("${exportType}", false, null)`);
            console.log("Export result string: ", resultStr);

            if (!resultStr) {
                throw new Error("No response from Illustrator. Check debug.log.");
            }

            let result;
            try {
                result = JSON.parse(resultStr);
            } catch (e) {
                throw new Error("Invalid JSON response: " + resultStr);
            }

            if (!result.ok) {
                if (result.log) {
                    console.log("Export Error Log:\n" + result.log);
                }
                setExportStatus(result.message, true);
                return;
            }

            if (result.status === "CONFIRM_OVERWRITE") {
                // Ask user
                const shouldOverwrite = confirm(result.message || "Files already exist. Overwrite them?");

                if (shouldOverwrite) {
                    // Retry with overwrite=true
                    resultStr = await evalScript(`MDUX_performExport("${exportType}", true, null)`);
                    console.log("Overwrite result string: ", resultStr);
                    result = JSON.parse(resultStr);
                } else {
                    // Ask for version
                    const version = prompt("Enter version suffix (e.g. '1' for V1):");
                    if (version) {
                        // Retry with version
                        resultStr = await evalScript(`MDUX_performExport("${exportType}", false, "${version}")`);
                        console.log("Version result string: ", resultStr);
                        result = JSON.parse(resultStr);
                    } else {
                        setExportStatus("Export cancelled.", false);
                        return;
                    }
                }
            }

            if (result.ok) {
                if (result.log) {
                    console.log("Export Success Log:\n" + result.log);
                }
                setExportStatus(result.message, false);
            } else {
                if (result.log) {
                    console.log("Export Error Log:\n" + result.log);
                }
                setExportStatus(result.message, true);
            }

        } catch (e) {
            console.error("Export error:", e);
            setExportStatus("Error: " + e.message, true);
        }
    }

    // Transform Each State
    let teIsBusy = false;
    let teDragActive = false;
    let teTransformAppliedInDrag = false;
    let teNextPayload = null; // {scale, rotate, undoPrevious}
    let tePendingSliderSource = null;
    let teSliderCommitInFlight = false;

    // Track start values for the current drag session
    let teDragStartScale = 100;
    let teDragStartRotate = 0;
    let teScaleDirty = false;
    let teRotateDirty = false;
    let lastSelectionScale = 100;
    let lastSelectionRotation = 0;

    // Track committed values (where the slider was left after last drag)
    // We need this because the slider value is absolute (e.g. 110), but we need to calculate
    // the factor relative to the START of the drag.
    // Actually, we can just read the slider value on mousedown.

    async function processTransformQueue() {
        if (teIsBusy) return;
        if (!teNextPayload) return;

        teIsBusy = true;

        // Capture current payload
        const payload = teNextPayload;
        console.log('[TRANSFORM] payload', JSON.stringify(payload));
        teNextPayload = null; // Clear it, so we can catch new updates

        // Update status for feedback
        // Debugging: Show start/current values to diagnose scaling issues
        // setSelectionStatus(`Live: Scale ${payload.scale.toFixed(1)}%, Rot ${payload.rotate.toFixed(1)}Â°`, false);
        // setSelectionStatus(`Live: ${payload.scale.toFixed(1)}% (Start:${teDragStartScale}->Cur:${teScaleSlider.value})`, false);

        try {
            // Add a timeout race to prevent hanging if Illustrator doesn't respond
            const transformPromise = evalScript(`MDUX_cppTransformEachLive(${payload.scale}, ${payload.rotate})`);
            const timeoutPromise = new Promise(resolve => setTimeout(() => resolve("TIMEOUT"), 1000));

            const result = await Promise.race([transformPromise, timeoutPromise]);

            if (result === "TIMEOUT") {
                setSelectionStatus("Transform timed out", true);
            } else {
                // If successful, mark that we have applied a transform in this drag session
                if (teDragActive) {
                    teTransformAppliedInDrag = true;
                }

                lastSelectionScale = payload.scale;
                lastSelectionRotation = payload.rotate;
                console.log('[TRANSFORM] applied scale/rotate', lastSelectionScale, lastSelectionRotation);

                // DEBUG: Show the result message from JSX
                try {
                    const resObj = JSON.parse(result);
                    if (resObj && resObj.message) {
                        setSelectionStatus(resObj.message, false);
                    } else {
                        setSelectionStatus("No msg: " + result, false);
                    }
                } catch (e) {
                    setSelectionStatus("Parse err: " + result, true);
                }
            }
        } catch (e) {
            console.error("Transform error:", e);
            setSelectionStatus("Error: " + e.message, true);
        } finally {
            teIsBusy = false;
            // Check if more accumulated while we were busy
            if (teNextPayload) {
                // IMPORTANT: If we are processing a queued item, it MUST undo the previous one
                // if a transform was applied.
                if (teTransformAppliedInDrag) {
                    teNextPayload.undoPrevious = true;
                }
                processTransformQueue();
            }
        }
    }

    // Debounce timer for transform (500ms as per V3 spec)
    let transformDebounceTimer = null;
    const TRANSFORM_DEBOUNCE_MS = 0;

    function handleLiveTransform(source, commitNow) {
        // Check if Live is enabled
        if (teLiveOption && !teLiveOption.checked) {
            teSliderCommitInFlight = false;
            return;
        }

        // Ensure elements are found
        if (!teScaleSlider || !teRotateSlider) {
            teSliderCommitInFlight = false;
            return;
        }

        const currentScale = parseFloat(teScaleSlider.value);
        const currentRotate = parseFloat(teRotateSlider.value);
        const useScale = (source === 'rotate' && !teScaleDirty && lastSelectionScale !== null) ? lastSelectionScale : currentScale;
        const useRotate = (source === 'scale' && !teRotateDirty && lastSelectionRotation !== null) ? lastSelectionRotation : currentRotate;

        if (isNaN(useScale) || isNaN(useRotate)) {
            teSliderCommitInFlight = false;
            return;
        }
        console.log('[TRANSFORM] live', source, 'cur', currentScale, currentRotate, 'use', useScale, useRotate, 'dirty', teScaleDirty, teRotateDirty, 'last', lastSelectionScale, lastSelectionRotation);

        // Send ABSOLUTE values - the slider value IS the target scale/rotation
        // MDUX_transformEach calculates the resize factor from current metadata
        // No undo needed - debounce ensures only final value is applied
        teNextPayload = {
            scale: useScale,      // Absolute target scale (e.g., 120 means 120%)
            rotate: useRotate,    // Absolute target rotation (e.g., 45 means 45deg)
            scaleDirty: teScaleDirty,
            rotateDirty: teRotateDirty,
            undoPrevious: false   // Not needed with absolute values + debounce
        };

        if (teDragActive && !commitNow) {
            tePendingSliderSource = source;
            return;
        }

        if (TRANSFORM_DEBOUNCE_MS <= 0) {
            processTransformQueue().finally(() => {
                teDragActive = false;
                tePendingSliderSource = null;
                teSliderCommitInFlight = false;
            });
            return;
        }

        // V3: Apply debounce - transform only takes effect after user stops dragging
        if (transformDebounceTimer) {
            clearTimeout(transformDebounceTimer);
        }
        transformDebounceTimer = setTimeout(async () => {
            await processTransformQueue();
            transformDebounceTimer = null;
            // Small delay to let Illustrator finalize state before polling can run
            await new Promise(r => setTimeout(r, 100));
            // NOW safe to allow polling again - transform is complete
            teDragActive = false;
            tePendingSliderSource = null;
            teSliderCommitInFlight = false;
        }, TRANSFORM_DEBOUNCE_MS);
    }

    function commitPendingTransform(source) {
        if (teSliderCommitInFlight) return;
        const commitSource = source || tePendingSliderSource || 'scale';
        if (!teScaleDirty && !teRotateDirty) {
            teDragActive = false;
            tePendingSliderSource = null;
            return;
        }
        if (transformDebounceTimer) {
            clearTimeout(transformDebounceTimer);
            transformDebounceTimer = null;
        }
        teSliderCommitInFlight = true;
        handleLiveTransform(commitSource, true);
    }

    function resetTransformControls(resetValues = true) {
        if (resetValues) {
            if (teScaleSlider) teScaleSlider.value = 100;
            if (teScaleInput) teScaleInput.value = 100;
            if (teRotateSlider) teRotateSlider.value = 0;
            if (teRotateInput) teRotateInput.value = 0;
        }

        teDragActive = false;
        teTransformAppliedInDrag = false;
        teNextPayload = null;
        tePendingSliderSource = null;
        teSliderCommitInFlight = false;
        teDragStartScale = 100;
        teDragStartRotate = 0;
        teScaleDirty = false;
        teRotateDirty = false;
        lastSelectionScale = 100;
        lastSelectionRotation = 0;
    }

    async function handleTransformEach() {
        // If Live is ON, the button just resets the controls (commits the change)
        if (teLiveOption && teLiveOption.checked) {
            resetTransformControls(true);
            setSelectionStatus("Transformation committed.", false);
        } else {
            // If Live is OFF, apply the current values
            const s = parseFloat(teScaleInput.value) || 100;
            const r = parseFloat(teRotateInput.value) || 0;

            if (s === 100 && r === 0) {
                setSelectionStatus("No changes to apply.", false);
                return;
            }

            setSelectionStatus("Transforming...", false);
            try {
                await evalScript(`MDUX_cppTransformEach(${s}, ${r})`);
                setSelectionStatus("Transformation applied.", false);
                // Reset internal state but NOT input values - let refresh update them from metadata
                resetTransformControls(false);
                // Refresh selection state to load the metadata we just saved
                await refreshSelectionTransformState();
            } catch (e) {
                setSelectionStatus("Error: " + e.message, true);
            }
        }
    }

    async function handleResetOriginal() {
        setSelectionStatus("Resetting to original...", false);
        try {
            await evalScript('MDUX_cppResetOriginal()');
            setSelectionStatus("Reset complete.", false);
            resetTransformControls(true);
        } catch (e) {
            setSelectionStatus("Error: " + e.message, true);
        }
    }

    async function refreshSelectionTransformState() {
        console.log('[JS] refreshSelectionTransformState called, teDragActive=', teDragActive, 'teIsBusy=', teIsBusy);
        if (isCepSuspended()) return;
        if (teDragActive || teIsBusy) {
            // PERF: Removed debug logging to prevent blocking ExtendScript calls
            return;
        }

        // PERF: Removed debug logging to prevent blocking ExtendScript calls

        try {
            await ensureBridgeLoaded();
        } catch (e) {
            console.error('Bridge load failed in refreshSelectionTransformState:', e);
            // PERF: Removed debug logging to prevent blocking ExtendScript calls
            return;
        }

        try {
            const raw = await evalScript('MDUX_getSelectionTransformState()');
            console.log('[JS] refreshSelectionTransformState raw:', raw);
            if (!raw) {
                // PERF: Removed debug logging to prevent blocking ExtendScript calls
                return;
            }
            const res = JSON.parse(raw);
            console.log('[JS] refreshSelectionTransformState parsed:', res);

            const statusEl = document.getElementById('selection-status');
            if (statusEl) statusEl.textContent = '';

            if (res.ok && res.count > 0) {
                let statusMsg = [];
                const preserveScale = teScaleDirty || teRotateDirty;
                const preserveRotate = teRotateDirty || teScaleDirty;

                // If either input has focus, blur it so we can update values
                // This handles the case where user clicks a new object in Illustrator
                // but the panel input still has focus (different windows)
                if (document.activeElement === teScaleInput) {
                    teScaleInput.blur();
                }
                if (document.activeElement === teRotateInput) {
                    teRotateInput.blur();
                }

                // Update scale
                if (!preserveScale) {
                    if (res.mixedScale) {
                        console.log('[JS] Mixed scale detected');
                        teScaleInput.value = '';
                        teScaleInput.placeholder = 'Mixed';
                        teScaleSlider.value = 100;
                        statusMsg.push('Multiple different scales in selection');
                        lastSelectionScale = null;
                    } else {
                        console.log('[JS] Setting scale to:', res.scale);
                        teScaleInput.value = res.scale;
                        teScaleInput.placeholder = '';
                        teScaleSlider.value = res.scale;
                        console.log('[JS] Scale slider value now:', teScaleSlider.value);
                        lastSelectionScale = res.scale;
                    }
                } else {
                    console.log('[JS] Preserve scale UI during live drag');
                }

                // Update rotation
                if (!preserveRotate) {
                    if (res.mixedRotation) {
                        console.log('[JS] Mixed rotation detected');
                        teRotateInput.value = '';
                        teRotateInput.placeholder = 'Mixed';
                        teRotateSlider.value = 0;
                        statusMsg.push('Multiple different rotations in selection');
                        lastSelectionRotation = null;
                    } else {
                        console.log('[JS] Setting rotation to:', res.rotation);
                        teRotateInput.value = res.rotation;
                        teRotateInput.placeholder = '';
                        teRotateSlider.value = res.rotation;
                        console.log('[JS] Rotation slider value now:', teRotateSlider.value);
                        lastSelectionRotation = res.rotation;
                    }
                } else {
                    console.log('[JS] Preserve rotation UI during live drag');
                }
                if (!preserveScale) {
                    teScaleDirty = false;
                }
                if (!preserveRotate) {
                    teRotateDirty = false;
                }
                console.log('[JS] selection state', { scale: lastSelectionScale, rotation: lastSelectionRotation, mixedScale: res.mixedScale, mixedRotation: res.mixedRotation });

                if (statusEl && statusMsg.length > 0) {
                    statusEl.textContent = statusMsg.join(' â€¢ ');
                }
            } else {
                console.log('[JS] No selection or not ok, resetting to defaults');
                // No selection or no tagged items
                teScaleInput.value = 100;
                teScaleInput.placeholder = '';
                teScaleSlider.value = 100;
                teRotateInput.value = 0;
                teRotateInput.placeholder = '';
                teRotateSlider.value = 0;
                lastSelectionScale = 100;
                lastSelectionRotation = 0;
                teScaleDirty = false;
                teRotateDirty = false;
                console.log('[JS] selection state', { scale: lastSelectionScale, rotation: lastSelectionRotation, mixedScale: res.mixedScale, mixedRotation: res.mixedRotation });
            }
        } catch (e) {
            console.error('Refresh transform state failed:', e);
        }
    }

    // Helper to handle drag start/end globally
    async function handleDragStart(source) {
        // Load current state from metadata BEFORE setting teDragActive
        // (refreshSelectionTransformState skips if teDragActive is true)
        await refreshSelectionTransformState();

        // Now set drag active
        teDragActive = true;
        teTransformAppliedInDrag = false;
        tePendingSliderSource = source || null;

        // Capture start values from both sliders (now updated from metadata)
        teDragStartScale = parseFloat(teScaleSlider.value) || 100;
        teDragStartRotate = parseFloat(teRotateSlider.value) || 0;

        // Listen for global mouseup to end the session
        window.addEventListener('mouseup', handleDragEnd, { once: true });
    }

    function handleDragEnd() {
        if (tePendingSliderSource) {
            commitPendingTransform(tePendingSliderSource);
            return;
        }
        // DON'T set teDragActive = false here if there's a pending debounce timer
        // The debounce callback will set it after the transform completes
        // This prevents polling from overwriting the slider value before the transform fires
        if (!transformDebounceTimer) {
            teDragActive = false;
        }
        // The transformation is already applied during 'input' events.
        // We do NOT reset controls here so the user can see where they left it.
        teTransformAppliedInDrag = false;
    }

    function initCollapsibleSections() {
        const collapsibleHeaders = document.querySelectorAll('.collapsible-header');
        collapsibleHeaders.forEach(header => {
            header.addEventListener('click', function() {
                const section = this.closest('.collapsible');
                const content = section.querySelector('.collapsible-content');
                const icon = this.querySelector('.collapse-icon');

                if (content.style.display === 'none') {
                    content.style.display = 'block';
                    icon.textContent = 'â–²';
                    section.classList.add('expanded');
                } else {
                    content.style.display = 'none';
                    icon.textContent = 'â–¼';
                    section.classList.remove('expanded');
                }
            });
        });
    }

    function attachListeners() {
        panelFileLog('[INIT] attachListeners start: processPlacedBtn=' + (processPlacedBtn ? 'yes' : 'no'));
        htmlLog('[INIT] attachListeners start: processPlacedBtn=' + (processPlacedBtn ? 'yes' : 'no'));
        if (processBtn) processBtn.addEventListener('click', handleProcessClick);
        if (processPlacedBtn) processPlacedBtn.addEventListener('click', handleProcessPlacedApiClick);
        if (toggleConnectorStyleBtn) toggleConnectorStyleBtn.addEventListener('click', handleToggleConnectorStyleClick);
        if (markOverlapSeparateBtn) markOverlapSeparateBtn.addEventListener('click', handleMarkOverlapSeparateClick);
        if (setEmoryStartBtn) setEmoryStartBtn.addEventListener('click', handleSetEmoryStartClick);
        if (processEmoryBtn) processEmoryBtn.addEventListener('click', handleProcessEmoryClick);
        if (emoryWidthSlider && emoryWidthInput) {
            const beginEmoryWidthDrag = function () {
                emoryWidthDragActive = true;
                emoryWidthRefreshPending = false;
            };
            emoryWidthSlider.addEventListener('mousedown', function () {
                beginEmoryWidthDrag();
            });
            emoryWidthSlider.addEventListener('touchstart', function () {
                beginEmoryWidthDrag();
            }, { passive: true });
            window.addEventListener('mouseup', function () {
                finishEmoryWidthDrag(true);
            });
            window.addEventListener('touchend', function () {
                finishEmoryWidthDrag(true);
            }, { passive: true });
            window.addEventListener('touchcancel', function () {
                finishEmoryWidthDrag(false);
            }, { passive: true });
            emoryWidthSlider.addEventListener('input', function () {
                const numericWidth = Number(emoryWidthSlider.value);
                if (!isFinite(numericWidth) || numericWidth <= 0) return;
                beginEmoryWidthDrag();
                emoryWidthInput.value = emoryWidthSlider.value;
                setEmoryWidthStatus('Release to apply width.', false);
            });
            emoryWidthSlider.addEventListener('change', function () {
                const numericWidth = Number(emoryWidthSlider.value);
                if (!isFinite(numericWidth) || numericWidth <= 0) return;
                emoryWidthInput.value = emoryWidthSlider.value;
                finishEmoryWidthDrag(true);
            });
        }
        if (emoryWidthInput && emoryWidthSlider) {
            emoryWidthInput.addEventListener('input', function () {
                const numericWidth = Number(emoryWidthInput.value);
                if (!isFinite(numericWidth) || numericWidth <= 0) return;
                syncEmoryWidthControls(numericWidth, true);
            });
            emoryWidthInput.addEventListener('change', function () {
                const numericWidth = Number(emoryWidthInput.value);
                if (!isFinite(numericWidth) || numericWidth <= 0) {
                    setEmoryWidthStatus('Width must be greater than zero.', true);
                    return;
                }
                emoryWidthDragActive = false;
                syncEmoryWidthControls(numericWidth, true);
                queueEmoryWidthApply(numericWidth, true);
            });
            emoryWidthInput.addEventListener('keydown', function (e) {
                if (e.key !== 'Enter') return;
                e.preventDefault();
                const numericWidth = Number(emoryWidthInput.value);
                if (!isFinite(numericWidth) || numericWidth <= 0) {
                    setEmoryWidthStatus('Width must be greater than zero.', true);
                    return;
                }
                emoryWidthDragActive = false;
                syncEmoryWidthControls(numericWidth, true);
                queueEmoryWidthApply(numericWidth, true);
            });
        }
        if (revertBtn) revertBtn.addEventListener('click', handleRevertPreOrtho);
        if (clearRotationMetadataBtn) clearRotationMetadataBtn.addEventListener('click', handleClearRotationMetadata);
        if (getAngleBtn) getAngleBtn.addEventListener('click', handleGetAngle);
        if (clearRotationBtn && rotationInput) {
            clearRotationBtn.addEventListener('click', () => {
                rotationInput.value = '';
                rotationInput.dataset.autoValue = '';
                rotationInput.dataset.multi = 'false';
                rotationInput.placeholder = 'Leave blank to skip';
                setProcessStatus('');
            });
        }
        if (rotationInput) {
            rotationInput.addEventListener('input', () => {
                rotationInput.dataset.autoValue = '';
                rotationInput.dataset.multi = 'false';
                const val = parseFloat(rotationInput.value);
                if (!isNaN(val)) {
                    evalScript(`MDUX_cppSetRotationOverride(${val})`);
                } else if (!rotationInput.value.trim()) {
                    evalScript('MDUX_cppClearRotationOverride()');
                }
            });
            // Handle Enter key to apply rotation
            rotationInput.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    const val = parseFloat(rotationInput.value);
                    if (!isNaN(val)) {
                        console.log('[ROTATION] Enter pressed, applying rotation: ' + val);
                        rotateSelection(val);
                        evalScript(`MDUX_cppSetRotationOverride(${val})`);
                    }
                }
            });
            // Normalize angle on blur (when user leaves the field)
            rotationInput.addEventListener('blur', () => {
                const val = parseFloat(rotationInput.value);
                if (!isNaN(val)) {
                    const normalized = normalizeAngle(val);
                    if (normalized !== val) {
                        rotationInput.value = normalized;
                        console.log('[ROTATION] Normalized ' + val + '? to ' + normalized + '?');
                    }
                    evalScript(`MDUX_cppSetRotationOverride(${normalized})`);
                } else if (!rotationInput.value.trim()) {
                    evalScript('MDUX_cppClearRotationOverride()');
                }
            });
        }

        // Document Scale listeners (read-only - only refresh button active)
        if (getDocScaleBtn) getDocScaleBtn.addEventListener('click', refreshDocScale);
        // Legacy: Document scaling controls disabled to prevent desync issues
        // if (setDocScale100Btn) setDocScale100Btn.addEventListener('click', handleSetDocScale100);
        // if (applyDocScaleBtn) applyDocScaleBtn.addEventListener('click', handleApplyDocScale);

        // New Mutual Exclusivity Logic
        const orthoToggles = [skipOrthoOption, skipAllBranchesOption, skipFinalOption];
        const processOrthoToggles = [processSkipOrthoOption, processSkipAllBranchesOption, processSkipFinalOption];
        const orthoGrid = document.getElementById('ortho-toggle-grid');

        function syncProcessOrthoFromMain() {
            if (processSkipOrthoOption) processSkipOrthoOption.checked = !!(skipOrthoOption && skipOrthoOption.checked);
            if (processSkipAllBranchesOption) processSkipAllBranchesOption.checked = !!(skipAllBranchesOption && skipAllBranchesOption.checked);
            if (processSkipFinalOption) processSkipFinalOption.checked = !!(skipFinalOption && skipFinalOption.checked);
        }

        function syncMainOrthoFromProcess(changedInput) {
            if (!changedInput) return;
            if (changedInput === processSkipOrthoOption && skipOrthoOption) {
                skipOrthoOption.checked = !!changedInput.checked;
            } else if (changedInput === processSkipAllBranchesOption && skipAllBranchesOption) {
                skipAllBranchesOption.checked = !!changedInput.checked;
            } else if (changedInput === processSkipFinalOption && skipFinalOption) {
                skipFinalOption.checked = !!changedInput.checked;
            }
            if (changedInput.checked) {
                if (changedInput !== processSkipOrthoOption && skipOrthoOption) skipOrthoOption.checked = false;
                if (changedInput !== processSkipAllBranchesOption && skipAllBranchesOption) skipAllBranchesOption.checked = false;
                if (changedInput !== processSkipFinalOption && skipFinalOption) skipFinalOption.checked = false;
            }
            updateOrthoState(changedInput === processSkipOrthoOption ? skipOrthoOption
                : changedInput === processSkipAllBranchesOption ? skipAllBranchesOption
                : skipFinalOption);
            syncProcessOrthoFromMain();
        }

        function updateOrthoState(changedInput) {
            if (changedInput && changedInput.checked) {
                orthoToggles.forEach(t => {
                    if (t !== changedInput) t.checked = false;
                });
            }

            // Update visual state
            if (orthoGrid) {
                const anyChecked = orthoToggles.some(t => t.checked);
                if (anyChecked) {
                    orthoGrid.classList.add('has-selection');
                } else {
                    orthoGrid.classList.remove('has-selection');
                }
            }
            syncProcessOrthoFromMain();
        }

        orthoToggles.forEach(t => {
            if (t) {
                t.addEventListener('change', () => updateOrthoState(t));
            }
        });
        processOrthoToggles.forEach(t => {
            if (t) {
                t.addEventListener('change', () => syncMainOrthoFromProcess(t));
            }
        });

        // Initial state check
        updateOrthoState(null);
        if (rotate90Btn) rotate90Btn.addEventListener('click', () => rotateSelection(90));
        if (rotate45Btn) rotate45Btn.addEventListener('click', () => rotateSelection(45));
        if (rotate180Btn) rotate180Btn.addEventListener('click', () => rotateSelection(180));

        // V3 Quick Rotate - negative rotations
        if (rotateNeg45Btn) rotateNeg45Btn.addEventListener('click', () => rotateSelection(-45));
        if (rotateNeg90Btn) rotateNeg90Btn.addEventListener('click', () => rotateSelection(-90));
        if (rotateCustomBtn) {
            rotateCustomBtn.addEventListener('click', () => {
                const angle = prompt('Enter rotation angle in degrees:', '0');
                if (angle !== null && !isNaN(parseFloat(angle))) {
                    rotateSelection(parseFloat(angle));
                }
            });
        }
    }

    // V3 Process Ductwork Panel - Secondary Buttons
    if (rotateRegistersBtn) {
        rotateRegistersBtn.addEventListener('click', async () => {
            setProcessStatus('Rotating registers...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_rotateRegistersOnly()');
                setProcessStatus(result || 'Registers rotated');
            } catch (e) {
                setProcessStatus('Error: ' + e.message, true);
            }
        });
    }

    if (carveRegistersBtn) {
        carveRegistersBtn.addEventListener('click', async () => {
            setProcessStatus('Carving for registers...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_carveForRegistersOnly()');
                setProcessStatus(result || 'Carved for registers');
            } catch (e) {
                setProcessStatus('Error: ' + e.message, true);
            }
        });
    }

    if (carveOverlapsBtn) {
        carveOverlapsBtn.addEventListener('click', async () => {
            setProcessStatus('Carving overlaps...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_carveOverlapsOnly()');
                setProcessStatus(result || 'Overlaps carved');
            } catch (e) {
                setProcessStatus('Error: ' + e.message, true);
            }
        });
    }

    // V3 Orthogonalize Panel Buttons
    function setOrthoStatus(msg, isError) {
        if (orthoStatus) {
            orthoStatus.textContent = msg || '';
            orthoStatus.className = isError ? 'status error' : 'status';
        }
    }

    if (orthoTrunkBtn) {
        orthoTrunkBtn.addEventListener('click', async () => {
            setOrthoStatus('Orthogonalizing trunks...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_orthoTrunkOnly()');
                setOrthoStatus(result || 'Trunks orthogonalized');
            } catch (e) {
                setOrthoStatus('Error: ' + e.message, true);
            }
        });
    }

    if (orthoBranchesBtn) {
        orthoBranchesBtn.addEventListener('click', async () => {
            setOrthoStatus('Orthogonalizing branches...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_orthoBranchesOnly()');
                setOrthoStatus(result || 'Branches orthogonalized');
            } catch (e) {
                setOrthoStatus('Error: ' + e.message, true);
            }
        });
    }

    if (orthoFinalBtn) {
        orthoFinalBtn.addEventListener('click', async () => {
            setOrthoStatus('Orthogonalizing final segments...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_orthoFinalOnly()');
                setOrthoStatus(result || 'Final segments orthogonalized');
            } catch (e) {
                setOrthoStatus('Error: ' + e.message, true);
            }
        });
    }

    if (orthoAllBtn) {
        orthoAllBtn.addEventListener('click', async () => {
            setOrthoStatus('Orthogonalizing all...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_orthoAll()');
                setOrthoStatus(result || 'All orthogonalized');
            } catch (e) {
                setOrthoStatus('Error: ' + e.message, true);
            }
        });
    }

    // V3 Ductwork Parts Panel Buttons
    function setPartsStatus(msg, isError) {
        if (partsStatus) {
            partsStatus.textContent = msg || '';
            partsStatus.className = isError ? 'status error' : 'status';
        }
    }

    if (createUpdatePartsBtn) {
        createUpdatePartsBtn.addEventListener('click', async () => {
            setPartsStatus('Creating/updating ductwork parts...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_createUpdateDuctworkParts()');
                setPartsStatus(result || 'Ductwork parts created/updated');
            } catch (e) {
                setPartsStatus('Error: ' + e.message, true);
            }
        });
    }

    if (createAnchorsBtn) {
        createAnchorsBtn.addEventListener('click', async () => {
            setPartsStatus('Creating part anchors...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_createPartAnchorsOnly()');
                setPartsStatus(result || 'Part anchors created');
            } catch (e) {
                setPartsStatus('Error: ' + e.message, true);
            }
        });
    }

    if (selectPartsBtn) {
        selectPartsBtn.addEventListener('click', async () => {
            setPartsStatus('Selecting ductwork parts...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_selectDuctworkParts()');
                setPartsStatus(result || 'Parts selected');
            } catch (e) {
                setPartsStatus('Error: ' + e.message, true);
            }
        });
    }

    if (selectAnchorsBtn) {
        selectAnchorsBtn.addEventListener('click', async () => {
            setPartsStatus('Selecting anchors...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_selectDuctworkAnchors()');
                setPartsStatus(result || 'Anchors selected');
            } catch (e) {
                setPartsStatus('Error: ' + e.message, true);
            }
        });
    }

    if (deleteAnchorsBtn) {
        deleteAnchorsBtn.addEventListener('click', async () => {
            if (!confirm('Delete all selected ductwork part anchors?')) return;
            setPartsStatus('Deleting anchors...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_deleteSelectedAnchors()');
                setPartsStatus(result || 'Anchors deleted');
            } catch (e) {
                setPartsStatus('Error: ' + e.message, true);
            }
        });
    }

    if (placeGraphicsBtn) {
        placeGraphicsBtn.addEventListener('click', async () => {
            setPartsStatus('Placing ductwork part graphics...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_placeDuctworkPartGraphics()');
                setPartsStatus(result || 'Graphics placed');
            } catch (e) {
                setPartsStatus('Error: ' + e.message, true);
            }
        });
    }

    // V3 Transform Panel - Reset Parts Rotation button
    if (resetPartsRotationBtn) {
        resetPartsRotationBtn.addEventListener('click', async () => {
            setSelectionStatus('Resetting ductwork parts rotation...');
            try {
                await ensureBridgeLoaded();
                const result = await evalScript('MDUX_cppResetRotation()');
                setSelectionStatus(result || 'Parts rotation reset');
                resetTransformControls(true);
            } catch (e) {
                setSelectionStatus('Error: ' + e.message, true);
            }
        });
    }

    // V3 Export Floorplan button
    if (exportFloorplanBtn) {
        exportFloorplanBtn.addEventListener('click', () => handleExport('floorplan'));
    }

        if (teLiveOption && !teLiveOption.checked) {
            teLiveOption.checked = true;
        }

        if (teScaleSlider && teScaleInput) {
            teScaleSlider.addEventListener('mousedown', () => handleDragStart('scale'));
            teScaleSlider.addEventListener('change', () => commitPendingTransform('scale'));

            teScaleSlider.addEventListener('input', (e) => {
                const rawValue = parseFloat(teScaleSlider.value);
                let newValue = rawValue;
                const sliderMax = parseFloat(teScaleSlider.max);
                const sliderMin = parseFloat(teScaleSlider.min);
                if (teDragActive) {
                    const delta = rawValue - teDragStartScale;
                    const speed = e.shiftKey ? 1.0 : 0.25;
                    newValue = teDragStartScale + (delta * speed);
                    // Clamp to slider range
                    newValue = Math.max(sliderMin, Math.min(sliderMax, newValue));
                }
                teScaleInput.value = Math.round(newValue);
                lastSelectionScale = newValue;
                teScaleDirty = true;
                console.log('[TRANSFORM] scale input', rawValue, '->', newValue);
                handleLiveTransform('scale', false);
            });

        // Track if Enter was just pressed to skip change event
        let scaleEnterPressed = false;

        teScaleInput.addEventListener('change', () => {
            // Skip if Enter was pressed (we handle that separately)
            if (scaleEnterPressed) {
                scaleEnterPressed = false;
                return;
            }

            let val = parseFloat(teScaleInput.value);
            if (isNaN(val)) return;

            teDragStartScale = 100;
            teDragStartRotate = 0;
            teScaleSlider.value = val;
            lastSelectionScale = val;

            teDragActive = true;
            teTransformAppliedInDrag = false;
            teScaleDirty = true;
            handleLiveTransform('scale', true);

            teDragActive = false;
            teTransformAppliedInDrag = false;
        });

        // Handle Enter key to apply scale immediately
        teScaleInput.addEventListener('keydown', async (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                e.stopPropagation();

                // CRITICAL: Cancel any pending slider debounce to prevent competing transforms
                if (transformDebounceTimer) {
                    clearTimeout(transformDebounceTimer);
                    transformDebounceTimer = null;
                }
                teNextPayload = null;

                scaleEnterPressed = true;

                // Get the typed value BEFORE any blur/reset
                const typedScale = parseFloat(teScaleInput.value) || 100;
                const currentRotation = parseFloat(teRotateInput.value) || 0;

                console.log('[TRANSFORM] Enter pressed on scale, applying value:', typedScale);

                // Sync the slider to match typed value
                if (teScaleSlider) teScaleSlider.value = Math.max(10, Math.min(400, typedScale));

                // Apply transform directly regardless of Live mode
                if (typedScale !== 100 || currentRotation !== 0) {
                    setSelectionStatus("Transforming...", false);
                    try {
                        await evalScript(`MDUX_cppTransformEach(${typedScale}, ${currentRotation})`);
                        setSelectionStatus("Transformation applied.", false);
                    } catch (err) {
                        setSelectionStatus("Error: " + err.message, true);
                    }
                }

                // Reset controls after applying
                resetTransformControls(true);
                teScaleInput.blur();
            }
        });
    }
    if (teRotateSlider && teRotateInput) {
        teRotateSlider.addEventListener('mousedown', () => handleDragStart('rotate'));
        teRotateSlider.addEventListener('change', () => commitPendingTransform('rotate'));

        teRotateSlider.addEventListener('input', (e) => {
            const rawValue = parseFloat(teRotateSlider.value);
            let newValue = rawValue;
            const sliderMax = parseFloat(teRotateSlider.max);
            const sliderMin = parseFloat(teRotateSlider.min);
            if (teDragActive) {
                const delta = rawValue - teDragStartRotate;
                const speed = e.shiftKey ? 1.0 : 0.25;
                newValue = teDragStartRotate + (delta * speed);
                // Clamp to slider range
                newValue = Math.max(sliderMin, Math.min(sliderMax, newValue));
            }
            teRotateInput.value = Math.round(newValue);
            lastSelectionRotation = newValue;
            teRotateDirty = true;
            console.log('[TRANSFORM] rotate input', rawValue, '->', newValue);
            handleLiveTransform('rotate', false);
        });

        // Track if Enter was just pressed to skip change event
        let rotateEnterPressed = false;

        teRotateInput.addEventListener('change', () => {
            // Skip if Enter was pressed (we handle that separately)
            if (rotateEnterPressed) {
                rotateEnterPressed = false;
                return;
            }

            let val = parseFloat(teRotateInput.value);
            if (isNaN(val)) return;

            // When user types a value in the rotation field, they want ABSOLUTE rotation
            console.log('[TRANSFORM] Rotation input changed to ' + val + 'Â°, applying absolute rotation');
            rotateSelection(val);

            teRotateSlider.value = val;
            teDragStartRotate = val;
            teTransformAppliedInDrag = false;
        });

        // Handle Enter key to apply rotation immediately - use ABSOLUTE rotation
        teRotateInput.addEventListener('keydown', async (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                e.stopPropagation();
                let val = parseFloat(teRotateInput.value);
                if (isNaN(val)) return;

                // CRITICAL: Cancel any pending slider debounce to prevent competing transforms
                if (transformDebounceTimer) {
                    clearTimeout(transformDebounceTimer);
                    transformDebounceTimer = null;
                }
                teNextPayload = null;

                rotateEnterPressed = true;
                teRotateInput.blur();
                console.log('[TRANSFORM] Enter pressed on rotation, applying absolute rotation: ' + val + 'Â°');
                await rotateSelection(val);
                teRotateSlider.value = val;
            }
        });
    }

    reloadBtn.addEventListener('click', () => {
        forceReloadScripts().finally(() => reloadExtensionView());
    });

    // Refresh button (top right)
    const refreshBtnTop = document.getElementById('refresh-btn-top');
    if (refreshBtnTop) {
        refreshBtnTop.addEventListener('click', () => {
            forceReloadScripts().finally(() => reloadExtensionView());
        });
    }

    // Add keyboard shortcut for reloading (F5)
    window.addEventListener('keydown', (e) => {
        if (e.key === 'F5') {
            forceReloadScripts().finally(() => reloadExtensionView());
        }
        // ESC to close debug log
        if (e.key === 'Escape') {
            const modal = document.getElementById('debug-log-modal');
            if (modal && modal.style.display === 'block') {
                modal.style.display = 'none';
            }
        }
    });

    async function init() {
        if (window.MDUX_INIT_DONE) {
            return;
        }
        panelFileLog('[INIT] init() starting');
        htmlLog('[INIT] init() starting');
        // Log immediately using raw csInterface (not Promise wrapper)
        csInterface.evalScript('MDUX_debugLog("[INIT] init() starting...")', function() {});

        try {
            if (isCepSuspended()) {
                if (!window.MDUX_INIT_WAITING) {
                    window.MDUX_INIT_WAITING = true;
                    const resumeTimer = setInterval(() => {
                        if (!isCepSuspended()) {
                            clearInterval(resumeTimer);
                            window.MDUX_INIT_WAITING = false;
                            init();
                        }
                    }, 1000);
                }
                return;
            }

            refreshDomRefs();
            // Re-fetch elements to ensure they exist (in case script ran before DOM)
            teScaleInput = document.getElementById('te-scale');
            teRotateInput = document.getElementById('te-rotate');
            teScaleSlider = document.getElementById('te-scale-slider');
            teRotateSlider = document.getElementById('te-rotate-slider');
            transformEachBtn = document.getElementById('transform-each-btn');
            teResetOriginalBtn = document.getElementById('te-reset-original-btn');
            teLiveOption = document.getElementById('te-live-option');

            csInterface.evalScript('MDUX_debugLog("[INIT] Elements fetched")', function() {});

            initCollapsibleSections();
            csInterface.evalScript('MDUX_debugLog("[INIT] Collapsible sections initialized")', function() {});

            try {
                attachListeners();
            } catch (e) {
                const msg = String(e && e.message ? e.message : e);
                if (debugStatus) {
                    debugStatus.textContent = '[INIT] Listener error: ' + msg;
                }
                csInterface.evalScript('MDUX_debugLog("[INIT] Listener error: ' + msg.replace(/'/g, "\\'") + '")', function() {});
            }
            csInterface.evalScript('MDUX_debugLog("[INIT] Listeners attached")', function() {});

            // Attach isolation and export listeners
            if (isolatePartsBtn) isolatePartsBtn.addEventListener('click', () => isolate('parts'));
            if (isolateLinesBtn) isolateLinesBtn.addEventListener('click', () => isolate('lines'));
            if (unlockDuctworkBtn) unlockDuctworkBtn.addEventListener('click', () => isolate('unlock'));
            if (createLayersBtn) createLayersBtn.addEventListener('click', () => isolate('create'));
            if (importStylesBtn) importStylesBtn.addEventListener('click', importGraphicStyles);
            if (exportDuctworkBtn) exportDuctworkBtn.addEventListener('click', () => handleExport('ductwork'));
            if (reexportFloorplanBtn) reexportFloorplanBtn.addEventListener('click', () => handleExport('floorplan'));

            // Heal Gaps button handler
            if (healGapsBtn) {
                healGapsBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Healing gaps...';
                            protectionStatus.style.color = '#f0f';
                        }
                        const result = await evalScript('MDUX_healGapsInSelection()');
                        if (protectionStatus) {
                            protectionStatus.textContent = result || 'Heal complete';
                            protectionStatus.style.color = '#0f0';
                            setTimeout(() => { if (protectionStatus) protectionStatus.textContent = ''; }, 4000);
                        }
                    } catch (err) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + err;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            // Recreate Gaps button handler
            if (recutGapsBtn) {
                recutGapsBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Recreating gaps...';
                            protectionStatus.style.color = '#f0f';
                        }
                        const result = await evalScript('MDUX_recreateGapsInSelection()');
                        if (protectionStatus) {
                            protectionStatus.textContent = result || 'Recreate complete';
                            protectionStatus.style.color = '#0f0';
                            setTimeout(() => { if (protectionStatus) protectionStatus.textContent = ''; }, 4000);
                        }
                    } catch (err) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + err;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            // Merge Paths button handler
            if (mergePathsBtn) {
                mergePathsBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Merging paths...';
                            protectionStatus.style.color = '#f0f';
                        }
                        const result = await evalScript('MDUX_mergePathsAtEndpoints()');
                        if (protectionStatus) {
                            protectionStatus.textContent = result || 'Merge complete';
                            protectionStatus.style.color = '#0f0';
                            setTimeout(() => { if (protectionStatus) protectionStatus.textContent = ''; }, 3000);
                        }
                    } catch (err) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + err;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            csInterface.evalScript('MDUX_debugLog("[INIT] Isolation listeners attached")', function() {});

            // Fix Selection Transform listeners
            if (transformEachBtn) transformEachBtn.addEventListener('click', handleTransformEach);
            if (teResetOriginalBtn) teResetOriginalBtn.addEventListener('click', handleResetOriginal);

            // Reset Strokes and Normalize Ductwork Parts buttons
            if (resetStrokesBtn) {
                resetStrokesBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        const result = await evalScript('MDUX_cppResetStrokes()');
                        if (selectionStatus) selectionStatus.textContent = result || 'Strokes reset';
                    } catch (e) {
                        if (selectionStatus) selectionStatus.textContent = 'Error: ' + e.message;
                    }
                });
            }
            if (resetPartsScaleBtn) {
                resetPartsScaleBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                const result = await evalScript('MDUX_cppResetScale()');
                if (selectionStatus) selectionStatus.textContent = result || 'Parts scale reset';
                if (teScaleSlider) teScaleSlider.value = 100;
                if (teScaleInput) teScaleInput.value = 100;
                lastSelectionScale = 100;
                teScaleDirty = false;
            } catch (e) {
                        if (selectionStatus) selectionStatus.textContent = 'Error: ' + e.message;
                    }
                });
            }

            // Collapsible Document Scale Anchor section
            if (docScaleToggle) {
                docScaleToggle.addEventListener('click', () => {
                    const content = docScaleSection.querySelector('.collapsible-content');
                    const titleSpan = docScaleToggle.querySelector('span');
                    if (docScaleSection.classList.contains('collapsed')) {
                        docScaleSection.classList.remove('collapsed');
                        content.style.display = 'block';
                        titleSpan.textContent = 'â–¼ Document Scale Anchor';
                    } else {
                        docScaleSection.classList.add('collapsed');
                        content.style.display = 'none';
                        titleSpan.textContent = 'â–¶ Document Scale Anchor';
                    }
                });
            }

            // Debug buttons
            const testNoteBtn = document.getElementById('test-note-btn');
            const viewLogBtn = document.getElementById('view-log-btn');
            const clearLogBtn = document.getElementById('clear-log-btn');
            const reloadJsxBtn = document.getElementById('reload-jsx-btn');
            const debugLogModal = document.getElementById('debug-log-modal');
            const closeLogBtn = document.getElementById('close-log-btn');
            const debugLogContent = document.getElementById('debug-log-content');

            if (testNoteBtn) {
                testNoteBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        const result = await evalScript('MDUX_testNoteProperty()');
                        alert(result);
                    } catch (e) {
                        alert('Error: ' + e.message);
                    }
                });
            }

            if (viewLogBtn) {
                viewLogBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        const log = await evalScript('MDUX_getDebugLog()');
                        debugLogContent.value = log;
                        debugLogModal.style.display = 'block';
                    } catch (e) {
                        alert('Error: ' + e.message);
                    }
                });
            }

            if (clearLogBtn) {
                clearLogBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        await evalScript('MDUX_clearDebugLog()');
                        debugStatus.textContent = 'Debug log cleared';
                    } catch (e) {
                        alert('Error: ' + e.message);
                    }
                });
            }

            if (resetSessionBtn) {
                resetSessionBtn.addEventListener('click', async () => {
                    try {
                        await ensureBridgeLoaded();
                        const result = await evalScript('MDUX_resetSessionStateBridge()');
                        debugStatus.textContent = result || 'Session state reset';
                        scheduleSkipOrthoRefresh();
                        refreshDebugLoggingState().catch(function () { });
                        refreshYieldToUiState().catch(function () { });
                    } catch (e) {
                        debugStatus.textContent = 'Reset failed: ' + e.message;
                    }
                });
            }

            if (reloadJsxBtn) {
                reloadJsxBtn.addEventListener('click', async () => {
                    try {
                        await forceReloadScripts();
                    } catch (e) {
                        debugStatus.textContent = 'Reload JSX failed: ' + e.message;
                    }
                });
            }

            if (closeLogBtn) {
                closeLogBtn.addEventListener('click', () => {
                    debugLogModal.style.display = 'none';
                });
            }

            if (debugLoggingOption) {
                debugLoggingOption.addEventListener('change', () => {
                    setDebugLoggingState(debugLoggingOption.checked);
                });
            }

            if (yieldToUiOption) {
                yieldToUiOption.addEventListener('change', () => {
                    setYieldToUiState(yieldToUiOption.checked);
                });
            }

            csInterface.evalScript('MDUX_debugLog("[INIT] Debug buttons attached")', function() {});

            // Gap Editor Overlay button handlers
            function updateGapEditorUI(inEditMode) {
                isInGapEditMode = inEditMode;
                if (gapEditorNormal) gapEditorNormal.style.display = inEditMode ? 'none' : 'block';
                if (gapEditorActive) gapEditorActive.style.display = inEditMode ? 'block' : 'none';
            }

            if (editGapsBtn) {
                editGapsBtn.addEventListener('click', async function() {
                    try {
                        await ensureBridgeLoaded();
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Entering gap edit mode...';
                            protectionStatus.style.color = '#f0f';
                        }
                        const resultStr = await evalScript('MDUX_enterGapEditMode()');
                        const result = JSON.parse(resultStr);
                        if (result.ok) {
                            updateGapEditorUI(true);
                            if (protectionStatus) {
                                protectionStatus.textContent = result.message;
                                protectionStatus.style.color = '#0f0';
                            }
                        } else {
                            if (protectionStatus) {
                                protectionStatus.textContent = result.message;
                                protectionStatus.style.color = '#f00';
                            }
                        }
                    } catch (e) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + e;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            if (saveGapsBtn) {
                saveGapsBtn.addEventListener('click', async function() {
                    try {
                        await ensureBridgeLoaded();
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Saving gaps...';
                            protectionStatus.style.color = '#f0f';
                        }
                        const resultStr = await evalScript('MDUX_saveGapEditMode()');
                        const result = JSON.parse(resultStr);
                        updateGapEditorUI(false);
                        if (protectionStatus) {
                            protectionStatus.textContent = result.message;
                            protectionStatus.style.color = result.ok ? '#0f0' : '#f00';
                            setTimeout(function() {
                                if (protectionStatus) protectionStatus.textContent = '';
                            }, 3000);
                        }
                    } catch (e) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + e;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            if (cancelGapsBtn) {
                cancelGapsBtn.addEventListener('click', async function() {
                    try {
                        await ensureBridgeLoaded();
                        const resultStr = await evalScript('MDUX_cancelGapEditMode()');
                        const result = JSON.parse(resultStr);
                        updateGapEditorUI(false);
                        if (protectionStatus) {
                            protectionStatus.textContent = result.message;
                            protectionStatus.style.color = result.ok ? '#ff0' : '#f00';
                            setTimeout(function() {
                                if (protectionStatus) protectionStatus.textContent = '';
                            }, 2000);
                        }
                    } catch (e) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + e;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            if (addGapBtn) {
                addGapBtn.addEventListener('click', async function() {
                    try {
                        await ensureBridgeLoaded();
                        const resultStr = await evalScript('MDUX_addGapMarker()');
                        const result = JSON.parse(resultStr);
                        if (protectionStatus) {
                            protectionStatus.textContent = result.message;
                            protectionStatus.style.color = result.ok ? '#0f0' : '#f00';
                            setTimeout(function() {
                                if (protectionStatus) protectionStatus.textContent = '';
                            }, 2000);
                        }
                    } catch (e) {
                        if (protectionStatus) {
                            protectionStatus.textContent = 'Error: ' + e;
                            protectionStatus.style.color = '#f00';
                        }
                    }
                });
            }

            // Check gap edit mode state on document switch
            async function checkGapEditModeState() {
                try {
                    const result = await evalScript('MDUX_isInGapEditMode()');
                    updateGapEditorUI(result === 'true');
                } catch (e) { }
            }

            if (debugStatus) debugStatus.textContent = 'Remote debugging available at http://localhost:8088';
            if (skipOrthoOption) {
                skipOrthoOption.indeterminate = false;
                skipOrthoOption.checked = false;
            }
            if (processSkipOrthoOption) processSkipOrthoOption.checked = !!(skipOrthoOption && skipOrthoOption.checked);
            if (rotationInput) {
                rotationInput.value = '';
                rotationInput.dataset.autoValue = '';
                rotationInput.dataset.multi = 'false';
            }
            if (skipAllBranchesOption) skipAllBranchesOption.checked = false;
            if (skipFinalOption) skipFinalOption.checked = false;  // Default to unchecked
            if (processSkipAllBranchesOption) processSkipAllBranchesOption.checked = !!(skipAllBranchesOption && skipAllBranchesOption.checked);
            if (processSkipFinalOption) processSkipFinalOption.checked = !!(skipFinalOption && skipFinalOption.checked);
            if (createRegisterWiresOption) createRegisterWiresOption.checked = false;
            if (debugLoggingOption) debugLoggingOption.checked = true;
            if (yieldToUiOption) yieldToUiOption.checked = true;
            // Scale controls are hidden - only set values if they exist
            if (scaleSlider) scaleSlider.value = 100;
            if (scaleLabel) scaleLabel.textContent = '100%';
            if (scaleInput) scaleInput.value = '';

            csInterface.evalScript('MDUX_debugLog("[INIT] State initialized")', function() {});

            // Initialize Transform Each controls
            resetTransformControls();

            setProcessStatus('');
            setRevertStatus('');
            setSelectionStatus('');
            setLayerStatus('');
            setImportStatus('');

            csInterface.evalScript('MDUX_debugLog("[INIT] About to load bridge...")', function() {});

            try {
                await ensureBridgeLoaded();
                csInterface.evalScript('MDUX_debugLog("[INIT] Bridge loaded successfully")', function() {});
            } catch (e) {
                var bridgeErrMsg = String(e && e.message ? e.message : e).replace(/'/g, "\\'").replace(/\n/g, " ");
                if (debugStatus) debugStatus.textContent = 'Bridge load failed: ' + bridgeErrMsg;
                csInterface.evalScript('MDUX_debugLog("[INIT] Bridge load failed: ' + bridgeErrMsg + '")', function() {});
            }

        startCepRuntime();
        ensureSuspendMonitor();
        ensureSelectionMonitor();
            csInterface.evalScript('MDUX_debugLog("[INIT] Event listeners registered")', function() {});

            refreshSkipOrthoState().catch(function () { });
            refreshRotationOverrideState().catch(function () { });
            refreshDebugLoggingState().catch(function () { });
            refreshYieldToUiState().catch(function () { });
            refreshDocScale().catch(function () { });
            refreshEmorySelectionState(true).catch(function () { });

            // Also refresh when panel gets focus (removed blocking debug log)
            window.addEventListener('focus', function() {
                if (!AUTO_SELECTION_REFRESH_ENABLED) return;
                if (isCepSuspended()) return;
                updateSkipSelectionRefresh().then(() => {
                    if (skipSelectionRefresh) return;
                    refreshSelectionTransformState().catch(function() {});
                    refreshRotationOverrideState().catch(function() {});
                    scheduleEmorySelectionRefresh(true);
                }).catch(function() {});
            });

            csInterface.evalScript('MDUX_debugLog("[INIT] Init complete!")', function() {});
            window.MDUX_INIT_DONE = true;
        } catch (initError) {
            // Escape single quotes in error message to avoid breaking the evalScript string
            var errMsg = String(initError && initError.message ? initError.message : initError);
            errMsg = errMsg.replace(/'/g, "\\'").replace(/\n/g, " ");
            csInterface.evalScript('MDUX_debugLog("[INIT] ERROR: ' + errMsg + '")', function() {});
        }
    }

    window.addEventListener('beforeunload', function () {
        stopCepRuntime();
        if (suspendMonitor) {
            clearInterval(suspendMonitor);
            suspendMonitor = null;
        }
        if (selectionMonitor) {
            clearInterval(selectionMonitor);
            selectionMonitor = null;
        }
        evalScript('MDUX_cleanupBridge()');
    });

    document.addEventListener('DOMContentLoaded', init);
})();

