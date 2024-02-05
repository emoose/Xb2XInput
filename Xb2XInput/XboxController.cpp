#include "stdafx.hpp"
#include "XboxController.hpp"
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>

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
  {0x0738, 0x4520}, // MadCatz MC2 Racing Wheel and Pedals
  {0x0738, 0x4522}, // MadCatz LumiCON (my one, also the one no drivers seem to include out of the box)
  {0x0738, 0x4526}, // MadCatz Control Pad Pro
  {0x0738, 0x4536}, // MadCatz MicroCON
  {0x0738, 0x4556}, // MadCatz Lynx Wireless Controller
  {0x0738, 0x4586}, // MadCatz MicroCon Wireless Controller
  {0x0738, 0x4588}, // MadCatz Blaster
  {0x0738, 0x6320}, // MadCatz MC2 Universal Racing Wheel and Pedals
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

UserSettings defaults_;

// Xb2XInput.cpp externs
void USBDeviceChanged(const XboxController& controller, bool added);
extern char ini_path[4096];
extern int poll_ms;

extern int combo_guideButton;
extern int combo_deadzoneIncrease;
extern int combo_deadzoneDecrease;

// Hacky defines to help with remap stuff
#define XUSB_GAMEPAD_Start XUSB_GAMEPAD_START
#define XUSB_GAMEPAD_Back XUSB_GAMEPAD_BACK
#define XUSB_GAMEPAD_LS XUSB_GAMEPAD_LEFT_THUMB
#define XUSB_GAMEPAD_RS XUSB_GAMEPAD_RIGHT_THUMB
#define XUSB_GAMEPAD_White XUSB_GAMEPAD_LEFT_SHOULDER
#define XUSB_GAMEPAD_Black XUSB_GAMEPAD_RIGHT_SHOULDER

#define XUSB_GAMEPAD_DpadUp XUSB_GAMEPAD_DPAD_UP
#define XUSB_GAMEPAD_DpadDown XUSB_GAMEPAD_DPAD_DOWN
#define XUSB_GAMEPAD_DpadLeft XUSB_GAMEPAD_DPAD_LEFT
#define XUSB_GAMEPAD_DpadRight XUSB_GAMEPAD_DPAD_RIGHT

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

