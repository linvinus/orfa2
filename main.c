/*
    ORFA2 - Copyright (C) 2012 Vladimir Ermakov.

    This file is part of ORFA2.

    ORFA2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ORFA2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ch.h"
#include "rhal.h"
#include "chprintf.h"
#include "servo.h"
#include "eterm.h"
#include "rosapp.h"

#include "shell.h"

#include "usb_cdc.h"

#if HAL_USE_SERIAL_USB
/*
 * USB Driver structure.
 */
static SerialUSBDriver SDU1;

/*
 * USB Device Descriptor.
 */
static const uint8_t vcom_device_descriptor_data[18] = {
  USB_DESC_DEVICE       (0x0110,        /* bcdUSB (1.1).                    */
                         0x02,          /* bDeviceClass (CDC).              */
                         0x00,          /* bDeviceSubClass.                 */
                         0x00,          /* bDeviceProtocol.                 */
                         0x40,          /* bMaxPacketSize.                  */
                         0x0483,        /* idVendor (ST).                   */
                         0x5740,        /* idProduct.                       */
                         0x0200,        /* bcdDevice.                       */
                         1,             /* iManufacturer.                   */
                         2,             /* iProduct.                        */
                         3,             /* iSerialNumber.                   */
                         1)             /* bNumConfigurations.              */
};

/*
 * Device Descriptor wrapper.
 */
static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

/* Configuration Descriptor tree for a CDC.*/
static const uint8_t vcom_configuration_descriptor_data[67] = {
  /* Configuration Descriptor.*/
  USB_DESC_CONFIGURATION(67,            /* wTotalLength.                    */
                         0x02,          /* bNumInterfaces.                  */
                         0x01,          /* bConfigurationValue.             */
                         0,             /* iConfiguration.                  */
                         0xC0,          /* bmAttributes (self powered).     */
                         50),           /* bMaxPower (100mA).               */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x00,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x01,          /* bNumEndpoints.                   */
                         0x02,          /* bInterfaceClass (Communications
                                           Interface Class, CDC section
                                           4.2).                            */
                         0x02,          /* bInterfaceSubClass (Abstract
                                         Control Model, CDC section 4.3).   */
                         0x01,          /* bInterfaceProtocol (AT commands,
                                           CDC section 4.4).                */
                         0),            /* iInterface.                      */
  /* Header Functional Descriptor (CDC section 5.2.3).*/
  USB_DESC_BYTE         (5),            /* bLength.                         */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x00),         /* bDescriptorSubtype (Header
                                           Functional Descriptor.           */
  USB_DESC_BCD          (0x0110),       /* bcdCDC.                          */
  /* Call Management Functional Descriptor. */
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x01),         /* bDescriptorSubtype (Call Management
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bmCapabilities (D0+D1).          */
  USB_DESC_BYTE         (0x01),         /* bDataInterface.                  */
  /* ACM Functional Descriptor.*/
  USB_DESC_BYTE         (4),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x02),         /* bDescriptorSubtype (Abstract
                                           Control Management Descriptor).  */
  USB_DESC_BYTE         (0x02),         /* bmCapabilities.                  */
  /* Union Functional Descriptor.*/
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x06),         /* bDescriptorSubtype (Union
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bMasterInterface (Communication
                                           Class Interface).                */
  USB_DESC_BYTE         (0x01),         /* bSlaveInterface0 (Data Class
                                           Interface).                      */
  /* Endpoint 2 Descriptor.*/
  USB_DESC_ENDPOINT     (USB_CDC_INTERRUPT_REQUEST_EP|0x80,
                         0x03,          /* bmAttributes (Interrupt).        */
                         0x0008,        /* wMaxPacketSize.                  */
                         0xFF),         /* bInterval.                       */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x01,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x02,          /* bNumEndpoints.                   */
                         0x0A,          /* bInterfaceClass (Data Class
                                           Interface, CDC section 4.5).     */
                         0x00,          /* bInterfaceSubClass (CDC section
                                           4.6).                            */
                         0x00,          /* bInterfaceProtocol (CDC section
                                           4.7).                            */
                         0x00),         /* iInterface.                      */
  /* Endpoint 3 Descriptor.*/
  USB_DESC_ENDPOINT     (USB_CDC_DATA_AVAILABLE_EP,     /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00),         /* bInterval.                       */
  /* Endpoint 1 Descriptor.*/
  USB_DESC_ENDPOINT     (USB_CDC_DATA_REQUEST_EP|0x80,  /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00)          /* bInterval.                       */
};

/*
 * Configuration Descriptor wrapper.
 */
static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};

/*
 * U.S. English language identifier.
 */
static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  USB_DESC_WORD(0x0409)                 /* wLANGID (U.S. English).          */
};

/*
 * Vendor string.
 */
static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(36),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'O', 0, 'p', 0, 'e', 0, 'n', 0, '-', 0, 'R', 0, 'o', 0, 'b', 0,
  'o', 0, 't', 0, 'i', 0, 'c', 0, 's', 0, '.', 0, 'o', 0, 'r', 0,
  'g', 0
};

/*
 * Device Description string.
 */
static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(58),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'O', 0, 'R', 0, 'F', 0, 'A', 0, '2', 0, ' ', 0, '@', 0, ' ', 0,
  'C', 0, 'h', 0, 'i', 0, 'b', 0, 'i', 0, 'O', 0, 'S', 0, '/', 0,
  'R', 0, 'T', 0, ' ', 0, 'V', 0, 'C', 0, 'O', 0, 'M', 0, ' ', 0,
  'P', 0, 'o', 0, 'r', 0, 't', 0
};

