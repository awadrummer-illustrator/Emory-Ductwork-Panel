# Emory Ductwork (Illustrator C++ plug-in)

C++ Illustrator plug-in scaffold for the Emory ductwork pipeline.

This project starts from the proven Magic native plugin shell, but the geometry port is not complete yet. Shared Illustrator behavior can stay borrowed from Magic; Emory duct generation is the part that now needs native implementation.

## Requirements
- Adobe Illustrator 2024 (v28)
- Adobe Illustrator SDK (Windows)
- Visual Studio 2022 with "Desktop development with C++"

## SDK Download (Manual)
Adobe hosts the SDK behind the Developer Console sign-in.

1) Open https://console.adobe.io/downloads/ai
2) Sign in, then use "Download Resources" to get the Illustrator SDK (Windows).
3) Save the ZIP somewhere local.

## SDK Setup
Extract the SDK into this repo with the helper script:

```powershell
.\scripts\setup-sdk.ps1 -SdkZip "C:\path\to\IllustratorSDK.zip"
```

This extracts to `sdk\IllustratorSDK` by default.

## Environment Variable (Optional)
Set `AI_SDK_ROOT` for your current shell:

```powershell
.\scripts\env.ps1
```

## Build Target
Final `.aip` goes here:
`C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\DuctworkMenu\EmoryDuctwork.aip`

(Requires admin privileges to copy into Program Files.)
