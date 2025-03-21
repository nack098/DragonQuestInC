// Standard Library
#include <stdint.h>
#include <stdio.h>

// Windows Library
#include <windows.h>
#include <psapi.h>

#include "Main.h"
#include "Menus.h"

#if defined(AVX)
#include <immintrin.h>
#elif defined(SIMD)
#include <emmintrin.h>
#endif

#pragma comment(lib, "Winmm.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:WinMainCRTStartup")

HWND gGameWindow;
BOOL gGameIsRunning;    // Set False To Exit the Game (Controlling WinMain)
BOOL gGameIsInProgress; // Check if Player Start The Game or Not
BOOL gWindowHasFocus;

GAMEBITMAP gBackBuffer;
GAMEBITMAP gFont;
GAMEPERFDATA gPerformanceData;
PLAYER gPlayer;

GAMEMAP gOverworld;

REGISTRYPARAMS gRegistryParams;

GAMESTATE gCurrentGameState = GS_SPLASHSCREEN;
GAMESTATE gPreviousGameState;
GAMESTATE gDesiredGameState;
UPOINT gCamera;

GAMEINPUT gGameInput;

int __stdcall WinMain(_In_ HINSTANCE Instance,
                      _In_opt_ HINSTANCE PreviousInstance,
                      _In_ PSTR CommandLine, _In_ INT CmdShow) {
  UNREFERENCED_PARAMETER(Instance);
  UNREFERENCED_PARAMETER(PreviousInstance);
  UNREFERENCED_PARAMETER(CommandLine);
  UNREFERENCED_PARAMETER(CmdShow);

  MSG Message = {0};

  int64_t FrameStart = 0;
  int64_t FrameEnd = 0;
  int64_t ElapsedMicroSeconds = 0;
  int64_t ElapsedMicroSecondsAccumulatorRaw = 0;
  int64_t ElapsedMicroSecondsAccumulatorCooked = 0;

  int64_t CurrentUserCPUTime = 0;
  int64_t CurrentKernelCPUTime = 0;
  int64_t PreviousUserCPUTime = 0;
  int64_t PreviousKernalCPUTime = 0;

  FILETIME ProcessInfoCreationTime = {0};
  FILETIME ProcessInfoExitTime = {0};

  HANDLE ProcessHandle = GetCurrentProcess();

  HMODULE NtDllModuleHandle = NULL;

  if (LoadRegistryParameters() != ERROR_SUCCESS) {
    goto Exit;
  }

  if ((NtDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL) {
    LogMessageA(LL_ERROR, "[%s] Couldn't load ntdll.dll! Error 0x%08lx!",
                __FUNCTION__, GetLastError());
    MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((NtQueryTimerResolution = CAST_FUNC(_NtQueryTimerResolution)
           GetProcAddress(NtDllModuleHandle, "NtQueryTimerResolution")) ==
      NULL) {
    LogMessageA(LL_ERROR,
                "[%s] Couldn't find NtQueryTimerResolution in ntdll.dll! "
                "GetProcAddress failed! Error 0x%08lx!",
                __FUNCTION__, GetLastError());
    MessageBoxA(NULL, "Couldn't find NtQueryTimerResolution in ntdll.dll!",
                "Error!", MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  NtQueryTimerResolution((PULONG)&gPerformanceData.MinimumTimerResolution,
                         (PULONG)&gPerformanceData.MaximumTimerResolution,
                         (PULONG)&gPerformanceData.CurrentTimerResolution);

  GetSystemInfo(&gPerformanceData.SystemInfo);
  GetSystemTimeAsFileTime((FILETIME *)&gPerformanceData.PreviousSystemTime);

  if (GameIsAlreadyRunning() == TRUE) {
    LogMessageA(LL_WARNING,
                "[%s] Another instance of this program is already running!",
                __FUNCTION__);
    MessageBoxA(NULL, "Another instance of this program is already running!",
                "Error!", MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if (timeBeginPeriod(1) == TIMERR_NOCANDO) {
    LogMessageA(LL_ERROR, "[%s] Failed to set timer resolution!", __FUNCTION__);
    MessageBoxA(NULL, "Failed to set timer resolution!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if (SetPriorityClass(ProcessHandle, HIGH_PRIORITY_CLASS) == 0) {
    LogMessageA(LL_ERROR, "[%s] Failed to set process priority! Error 0x%08lx",
                __FUNCTION__, GetLastError());
    MessageBoxA(NULL, "Failed to set process priority!", "Error!",
                MB_ICONERROR | MB_OK);
    goto Exit;
  }

  if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0) {
    LogMessageA(LL_ERROR, "[%s] Failed to set thread priority! Error 0x%08lx",
                __FUNCTION__, GetLastError());
    MessageBoxA(NULL, "Failed to set thread priority!", "Error!",
                MB_ICONERROR | MB_OK);
    goto Exit;
  }

  if (CreateMainGameWindow() != ERROR_SUCCESS) {
    LogMessageA(LL_ERROR, "[%s] CreateMainGameWindow failed!", __FUNCTION__);
    goto Exit;
  }

  QueryPerformanceFrequency((LARGE_INTEGER *)&gPerformanceData.PerfFrequency);

  gBackBuffer.BitmapInfo.bmiHeader.biSize =
      sizeof(gBackBuffer.BitmapInfo.bmiHeader);
  gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
  gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
  gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
  gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
  gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;
  gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE,
                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  if (gBackBuffer.Memory == NULL) {
    LogMessageA(
        LL_ERROR,
        "[%s] Failed to allocate memory for drawing surface! Error 0x%08lx!",
        __FUNCTION__, GetLastError());
    MessageBoxA(NULL, "Failed to allocate memory to drawing surface!", "Error!",
                MB_ICONERROR | MB_OK);
    goto Exit;
  }

  if (InitializePlayer() != ERROR_SUCCESS) {
    LogMessageA(LL_ERROR, "[%s] Failed to initialize player!", __FUNCTION__);
    goto Exit;
  }

  if (Load32BppBitmapFromFile("./Assets/fonts.bmpx", &gFont) != ERROR_SUCCESS) {
    LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile Failed!", __FUNCTION__);
    MessageBoxA(NULL, "Failed to load font bitmap!", "Error!",
                MB_ICONERROR | MB_OK);
    goto Exit;
  }

  if (LoadTilemapFromFile("./Assets/level.dat", &gOverworld.TileMap) !=
      ERROR_SUCCESS) {
    LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile Failed!", __FUNCTION__);
    MessageBoxA(NULL, "Failed to load level data!", "Error!",
                MB_ICONERROR | MB_OK);
    goto Exit;
  }

  gOverworld.TileSets =
      HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(GAMEBITMAP) * 271);
  for (int16_t Index = 0; Index < 271; Index++) {
    char Path[25];
    snprintf(Path, 25, "./Assets/tiles/%d.bmp", Index);
    Load32BppBitmapFromFile(Path, &gOverworld.TileSets[Index]);
  }

  gGameIsRunning = TRUE;

  while (gGameIsRunning == TRUE) {
    QueryPerformanceCounter((LARGE_INTEGER *)&FrameStart);

    while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE)) {
      DispatchMessageA(&Message);
    }

    ProcessPlayerInput();

    UpdateAnimation();

    RenderFrameGraphics();

    QueryPerformanceCounter((LARGE_INTEGER *)&FrameEnd);
    ElapsedMicroSeconds = FrameEnd - FrameStart;
    ElapsedMicroSeconds *= 1000000;
    ElapsedMicroSeconds /= gPerformanceData.PerfFrequency;

    gPerformanceData.TotalFramesRendered++;
    ElapsedMicroSecondsAccumulatorRaw += ElapsedMicroSeconds;

    while (ElapsedMicroSeconds <= TARGET_MICROSECONDS_PER_FRAME) {
      QueryPerformanceCounter((LARGE_INTEGER *)&FrameEnd);
      ElapsedMicroSeconds = FrameEnd - FrameStart;
      ElapsedMicroSeconds *= 1000000;
      ElapsedMicroSeconds /= gPerformanceData.PerfFrequency;

      if (ElapsedMicroSeconds <= (TARGET_MICROSECONDS_PER_FRAME * 0.75f)) {
        Sleep(1);
      }
    }

    ElapsedMicroSecondsAccumulatorCooked += ElapsedMicroSeconds;

    if ((gPerformanceData.TotalFramesRendered %
         CALCULATE_AVERAGE_FPS_EVERY_X_FRAMES) == 0) {
      GetSystemTimeAsFileTime(&gPerformanceData.CurrentSystemTime);

      GetProcessTimes(ProcessHandle, &ProcessInfoCreationTime,
                      &ProcessInfoExitTime, (FILETIME *)&CurrentUserCPUTime,
                      (FILETIME *)&CurrentKernelCPUTime);
      GetProcessHandleCount(ProcessHandle, &gPerformanceData.HandleCount);

      gPerformanceData.CPUPercent =
          ((float)CurrentUserCPUTime - (float)PreviousUserCPUTime) +
          ((float)CurrentKernelCPUTime - (float)PreviousKernalCPUTime);
      gPerformanceData.CPUPercent /=
          (float)gPerformanceData.CurrentSystemTime.dwLowDateTime -
          (float)gPerformanceData.PreviousSystemTime.dwLowDateTime;

      gPerformanceData.CPUPercent /=
          (float)gPerformanceData.SystemInfo.dwNumberOfProcessors;
      gPerformanceData.CPUPercent *= 100;

      K32GetProcessMemoryInfo(
          ProcessHandle, (PROCESS_MEMORY_COUNTERS *)&gPerformanceData.MemInfo,
          sizeof(gPerformanceData.MemInfo));

      gPerformanceData.RawFPSAverage =
          CALCULATE_AVERAGE_FPS_EVERY_X_FRAMES /
          (ElapsedMicroSecondsAccumulatorRaw * 0.000001f);
      gPerformanceData.CookedFPSAverage =
          CALCULATE_AVERAGE_FPS_EVERY_X_FRAMES /
          (ElapsedMicroSecondsAccumulatorCooked * 0.000001f);

      ElapsedMicroSecondsAccumulatorRaw = 0;
      ElapsedMicroSecondsAccumulatorCooked = 0;

      PreviousUserCPUTime = CurrentUserCPUTime;
      PreviousKernalCPUTime = CurrentKernelCPUTime;
      gPerformanceData.PreviousSystemTime = gPerformanceData.CurrentSystemTime;
    }
  }

Exit:
  // Cleanup
  return (0);
}

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_ UINT Message,
                                _In_ WPARAM WParam, _In_ LPARAM LParam) {
  LRESULT Result = 0;

  switch (Message) {
  case WM_CLOSE: {
    gGameIsRunning = FALSE;
    PostQuitMessage(0);
    break;
  }
  case WM_ACTIVATE: {
    if (WParam == 0) {
      // Lost Focus
      gWindowHasFocus = FALSE;
    } else {
      // Gained Focus
      ShowCursor(FALSE);
      gWindowHasFocus = TRUE;
    }
    break;
  }
  default: {
    Result = DefWindowProc(WindowHandle, Message, WParam, LParam);
  }
  }

  return Result;
}

DWORD CreateMainGameWindow(void) {
  DWORD Result = ERROR_SUCCESS;

  WNDCLASSEXA WindowClass = {0};
  WindowClass.cbSize = sizeof(WNDCLASSEXA);
  WindowClass.style = 0;
  WindowClass.lpfnWndProc = MainWindowProc;
  WindowClass.cbClsExtra = 0;
  WindowClass.cbWndExtra = 0;
  WindowClass.hInstance = GetModuleHandleA(NULL);
  WindowClass.hIcon = LoadIconA(NULL, IDI_APPLICATION);
  WindowClass.hIconSm = LoadIconA(NULL, IDI_APPLICATION);
  WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
#ifdef _DEBUG
  WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
#else
  WindowClass.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
#endif
  WindowClass.lpszMenuName = NULL;
  WindowClass.lpszClassName = GAME_NAME;

  if (RegisterClassExA(&WindowClass) == 0) {
    Result = GetLastError();
    LogMessageA(LL_ERROR, "[%s] RegisterClassEx Failed! Error 0x%08lx!",
                __FUNCTION__, Result);
    MessageBoxA(NULL, "Window Registration Failed!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  gGameWindow = CreateWindowExA(
      0, WindowClass.lpszClassName, WindowClass.lpszClassName,
      WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
      NULL, NULL, WindowClass.hInstance, NULL);

  if (gGameWindow == NULL) {
    Result = GetLastError();

    LogMessageA(LL_ERROR, "[%s] CreateWindowEx Failed! Error 0x%08lx!",
                __FUNCTION__, Result);

    MessageBoxA(NULL, "Window Creation Failed!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);

    goto Exit;
  }

  gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

  if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY),
                      &gPerformanceData.MonitorInfo) == 0) {
    Result = ERROR_MONITOR_NO_DESCRIPTOR;

    LogMessageA(
        LL_ERROR,
        "[%s] GetMonitorInfoA(MonitorFromWindow()) Failed! Error 0x%08lx!",
        __FUNCTION__, Result);

    goto Exit;
  }

  gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right -
                                  gPerformanceData.MonitorInfo.rcMonitor.left;
  gPerformanceData.MonitorHeight =
      gPerformanceData.MonitorInfo.rcMonitor.bottom -
      gPerformanceData.MonitorInfo.rcMonitor.top;

  if (gPerformanceData.WindowWidth == 0) {
    gPerformanceData.WindowWidth = gPerformanceData.MonitorWidth;
  }

  if (gPerformanceData.WindowHeight == 0) {
    gPerformanceData.WindowHeight = gPerformanceData.MonitorHeight;
  }

  if (gPerformanceData.WindowHeight > gPerformanceData.MonitorHeight ||
      gPerformanceData.WindowWidth > gPerformanceData.MonitorWidth) {
    LogMessageA(LL_INFO,
                "[%s] Window Width or Height is greater than current monitor "
                "size. Resetting.",
                __FUNCTION__);
    gPerformanceData.WindowHeight = gPerformanceData.MonitorHeight;
    gPerformanceData.WindowWidth = gPerformanceData.MonitorWidth;
  }

  for (uint8_t Counter = 1; Counter < 12; Counter++) {
    if (gPerformanceData.MonitorWidth < gPerformanceData.MonitorHeight) {
      if (GAME_RES_WIDTH * Counter > gPerformanceData.MonitorWidth) {
        gPerformanceData.MaxScaleFactor = Counter - 1;
        break;
      }
    } else {
      if (GAME_RES_HEIGHT * Counter > gPerformanceData.MonitorHeight) {
        gPerformanceData.MaxScaleFactor = Counter - 1;
        break;
      }
    }
  }

  gPerformanceData.CurrentScaleFactor = gPerformanceData.MaxScaleFactor;

  if (SetWindowLongPtrA(gGameWindow, GWL_STYLE,
                        (WS_OVERLAPPEDWINDOW | WS_VISIBLE) &
                            ~WS_OVERLAPPEDWINDOW) == 0) {

    Result = GetLastError();

    LogMessageA(LL_ERROR, "[%s] SetWindowLong Failed! Error 0x%08lx!",
                __FUNCTION__, Result);

    goto Exit;
  }

  if (SetWindowPos(
          gGameWindow, HWND_TOP, gPerformanceData.MonitorInfo.rcMonitor.left,
          gPerformanceData.MonitorInfo.rcMonitor.top,
          gPerformanceData.MonitorWidth, gPerformanceData.MonitorHeight,
          SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0) {

    Result = GetLastError();

    LogMessageA(LL_ERROR, "[%s] SetWindowPos Failed! Error 0x%08lx!",
                __FUNCTION__, Result);

    goto Exit;
  }

Exit:

  return (Result);
}

BOOL __forceinline GameIsAlreadyRunning(void) {
  CreateMutexA(NULL, FALSE, GAME_NAME "_GameMutex");

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    return (TRUE);
  } else {
    return (FALSE);
  }
}

void ProcessPlayerInput(void) {
  // Keyboard Input

  if (gWindowHasFocus == FALSE) {
    return;
  }
  gGameInput.EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);
  gGameInput.DebugKeyIsDown = GetAsyncKeyState(VK_F1);
  gGameInput.LeftKeyIsDown = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');
  gGameInput.RightKeyIsDown =
      GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');
  gGameInput.UpKeyIsDown = GetAsyncKeyState(VK_UP) | GetAsyncKeyState('W');
  gGameInput.DownKeyIsDown = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState('S');
  gGameInput.ChooseKeyIsDown = GetAsyncKeyState(VK_RETURN);

  if (gGameInput.DebugKeyIsDown != 0 && !gGameInput.DebugKeyWasDown) {
    gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
  }

  switch (gCurrentGameState) {
  case GS_SPLASHSCREEN: {
    PPI_SplashScreen();
    break;
  }
  case GS_TITLE_SCREEN: {
    PPI_TitleScreen();
    break;
  }
  case GS_OVERWORLD: {
    PPI_OverWorld();
    break;
  }
  case GS_EXITYESNOSCREEN: {
    PPI_ExitYesNoScreen();
    break;
  }
  case GS_CHARACTER_NAMING: {
    PPI_CharacterNaming();
    break;
  }
  default: {
    ASSERT(FALSE, "Unknown Game State!");
  }
  }

  gGameInput.DebugKeyWasDown = gGameInput.DebugKeyIsDown;
  gGameInput.EscapeKeyWasDown = gGameInput.EscapeKeyIsDown;
  gGameInput.LeftKeyWasDown = gGameInput.LeftKeyIsDown;
  gGameInput.RightKeyWasDown = gGameInput.RightKeyIsDown;
  gGameInput.UpKeyWasDown = gGameInput.UpKeyIsDown;
  gGameInput.DownKeyWasDown = gGameInput.DownKeyIsDown;
  gGameInput.ChooseKeyWasDown = gGameInput.ChooseKeyIsDown;
}

