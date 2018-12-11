#include "stdafx.hpp"
#include "XboxController.hpp"
#include <vector>

std::vector<std::pair<int, int>> xbox_devices =
{
  {0x044F, 0x0F07}, // Thrustmaster Controller
  {0x045E, 0x0202}, // Microsoft Xbox Controller v1 (US)
  {0x045E, 0x0285}, // Microsoft Xbox Controller S (Japan)
  {0x045E, 0x0287}, // Microsoft Xbox Controller S
  {0x045E, 0x0288}, // Microsoft Xbox Controller S v2
  {0x045E, 0x0289}, // Microsoft Xbox Controller v2 (US)
  {0x046D, 0xCA84}, // Logitech Cordless Precision
  {0x046D, 0xCA88}, // Logitech Thunderpad
  {0x05FD, 0x1007}, // Mad Catz Controller (unverified)
  {0x05FD, 0x107A}, // InterAct PowerPad Pro X-box pad
  {0x05FE, 0x3030}, // Chic Controller
  {0x05FE, 0x3031}, // Chic Controller
  {0x062A, 0x0020}, // Logic3 Xbox GamePad
  {0x06A3, 0x0201}, // Saitek Adrenalin
  {0x0738, 0x4506}, // MadCatz 4506 Wireless Controller
  {0x0738, 0x4516}, // MadCatz Control Pad
  {0x0738, 0x4520}, // MadCatz Control Pad Pro
  {0x0738, 0x4522}, // MadCatz LumiCON (my one, also the one no drivers seem to include out of the box)
  {0x0738, 0x4526}, // MadCatz Control Pad Pro
  {0x0738, 0x4536}, // MadCatz MicroCON
  {0x0738, 0x4556}, // MadCatz Lynx Wireless Controller
  {0x0738, 0x4586}, // MadCatz MicroCon Wireless Controller
  {0x0738, 0x4588}, // MadCatz Blaster
  {0x0C12, 0x0005}, // Intec wireless
  {0x0C12, 0x8801}, // Nyko Xbox Controller
  {0x0C12, 0x8802}, // Zeroplus Xbox Controller
  {0x0C12, 0x880A}, // Pelican Eclipse PL-2023
  {0x0C12, 0x8810}, // Zeroplus Xbox Controller
  {0x0C12, 0x9902}, // HAMA VibraX - "FAULTY HARDWARE"
  {0x0E4C, 0x1097}, // Radica Gamester Controller
  {0x0E4C, 0x2390}, // Radica Games Jtech Controller
  {0x0E4C, 0x3510}, // Radica Gamester
  {0x0E6F, 0x0003}, // Logic3 Freebird wireless Controller
  {0x0E6F, 0x0005}, // Eclipse wireless Controller
  {0x0E6F, 0x0006}, // Edge wireless Controller
  {0x0E6F, 0x0008}, // After Glow Pro Controller
  {0x0F30, 0x010B}, // Philips Recoil
  {0x0F30, 0x0202}, // Joytech Advanced Controller
  {0x0F30, 0x8888}, // BigBen XBMiniPad Controller
  {0x102C, 0xFF0C}, // Joytech Wireless Advanced Controller
  //{0xFFFF, 0xFFFF}, // "Chinese-made Xbox Controller" (disabled since ID seems sketchy)
};

void dbgprintf(const char* format, ...)
{
  static char buffer[256];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  OutputDebugStringA(buffer);
  va_end(args);
}

std::vector<XboxController> controllers_;
std::mutex controller_mutex_;
std::vector<std::function<void(const XboxController& device, bool added)>> device_change_callbacks_;
bool ports_[4];

libusb_device_handle* XboxController::OpenDevice()
{
  libusb_device_handle* ret = nullptr;
  for (auto device : xbox_devices)
  {
    ret = libusb_open_device_with_vid_pid(NULL, device.first, device.second);
    if (!ret)
      continue;

    int portNum = 4;
    for (int i = 0; i < 4; i++)
    {
      if (!ports_[i])
      {
        portNum = i;
        ports_[i] = true;
        break;
      }
    }

    std::lock_guard<std::mutex> guard(controller_mutex_);

    auto controller = XboxController(ret, portNum);
    controllers_.push_back(controller);

    deviceChanged(controller, true);

    return ret;
  }
  return nullptr;
}

bool XboxController::Initialize(WCHAR* app_title)
{
  static bool inited = false;
  if (inited)
    return true;

  // Init libusb & XOutput
  auto ret = libusb_init(NULL);
  if (ret < 0)
  {
    MessageBox(NULL, L"Failed to init libusb, make sure libusb-1.0.dll is located next to the exe!", app_title, MB_OK);
    return false;
  }

  try {
    XOutput::XOutputInitialize();
  }
  catch (XOutput::XOutputError &e) {
    wchar_t buf[256];
    swprintf_s(buf, L"Failed to init XOutput (error: %S)\r\nMake sure XOutput1_1.dll is located next to the exe!", e.what());
    MessageBox(NULL, buf, app_title, MB_OK);
    return false;
  }

  DWORD unused;
  if (XOutput::XOutputGetRealUserIndex(0, &unused) != 0) {
    MessageBox(NULL, L"Failed to setup XOutput :(", app_title, MB_OK);
    return false;
  }

  // Unplug any existing XOutput devices
  XOutput::XOutputUnPlugAll();
  for (int i = 0; i < 4; i++)
  {
    XOutput::XOutputUnPlug(i);
    ports_[i] = false;
  }

  inited = true;
  return true;
}

