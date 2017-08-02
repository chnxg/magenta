// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/protocol/platform-devices.h>
#include <hw/reg.h>
#include <pretty/hexdump.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dwc3.h"
#include "dwc3-regs.h"
#include "dwc3-types.h"

// MMIO indices
enum {
    MMIO_USB3OTG,
};

// IRQ indices
enum {
    IRQ_USB3,
};

void dwc3_wait_bits(volatile uint32_t* ptr, uint32_t bits, uint32_t expected) {
    uint32_t value = DWC3_READ32(ptr);
    while ((value & bits) != expected) {
        usleep(1000);
        value = DWC3_READ32(ptr);
    }
}

static mx_status_t dwc3_start(dwc3_t* dwc) {
printf("dwc3_start\n");
    volatile void* mmio = dwc3_mmio(dwc);

    uint32_t temp = DWC3_READ32(mmio + DCTL);
    temp &= ~DCTL_RUN_STOP;
    temp |= DCTL_CSFTRST;
    DWC3_WRITE32(mmio + DCTL, temp);
    dwc3_wait_bits(mmio + DCTL, DCTL_CSFTRST, 0);

#if 0
    // set device mode
    uint32_t temp = DWC3_READ32(mmio + GCTL);
    temp &= ~GCTL_PRTCAPDIR_MASK;
    temp |= GCTL_PRTCAPDIR_HOST;
    DWC3_WRITE32(mmio + GCTL, temp);
#else
    DWC3_WRITE32(mmio + GCTL, GCTL_U2EXIT_LFPS | GCTL_PRTCAPDIR_DEVICE | GCTL_U2RSTECN | GCTL_PWRDNSCALE(2));
#endif

/*
  // set TX FIFO size 
  Addr = TX_FIFO_ADDR;
  DwUsb3SetFifoSize (Addr, RAM_TX0_DEPTH / RAM_WIDTH, FIFO_DIR_TX, 0);
  Addr += RAM_TX0_DEPTH / RAM_WIDTH;
  DwUsb3SetFifoSize (Addr, RAM_TX1_DEPTH / RAM_WIDTH, FIFO_DIR_TX, 1);
  // set RX FIFO size 
  DwUsb3SetFifoSize (RX_FIFO_ADDR, RAM_RX_DEPTH / RAM_WIDTH, FIFO_DIR_RX, 0);

  // init event buf 
  MmioWrite32 (GEVNTADRL(0), (UINT32)(UINTN)gEventBuf);
  MmioWrite32 (GEVNTADRH(0), (UINTN)gEventBuf >> 32);
  MmioWrite32 (GEVNTSIZ(0), DWUSB3_EVENT_BUF_SIZE << 2);
  MmioWrite32 (GEVNTCOUNT(0), 0);
*/

    temp = DWC3_READ32(mmio + DCFG);
    uint32_t nump = 16;
    uint32_t max_speed = DCFG_DEVSPD_SUPER;
    temp &= ~DWC3_MASK(DCFG_NUMP_START, DCFG_NUMP_BITS);
    temp |= nump << DCFG_NUMP_START;
    temp &= ~DWC3_MASK(DCFG_DEVSPD_START, DCFG_DEVSPD_BITS);
    temp |= max_speed << DCFG_DEVSPD_START;
    temp &= ~DWC3_MASK(DCFG_DEVADDR_START, DCFG_DEVADDR_BITS);  // clear address
    DWC3_WRITE32(mmio + DCFG, temp);
    
    // configure and enable PHYs
    temp = DWC3_READ32(mmio + GUSB3PIPECTL(0));
    temp &= ~(GUSB3PIPECTL_DELAYP1TRANS | GUSB3PIPECTL_SUSPENDENABLE);
    temp |= GUSB3PIPECTL_LFPSFILTER | GUSB3PIPECTL_SS_TX_DE_EMPHASIS(1);
    DWC3_WRITE32(mmio + GUSB3PIPECTL(0), temp);

    temp = DWC3_READ32(mmio + GUSB2PHYCFG(0));
    temp &= ~(GUSB2PHYCFG_USBTRDTIM_MASK | GUSB2PHYCFG_SUSPENDUSB20);
    temp |= GUSB2PHYCFG_USBTRDTIM(9);
    DWC3_WRITE32(mmio + GUSB2PHYCFG(0), temp);

    uint32_t enable = DEVTEN_LDMEVTEN | DEVTEN_L1WKUPEVTEN | DEVTEN_STOP_ON_DISCONNECT_EN |
                      DEVTEN_VENDEVTSTRCVDEN | DEVTEN_ERRTICERREVTEN | DEVTEN_L1SUSPEN |
                      DEVTEN_SOFTEVTEN | DEVTEN_U3_L2_SUSP_EN | DEVTEN_HIBERNATION_REQ_EVT_EN |
                      DEVTEN_WKUPEVTEN | DEVTEN_ULSTCNGEN | DEVTEN_CONNECTDONEEVTEN | 
                      DEVTEN_USBRSTEVTEN | DEVTEN_DISSCONNEVTEN;

    DWC3_WRITE32(mmio + DEVTEN, enable);

    dwc3_events_start(dwc);
    
    // start the controller
    DWC3_WRITE32(mmio + DCTL, DCTL_RUN_STOP);

    temp = DWC3_READ32(mmio + DCTL);
    temp |= DCTL_RUN_STOP;
    DWC3_WRITE32(mmio + DCTL, temp);

    printf("global registers after start:\n");
    hexdump((void *)mmio + GSBUSCFG0, 256);
    printf("device registers after start:\n");
    hexdump((void *)mmio + DCFG, 256);

    return MX_OK;
}

