# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Xilinx network device drivers.
#

ll_temac-objs := ll_temac_main.o ll_temac_mdio.o
obj-$(CONFIG_XILINX_LL_TEMAC) += ll_temac.o
obj-$(CONFIG_XILINX_EMACLITE) += xilinx_emaclite.o
obj-$(CONFIG_XILINX_TSN) += xilinx_tsn_ep.o xilinx_tsn_ip.o
obj-$(CONFIG_XILINX_TSN_PTP) += xilinx_tsn_ptp_xmit.o xilinx_tsn_ptp_clock.o
obj-$(CONFIG_XILINX_TSN_QBV) += xilinx_tsn_shaper.o
obj-$(CONFIG_XILINX_TSN_QCI) += xilinx_tsn_qci.o
obj-$(CONFIG_XILINX_TSN_CB) += xilinx_tsn_cb.o
obj-$(CONFIG_XILINX_TSN_SWITCH) += xilinx_tsn_switch.o
xilinx_emac-objs := xilinx_axienet_main.o xilinx_axienet_mdio.o xilinx_axienet_dma.o
obj-$(CONFIG_XILINX_AXI_EMAC) += xilinx_emac.o
obj-$(CONFIG_XILINX_TSN_QBR) += xilinx_tsn_preemption.o
obj-$(CONFIG_AXIENET_HAS_MCDMA) += xilinx_axienet_mcdma.o