UserSettings XboxController::LoadSettings(const std::string& ini_key, const UserSettings& defaults)
{
  UserSettings ret;

  ret.guide_enabled = GetSettingBool("EnableGuide", defaults.guide_enabled, ini_key);

  if (ret.guide_enabled == defaults.guide_enabled) // write setting to ini if this == default, in case it didn't already exist
    SetSetting("EnableGuide", ret.guide_enabled ? "true" : "false", ini_key);

  ret.vibration_enabled = GetSettingBool("EnableVibration", defaults.vibration_enabled, ini_key);

  if (ret.vibration_enabled == defaults.vibration_enabled) // write setting to ini if this == default, in case it didn't already exist
    SetSetting("EnableVibration", ret.vibration_enabled ? "true" : "false", ini_key); // write setting to ini if this == default, in case it didn't already exist

  ret.deadzone.sThumbL = min(max(GetSettingInt("DeadzoneLeftStick", defaults.deadzone.sThumbL, ini_key), 0), SHRT_MAX);
  ret.deadzone.sThumbR = min(max(GetSettingInt("DeadzoneRightStick", defaults.deadzone.sThumbR, ini_key), 0), SHRT_MAX);
  ret.deadzone.bLeftTrigger = min(max(GetSettingInt("DeadzoneLeftTrigger", defaults.deadzone.bLeftTrigger, ini_key), 0), 0xFF);
  ret.deadzone.bRightTrigger = min(max(GetSettingInt("DeadzoneRightTrigger", defaults.deadzone.bRightTrigger, ini_key), 0), 0xFF);

  ret.button_remap.clear();
  if (defaults.remap_enabled)
    ret.button_remap = defaults.button_remap;

  ret.remap_enabled = GetSettingBool("RemapEnable", defaults.remap_enabled, ini_key);

  if (ret.remap_enabled)
  {
    std::string remap;

    int ParseButtonCombination(const char* combo); // Xb2XInput.cpp

    // TODO: if RemapEnable, should we SetSetting for all of these so INI is populated with the proper Remap* settings?
#define LoadMap(btn) \
    remap = GetSettingString("Remap" #btn, "", ini_key); \
    if (remap.length()) \
    { \
      int combo = ParseButtonCombination(remap.c_str()); \
      if (combo) \
        ret.button_remap[XUSB_GAMEPAD_##btn] = combo; \
    }

    LoadMap(A);
    LoadMap(B);
    LoadMap(X);
    LoadMap(Y);
    LoadMap(Start);
    LoadMap(Back);
    LoadMap(LS);
    LoadMap(RS);
    LoadMap(Black);
    LoadMap(White);
    LoadMap(DpadUp);
    LoadMap(DpadDown);
    LoadMap(DpadLeft);
    LoadMap(DpadRight);
    LoadMap(LT);
    LoadMap(RT);

#undef LoadMap
  }

  return ret;
}

bool XboxController::Initialize(WCHAR* app_title)
{
  static bool inited = false;
  if (inited)
    return true;

  // Load default INI settings
  defaults_.guide_enabled = true;
  defaults_.vibration_enabled = true;
  defaults_.deadzone = { 0 };
  defaults_.remap_enabled = false;

  defaults_ = LoadSettings("Default", defaults_);

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

std::vector<XboxController>& XboxController::GetControllers()
{
  return controllers_;
}

XboxController::XboxController(libusb_device_handle* handle, uint8_t* usb_ports, int num_ports) : usb_handle_(handle) {
  usb_productname_[0] = 0;
  usb_vendorname_[0] = 0;
  usb_serialno_[0] = 0;

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
  libusb_get_string_descriptor_ascii(handle, usb_desc_.iSerialNumber, (unsigned char*)usb_serialno_, sizeof(usb_serialno_));

  // Use serial no. as INI key if controller has one, else VID/PID
  std::stringstream ss;
  if (strlen(usb_serialno_))
    ss << usb_serialno_;
  else
  {
    ss << std::setfill('0') << std::setw(4) <<
      std::hex << usb_desc_.idVendor;
    ss << ':';
    ss << std::setfill('0') << std::setw(4) <<
      std::hex << usb_desc_.idProduct;
  }

  ini_key_ = ss.str();

  // Read in INI settings for this controller
  settings_ = LoadSettings(ini_key_, defaults_);

  usb_product_ = usb_desc_.idProduct;
  usb_vendor_ = usb_desc_.idVendor;
}

XboxController::~XboxController()
{
  closing_ = true;
  active_ = false;
}

void CALLBACK XboxController::OnVigemNotification(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData)
{
  for (auto& controller : controllers_)
  {
    if (controller.target_ != Target)
      continue;

    if (!controller.settings_.vibration_enabled)
      LargeMotor = SmallMotor = 0;

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

int XboxController::deadZoneCalc(short *x_out, short *y_out, short x, short y, short deadzone, short sickzone){
  // Returns 0 if in deadzone, 1 in sickzone, 2 if passthrough. 

  // Protect from NULL input pointers (used for 1-D deadzone)
  short dummyvar;
  if (!x_out){
    x_out = &dummyvar;
  }
  if (!y_out){
    y_out = &dummyvar;
  }

  short status;
  
  // If no deadzone, pass directly through.
  if (deadzone == 0){
    *x_out = x;
    *y_out = y;
    return 2;
  } 

  // convert to polar coordinates
  int r_in = sqrt(pow(x,2)+pow(y,2));
  short r_sign = (y >= 0 ? 1 : -1); // For negative Y-axis cartesian coordinates 
  float theta = acos((float)x/fmax(1.0,r_in));
  int r_out;

  // Return origin if in Deadzone 
  if (r_in < deadzone){
    status = 0;
    r_out = 0;
  }

  // Scale to full range over "sickzone" for precision near deadzone
  // this way output doesn't jump from 0,0 to deadzone limit.
  else if (r_in < sickzone){ 
    status = 1;
    r_out = (float)(r_in - deadzone) / (float)(sickzone - deadzone) * sickzone;
  } else {
    status = 2;
    r_out = r_in;
  }

  // Convert back to cartesian coordinates for x,y output
  *x_out = r_out*cos(theta);
  *y_out = r_sign*r_out*sin(theta);

  return status;
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
        vigem_target_x360_register_notification(vigem, target_, XboxController::OnVigemNotification, NULL);
        active_ = true;
        break;
      }

      // failed to create ViGEm target, retry next update
      return true;
    }
  }

  if (closing_)
    return true;

  if (usb_transfer_) {
    if (usb_transfer_->status == LIBUSB_TRANSFER_TIMED_OUT) {
      libusb_free_transfer(usb_transfer_);
      usb_transfer_ = NULL;
    }
  }
  else {
    if (!PollUSB())
      return false;
  }

  return true;
}

// XboxController::pollUSB: sends a poll request to usb and calls back when finished
bool XboxController::PollUSB()
{
  int ret = -1;
  extern int poll_ms;
  usb_transfer_ = libusb_alloc_transfer(0);

  // if we have interrupt endpoints use those for better compatibility, otherwise fallback to control transfers
  if (endpoint_in_)
  {
    libusb_fill_interrupt_transfer(usb_transfer_,
      usb_handle_,
      endpoint_in_,
      (unsigned char*)&(ctrl_buff_.buffer),
      sizeof(XboxInputReport),
      SendViGemWrapper,
      this,
      0
    );
    ret = libusb_submit_transfer(usb_transfer_);
  }
  else
  {
    std::lock_guard<std::mutex> guard(usb_mutex_);
    libusb_fill_control_setup((unsigned char*)&ctrl_buff_,
      LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
      HID_GET_REPORT,
      (HID_REPORT_TYPE_INPUT << 8) | 0x00,
      0,
      sizeof(XboxInputReport)
    );
    libusb_fill_control_transfer(usb_transfer_,
      usb_handle_,
      (unsigned char*)&ctrl_buff_,
      SendViGemWrapper,
      this,
      0
    );
    ret = libusb_submit_transfer(usb_transfer_);
  }

  if (ret < 0) {
    dbgprintf(__FUNCTION__ ": libusb send failed: %s\n", libusb_strerror(ret));
    return false;
  }

  return true;
}

void LIBUSB_CALL XboxController::SendViGemWrapper(struct libusb_transfer* transfer)
{
  XboxController* dev = reinterpret_cast<XboxController*>(transfer->user_data);
  libusb_free_transfer(dev->usb_transfer_);
  dev->usb_transfer_ = NULL;
  dev->SendViGem();
}

// XboxController::sendViGem: callback to take data to vigembus
void XboxController::SendViGem()
{
  XboxInputReport input_prev_ = ctrl_buff_.buffer;
  if (input_prev_.bSize != sizeof(XboxInputReport))
  {
    //dbgprintf(__FUNCTION__ ": controller returned invalid report size %d (expected %d)\n", input_prev_.bSize, sizeof(XboxInputReport));
    return;
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

  // Trigger Deadzone Calculations
  short triggerbuf;
  deadZoneCalc(&triggerbuf, NULL, input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_LEFT_TRIGGER], 0, settings_.deadzone.bLeftTrigger, 0xFF);
  gamepad_.bLeftTrigger = triggerbuf;
  deadZoneCalc(&triggerbuf, NULL, input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_RIGHT_TRIGGER], 0, settings_.deadzone.bRightTrigger, 0xFF);
  gamepad_.bRightTrigger = triggerbuf;

  if (settings_.remap_enabled && settings_.button_remap.size())
  {
    auto buttons = gamepad_.wButtons;
    gamepad_.wButtons = 0;

    auto leftTrig = gamepad_.bLeftTrigger;
    auto rightTrig = gamepad_.bRightTrigger;

    // TODO: could probably change this into a loop over XUSB_BUTTON enum instead of a macro?
#define LoadMap(btn) \
    if (buttons & XUSB_GAMEPAD_##btn) \
    { \
      auto remap = (int)XUSB_GAMEPAD_##btn; \
      if (settings_.button_remap.count(XUSB_GAMEPAD_##btn)) \
        remap = settings_.button_remap[XUSB_GAMEPAD_##btn]; \
      gamepad_.wButtons |= (remap & ~(XUSB_GAMEPAD_LT | XUSB_GAMEPAD_RT)); \
      if (remap & XUSB_GAMEPAD_LT) \
        gamepad_.bLeftTrigger = 255; \
      if (remap & XUSB_GAMEPAD_RT) \
        gamepad_.bRightTrigger = 255; \
    }

    LoadMap(A);
    LoadMap(B);
    LoadMap(X);
    LoadMap(Y);
    LoadMap(Start);
    LoadMap(Back);
    LoadMap(LS);
    LoadMap(RS);
    LoadMap(Black);
    LoadMap(White);
    LoadMap(DpadUp);
    LoadMap(DpadDown);
    LoadMap(DpadLeft);
    LoadMap(DpadRight);
#undef LoadMap

    if (leftTrig >= 8)
    {
      if (settings_.button_remap.count(XUSB_GAMEPAD_LT))
      {
        gamepad_.bLeftTrigger = 0;

        auto remap = settings_.button_remap[XUSB_GAMEPAD_LT];
        gamepad_.wButtons |= (remap & ~(XUSB_GAMEPAD_LT | XUSB_GAMEPAD_RT));

        if (remap & XUSB_GAMEPAD_LT)
          gamepad_.bLeftTrigger = leftTrig;

        if (remap & XUSB_GAMEPAD_RT)
          gamepad_.bRightTrigger = leftTrig;
      }
    }

    if (rightTrig >= 8)
    {
      if (settings_.button_remap.count(XUSB_GAMEPAD_RT))
      {
        gamepad_.bRightTrigger = 0;

        auto remap = settings_.button_remap[XUSB_GAMEPAD_RT];
        gamepad_.wButtons |= (remap & ~(XUSB_GAMEPAD_LT | XUSB_GAMEPAD_RT));

        if (remap & XUSB_GAMEPAD_LT)
          gamepad_.bLeftTrigger = rightTrig;

        if (remap & XUSB_GAMEPAD_RT)
          gamepad_.bRightTrigger = rightTrig;
      }
    }
  }

  // Secret Deadzone Adjustment Combinations: 
  extern bool deadzoneCombinationEnabled; 
  if(deadzoneCombinationEnabled){

    // Analog Stick Deadzone Adjustment: LT + RT + (LS | RS) + D-Pad Up/Down
    if ((input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_LEFT_THUMB) ^ (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_RIGHT_THUMB) && // // (LS XOR RS) AND
    ((input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_LEFT_TRIGGER] >= 0x8) && (input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_RIGHT_TRIGGER] >= 0x8)) &&  // Left and Right Trigger AND
    (input_prev_.Gamepad.wButtons & (OGXINPUT_GAMEPAD_DPAD_UP | OGXINPUT_GAMEPAD_DPAD_DOWN))) // Direction to change deadzone
    {
      // wait for previous deadzone adjustment button release
      if (!settings_.deadzone.hold){
        short adjustment = (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_DPAD_UP ? 500 : -500);

        if (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_LEFT_THUMB){
          settings_.deadzone.sThumbL = min(max(settings_.deadzone.sThumbL+adjustment,0), SHRT_MAX);
        } 
        if (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_RIGHT_THUMB){
          settings_.deadzone.sThumbR = min(max(settings_.deadzone.sThumbR+adjustment,0), SHRT_MAX);
        }

        SaveDeadzones();

        // wait for button release
        settings_.deadzone.hold = true;
      }

    // Trigger Deadzone Adjustment: (LT | RT) + LS + RS + D-Pad Up/Down
    } else if ((input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_LEFT_THUMB) && (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_RIGHT_THUMB) && // // (LS && RS) AND
    ((input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_LEFT_TRIGGER] >= 0x8) ^ (input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_RIGHT_TRIGGER] >= 0x8)) &&  // Left XOR Right Trigger AND
    (input_prev_.Gamepad.wButtons & (OGXINPUT_GAMEPAD_DPAD_UP | OGXINPUT_GAMEPAD_DPAD_DOWN))) // Direction to change deadzone
    {
      if(!settings_.deadzone.hold){
        short adjustment = (input_prev_.Gamepad.wButtons & OGXINPUT_GAMEPAD_DPAD_UP ? 15 : -15);
        
        if (input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_LEFT_TRIGGER]){
          settings_.deadzone.bLeftTrigger = min(max(settings_.deadzone.bLeftTrigger+adjustment,0), 0xFF);
        }
        if (input_prev_.Gamepad.bAnalogButtons[OGXINPUT_GAMEPAD_RIGHT_TRIGGER]){
          settings_.deadzone.bRightTrigger = min(max(settings_.deadzone.bRightTrigger+adjustment,0), 0xFF);
        }
        
        SaveDeadzones();

        // wait for button release
        settings_.deadzone.hold = true;
      }
    } else {
      // reset button release
      settings_.deadzone.hold = false;
    }
  }

  // Analog Stick Deadzone Calculations
  deadZoneCalc(&gamepad_.sThumbLX, &gamepad_.sThumbLY, input_prev_.Gamepad.sThumbLX, input_prev_.Gamepad.sThumbLY, settings_.deadzone.sThumbL, SHRT_MAX);
  deadZoneCalc(&gamepad_.sThumbRX, &gamepad_.sThumbRY, input_prev_.Gamepad.sThumbRX, input_prev_.Gamepad.sThumbRY, settings_.deadzone.sThumbR, SHRT_MAX);

  // Create a 'digital' bitfield so we can test combinations against LT/RT
  int digitalPressed = gamepad_.wButtons;
  if (gamepad_.bLeftTrigger >= 0x8)
    digitalPressed |= XUSB_GAMEPAD_LT;
  if (gamepad_.bRightTrigger >= 0x8)
    digitalPressed |= XUSB_GAMEPAD_RT;

  // Secret guide combination
  if (settings_.guide_enabled && combo_guideButton && (digitalPressed & combo_guideButton) == combo_guideButton)
  {
    gamepad_.wButtons |= XUSB_GAMEPAD_GUIDE;

    // Clear combination from the emulated pad, don't want it to interfere with anything
    gamepad_.wButtons &= ~((USHORT)combo_guideButton);
    if (combo_guideButton & XUSB_GAMEPAD_LT)
      gamepad_.bLeftTrigger = 0;
    if (combo_guideButton & XUSB_GAMEPAD_RT)
      gamepad_.bRightTrigger = 0;
  }

  // Write gamepad to virtual XInput device
  vigem_target_x360_update(vigem, target_, gamepad_);
  memset(&(ctrl_buff_.buffer), 0, sizeof(XboxInputReport));
}

