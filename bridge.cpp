#define DIRECTINPUT_VERSION 0x0800
#include "steam_input_api.h"
#include <shlwapi.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <tlhelp32.h>
#include <dinput.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace {
HMODULE g_module = nullptr;
enum Key {
  KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE, KEY_ALT, KEY_X, KEY_LSHIFT, KEY_LCONTROL,
  KEY_L, KEY_C, KEY_R, KEY_M, KEY_1, KEY_2, KEY_3, KEY_4, KEY_ENTER, KEY_ESC,
  KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_COUNT
};

const WORD keyCodes[KEY_COUNT] = {
  'W', 'S', 'A', 'D', VK_SPACE, VK_MENU, 'X', VK_LSHIFT, VK_LCONTROL,
  'L', 'C', 'R', 'M', '1', '2', '3', '4', VK_RETURN, VK_ESCAPE,
  VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT
};

const BYTE dikCodes[KEY_COUNT] = {
  DIK_W, DIK_S, DIK_A, DIK_D, DIK_SPACE, DIK_LMENU, DIK_X, DIK_LSHIFT, DIK_LCONTROL,
  DIK_L, DIK_C, DIK_R, DIK_M, DIK_1, DIK_2, DIK_3, DIK_4, DIK_RETURN, DIK_ESCAPE,
  DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT
};

struct DigitalAction {
  const char* name;
  Key key;
};

const DigitalAction digitalActions[] = {
  {"Accelerate", KEY_W}, {"Brake", KEY_S}, {"SteerLeft", KEY_A}, {"SteerRight", KEY_D},
  {"Handbrake", KEY_SPACE}, {"Nitrous", KEY_ALT}, {"Speedbreaker", KEY_X},
  {"ShiftUp", KEY_LSHIFT}, {"ShiftDown", KEY_LCONTROL}, {"LookBack", KEY_L},
  {"Camera", KEY_C}, {"Reset", KEY_R}, {"Map", KEY_M}, {"Options", KEY_ESC},
  {"Powerup1", KEY_1}, {"Powerup2", KEY_2}, {"Powerup3", KEY_3}, {"Powerup4", KEY_4},
  {"Accept", KEY_ENTER}, {"Back", KEY_ESC}, {"MenuUp", KEY_UP}, {"MenuDown", KEY_DOWN},
  {"MenuLeft", KEY_LEFT}, {"MenuRight", KEY_RIGHT}
};

constexpr int digitalCount = sizeof(digitalActions) / sizeof(digitalActions[0]);
constexpr const char* kActionManifestFile = "game_actions_413080.vdf";

void Log(const char* format, ...) {
  char path[MAX_PATH] = {};
  GetModuleFileNameA(g_module, path, MAX_PATH);
  char* slash = std::strrchr(path, '\\');
  if (slash) std::strcpy(slash + 1, "SteamControllerBridge.log");
  FILE* file = nullptr;
  fopen_s(&file, path, "a");
  if (!file) return;
  va_list args;
  va_start(args, format);
  vfprintf(file, format, args);
  va_end(args);
  fputc('\n', file);
  fclose(file);
}

bool GameIsForeground() {
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &pid);
  return pid == GetCurrentProcessId();
}

bool InstallActionManifest(const char* source) {
  char steamPath[MAX_PATH] = {};
  DWORD size = MAX_PATH;
  if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, steamPath, &size) != ERROR_SUCCESS)
    return false;
  char directory[MAX_PATH] = {};
  snprintf(directory, sizeof(directory), "%s\\controller_config", steamPath);
  CreateDirectoryA(directory, nullptr);
  char destination[MAX_PATH] = {};
  snprintf(destination, sizeof(destination), "%s\\%s", directory, kActionManifestFile);
  if (CopyFileA(source, destination, FALSE) != 0) return true;
  return PathFileExistsA(destination) != 0;
}

void ApplyKeys(std::uint32_t& pressed, std::uint32_t desired) {
  std::uint32_t changed = pressed ^ desired;
  if (!changed) return;
  INPUT inputs[KEY_COUNT] = {};
  int count = 0;
  for (int i = 0; i < KEY_COUNT; ++i) {
    if (!(changed & (1u << i))) continue;
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = keyCodes[i];
    inputs[count].ki.dwFlags = (desired & (1u << i)) ? 0 : KEYEVENTF_KEYUP;
    ++count;
  }
  if (count) SendInput(count, inputs, sizeof(INPUT));
  pressed = desired;
}

struct HidInterface {
  char path[MAX_PATH] = {};
  HANDLE handle = INVALID_HANDLE_VALUE;
};