DWORD Load32BppBitmapFromFile(_In_ char *FileName,
                              _Inout_ GAMEBITMAP *GameBitmap) {
  DWORD Error = ERROR_SUCCESS;
  HANDLE FileHandle = INVALID_HANDLE_VALUE;
  WORD FileType = 0;
  DWORD NumberOfBytesRead = 0;
  DWORD PixelDataOffset = 0;

  if ((FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) ==
      INVALID_HANDLE_VALUE) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] CreateFileA Failed! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  if (ReadFile(FileHandle, &FileType, 2, &NumberOfBytesRead, NULL) == 0) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] ReadFile Failed! Error 0x%08lx!", __FUNCTION__,
                Error);
    goto Exit;
  }

  if (FileType != 0x4D42) {
    Error = ERROR_FILE_INVALID;
    LogMessageA(LL_ERROR, "[%s] Invalid File Type! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  if (SetFilePointer(FileHandle, 0x0A, NULL, FILE_BEGIN) ==
      INVALID_SET_FILE_POINTER) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] SetFilePointer Failed! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  if (ReadFile(FileHandle, &PixelDataOffset, sizeof(DWORD), &NumberOfBytesRead,
               NULL) == 0) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] ReadFile Failed! Error 0x%08lx!", __FUNCTION__,
                Error);
    goto Exit;
  }

  if (SetFilePointer(FileHandle, 0x0E, NULL, FILE_BEGIN) ==
      INVALID_SET_FILE_POINTER) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] SetFilePointer Failed! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  if (ReadFile(FileHandle, &GameBitmap->BitmapInfo.bmiHeader,
               sizeof(BITMAPINFOHEADER), &NumberOfBytesRead, NULL) == 0) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] ReadFile Failed! Error 0x%08lx!", __FUNCTION__,
                Error);
    goto Exit;
  }

  if ((GameBitmap->Memory =
           HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                     GameBitmap->BitmapInfo.bmiHeader.biSizeImage)) == NULL) {
    Error = ERROR_NOT_ENOUGH_MEMORY;
    LogMessageA(LL_ERROR, "[%s] HeapAlloc Failed! Error 0x%08lx!", __FUNCTION__,
                Error);
    goto Exit;
  }

  if (SetFilePointer(FileHandle, PixelDataOffset, NULL, FILE_BEGIN) ==
      INVALID_SET_FILE_POINTER) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] SetFilePointer Failed! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  if (ReadFile(FileHandle, GameBitmap->Memory,
               GameBitmap->BitmapInfo.bmiHeader.biSizeImage, &NumberOfBytesRead,
               NULL) == 0) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] ReadFile Failed! Error 0x%08lx!", __FUNCTION__,
                Error);
    goto Exit;
  }

