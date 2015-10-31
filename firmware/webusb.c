#include "eeprom.h"
#include "requests.h"
#include "webusb.h"

#include <avr/pgmspace.h>
#include <avr/wdt.h>

#define USB_BOS_DESCRIPTOR_TYPE (15)

#define MS_OS_20_DESCRIPTOR_LENGTH (0x1e)

PROGMEM const uchar BOS_DESCRIPTOR[] = {
  // BOS descriptor header
  0x05, 0x0F, 0x38, 0x00, 0x02,

  // WebUSB Platform Capability descriptor
  0x17, 0x10, 0x05, 0x00,

  0x38, 0xB6, 0x08, 0x34,
  0xA9, 0x09,
  0xA0, 0x47,
  0x8B, 0xFD,
  0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65,
  0x00, 0x01,
  WL_REQUEST_WEBUSB,

  // Microsoft OS 2.0 Platform Capability Descriptor
  // Thanks http://janaxelson.com/files/ms_os_20_descriptors.c
  0x1C,  // Descriptor size (28 bytes)
  0x10,  // Descriptor type (Device Capability)
  0x05,  // Capability type (Platform)
  0x00,  // Reserved

  // MS OS 2.0 Platform Capability ID (D8DD60DF-4589-4CC7-9CD2-659D9E648A9F)
  0xDF, 0x60, 0xDD, 0xD8,
  0x89, 0x45,
  0xC7, 0x4C,
  0x9C, 0xD2,
  0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,

  0x00, 0x00, 0x03, 0x06,    // Windows version (8.1) (0x06030000)
  MS_OS_20_DESCRIPTOR_LENGTH, 0x00,
  WL_REQUEST_WINUSB,         // Vendor-assigned bMS_VendorCode
  0x00                       // Doesn’t support alternate enumeration
};

