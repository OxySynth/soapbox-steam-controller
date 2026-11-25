# Release installation

## Download

Download the latest release ZIP and extract:

- `SteamControllerInput.asi`
- `game_actions_413080.vdf`
- `GAMEPAD.md`

Do not install the C++ source, build script, or any `.obj` files.

## Install

1. Close Soapbox Race World and its launcher.
2. Open your Soapbox game folder.
3. If `scripts\SteamControllerInput.asi` already exists, rename the backup with a non-`.asi` extension, such as `SteamControllerInput.asi.bak`.
4. Copy `SteamControllerInput.asi` and `game_actions_413080.vdf` into the game's `scripts` folder.
5. Start the game with `SBRW.Launcher.exe` from your Soapbox installation.

Do not launch `nfsw.exe` directly; it does not provide the required server address/login token.

Only one controller bridge may be installed. Files such as `SteamControllerInput.backup.asi` are also loaded by the game and can cause conflicting input.

## Verify

Keep Steam running. After entering a race, check `scripts\SteamControllerBridge.log`:

- `hid=4` — all Puck HID interfaces were found.
- `dinput=1` — the keyboard input hook is active.
- `mapping=1` — the analog driving hook is active.

See [GAMEPAD.md](GAMEPAD.md) for controls and troubleshooting.

## Compatibility

This release targets the 32-bit `nfsw.exe` with SHA-256:

```text
B2C616C641D3512B4D1EDEC9AAB750F3308281752628ABC845142D1C434EC819
```

If the game executable is updated, the internal analog hook may need new offsets.

## Uninstall

1. Close the game and launcher.
2. Delete or rename `scripts\SteamControllerInput.asi`.
3. Restart the game.

The action manifest can also be removed from `scripts` if it is no longer needed.

## Known issue

On the current Puck firmware, left-pad touch can still duplicate the X/Speedbreaker action. The raw bridge itself maps the left pad only to power-up slots.