Exit:
  if (FileHandle && (FileHandle != INVALID_HANDLE_VALUE)) {
    CloseHandle(FileHandle);
  }

  return (Error);
}

void BlitStringToBuffer(_In_ char *String, _In_ GAMEBITMAP *FontSheet,
                        _In_ PIXEL32 *Color, _In_ uint16_t x, _In_ uint16_t y) {

  static uint8_t FontCharacterPixelOffset[] = {
      //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..
      //  .. .. .. .. .. .. .. .. ..
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      //     !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /  0  1  2  3  4  5  6  7
      //     8  9  :  ;  <  =  >  ?
      94, 64, 87, 66, 67, 68, 70, 85, 72, 73, 71, 77, 88, 74, 91, 92, 52, 53,
      54, 55, 56, 57, 58, 59, 60, 61, 86, 84, 89, 75, 90, 93,
      //  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W
      //  X  Y  Z  [  \  ]  ^  _
      65, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      20, 21, 22, 23, 24, 25, 80, 78, 81, 69, 76,
      //  `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w
      //  x  y  z  {  |  }  ~  ..
      62, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
      43, 44, 45, 46, 47, 48, 49, 50, 51, 82, 79, 83, 63, 93,
      //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..
      //  .. .. .. .. .. .. .. .. ..
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      //  .. .. .. .. .. .. .. .. .. .. .. bb .. .. .. .. .. .. .. .. .. .. ..
      //  .. .. .. .. ab .. .. .. ..
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 96, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 95, 93, 93, 93, 93,
      //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..
      //  .. .. .. .. .. .. .. .. ..
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      //  .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. F2 .. .. .. ..
      //  .. .. .. .. .. .. .. .. ..
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      97, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93};

  int16_t CharWidth = (int16_t)FontSheet->BitmapInfo.bmiHeader.biWidth /
                      FONT_SHEET_CHARACTERS_PER_ROW;
  int16_t CharHeight = (int16_t)FontSheet->BitmapInfo.bmiHeader.biHeight;
  int16_t BytesPerCharacter =
      (CharWidth * CharHeight *
       (FontSheet->BitmapInfo.bmiHeader.biBitCount / 8));

  int16_t StringLength = (int16_t)strlen(String);

  GAMEBITMAP StringBitmap = {0};
  StringBitmap.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
  StringBitmap.BitmapInfo.bmiHeader.biHeight = CharHeight;
  StringBitmap.BitmapInfo.bmiHeader.biWidth = CharWidth * StringLength;
  StringBitmap.BitmapInfo.bmiHeader.biPlanes = 1;
  StringBitmap.BitmapInfo.bmiHeader.biCompression = BI_RGB;
  StringBitmap.Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                  BytesPerCharacter * StringLength);

  for (int16_t Character = 0; Character < StringLength; Character++) {
    int16_t StartingFontSheetPixel = 0;
    int16_t FontSheetOffset = 0;
    int16_t StringBitmapOffset = 0;

    PIXEL32 FontSheetPixel = {0};

    StartingFontSheetPixel =
        (int16_t)(FontSheet->BitmapInfo.bmiHeader.biWidth *
                  FontSheet->BitmapInfo.bmiHeader.biHeight) -
        (int16_t)FontSheet->BitmapInfo.bmiHeader.biWidth +
        (CharWidth * FontCharacterPixelOffset[(uint8_t)String[Character]]);

    for (int16_t YPixel = 0; YPixel < CharHeight; YPixel++) {
      for (int16_t XPixel = 0; XPixel < CharWidth; XPixel++) {
        FontSheetOffset =
            StartingFontSheetPixel + XPixel -
            ((int16_t)FontSheet->BitmapInfo.bmiHeader.biWidth * YPixel);
        StringBitmapOffset =
            (Character * CharWidth) +
            ((int16_t)(StringBitmap.BitmapInfo.bmiHeader.biWidth *
                       StringBitmap.BitmapInfo.bmiHeader.biHeight) -
             (int16_t)StringBitmap.BitmapInfo.bmiHeader.biWidth) +
            XPixel -
            ((int16_t)StringBitmap.BitmapInfo.bmiHeader.biWidth) * YPixel;

        memcpy_s(&FontSheetPixel, sizeof(PIXEL32),
                 (PIXEL32 *)FontSheet->Memory + FontSheetOffset,
                 sizeof(PIXEL32));

        FontSheetPixel.Red = Color->Red;
        FontSheetPixel.Green = Color->Green;
        FontSheetPixel.Blue = Color->Blue;

        memcpy_s((PIXEL32 *)StringBitmap.Memory + StringBitmapOffset,
                 sizeof(PIXEL32), &FontSheetPixel, sizeof(PIXEL32));
      }
    }
  }

  Blit32BppBitmapToBuffer(&StringBitmap, x, y);

  if (StringBitmap.Memory) {
    HeapFree(GetProcessHeap(), 0, StringBitmap.Memory);
  }
}

