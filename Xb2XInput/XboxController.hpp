#pragma once
#include "XOutput.hpp"
#include <libusb.h>

#include <vector>

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
#pragma pack(pop)

#define HID_GET_REPORT                0x01
#define HID_SET_REPORT                0x09
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02

class XboxController
{
  int port_;
  bool active_ = false;
  libusb_device_handle* usb_handle_ = nullptr;
  int usb_product_ = 0;
  int usb_vendor_ = 0;

  libusb_device_descriptor usb_desc_;
  char usb_productname_[128];
  char usb_vendorname_[128];

  bool closing_ = false;

  XboxInputReport input_prev_;
  XboxOutputReport output_prev_;
  XINPUT_GAMEPAD gamepad_;

  bool update();
public:
  XboxController(libusb_device_handle* handle, int port);
  ~XboxController();
  int GetPortNum() const { return port_; }
  int GetProductId() const { return usb_product_; }
  int GetVendorId() const { return usb_vendor_; }
  const char* GetProductName() const { return usb_productname_; }
  const char* GetVendorName() const { return usb_vendorname_; }

  static bool Initialize(WCHAR* app_title);
  static void UpdateAll();
  static void Close();
  static libusb_device_handle* OpenDevice();
  static const std::vector<XboxController>& GetControllers();
};
