/*
 * descriptor_layout.c (BAD) — the same hierarchy with two classic bugs:
 *
 *  1. bEndpointAddress is declared uint16_t (it is a single uint8_t), so the
 *     endpoint struct packs to 8 bytes instead of 7 and every following field
 *     shifts.
 *  2. wTotalLength is computed without the OUT endpoint (26 instead of 32) and
 *     the descriptors are serialized with the endpoints BEFORE the interface.
 *
 * Compiles cleanly so the program can run and print the wrong computed
 * lengths, then detects both bugs and exits 1.
 *
 * gcc -std=c11 -Wall -Wextra -Werror examples/bad/descriptor_layout.c -o out && ./out
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    uint16_t bEndpointAddress;     /* BUG: must be uint8_t (one byte, 0x81) */
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_descriptor_t;
#pragma pack(pop)

int main(void)
{
    usb_device_descriptor_t dev = {0x12, 0x01, 0x0200, 0x00, 0x00, 0x00, 0x40,
                                   0x1234, 0x5678, 0x0100, 0x01, 0x02, 0x03, 0x01};
    usb_config_descriptor_t cfg = {0x09, 0x02, 0x0000, 0x01, 0x01, 0x00, 0x80, 0x32};
    usb_interface_descriptor_t ifc = {0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00};
    usb_endpoint_descriptor_t ep_in  = {0x07, 0x05, 0x0081, 0x02, 0x0040, 0x00};
    usb_endpoint_descriptor_t ep_out = {0x07, 0x05, 0x0002, 0x02, 0x0040, 0x00};

    /* BUG 1: wTotalLength omits the OUT endpoint (counts only cfg + ifc + EP IN). */
    uint16_t declared = (uint16_t)(sizeof(cfg) + sizeof(ifc) + sizeof(ep_in));
    cfg.wTotalLength = declared;

    /* BUG 2: endpoints are serialized before the interface descriptor. */
    uint8_t config_blob[64];
    size_t n = 0;
    memcpy(config_blob + n, &cfg, sizeof(cfg));        n += sizeof(cfg);
    memcpy(config_blob + n, &ep_in, sizeof(ep_in));    n += sizeof(ep_in);
    memcpy(config_blob + n, &ep_out, sizeof(ep_out));  n += sizeof(ep_out);
    memcpy(config_blob + n, &ifc, sizeof(ifc));        n += sizeof(ifc);

    printf("sizeof endpoint descriptor: %u (BUG: 8, should be 7)\n", (unsigned)sizeof(ep_in));
    printf("config wTotalLength        : %u (BUG: 26, should be 32)\n", (unsigned)declared);
    printf("serialized config bytes    : %u (BUG: 34, should be 32)\n", (unsigned)n);
    printf("sizeof device descriptor   : %u (correct)\n", (unsigned)sizeof(dev));

    size_t i = 0;
    int ifc_seen = 0;
    while (i < n) {
        uint8_t len = config_blob[i];
        uint8_t type = config_blob[i + 1];
        if (type == 0x04) ifc_seen = 1;
        if (type == 0x05 && !ifc_seen) {
            printf("ORDERING BUG: endpoint 0x%02X serialized before the interface descriptor\n",
                   (unsigned)config_blob[i + 2]);
            return 1;
        }
        i += len;
    }
    if (declared != 32) {
        printf("LENGTH BUG: wTotalLength %u != 32 (endpoint descriptor not counted)\n",
               (unsigned)declared);
        return 1;
    }
    printf("DESCRIPTOR LAYOUT OK\n");
    return 0;
}