static mx_status_t dwc_set_interface(void* ctx, usb_dci_interface_t* dci_intf) {
    dwc3_t* dwc = ctx;
    memcpy(&dwc->dci_intf, dci_intf, sizeof(dwc->dci_intf));
    return MX_OK;
}

static mx_status_t dwc_config_ep(void* ctx, const usb_endpoint_descriptor_t* ep_desc) {
    return MX_OK;
}

static mx_status_t dwc_set_enabled(void* ctx, bool enabled) {
    dwc3_t* dwc = ctx;

    if (enabled) {
        return dwc3_start(dwc);
    } else {
        // TODO(voydanoff) disable
        return MX_OK;
    }
}

usb_dci_protocol_ops_t dwc_dci_protocol = {
    .set_interface = dwc_set_interface,
    .config_ep = dwc_config_ep,
    .set_enabled = dwc_set_enabled,
};

static void dwc3_unbind(void* ctx) {
    dwc3_t* dwc = ctx;

    mx_interrupt_signal(dwc->irq_handle);
    thrd_join(dwc->irq_thread, NULL);
    device_remove(dwc->mxdev);
}

static void dwc3_release(void* ctx) {
    dwc3_t* dwc = ctx;

    for (unsigned i = 0; i < countof(dwc->eps); i++) {
        dwc3_ep_release(dwc, i);
    }
    pdev_mmio_buffer_release(&dwc->mmio);
    mx_handle_close(dwc->irq_handle);
    free(dwc);
}

static mx_protocol_device_t dwc3_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = dwc3_release,
};

static mx_status_t dwc3_bind(void* ctx, mx_device_t* dev, void** cookie) {
    printf("dwc3_bind\n");

    dwc3_t* dwc = calloc(1, sizeof(dwc3_t));
    if (!dwc) {
        return MX_ERR_NO_MEMORY;
    }

    platform_device_protocol_t pdev;
    mx_status_t status = device_get_protocol(dev, MX_PROTOCOL_PLATFORM_DEV, &pdev);
    if (status != MX_OK) {
        goto fail;
    }

    status = pdev_map_mmio_buffer(&pdev, MMIO_USB3OTG, MX_CACHE_POLICY_UNCACHED_DEVICE,
                                  &dwc->mmio);
    if (status != MX_OK) {
        printf("dwc3_bind: pdev_map_mmio_buffer failed\n");
        goto fail;
    }

    status = dwc3_ep0_enable(dwc);
    if (status != MX_OK) {
        printf("dwc3_bind: dwc3_ep0_enable failed\n");
        goto fail;
    }

    status = pdev_map_interrupt(&pdev, IRQ_USB3, &dwc->irq_handle);
    if (status != MX_OK) {
        printf("dwc3_bind: pdev_map_interrupt failed\n");
        goto fail;
    }

    status = dwc3_events_init(dwc);
    if (status != MX_OK) {
        printf("dwc3_bind: dwc3_events_init failed\n");
        goto fail;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "dwc3",
        .ctx = dwc,
        .ops = &dwc3_device_proto,
        .proto_id = MX_PROTOCOL_USB_DCI,
        .proto_ops = &dwc_dci_protocol,
    };

    status = device_add(dev, &args, &dwc->mxdev);
    if (status != MX_OK) {
        goto fail;
    }

    return MX_OK;

fail:
    printf("dwc3_bind failed %d\n", status);
    dwc3_release(dwc);
    return status;
}

static mx_driver_ops_t dwc3_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = dwc3_bind,
};

// The formatter does not play nice with these macros.
// clang-format off
MAGENTA_DRIVER_BEGIN(dwc3, dwc3_driver_ops, "magenta", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_GENERIC),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_GENERIC),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_USB_DWC3),
MAGENTA_DRIVER_END(dwc3)
// clang-format on
