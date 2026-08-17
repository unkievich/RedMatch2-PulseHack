# Pulse Hack

Internal DX11/ImGui module for Redmatch 2.

## Project layout

```text
include/pulse/          Public plugin API
src/main.cpp            DLL entry point and DX11 hook lifecycle
src/features/esp.*      ESP, player cache and game data
src/features/aimbot.*   Silent aim settings and shot hooks
src/features/movement.* Speed, fly, third-person and movement hooks
src/ui/                 Menu, blur and embedded UI assets
src/ui/assets/font/     Text and icon fonts
src/ui/assets/logo/     Pulse logo
vendor/imgui/           Vendored ImGui dependency for offline builds
```

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Release output:

```text
build/bin/Release/pulse-hack.dll
```