HidInterface hidInterfaces[8];
int hidInterfaceCount = 0;
HWND hidWindow = nullptr;
volatile std::uint32_t rawKeys = 0;
volatile std::int16_t rawLeftStickX = 0;
volatile std::int16_t rawLeftStickY = 0;
volatile std::int16_t rawRightStickX = 0;
volatile std::int16_t rawRightStickY = 0;
volatile std::uint16_t rawLeftTrigger = 0;
volatile std::uint16_t rawRightTrigger = 0;
volatile std::uint32_t rawButtons = 0;
volatile bool rawAnalogValid = false;
DWORD lastRawReport = 0;
DWORD lastHidCommand = 0;
DWORD lastHidRescan = 0;

enum TritonButton : std::uint32_t {
  TRITON_A = 0x00000001, TRITON_B = 0x00000002, TRITON_X = 0x00000004, TRITON_Y = 0x00000008,
  TRITON_R3 = 0x00000020, TRITON_VIEW = 0x00000040, TRITON_R4 = 0x00000080,
  TRITON_R5 = 0x00000100, TRITON_RB = 0x00000200,
  TRITON_DPAD_DOWN = 0x00000400, TRITON_DPAD_RIGHT = 0x00000800,
  TRITON_DPAD_LEFT = 0x00001000, TRITON_DPAD_UP = 0x00002000,
  TRITON_MENU = 0x00004000, TRITON_L3 = 0x00008000,
  TRITON_L4 = 0x00020000, TRITON_L5 = 0x00040000, TRITON_LB = 0x00080000,
  TRITON_RIGHT_PAD_TOUCH = 0x00200000, TRITON_RIGHT_PAD_CLICK = 0x00400000,
  TRITON_LEFT_PAD_TOUCH = 0x02000000, TRITON_LEFT_PAD_CLICK = 0x04000000
};

std::uint16_t ReadU16(const BYTE* data) {
  return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}

std::int16_t ReadS16(const BYTE* data) {
  return static_cast<std::int16_t>(ReadU16(data));
}

void AddPadCross(std::uint32_t& keys, std::int16_t x, std::int16_t y, bool active,
                 Key left, Key right, Key up, Key down) {
  constexpr int deadzone = 6000;
  if (!active) return;
  int ax = std::abs(x);
  int ay = std::abs(y);
  if (ax > deadzone && ax > ay * 5 / 4) keys |= 1u << (x < 0 ? left : right);
  else if (ay > deadzone) keys |= 1u << (y > 0 ? up : down);
}

void UpdateRawKeys(const BYTE* data, UINT size) {
  if (size < 46 || (data[0] != 0x42 && data[0] != 0x45 && data[0] != 0x47)) return;
  std::uint32_t buttons = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
  std::uint16_t leftTrigger = ReadU16(data + 6);
  std::uint16_t rightTrigger = ReadU16(data + 8);
  std::int16_t stickX = ReadS16(data + 10);
  std::int16_t stickY = ReadS16(data + 12);
  std::int16_t rightStickX = ReadS16(data + 14);
  std::int16_t rightStickY = ReadS16(data + 16);
  std::int16_t leftPadX = ReadS16(data + 18);
  std::int16_t leftPadY = ReadS16(data + 20);
  std::int16_t rightPadX = ReadS16(data + 24);
  std::int16_t rightPadY = ReadS16(data + 26);

  std::uint32_t keys = 0;
  if (rightTrigger > 8000) keys |= 1u << KEY_W;
  if (leftTrigger > 8000) keys |= 1u << KEY_S;
  if (stickX < -8000) keys |= 1u << KEY_A;
  if (stickX > 8000) keys |= 1u << KEY_D;
  if (buttons & (TRITON_R4 | TRITON_R5)) keys |= 1u << KEY_SPACE;
  if (buttons & TRITON_A) keys |= (1u << KEY_SPACE) | (1u << KEY_ENTER);
  if (buttons & TRITON_B) keys |= (1u << KEY_ALT) | (1u << KEY_ESC);
  if (buttons & TRITON_X) keys |= 1u << KEY_X;
  if (buttons & TRITON_Y) keys |= 1u << KEY_M;
  if (buttons & TRITON_LB) keys |= 1u << KEY_L;
  if (buttons & TRITON_RB) keys |= 1u << KEY_C;
  if (buttons & TRITON_L3) keys |= 1u << KEY_R;
  if (buttons & TRITON_MENU) keys |= 1u << KEY_ESC;

  AddPadCross(keys, leftPadX, leftPadY, (buttons & TRITON_LEFT_PAD_TOUCH) != 0,
              KEY_2, KEY_3, KEY_1, KEY_4);
  if (buttons & TRITON_DPAD_UP) keys |= 1u << KEY_1;
  if (buttons & TRITON_DPAD_LEFT) keys |= 1u << KEY_2;
  if (buttons & TRITON_DPAD_RIGHT) keys |= 1u << KEY_3;
  if (buttons & TRITON_DPAD_DOWN) keys |= 1u << KEY_4;

  AddPadCross(keys, rightPadX, rightPadY, (buttons & TRITON_RIGHT_PAD_TOUCH) != 0,
              KEY_LEFT, KEY_RIGHT, KEY_LSHIFT, KEY_LCONTROL);
  if (buttons & TRITON_RIGHT_PAD_CLICK) keys |= 1u << KEY_ENTER;

  rawLeftStickX = stickX;
  rawLeftStickY = stickY;
  rawRightStickX = rightStickX;
  rawRightStickY = rightStickY;
  rawLeftTrigger = leftTrigger;
  rawRightTrigger = rightTrigger;
  rawButtons = buttons;
  rawAnalogValid = true;
  rawKeys = keys;
  lastRawReport = GetTickCount();
}

