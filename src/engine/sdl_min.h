// Минимальные объявления SDL2: только то, чем пользуется игра.
//
// Зачем: проект собирается без установленных заголовков SDL2 — нужна лишь сама
// библиотека (libSDL2). ABI у SDL 2.0.x стабилен, поэтому объявления ниже
// совпадают с настоящими. Если хочется собирать с системными заголовками,
// включи в CMake опцию NORMA_SYSTEM_SDL_HEADERS (см. README).
#pragma once

#if defined(NORMA_SYSTEM_SDL_HEADERS)
#  define SDL_MAIN_HANDLED
#  include <SDL.h>
#else

#include <cstdint>

extern "C" {

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#if defined(_WIN32)
#  define SDLCALL __cdecl
#else
#  define SDLCALL
#endif

// ---- инициализация ----
#define SDL_INIT_TIMER 0x00000001u
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_EVENTS 0x00004000u

int SDLCALL SDL_Init(Uint32 flags);
void SDLCALL SDL_Quit(void);
const char* SDLCALL SDL_GetError(void);
void SDLCALL SDL_free(void* mem);
char* SDLCALL SDL_GetPrefPath(const char* org, const char* app);
char* SDLCALL SDL_GetBasePath(void);
SDL_bool SDLCALL SDL_SetHint(const char* name, const char* value);

// ---- время ----
Uint64 SDLCALL SDL_GetPerformanceCounter(void);
Uint64 SDLCALL SDL_GetPerformanceFrequency(void);
void SDLCALL SDL_Delay(Uint32 ms);

// ---- окно и контекст OpenGL ----
typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;

#define SDL_WINDOWPOS_CENTERED 0x2FFF0000

typedef enum {
    SDL_WINDOW_FULLSCREEN = 0x00000001,
    SDL_WINDOW_OPENGL = 0x00000002,
    SDL_WINDOW_SHOWN = 0x00000004,
    SDL_WINDOW_HIDDEN = 0x00000008,
    SDL_WINDOW_RESIZABLE = 0x00000020,
    SDL_WINDOW_FULLSCREEN_DESKTOP = (SDL_WINDOW_FULLSCREEN | 0x00001000),
    SDL_WINDOW_ALLOW_HIGHDPI = 0x00002000
} SDL_WindowFlags;

typedef enum {
    SDL_GL_RED_SIZE,
    SDL_GL_GREEN_SIZE,
    SDL_GL_BLUE_SIZE,
    SDL_GL_ALPHA_SIZE,
    SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_STENCIL_SIZE,
    SDL_GL_ACCUM_RED_SIZE,
    SDL_GL_ACCUM_GREEN_SIZE,
    SDL_GL_ACCUM_BLUE_SIZE,
    SDL_GL_ACCUM_ALPHA_SIZE,
    SDL_GL_STEREO,
    SDL_GL_MULTISAMPLEBUFFERS,
    SDL_GL_MULTISAMPLESAMPLES,
    SDL_GL_ACCELERATED_VISUAL,
    SDL_GL_RETAINED_BACKING,
    SDL_GL_CONTEXT_MAJOR_VERSION,
    SDL_GL_CONTEXT_MINOR_VERSION,
    SDL_GL_CONTEXT_EGL,
    SDL_GL_CONTEXT_FLAGS,
    SDL_GL_CONTEXT_PROFILE_MASK
} SDL_GLattr;

typedef enum {
    SDL_GL_CONTEXT_PROFILE_CORE = 0x0001,
    SDL_GL_CONTEXT_PROFILE_COMPATIBILITY = 0x0002,
    SDL_GL_CONTEXT_PROFILE_ES = 0x0004
} SDL_GLprofile;

typedef enum {
    SDL_GL_CONTEXT_DEBUG_FLAG = 0x0001,
    SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG = 0x0002
} SDL_GLcontextFlag;

SDL_Window* SDLCALL SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags);
void SDLCALL SDL_DestroyWindow(SDL_Window* window);
void SDLCALL SDL_GetWindowSize(SDL_Window* window, int* w, int* h);
int SDLCALL SDL_SetWindowFullscreen(SDL_Window* window, Uint32 flags);
int SDLCALL SDL_GL_SetAttribute(SDL_GLattr attr, int value);
SDL_GLContext SDLCALL SDL_GL_CreateContext(SDL_Window* window);
void SDLCALL SDL_GL_DeleteContext(SDL_GLContext context);
int SDLCALL SDL_GL_SetSwapInterval(int interval);
void SDLCALL SDL_GL_SwapWindow(SDL_Window* window);
void SDLCALL SDL_GL_GetDrawableSize(SDL_Window* window, int* w, int* h);
void* SDLCALL SDL_GL_GetProcAddress(const char* proc);
int SDLCALL SDL_SetRelativeMouseMode(SDL_bool enabled);

