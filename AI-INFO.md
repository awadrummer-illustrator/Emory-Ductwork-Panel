# Emory Ductwork Panel - AI Information

## What This Project Is

This project is the Illustrator fork for Emory ductwork. It keeps the proven Magic panel/tooling where that behavior is still correct, and replaces only the duct geometry engine.

## What Stays Shared With Magic

- register and thermostat workflows
- orthogonalizing and carve logic
- layer and selection utilities
- CEP panel structure
- native Illustrator plugin shell

## What Must Become Emory-Specific

- thick duct body generation
- elbow and connector geometry
- segment-width cascading
- color/styling rules for Emory duct bodies
- regenerate/update behavior for edited centerlines

## Current State

- CEP extension IDs and deployment paths are renamed to `Emory-Ductwork-Panel`
- native script-message plugin name is renamed to `EmoryDuctwork`
- native build output is renamed to `EmoryDuctwork.aip`
- the main process action now runs natively and builds locked Emory duct bodies from selected centerlines
- generated Emory bodies are tagged for in-place delete/rebuild on rerun
- color-layer centerlines use the Emory fill/stroke palette in native styles and body fills

## Important Constraint

The old hidden `process-emory` logic copied from Magic is not a valid base. Do not revive it as-is. Port the Emory geometry cleanly instead of trying to patch that dead path.

## Files That Matter First

- `index.html`
- `js/panel.js`
- `jsx/panel-bridge.jsx`
- `jsx/magic-final.jsx`
- `cpp-plugin/src/ProcessDuctwork/EmoryDuctwork.vcxproj`
- `cpp-plugin/src/ProcessDuctwork/Source/ProcessDuctworkPlugin.cpp`
- `cpp-plugin/src/ProcessDuctwork/Source/EmoryDuctworkPanel.cpp`
- `cpp-plugin/src/ProcessDuctwork/Source/ProcessDuctworkGeometry.cpp`

## Build / Deploy

- CEP deploy target: `C:\Users\Chris\AppData\Roaming\Adobe\CEP\extensions\Emory-Ductwork-Panel`
- Native plugin target: `C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\DuctworkMenu\EmoryDuctwork.aip`

The Illustrator SDK is present in the mirrored `cpp-plugin\sdk` path used by the native project.
