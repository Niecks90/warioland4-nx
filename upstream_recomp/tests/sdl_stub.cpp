#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
// Stub implementations for headless contract testing without SDL2.dll dependency
void* SDL_CreateRGBSurfaceFrom(void*, int, int, int, int, uint32_t, uint32_t, uint32_t, uint32_t) { return nullptr; }
void* SDL_CreateWindow(const char*, int, int, int, int, uint32_t) { return (void*)1; }
void SDL_DestroyWindow(void*) {}
void SDL_FreeSurface(void*) {}
int SDL_GetDisplayDPI(int, float*, float*, float*) { return -1; }
int SDL_GetDisplayUsableBounds(int, void*) { return -1; }
const char* SDL_GetError() { return ""; }
const char* SDL_GetRevision() { return ""; }
void SDL_GetVersion(void*) {}
int SDL_GetWindowBordersSize(void*, int*, int*, int*, int*) { return -1; }
int SDL_GetWindowDisplayIndex(void*) { return 0; }
void SDL_GetWindowSize(void*, int* w, int* h) { if (w) *w = 240; if (h) *h = 160; }
void* SDL_GL_CreateContext(void*) { return (void*)1; }
void SDL_GL_DeleteContext(void*) {}
void SDL_GL_GetDrawableSize(void*, int* w, int* h) { if (w) *w = 240; if (h) *h = 160; }
int SDL_GL_MakeCurrent(void*, void*) { return 0; }
void SDL_GL_ResetAttributes() {}
int SDL_GL_SetAttribute(int, int) { return 0; }
int SDL_GL_SetSwapInterval(int) { return 0; }
void SDL_GL_SwapWindow(void*) {}
int SDL_Init(uint32_t) { return 0; }
void SDL_MaximizeWindow(void*) {}
void SDL_Quit() {}
void SDL_RaiseWindow(void*) {}
int SDL_SetHint(const char*, const char*) { return 1; }
int SDL_SetHintWithPriority(const char*, const char*, int) { return 1; }
void SDL_SetMainReady() {}
void SDL_SetWindowIcon(void*, void*) {}
void SDL_SetWindowPosition(void*, int, int) {}
void SDL_SetWindowSize(void*, int, int) {}
void SDL_GameControllerClose(void*) {}
int16_t SDL_GameControllerGetAxis(void*, int) { return 0; }
uint8_t SDL_GameControllerGetButton(void*, int) { return 0; }
int SDL_GameControllerGetSensorData(void*, int, float*, int) { return -1; }
int SDL_GameControllerHasSensor(void*, int) { return 0; }
char* SDL_GameControllerMapping(void*) { return nullptr; }
const char* SDL_GameControllerNameForIndex(int) { return ""; }
void* SDL_GameControllerOpen(int) { return nullptr; }
int SDL_GameControllerSetSensorEnabled(void*, int, int) { return -1; }
int SDL_IsGameController(int) { return 0; }
void SDL_JoystickGetDeviceGUID(int) {}
int SDL_JoystickGetDeviceInstanceID(int) { return -1; }
void SDL_JoystickGetGUIDString(int, char* out, int) { if (out) out[0] = 0; }
int SDL_NumJoysticks() { return 0; }
char* SDL_getenv(const char*) { return nullptr; }
int SDL_GetKeyFromName(const char*) { return 0; }
int SDL_GetScancodeFromKey(int) { return 0; }
uint32_t SDL_GetWindowID(void*) { return 1; }
int SDL_PushEvent(void*) { return 1; }
void SDL_WarpMouseInWindow(void*, int, int) {}
const char* SDL_GameControllerGetStringForAxis(int) { return "Axis"; }
const char* SDL_GameControllerGetStringForButton(int) { return "Btn"; }
const char* SDL_GetKeyName(int) { return "Key"; }
const char* SDL_GetScancodeName(int) { return "Key"; }
int SDL_GetScancodeFromName(const char*) { return 0; }
int SDL_atoi(const char* str) { return str ? std::atoi(str) : 0; }
char* SDL_GetBasePath() { return nullptr; }
const char* SDL_GetCurrentVideoDriver() { return "dummy"; }
uint32_t SDL_GetGlobalMouseState(int*, int*) { return 0; }
void* SDL_GetKeyboardFocus() { return nullptr; }
uint32_t SDL_GetWindowFlags(void*) { return 0; }
void SDL_GetWindowPosition(void*, int* x, int* y) { if (x) *x = 0; if (y) *y = 0; }
int SDL_PollEvent(void*) { return 0; }
int SDL_WaitEventTimeout(void*, int) { return 0; }
int SDL_CaptureMouse(int) { return 0; }
void* SDL_CreateSystemCursor(int) { return nullptr; }
void SDL_FreeCursor(void*) {}
char* SDL_GetClipboardText() { return strdup(""); }
uint64_t SDL_GetPerformanceCounter() { return 1000; }
uint64_t SDL_GetPerformanceFrequency() { return 1000; }
int SDL_GetRelativeMouseMode() { return 0; }
void SDL_GetRendererOutputSize(void*, int* w, int* h) { if (w) *w = 240; if (h) *h = 160; }
int SDL_GetWindowWMInfo(void*, void*) { return 0; }
int SDL_OpenURL(const char*) { return 0; }
int SDL_SetClipboardText(const char*) { return 0; }
void SDL_SetCursor(void*) {}
void SDL_SetTextInputRect(const void*) {}
int SDL_ShowCursor(int) { return 0; }
int SDL_Vulkan_GetDrawableSize(void*, int*, int*) { return 0; }
}
