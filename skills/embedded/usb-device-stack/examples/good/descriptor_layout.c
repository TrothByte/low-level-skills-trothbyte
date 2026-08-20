/*
 * descriptor_layout.c (GOOD) — USB descriptor hierarchy as #pragma pack(1) C
 * structs. Compile with -Wall -Wextra -Werror and run: prints the computed
 * lengths (device 18, config 9, interface 9, endpoint 7) and a config
 * wTotalLength derived from the structs. Exits 0.
 *
 * gcc -std=c11 -Wall -Wextra -Werror examples/good/descriptor_layout.c -o out && ./out
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;      /* 0x01 */
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_device_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;      /* 0x02 */
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_config_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;      /* 0x04 */
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} usb_interface_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;      /* 0x05 */
    uint8_t  bEndpointAddress;     /* bit 7 = direction, bits 3:0 = number */
    uint8_t  bmAttributes;         /* bits 0:1 = transfer type */
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_descriptor_t;
#pragma pack(pop)

_Static_assert(sizeof(usb_device_descriptor_t) == 18, "device descriptor must be 18 bytes");
_Static_assert(sizeof(usb_config_descriptor_t) == 9, "config descriptor must be 9 bytes");
_Static_assert(sizeof(usb_interface_descriptor_t) == 9, "interface descriptor must be 9 bytes");
_Static_assert(sizeof(usb_endpoint_descriptor_t) == 7, "endpoint descriptor must be 7 bytes");
_Static_assert(offsetof(usb_device_descriptor_t, bMaxPacketSize0) == 7,
               "bMaxPacketSize0 must sit at offset 7");
_Static_assert(offsetof(usb_config_descriptor_t, wTotalLength) == 2,
               "wTotalLength must sit at offset 2");
_Static_assert(offsetof(usb_endpoint_descriptor_t, wMaxPacketSize) == 4,
               "wMaxPacketSize must sit at offset 4");

int main(void)
{
    usb_device_descriptor_t dev = {0x12, 0x01, 0x0200, 0x00, 0x00, 0x00, 0x40,
                                   0x1234, 0x5678, 0x0100, 0x01, 0x02, 0x03, 0x01};
    usb_config_descriptor_t cfg = {0x09, 0x02, 0x0000, 0x01, 0x01, 0x00, 0x80, 0x32};
    usb_interface_descriptor_t ifc = {0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00};
    usb_endpoint_descriptor_t ep_in  = {0x07, 0x05, 0x81, 0x02, 0x0040, 0x00};
    usb_endpoint_descriptor_t ep_out = {0x07, 0x05, 0x02, 0x02, 0x0040, 0x00};

    uint16_t w_total = (uint16_t)(sizeof(cfg) + sizeof(ifc) + sizeof(ep_in) + sizeof(ep_out));
    cfg.wTotalLength = w_total;

    printf("sizeof device descriptor  : %u (expect 18)\n", (unsigned)sizeof(dev));
    printf("sizeof config descriptor  : %u (expect 9)\n",  (unsigned)sizeof(cfg));
    printf("sizeof interface descriptor: %u (expect 9)\n", (unsigned)sizeof(ifc));
    printf("sizeof endpoint descriptor: %u (expect 7)\n",  (unsigned)sizeof(ep_in));
    printf("config wTotalLength        : %u (expect 32 = 9+9+7+7)\n", (unsigned)w_total);
    printf("EP1 IN address             : 0x%02X (expect 0x81, dir bit 7 set)\n", ep_in.bEndpointAddress);
    printf("EP1 OUT address            : 0x%02X (expect 0x02, no dir bit)\n",   ep_out.bEndpointAddress);
    printf("bmAttributes               : 0x%02X (expect 0x80, bit 7 set)\n",     cfg.bmAttributes);

    if (w_total != 32 || sizeof(dev) != 18 || sizeof(cfg) != 9 ||
        sizeof(ifc) != 9 || sizeof(ep_in) != 7) {
        printf("DESCRIPTOR LAYOUT MISMATCH\n");
        return 1;
    }
    printf("DESCRIPTOR LAYOUT OK\n");
    return 0;
}
