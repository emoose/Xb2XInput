#include "stdafx.hpp"
#include "XboxController.hpp"
#include <vector>
#include <mutex>

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
  {0x0E4C, 0x3240}, // Radica Gamester
  {0x0E4C, 0x3510}, // Radica Gamester
  {0x0E6F, 0x0003}, // Logic3 Freebird wireless Controller
  {0x0E6F, 0x0005}, // Eclipse wireless Controller
  {0x0E6F, 0x0006}, // Edge wireless Controller
  {0x0E6F, 0x0008}, // After Glow Pro Controller
  {0x0F30, 0x010B}, // Philips Recoil
  {0x0F30, 0x0202}, // Joytech Advanced Controller
  {0x0F30, 0x8888}, // BigBen XBMiniPad Controller
  {0x102C, 0xFF0C}, // Joytech Wireless Advanced Controller
  {0xFFFF, 0xFFFF}, // PowerWave Xbox Controller (The ID's may look sketchy but this controller actually uses it)
};

void USBDeviceChanged(const XboxController& controller, bool added); // from Xb2XInput.cpp

void dbgprintf(const char* format, ...)
{
  static char buffer[256];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  OutputDebugStringA(buffer);
  va_end(args);
}

PVIGEM_CLIENT vigem;
std::vector<XboxController> controllers_;
std::mutex controller_mutex_;
std::mutex usb_mutex_;
std::mutex vigem_alloc_mutex_;

libusb_device_handle* XboxController::OpenDevice()
{
  libusb_device_handle* ret = nullptr;
  libusb_device **devs;
  libusb_device_descriptor desc;
  uint8_t usb_ports[32];

  auto num_devices = libusb_get_device_list(NULL, &devs);
  for (int i = 0; i < num_devices; i++)
  {
    libusb_get_device_descriptor(devs[i], &desc);

    bool found = false;
    for (auto device : xbox_devices)
    {
      if (desc.idVendor == device.first && desc.idProduct == device.second)
      {
        found = true;
        break;
      }
    }
    if (!found)
      continue;

    // check if we're already handling this device
    // (have to check USB port info since libusb_claim_interface doesn't seem to work...)
    bool exists = false;
    int num_ports = libusb_get_port_numbers(devs[i], (uint8_t*)&usb_ports, 32);
    for (auto& controller : controllers_)
    {
      auto& cnt_ports = controller.usb_ports_;
      if (cnt_ports.size() == num_ports && !memcmp(cnt_ports.data(), &usb_ports, cnt_ports.size()))
      {
        exists = true;
        break;
      }
    }
    if (exists)
      continue;

    // open USB device by libusb_device* returned from device list
    if (libusb_open(devs[i],&ret))
      continue;

    auto controller = XboxController(ret, (uint8_t*)&usb_ports, num_ports);
    std::lock_guard<std::mutex> guard(controller_mutex_);

    controllers_.push_back(controller);

    USBDeviceChanged(controller, true);

    return ret;
  }

  return ret;
}

bool XboxController::Initialize(WCHAR* app_title)
{
  static bool inited = false;
  if (inited)
    return true;

  // Init libusb & ViGEm
  auto ret = libusb_init(NULL);
  if (ret < 0)
  {
    MessageBox(NULL, L"Failed to init libusb, make sure libusb-1.0.dll is located next to the exe!", app_title, MB_OK);
    return false;
  }

  vigem = vigem_alloc();
  const auto retval = vigem_connect(vigem);
  if (!vigem || !VIGEM_SUCCESS(retval))
  {
    wchar_t buf[256];
    swprintf_s(buf, L"Failed to init ViGEm (error: %d)", retval);
    MessageBox(NULL, buf, app_title, MB_OK);
    return false;
  }

  inited = true;
  return true;
}

void XboxController::UpdateAll()
{
  std::lock_guard<std::mutex> guard(controller_mutex_);
  auto iter = controllers_.begin();
  while (iter != controllers_.end())
  {
    if (!iter->update())
    {
      USBDeviceChanged(*iter, false);

      libusb_close(iter->usb_handle_);

      std::lock_guard<std::mutex> vigem_guard(vigem_alloc_mutex_);
      vigem_target_x360_unregister_notification(iter->target_);
      vigem_target_remove(vigem, iter->target_);
      vigem_target_free(iter->target_);

      iter = controllers_.erase(iter);
    }
    else
      ++iter;
  }
}

