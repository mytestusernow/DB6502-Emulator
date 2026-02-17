/*
 * DB6502 Emulator
 *
 * Based on Troy Schrapel's HBC-56 Emulator (MIT License)
 * https://github.com/visrealm/hbc-56/emulator
 *
 * Adapted for the DB6502 single board computer by Paul
 */

#include "hbc56emu.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "ImGuiFileBrowser.h"

#include "audio.h"

#include "debugger/debugger.h"

#include "devices/memory_device.h"
#include "devices/6502_device.h"
#include "devices/tms9918_device.h"
#include "devices/keyboard_device.h"
#include "devices/ay38910_device.h"
#include "devices/via_device.h"
#include "devices/acia_device.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <queue>
#include <string>


static HBC56Device devices[HBC56_MAX_DEVICES];
static int deviceCount = 0;

static HBC56Device* cpuDevice = NULL;
static HBC56Device* romDevice = NULL;
static HBC56Device* kbDevice = NULL;
static HBC56Device* aciaDevice = NULL;

static SDL_Window* window = NULL;

static char tempBuffer[256];

#define MAX_IRQS 5
static HBC56InterruptSignal irqs[MAX_IRQS];

static SDL_Renderer* renderer = NULL;
SDL_mutex* kbQueueMutex = nullptr;

static std::queue<SDL_KeyboardEvent> pasteQueue;
static std::queue<uint8_t> aciaPasteQueue;

static int loadRom(const char* filename);

static std::string currentRomFile;
static bool programLoaded = false;

static imgui_addons::ImGuiFileBrowser file_dialog;

/* ACIA terminal accessor declarations (defined in acia_device.c) */
extern "C" {
  const char* aciaGetTermBuffer(HBC56Device* device);
  int aciaGetTermLen(HBC56Device* device);
  int aciaGetScrollToBottom(HBC56Device* device);
  int aciaDeviceRxBufEmpty(HBC56Device* device);
}