void XboxController::OnDeviceChanged(std::function<void(const XboxController& device, bool added)> callback)
{
  device_change_callbacks_.push_back(callback);
}

void XboxController::UpdateAll()
{
  std::lock_guard<std::mutex> guard(controller_mutex_);
  auto iter = controllers_.begin();
  while (iter != controllers_.end())
  {
    if (!iter->update())
    {
      deviceChanged(*iter, false);

      int port = iter->GetPortNum();
      iter = controllers_.erase(iter);
      ports_[port] = false;
    }
    else
      ++iter;
  }
}

void XboxController::Close()
{
  std::lock_guard<std::mutex> guard(controller_mutex_);
  controllers_.clear();
}

void XboxController::deviceChanged(const XboxController& device, bool added)
{
  for (auto cb : device_change_callbacks_)
    cb(device, added);
}

std::vector<XboxController>& XboxController::GetControllers() 
{
  return controllers_;
}

XboxController::XboxController(libusb_device_handle* handle, int port) : usb_handle_(handle), port_(port) {
  usb_productname_[0] = 0;
  usb_vendorname_[0] = 0;

  // try getting USB product info
  auto* dev = libusb_get_device(handle);
  if (dev)
  {
    if (libusb_get_device_descriptor(dev, &usb_desc_) != 0)
      return;

    auto prod_idx = usb_desc_.iProduct;

    libusb_get_string_descriptor_ascii(handle, usb_desc_.iProduct, (unsigned char*)usb_productname_, sizeof(usb_productname_));
    libusb_get_string_descriptor_ascii(handle, usb_desc_.iManufacturer, (unsigned char*)usb_vendorname_, sizeof(usb_vendorname_));

    usb_product_ = usb_desc_.idProduct;
    usb_vendor_ = usb_desc_.idVendor;
  }
}

XboxController::~XboxController()
{
  XOutput::XOutputUnPlug(port_);
  active_ = false;
}

// XboxController::Update: returns false if controller disconnected
bool XboxController::update()
{
  if (!active_)
  {
    while (true)
    {
      if (closing_)
        return true;

      auto ret = XOutput::XOutputPlugIn(port_);
      if (!ret)
      {
        active_ = true;
        break;
      }

      // failed to plug in XOutput port, retry in 1.5s
      Sleep(1500);
    }
  }

  if (closing_)
    return true;

  auto ret = libusb_control_transfer(usb_handle_, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
    HID_GET_REPORT, (HID_REPORT_TYPE_INPUT << 8) | 0x00, 0, (unsigned char*)&input_prev_, sizeof(XboxInputReport), 1000);

  if (ret < 0)
  {
    dbgprintf(__FUNCTION__ ": libusb transfer failed (code %d)", ret);
    return false;
  }

  if (input_prev_.bSize != sizeof(XboxInputReport))
  {
    dbgprintf(__FUNCTION__ ": controller returned invalid report size %d (expected %d)", input_prev_.bSize, sizeof(XboxInputReport));
    return false;
  }

  memset(&gamepad_, 0, sizeof(XINPUT_GAMEPAD));

  // Copy over digital buttons
  gamepad_.wButtons = input_prev_.Gamepad.wButtons;

  // Convert analog buttons to digital
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_A] ? XINPUT_GAMEPAD_A : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_B] ? XINPUT_GAMEPAD_B : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_X] ? XINPUT_GAMEPAD_X : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_Y] ? XINPUT_GAMEPAD_Y : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_WHITE] ? XINPUT_GAMEPAD_LEFT_SHOULDER : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_BLACK] ? XINPUT_GAMEPAD_RIGHT_SHOULDER : 0;

  // Copy over remaining analog values
  gamepad_.bLeftTrigger = input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_LEFT_TRIGGER];
  gamepad_.bRightTrigger = input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_RIGHT_TRIGGER];

  gamepad_.sThumbLX = input_prev_.Gamepad.sThumbLX;
  gamepad_.sThumbLY = input_prev_.Gamepad.sThumbLY;
  gamepad_.sThumbRX = input_prev_.Gamepad.sThumbRX;
  gamepad_.sThumbRY = input_prev_.Gamepad.sThumbRY;

  // Write gamepad to virtual XInput device
  XOutput::XOutputSetState(0, &gamepad_);

  if (closing_)
    return true;

  BYTE vibrate = 0;
  BYTE big_motor = 0;
  BYTE small_motor = 0;
  BYTE led = 0;

  if (closing_)
    return true;

  ret = XOutput::XOutputGetState(0, &vibrate, &big_motor, &small_motor, &led);

  if (closing_)
    return true;

  if (!ret && vibrate != 0)
  {
    memset(&output_prev_, 0, sizeof(XboxOutputReport));
    output_prev_.bSize = sizeof(XboxOutputReport);
    output_prev_.Rumble.wLeftMotorSpeed = _byteswap_ushort(big_motor); // why do these need to be byteswapped???
    output_prev_.Rumble.wRightMotorSpeed = _byteswap_ushort(small_motor);

    libusb_control_transfer(usb_handle_, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
      HID_SET_REPORT, (HID_REPORT_TYPE_OUTPUT << 8) | 0x00, 0, (unsigned char*)&output_prev_, sizeof(XboxOutputReport), 1000);
  }

  return true;
}