void XboxController::Close()
{
  std::lock_guard<std::mutex> guard(controller_mutex_);
  std::lock_guard<std::mutex> vigem_guard(vigem_alloc_mutex_);

  for (auto& controller : controllers_)
  {
    vigem_target_x360_unregister_notification(controller.target_);
    vigem_target_remove(vigem, controller.target_);
    vigem_target_free(controller.target_);
  }
  controllers_.clear();

  vigem_free(vigem);
}

const std::vector<XboxController>& XboxController::GetControllers()
{
  return controllers_;
}

XboxController::XboxController(libusb_device_handle* handle, uint8_t* usb_ports, int num_ports) : usb_handle_(handle) {
  usb_productname_[0] = 0;
  usb_vendorname_[0] = 0;

  usb_ports_.resize(num_ports);
  memcpy(usb_ports_.data(), usb_ports, num_ports);

  // try getting USB product info
  auto* dev = libusb_get_device(handle);
  if (!dev)
    return;

  if (libusb_get_device_descriptor(dev, &usb_desc_) != 0)
    return;

  // try finding interrupt/bulk endpoints
  struct libusb_config_descriptor *conf_desc;
  libusb_get_config_descriptor(dev, 0, &conf_desc);
  int nb_ifaces = conf_desc->bNumInterfaces;
  for (int i = 0; i < nb_ifaces; i++)
  {
    for (int j = 0; j < conf_desc->interface[i].num_altsetting; j++)
    {
      for (int k = 0; k < conf_desc->interface[i].altsetting[j].bNumEndpoints; k++)
      {
        auto endpoint = &conf_desc->interface[i].altsetting[j].endpoint[k];
        // Use the first interrupt or bulk IN/OUT endpoints as default
        if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) & (LIBUSB_TRANSFER_TYPE_BULK | LIBUSB_TRANSFER_TYPE_INTERRUPT))
        {
          if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
          {
            if (!endpoint_in_)
            {
              usb_iface_num_ = i;
              usb_iface_setting_num_ = j;
              endpoint_in_ = endpoint->bEndpointAddress;
            }
          }
          else
          {
            if (!endpoint_out_)
              endpoint_out_ = endpoint->bEndpointAddress;
          }
        }
      }
    }
  }
  libusb_free_config_descriptor(conf_desc);

  // if we have interrupt endpoints then we have to claim the interface & set altsetting in order to use them
  if (endpoint_in_ || endpoint_out_)
  {
    libusb_claim_interface(handle, usb_iface_num_);
    libusb_set_interface_alt_setting(handle, usb_iface_num_, usb_iface_setting_num_);
  }

  libusb_get_string_descriptor_ascii(handle, usb_desc_.iProduct, (unsigned char*)usb_productname_, sizeof(usb_productname_));
  libusb_get_string_descriptor_ascii(handle, usb_desc_.iManufacturer, (unsigned char*)usb_vendorname_, sizeof(usb_vendorname_));

  usb_product_ = usb_desc_.idProduct;
  usb_vendor_ = usb_desc_.idVendor;
}

XboxController::~XboxController()
{
  closing_ = true;
  active_ = false;
}

void CALLBACK XboxController::OnVigemNotification(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber)
{
  for (auto& controller : controllers_)
  {
    if (controller.target_ != Target)
      continue;

    extern bool vibrationEnabled;
    if (!vibrationEnabled)
    {
      LargeMotor = SmallMotor = 0;
    }

    memset(&controller.output_prev_, 0, sizeof(XboxOutputReport));
    controller.output_prev_.bSize = sizeof(XboxOutputReport);
    controller.output_prev_.Rumble.wLeftMotorSpeed = _byteswap_ushort(LargeMotor); // why do these need to be byteswapped???
    controller.output_prev_.Rumble.wRightMotorSpeed = _byteswap_ushort(SmallMotor);

    {
      std::lock_guard<std::mutex> guard(usb_mutex_);
      libusb_control_transfer(controller.usb_handle_, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        HID_SET_REPORT, (HID_REPORT_TYPE_OUTPUT << 8) | 0x00, 0, (unsigned char*)&controller.output_prev_, sizeof(XboxOutputReport), 1000);
    }

    break;
  }
}