#ifdef __cplusplus
extern "C" {
#endif

  bool fileOpen = false;

  void hbc56Reset()
  {
    for (size_t i = 0; i < deviceCount; ++i)
    {
      resetDevice(&devices[i]);
    }

    for (int i = 0; i < MAX_IRQS; ++i)
    {
      irqs[i] = INTERRUPT_RELEASE;
    }

    debug6502State(cpuDevice, CPU_RUNNING);
  }

  int hbc56NumDevices()
  {
    return deviceCount;
  }

  HBC56Device* hbc56Device(size_t deviceNum)
  {
    if (deviceNum < deviceCount)
      return &devices[deviceNum];
    return NULL;
  }

  HBC56Device* hbc56AddDevice(HBC56Device device)
  {
    if (deviceCount < (HBC56_MAX_DEVICES - 1))
    {
      devices[deviceCount] = device;
      return &devices[deviceCount++];
    }
    return NULL;
  }

  void hbc56Interrupt(uint8_t irq, HBC56InterruptSignal signal)
  {
    if (irq == 0 || irq > MAX_IRQS) return;
    irq--;

    irqs[irq] = signal;

    if (cpuDevice)
    {
      signal = INTERRUPT_RELEASE;

      for (int i = 0; i < MAX_IRQS;++i)
      {
        if (irqs[i] == INTERRUPT_RAISE)
        {
          signal = INTERRUPT_RAISE;
        }
        else if (irqs[i] == INTERRUPT_TRIGGER)
        {
          irqs[i] = INTERRUPT_RELEASE;
          signal = INTERRUPT_RAISE;
        }
      }

      interrupt6502(cpuDevice, INTERRUPT_INT, signal);
    }
  }

  int hbc56LoadRom(const uint8_t* romData, int romDataSize)
  {
    int status = 1;

    currentRomFile.clear();

    if (romDataSize != HBC56_ROM_SIZE)
    {
      SDL_snprintf(tempBuffer, sizeof(tempBuffer), "Error. ROM file must be %d bytes.", HBC56_ROM_SIZE);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DB6502 Emulator", tempBuffer, NULL);
      status = 0;
    }

    if (status)
    {
      debug6502State(cpuDevice, CPU_BREAK);
      SDL_Delay(1);
      if (!romDevice)
      {
        romDevice = hbc56AddDevice(createRomDevice(HBC56_ROM_START, HBC56_ROM_END, romData));
      }
      else
      {
        status = setMemoryDeviceContents(romDevice, romData, romDataSize);
      }
      programLoaded = true;
      hbc56Reset();
    }
    return status;
  }

  void hbc56LoadLabels(const char* labelFileContents)
  {
    debuggerLoadLabels(labelFileContents);
  }

  void hbc56LoadSource(const char* labelFileContents)
  {
    debuggerLoadSource(labelFileContents);
  }

  void hbc56LoadLayout(const char* layoutFile)
  {
    if (layoutFile[0] == 0)
    {
      ImGui::LoadIniSettingsFromDisk("imgui.ini");
      return;
    }
    ImGui::LoadIniSettingsFromMemory(layoutFile);
  }

  const char* hbc56GetLayout()
  {
    return ImGui::SaveIniSettingsToMemory(0);
  }

  void hbc56PasteText(const char* text)
  {
    SDL_LockMutex(kbQueueMutex);

    SDL_KeyboardEvent ev;
    ev.type = SDL_KEYUP;
    ev.keysym.scancode = SDL_SCANCODE_LCTRL;
    pasteQueue.push(ev);
    ev.keysym.scancode = SDL_SCANCODE_RCTRL;
    pasteQueue.push(ev);

    while (*text)
    {
      char c = *(text++);

      /* for ACIA: queue chars for throttled delivery to the ACIA */
      if (aciaDevice)
      {
        uint8_t byte = (uint8_t)c;
        if (c == '\n') byte = '\r'; /* convert LF to CR for BASIC */
        aciaPasteQueue.push(byte);
      }

      SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
      bool shift = false;
      if (SDL_islower(c)) {
        sc = (SDL_Scancode)(c - 'a' + SDL_SCANCODE_A);
      }
      else if (SDL_isupper(c))
      {
        sc = (SDL_Scancode)(c - 'A' + SDL_SCANCODE_A);
        shift = true;
      }
      else if (SDL_isdigit(c))
      {
        if (c == '0') sc = SDL_SCANCODE_0;
        else sc = (SDL_Scancode)(c - '1' + SDL_SCANCODE_1);
      }
      else
      {
        switch (c)
        {
          case ' ': sc = SDL_SCANCODE_SPACE; break;
          case '!': sc = SDL_SCANCODE_1; shift = true; break;
          case '\"': sc = SDL_SCANCODE_APOSTROPHE; shift = true; break;
          case '#': sc = SDL_SCANCODE_3; shift = true; break;
          case '$': sc = SDL_SCANCODE_4; shift = true; break;
          case '%': sc = SDL_SCANCODE_5; shift = true; break;
          case '&': sc = SDL_SCANCODE_7; shift = true; break;
          case '\'': sc = SDL_SCANCODE_APOSTROPHE; break;
          case '(': sc = SDL_SCANCODE_9; shift = true; break;
          case ')': sc = SDL_SCANCODE_0; shift = true; break;
          case '*': sc = SDL_SCANCODE_8; shift = true; break;
          case '+': sc = SDL_SCANCODE_EQUALS; shift = true; break;
          case ',': sc = SDL_SCANCODE_COMMA;  break;
          case '-': sc = SDL_SCANCODE_MINUS;  break;
          case '.': sc = SDL_SCANCODE_PERIOD;  break;
          case '/': sc = SDL_SCANCODE_SLASH;  break;
          case ':': sc = SDL_SCANCODE_SEMICOLON; shift = true; break;
          case ';': sc = SDL_SCANCODE_SEMICOLON; break;
          case '<': sc = SDL_SCANCODE_COMMA; shift = true; break;
          case '=': sc = SDL_SCANCODE_EQUALS;  break;
          case '>': sc = SDL_SCANCODE_PERIOD; shift = true; break;
          case '?': sc = SDL_SCANCODE_SLASH; shift = true; break;
          case '[': sc = SDL_SCANCODE_LEFTBRACKET; break;
          case '\\': sc = SDL_SCANCODE_BACKSLASH; break;
          case ']': sc = SDL_SCANCODE_RIGHTBRACKET; break;
          case '^': sc = SDL_SCANCODE_6; shift = true; break;
          case '_': sc = SDL_SCANCODE_MINUS; shift = true; break;
          case '`': sc = SDL_SCANCODE_GRAVE; break;
          case '{': sc = SDL_SCANCODE_LEFTBRACKET; shift = true; break;
          case '|': sc = SDL_SCANCODE_BACKSLASH; shift = true; break;
          case '}': sc = SDL_SCANCODE_RIGHTBRACKET; shift = true; break;
          case '~': sc = SDL_SCANCODE_GRAVE; shift = true; break;
          case '\t': sc = SDL_SCANCODE_TAB; break;
          case '\n': sc = SDL_SCANCODE_RETURN; break;
        }
      }

      if (sc != SDL_SCANCODE_UNKNOWN)
      {
        ev.type = SDL_KEYDOWN;
        if (shift)
        {
          ev.keysym.scancode = SDL_SCANCODE_LSHIFT;
          pasteQueue.push(ev);
        }

        ev.keysym.scancode = sc;
        pasteQueue.push(ev);
        ev.type = SDL_KEYUP;
        pasteQueue.push(ev);

        if (shift)
        {
          ev.keysym.scancode = SDL_SCANCODE_LSHIFT;
          pasteQueue.push(ev);
        }
      }
    }
    SDL_UnlockMutex(kbQueueMutex);
  }

  void hbc56ToggleDebugger()
  {
    debug6502State(cpuDevice, (getDebug6502State(cpuDevice) == CPU_RUNNING) ? CPU_BREAK : CPU_RUNNING);
  }

  void hbc56DebugBreak()
  {
    debug6502State(cpuDevice, CPU_BREAK);
  }

  void hbc56DebugRun()
  {
    debug6502State(cpuDevice, CPU_RUNNING);
  }

  void hbc56DebugStepInto()
  {
    debug6502State(cpuDevice, CPU_STEP_INTO);
  }

  void hbc56DebugStepOver()
  {
    debug6502State(cpuDevice, CPU_STEP_OVER);
  }

  void hbc56DebugStepOut()
  {
    debug6502State(cpuDevice, CPU_STEP_OUT);
  }

  void hbc56DebugBreakOnInt()
  {
    debug6502State(cpuDevice, CPU_BREAK_ON_INTERRUPT);
  }

  double hbc56CpuRuntimeSeconds()
  {
    return getCpuRuntimeSeconds(cpuDevice);
  }

  uint8_t hbc56MemRead(uint16_t addr, bool dbg)
  {
    uint8_t val = 0x00;

    SDL_LockMutex(kbQueueMutex);
    for (size_t i = 0; i < deviceCount; ++i)
    {
      if (readDevice(&devices[i], addr, &val, dbg))
        break;
    }
    SDL_UnlockMutex(kbQueueMutex);

    return val;
  }

  void hbc56MemWrite(uint16_t addr, uint8_t val)
  {
    for (size_t i = 0; i < deviceCount; ++i)
    {
      if (writeDevice(&devices[i], addr, val))
        break;
    }
  }

#ifdef __cplusplus
}
#endif


