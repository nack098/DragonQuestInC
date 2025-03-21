#pragma once

#ifdef _DEBUG
#define ASSERT(Expression, Message) if (!(Expression)) { *(int *)0 = 0; }
#else
#define ASSERT(Expression, Message) ((void) 0);
#endif

#define GAME_NAME	"Dragon Quest 3 Clone"
#define STUDIO		"- Please Let Me Sleep -"
#define GAME_RES_WIDTH	384
#define GAME_RES_HEIGHT	240 
#define GAME_BPP	32
#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP / 8))
#define CALCULATE_AVERAGE_FPS_EVERY_X_FRAMES 120
#define TARGET_MICROSECONDS_PER_FRAME 16667ULL // 60 FPS
#define ANIMATION_DELAY 15 // In frames
#define FONT_SHEET_CHARACTERS_PER_ROW 98

// This used for consistency when loading
#define FACING_DOWN_1	0
#define FACING_DOWN_2	1
#define FACING_RIGHT_1	2
#define FACING_RIGHT_2	3
#define FACING_UP_1		4
#define FACING_UP_2		5
#define FACING_LEFT_1	6
#define FACING_LEFT_2	7

#define AVX
#define CAST_FUNC(x) (x)(void *)(uintptr_t)

#define LOG_FILE_NAME GAME_NAME ".log"

typedef LONG (NTAPI* _NtQueryTimerResolution) (
	OUT PULONG MinimumResolution,
	OUT PULONG MaximumResolution,
	OUT PULONG CurrentResolution);

_NtQueryTimerResolution NtQueryTimerResolution;

typedef enum LOGLEVEL {

	LL_NONE = 0,
	LL_ERROR = 1,
	LL_WARNING = 2,
	LL_INFO = 3,
	LL_DEBUG = 4

} LOGLEVEL;

typedef enum GAMESTATE {

	GS_SPLASHSCREEN,
	GS_TITLE_SCREEN,
	GS_CHARACTER_NAMING,
	GS_OVERWORLD,
	GS_BATTLE,
	GS_OPTIONSCREEN,
	GS_EXITYESNOSCREEN

} GAMESTATE;

typedef struct GAMEBITMAP {

	BITMAPINFO BitmapInfo;
	void* Memory;

} GAMEBITMAP;

typedef enum DIRECTION {
	DOWN = 0,
	RIGHT = 2,
	UP = 4,
	LEFT = 6
} DIRECTION;

typedef struct UPOINT {
	uint16_t x;
	uint16_t y;
} UPOINT;

typedef struct TILEMAP {
	uint16_t Width;
	uint16_t Height;
	uint8_t* Map;
} TILEMAP;

typedef struct GAMEMAP {
	TILEMAP TileMap;
	GAMEBITMAP *TileSets;
} GAMEMAP;

typedef struct GAMEINPUT {
	int16_t EscapeKeyIsDown;
	int16_t DebugKeyIsDown;
	int16_t LeftKeyIsDown;
	int16_t RightKeyIsDown;
	int16_t UpKeyIsDown;
	int16_t DownKeyIsDown;
	int16_t ChooseKeyIsDown;

	// Key Debouncing
	int16_t EscapeKeyWasDown;
	int16_t DebugKeyWasDown;
	int16_t LeftKeyWasDown;
	int16_t RightKeyWasDown;
	int16_t UpKeyWasDown;
	int16_t DownKeyWasDown;
	int16_t ChooseKeyWasDown;
	
} GAMEINPUT;

typedef struct GAMEPERFDATA {

	uint64_t TotalFramesRendered;
	float RawFPSAverage;
	float CookedFPSAverage;
	MONITORINFO MonitorInfo;
	int64_t PerfFrequency;
	int32_t MonitorWidth;
	int32_t MonitorHeight;
	int32_t WindowWidth;
	int32_t WindowHeight;
	BOOL DisplayDebugInfo;
	ULONG MinimumTimerResolution;
	ULONG MaximumTimerResolution;
	ULONG CurrentTimerResolution;
	DWORD HandleCount;
	PROCESS_MEMORY_COUNTERS_EX MemInfo;
	SYSTEM_INFO SystemInfo;
	FILETIME CurrentSystemTime;
	FILETIME PreviousSystemTime;
	float CPUPercent;
	uint8_t MaxScaleFactor;
	uint8_t CurrentScaleFactor;

} GAMEPERFDATA;

typedef struct PLAYER {

	uint8_t Name[12];
	BOOL Active;
	GAMEBITMAP Sprite[8];
	UPOINT ScreenPos;
	UPOINT HitBox;
	UPOINT WorldPos;
	int32_t HP;
	int32_t Strength;
	int32_t MP;
	DIRECTION Direction;
	uint8_t Frame;

} PLAYER;

typedef struct PIXEL32 {

	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alpha;

} PIXEL32;

LRESULT CALLBACK MainWindowProc(

	_In_ HWND WindowHandle,
	_In_ UINT Message,
	_In_ WPARAM WParam,
	_In_ LPARAM LParam

);

typedef struct REGISTRYPARAMS {

	LOGLEVEL LogLevel;

} REGISTRYPARAMS;

DWORD CreateMainGameWindow(void);

BOOL __forceinline GameIsAlreadyRunning(void);

void  ProcessPlayerInput(void);

void UpdateAnimation(void);

DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitmap);

DWORD InitializePlayer(void);

void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP *GameBitmap, _In_ uint16_t x, _In_ uint16_t y);

void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32 *Color, _In_ uint16_t x, _In_ uint16_t y);

void BlitTilemapToBuffer(_In_ GAMEMAP* GameBitmap);

void RenderFrameGraphics(void);

DWORD LoadRegistryParameters(void);

DWORD LoadTilemapFromFile(_In_ char* FileName, _In_ TILEMAP* TileMap);

void LogMessageA(_In_ LOGLEVEL LogLevel, _In_ char* Message, _In_ ...);

#if defined(AVX)
#include <immintrin.h>
#elif defined(SIMD)
#include <emmintrin.h>
#endif

#if defined(AVX)
__forceinline void ClearScreen(_In_ __m256i* Color);
#elif defined(SIMD)
__forceinline void ClearScreen(_In_ __m128i* Color);
#else
__forceinline void ClearScreen(_In_ PIXEL32 *Color);
#endif

void DrawSplashScreen(void);
void DrawTitleScreen(void);
void DrawExitYesNoScreen(void);
void DrawCharacterNaming(void);
void DrawDebugInfo(void);

void PPI_SplashScreen(void);
void PPI_TitleScreen(void);
void PPI_OverWorld(void);
void PPI_ExitYesNoScreen(void);
void PPI_CharacterNaming(void);
