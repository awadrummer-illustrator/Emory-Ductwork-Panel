# Urgent AI Instructions!

After revisions automatically copy all changed CEP files from this project folder to `C:\Users\Chris\AppData\Roaming\Adobe\CEP\extensions\Emory-Ductwork-Panel`.

---

# Emory Ductwork Panel

Adobe Illustrator extension for the Emory ductwork workflow.

## Current Status

This project is a clean fork of `Magic-Ductwork-Panel`.

Shared Illustrator behavior is intentionally being reused:
- registers and thermostats
- orthogonalizing and carve tools
- selection and layer utilities
- existing CEP panel shell
- existing native C++ plugin shell

The main Emory process path now runs through the native C++ plugin. It generates locked Emory duct bodies from selected centerlines, preserves the shared Magic register/thermostat workflows, and can be rerun after centerline edits.

## Runtime Targets

- Development source: `E:\Work\Work\Custom Sketchup, Illustrator and Photoshop Scripts and Extensions\Illustrator\Extensions\Emory-Ductwork-Panel`
- CEP deployment: `C:\Users\Chris\AppData\Roaming\Adobe\CEP\extensions\Emory-Ductwork-Panel`
- Native plug-in output: `C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\DuctworkMenu\EmoryDuctwork.aip`

## Project Shape

- `CSXS/manifest.xml`: CEP manifest for `com.chris.emoryductwork.*`
- `index.html`, `js/`, `css/`: dockable Illustrator panel
- `jsx/`: ExtendScript bridge and shared Illustrator logic
- `cpp-plugin/`: native C++ Illustrator plugin baseline for the performance-critical port
- `python/`: copied from Magic for reference only; not the target long-term Emory engine

## Architecture Direction

- Shared Magic behaviors stay where they are unless Emory requires changes.
- Emory duct generation will move into the native C++ plugin because that is the fastest practical path in Illustrator.
- The CEP panel remains the UI shell and command surface.
- The ExtendScript bridge remains the compatibility layer for Illustrator document access and any legacy helpers still worth keeping.

See `ARCHITECTURE.md` for the Emory split between shared behavior and the new geometry engine.

## Deployment

Deploy the CEP files by mirroring this folder to:

```text
C:\Users\Chris\AppData\Roaming\Adobe\CEP\extensions\Emory-Ductwork-Panel
```

Excluded from deployment:
- `.git/`
- `.claude/`
- `.vscode/`
- `cpp-plugin/`
- `logs/`
- `node_modules/`
- `*.log`
- `README.md`
- `AI-INFO.md`
- `ARCHITECTURE.md`
- `DEPLOYMENT_INSTRUCTIONS.md`
- `js/debug-location.jsx`

## Native Build Notes

The native plugin project is prepared under:

```text
cpp-plugin\src\ProcessDuctwork\EmoryDuctwork.vcxproj
```

Requirements:
- Adobe Illustrator 2024 SDK
- Visual Studio 2022 with Desktop C++

This fork builds against the Illustrator SDK mirrored under `cpp-plugin\sdk`.
