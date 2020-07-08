#include "stdafx.hpp"
#include "resource.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include "XboxController.hpp"

// how many times to check the USB device each second, must be 1000 or lower, higher value = higher CPU usage
// 144 seems a good value, i don't really know anyone that uses a higher refresh rate than that...
// TODO: make this configurable?
const int poll_rate = 144;

int poll_ms = (1000 / min(1000, poll_rate));

// Analog Stick and Trigger Deadzone Adjustment Enabled
bool deadzoneCombinationEnabled = true;

// Path of our config INI, based on EXE path
char ini_path[4096];

// button bitmask for guide combination
int combo_guideButton = 0;

WCHAR title[256];
bool usb_end = false;

#pragma region Startup Helpers
long RegistryGetString(HKEY hKey, const std::wstring& valueName, std::wstring& value, const std::wstring& defaultValue)
{
  value = defaultValue;
  WCHAR buffer[512];
  DWORD bufferSize = sizeof(buffer);
  long res = RegQueryValueEx(hKey, valueName.c_str(), 0, NULL, (LPBYTE)buffer, &bufferSize);
  if (res == ERROR_SUCCESS)
    value = buffer;

  return res;
}

long RegistrySetString(HKEY hKey, const std::wstring& valueName, const std::wstring& value)
{
  return RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ, (LPBYTE)value.c_str(), (DWORD)(value.length() * sizeof(WCHAR)));
}

// returns true if startup entry is set to this exe
bool StartupIsSet()
{
  HKEY runKey = NULL;
  std::wstring runKeyVal;
  if (RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &runKey) != ERROR_SUCCESS)
  {
    OutputDebugString(L"StartupIsSet: Failed to open Run key!\n");
    runKey = NULL;
    return false;
  }
  else
  {
    if (RegistryGetString(runKey, title, runKeyVal, L"") != ERROR_SUCCESS)
    {
      OutputDebugString(L"StartupIsSet: Failed to query startup entry!\n");
      return false;
    }
  }

  WCHAR buffer[512];
  DWORD bufferSize = sizeof(buffer);
  GetModuleFileName(GetModuleHandle(NULL), buffer, bufferSize);

  return runKeyVal == std::wstring(buffer);
}

bool StartupCreateEntry()
{
  HKEY runKey = NULL;
  std::wstring runKeyVal;
  if (RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &runKey) != ERROR_SUCCESS)
  {
    OutputDebugString(L"StartupCreateEntry: Failed to open Run key!\n");
    runKey = NULL;
    return false;
  }

  WCHAR buffer[512];
  DWORD bufferSize = sizeof(buffer);
  GetModuleFileName(GetModuleHandle(NULL), buffer, bufferSize);

  OutputDebugString(L"StartupCreateEntry: Setting startup entry to current exe...\n");
  OutputDebugString(buffer);

  if (RegistrySetString(runKey, title, buffer) != ERROR_SUCCESS)
  {
    OutputDebugString(L"StartupCreateEntry: Failed to create startup entry!\n");
    return false;
  }

  return true;
}

bool StartupDeleteEntry()
{
  HKEY runKey = NULL;
  std::wstring runKeyVal;
  if (RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &runKey) != ERROR_SUCCESS)
  {
    OutputDebugString(L"StartupDeleteEntry: Failed to open Run key!\n");
    runKey = NULL;
    return false;
  }

  return RegDeleteValue(runKey, title) == ERROR_SUCCESS;
}
#pragma endregion

#pragma region SysTray / WndProc stuff
#define WM_USER_TRAYICON ( WM_USER + 1 )

#define ID_TRAY_APP_ICON 5005
#define ID_TRAY_NAME 5001
#define ID_TRAY_STARTUP 5002
#define ID_TRAY_SEP 5003
#define ID_TRAY_EXIT 5004
#define ID_TRAY_CONTROLLER 5006
#define ID_TRAY_DEADZONE 5100

// lower 12 bits are controller index into XboxController::controllers_
#define ID_CONTROLLER_GUIDEBTN  0x2000
#define ID_CONTROLLER_VIBRATION 0x4000

WCHAR tray_text[128];

HWND hwnd;
HINSTANCE instance;

NOTIFYICONDATA notifyIconData;

