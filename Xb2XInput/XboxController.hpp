#pragma once
#include <functional>
#include <mutex>
#include <vector>

#include "XOutput.hpp"
#include <libusb.h>

#pragma pack(push, 1)
struct XboxInputReport {
  /* 0x00 */ uint8_t unused;
  /* 0x01 */ uint8_t report_len;
  /* 0x02 */ uint16_t digital_btns;
  /* 0x04 */ uint8_t btn_a;
  /* 0x05 */ uint8_t btn_b;
  /* 0x06 */ uint8_t btn_x;
  /* 0x07 */ uint8_t btn_y;
  /* 0x08 */ uint8_t btn_black;
  /* 0x09 */ uint8_t btn_white;
  /* 0x0A */ uint8_t trigger_left;
  /* 0x0B */ uint8_t trigger_right;
  /* 0x0C */ int16_t stick_left_x;
  /* 0x0E */ int16_t stick_left_y;
  /* 0x10 */ int16_t stick_right_x;
  /* 0x12 */ int16_t stick_right_y;
};

struct XboxOutputReport {
  uint8_t unk_0;
  uint8_t report_len;
  uint8_t unk_2;
  uint8_t left;
  uint8_t unk_4;
  uint8_t right;
};
#pragma pack(pop)

enum class XboxButton {
  DpadUp = 1,
  DpadDown = 2,
  DpadLeft = 4,
  DpadRight = 8,
  Start = 0x10,
  Back = 0x20,
  LS = 0x40,
  RS = 0x80
};

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

  bool output_inited_ = false;
  bool closing_ = false;

  XboxInputReport input_prev_;
  XboxOutputReport output_prev_;
  XINPUT_GAMEPAD gamepad_;

  bool update();

  static void deviceChanged(const XboxController& device, bool added);
public:
  XboxController(libusb_device_handle* handle, int port);
  ~XboxController();
  int GetPortNum() const { return port_; }
  int GetProductId() const { return usb_product_; }
  int GetVendorId() const { return usb_vendor_; }
  const char* GetProductName() const { return usb_productname_; }
  const char* GetVendorName() const { return usb_vendorname_; }

  static bool Initialize(WCHAR* app_title);
  static void OnDeviceChanged(std::function<void(const XboxController& device, bool added)> callback);
  static void UpdateAll();
  static void Close();
  static libusb_device_handle* OpenDevice();
  static std::vector<XboxController>& GetControllers();
};