/* emulator constants */
#define LOGICAL_DISPLAY_SIZE_X 320
#define LOGICAL_DISPLAY_SIZE_Y 240
#define LOGICAL_DISPLAY_BPP    3

/* emulator state */
static int done;
static double perfFreq = 0.0;
static int tickCount = 0;
static int mouseZ = 0;


static void doTick()
{
  static double lastTime = (double)SDL_GetPerformanceCounter() / perfFreq;

  double deltaTime = 0.0001;  /* 100us per batch */
  uint32_t deltaClockTicks = (uint32_t)(HBC56_CLOCK_FREQ * deltaTime);

  double currentTime = (double)SDL_GetPerformanceCounter() / perfFreq;
  double elapsed = currentTime - lastTime;

  if (elapsed <= 0) return;
  if (elapsed > 0.05) elapsed = 0.05; /* cap at 50ms to avoid long freezes */

  int batches = (int)(elapsed / deltaTime);
  if (batches < 1) batches = 1;

  for (int b = 0; b < batches; ++b)
  {
    /* drip-feed pasted text into the ACIA with flow control.
     * Check BIOS circular buffer fill level via zero page pointers:
     *   READ_PTR at $0000, WRITE_PTR at $0001
     * Only send when buffer has room (< 192 bytes used). */
    if (aciaDevice && !aciaPasteQueue.empty() && aciaDeviceRxBufEmpty(aciaDevice))
    {
      uint8_t wrPtr = hbc56MemRead(0x0001, true);
      uint8_t rdPtr = hbc56MemRead(0x0000, true);
      uint8_t bufUsed = (wrPtr - rdPtr); /* wraps correctly for uint8_t */
      if (bufUsed < 192)
      {
        aciaDeviceReceiveByte(aciaDevice, aciaPasteQueue.front());
        aciaPasteQueue.pop();
      }
    }

    for (size_t i = 0; i < deviceCount; ++i)
    {
      tickDevice(&devices[i], deltaClockTicks, deltaTime);
    }
  }

  lastTime = currentTime;
}