void DrawOverworldScreen() {
  BlitTilemapToBuffer(&gOverworld);
  Blit32BppBitmapToBuffer(&gPlayer.Sprite[gPlayer.Direction + gPlayer.Frame],
                          gPlayer.ScreenPos.x, gPlayer.ScreenPos.y);
}

void RenderFrameGraphics(void) {
  memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);
  switch (gCurrentGameState) {
  case GS_SPLASHSCREEN: {
    DrawSplashScreen();
    break;
  }
  case GS_TITLE_SCREEN: {
    DrawTitleScreen();
    break;
  }
  case GS_CHARACTER_NAMING: {
    DrawCharacterNaming();
    break;
  };
  case GS_EXITYESNOSCREEN: {
    DrawExitYesNoScreen();
    break;
  }
  case GS_OVERWORLD: {
    DrawOverworldScreen();
    break;
  }
  default: {
    ASSERT(FALSE, "GameState not implemented!");
  }
  }

  if (gPerformanceData.DisplayDebugInfo == TRUE) {
    DrawDebugInfo();
  }

  HDC DeviceContext = GetDC(gGameWindow);

  StretchDIBits(
      DeviceContext,
      (gPerformanceData.MonitorWidth / 2) -
          ((GAME_RES_WIDTH * gPerformanceData.CurrentScaleFactor) / 2),
      (gPerformanceData.MonitorHeight / 2) -
          ((GAME_RES_HEIGHT * gPerformanceData.CurrentScaleFactor) / 2),
      GAME_RES_WIDTH * gPerformanceData.CurrentScaleFactor,
      GAME_RES_HEIGHT * gPerformanceData.CurrentScaleFactor, 0, 0,
      GAME_RES_WIDTH, GAME_RES_HEIGHT, gBackBuffer.Memory,
      &gBackBuffer.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);

  ReleaseDC(gGameWindow, DeviceContext);
}

DWORD LoadTilemapFromFile(_In_ char *FileName, _In_ TILEMAP *TileMap) {
  DWORD Error = ERROR_SUCCESS;
  HANDLE FileHandle = INVALID_HANDLE_VALUE;
  LARGE_INTEGER FileSize = {0};
  DWORD NumberOfByteRead = 0;

  if ((FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) ==
      INVALID_HANDLE_VALUE) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] CreateFileA Failed! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  if (GetFileSizeEx(FileHandle, &FileSize) == 0) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] GetFileSizeEx Failed! Error 0x%08lx!",
                __FUNCTION__, Error);
    goto Exit;
  }

  LogMessageA(LL_INFO, "[%s] Size of file %s: %lu.", __FUNCTION__, FileName,
              FileSize);

  TileMap->Map =
      HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, FileSize.QuadPart);

  if (TileMap->Map == NULL) {
    Error = ERROR_OUTOFMEMORY;
    goto Exit;
  }

  if (ReadFile(FileHandle, TileMap->Map, (DWORD)FileSize.QuadPart,
               &NumberOfByteRead, NULL) == 0) {
    Error = GetLastError();
    LogMessageA(LL_ERROR, "[%s] ReadFile Failed! Error 0x%08lx!", __FUNCTION__,
                Error);
  }
Exit:
  if (FileHandle && (FileHandle != INVALID_HANDLE_VALUE)) {
    CloseHandle(FileHandle);
  }
  return (Error);
}

