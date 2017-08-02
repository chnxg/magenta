// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <magenta/assert.h>

#include "dwc3.h"
#include "dwc3-regs.h"
#include "dwc3-types.h"

#include <stdio.h>
#include <string.h>

#define EP_FIFO_SIZE    PAGE_SIZE

// physical endpoint numbers for ep0
#define EPO_OUT             0
#define EPO_IN              1
#define EPO_FIFO            EPO_OUT

#define EPO_MAX_PACKET_SIZE 512

static mx_paddr_t dwc3_ep_trb_phys(dwc3_endpoint_t* ep, dwc3_trb_t* trb) {
    return io_buffer_phys(&ep->fifo_buffer) + ((void *)trb - (void *)ep->fifo_start);
}

static void dwc3_enable_eps(dwc3_t* dwc, uint32_t ep_bits, uint32_t mask) {
    volatile void* reg = dwc3_mmio(dwc) + DALEPENA;
    uint32_t temp = DWC3_READ32(reg);
    temp = (temp & ~mask) | (ep_bits & mask);
    DWC3_WRITE32(reg, temp);
}

mx_status_t dwc3_ep_init(dwc3_t* dwc, unsigned ep_num) {
    MX_DEBUG_ASSERT(ep_num < countof(dwc->eps));
    dwc3_endpoint_t* ep = &dwc->eps[ep_num];
    
    mx_status_t status = io_buffer_init(&ep->fifo_buffer, EP_FIFO_SIZE, IO_BUFFER_RW);
    if (status != MX_OK) {
        return status;
    }

    ep->fifo_start = io_buffer_virt(&ep->fifo_buffer);
    ep->fifo_current = ep->fifo_start;
    ep->fifo_last = (void *)ep->fifo_start + EP_FIFO_SIZE - sizeof(dwc3_trb_t);
    // set up link TRB back to the start of the fifo
    dwc3_trb_t* trb = ep->fifo_last;
    trb->ptr = io_buffer_phys(&ep->fifo_buffer);
    trb->status = 0;
    trb->control = TRB_TRBCTL_LINK | TRB_HWO;
    
    return MX_OK;
}

void dwc3_ep_release(dwc3_t* dwc, unsigned ep_num) {
    MX_DEBUG_ASSERT(ep_num < countof(dwc->eps));
    dwc3_endpoint_t* ep = &dwc->eps[ep_num];

    io_buffer_release(&ep->fifo_buffer);
    memset(ep, 0, sizeof(*ep));
}

mx_status_t dwc3_ep0_enable(dwc3_t* dwc) {
    mx_status_t status;

    // fifo only needed for physical endpoint 0
    if ((status = dwc3_ep_init(dwc, EPO_OUT)) != MX_OK) {
        return status;
    }

    if ((status = dwc3_cmd_start_config(dwc, EPO_OUT, 0)) != MX_OK) {
        return status;
    }

    if ((status = dwc3_cmd_ep_config_init(dwc, EPO_OUT, EPO_FIFO, USB_ENDPOINT_CONTROL,
                                          EPO_MAX_PACKET_SIZE, 0)) != MX_OK ||
        (status = dwc3_cmd_ep_config_init(dwc, EPO_IN, EPO_FIFO, USB_ENDPOINT_CONTROL,
                                          EPO_MAX_PACKET_SIZE, 0)) != MX_OK) {
                                        
        return status;                                     
    }
    if ((status = dwc3_cmd_ep_transfer_config(dwc, EPO_OUT)) != MX_OK ||
        (status = dwc3_cmd_ep_transfer_config(dwc, EPO_IN)) != MX_OK) {
                                        
        return status;                                     
    }

    // need to queue a setup packet first, then issue DEPSTRTXFER
    dwc3_endpoint_t* ep = &dwc->eps[EPO_OUT];
    dwc3_trb_t* trb = ep->fifo_start;
    mx_paddr_t trb_phys =  dwc3_ep_trb_phys(ep, trb);
    trb->ptr = trb_phys;   // point the TRB pointer to the TRB itself
    trb->status = TRB_BUFSIZ(8);
    trb->control = TRB_TRBCTL_SETUP | TRB_LST | TRB_IOC | TRB_HWO;

    dwc3_cmd_ep_start_transfer(dwc, EPO_OUT, trb_phys);

    uint32_t ep_bits = (1 << EPO_OUT) | (1 << EPO_IN);
    dwc3_enable_eps(dwc, ep_bits, ep_bits);

    return MX_OK;
}