// Microsoft OS 2.0 Descriptor Set
//
// See https://goo.gl/4T73ef for discussion about bConfigurationValue:
//
// "It looks like we'll need to update the MSOS 2.0 Descriptor docs to
// match the implementation in USBCCGP. The bConfigurationValue in the
// configuration subset header should actually just be an index value,
// not the configuration value. Specifically it's the index value
// passed to GET_DESCRIPTOR to retrieve the configuration descriptor.
// Try changing the value to 0 and see if that resolves the issue.
// Sorry for the confusion."
#define WINUSB_REQUEST_DESCRIPTOR (0x07)
PROGMEM const uchar MS_OS_20_DESCRIPTOR_SET[MS_OS_20_DESCRIPTOR_LENGTH] = {
  // Microsoft OS 2.0 descriptor set header (table 10)
  0x0A, 0x00,  // Descriptor size (10 bytes)
  0x00, 0x00,  // MS OS 2.0 descriptor set header
  0x00, 0x00, 0x03, 0x06,  // Windows version (8.1) (0x06030000)
  MS_OS_20_DESCRIPTOR_LENGTH, 0x00,  // Size, MS OS 2.0 descriptor set

  // Microsoft OS 2.0 compatible ID descriptor (table 13)
  0x14, 0x00,  // wLength
  0x03, 0x00,  // MS_OS_20_FEATURE_COMPATIBLE_ID
  'W',  'I',  'N',  'U',  'S',  'B',  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define WEBUSB_REQUEST_GET_LANDING_PAGE (0x02)
const uchar WEBUSB_LANDING_PAGE[] = {
  0x23, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 's', 'o', 'w', 'b', 'u',
  'g', '.', 'g', 'i', 't', 'h', 'u', 'b', '.', 'i', 'o', '/',
  'w', 'e', 'b', 'l', 'i', 'g', 'h', 't'
};

#define WEBUSB_REQUEST_GET_ALLOWED_ORIGINS (0x01)
const uchar WEBUSB_ALLOWED_ORIGINS[] = {
  0x04, 0x00, 0x35, 0x00, 0x1A, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/',
  's', 'o', 'w', 'b', 'u', 'g', '.', 'g', 'i', 't', 'h', 'u', 'b',
  '.', 'i', 'o', 0x17, 0x03, 'h', 't', 't', 'p', ':', '/', '/',
  'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't', ':', '8', '0', '0', '0'
};

PROGMEM const char usbDescriptorDevice[] = {  // USB device descriptor
  0x12,  // sizeof(usbDescriptorDevice): length of descriptor in bytes
  USBDESCR_DEVICE,        // descriptor type
  0x10, 0x02,             // USB version supported == 2.1
  USB_CFG_DEVICE_CLASS,
  USB_CFG_DEVICE_SUBCLASS,
  0,                      // protocol
  8,                      // max packet size
  // the following two casts affect the first byte of the constant only, but
  // that's sufficient to avoid a warning with the default values.
  (char)USB_CFG_VENDOR_ID,
  (char)USB_CFG_DEVICE_ID,
  USB_CFG_DEVICE_VERSION,
  1,  // manufacturer string index
  2,  // product string index
  3,  // serial number string index
  1,  // number of configurations
};

#define SERIAL_NUMBER_BYTE_COUNT (EEPROM_SERIAL_LENGTH * sizeof(int))
int webUsbDescriptorStringSerialNumber[EEPROM_SERIAL_LENGTH + 1] = {
  USB_STRING_DESCRIPTOR_HEADER(EEPROM_SERIAL_LENGTH)
};

USB_PUBLIC usbMsgLen_t usbFunctionDescriptor(usbRequest_t *rq) {
  switch (rq->wValue.bytes[1]) {
    case USBDESCR_STRING:
      switch (rq->wValue.bytes[0]) {
        case 3:
          usbMsgPtr = (usbMsgPtr_t)(webUsbDescriptorStringSerialNumber);
          return sizeof(webUsbDescriptorStringSerialNumber);
      }
      break;
    case USB_BOS_DESCRIPTOR_TYPE:
      usbMsgPtr = (usbMsgPtr_t)(BOS_DESCRIPTOR);
      return sizeof(BOS_DESCRIPTOR);
    default:
      break;
  }
  return 0;
}

void forceReset() {
  StatusBlink(3);
  wdt_enable(WDTO_15MS);
  for(;;)
    ;
}

static uchar buffer[8];
static uchar currentPosition, bytesRemaining;
const uchar *pmResponsePtr;
uchar pmResponseBytesRemaining;
usbMsgLen_t usbFunctionSetup(uchar data[8]) {
  usbRequest_t    *rq = (void *)data;
  static uchar    dataBuffer[4];

  usbMsgPtr = (int)dataBuffer;
  switch (rq->bRequest) {
  case WL_REQUEST_ECHO:
    dataBuffer[0] = rq->wValue.bytes[0];
    dataBuffer[1] = rq->wValue.bytes[1];
    dataBuffer[2] = rq->wIndex.bytes[0];
    dataBuffer[3] = rq->wIndex.bytes[1];
    return 4;
  case WL_REQUEST_SET_RGB:
    currentPosition = 0;
    bytesRemaining = rq->wLength.word;
    if (bytesRemaining > sizeof(buffer)) {
      bytesRemaining = sizeof(buffer);
    }
    return USB_NO_MSG;
  case WL_REQUEST_SET_LED_COUNT: {
    uint8_t count = rq->wValue.bytes[0];
    SetLEDCount(count);
    WriteLEDCount();
    return count;
  }
  case WL_REQUEST_WEBUSB: {
    switch (rq->wIndex.word) {
    case WEBUSB_REQUEST_GET_ALLOWED_ORIGINS:
      usbMsgPtr = (usbMsgPtr_t)(WEBUSB_ALLOWED_ORIGINS);
      return sizeof(WEBUSB_ALLOWED_ORIGINS);
    case WEBUSB_REQUEST_GET_LANDING_PAGE:
      usbMsgPtr = (usbMsgPtr_t)(WEBUSB_LANDING_PAGE);
      return sizeof(WEBUSB_LANDING_PAGE);
    }
    break;
  }
  case WL_REQUEST_WINUSB: {
    switch (rq->wIndex.word) {
    case WINUSB_REQUEST_DESCRIPTOR:
      pmResponsePtr = MS_OS_20_DESCRIPTOR_SET;
      pmResponseBytesRemaining = sizeof(MS_OS_20_DESCRIPTOR_SET);
      return USB_NO_MSG;
    }
    break;
  }
  case WL_REQUEST_RESET_DEVICE:
    forceReset();
    break;
  }

  return 0;
}

USB_PUBLIC uchar usbFunctionRead(uchar *data, uchar len) {
  if (len > pmResponseBytesRemaining) {
    len = pmResponseBytesRemaining;
  }
  memcpy_P(data, pmResponsePtr, len);
  pmResponsePtr += len;
  pmResponseBytesRemaining -= len;
  return len;
}

USB_PUBLIC uchar usbFunctionWrite(uchar *data, uchar len) {
  uchar i;

  if (len > bytesRemaining) {
    len = bytesRemaining;
  }
  bytesRemaining -= len;
  for (i = 0; i < len; i++) {
    buffer[currentPosition++] = data[i];
  }

  if (bytesRemaining == 0) {
    SetLEDs(buffer[0], buffer[1], buffer[2]);
  }

  return bytesRemaining == 0;             // return 1 if we have all data
}