DWORD InitializePlayer(void) {
  DWORD Error = ERROR_SUCCESS;

  gPlayer.ScreenPos.x = 0;
  gPlayer.ScreenPos.y = 0;
  gPlayer.WorldPos.x = 0;
  gPlayer.WorldPos.y = 0;
  gPlayer.HitBox.x = 16;
  gPlayer.HitBox.y = 24;
  gPlayer.Direction = DOWN;
  gPlayer.Active = TRUE;
  gPlayer.Frame = 0;

  gCamera.x = (GAME_RES_WIDTH / 2);
  gCamera.y = (GAME_RES_HEIGHT / 2);

  if ((Error = Load32BppBitmapFromFile("./Assets/down1.bmp",
                                       &gPlayer.Sprite[FACING_DOWN_1])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load down1.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/down2.bmp",
                                       &gPlayer.Sprite[FACING_DOWN_2])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load down2.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/right1.bmp",
                                       &gPlayer.Sprite[FACING_RIGHT_1])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load right1.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/right2.bmp",
                                       &gPlayer.Sprite[FACING_RIGHT_2])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load right2.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/up1.bmp",
                                       &gPlayer.Sprite[FACING_UP_1])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load up1.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/up2.bmp",
                                       &gPlayer.Sprite[FACING_UP_2])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load up2.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/left1.bmp",
                                       &gPlayer.Sprite[FACING_LEFT_1])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load left1.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

  if ((Error = Load32BppBitmapFromFile("./Assets/left2.bmp",
                                       &gPlayer.Sprite[FACING_LEFT_2])) !=
      ERROR_SUCCESS) {
    MessageBoxA(NULL, "Failed to load left2.bmp!", "Error!",
                MB_ICONEXCLAMATION | MB_OK);
    goto Exit;
  }

Exit:
  return (Error);
}

#if defined(AVX)

__forceinline void ClearScreen(_In_ __m256i *Color) {
#define PIXELS_PER_M256I 8
  for (int index = 0;
       index < GAME_RES_HEIGHT * GAME_RES_WIDTH / PIXELS_PER_M256I; index++) {
    _mm256_store_si256((__m256i *)gBackBuffer.Memory + index, *Color);
  }
}

#elif defined(SIMD)

__forceinline void ClearScreen(_In_ __m128i *Color) {
  for (int index = 0; index < GAME_RES_HEIGHT * GAME_RES_WIDTH / 4; index++) {
    _mm_store_si128((__m128i *)gBackBuffer.Memory + index, *Color);
  }
}

#else

__forceinline void ClearScreen(_In_ PIXEL32 *Color) {
  // Defend Against Spectre Attack (Spectre Variant 1)
  for (int x = 0; x < GAME_RES_HEIGHT * GAME_RES_WIDTH; x++) {
    memcpy((PIXEL32 *)gBackBuffer.Memory + x, Color, sizeof(PIXEL32));
  }
}

#endif

void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP *GameBitmap, _In_ uint16_t x,
                             _In_ uint16_t y) {
  int32_t StartingScreenPixel =
      ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) -
      (GAME_RES_WIDTH * y) + x;
  int32_t StartingBitmapPixel = ((GameBitmap->BitmapInfo.bmiHeader.biWidth *
                                  GameBitmap->BitmapInfo.bmiHeader.biHeight) -
                                 GameBitmap->BitmapInfo.bmiHeader.biWidth);

  int32_t MemoryOffset = 0;
  int32_t BitmapOffset = 0;

  PIXEL32 BitmapPixel = {0};
  for (int16_t YPixel = 0; YPixel < GameBitmap->BitmapInfo.bmiHeader.biHeight;
       YPixel++) {
    for (int16_t XPixel = 0; XPixel < GameBitmap->BitmapInfo.bmiHeader.biWidth;
         XPixel++) {
      MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);
      BitmapOffset = StartingBitmapPixel + XPixel -
                     (GameBitmap->BitmapInfo.bmiHeader.biWidth * YPixel);

      memcpy_s(&BitmapPixel, sizeof(PIXEL32),
               (PIXEL32 *)GameBitmap->Memory + BitmapOffset, sizeof(PIXEL32));

      // TODO: Implement Alpha Blending
      if (BitmapPixel.Alpha == 0) {
        continue;
      }

      memcpy_s((PIXEL32 *)gBackBuffer.Memory + MemoryOffset, sizeof(PIXEL32),
               &BitmapPixel, sizeof(PIXEL32));
    }
  }
}

void BlitTilemapToBuffer(_In_ GAMEMAP *GameMap) {
  for (uint16_t y = 0; y < GAME_RES_HEIGHT / 16; y++) {
    for (uint16_t x = 0; x < GAME_RES_WIDTH / 16; x++) {
      uint16_t x_index = (gCamera.x / 16) - 12 + x;
      uint16_t y_index = (gCamera.y / 16) - 7 + y;
      uint16_t tile_index = GameMap->TileMap.Map[x_index + (y_index * 256)];
      GAMEBITMAP current_tile = GameMap->TileSets[tile_index];
      Blit32BppBitmapToBuffer(&current_tile, x * 16, y * 16);
      // char buffer[5] = { 0 };
      // snprintf(buffer, 5, "%d", tile_index);
      // BlitStringToBuffer(buffer, &gFont, &(PIXEL32) {0xff, 0xff, 0xff, 0xff},
      // x * 16, y * 16);
    }
  }
}

void UpdateAnimation(void) {
  if (gPerformanceData.TotalFramesRendered % ANIMATION_DELAY == 0) {
    gPlayer.Frame = (gPlayer.Frame + 1) % 2;
  }
}

DWORD LoadRegistryParameters(void) {
  DWORD Result = ERROR_SUCCESS;
  HKEY RegKey = NULL;
  DWORD RegDisposition = 0;
  DWORD RegBytesRead = sizeof(DWORD);

  Result = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\" GAME_NAME, 0, NULL,
                           REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
                           &RegKey, &RegDisposition);

  if (Result != ERROR_SUCCESS) {
    LogMessageA(LL_ERROR, "[%s] RegCreateKey failed with error code 0x%08lx!",
                __FUNCTION__, Result);
    goto Exit;
  }

  if (RegDisposition == REG_CREATED_NEW_KEY) {
    LogMessageA(
        LL_INFO,
        "[%s] Registry key did not exist; created new key HKCU\\SOFTWARE\\%s.",
        __FUNCTION__, GAME_NAME);
  } else {
    LogMessageA(LL_INFO,
                "[%s] Opened existing registry key HKCU\\SOFTWARE\\%s.",
                __FUNCTION__, GAME_NAME);
  }

  Result = RegGetValueA(RegKey, NULL, "LogLevel", RRF_RT_DWORD, NULL,
                        (BYTE *)&gRegistryParams.LogLevel, &RegBytesRead);

  if (Result != ERROR_SUCCESS) {
    if (Result == ERROR_FILE_NOT_FOUND) {
      Result = ERROR_SUCCESS;
      LogMessageA(LL_INFO,
                  "[%s] LogLevel value not found in registry; using default "
                  "value of 0. (LOG_LEVEL_NONE)",
                  __FUNCTION__);
      gRegistryParams.LogLevel = LL_NONE;
    } else {
      LogMessageA(
          LL_ERROR,
          "[%s] Failed to read the 'LogLevel' registry value! Error 0x%08lx!",
          __FUNCTION__, Result);
      goto Exit;
    }
  }

  LogMessageA(LL_INFO, "[%s] LogLevel is %d", __FUNCTION__,
              gRegistryParams.LogLevel);