int XboxController::GetUserIndex() {
  if (vigem && target_)
  {
    ULONG idx = 0;
    if (VIGEM_SUCCESS(vigem_target_x360_get_user_index(vigem, target_, &idx)))
      return idx;
  }
  return -1;
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

      std::lock_guard<std::mutex> vigem_guard(vigem_alloc_mutex_);
      target_ = vigem_target_x360_alloc();
      auto ret = vigem_target_add(vigem, target_);
      if (VIGEM_SUCCESS(ret))
      {
        vigem_target_x360_register_notification(vigem, target_, XboxController::OnVigemNotification);
        active_ = true;
        break;
      }

      // failed to create ViGEm target, retry next update
      return true;
    }
  }

  if (closing_)
    return true;

  memset(&input_prev_, 0, sizeof(XboxInputReport));
  int length = 0;
  int ret = -1;


  // if we have interrupt endpoints use those for better compatibility, otherwise fallback to control transfers
  if (endpoint_in_)
  {
    extern int poll_ms;
    ret = libusb_interrupt_transfer(usb_handle_, endpoint_in_, (unsigned char*)&input_prev_, sizeof(XboxInputReport), &length, poll_ms);
    if (ret < 0)
      return true; // No input available atm
  }
  else
  {
    std::lock_guard<std::mutex> guard(usb_mutex_);
    ret = libusb_control_transfer(usb_handle_, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
      HID_GET_REPORT, (HID_REPORT_TYPE_INPUT << 8) | 0x00, 0, (unsigned char*)&input_prev_, sizeof(XboxInputReport), 1000);

    if (ret < 0)
    {
      dbgprintf(__FUNCTION__ ": libusb control transfer failed (code %d)", ret);
      return false;
    }
  }

  if (input_prev_.bSize != sizeof(XboxInputReport))
  {
    dbgprintf(__FUNCTION__ ": controller returned invalid report size %d (expected %d)", input_prev_.bSize, sizeof(XboxInputReport));
    return false;
  }

  memset(&gamepad_, 0, sizeof(XUSB_REPORT));

  // Copy over digital buttons
  gamepad_.wButtons = input_prev_.Gamepad.wButtons;

  // Convert analog buttons to digital
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_A] ? XUSB_GAMEPAD_A : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_B] ? XUSB_GAMEPAD_B : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_X] ? XUSB_GAMEPAD_X : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_Y] ? XUSB_GAMEPAD_Y : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_WHITE] ? XUSB_GAMEPAD_LEFT_SHOULDER : 0;
  gamepad_.wButtons |= input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_BLACK] ? XUSB_GAMEPAD_RIGHT_SHOULDER : 0;

  // Copy over remaining analog values
  gamepad_.bLeftTrigger = input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_LEFT_TRIGGER];
  gamepad_.bRightTrigger = input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_RIGHT_TRIGGER];

  // Secret guide combination: LT + RT + LS + RS
  extern bool guideCombinationEnabled;
  if(guideCombinationEnabled)
    if ((input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_LEFT_THUMB) && (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_RIGHT_THUMB) &&
      (gamepad_.bLeftTrigger >= 0x8) && (gamepad_.bRightTrigger >= 0x8))
    {
      gamepad_.wButtons |= XUSB_GAMEPAD_GUIDE;

      // Clear combination from the emulated pad, don't want it to interfere with guide:
      gamepad_.wButtons &= ~XUSB_GAMEPAD_LEFT_THUMB;
      gamepad_.wButtons &= ~XUSB_GAMEPAD_RIGHT_THUMB;
      gamepad_.bLeftTrigger = 0;
      gamepad_.bRightTrigger = 0;
    }

  gamepad_.sThumbLX = input_prev_.Gamepad.sThumbLX;
  gamepad_.sThumbLY = input_prev_.Gamepad.sThumbLY;
  gamepad_.sThumbRX = input_prev_.Gamepad.sThumbRX;
  gamepad_.sThumbRY = input_prev_.Gamepad.sThumbRY;

  // Write gamepad to virtual XInput device
  vigem_target_x360_update(vigem, target_, gamepad_);

  return true;
}
