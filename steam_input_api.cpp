#include "steam_input_api.h"
#include <shlwapi.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")

static HMODULE FindSteamApi() {
  HMODULE local = LoadLibraryA("scripts\\steam_api.dll");
  if (!local) local = LoadLibraryA("steam_api.dll");
  if (local) return local;
  char steamPath[MAX_PATH] = {};
  DWORD size = MAX_PATH;
  if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, steamPath, &size) == ERROR_SUCCESS) {
    std::string path = std::string(steamPath) + "\\steamapps\\common\\SteamVR\\bin\\win32\\steam_api.dll";
    HMODULE module = LoadLibraryA(path.c_str());
    if (module) return module;
  }
  wchar_t programFiles[MAX_PATH] = {};
  if (GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, MAX_PATH)) {
    std::wstring path = std::wstring(programFiles) + L"\\Steam\\steamapps\\common\\SteamVR\\bin\\win32\\steam_api.dll";
    HMODULE module = LoadLibraryW(path.c_str());
    if (module) return module;
  }
  return nullptr;
}

template <typename T>
static T Resolve(HMODULE module, const char* name) {
  return reinterpret_cast<T>(GetProcAddress(module, name));
}

bool SteamInputApi::Load() {
  if (module) return true;
  SetEnvironmentVariableA("SteamAppId", "413080");
  SetEnvironmentVariableA("SteamGameId", "413080");
  module = FindSteamApi();
  if (!module) return false;

  initInternal = Resolve<decltype(initInternal)>(module, "SteamInternal_SteamAPI_Init");
  initFlat = Resolve<decltype(initFlat)>(module, "SteamAPI_InitFlat");
  shutdown = Resolve<decltype(shutdown)>(module, "SteamAPI_Shutdown");
  runCallbacks = Resolve<decltype(runCallbacks)>(module, "SteamAPI_RunCallbacks");
  getInput = Resolve<decltype(getInput)>(module, "SteamAPI_SteamInput_v007");
  if (!getInput) getInput = Resolve<decltype(getInput)>(module, "SteamAPI_SteamInput_v006");
  inputInit = Resolve<decltype(inputInit)>(module, "SteamAPI_ISteamInput_Init");
  inputShutdown = Resolve<decltype(inputShutdown)>(module, "SteamAPI_ISteamInput_Shutdown");
  setManifest = Resolve<decltype(setManifest)>(module, "SteamAPI_ISteamInput_SetInputActionManifestFilePath");
  runFrame = Resolve<decltype(runFrame)>(module, "SteamAPI_ISteamInput_RunFrame");
  getConnected = Resolve<decltype(getConnected)>(module, "SteamAPI_ISteamInput_GetConnectedControllers");
  showBindingPanel = Resolve<decltype(showBindingPanel)>(module, "SteamAPI_ISteamInput_ShowBindingPanel");
  getActionSet = Resolve<decltype(getActionSet)>(module, "SteamAPI_ISteamInput_GetActionSetHandle");
  activateActionSet = Resolve<decltype(activateActionSet)>(module, "SteamAPI_ISteamInput_ActivateActionSet");
  getAnalogHandle = Resolve<decltype(getAnalogHandle)>(module, "SteamAPI_ISteamInput_GetAnalogActionHandle");
  getDigitalHandle = Resolve<decltype(getDigitalHandle)>(module, "SteamAPI_ISteamInput_GetDigitalActionHandle");
  getAnalogData = Resolve<decltype(getAnalogData)>(module, "SteamAPI_ISteamInput_GetAnalogActionData");
  getDigitalData = Resolve<decltype(getDigitalData)>(module, "SteamAPI_ISteamInput_GetDigitalActionData");

  return initInternal && shutdown && runCallbacks && getInput && inputInit && setManifest && runFrame && getConnected && getActionSet &&
         activateActionSet && getAnalogHandle && getDigitalHandle && getAnalogData && getDigitalData;
}

bool SteamInputApi::Init(const char* manifestPath) {
  if (!Load()) return false;
  char error[1024] = {};
  int result = initFlat ? initFlat(error) : (initInternal ? initInternal("", error) : -1);
  if (result != 0) return false;
  input = getInput();
  if (!input || !inputInit(input, true)) return false;
  setManifest(input, manifestPath);
  return true;
}

void SteamInputApi::Shutdown() {
  if (input && inputShutdown) inputShutdown(input);
  if (shutdown) shutdown();
  input = nullptr;
}