Exit:
  if (RegKey) {
    RegCloseKey(RegKey);
  }

  return (Result);
}

void LogMessageA(_In_ LOGLEVEL LogLevel, _In_ char *Message, _In_...) {
  size_t MessageLength = strlen(Message);
  SYSTEMTIME Time = {0};
  HANDLE LogFileHandle = INVALID_HANDLE_VALUE;

  DWORD EndOfFile = 0;
  DWORD NumberOfBytesWritten = 0;

  char DateTimeString[96] = {0};
  char SeverityString[8] = {0};
  char FormattedString[4096] = {0};

  if (gRegistryParams.LogLevel < LogLevel) {
    return;
  }

  if (MessageLength < 1 || MessageLength > 4096) {
    ASSERT(FALSE, "Message was either too short or too long!");
    return;
  }

  switch (LogLevel) {
  case LL_NONE: {
    return;
  }
  case LL_INFO: {
    strcpy_s(SeverityString, sizeof(SeverityString), "[INFO] ");
    break;
  }
  case LL_WARNING: {
    strcpy_s(SeverityString, sizeof(SeverityString), "[WARN] ");
    break;
  }
  case LL_ERROR: {
    strcpy_s(SeverityString, sizeof(SeverityString), "[ERROR]");
    break;
  }
  case LL_DEBUG: {
    strcpy_s(SeverityString, sizeof(SeverityString), "[DEBUG]");
    break;
  }
  default: {
    ASSERT(FALSE, "Unrecognized log level!");
    break;
  }
  }

  GetLocalTime(&Time);

  va_list ArgPointer = NULL;

  va_start(ArgPointer, Message);

  _vsnprintf_s(FormattedString, sizeof(FormattedString), _TRUNCATE, Message,
               ArgPointer);

  va_end(ArgPointer);

  _snprintf_s(DateTimeString, sizeof(DateTimeString), _TRUNCATE,
              "\r\n[%02u/%02u/%u %02u:%02u:%02u.%03u]", Time.wDay, Time.wMonth,
              Time.wYear, Time.wHour, Time.wMinute, Time.wSecond,
              Time.wMilliseconds);

  if ((LogFileHandle = CreateFileA(
           LOG_FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
           FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
    ASSERT(FALSE, "Failed to access log file!");
    return;
  }

  EndOfFile = SetFilePointer(LogFileHandle, 0, NULL, FILE_END);

  WriteFile(LogFileHandle, DateTimeString, (DWORD)strlen(DateTimeString),
            &NumberOfBytesWritten, NULL);

  WriteFile(LogFileHandle, SeverityString, (DWORD)strlen(SeverityString),
            &NumberOfBytesWritten, NULL);

  WriteFile(LogFileHandle, FormattedString, (DWORD)strlen(FormattedString),
            &NumberOfBytesWritten, NULL);

  if (LogFileHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(LogFileHandle);
  }
}

void DrawDebugInfo(void) {
  char DebugTextBuffer[64] = {0};
  PIXEL32 White = {0xff, 0xff, 0xff, 0xff};

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "FPS Raw:    %.01f",
            gPerformanceData.RawFPSAverage);

  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 0);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "FPS Cooked: %.01f",
            gPerformanceData.CookedFPSAverage);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 8);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "Min Timer Res: %.02f",
            gPerformanceData.MinimumTimerResolution / 10000.0f);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 16);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "Max Timer Res: %.02f",
            gPerformanceData.MaximumTimerResolution / 10000.0f);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 24);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "Cur Timer Res: %.02f",
            gPerformanceData.CurrentTimerResolution / 10000.0f);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 32);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "Handles: %lu",
            gPerformanceData.HandleCount);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 40);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "Memory:  %lld KB",
            (int64_t)gPerformanceData.MemInfo.PrivateUsage / 1024);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 48);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "CPU:     %.01f%%",
            gPerformanceData.CPUPercent);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 56);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "Total Frames:     %llu",
            gPerformanceData.TotalFramesRendered);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 64);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "ScreenPos:  (%d, %d)",
            gPlayer.ScreenPos.x, gPlayer.ScreenPos.y);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 72);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "WorldPos:  (%d, %d)",
            gPlayer.WorldPos.x, gPlayer.WorldPos.y);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 80);

  sprintf_s(DebugTextBuffer, sizeof(DebugTextBuffer), "CameraPos:  (%d, %d)",
            gCamera.x, gCamera.y);
  BlitStringToBuffer(DebugTextBuffer, &gFont, &White, 0, 88);
}

void MenuItem_TitleScreen_StartNew(void) {
  gPreviousGameState = gCurrentGameState;
  gCurrentGameState = GS_CHARACTER_NAMING;
}

void MenuItem_TitleScreen_Options(void) {}

void MenuItem_TitleScreen_Exit(void) {
  gPreviousGameState = gCurrentGameState;
  gCurrentGameState = GS_EXITYESNOSCREEN;
}

void MenuItem_ExitYesNoScreen_Yes(void) {
  SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
}

void MenuItem_ExitYesNoScreen_No(void) {
  gCurrentGameState = gPreviousGameState;
  gPreviousGameState = GS_EXITYESNOSCREEN;
}

void MenuItem_CharacterNaming_Add(void) {
  if (strlen(gPlayer.Name) < 8) {
    gPlayer.Name[strlen(gPlayer.Name)] =
        gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]
            ->Name[0];
  }
}

void MenuItem_CharacterNaming_Back(void) {
  if (strlen(gPlayer.Name) < 1) {
    gPreviousGameState = gCurrentGameState;
    gCurrentGameState = GS_TITLE_SCREEN;

    LogMessageA(
        LL_INFO,
        "[%s] Transitioning from game state %d to %d. Player selected '%s' "
        "from '%s' menu.",
        __FUNCTION__, gPreviousGameState, gCurrentGameState,
        gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]->Name,
        gMenu_CharacterNaming.Name);
  } else {
    gPlayer.Name[strlen(gPlayer.Name) - 1] = '\0';
  }
}

void MenuItem_CharacterNaming_OK(void) {
  if (strlen(gPlayer.Name) > 0) {
    ASSERT(gCurrentGameState == GS_CHARACTER_NAMING, "Invalid game state!");

    gPreviousGameState = gCurrentGameState;

    gCurrentGameState = GS_OVERWORLD;
    LogMessageA(
        LL_INFO,
        "[%s] Transitioning from game state %d to %d. Player selected '%s' "
        "from '%s' menu.",
        __FUNCTION__, gPreviousGameState, gCurrentGameState,
        gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]->Name,
        gMenu_CharacterNaming.Name);
    gPlayer.Active = TRUE;
  }
}