/*
 * Serial Number string.
 */
static const uint8_t vcom_string3[] = {
  USB_DESC_BYTE(12),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  '0' + CH_KERNEL_MAJOR, 0, '.', 0,
  '0' + CH_KERNEL_MINOR, 0, '.', 0,
  '0' + CH_KERNEL_PATCH, 0
};

/*
 * Strings wrappers array.
 */
static const USBDescriptor vcom_strings[] = {
  {sizeof vcom_string0, vcom_string0},
  {sizeof vcom_string1, vcom_string1},
  {sizeof vcom_string2, vcom_string2},
  {sizeof vcom_string3, vcom_string3}
};

/*
 * Handles the GET_DESCRIPTOR callback. All required descriptors must be
 * handled here.
 */
static const USBDescriptor *get_descriptor(USBDriver *usbp,
                                           uint8_t dtype,
                                           uint8_t dindex,
                                           uint16_t lang) {

  (void)usbp;
  (void)lang;
  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex < 4)
      return &vcom_strings[dindex];
  }
  return NULL;
}

/**
 * @brief   IN EP1 state.
 */
static USBInEndpointState ep1instate;

/**
 * @brief   EP1 initialization structure (IN only).
 */
static const USBEndpointConfig ep1config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  sduDataTransmitted,
  NULL,
  0x0040,
  0x0000,
  &ep1instate,
  NULL,
  NULL
};

/**
 * @brief   OUT EP2 state.
 */
USBOutEndpointState ep2outstate;

/**
 * @brief   EP2 initialization structure (IN only).
 */
static const USBEndpointConfig ep2config = {
  USB_EP_MODE_TYPE_INTR,
  NULL,
  sduInterruptTransmitted,
  NULL,
  0x0010,
  0x0000,
  NULL,
  &ep2outstate,
  NULL
};

/**
 * @brief   OUT EP2 state.
 */
USBOutEndpointState ep3outstate;

/**
 * @brief   EP3 initialization structure (OUT only).
 */
static const USBEndpointConfig ep3config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  NULL,
  sduDataReceived,
  0x0000,
  0x0040,
  NULL,
  &ep3outstate,
  NULL
};

/*
 * Handles the USB driver global events.
 */
static void usb_event(USBDriver *usbp, usbevent_t event) {

  switch (event) {
  case USB_EVENT_RESET:
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromIsr();

    /* Enables the endpoints specified into the configuration.
       Note, this callback is invoked from an ISR so I-Class functions
       must be used.*/
    usbInitEndpointI(usbp, USB_CDC_DATA_REQUEST_EP, &ep1config);
    usbInitEndpointI(usbp, USB_CDC_INTERRUPT_REQUEST_EP, &ep2config);
    usbInitEndpointI(usbp, USB_CDC_DATA_AVAILABLE_EP, &ep3config);

    /* Resetting the state of the CDC subsystem.*/
    sduConfigureHookI(usbp);

    chSysUnlockFromIsr();
    return;
  case USB_EVENT_SUSPEND:
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  return;
}

/*
 * Serial over USB driver configuration.
 */
static const SerialUSBConfig serusbcfg = {
  &USBD1,
  {
    usb_event,
    get_descriptor,
    sduRequestsHook,
    NULL
  }
};

#endif /* HAL_USE_SERIAL_USB */

const ShellCommand shell_cmds[] = {
  { "eterm", appEterm },
  { "ros", appRos },
  { NULL, NULL }
};

#define SHELL_WA_SIZE 4096

ShellConfig shell_serial_cfg = {
  .sc_channel = (BaseSequentialStream*)&SD1,
  .sc_commands = shell_cmds
};

static WORKING_AREA(shell_serial_wa, SHELL_WA_SIZE);

#if HAL_USE_SERIAL_USB
ShellConfig shell_usb_cfg = {
  .sc_channel = (BaseSequentialStream*)&SDU1,
  .sc_commands = shell_cmds
};

static WORKING_AREA(shell_usb_wa, SHELL_WA_SIZE);
#endif /* HAL_USE_SERIAL_USB */

/*
 * Application entry point.
 */
int main(void) {
  static Thread *shell_serial_tp = NULL;
  static Thread *shell_usb_tp = NULL;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  rhalInit();
  chSysInit();

  sdStart(&SD1, NULL);

#if HAL_USE_SERIAL_USB
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(100);
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);
  usbConnectBus(serusbcfg.usbp);
#endif /* HAL_USE_SERIAL_USB */

#if HAL_USE_ADC
  adcStart(&ADCD1, NULL);
#endif
  dcmStart(&DCMD1, NULL);

  servoInit();

  shellInit();
  while (!chThdShouldTerminate()) {

#if 1
    if (!shell_serial_tp) {
      shell_serial_tp = shellCreateStatic(&shell_serial_cfg, shell_serial_wa, SHELL_WA_SIZE, NORMALPRIO);
    }
    else if (chThdTerminated(shell_serial_tp)) {
      shell_serial_tp = NULL;
    }
#endif

#if HAL_USE_SERIAL_USB
    if (!shell_usb_tp && SDU1.config->usbp->state == USB_ACTIVE) {
      shell_usb_tp = shellCreateStatic(&shell_usb_cfg, shell_usb_wa, SHELL_WA_SIZE, NORMALPRIO);
    }
    else if (chThdTerminated(shell_usb_tp)) {
      shell_usb_tp = NULL;
    }
#endif /* HAL_USE_SERIAL_USB */

    palTogglePad(GPIOB, GPIOB_LED);
    chThdSleepMilliseconds(500);
  }

  return 0;
}