void SysTrayInit()
{
  memset(&notifyIconData, 0, sizeof(NOTIFYICONDATA));

  notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
  notifyIconData.hWnd = hwnd;
  notifyIconData.uID = ID_TRAY_APP_ICON;

  notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

  notifyIconData.uCallbackMessage = WM_USER_TRAYICON;
  notifyIconData.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPICON));

  wcscpy_s(notifyIconData.szTip, tray_text);
  Shell_NotifyIcon(NIM_ADD, &notifyIconData);
}

void SysTrayShowContextMenu()
{
  POINT lpClickPoint;
  GetCursorPos(&lpClickPoint);

  HMENU hPopMenu = CreatePopupMenu();
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_GRAYED, ID_TRAY_NAME, tray_text);
  auto& pads = XboxController::GetControllers();
  wchar_t ctl_text[128];
  int i = 0;
  for (auto& controller : pads)
  {
    auto hControllerMenu = CreatePopupMenu();
    InsertMenu(hControllerMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING |
      (controller.VibrationEnabled() ? MF_CHECKED : MF_UNCHECKED), ID_CONTROLLER_VIBRATION + i, L"Enable vibration/rumble");

    InsertMenu(hControllerMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING |
      (controller.GuideEnabled() ? MF_CHECKED : MF_UNCHECKED), ID_CONTROLLER_GUIDEBTN + i, L"Enable guide button combination");
    if (combo_guideButton)
    {
      std::string PrintButtonCombination(int combo); // implemented below

      auto combo = "- Combination: " + PrintButtonCombination(combo_guideButton);
      InsertMenuA(hControllerMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_GRAYED, ID_TRAY_SEP, combo.c_str());
    }

    InsertMenu(hControllerMenu, 0xFFFFFFFF, MF_SEPARATOR, ID_TRAY_SEP, L"SEP");

    // Insert current deadzone adjustments into context menu
    auto dz = controller.GetDeadzone();
    swprintf_s(ctl_text, L"Deadzone: LS(%d) RS(%d) LT(%d) RT(%d)", dz.sThumbL, dz.sThumbR, dz.bLeftTrigger, dz.bRightTrigger);
    InsertMenu(hControllerMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_GRAYED, ID_TRAY_DEADZONE + i, ctl_text);

    const char* usb_productname = controller.GetProductName();
    std::string productname;
    if (strlen(usb_productname) > 0)
      productname = std::string(" (") + std::string(usb_productname) + std::string(")");

    auto serialNo = controller.GetSerialNo();
    if(strlen(serialNo))
      swprintf_s(ctl_text, L"%d: %04X:%04X%S (#%S)", controller.GetControllerIndex(),
        controller.GetVendorId(), controller.GetProductId(), productname.c_str(), serialNo);
    else
      swprintf_s(ctl_text, L"%d: %04X:%04X%S", controller.GetControllerIndex(),
      controller.GetVendorId(), controller.GetProductId(), productname.c_str());

    InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)hControllerMenu, ctl_text);
    i++;
  }
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_SEPARATOR, ID_TRAY_SEP, L"SEP");
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING |
    (deadzoneCombinationEnabled ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_DEADZONE, L"Enable Deadzone Adjustment:");
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_GRAYED | MF_UNCHECKED, ID_TRAY_DEADZONE, L"  - Analog Stick (LT+RT+(LS/RS)+DPAD Up/Dn)");
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_GRAYED | MF_UNCHECKED, ID_TRAY_DEADZONE, L"  - Trigger ((LT/RT)+LS+RS+DPAD Up/Dn)");
  
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_SEPARATOR, ID_TRAY_SEP, L"SEP");
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING |
    (StartupIsSet() ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_STARTUP, L"Run on startup");
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_SEPARATOR, ID_TRAY_SEP, L"SEP");
  InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

  SetForegroundWindow(hwnd);
  TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN,
    lpClickPoint.x, lpClickPoint.y, 0, hwnd, NULL);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, lmId;

  static UINT taskbarCreatedMsg = 0;

  switch (message)
  {
  case WM_CREATE:
    // register the "TaskbarCreated" window message, gets used if the systray icon needs to be recreated (eg explorer.exe crashed)
    taskbarCreatedMsg = RegisterWindowMessageW(TEXT("TaskbarCreated"));
    break;
  case WM_COMMAND:
    wmId = LOWORD(wParam);

    if (wmId & ID_CONTROLLER_GUIDEBTN || wmId & ID_CONTROLLER_VIBRATION)
    {
      auto controllerId = wmId & 0xFFF;
      auto& pads = XboxController::GetControllers();
      if (pads.size() > controllerId)
      {
        auto& controller = pads[controllerId];
        if (wmId & ID_CONTROLLER_GUIDEBTN)
          controller.GuideEnabled(!controller.GuideEnabled());
        if (wmId & ID_CONTROLLER_VIBRATION)
          controller.VibrationEnabled(!controller.VibrationEnabled());
      }
    }
    else
    {
      switch (wmId)
      {
      case ID_TRAY_EXIT:
        DestroyWindow(hWnd);
        break;
      case ID_TRAY_STARTUP:
        if (StartupIsSet())
          StartupDeleteEntry();
        else
          StartupCreateEntry();
        break;
      case ID_TRAY_DEADZONE:
        deadzoneCombinationEnabled = !deadzoneCombinationEnabled;
        break;
      default:
        return DefWindowProc(hWnd, message, wParam, lParam);
      }
    }
    break;
  case WM_DESTROY:
    usb_end = true; // signal threads to stop
    PostQuitMessage(0);
    break;
  case WM_USER_TRAYICON:
    lmId = LOWORD(lParam);
    switch (lmId)
    {
    case WM_RBUTTONUP:
    case WM_LBUTTONUP:
      SysTrayShowContextMenu();
      break;
    }
    break;
  default:
    if (message == taskbarCreatedMsg)
      SysTrayInit(); // reinit systray icon
    break;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
#pragma endregion

#pragma region USB check/update threads
void USBDeviceChanged(const XboxController& controller, bool added)
{
  auto& controllers = XboxController::GetControllers();
  int num = (int)controllers.size();
  if (!added)
    num--; // controller is only removed from controllers vector after this function gets called, so minus 1 from the count.

  if (num == 1)
  {
    // only 1 controller left in vector, get info for that controller
    auto& controller = controllers[0];

    const char* usb_productname = controller.GetProductName();
    std::string productname;
    if (strlen(usb_productname) > 0)
      productname = std::string(" (") + std::string(usb_productname) + std::string(")");

    // only 1 controller, set hover text to display info about it, but context-menu text should just display the count
    swprintf_s(notifyIconData.szTip, L"Xb2XInput - active with controller %04X:%04X%S",
      controller.GetVendorId(), controller.GetProductId(), productname.c_str());
    wcscpy_s(tray_text, L"Xb2XInput - active with 1 controller");
  }
  else
  {
    if (!num)
      swprintf_s(notifyIconData.szTip, L"Xb2XInput - waiting for controller");
    else
      swprintf_s(notifyIconData.szTip, L"Xb2XInput - active with %d controllers", num);

    // Update context-menu title text
    wcscpy_s(tray_text, notifyIconData.szTip);
  }

  // Create notification balloon
  notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
  notifyIconData.dwInfoFlags = NIIF_USER;

  notifyIconData.uCallbackMessage = WM_USER_TRAYICON;
  notifyIconData.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPICON));

  wcscpy_s(notifyIconData.szInfoTitle, title);

  swprintf_s(notifyIconData.szInfo, L"%s controller %04X:%04X (%S)", added ? L"Connected" : L"Disconnected",
    controller.GetVendorId(), controller.GetProductId(), controller.GetProductName());

  Shell_NotifyIcon(NIM_MODIFY, &notifyIconData);
}