void XboxController::GuideEnabled(bool value)
{
  settings_.guide_enabled = value;
  SetSetting("EnableGuide", value ? "true" : "false", ini_key_);
}

void XboxController::VibrationEnabled(bool value)
{
  settings_.vibration_enabled = value;
  SetSetting("EnableVibration", value ? "true" : "false", ini_key_);
}

void XboxController::RemapEnabled(bool value)
{
  settings_.remap_enabled = value;
  SetSetting("RemapEnable", value ? "true" : "false", ini_key_);
}

void XboxController::SaveDeadzones()
{
  // WritePrivateProfile can only write strings, bleh
  if (settings_.deadzone.sThumbL)
    SetSetting("DeadzoneLeftStick", std::to_string(settings_.deadzone.sThumbL), ini_key_);

  if (settings_.deadzone.sThumbR)
    SetSetting("DeadzoneRightStick", std::to_string(settings_.deadzone.sThumbR), ini_key_);

  if (settings_.deadzone.bLeftTrigger)
    SetSetting("DeadzoneLeftTrigger", std::to_string(settings_.deadzone.bLeftTrigger), ini_key_);

  if (settings_.deadzone.bRightTrigger)
    SetSetting("DeadzoneRightTrigger", std::to_string(settings_.deadzone.bRightTrigger), ini_key_);
}

int XboxController::GetSettingInt(const std::string& setting, int default_val, const std::string& ini_key)
{
  return GetPrivateProfileIntA(ini_key.c_str(), setting.c_str(), default_val, ini_path);
}

std::string XboxController::GetSettingString(const std::string& setting, const std::string& default_val, const std::string& ini_key)
{
  char result[256];

  GetPrivateProfileStringA(ini_key.c_str(), setting.c_str(), default_val.c_str(), result, 256, ini_path);

  return result;
}

bool XboxController::GetSettingBool(const std::string& setting, bool default_val, const std::string& ini_key)
{
  auto res = GetSettingString(setting, default_val ? "true" : "false", ini_key);
  return res == "true" || res == "TRUE" || res == "yes" || res == "YES" || res == "1" || res == "Y";
}

void XboxController::SetSetting(const std::string& setting, const std::string& value, const std::string& ini_key)
{
  WritePrivateProfileStringA(ini_key.c_str(), setting.c_str(), value.c_str(), ini_path);
}


