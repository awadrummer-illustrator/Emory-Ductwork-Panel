# Build and Test

Use the full MSBuild path (plain `msbuild` is not on PATH in this shell):

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "E:\Work\Work\Custom Sketchup, Illustrator and Photoshop Scripts and Extensions\Illustrator\Extensions\Emory-Ductwork-Panel\cpp-plugin\src\ProcessDuctwork\EmoryDuctwork.vcxproj" /p:Configuration=Release /p:Platform=x64
```

User needs to Run the test loop from Admin Terminal(reload, launch, open test file):

```powershell
PowerShell -ExecutionPolicy Bypass -File "E:\Work\Work\Custom Sketchup, Illustrator and Photoshop Scripts and Extensions\Illustrator\Extensions\Emory-Ductwork-Panel\cpp-plugin\tools\reload-illustrator.ps1"
```
The companion interface for this extension is located at

E:\Work\Work\Custom Sketchup, Illustrator and Photoshop Scripts and Extensions\Illustrator\Extensions\Emory-Ductwork-Panel