void DrawSplashScreen(void) {
  static uint64_t LocalFrameCounter;
  static uint64_t LastFrameSeen;
  static PIXEL32 TextColor = {0xff, 0xff, 0xff, 0xff};

  if (gPerformanceData.TotalFramesRendered > (LastFrameSeen + 1)) {
    LocalFrameCounter = 0;
  }

  if (LocalFrameCounter >= 120) {
    if ((LocalFrameCounter >= 180 && LocalFrameCounter <= 210) &&
        (LocalFrameCounter % 15 == 0)) {
      TextColor.Red -= 64;
      TextColor.Green -= 64;
      TextColor.Blue -= 64;
    }

    if (LocalFrameCounter > 225) {
      TextColor.Red = 0;
      TextColor.Green = 0;
      TextColor.Blue = 0;
    }

    if (LocalFrameCounter > 240) {
      gPreviousGameState = gCurrentGameState;
      gCurrentGameState = GS_TITLE_SCREEN;
    }

    BlitStringToBuffer(
        STUDIO, &gFont, &TextColor,
        (GAME_RES_WIDTH / 2) - ((uint16_t)(strlen(STUDIO) * 6) / 2), 100);
    BlitStringToBuffer("Presents", &gFont, &TextColor,
                       (GAME_RES_WIDTH / 2) - ((uint16_t)(8 * 6) / 2), 120);
  }

  LocalFrameCounter++;
  LastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawTitleScreen(void) {
  static PIXEL32 TextColor = {0x00, 0x00, 0x00, 0x00};
  static uint64_t LocalFrameCounter;
  static uint64_t LastFrameSeen;

  if (gPerformanceData.TotalFramesRendered > (LastFrameSeen + 1)) {
    LocalFrameCounter = 0;
    TextColor.Red = 0;
    TextColor.Green = 0;
    TextColor.Blue = 0;
  }

  switch (LocalFrameCounter) {
  case 15:
  case 30:
  case 45: {
    TextColor.Red += 64;
    TextColor.Green += 64;
    TextColor.Blue += 64;
    break;
  }
  case 60: {
    TextColor.Red = 255;
    TextColor.Green = 255;
    TextColor.Blue = 255;
    break;
  }
  }

  BlitStringToBuffer(
      GAME_NAME, &gFont, &TextColor,
      (GAME_RES_WIDTH / 2) - ((uint16_t)(strlen(GAME_NAME) * 6) / 2), 80);

  for (uint8_t MenuItem = 0; MenuItem < gMenu_TitleScreen.ItemCount;
       MenuItem++) {
    BlitStringToBuffer(gMenu_TitleScreen.Items[MenuItem]->Name, &gFont,
                       &TextColor, gMenu_TitleScreen.Items[MenuItem]->X,
                       gMenu_TitleScreen.Items[MenuItem]->Y);
  }

  BlitStringToBuffer(
      "\xbb", &gFont, &TextColor,
      gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->X - 8,
      gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->Y);

  LocalFrameCounter++;
  LastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawExitYesNoScreen(void) {
  PIXEL32 White = {0xff, 0xff, 0xff, 0xff};
  BlitStringToBuffer(
      GAME_NAME, &gFont, &White,
      (GAME_RES_WIDTH / 2) - ((uint16_t)(strlen(GAME_NAME) * 6) / 2), 80);

  for (uint8_t MenuItem = 0; MenuItem < gMenu_ExitYesNoScreen.ItemCount;
       MenuItem++) {
    BlitStringToBuffer(gMenu_ExitYesNoScreen.Items[MenuItem]->Name, &gFont,
                       &White, gMenu_ExitYesNoScreen.Items[MenuItem]->X,
                       gMenu_ExitYesNoScreen.Items[MenuItem]->Y);
  }

  BlitStringToBuffer(
      "\xbb", &gFont, &White,
      gMenu_ExitYesNoScreen.Items[gMenu_ExitYesNoScreen.SelectedItem]->X - 8,
      gMenu_ExitYesNoScreen.Items[gMenu_ExitYesNoScreen.SelectedItem]->Y);
}

void DrawCharacterNaming(void) {
  static PIXEL32 TextColor = {0x00, 0x00, 0x00, 0x00};
  static uint64_t LocalFrameCounter;
  static uint64_t LastFrameSeen;

  if (gPerformanceData.TotalFramesRendered > (LastFrameSeen + 1)) {
    LocalFrameCounter = 0;
    TextColor.Red = 0;
    TextColor.Green = 0;
    TextColor.Blue = 0;
  }

  switch (LocalFrameCounter) {
  case 15:
  case 30:
  case 45: {
    TextColor.Red += 64;
    TextColor.Green += 64;
    TextColor.Blue += 64;
    break;
  }
  case 60: {
    TextColor.Red = 255;
    TextColor.Green = 255;
    TextColor.Blue = 255;
    break;
  }
  }

  BlitStringToBuffer(gMenu_CharacterNaming.Name, &gFont, &TextColor,
                     (GAME_RES_WIDTH / 2) -
                         ((int)(strlen(gMenu_CharacterNaming.Name) * 6) / 2),
                     29);

  Blit32BppBitmapToBuffer(&gPlayer.Sprite[FACING_DOWN_1], 153, 80);

  for (int Letter = 0; Letter < _countof(gPlayer.Name) - 1; Letter++) {
    if (gPlayer.Name[Letter] == '\0') {
      BlitStringToBuffer("_", &gFont, &TextColor, 173 + (Letter * 6), 89);
    } else {
      BlitStringToBuffer((char[2]){gPlayer.Name[Letter], '\0'}, &gFont,
                         &TextColor, 173 + (Letter * 6), 89);
    }
  }

  for (int Counter = 0; Counter < gMenu_CharacterNaming.ItemCount; Counter++) {
    BlitStringToBuffer(gMenu_CharacterNaming.Items[Counter]->Name, &gFont,
                       &TextColor, gMenu_CharacterNaming.Items[Counter]->X,
                       gMenu_CharacterNaming.Items[Counter]->Y);
  }

  BlitStringToBuffer(
      "\xBB", &gFont, &TextColor,
      gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]->X - 6,
      gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]->Y);

  LocalFrameCounter++;
  LastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void PPI_SplashScreen(void) {
  if (gGameInput.EscapeKeyIsDown && !gGameInput.EscapeKeyWasDown) {
    gCurrentGameState = GS_TITLE_SCREEN;
  }
}

void PPI_TitleScreen(void) {
  if (gGameInput.UpKeyIsDown != 0 && !gGameInput.UpKeyWasDown) {
    if (gMenu_TitleScreen.SelectedItem == 0) {
      gMenu_TitleScreen.SelectedItem = gMenu_TitleScreen.ItemCount - 1;
    } else {
      gMenu_TitleScreen.SelectedItem--;
    }
  }

  if (gGameInput.DownKeyIsDown != 0 && !gGameInput.DownKeyWasDown) {
    gMenu_TitleScreen.SelectedItem =
        (gMenu_TitleScreen.SelectedItem + 1) % gMenu_TitleScreen.ItemCount;
  }

  if (gGameInput.ChooseKeyIsDown != 0 && !gGameInput.ChooseKeyWasDown) {
    gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->Action();
  }
}

void PPI_OverWorld(void) {
  if (gGameInput.EscapeKeyIsDown && !gGameInput.EscapeKeyWasDown) {
    gPreviousGameState = gCurrentGameState;
    gCurrentGameState = GS_TITLE_SCREEN;
  }

  BOOL CanMove = FALSE;
  DIRECTION Direction = DOWN;
  if (gGameInput.LeftKeyIsDown != 0 && gPlayer.ScreenPos.x > 0) {
    Direction = LEFT;
    CanMove = TRUE;
  }

  if (gGameInput.RightKeyIsDown != 0 &&
      gPlayer.ScreenPos.x < GAME_RES_WIDTH - gPlayer.HitBox.x) {
    Direction = RIGHT;
    CanMove = TRUE;
  }

  if (gGameInput.UpKeyIsDown != 0 && gPlayer.ScreenPos.y > 0) {
    Direction = UP;
    CanMove = TRUE;
  }

  if (gGameInput.DownKeyIsDown != 0 &&
      gPlayer.ScreenPos.y < GAME_RES_HEIGHT - gPlayer.HitBox.y) {
    Direction = DOWN;
    CanMove = TRUE;
  }

  if (CanMove == TRUE) {
    switch (Direction) {
    case UP: {
      if ((gPlayer.ScreenPos.y <= (GAME_RES_HEIGHT - gPlayer.HitBox.y) / 2) &&
          (gPlayer.WorldPos.y > (GAME_RES_HEIGHT - gPlayer.HitBox.y) / 2)) {
        gCamera.y--;
      } else {
        gPlayer.ScreenPos.y--;
      }
      gPlayer.WorldPos.y--;
      break;
    }
    case DOWN: {
      if (gPlayer.ScreenPos.y >= (GAME_RES_HEIGHT - gPlayer.HitBox.y) / 2 &&
          (gPlayer.WorldPos.y <
           4096 - ((GAME_RES_HEIGHT - gPlayer.HitBox.y) / 2))) {
        gCamera.y++;
      } else {
        gPlayer.ScreenPos.y++;
      }
      gPlayer.WorldPos.y++;
      break;
    }
    case LEFT: {
      if (gPlayer.ScreenPos.x <= (GAME_RES_WIDTH - gPlayer.HitBox.x) / 2 &&
          (gPlayer.WorldPos.x > (GAME_RES_WIDTH - gPlayer.HitBox.x) / 2)) {
        gCamera.x--;
      } else {
        gPlayer.ScreenPos.x--;
      }
      gPlayer.WorldPos.x--;
      break;
    }
    case RIGHT: {
      if (gPlayer.ScreenPos.x >= (GAME_RES_WIDTH - gPlayer.HitBox.x) / 2 &&
          (gPlayer.WorldPos.x <
           4096 - ((GAME_RES_WIDTH - gPlayer.HitBox.x) / 2))) {
        gCamera.x++;
      } else {
        gPlayer.ScreenPos.x++;
      }
      gPlayer.WorldPos.x++;
      break;
    }
    }
    gPlayer.Direction = Direction;
  }
}

void PPI_ExitYesNoScreen(void) {
  if (gGameInput.UpKeyIsDown != 0 && !gGameInput.UpKeyWasDown) {
    if (gMenu_ExitYesNoScreen.SelectedItem == 0) {
      gMenu_ExitYesNoScreen.SelectedItem = gMenu_ExitYesNoScreen.ItemCount - 1;
    } else {
      gMenu_ExitYesNoScreen.SelectedItem--;
    }
  }

  if (gGameInput.DownKeyIsDown != 0 && !gGameInput.DownKeyWasDown) {
    gMenu_ExitYesNoScreen.SelectedItem =
        (gMenu_ExitYesNoScreen.SelectedItem + 1) %
        gMenu_ExitYesNoScreen.ItemCount;
  }

  if (gGameInput.ChooseKeyIsDown != 0 && !gGameInput.ChooseKeyWasDown) {
    gMenu_ExitYesNoScreen.Items[gMenu_ExitYesNoScreen.SelectedItem]->Action();
  }
}

void PPI_CharacterNaming(void) {
#define ROW_WIDTH 13

#define BACK_BUTTON 52

#define OK_BUTTON 53

#define SELECTED_TEXT(Text)                                                    \
  strcmp(Text, gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem] \
                   ->Name) == 0                                                \
      ? TRUE                                                                   \
      : FALSE

  if (gGameInput.UpKeyIsDown && !gGameInput.UpKeyWasDown) {
    if (SELECTED_TEXT("Back")) {
      gMenu_CharacterNaming.SelectedItem -= ROW_WIDTH;
    } else if (SELECTED_TEXT("OK")) {
      gMenu_CharacterNaming.SelectedItem -= 2;
    } else {
      switch (
          (char)gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]
              ->Name[0]) {
      case 'A':
      case 'B':
      case 'C': {
        gMenu_CharacterNaming.SelectedItem = BACK_BUTTON;
        break;
      }
      case 'L':
      case 'M':
      case 'K': {
        gMenu_CharacterNaming.SelectedItem = OK_BUTTON;
        break;
      }
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J': {
        gMenu_CharacterNaming.SelectedItem += (ROW_WIDTH * 3);
        break;
      }
      default: {
        gMenu_CharacterNaming.SelectedItem -= ROW_WIDTH;
        break;
      }
      }
    }
  }

  if (gGameInput.DownKeyIsDown && !gGameInput.DownKeyWasDown) {
    if (SELECTED_TEXT("Back")) {
      gMenu_CharacterNaming.SelectedItem = 0;
    } else if (SELECTED_TEXT("OK")) {
      gMenu_CharacterNaming.SelectedItem = ROW_WIDTH - 1;
    } else {
      switch (
          (char)gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]
              ->Name[0]) {
      case 'n':
      case 'o':
      case 'p': {
        gMenu_CharacterNaming.SelectedItem = BACK_BUTTON;

        break;
      }
      case 'x':
      case 'y':
      case 'z': {
        gMenu_CharacterNaming.SelectedItem = OK_BUTTON;

        break;
      }
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w': {
        gMenu_CharacterNaming.SelectedItem -= (ROW_WIDTH * 3);

        break;
      }
      default: {
        gMenu_CharacterNaming.SelectedItem += ROW_WIDTH;

        break;
      }
      }
    }
  }

  if (gGameInput.LeftKeyIsDown && !gGameInput.LeftKeyWasDown) {
    if (SELECTED_TEXT("Back")) {
      gMenu_CharacterNaming.SelectedItem = OK_BUTTON;
    } else if (SELECTED_TEXT("OK")) {
      gMenu_CharacterNaming.SelectedItem = BACK_BUTTON;
    } else {
      switch (
          (char)gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]
              ->Name[0]) {
      case 'A':
      case 'N':
      case 'a':
      case 'n': {
        gMenu_CharacterNaming.SelectedItem += ROW_WIDTH - 1;

        break;
      }
      default: {
        gMenu_CharacterNaming.SelectedItem--;

        break;
      }
      }
    }
  }

  if (gGameInput.RightKeyIsDown && !gGameInput.RightKeyWasDown) {
    if (SELECTED_TEXT("Back")) {
      gMenu_CharacterNaming.SelectedItem = OK_BUTTON;
    } else if (SELECTED_TEXT("OK")) {
      gMenu_CharacterNaming.SelectedItem = BACK_BUTTON;
    } else {
      switch (
          (char)gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]
              ->Name[0]) {
      case 'M':
      case 'Z':
      case 'm':
      case 'z': {
        gMenu_CharacterNaming.SelectedItem -= ROW_WIDTH - 1;
        break;
      }
      default: {
        gMenu_CharacterNaming.SelectedItem++;
        break;
      }
      }
    }
  }

  if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown) {
    gMenu_CharacterNaming.Items[gMenu_CharacterNaming.SelectedItem]->Action();
  }

  if (gGameInput.EscapeKeyIsDown && !gGameInput.EscapeKeyWasDown) {
    MenuItem_CharacterNaming_Back();
  }
}