LRESULT CALLBACK RawInputProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message != WM_INPUT) return DefWindowProcA(window, message, wParam, lParam);
  UINT size = 0;
  if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0) return 0;
  auto* buffer = static_cast<BYTE*>(malloc(size));
  if (!buffer) return 0;
  if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size) {
    auto* input = reinterpret_cast<const RAWINPUT*>(buffer);
    if (input->header.dwType == RIM_TYPEHID) UpdateRawKeys(input->data.hid.bRawData, input->data.hid.dwSizeHid);
  }
  free(buffer);
  return 0;
}

void CloseHidInterfaces() {
  for (int i = 0; i < hidInterfaceCount; ++i) {
    if (hidInterfaces[i].handle != INVALID_HANDLE_VALUE) CloseHandle(hidInterfaces[i].handle);
  }
  hidInterfaceCount = 0;
}

void OpenHidInterfaces() {
  CloseHidInterfaces();
  GUID guid = {};
  HidD_GetHidGuid(&guid);
  HDEVINFO devices = SetupDiGetClassDevsA(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (devices == INVALID_HANDLE_VALUE) return;
  SP_DEVICE_INTERFACE_DATA data = { sizeof(data) };
  for (DWORD index = 0; hidInterfaceCount < 8 && SetupDiEnumDeviceInterfaces(devices, nullptr, &guid, index, &data); ++index) {
    DWORD size = 0;
    SetupDiGetDeviceInterfaceDetailA(devices, &data, nullptr, 0, &size, nullptr);
    auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(malloc(size));
    if (!detail) continue;
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
    if (SetupDiGetDeviceInterfaceDetailA(devices, &data, detail, size, nullptr, nullptr)) {
      HANDLE handle = CreateFileA(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
      if (handle != INVALID_HANDLE_VALUE) {
        HIDD_ATTRIBUTES attributes = { sizeof(attributes) };
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (HidD_GetAttributes(handle, &attributes) && attributes.VendorID == 0x28DE && attributes.ProductID == 0x1304 &&
            HidD_GetPreparsedData(handle, &preparsed)) {
          HIDP_CAPS caps = {};
          HidP_GetCaps(preparsed, &caps);
          if (caps.UsagePage == 0xFF00 && caps.Usage == 1) {
            strcpy_s(hidInterfaces[hidInterfaceCount].path, detail->DevicePath);
            hidInterfaces[hidInterfaceCount].handle = handle;
            ++hidInterfaceCount;
            handle = INVALID_HANDLE_VALUE;
          }
          HidD_FreePreparsedData(preparsed);
        }
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
      }
    }
    free(detail);
  }
  SetupDiDestroyDeviceInfoList(devices);
  lastHidRescan = GetTickCount();
}

bool InitRawHid() {
  WNDCLASSA windowClass = {};
  windowClass.lpfnWndProc = RawInputProc;
  windowClass.hInstance = GetModuleHandleA(nullptr);
  windowClass.lpszClassName = "SoapboxSteamControllerRawInput";
  RegisterClassA(&windowClass);
  hidWindow = CreateWindowExA(0, windowClass.lpszClassName, "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
  if (!hidWindow) return false;
  RAWINPUTDEVICE device = { 0xFF00, 1, RIDEV_INPUTSINK, hidWindow };
  if (!RegisterRawInputDevices(&device, 1, sizeof(device))) return false;
  OpenHidInterfaces();
  Log("raw HID interfaces=%d", hidInterfaceCount);
  return hidInterfaceCount > 0;
}

void SendHidFeature(HANDLE handle, const BYTE* data, size_t size) {
  BYTE report[64] = {};
  memcpy(report, data, size);
  HidD_SetFeature(handle, report, sizeof(report));
}

void MaintainRawHid(bool foreground) {
  MSG message = {};
  while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageA(&message);
  DWORD now = GetTickCount();
  if (!foreground) return;
  if (hidInterfaceCount == 0 && now - lastHidRescan >= 5000) OpenHidInterfaces();
  if (hidInterfaceCount == 0) {
    rawKeys = 0;
    rawAnalogValid = false;
    rawLeftStickX = rawLeftStickY = rawRightStickX = rawRightStickY = 0;
    rawLeftTrigger = rawRightTrigger = 0;
    rawButtons = 0;
    return;
  }
  if (lastRawReport && now - lastRawReport < 250) return;
  if (now - lastHidCommand < 800) return;

  const BYTE clearMappings[] = { 0x01, 0x81, 0x00 };
  const BYTE trackpadsOff[] = { 0x01, 0x87, 0x06, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00 };
  const BYTE lizardOff[] = { 0x01, 0x87, 0x03, 0x09, 0x00, 0x00 };
  for (int i = 0; i < hidInterfaceCount; ++i) {
    SendHidFeature(hidInterfaces[i].handle, clearMappings, sizeof(clearMappings));
    SendHidFeature(hidInterfaces[i].handle, trackpadsOff, sizeof(trackpadsOff));
    SendHidFeature(hidInterfaces[i].handle, lizardOff, sizeof(lizardOff));
  }
  lastHidCommand = now;
}

using DirectInput8CreateFn = HRESULT (WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using CreateDeviceFn = HRESULT (__stdcall*)(void*, REFGUID, LPDIRECTINPUTDEVICE8A*, LPUNKNOWN);
using GetDeviceStateFn = HRESULT (__stdcall*)(IDirectInputDevice8A*, DWORD, LPVOID);
using GetJoystickStateFn = HRESULT (__stdcall*)(IDirectInputDevice8A*, DWORD, LPVOID);
using GetAsyncKeyStateFn = SHORT (WINAPI*)(int);
using GetKeyStateFn = SHORT (WINAPI*)(int);
using GetKeyboardStateFn = BOOL (WINAPI*)(PBYTE);
using InputMappingFn = void (__fastcall*)(void*, void*);

DirectInput8CreateFn originalDirectInput8Create = nullptr;
CreateDeviceFn originalCreateDevice = nullptr;
GetDeviceStateFn originalGetDeviceState = nullptr;
GetJoystickStateFn originalGetJoystickState = nullptr;
GetAsyncKeyStateFn originalGetAsyncKeyState = nullptr;
GetKeyStateFn originalGetKeyState = nullptr;
GetKeyboardStateFn originalGetKeyboardState = nullptr;
InputMappingFn originalInputMapping = nullptr;
bool analogMappingHooked = false;
bool PatchObjectVtable(void* object, int count, int slot, void* hook);
int createDeviceLogCount = 0;
void** directInputVtable = nullptr;
void** keyboardVtable = nullptr;
void** joystickVtable = nullptr;
bool keyboardHooked = false;
bool joystickHooked = false;
DWORD joystickStateCalls = 0;
DWORD joystickStateSize = 0;

constexpr std::uint32_t movementKeys = (1u << KEY_W) | (1u << KEY_S) | (1u << KEY_A) | (1u << KEY_D);

constexpr int kSteerLeftAction = 1;
constexpr int kSteerRightAction = 2;
constexpr int kGasAction = 7;
constexpr int kBrakeAction = 8;

// shortcut: World offsets are build-specific; re-check after any nfsw.exe update.
struct ActionNode {
  int action;
  float* value;
};

void __fastcall ApplyAnalogMapping(void* manager, void*) {
  if (!rawAnalogValid) return;

  // World's mapping nodes store Action at +20 and CurrentValue at +36.
  auto* list = *reinterpret_cast<BYTE**>(static_cast<BYTE*>(manager) + 0x270);
  if (!list) return;
  ActionNode steerNodes[32] = {};
  ActionNode pedalNodes[32] = {};
  int steerCount = 0;
  int pedalCount = 0;
  bool keyboardSteering = false;
  bool keyboardPedals = false;
  BYTE* node = *reinterpret_cast<BYTE**>(list);
  for (int count = 0; node && node != list && count < 256; ++count) {
    int action = *reinterpret_cast<int*>(node + 20);
    float* value = reinterpret_cast<float*>(node + 36);
    if (action == kSteerLeftAction || action == kSteerRightAction) {
      if (steerCount < 32) steerNodes[steerCount++] = { action, value };
      if (*value > 0.01f) keyboardSteering = true;
    } else if (action == kGasAction || action == kBrakeAction) {
      if (pedalCount < 32) pedalNodes[pedalCount++] = { action, value };
      if (*value > 0.01f) keyboardPedals = true;
    }
    node = *reinterpret_cast<BYTE**>(node);
  }
  if (!steerCount || !pedalCount) {
    static bool reported = false;
    if (!reported) {
      Log("InputMapping driving actions missing: steer=%d pedals=%d", steerCount, pedalCount);
      reported = true;
    }
    return;
  }
  static bool reported = false;
  if (!reported) {
    Log("InputMapping driving actions found: steer=%d pedals=%d", steerCount, pedalCount);
    reported = true;
  }

  float left = rawLeftStickX < 0 ? -static_cast<float>(rawLeftStickX) / 32768.0f : 0.0f;
  float right = rawLeftStickX > 0 ? static_cast<float>(rawLeftStickX) / 32768.0f : 0.0f;
  float gas = rawRightTrigger / 32767.0f;
  float brake = rawLeftTrigger / 32767.0f;

  // Physical keyboard input keeps priority over the controller.
  if (!keyboardSteering) {
    for (int i = 0; i < steerCount; ++i) {
      if (steerNodes[i].action == kSteerLeftAction) *steerNodes[i].value = left;
      else *steerNodes[i].value = right;
    }
  }
  if (!keyboardPedals) {
    for (int i = 0; i < pedalCount; ++i) {
      if (pedalNodes[i].action == kGasAction) *pedalNodes[i].value = gas;
      else *pedalNodes[i].value = brake;
    }
  }

  static float lastLeft = -1, lastRight = -1, lastGas = -1, lastBrake = -1;
  if (std::abs(left - lastLeft) > 0.1f || std::abs(right - lastRight) > 0.1f ||
      std::abs(gas - lastGas) > 0.1f || std::abs(brake - lastBrake) > 0.1f) {
    Log("analog left=%.3f right=%.3f gas=%.3f brake=%.3f", left, right, gas, brake);
    lastLeft = left; lastRight = right; lastGas = gas; lastBrake = brake;
  }
}

void __declspec(naked) HookInputMapping() {
  __asm {
    push ecx
    push [esp+8]
    call originalInputMapping
    mov ecx, [esp]
    add esp, 4
    mov edx, [esp+4]
    call ApplyAnalogMapping
    ret 4
  }
}

LONG AxisFromRaw(std::int16_t value, bool invert = false) {
  LONG axis = 32768 + (static_cast<LONG>(value) * 32767 / 32768);
  if (invert) axis = 65535 - axis;
  return axis < 0 ? 0 : (axis > 65535 ? 65535 : axis);
}

bool PatchObjectVtable(void* object, int count, int slot, void* hook) {
  if (!object) return false;
  void** oldVtable = *reinterpret_cast<void***>(object);
  if (!oldVtable || oldVtable[slot] == hook) return false;
  void** newVtable = static_cast<void**>(malloc(count * sizeof(void*)));
  if (!newVtable) return false;
  memcpy(newVtable, oldVtable, count * sizeof(void*));
  newVtable[slot] = hook;
  DWORD oldProtect = 0;
  if (!VirtualProtect(object, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
    free(newVtable);
    return false;
  }
  *reinterpret_cast<void***>(object) = newVtable;
  VirtualProtect(object, sizeof(void*), oldProtect, &oldProtect);
  return true;
}

HRESULT __stdcall HookGetDeviceState(IDirectInputDevice8A* device, DWORD size, LPVOID data) {
  HRESULT result = originalGetDeviceState(device, size, data);
  if (SUCCEEDED(result) && data && size >= 256) {
    auto* keys = static_cast<BYTE*>(data);
    for (int i = 0; i < KEY_COUNT; ++i) {
      if ((analogMappingHooked || joystickStateCalls) && (movementKeys & (1u << i))) continue;
      if (rawKeys & (1u << i)) keys[dikCodes[i]] = 0x80;
    }
  }
  return result;
}

HRESULT __stdcall HookGetJoystickState(IDirectInputDevice8A* device, DWORD size, LPVOID data) {
  HRESULT result = originalGetJoystickState(device, size, data);
  ++joystickStateCalls;
  joystickStateSize = size;
  if (SUCCEEDED(result) && data && size >= sizeof(DIJOYSTATE) && rawAnalogValid) {
    auto* joy = reinterpret_cast<DIJOYSTATE*>(data);
    joy->lX = AxisFromRaw(rawLeftStickX);
    joy->lY = AxisFromRaw(rawLeftStickY);
    joy->lZ = AxisFromRaw(rawRightTrigger);
    joy->lRz = AxisFromRaw(rawLeftTrigger);
    joy->lRx = AxisFromRaw(rawRightStickX);
    joy->lRy = AxisFromRaw(rawRightStickY);
    joy->rglSlider[0] = joy->rglSlider[1] = 32768;
    joy->rgdwPOV[0] = -1;
    joy->rgdwPOV[1] = joy->rgdwPOV[2] = joy->rgdwPOV[3] = -1;
    memset(joy->rgbButtons, 0, sizeof(joy->rgbButtons));
    joy->rgbButtons[0] = (rawButtons & TRITON_A) ? 0x80 : 0;
    joy->rgbButtons[1] = (rawButtons & TRITON_B) ? 0x80 : 0;
    joy->rgbButtons[2] = (rawButtons & TRITON_X) ? 0x80 : 0;
    joy->rgbButtons[3] = (rawButtons & TRITON_Y) ? 0x80 : 0;
    joy->rgbButtons[4] = (rawButtons & TRITON_LB) ? 0x80 : 0;
    joy->rgbButtons[5] = (rawButtons & TRITON_RB) ? 0x80 : 0;
    joy->rgbButtons[6] = (rawButtons & TRITON_VIEW) ? 0x80 : 0;
    joy->rgbButtons[7] = (rawButtons & TRITON_MENU) ? 0x80 : 0;
    joy->rgbButtons[8] = (rawButtons & TRITON_L3) ? 0x80 : 0;
    joy->rgbButtons[9] = (rawButtons & TRITON_R3) ? 0x80 : 0;
    bool up = (rawButtons & TRITON_DPAD_UP) != 0;
    bool down = (rawButtons & TRITON_DPAD_DOWN) != 0;
    bool left = (rawButtons & TRITON_DPAD_LEFT) != 0;
    bool right = (rawButtons & TRITON_DPAD_RIGHT) != 0;
    if (up && !down && !left && !right) joy->rgdwPOV[0] = 0;
    else if (up && right) joy->rgdwPOV[0] = 4500;
    else if (right && !up && !down) joy->rgdwPOV[0] = 9000;
    else if (down && right) joy->rgdwPOV[0] = 13500;
    else if (down && !left && !right) joy->rgdwPOV[0] = 18000;
    else if (down && left) joy->rgdwPOV[0] = 22500;
    else if (left && !up && !down) joy->rgdwPOV[0] = 27000;
    else if (up && left) joy->rgdwPOV[0] = 31500;
  }
  return result;
}

HRESULT __stdcall HookCreateDevice(void* self, REFGUID guid, LPDIRECTINPUTDEVICE8A* device, LPUNKNOWN outer) {
  HRESULT result = originalCreateDevice(self, guid, device, outer);
  if (createDeviceLogCount < 8) {
    Log("CreateDevice guid=%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X result=%08X",
        guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7], result);
    ++createDeviceLogCount;
  }
  const GUID keyboardGuid = { 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
  const GUID mouseGuid = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
  if (SUCCEEDED(result) && device && IsEqualGUID(guid, keyboardGuid)) {
    void** oldVtable = *reinterpret_cast<void***>(*device);
    if (oldVtable != keyboardVtable && PatchObjectVtable(*device, 32, 9, HookGetDeviceState)) {
      keyboardVtable = *reinterpret_cast<void***>(*device);
      originalGetDeviceState = reinterpret_cast<GetDeviceStateFn>(oldVtable[9]);
      keyboardHooked = true;
    }
  } else if (SUCCEEDED(result) && device && !IsEqualGUID(guid, mouseGuid)) {
    void** oldVtable = *reinterpret_cast<void***>(*device);
    if (oldVtable != joystickVtable && PatchObjectVtable(*device, 32, 9, HookGetJoystickState)) {
      joystickVtable = *reinterpret_cast<void***>(*device);
      originalGetJoystickState = reinterpret_cast<GetJoystickStateFn>(oldVtable[9]);
      joystickHooked = true;
    }
  }
  return result;
}

HRESULT WINAPI HookDirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid, LPVOID* output, LPUNKNOWN outer) {
  HRESULT result = originalDirectInput8Create(instance, version, iid, output, outer);
  if (SUCCEEDED(result) && output && *output) {
    void** oldVtable = *reinterpret_cast<void***>(*output);
    if (oldVtable != directInputVtable && PatchObjectVtable(*output, 8, 3, HookCreateDevice)) {
      directInputVtable = *reinterpret_cast<void***>(*output);
      originalCreateDevice = reinterpret_cast<CreateDeviceFn>(oldVtable[3]);
    }
  }
  return result;
}

std::uint32_t KeyBitFromVk(int vk) {
  for (int i = 0; i < KEY_COUNT; ++i) if (keyCodes[i] == vk) return 1u << i;
  return 0;
}

SHORT WINAPI HookGetAsyncKeyState(int vk) {
  SHORT state = originalGetAsyncKeyState(vk);
  std::uint32_t bit = KeyBitFromVk(vk);
  if (!((analogMappingHooked || joystickStateCalls) && (bit & movementKeys)) && (rawKeys & bit)) state |= 0x8000;
  return state;
}

SHORT WINAPI HookGetKeyState(int vk) {
  SHORT state = originalGetKeyState(vk);
  std::uint32_t bit = KeyBitFromVk(vk);
  if (!((analogMappingHooked || joystickStateCalls) && (bit & movementKeys)) && (rawKeys & bit)) state |= 0x8000;
  return state;
}

BOOL WINAPI HookGetKeyboardState(PBYTE state) {
  BOOL result = originalGetKeyboardState(state);
  if (result && state) {
    for (int i = 0; i < KEY_COUNT; ++i) {
      if ((analogMappingHooked || joystickStateCalls) && (movementKeys & (1u << i))) continue;
      if (rawKeys & (1u << i)) state[keyCodes[i]] = 0x80;
    }
  }
  return result;
}

void HookImport(const char* dllName, const char* functionName, void* hook, void** original) {
  auto base = reinterpret_cast<DWORD_PTR>(GetModuleHandleA(nullptr));
  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
  auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return;
  auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!directory.VirtualAddress) return;
  auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
  for (; descriptor->Name; ++descriptor) {
    if (_stricmp(reinterpret_cast<const char*>(base + descriptor->Name), dllName)) continue;
    auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
    auto* addressThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
    for (; nameThunk->u1.AddressOfData; ++nameThunk, ++addressThunk) {
      if (nameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
      auto* importName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
      if (strcmp(importName->Name, functionName)) continue;
      DWORD oldProtect = 0;
      if (!VirtualProtect(&addressThunk->u1.Function, sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect)) return;
      if (original) *original = reinterpret_cast<void*>(addressThunk->u1.Function);
      addressThunk->u1.Function = reinterpret_cast<DWORD_PTR>(hook);
      VirtualProtect(&addressThunk->u1.Function, sizeof(DWORD_PTR), oldProtect, &oldProtect);
      return;
    }
  }
}

void InitInputHooks() {
  DWORD_PTR base = reinterpret_cast<DWORD_PTR>(GetModuleHandleA(nullptr));
  BYTE* callSite = reinterpret_cast<BYTE*>(base + 0x35E50A);
  if (callSite[0] == 0xE8) {
    INT32 relative = 0;
    memcpy(&relative, callSite + 1, sizeof(relative));
    DWORD_PTR target = reinterpret_cast<DWORD_PTR>(callSite + 5 + relative);
    if (target == base + 0x35E1C0) {
      originalInputMapping = reinterpret_cast<InputMappingFn>(target);
      DWORD newRelative = reinterpret_cast<DWORD>(HookInputMapping) - static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(callSite) + 5);
      DWORD protect = 0;
      if (VirtualProtect(callSite, 5, PAGE_EXECUTE_READWRITE, &protect)) {
        memcpy(callSite + 1, &newRelative, sizeof(newRelative));
        VirtualProtect(callSite, 5, protect, &protect);
        FlushInstructionCache(GetCurrentProcess(), callSite, 5);
        analogMappingHooked = true;
        Log("InputMapping analog hook installed at 0x%08X", callSite);
      }
    } else {
      Log("InputMapping hook target mismatch: 0x%08X", target);
    }
  } else {
    Log("InputMapping hook site mismatch: 0x%02X", callSite[0]);
  }
  HookImport("DINPUT8.dll", "DirectInput8Create", HookDirectInput8Create, reinterpret_cast<void**>(&originalDirectInput8Create));
  HookImport("USER32.dll", "GetAsyncKeyState", HookGetAsyncKeyState, reinterpret_cast<void**>(&originalGetAsyncKeyState));
  HookImport("USER32.dll", "GetKeyState", HookGetKeyState, reinterpret_cast<void**>(&originalGetKeyState));
  HookImport("USER32.dll", "GetKeyboardState", HookGetKeyboardState, reinterpret_cast<void**>(&originalGetKeyboardState));
}

void LogModules() {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return;
  MODULEENTRY32W entry = {};
  entry.dwSize = sizeof(entry);
  if (Module32FirstW(snapshot, &entry)) {
    do {
      char name[MAX_PATH] = {};
      WideCharToMultiByte(CP_ACP, 0, entry.szModule, -1, name, sizeof(name), nullptr, nullptr);
      Log("module=%s", name);
    } while (Module32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
}

DWORD WINAPI BridgeThread(void*) {
  char manifest[MAX_PATH] = {};
  GetModuleFileNameA(g_module, manifest, MAX_PATH);
  char* slash = std::strrchr(manifest, '\\');
  if (slash) std::strcpy(slash + 1, kActionManifestFile);

  if (!InstallActionManifest(manifest)) Log("Could not install Steam Input action manifest");
  SteamInputApi api;
  bool steamReady = api.Init(manifest);
  if (!steamReady) Log("Steam Input initialization failed; continuing with raw HID");
  if (!InitRawHid()) Log("Raw HID initialization failed");
  LogModules();

  uint64_t digital[digitalCount] = {};
  if (steamReady) {
    for (int i = 0; i < digitalCount; ++i) digital[i] = api.getDigitalHandle(api.input, digitalActions[i].name);
  }

  const uint64_t actionSet = steamReady ? api.getActionSet(api.input, "Drive") : 0;
  std::uint32_t pressed = 0;
  DWORD lastLog = 0;
  uint64_t controller = 0;
  bool foreground = false;

  for (;;) {
    bool nowForeground = GameIsForeground();
    if (nowForeground != foreground) {
      rawKeys = 0;
      rawAnalogValid = false;
      rawLeftStickX = rawLeftStickY = rawRightStickX = rawRightStickY = 0;
      rawLeftTrigger = rawRightTrigger = 0;
      rawButtons = 0;
      lastRawReport = 0;
      lastHidCommand = 0;
      foreground = nowForeground;
    }
    MaintainRawHid(foreground);
    if (steamReady) {
      api.runCallbacks();
      api.runFrame(api.input, true);
    }
    uint64_t controllers[16] = {};
    int count = steamReady ? api.getConnected(api.input, controllers) : 0;
    controller = count > 0 ? controllers[0] : 0;
    std::uint32_t desired = foreground ? rawKeys : 0;
    int activeDigitals = 0;
    char pressedNames[512] = {};

    if (steamReady && controller && actionSet) {
      api.activateActionSet(api.input, controller, actionSet);
      for (int i = 0; i < digitalCount; ++i) {
        auto state = api.getDigitalData(api.input, controller, digital[i]);
        if (state.active) ++activeDigitals;
        if (state.active && state.state) {
          if (foreground) desired |= 1u << digitalActions[i].key;
          if (pressedNames[0]) strcat_s(pressedNames, ", ");
          strcat_s(pressedNames, digitalActions[i].name);
        }
      }
    }

    if (analogMappingHooked || joystickStateCalls) desired &= ~movementKeys;

    if (desired != pressed) Log("input changed keys=0x%08x raw=0x%08x actions=%s", desired, rawKeys, pressedNames[0] ? pressedNames : "none");
    ApplyKeys(pressed, desired);
    DWORD now = GetTickCount();
    if (now - lastLog >= 1000) {
      lastLog = now;
      Log("controllers=%d foreground=%d hid=%d rawAge=%lu dinput=%d mapping=%d joy=%d/%lu analog=%d activeActions=%d keys=0x%08x",
          count, foreground, hidInterfaceCount, lastRawReport ? now - lastRawReport : 9999, keyboardHooked,
          analogMappingHooked, joystickHooked, joystickStateSize, rawAnalogValid, activeDigitals, pressed);
    }
    Sleep(8);
  }
}
}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_module = module;
    DisableThreadLibraryCalls(module);
    InitInputHooks();
    CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
  }
  return TRUE;
}
