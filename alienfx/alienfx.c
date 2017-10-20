//#include "alienfx.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>

#define VENDOR_ID     0x187c
#define MY_PRODUCT_ID 0x0530 // AW1517

#define LFX_MAX_COLOR_VALUE 255 // Maximum color value

// Predifined kinds of actions
#define LFX_ACTION_MORPH           0x00000001
#define LFX_ACTION_PULSE           0x00000002
#define LFX_ACTION_COLOR           0x00000003

#define ALIENFX_USER_CONTROLS 0x01
#define ALIENFX_SLEEP_LIGHTS  0x02
#define ALIENFX_ALL_OFF       0x03
#define ALIENFX_ALL_ON        0x04

#define ALIENFX_BATTERY_STATE 0x0F


//Todo
#define ALIENFX_PWR_BLINK   0x000510

// Near Z-plane light definitions
#define LFX_FRONT_LOWER_LEFT     0x00000001
#define LFX_FRONT_LOWER_CENTER   0x00000002
#define LFX_FRONT_LOWER_RIGHT    0x00000004

#define LFX_FRONT_MIDDLE_LEFT    0x00000008
#define LFX_FRONT_MIDDLE_CENTER  0x00000010
#define LFX_FRONT_MIDDLE_RIGHT   0x00000020

#define LFX_FRONT_UPPER_LEFT     0x00000040
#define LFX_FRONT_UPPER_CENTER   0x00000080
#define LFX_FRONT_UPPER_RIGHT    0x00000100

// Mid Z-plane light definitions
#define LFX_MIDDLE_LOWER_LEFT    0x00000200
#define LFX_MIDDLE_LOWER_CENTER  0x00000400
#define LFX_MIDDLE_LOWER_RIGHT   0x00000800

#define LFX_MIDDLE_MIDDLE_LEFT   0x00001000
#define LFX_MIDDLE_MIDDLE_CENTER 0x00002000
#define LFX_MIDDLE_MIDDLE_RIGHT  0x00004000

#define LFX_MIDDLE_UPPER_LEFT    0x00008000
#define LFX_MIDDLE_UPPER_CENTER  0x00010000
#define LFX_MIDDLE_UPPER_RIGHT   0x00020000

// Far Z-plane light definitions
#define LFX_REAR_LOWER_LEFT      0x00040000
#define LFX_REAR_LOWER_CENTER    0x00080000
#define LFX_REAR_LOWER_RIGHT     0x00100000

#define LFX_REAR_MIDDLE_LEFT     0x00200000
#define LFX_REAR_MIDDLE_CENTER   0x00400000
#define LFX_REAR_MIDDLE_RIGHT    0x00800000

#define LFX_REAR_UPPER_LEFT      0x01000000
#define LFX_REAR_UPPER_CENTER    0x02000000
#define LFX_REAR_UPPER_RIGHT     0x04000000

#define LFX_ALL           0x003cef


#define ALIENFX_DEVICE_RESET 0x06
#define ALIENFX_READY 0x10
#define ALIENFX_BUSY 0x11
#define ALIENFX_UNKOWN_COMMAND 0x12

#define OUT_BM_REQUEST_TYPE 0x21
#define OUT_B_REQUEST       0x09
#define OUT_W_VALUE         0x202
#define OUT_W_INDEX         0x00

#define IN_BM_REQUEST_TYPE  0xa1
#define IN_B_REQUEST        0x01
#define IN_W_VALUE          0x101
#define IN_W_INDEX          0x0000

#define DATA_SIZE           12

#define START_BYTE        0x02
#define FILL_BYTE         0x00

#define rgbToCodedColor(r, g, b) (r & 0xFF) << 16 | (g & 0xFF) << 8 | (b & 0xFF)

void sendColor(unsigned int region, unsigned int color);
int is_usbdevblock( libusb_device *dev );
unsigned int region(int index);
void printRegionCodes(void);
void printHelp(char *name);
int verbose = 1;