void USBCheckThread()
{
  while (true)
  {
    if (usb_end)
      return;

    XboxController::OpenDevice();
    Sleep(1500);
  }
}

void USBUpdateThread()
{
  while (true)
  {
    if (usb_end)
      return;

    if (XboxController::GetControllers().size() <= 0)
      Sleep(500); // sleep for a bit so we don't hammer the CPU

    XboxController::UpdateAll();
    Sleep(poll_ms);
  }
}
#pragma endregion

std::thread check_thread;
std::thread update_thread;

std::unordered_map<std::string, int> xinput_buttons =
{
  { "LT", XUSB_GAMEPAD_LT },
  { "RT", XUSB_GAMEPAD_RT },

  { "LB", XUSB_GAMEPAD_LEFT_SHOULDER },
  { "RB", XUSB_GAMEPAD_RIGHT_SHOULDER },

  { "LS", XUSB_GAMEPAD_LEFT_THUMB },
  { "RS", XUSB_GAMEPAD_RIGHT_THUMB },

  { "UP", XUSB_GAMEPAD_DPAD_UP },
  { "DOWN", XUSB_GAMEPAD_DPAD_DOWN },
  { "LEFT", XUSB_GAMEPAD_DPAD_LEFT },
  { "RIGHT", XUSB_GAMEPAD_DPAD_RIGHT },

  { "DPADUP", XUSB_GAMEPAD_DPAD_UP },
  { "DPADDOWN", XUSB_GAMEPAD_DPAD_DOWN },
  { "DPADLEFT", XUSB_GAMEPAD_DPAD_LEFT },
  { "DPADRIGHT", XUSB_GAMEPAD_DPAD_RIGHT },

  { "A", XUSB_GAMEPAD_A },
  { "B", XUSB_GAMEPAD_B },
  { "X", XUSB_GAMEPAD_X },
  { "Y", XUSB_GAMEPAD_Y },

  { "START", XUSB_GAMEPAD_START },
  { "BACK", XUSB_GAMEPAD_BACK },
};

