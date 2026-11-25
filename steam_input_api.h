#pragma once
#include <windows.h>
#include <cstdint>

struct InputAnalogActionData {
  int mode;
  float x;
  float y;
  bool active;
};

struct InputDigitalActionData {
  bool state;
  bool active;
};

struct SteamInputApi {
  HMODULE module = nullptr;
  void* input = nullptr;

  int (__cdecl* initInternal)(const char*, char*) = nullptr;
  int (__cdecl* initFlat)(char*) = nullptr;
  void (__cdecl* shutdown)() = nullptr;
  void (__cdecl* runCallbacks)() = nullptr;
  void* (__cdecl* getInput)() = nullptr;
  bool (__cdecl* inputInit)(void*, bool) = nullptr;
  bool (__cdecl* inputShutdown)(void*) = nullptr;
  bool (__cdecl* setManifest)(void*, const char*) = nullptr;
  void (__cdecl* runFrame)(void*, bool) = nullptr;
  int (__cdecl* getConnected)(void*, uint64_t*) = nullptr;
  bool (__cdecl* showBindingPanel)(void*, uint64_t) = nullptr;
  uint64_t (__cdecl* getActionSet)(void*, const char*) = nullptr;
  void (__cdecl* activateActionSet)(void*, uint64_t, uint64_t) = nullptr;
  uint64_t (__cdecl* getAnalogHandle)(void*, const char*) = nullptr;
  uint64_t (__cdecl* getDigitalHandle)(void*, const char*) = nullptr;
  InputAnalogActionData (__cdecl* getAnalogData)(void*, uint64_t, uint64_t) = nullptr;
  InputDigitalActionData (__cdecl* getDigitalData)(void*, uint64_t, uint64_t) = nullptr;

  bool Load();
  bool Init(const char* manifestPath);
  void Shutdown();
};