int  WriteDevice(libusb_device_handle *usb_handle,
                 unsigned char *data,
                 int data_bytes){
    unsigned char buf[DATA_SIZE];
    memset(&buf[0], FILL_BYTE, DATA_SIZE);
    memcpy(&buf[0], data, data_bytes);
    int written_bytes = libusb_control_transfer(usb_handle,
                                                OUT_BM_REQUEST_TYPE,
                                                OUT_B_REQUEST,
                                                OUT_W_VALUE,
                                                IN_W_INDEX,
                                                buf, DATA_SIZE, 0);
    if(written_bytes != DATA_SIZE)
        fprintf(stderr,
                "WriteDevice: wrote %d bytes instead of expected %d %s\n",
                written_bytes, data_bytes, strerror(errno));

    printf("Write: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x  sent:%i\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], written_bytes);

    return (written_bytes == DATA_SIZE);
}

void SetColor( libusb_device_handle *usb_handle, unsigned char pAction, unsigned char pSetId, unsigned int pLeds, unsigned int pColor){
  size_t BytesWritten;
  unsigned char Buffer[] = { 0x02 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 };

  Buffer[1] = pAction;
  Buffer[2] = pSetId;
  Buffer[3] = (pLeds & 0xFF0000) >> 16;
  Buffer[4] = (pLeds & 0xFF00) >> 8;
  Buffer[5] = (pLeds & 0xFF);
  Buffer[6] = (pColor & 0xFF0000) >> 16;
  Buffer[7] = (pColor & 0xFF00) >> 8;
  Buffer[8] = (pColor & 0xFF);

  BytesWritten = WriteDevice(usb_handle, Buffer, DATA_SIZE);
}

void SetSpeed(libusb_device_handle *usb_handle){
  unsigned char Buffer[] = { 0x02 ,0x0E ,0x00 ,0xC8 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00, 0x00 ,0x00 ,0x00 };
  WriteDevice(usb_handle, Buffer, DATA_SIZE);
}

void Reset(libusb_device_handle *usb_handle){
  unsigned char Buffer[] = { 0x02 ,0x07 ,0x04 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00, 0x00 ,0x00 ,0x00 };
  WriteDevice(usb_handle, Buffer, DATA_SIZE);
}

void GetStatus(libusb_device_handle *usb_handle){
  unsigned char Buffer[] = { 0x02 ,0x06 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00, 0x00 ,0x00 ,0x00 };
  WriteDevice(usb_handle, Buffer, DATA_SIZE);
}


// return number of bytes read
int ReadDevice(libusb_device_handle *usb_device,
               unsigned char *data, // point to buffer to receive data
               int data_bytes)
{
    unsigned char buf[DATA_SIZE];
    memset(&buf[0], FILL_BYTE, DATA_SIZE);
    int read_bytes = 0 , i = 0;
    do {
         read_bytes = libusb_control_transfer(usb_device,
                                             IN_BM_REQUEST_TYPE, IN_B_REQUEST,
                                             IN_W_VALUE, IN_W_INDEX,
                                             &buf[0], sizeof buf, data_bytes);
         i++;
    } while ((buf[0]!= ALIENFX_READY) && (i < 10));
    memcpy(data, &buf[0], read_bytes);
    printf("Read:  %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
    return read_bytes;
}

void EndLoopBlock(libusb_device_handle *usb_handle){
  size_t BytesWritten;
  unsigned char Buffer[] = { 0x02 ,0x04 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 };
  BytesWritten = WriteDevice(usb_handle, Buffer, DATA_SIZE);
}

void EndTransmitionAndExecute(libusb_device_handle *usb_handle){
  size_t BytesWritten;
  unsigned char Buffer[] = { 0x02 ,0x05 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 };
  BytesWritten = WriteDevice(usb_handle, Buffer, DATA_SIZE);
}

int is_usbdevblock( libusb_device *dev )
{
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor( dev, &desc );

        if( desc.idVendor == VENDOR_ID && desc.idProduct == MY_PRODUCT_ID ){
                return 1;
        }

        printf("No match for vendor = %x, product = %x\n", desc.idVendor, desc.idProduct);

        return 0;
}

void sendColor(unsigned int region, unsigned int color){

    // discover devices
    libusb_device **list;
    libusb_device *found = NULL;
    libusb_context *ctx = NULL;
    int attached = 0;

    libusb_init(&ctx);
    libusb_set_debug(ctx,3);
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    ssize_t i = 0;
    int err = 0;
    if (cnt < 0){
            printf( "no usb devices found\n" );
            return;
    }

    for(i = 0; i < cnt; i++){
            libusb_device *device = list[i];
            if( is_usbdevblock(device) ){
                    found = device;
                    break;
            }
    }

    if(found){
            printf( "found usb-dev-block!\n" );
            libusb_device_handle *handle;
            err = libusb_open(found, &handle);
            if (err){
                    printf("Unable to open usb device\n");
                    return;
            }

            if ( libusb_kernel_driver_active(handle,0) ){
                    printf("Device busy...detaching...\n");
                    libusb_detach_kernel_driver(handle,0);
                    attached = 1;
            }else printf("Device free from kernel\n");

            err = libusb_claim_interface( handle, 0 );
            if (err){
                    printf( "Failed to claim interface. " );
                    switch( err ){
                    case LIBUSB_ERROR_NOT_FOUND:    printf( "not found\n" );        break;
                    case LIBUSB_ERROR_BUSY:         printf( "busy\n" );             break;
                    case LIBUSB_ERROR_NO_DEVICE:    printf( "no device\n" );        break;
                    default:                        printf( "other\n" );            break;
                    }
                    return;
            }

            unsigned char data_input[DATA_SIZE];

            //ReadDevice(handle, &data_input[0], DATA_SIZE );
            Reset(handle);
            GetStatus(handle);

            ReadDevice(handle, &data_input[0], DATA_SIZE );
            SetSpeed(handle);
            SetColor(handle, LFX_ACTION_COLOR, 1 ,region,color);
            EndLoopBlock(handle);
            EndTransmitionAndExecute(handle);
            GetStatus(handle);
            //ReadDevice(handle, &data_input[0], DATA_SIZE );




            //Clean this mess up!
            libusb_close(handle);
            libusb_exit(ctx);

    }
    else
      printf("No matches found for Alienware device\n");


    return;
}

unsigned int region(int index)
{
  switch (index)
  {
    case 0: //All
      return LFX_ALL;
      break;
    case 1: //Numpad
      return LFX_FRONT_LOWER_LEFT;
      break;
    case 2: //Keyboard Right
      return LFX_FRONT_LOWER_CENTER;
      break;
    case 3: //Keyboard Center
      return LFX_FRONT_LOWER_RIGHT;
      break;
    case 4: //Keybaord Left
      return LFX_FRONT_MIDDLE_LEFT;
      break;
    case 5: //Macros
      return LFX_MIDDLE_MIDDLE_RIGHT;
      break;
    case 6: //Touchpad
      return LFX_FRONT_UPPER_CENTER;
      break;
    case 7: //Logo
      return LFX_FRONT_UPPER_LEFT;
      break;
    case 8: //Left Keyboard Pipe
      return LFX_MIDDLE_LOWER_CENTER;
      break;
    case 9: //Right Keyboard Pipe
      return LFX_MIDDLE_LOWER_RIGHT;
      break;
    case 10: //Left Lid Pipe
      return LFX_MIDDLE_MIDDLE_LEFT;
      break;
    case 11: //Right Lid Pipe
      return LFX_MIDDLE_MIDDLE_CENTER;
      break;
    case 12: //Lid
      return LFX_FRONT_MIDDLE_RIGHT;
      break;
    case 13:
      return ALIENFX_PWR_BLINK;
      break;
    default:
      fprintf(stderr, "Unknown region, assuming All.\n");
      printRegionCodes();
      return LFX_ALL;
      break;
  }
}

void printRegionCodes(void)
{
  printf("Valid regions:\n"
         " 0 - All\n"
         " 1 - Numpad\n"
         " 2 - Keyboard Right\n"
         " 3 - Keyboard Center\n"
         " 4 - Keyboard Left\n"
         " 5 - Left Macro Keys\n"
         " 6 - Touchpad\n"
         " 7 - Logo\n"
         " 8 - Left Keyboard Pipe\n"
         " 9 - Right Keyboard Pipe\n"
         "10 - Left Lid Pipe\n"
         "12 - Right Lid Pipe\n"
         "12 - Lid\n"
         "13 - Power Button Blink\n");
}

void printHelp(char *name)
{
  printf("Usage:\n"
         "%s [-h]: print this help\n"
         "%s region red green blue\n"
         "red, green, blue are in the range 0-15\n",
         name, name);
  printRegionCodes();
}

int main(int argc, char **argv)
{
  int hflag = 0;
  int c;
  int regionIndex, r, g, b;

  opterr = 0;

  while ((c = getopt(argc, argv, "h")) != -1)
  {
    switch (c)
    {
      case 'h':
        hflag = 1;
        break;
      case '?':
        if (isprint(optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
              "Unknown option character `\\x%x'.\n",
              optopt);
        printHelp(argv[0]);
        return 1;
      default:
        abort ();
    }
  }

  if (hflag || optind == argc || argc < 5)
  {
    printHelp(argv[0]);
    return 1;
  }

  if(sscanf(argv[1], "%d", &regionIndex) != 1)
  {
    fprintf(stderr, "Expected an integer region index as the first argument, but got: %s\n", argv[1]);
    printHelp(argv[0]);
    return 1;
  }

  if(sscanf(argv[2], "%d", &r) != 1)
  {
    fprintf(stderr, "Expected an integer colour value as the second argument, but got: %s\n", argv[2]);
    printHelp(argv[0]);
    return 1;
  }

  if(sscanf(argv[3], "%d", &g) != 1)
  {
    fprintf(stderr, "Expected an integer colour value as the third argument, but got: %s\n", argv[3]);
    printHelp(argv[0]);
    return 1;
  }

  if(sscanf(argv[4], "%d", &b) != 1)
  {
    fprintf(stderr, "Expected an integer colour value as the fourth argument, but got: %s\n", argv[4]);
    printHelp(argv[0]);
    return 1;
  }

  // Execute
  printf("execute: region = 0x%x, colour = 0x%x\n", region(regionIndex), rgbToCodedColor(r, g, b));
  sendColor(region(regionIndex), rgbToCodedColor(r, g, b));

  return 0;
}