std::string PrintButtonCombination(int combo)
{
  std::stringstream ss;
  for (auto button : xinput_buttons)
  {
    if (combo & button.second)
    {
      ss << button.first;
      ss << " + ";
    }
  }
  auto str = ss.str();
  auto length = str.length();
  if (length < 3)
    return str;

  return str.substr(0, length - 3);
}

int ParseButtonCombination(const char* combo)
{
  int len = strlen(combo);

  int retval = 0;
  std::string cur_token;

  // Parse combo tokens into buttons bitfield (tokens seperated by any non-alphabetical char, eg. +)
  for (int i = 0; i < len; i++)
  {
    char c = combo[i];

    if (!isalpha(c))
    {
      if (cur_token.length() && xinput_buttons.count(cur_token))
        retval |= xinput_buttons[cur_token];

      cur_token.clear();
      continue;
    }
    cur_token += ::toupper(c);
  }

  if (cur_token.length() && xinput_buttons.count(cur_token))
    retval |= xinput_buttons[cur_token];

  return retval;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPTSTR    lpCmdLine,
  _In_ int       nCmdShow)
{
  // Ini path based on executable filename
  int len = GetModuleFileNameA(NULL, ini_path, sizeof(ini_path));
  ini_path[len - 3] = 'i';
  ini_path[len - 2] = 'n';
  ini_path[len - 1] = 'i';

  // Read in & parse any button combinations from our INI
  char* guideComboDefault = "LT + RT + LS + RS";
  char guideCombo[256];

  int ret = GetPrivateProfileStringA("Combinations", "GuideButton", guideComboDefault, guideCombo, 256, ini_path);

  // Write out to INI & create it for us if it doesn't exist
  if (GetFileAttributesA(ini_path) == -1)
    WritePrivateProfileStringA("Combinations", "GuideButton", guideCombo, ini_path);

  combo_guideButton = ParseButtonCombination(guideCombo);

  instance = hInstance;
  wcscpy_s(title, L"Xb2XInput");
  swprintf_s(tray_text, L"Xb2XInput - waiting for controller");

  if (!XboxController::Initialize(title))
    return 1;

  // Start our USB threads
  check_thread = std::thread(USBCheckThread);
  update_thread = std::thread(USBUpdateThread);
  check_thread.detach();
  update_thread.detach();

  // Create window for systray icon
  WNDCLASSEX wcex;
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = instance;
  wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPICON));
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = title;
  wcex.lpszClassName = title;
  wcex.hIconSm = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPICON));

  RegisterClassEx(&wcex);

  hwnd = CreateWindow(title, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, instance, NULL);

  if (!hwnd)
    return FALSE;

  // Init systray icon
  SysTrayInit();

  // Enter window loop
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Window loop is over, delete controllers and systray icon
  usb_end = true;
  XboxController::Close();
  Shell_NotifyIcon(NIM_DELETE, &notifyIconData);

  return (int)msg.wParam;
}