// ---- события ----
typedef enum {
    SDL_QUIT = 0x100,
    SDL_WINDOWEVENT = 0x200,
    SDL_KEYDOWN = 0x300,
    SDL_KEYUP,
    SDL_TEXTEDITING,
    SDL_TEXTINPUT,
    SDL_MOUSEMOTION = 0x400,
    SDL_MOUSEBUTTONDOWN,
    SDL_MOUSEBUTTONUP,
    SDL_MOUSEWHEEL
} SDL_EventType;

typedef enum {
    SDL_WINDOWEVENT_NONE,
    SDL_WINDOWEVENT_SHOWN,
    SDL_WINDOWEVENT_HIDDEN,
    SDL_WINDOWEVENT_EXPOSED,
    SDL_WINDOWEVENT_MOVED,
    SDL_WINDOWEVENT_RESIZED,
    SDL_WINDOWEVENT_SIZE_CHANGED,
    SDL_WINDOWEVENT_MINIMIZED,
    SDL_WINDOWEVENT_MAXIMIZED,
    SDL_WINDOWEVENT_RESTORED,
    SDL_WINDOWEVENT_ENTER,
    SDL_WINDOWEVENT_LEAVE,
    SDL_WINDOWEVENT_FOCUS_GAINED,
    SDL_WINDOWEVENT_FOCUS_LOST
} SDL_WindowEventID;

// Скан-коды — физическое положение клавиши. WASD и E работают при любой
// раскладке, в том числе русской.
typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9,
    SDL_SCANCODE_R = 21,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_2 = 31,
    SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35,
    SDL_SCANCODE_7 = 36,
    SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38,
    SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_MINUS = 45,
    SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_2 = 90,
    SDL_SCANCODE_KP_3 = 91,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef Sint32 SDL_Keycode;

typedef struct SDL_Keysym {
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
    Uint32 unused;
} SDL_Keysym;

typedef struct SDL_WindowEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 event;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint32 data1;
    Sint32 data2;
} SDL_WindowEvent;

typedef struct SDL_KeyboardEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 state;
    Uint8 repeat;
    Uint8 padding2;
    Uint8 padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_MouseMotionEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Uint32 state;
    Sint32 x;
    Sint32 y;
    Sint32 xrel;
    Sint32 yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Uint8 button;
    Uint8 state;
    Uint8 clicks;
    Uint8 padding1;
    Sint32 x;
    Sint32 y;
} SDL_MouseButtonEvent;

#define SDL_BUTTON_LEFT 1

typedef union SDL_Event {
    Uint32 type;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    Uint8 padding[56];
} SDL_Event;

int SDLCALL SDL_PollEvent(SDL_Event* event);

// ---- звук ----
typedef Uint16 SDL_AudioFormat;
typedef Uint32 SDL_AudioDeviceID;
typedef void(SDLCALL* SDL_AudioCallback)(void* userdata, Uint8* stream, int len);

#define AUDIO_S16LSB 0x8010
#define AUDIO_S16SYS AUDIO_S16LSB

typedef struct SDL_AudioSpec {
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    SDL_AudioCallback callback;
    void* userdata;
} SDL_AudioSpec;

SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* device, int iscapture,
                                              const SDL_AudioSpec* desired,
                                              SDL_AudioSpec* obtained, int allowed_changes);
void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void SDLCALL SDL_LockAudioDevice(SDL_AudioDeviceID dev);
void SDLCALL SDL_UnlockAudioDevice(SDL_AudioDeviceID dev);
void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID dev);

} // extern "C"

#endif // NORMA_SYSTEM_SDL_HEADERS
