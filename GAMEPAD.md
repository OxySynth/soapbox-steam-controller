# Steam Controller support

`SteamControllerInput.asi` reads the Steam Controller Puck directly through Windows HID. Stick and trigger values are written into the game's own analog action mapping; other buttons use the game's keyboard path. The native keyboard controls remain available, and the last valid HID state is retained while the game is focused so mouse use and alt-tab do not reset driving.

The 2010 game officially supported the Xbox 360 Controller for Windows and a short list of racing wheels, not the Steam Controller. The previous NFS-XtendedInput plugin remains disabled because its World input replacement broke both keyboard and controller input.

## Start normally

Keep Steam running, then start the game with `SBRW.Launcher.exe` from your Soapbox installation.

A Steam shortcut or Steam overlay is not required. If the controller does not respond after an update, restart Steam once so it reloads the installed layout, then start the game again.

## Default layout

| Controller input | Game action |
| --- | --- |
| Left stick left / right | Steer left / right |
| Right trigger | Accelerate |
| Left trigger | Brake / reverse |
| R4 / R5 rear paddles | Handbrake |
| Left pad up | Power-up slot 1 |
| Left pad left | Power-up slot 2 |
| Left pad right | Power-up slot 3 |
| Left pad down | Power-up slot 4 |
| A | Handbrake; Accept in menus |
| B | Nitrous; Back in menus |
| X | Speedbreaker |
| Y | World map |
| Left bumper | Look back |
| Right bumper | Change camera |
| Left stick click | Reset car |
| Right pad up / down | Shift up / down |
| Right pad left / right | Menu left / right |
| Right pad click | Accept in menus |
| Menu button | Pause / options |

Steering, throttle, and brake are analog. Physical WASD input takes priority while it is held. The left cross always activates the current contents of power-up slots 1–4; it is not tied to a specific power-up such as Nitrous or Juggernaut.

## Troubleshooting

The bridge writes `scripts\SteamControllerBridge.log`. `hid=4` means the four Puck interfaces were found. `dinput=1` means the game's DirectInput keyboard hook is active. `mapping=1` means the analog action hook is installed. `rawAge` may grow while the controller is idle because the Puck reports changes rather than streaming continuously; the last valid analog position is intentionally retained until focus is lost. `controllers=1` is Steam's optional controller count and is not required for driving.

The analog hook is build-specific. If `nfsw.exe` is updated and `mapping=0` appears, re-check the hook offsets before using controller support.

Known issue: on the current Puck firmware, left-pad touch can still duplicate the X/Speedbreaker action. This is not yet fixed; the raw bridge itself only maps the left pad to power-up slots.

The Steam Input actions are installed as `game_actions_413080.vdf` in Steam's `controller_config` folder. The layout is installed for the Steam Controller Configs app; the previous layout is backed up as `controller_steamcontroller_gordon.vdf.bak` in that app's user config folder.

To disable controller support, rename `scripts\SteamControllerInput.asi` to `SteamControllerInput.asi.disabled` and restart the game.
