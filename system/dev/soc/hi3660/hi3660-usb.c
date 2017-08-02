// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <hw/reg.h>
#include <stdio.h>

#include "hi3660-bus.h"
#include "hi3660-regs.h"

//volatile void* xxx_usb3otg_bc;

#if 0
void
HiKey960UsbPhyCrWaitAck (
  void
  )
{
  int32_t                Timeout = 1000;
  uint32_t               Data;

  while (1) {
    Data = readl(xxx_usb3otg_bc + USB3OTG_PHY_CR_STS);
    if ((Data & USB3OTG_PHY_CR_ACK) == USB3OTG_PHY_CR_ACK) {
      return;
    }
    mx_nanosleep(mx_deadline_after(MX_USEC(50)));
    if (Timeout-- <= 0) {
      printf("Wait PHY_CR_ACK timeout!\n");
      return;
    }
  }
}

void
HiKey960UsbPhyCrSetAddr (
  uint32_t            Addr
  )
{
  // set addr
  writel(USB3OTG_PHY_CR_DATA_IN (Addr), xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  mx_nanosleep(mx_deadline_after(MX_USEC(100)));
  // cap addr
  uint32_t temp = readl(xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  temp |= USB3OTG_PHY_CR_CAP_ADDR;
  writel(temp, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  HiKey960UsbPhyCrWaitAck ();
  writel(0, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
}

uint16_t
HiKey960UsbPhyCrRead (
  uint32_t            Addr
  )
{
  int32_t                Timeout = 1000;
  uint32_t               Data;

  HiKey960UsbPhyCrSetAddr (Addr);
  writel(USB3OTG_PHY_CR_READ, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
    mx_nanosleep(mx_deadline_after(MX_USEC(100)));

  while (1) {
    Data = readl(xxx_usb3otg_bc + USB3OTG_PHY_CR_STS);
    if ((Data & USB3OTG_PHY_CR_ACK) == USB3OTG_PHY_CR_ACK) {
      printf("Addr 0x%x, Data Out:0x%x\n",
        Addr,
        USB3OTG_PHY_CR_DATA_OUT(Data)
        );
      break;
    }
    mx_nanosleep(mx_deadline_after(MX_USEC(50)));
    if (Timeout-- <= 0) {
      printf("Wait PHY_CR_ACK timeout!\n");
      break;
    }
  }
  writel(0, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  return (uint16_t)USB3OTG_PHY_CR_DATA_OUT(Data);
}

void
HiKey960UsbPhyCrWrite (
  uint32_t            Addr,
  uint32_t            Value
  )
{
  uint32_t               Data;

  HiKey960UsbPhyCrSetAddr (Addr);
  Data = USB3OTG_PHY_CR_DATA_IN(Value);
  writel(Data, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);

  Data = readl(xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  Data |= USB3OTG_PHY_CR_CAP_DATA;
  writel(Data, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  HiKey960UsbPhyCrWaitAck ();

  writel(0, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  writel(USB3OTG_PHY_CR_WRITE, xxx_usb3otg_bc + USB3OTG_PHY_CR_CTRL);
  HiKey960UsbPhyCrWaitAck ();
}

#endif

static void phy_cr_wait_ack(volatile void *otg_bc_base)
{
	int i = 1000;

	while (1) {
		if ((readl(otg_bc_base + USB3OTG_PHY_CR_STS) & USB3OTG_PHY_CR_ACK) == 1)
			break;
        mx_nanosleep(mx_deadline_after(MX_USEC(50)));
		if (i-- < 0) {
			printf("wait phy_cr_ack timeout!\n");
			break;
		}
	}
}

static void phy_cr_set_addr(volatile void *otg_bc_base, uint32_t addr)
{
	uint32_t reg;

	/* set addr */
	reg = USB3OTG_PHY_CR_DATA_IN(addr);
	writel(reg, otg_bc_base + USB3OTG_PHY_CR_CTRL);

    mx_nanosleep(mx_deadline_after(MX_USEC(100)));

	/* cap addr */
	reg = readl(otg_bc_base + USB3OTG_PHY_CR_CTRL);
	reg |= USB3OTG_PHY_CR_CAP_ADDR;
	writel(reg, otg_bc_base + USB3OTG_PHY_CR_CTRL);

	phy_cr_wait_ack(otg_bc_base);

	/* clear ctrl reg */
	writel(0, otg_bc_base + USB3OTG_PHY_CR_CTRL);
}

static uint16_t phy_cr_read(volatile void *otg_bc_base, uint32_t addr)
{
	uint32_t reg;
	int i = 1000;

	phy_cr_set_addr(otg_bc_base, addr);

	/* read cap */
	writel(USB3OTG_PHY_CR_READ, otg_bc_base + USB3OTG_PHY_CR_CTRL);

    mx_nanosleep(mx_deadline_after(MX_USEC(100)));

	while (1) {
		reg = readl(otg_bc_base + USB3OTG_PHY_CR_STS);
		if ((reg & USB3OTG_PHY_CR_ACK) == 1) {
			break;
		}
    mx_nanosleep(mx_deadline_after(MX_USEC(50)));
		if (i-- < 0) {
			printf("wait phy_cr_ack timeout!\n");
			break;
		}
	}

	/* clear ctrl reg */
	writel(0, otg_bc_base + USB3OTG_PHY_CR_CTRL);

	return (uint16_t)USB3OTG_PHY_CR_DATA_OUT(reg);
}

static void phy_cr_write(volatile void *otg_bc_base, uint32_t addr, uint32_t value)
{
	uint32_t reg;

	phy_cr_set_addr(otg_bc_base, addr);

	reg = USB3OTG_PHY_CR_DATA_IN(value);
	writel(reg, otg_bc_base + USB3OTG_PHY_CR_CTRL);

	/* cap data */
	reg = readl(otg_bc_base + USB3OTG_PHY_CR_CTRL);
	reg |= USB3OTG_PHY_CR_CAP_DATA;
	writel(reg, otg_bc_base + USB3OTG_PHY_CR_CTRL);

	/* wait ack */
	phy_cr_wait_ack(otg_bc_base);

	/* clear ctrl reg */
	writel(0, otg_bc_base + USB3OTG_PHY_CR_CTRL);

	reg = USB3OTG_PHY_CR_WRITE;
	writel(reg, otg_bc_base + USB3OTG_PHY_CR_CTRL);

	/* wait ack */
	phy_cr_wait_ack(otg_bc_base);
}

#define DWC3_PHY_RX_OVRD_IN_HI	0x1006
#define DWC3_PHY_RX_SCOPE_VDCC	0x1026
#define RX_SCOPE_LFPS_EN	(1 << 0)
#define TX_VBOOST_LVL_MASK             7
#define TX_VBOOST_LVL(x)               ((x) & TX_VBOOST_LVL_MASK)

void config_femtophy_param(volatile void* otg_bc_base)
{
	uint32_t reg;

	/* set high speed phy parameter */
	if (0 /* host */ ) {
//		writel(hisi_dwc->eye_diagram_host_param, otg_bc_base + USB3OTG_CTRL4);
//		printf("set hs phy param 0x%x for host\n",
//				readl(otg_bc_base + USB3OTG_CTRL4));
	} else {
		writel(0x01c466e3, otg_bc_base + USB3OTG_CTRL4);
		printf("set hs phy param 0x%x for device\n",
				readl(otg_bc_base + USB3OTG_CTRL4));
	}

	/* set usb3 phy cr config for usb3.0 */

	if (0 /*hisi_dwc->host_flag*/) {
//		phy_cr_write(otg_bc_base, DWC3_PHY_RX_OVRD_IN_HI,
//				hisi_dwc->usb3_phy_host_cr_param);
	} else {
		phy_cr_write(otg_bc_base, DWC3_PHY_RX_OVRD_IN_HI,
				0xb80 /*hisi_dwc->usb3_phy_cr_param*/);
	}

	printf("set ss phy rx equalization 0x%x\n",
			phy_cr_read(otg_bc_base, DWC3_PHY_RX_OVRD_IN_HI));

	/* enable RX_SCOPE_LFPS_EN for usb3.0 */
	reg = phy_cr_read(otg_bc_base, DWC3_PHY_RX_SCOPE_VDCC);
	reg |= RX_SCOPE_LFPS_EN;
	phy_cr_write(otg_bc_base, DWC3_PHY_RX_SCOPE_VDCC, reg);

	printf("set ss RX_SCOPE_VDCC 0x%x\n",
			phy_cr_read(otg_bc_base, DWC3_PHY_RX_SCOPE_VDCC));

	reg = readl(otg_bc_base + USB3OTG_CTRL6);
	reg &= ~TX_VBOOST_LVL_MASK;
	reg |= TX_VBOOST_LVL(0x5 /*hisi_dwc->usb3_phy_tx_vboost_lvl*/);
	writel(reg, otg_bc_base + USB3OTG_CTRL6);
	printf("set ss phy tx vboost lvl 0x%x\n", readl(otg_bc_base + USB3OTG_CTRL6));
}

mx_status_t hi3360_usb_init(hi3660_bus_t* bus) {
printf("hi3360_usb_init\n");
    volatile void* usb3otg_bc = bus->usb3otg_bc.vaddr;
    volatile void* peri_crg = bus->peri_crg.vaddr;
    volatile void* pctrl = bus->pctrl.vaddr;

// xxx_usb3otg_bc = usb3otg_bc;

    writel(PERRSTEN4_USB3OTG, peri_crg + PERI_CRG_PERRSTEN4);
    writel(PERRSTEN4_USB3OTGPHY_POR, peri_crg + PERI_CRG_PERRSTEN4);
    writel(PERRSTEN4_USB3OTG_MUX | PERRSTEN4_USB3OTG_AHBIF | PERRSTEN4_USB3OTG_32K, peri_crg + PERI_CRG_PERRSTEN4);
    writel(PEREN4_GT_ACLK_USB3OTG | PEREN4_GT_CLK_USB3OTG_REF, peri_crg + PERI_CRG_PERDIS4);

    uint32_t temp = readl(pctrl + PCTRL_CTRL24);
    temp &= PCTRL_CTRL24_USB3PHY_3MUX1_SEL;
    writel(temp, pctrl + PCTRL_CTRL24);
    writel((PCTRL_CTRL3_USB_TXCO_EN << 16) | 0, pctrl + PCTRL_CTRL3);

    mx_nanosleep(mx_deadline_after(MX_MSEC(10)));

// release part
    mx_nanosleep(mx_deadline_after(MX_MSEC(10)));

   /* enable USB REFCLK ISO */
    writel(PERISOEN_USB_REFCLK_ISO_EN, peri_crg + PERI_CRG_ISODIS);
 
  /* enable USB_TXCO_EN */
    writel((PCTRL_CTRL3_USB_TXCO_EN << 16) | PCTRL_CTRL3_USB_TXCO_EN, pctrl + PCTRL_CTRL3);

    writel(~PCTRL_CTRL24_USB3PHY_3MUX1_SEL, pctrl + PCTRL_CTRL24);
    writel(PEREN4_GT_ACLK_USB3OTG | PEREN4_GT_CLK_USB3OTG_REF, peri_crg + PERI_CRG_PEREN4);
    writel(PERRSTEN4_USB3OTG_MUX | PERRSTEN4_USB3OTG_AHBIF | PERRSTEN4_USB3OTG_32K, peri_crg + PERI_CRG_PERRSTDIS4);

    writel(PERRSTEN4_USB3OTG | PERRSTEN4_USB3OTGPHY_POR, peri_crg + PERI_CRG_PERRSTEN4);


  /* enable PHY REF CLK */
    temp = readl(usb3otg_bc + USB3OTG_CTRL0);
    writel(temp | USB3OTG_CTRL0_SC_USB3PHY_ABB_GT_EN, usb3otg_bc + USB3OTG_CTRL0);

    temp = readl(usb3otg_bc + USB3OTG_CTRL7);
    writel(temp | USB3OTG_CTRL7_REF_SSP_EN, usb3otg_bc + USB3OTG_CTRL7);

  /* exit from IDDQ mode */
    temp = readl(usb3otg_bc + USB3OTG_CTRL2);
    writel(temp & ~(USB3OTG_CTRL2_TEST_POWERDOWN_SSP | USB3OTG_CTRL2_TEST_POWERDOWN_HSP), usb3otg_bc + USB3OTG_CTRL2);

    mx_nanosleep(mx_deadline_after(MX_MSEC(10)));

    writel(PERRSTEN4_USB3OTGPHY_POR, peri_crg + PERI_CRG_PERRSTDIS4);
    writel(PERRSTEN4_USB3OTG, peri_crg + PERI_CRG_PERRSTDIS4);

    mx_nanosleep(mx_deadline_after(MX_MSEC(10)));

    temp = readl(usb3otg_bc + USB3OTG_CTRL3);
    writel(temp | USB3OTG_CTRL3_VBUSVLDEXT | USB3OTG_CTRL3_VBUSVLDEXTSEL, usb3otg_bc + USB3OTG_CTRL7);

    mx_nanosleep(mx_deadline_after(MX_MSEC(10)));

config_femtophy_param(usb3otg_bc);

#if 0
/* set eye diagram for usb 2.0 */
    writel(0x01c466e3, usb3otg_bc + USB3OTG_CTRL4);

#define DWC3_PHY_RX_OVRD_IN_HI         0x1006
#define DWC3_PHY_RX_SCOPE_VDCC         0x1026
#define RX_SCOPE_LFPS_EN               (1 << 0)

/* set eye diagram for usb 3.0 */
HiKey960UsbPhyCrRead (DWC3_PHY_RX_OVRD_IN_HI);
HiKey960UsbPhyCrWrite (DWC3_PHY_RX_OVRD_IN_HI, (1 << 11) | (1 << 9) | (1 << 8) | (1 << 7));
HiKey960UsbPhyCrRead (DWC3_PHY_RX_OVRD_IN_HI);

/* enable RX_SCOPE_LFPS_EN for usb 3.0 */
temp = HiKey960UsbPhyCrRead (DWC3_PHY_RX_SCOPE_VDCC);
temp |= RX_SCOPE_LFPS_EN;
HiKey960UsbPhyCrWrite (DWC3_PHY_RX_SCOPE_VDCC, temp);
HiKey960UsbPhyCrRead (DWC3_PHY_RX_SCOPE_VDCC);

#define TX_VBOOST_LVL_MASK             7
#define TX_VBOOST_LVL(x)               ((x) & TX_VBOOST_LVL_MASK)
#define USB_PHY_TX_VBOOST_LVL          5

    temp = readl(usb3otg_bc + USB3OTG_CTRL6);
    temp &= ~TX_VBOOST_LVL_MASK;
    temp = TX_VBOOST_LVL(USB_PHY_TX_VBOOST_LVL);
    writel(temp, usb3otg_bc + USB3OTG_CTRL6);
#endif
 
 #if 0
    /* enable usb_tcxo_en */
    writel(USB_TCXO_EN | (USB_TCXO_EN << PERI_CTRL3_MSK_START),
            pctrl + PCTRL_PERI_CTRL3);

    /* select usbphy clk from abb */
    uint32_t temp = readl(pctrl + PCTRL_PERI_CTRL24);
    temp &= ~SC_CLK_USB3PHY_3MUX1_SEL;
    writel(temp, pctrl + PCTRL_PERI_CTRL24);

    /* open clk gate */
    writel(GT_CLK_USB3OTG_REF | GT_ACLK_USB3OTG,
            peri_crg + PERI_CRG_CLK_EN4);

#endif
    return MX_OK;
}
