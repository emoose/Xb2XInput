#pragma once
#include "ViGEm/Client.h"
#include "ViGEm/Util.h"
#include <libusb.h>

#include <vector>
#include <mutex>
#include <unordered_map>

// original xbox XINPUT definitions from https://github.com/paralin/hl2sdk/blob/master/common/xbox/xboxstubs.h

// digital button bitmasks
#define OGXINPUT_GAMEPAD_DPAD_UP           0x0001
#define OGXINPUT_GAMEPAD_DPAD_DOWN         0x0002
#define OGXINPUT_GAMEPAD_DPAD_LEFT         0x0004
#define OGXINPUT_GAMEPAD_DPAD_RIGHT        0x0008
#define OGXINPUT_GAMEPAD_START             0x0010
#define OGXINPUT_GAMEPAD_BACK              0x0020
#define OGXINPUT_GAMEPAD_LEFT_THUMB        0x0040
#define OGXINPUT_GAMEPAD_RIGHT_THUMB       0x0080

// analog button indexes
#define OGXINPUT_GAMEPAD_A                0
#define OGXINPUT_GAMEPAD_B                1
#define OGXINPUT_GAMEPAD_X                2
#define OGXINPUT_GAMEPAD_Y                3
#define OGXINPUT_GAMEPAD_BLACK            4
#define OGXINPUT_GAMEPAD_WHITE            5
#define OGXINPUT_GAMEPAD_LEFT_TRIGGER     6
#define OGXINPUT_GAMEPAD_RIGHT_TRIGGER    7

// Used for button combination parsing/checking
// See Xb2XInput.cpp -> xinput_buttons map
#define XUSB_GAMEPAD_LT (XUSB_GAMEPAD_Y << 1)
#define XUSB_GAMEPAD_RT (XUSB_GAMEPAD_Y << 2)

#pragma pack(push, 1)
typedef struct _OGXINPUT_RUMBLE
{
  WORD   wLeftMotorSpeed;
  WORD   wRightMotorSpeed;
} OGXINPUT_RUMBLE, *POGXINPUT_RUMBLE;

typedef struct _OGXINPUT_GAMEPAD
{
  WORD    wButtons;
  BYTE    bAnalogButtons[8];
  short   sThumbLX;
  short   sThumbLY;
  short   sThumbRX;
  short   sThumbRY;
} OGXINPUT_GAMEPAD, *POGXINPUT_GAMEPAD;

struct XboxInputReport {
  BYTE bReportId;
  BYTE bSize;

  OGXINPUT_GAMEPAD Gamepad;
};

struct XboxOutputReport {
  BYTE bReportId;
  BYTE bSize;

  OGXINPUT_RUMBLE Rumble;
};

struct Deadzone {
  short sThumbL;
  short sThumbR;
  BYTE bLeftTrigger;
  BYTE bRightTrigger;
  bool hold; 
};

struct UserSettings {
  bool guide_enabled = false;
  bool vibration_enabled = false;

  Deadzone deadzone = { 0 };

  std::unordered_map<int, int> button_remap;
  bool remap_enabled = false;
};

#pragma pack(pop)

#define HID_GET_REPORT                0x01
#define HID_SET_REPORT                0x09
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02

class XboxController
{
  std::vector<uint8_t*> usb_ports_;
  bool active_ = false;
  libusb_device_handle* usb_handle_ = nullptr;
  int usb_product_ = 0;
  int usb_vendor_ = 0;

  PVIGEM_TARGET target_ = 0;

  libusb_device_descriptor usb_desc_;
  char usb_productname_[128];
  char usb_vendorname_[128];
  char usb_serialno_[128];
  
  std::string ini_key_; // key-name to use when loading settings from config ini

  bool closing_ = false;

  XboxInputReport input_prev_;
  XboxOutputReport output_prev_;
  XUSB_REPORT gamepad_;

  int usb_iface_num_ = 0;
  int usb_iface_setting_num_ = 0;
  uint8_t endpoint_in_ = 0;
  uint8_t endpoint_out_ = 0;

  int deadZoneCalc(short *x_out, short *y_out, short x, short y, short deadzone, short sickzone);

  UserSettings settings_;

  bool update();

  static int GetSettingInt(const std::string& setting, int default_val, const std::string& ini_key);
  static std::string GetSettingString(const std::string& setting, const std::string& default_val, const std::string& ini_key);
  static bool GetSettingBool(const std::string& setting, bool default_val, const std::string& ini_key);
  static void SetSetting(const std::string& setting, const std::string& value, const std::string& ini_key);

  static UserSettings LoadSettings(const std::string& ini_key, const UserSettings& defaults);
  void SaveDeadzones();

public:
  bool GuideEnabled() { return settings_.guide_enabled; }
  void GuideEnabled(bool value);

  bool VibrationEnabled() { return settings_.vibration_enabled; }
  void VibrationEnabled(bool value);

  XboxController(libusb_device_handle* handle, uint8_t* usb_ports, int num_ports);
  ~XboxController();
  int GetProductId() const { return usb_product_; }
  int GetVendorId() const { return usb_vendor_; }
  const char* GetProductName() const { return usb_productname_; }
  const char* GetVendorName() const { return usb_vendorname_; }
  const char* GetSerialNo() const { return usb_serialno_; }
  Deadzone GetDeadzone() { return settings_.deadzone; }
  
  int GetControllerIndex()
  { 
    if (!target_)
      return -1;
    return vigem_target_get_index(target_);
  }

  int GetUserIndex();

  static bool Initialize(WCHAR* app_title);
  static void UpdateAll();
  static void Close();
  static libusb_device_handle* OpenDevice();
  static std::vector<XboxController>& GetControllers();

  static void CALLBACK OnVigemNotification(
    PVIGEM_CLIENT Client,
    PVIGEM_TARGET Target,
    UCHAR LargeMotor,
    UCHAR SmallMotor,
    UCHAR LedNumber
  );
};