static void aboutDialog(bool* aboutOpen)
{
  if (ImGui::Begin("About DB6502 Emulator", aboutOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
  {
    ImGui::Text("DB6502 Emulator v0.1\n\n");
    ImGui::Text("Based on HBC-56 Emulator by Troy Schrapel\n");
    ImGui::Text("Adapted for DB6502 by Paul\n\n");
    ImGui::Separator();
    ImGui::Text("Licensed under the MIT License.\n\n");
    ImGui::Text("HBC-56: https://github.com/visrealm/hbc-56");
  }
  ImGui::End();
}


/* ACIA terminal window */
static void aciaTerminalWindow(bool* showTerminal)
{
  if (!aciaDevice) return;

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Serial Terminal", showTerminal))
  {
    const char* buf = aciaGetTermBuffer(aciaDevice);
    int bufLen = aciaGetTermLen(aciaDevice);

    /* terminal output area */
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    contentSize.y -= ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("TermOutput", contentSize, true);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); /* green text */

    if (bufLen > 0)
    {
      ImGui::TextUnformatted(buf, buf + bufLen);
    }

    ImGui::PopStyleColor();

    if (aciaGetScrollToBottom(aciaDevice))
    {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    /* input: capture keyboard when terminal is focused */
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
      ImGuiIO& io = ImGui::GetIO();
      /* process text input (filter CR/LF/BS - handled by special key checks below) */
      if (io.InputQueueCharacters.Size > 0)
      {
        for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
        {
          ImWchar c = io.InputQueueCharacters[i];
          if (c > 0 && c < 128 && c != '\r' && c != '\n' && c != '\b')
          {
            aciaDeviceReceiveByte(aciaDevice, (uint8_t)c);
          }
        }
        io.InputQueueCharacters.resize(0);
      }

      /* handle special keys */
      if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
      {
        aciaDeviceReceiveByte(aciaDevice, '\r');
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
      {
        aciaDeviceReceiveByte(aciaDevice, '\b');
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Escape))
      {
        aciaDeviceReceiveByte(aciaDevice, 0x1B);
      }
    }

    ImGui::Text("Type in terminal when focused | Ctrl+V to paste");
  }
  ImGui::End();
}


static void doRender()
{
  static bool aboutOpen = false;

  static bool showRegisters = true;
  static bool showStack = true;
  static bool showDisassembly = true;
  static bool showSource = true;
  static bool showBreakpoints = true;

  static bool showMemory = true;
  static bool showTms9918Memory = true;
  static bool showTms9918Registers = true;
  static bool showTms9918Patterns = true;
  static bool showTms9918Sprites = true;
  static bool showTms9918SpritePatterns = true;
  static bool showVia6522 = true;
  static bool showTerminal = true;

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  static bool open = true;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
  ImGui::Begin("Workspace", &open, window_flags);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleVar(2);

  ImGuiID dockspace_id = ImGui::GetID("Workspace");
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

  if (ImGui::BeginMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("Open...", "<Ctrl> + O")) { fileOpen = true; }
      if (ImGui::MenuItem("Reset", "<Ctrl> + R")) { hbc56Reset(); }
      if (ImGui::MenuItem("Exit", "Esc")) { done = true; }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug"))
    {
      bool isRunning = getDebug6502State(cpuDevice) == CPU_RUNNING;

      if (ImGui::MenuItem("Break", "<F12>", false, isRunning)) { hbc56DebugBreak(); }
      if (ImGui::MenuItem("Break on Interrupt", "<F7>", false, isRunning)) { hbc56DebugBreakOnInt(); }
      ImGui::Separator();
      if (ImGui::MenuItem("Continue", "<F5>", false, !isRunning)) { hbc56DebugRun(); }
      ImGui::Separator();
      if (ImGui::MenuItem("Step In", "<F11>", false, !isRunning)) { hbc56DebugStepInto(); }
      if (ImGui::MenuItem("Step Over", "<F10>", false, !isRunning)) { hbc56DebugStepOver(); }
      if (ImGui::MenuItem("Step Out", "<Shift> + <F11>", false, !isRunning)) { hbc56DebugStepOut(); }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
      ImGui::MenuItem("Serial Terminal", "", &showTerminal);
      ImGui::Separator();
      if (ImGui::BeginMenu("Debugger"))
      {
        ImGui::MenuItem("Registers", "<Ctrl> + E", &showRegisters);
        ImGui::MenuItem("Stack", "<Ctrl> + S", &showStack);
        ImGui::MenuItem("Disassembly", "<Ctrl> + D", &showDisassembly);
        ImGui::MenuItem("Source", "<Ctrl> + O", &showSource);
        ImGui::MenuItem("Memory", "<Ctrl> + M", &showMemory);
        ImGui::MenuItem("Breakpoints", "<Ctrl> + B", &showBreakpoints);
        ImGui::Separator();
        ImGui::MenuItem("TMS9918A VRAM", "<Ctrl> + G", &showTms9918Memory);
        ImGui::MenuItem("TMS9918A Registers", "<Ctrl> + T", &showTms9918Registers);
        ImGui::MenuItem("TMS9918A Patterns", "<Ctrl> + P", &showTms9918Patterns);
        ImGui::MenuItem("TMS9918A Sprites", "<Ctrl> + I", &showTms9918Sprites);
        ImGui::MenuItem("TMS9918A Sprite Patterns", "<Ctrl> + A", &showTms9918SpritePatterns);
        ImGui::Separator();
        ImGui::MenuItem("65C22 VIA", "<Ctrl> + V", &showVia6522);
        ImGui::EndMenu();
      }

      for (size_t i = 0; i < deviceCount; ++i)
      {
        if (devices[i].output)
        {
          ImGui::MenuItem(devices[i].name, "", &devices[i].visible);
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
      if (ImGui::MenuItem("About...")) { aboutOpen = true; }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  if (fileOpen) ImGui::OpenPopup("Open File");

  if (file_dialog.showFileDialog("Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), ".bin,.o", &fileOpen))
  {
    loadRom(file_dialog.selected_path.c_str());
    hbc56Reset();
  }

  for (size_t i = 0; i < deviceCount; ++i)
  {
    renderDevice(&devices[i]);
    if (devices[i].output && devices[i].visible)
    {
      int texW, texH;
      SDL_QueryTexture(devices[i].output, NULL, NULL, &texW, &texH);

      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
      ImGui::Begin(devices[i].name, &devices[i].visible);
      ImGui::PopStyleVar();

      ImVec2 windowSize = ImGui::GetContentRegionAvail();

      double scaleX = windowSize.x / (double)texW;
      double scaleY = windowSize.y / (double)texH;

      double scale = (scaleX < scaleY) ? scaleX : scaleY;

      ImVec2 imageSize = windowSize;
      imageSize.x = (float)(texW * scale);
      imageSize.y = (float)(texH * scale);

      ImVec2 pos = ImGui::GetCursorPos();
      pos.x += (windowSize.x - imageSize.x) / 2;
      pos.y += (windowSize.y - imageSize.y) / 2;
      ImGui::SetCursorPos(pos);

      ImGui::Image(devices[i].output, imageSize);
      ImGui::End();
    }
  }

  if (aboutOpen) aboutDialog(&aboutOpen);

  /* serial terminal */
  if (showTerminal) aciaTerminalWindow(&showTerminal);

  /* debugger windows */
  if (showRegisters) debuggerRegistersView(&showRegisters);
  if (showStack) debuggerStackView(&showStack);
  if (showDisassembly)debuggerDisassemblyView(&showDisassembly);
  if (showSource) debuggerSourceView(&showSource);
  if (showMemory) debuggerMemoryView(&showMemory);
  if (showBreakpoints) debuggerBreakpointsView(&showBreakpoints);
  if (showTms9918Memory) debuggerVramMemoryView(&showTms9918Memory);
  if (showTms9918Registers) debuggerTmsRegistersView(&showTms9918Registers);
  if (showTms9918Patterns) debuggerTmsPatternsView(renderer, &showTms9918Patterns);
  if (showTms9918Sprites) debuggerTmsSpritesView(renderer, &showTms9918Sprites);
  if (showTms9918SpritePatterns) debuggerTmsSpritePatternsView(renderer, &showTms9918SpritePatterns);
  if (showVia6522) debuggerVia6522View(&showVia6522);

  ImGui::PopStyleColor(4);

  ImGui::End();

  ImGui::Render();
  SDL_RenderClear(renderer);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
  SDL_RenderPresent(renderer);
}


static void doEvents()
{
  SDL_LockMutex(kbQueueMutex);

  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSDL2_ProcessEvent(&event);

    int skipProcessing = 0;
    switch (event.type)
    {
      case SDL_WINDOWEVENT:
        switch (event.window.event)
        {
          case SDL_WINDOWEVENT_CLOSE:
            done = 1;
            break;
          default:
            break;
        }
        break;

      case SDL_KEYDOWN:
        {
          bool withControl = (event.key.keysym.mod & KMOD_CTRL) ? 1 : 0;
          bool withShift = (event.key.keysym.mod & KMOD_SHIFT) ? 1 : 0;

          switch (event.key.keysym.sym)
          {
            case SDLK_r:
              if (withControl)
              {
                skipProcessing = 1;
                hbc56Reset();
              }
              break;

            case SDLK_d:
              if (withControl)
              {
                hbc56ToggleDebugger();
              }
              break;

            case SDLK_v:
              if (withControl)
              {
                skipProcessing = 1;
                if (SDL_HasClipboardText()) {
                  char* text = SDL_GetClipboardText();
                  hbc56PasteText(text);
                  SDL_free(text);
                }
              }
              break;

            case SDLK_F2:
              hbc56Audio(withControl == 0);
              break;

            case SDLK_F12:
              hbc56DebugBreak();
              break;

            case SDLK_F5:
              hbc56DebugRun();
              break;

            case SDLK_F7:
              hbc56DebugBreakOnInt();
              break;

            case SDLK_PAGEUP:
            case SDLK_KP_9:
              if (withControl)
              {
                debugTmsMemoryAddr -= withShift ? 0x1000 : 64;
              }
              else
              {
                debugMemoryAddr -= withShift ? 0x1000 : 64;
              }
              break;

            case SDLK_PAGEDOWN:
            case SDLK_KP_3:
              if (withControl)
              {
                debugTmsMemoryAddr += withShift ? 0x1000 : 64;
              }
              else
              {
                debugMemoryAddr += withShift ? 0x1000 : 64;
              }
              break;

            case SDLK_F11:
              if (withShift)
              {
                hbc56DebugStepOut();
              }
              else
              {
                hbc56DebugStepInto();
              }
              break;

            case SDLK_F10:
              hbc56DebugStepOver();
              break;

            case SDLK_ESCAPE:
              done = 1;
              break;

            default:
              break;
          }
        }

      case SDL_KEYUP:
        {
          bool withControl = (event.key.keysym.mod & KMOD_CTRL) ? 1 : 0;

          switch (event.key.keysym.sym)
          {
            case SDLK_r:
              if (withControl) skipProcessing = 1;
              break;
            case SDLK_d:
              if (withControl) skipProcessing = 1;
              break;
            case SDLK_v:
              if (withControl) skipProcessing = 1;
              break;
            default:
              break;
          }
          break;
        }

      case SDL_MOUSEWHEEL:
        {
          mouseZ = event.wheel.y;
          break;
        }

      case SDL_DROPFILE:
        {
          loadRom(event.drop.file);
          SDL_free(event.drop.file);
          hbc56Reset();
          return;
        }
    }

    if (!skipProcessing)
    {
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
      {
        pasteQueue.push(event.key);
      }
      else
      {
        for (size_t i = 0; i < deviceCount; ++i)
        {
          eventDevice(&devices[i], &event);
        }
      }
    }
  }

  if (keyboardDeviceQueueEmpty(kbDevice))
  {
    for (int j = 0; j < 2 && !pasteQueue.empty(); ++j)
    {
      SDL_Event ev;
      ev.type = pasteQueue.front().type;
      ev.key = pasteQueue.front();
      pasteQueue.pop();

      for (size_t i = 0; i < deviceCount; ++i)
      {
        eventDevice(&devices[i], &ev);
      }
    }
  }
  SDL_UnlockMutex(kbQueueMutex);
}

static void loop()
{
  static uint32_t lastRenderTicks = 0;

  if (programLoaded) doTick();

  ++tickCount;

  uint32_t currentTicks = SDL_GetTicks();
  if ((currentTicks - lastRenderTicks) > 17)
  {
    doRender();

    lastRenderTicks = currentTicks;
    tickCount = 0;

    doEvents();

    SDL_snprintf(tempBuffer, sizeof(tempBuffer), "DB6502 Emulator (CPU: %0.4f%%) (ROM: %s)", getCpuUtilization(cpuDevice) * 100.0f, currentRomFile.c_str());
    SDL_SetWindowTitle(window, tempBuffer);
  }
}


static char labelMapFile[FILENAME_MAX] = { 0 };

static int loadRom(const char* filename)
{
  FILE* ptr = NULL;
  int romLoaded = 0;

#ifndef HAVE_FOPEN_S
  ptr = fopen(filename, "rb");
#else
  fopen_s(&ptr, filename, "rb");
#endif

  if (ptr)
  {
    uint8_t rom[HBC56_ROM_SIZE];
    size_t romBytesRead = fread(rom, 1, sizeof(rom), ptr);
    fclose(ptr);

    romLoaded = hbc56LoadRom(rom, (int)romBytesRead);

    if (romLoaded)
    {
      currentRomFile = filename;

      SDL_strlcpy(labelMapFile, filename, FILENAME_MAX);
      size_t ln = SDL_strlen(labelMapFile);
      SDL_strlcpy(labelMapFile + ln, ".lmap", FILENAME_MAX - ln);

#ifndef HAVE_FOPEN_S
      ptr = fopen(labelMapFile, "rb");
#else
      fopen_s(&ptr, labelMapFile, "rb");
#endif
      if (ptr)
      {
        fseek(ptr, 0, SEEK_END);
        long fsize = ftell(ptr);
        fseek(ptr, 0, SEEK_SET);

        char* lblFileContent = (char*)malloc(fsize + 1);
        fread(lblFileContent, fsize, 1, ptr);
        lblFileContent[fsize] = 0;
        fclose(ptr);

        hbc56LoadLabels(lblFileContent);
        free(lblFileContent);
      }

      SDL_strlcpy(labelMapFile, filename, FILENAME_MAX);
      ln = SDL_strlen(labelMapFile);
      SDL_strlcpy(labelMapFile + ln, ".rpt", FILENAME_MAX - ln);

#ifndef HAVE_FOPEN_S
      ptr = fopen(labelMapFile, "rb");
#else
      fopen_s(&ptr, labelMapFile, "rb");
#endif
      if (ptr)
      {
        fseek(ptr, 0, SEEK_END);
        long fsize = ftell(ptr);
        fseek(ptr, 0, SEEK_SET);

        char* lblFileContent = (char*)malloc(fsize + 1);
        fread(lblFileContent, fsize, 1, ptr);
        lblFileContent[fsize] = 0;
        fclose(ptr);

        hbc56LoadSource(lblFileContent);
        free(lblFileContent);
      }
    }
  }
  else
  {
    SDL_snprintf(tempBuffer, sizeof(tempBuffer), "Error. ROM file '%s' does not exist.", filename);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DB6502 Emulator", tempBuffer, NULL);
    return 2;
  }

  return romLoaded;
}

int main(int argc, char* argv[])
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  kbQueueMutex = SDL_CreateMutex();

  int window_flags = 0;
  window_flags |= SDL_WINDOW_RESIZABLE;
  window = SDL_CreateWindow("DB6502 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, (SDL_WindowFlags)window_flags);

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  if (renderer == NULL)
  {
    SDL_Log("Error creating SDL_Renderer!");
    return 0;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  char* basePath = SDL_GetBasePath();
  char tmpBuf[512];
  SDL_snprintf(tmpBuf, sizeof(tmpBuf), "%simgui.ini", basePath);
  SDL_free(basePath);

  ImGui::LoadIniSettingsFromDisk(tmpBuf);
  ImGui::LoadIniSettingsFromDisk("imgui.ini");

  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    style.WindowMenuButtonPosition = ImGuiDir_Right;
  }

  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  perfFreq = (double)SDL_GetPerformanceFrequency();

  SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

  /* add the cpu device */
  cpuDevice = hbc56AddDevice(create6502CpuDevice(debuggerIsBreakpoint, HBC56_CLOCK_FREQ));

  /* initialise the debugger */
  debuggerInit(getCpuDevice(cpuDevice));

  int doBreak = 0;
  const char* romFile = NULL;

  /* parse arguments (defer ROM loading until after device setup) */
  for (int i = 1; i < argc;)
  {
    int consumed = 0;
    if (consumed <= 0)
    {
      consumed = -1;
      if (SDL_strcasecmp(argv[i], "--rom") == 0)
      {
        if (argv[i + 1])
        {
          consumed = 1;
          romFile = argv[++i];
        }
      }
      else if (SDL_strcasecmp(argv[i], "--brk") == 0)
      {
        consumed = 1;
        doBreak = 1;
      }
    }
    if (consumed < 0)
    {
      fprintf(stderr, "Unknown argument: '%s'\n", argv[i]);
      fprintf(stderr, "Usage: Db6502Emu [--rom <romfile>] [--brk]\n");
      return 2;
    }
    i += consumed;
  }

  srand((unsigned int)time(NULL));

  /* === DB6502 Device Setup === */
  /* Order matters: first device to claim an address wins on read/write */

  /* 1. RAM: $0000-$7FFF (32KB) */
  hbc56AddDevice(createRamDevice(HBC56_RAM_START, HBC56_RAM_END));

  /* 2. TMS9918A VDP: $8200 (data), $8201 (register) */
#if HBC56_HAVE_TMS9918
  HBC56Device* tms9918Device = hbc56AddDevice(createTms9918Device(
    HBC56_TMS9918_DAT_ADDR, HBC56_TMS9918_REG_ADDR, HBC56_TMS9918_IRQ, renderer));
  debuggerInitTms(tms9918Device);
#endif

  /* 3. AY-3-8910 PSG: $8300 */
  hbc56Audio(1);
#if HBC56_HAVE_AY_3_8910
  hbc56AddDevice(createAY38910Device(HBC56_AY38910_A_ADDR, HBC56_AY38910_CLOCK,
    hbc56AudioFreq(), hbc56AudioChannels()));
#endif

  /* 4. 65C51 ACIA: $8400-$8403 */
#if HBC56_HAVE_ACIA
  aciaDevice = hbc56AddDevice(createAciaDevice(HBC56_ACIA_ADDR, HBC56_ACIA_IRQ));
#endif

  /* 5. VIA2 (65C22): $8800 */
#if HBC56_HAVE_VIA2
  HBC56Device *via2Device = hbc56AddDevice(create65C22ViaDevice(HBC56_VIA2_ADDR, HBC56_VIA2_IRQ));
  (void)via2Device;
#endif

  /* 6. VIA1 (65C22): $9000 - synced to CPU */
#if HBC56_HAVE_VIA
  HBC56Device *viaDevice = hbc56AddDevice(create65C22ViaDevice(HBC56_VIA_ADDR, HBC56_VIA_IRQ));
  debuggerInitVia(viaDevice);
  sync6502CpuDevice(cpuDevice, viaDevice);
#endif

  /* 7. Keyboard: on VIA1 port A */
#if HBC56_HAVE_KB
  kbDevice = hbc56AddDevice(createKeyboardDevice(HBC56_KB_ADDR, HBC56_KB_IRQ));
#endif

  /* 8. ROM: $8000-$FFFF (32KB) - loaded LAST so I/O devices take priority */
  int romLoaded = 0;
  if (!romFile)
  {
    /* default ROM location */
    romFile = "/home/paul/AI_Terminal/DB6502_Basic/eater.bin";
  }
  romLoaded = loadRom(romFile);

  if (romLoaded == 0)
  {
    fileOpen = true;
  }

  done = 0;

  hbc56Reset();

  if (doBreak) hbc56DebugBreak();

  SDL_Delay(100);

  while (!done)
  {
    loop();
  }

  /* clean up */
  for (size_t i = 0; i < deviceCount; ++i)
  {
    destroyDevice(&devices[i]);
  }

  hbc56Audio(0);
  SDL_AudioQuit();

  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  SDL_DestroyMutex(kbQueueMutex);

  return 0;
}
