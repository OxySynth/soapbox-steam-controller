# Soapbox Steam Controller Bridge

Unofficial Steam Controller Puck support for Soapbox Race World.

The bridge reads the Puck directly through raw HID and injects:

- Analog steering, throttle, and brake into World's action mapping.
- Digital controls through the game's keyboard input path.
- A focus-aware HID heartbeat so the Puck stays in raw mode after alt-tab.

Physical keyboard controls remain active and take priority while held.

## Requirements

- 32-bit Windows build of Soapbox Race World.
- Visual Studio Build Tools with the x86 compiler and Windows SDK.
- Steam Controller Puck (`VID_28DE`, `PID_1304`).

## Build

Run `build.bat`. It uses `vswhere` to locate an installed Visual Studio/Build Tools C++ toolchain automatically. Alternatively, compile from any x86 Developer Command Prompt:

```bat
cl /nologo /LD /EHsc /O2 bridge.cpp steam_input_api.cpp ^
  /link /DLL /OUT:SteamControllerInput.asi ^
  advapi32.lib user32.lib shlwapi.lib setupapi.lib hid.lib
```

## Install

For end-user release installation, follow [RELEASE.md](RELEASE.md).

Copy `SteamControllerInput.asi` and `game_actions_413080.vdf` into the game's `scripts` folder, then start the game with `SBRW.Launcher.exe` from your Soapbox installation.

Launching `nfsw.exe` directly does not provide the required server address/login token.

## Controls

See [GAMEPAD.md](GAMEPAD.md).

## Compatibility

The internal analog hook is tied to this `nfsw.exe` build:

- SHA-256: `B2C616C641D3512B4D1EDEC9AAB750F3308281752628ABC845142D1C434EC819`

The hook validates its call target before patching. If the executable is updated, re-check the offsets before use.

## Known issue

On the current Puck firmware, left-pad touch can still duplicate the X/Speedbreaker action. The raw bridge itself maps the left pad only to power-up slots.
