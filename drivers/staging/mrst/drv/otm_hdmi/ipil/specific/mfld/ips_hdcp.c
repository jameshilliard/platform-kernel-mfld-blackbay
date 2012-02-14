/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#include <linux/types.h>
#include <linux/delay.h>
#include "hdcp_rx_defs.h"
#include "mfld_hdcp_reg.h"
#include "mfld_utils.h"
#include "ipil_hdcp_api.h"
#include "ips_hdcp_api.h"

static void ips_hdcp_capture_an(void);
static bool ips_hdcp_is_hdcp_on(void);
static bool ips_hdcp_is_an_ready(void);
static void ips_hdcp_read_an(uint8_t *an);
static void ips_hdcp_write_rx_ri(uint16_t rx_ri);
static void ips_hdcp_set_config(int val);
static int ips_hdcp_get_config(void);
static bool ips_hdcp_is_encrypting(void);
static uint8_t ips_hdcp_get_repeater_control(void);
static void ips_hdcp_set_repeater_control(int value);
static uint8_t ips_hdcp_get_repeater_status(void);
static int ips_hdcp_repeater_v_match_check(void);
static bool ips_hdcp_repeater_is_busy(void);
static int ips_hdcp_repeater_rdy_for_nxt_data(void);

static uint32_t ips_hdcp_get_status(void)
{
	return hdmi_read32(MDFLD_HDCP_STATUS_REG);
}

static void ips_hdcp_enable_port(bool enable)
{
	uint32_t hdmib_reg = hdmi_read32(MDFLD_HDMIB_CNTRL_REG);
	if (enable)
		hdmib_reg |= MDFLD_HDMIB_HDCP_PORT_SEL;
	else
		hdmib_reg &= ~MDFLD_HDMIB_HDCP_PORT_SEL;
	hdmi_write32(MDFLD_HDMIB_CNTRL_REG, hdmib_reg);
}

static void ips_hdcp_capture_an(void)
{
	hdmi_write32(MDFLD_HDCP_INIT_REG, (uint32_t) jiffies);
	hdmi_write32(MDFLD_HDCP_INIT_REG, (uint32_t) (jiffies >> 1));
	hdmi_write32(MDFLD_HDCP_CONFIG_REG, HDCP_CAPTURE_AN);
}

static bool ips_hdcp_is_hdcp_on(void)
{
	struct ips_hdcp_status_reg_t status;
	status.value = ips_hdcp_get_status();

	if (status.hdcp_on)
		return true;

	return false;
}

static bool ips_hdcp_is_an_ready(void)
{
	struct ips_hdcp_status_reg_t status;
	status.value = ips_hdcp_get_status();

	if (status.an_ready)
		return true;

	return false;
}

static void ips_hdcp_read_an(uint8_t *an)
{
	uint8_t i = 0;
	struct double_word_t temp;
	temp.value = 0;
	temp.low = hdmi_read32(MDFLD_HDCP_AN_LOW_REG);
	temp.high = hdmi_read32(MDFLD_HDCP_AN_HI_REG);
	for (i = 0; i < HDCP_AN_SIZE; i++)
		an[i] = temp.byte[i];
}

static void ips_hdcp_write_rx_ri(uint16_t rx_ri)
{
	hdmi_write32(MDFLD_HDCP_RECEIVER_RI_REG, rx_ri);
}

static void ips_hdcp_set_config(int val)
{
	struct ips_hdcp_config_reg_t config;
	config.value = hdmi_read32(MDFLD_HDCP_CONFIG_REG);
	config.hdcp_config = val;
	hdmi_write32(MDFLD_HDCP_CONFIG_REG, config.value);
}

static int ips_hdcp_get_config(void)
{
	struct ips_hdcp_config_reg_t config;
	config.value = hdmi_read32(MDFLD_HDCP_CONFIG_REG);
	return config.hdcp_config;
}

static bool ips_hdcp_config_is_encrypting(void)
{
	if (ips_hdcp_get_config() == HDCP_AUTHENTICATE_AND_ENCRYPT)
		return true;
	return false;
}

static bool ips_hdcp_is_encrypting(void)
{
	struct ips_hdcp_status_reg_t status;
	status.value = ips_hdcp_get_status();

	if (status.encrypting)
		return true;

	return false;
}

static uint8_t ips_hdcp_get_repeater_control(void)
{
	struct ips_hdcp_repeater_reg_t repeater;
	repeater.value = hdmi_read32(MDFLD_HDCP_REP_REG);
	return repeater.control;
}

static void ips_hdcp_set_repeater_control(int value)
{
	struct ips_hdcp_repeater_reg_t repeater;
	repeater.value = hdmi_read32(MDFLD_HDCP_REP_REG);
	repeater.control = value;
	hdmi_write32(MDFLD_HDCP_REP_REG, repeater.value);
}

static uint8_t ips_hdcp_get_repeater_status(void)
{
	struct ips_hdcp_repeater_reg_t repeater;
	repeater.value = hdmi_read32(MDFLD_HDCP_REP_REG);
	return repeater.status;
}

static int ips_hdcp_repeater_v_match_check(void)
{
	uint8_t status = ips_hdcp_get_repeater_status();
	switch (status) {
	case HDCP_REPEATER_STATUS_COMPLETE_MATCH:
		return 1;
	case HDCP_REPEATER_STATUS_BUSY:
		return -1;
	default:
		return 0;
	}
}

static bool ips_hdcp_repeater_is_busy(void)
{
	uint8_t status = ips_hdcp_get_repeater_status();
	if (status == HDCP_REPEATER_STATUS_BUSY)
		return true;
	return false;
}

static int ips_hdcp_repeater_rdy_for_nxt_data(void)
{
	uint8_t status = ips_hdcp_get_repeater_status();
	if (status == HDCP_REPEATER_STATUS_RDY_NEXT_DATA)
		return true;
	return false;
}

bool ips_hdcp_is_ready(void)
{
	struct ips_hdcp_status_reg_t status;
	status.value = ips_hdcp_get_status();

	if (status.fus_success && status.fus_complete)
		return true;

	return false;
}

void ips_hdcp_off(void)
{
	ips_hdcp_set_config(HDCP_Off);
	msleep(30);
}

void ips_hdcp_get_an(uint8_t *an)
{
	bool ret = false;
	ips_hdcp_off();
	ips_hdcp_capture_an();
	do {
		ret = ips_hdcp_is_an_ready();
	} while  (ret == false);
	ips_hdcp_read_an(an);
}

void ips_hdcp_get_aksv(uint8_t *aksv)
{
	static uint8_t save_aksv[HDCP_KSV_SIZE] = {0, 0, 0, 0, 0};
	static bool aksv_read_once = false;
	uint8_t i = 0;
	struct double_word_t temp;
	if (aksv_read_once == false) {
		temp.value = 0;
		temp.low = hdmi_read32(MDFLD_HDCP_AKSV_LOW_REG);
		temp.high = hdmi_read32(MDFLD_HDCP_AKSV_HI_REG);
		aksv_read_once = true;
		for (i = 0; i < HDCP_KSV_SIZE; i++)
			save_aksv[i] = temp.byte[i];
	}
	for (i = 0; i < HDCP_KSV_SIZE; i++)
		aksv[i] = save_aksv[i];
}

bool ips_hdcp_set_bksv(uint8_t *bksv)
{
	uint8_t i = 0;
	struct double_word_t temp;
	if (bksv == NULL)
		return false;
	temp.value = 0;
	for (i = 0; i < HDCP_KSV_SIZE; i++)
		temp.byte[i] = bksv[i];

	hdmi_write32(MDFLD_HDCP_BKSV_LOW_REG, temp.low);
	hdmi_write32(MDFLD_HDCP_BKSV_HI_REG, temp.high);
	return true;
}

bool ips_hdcp_set_repeater(bool present)
{
	struct ips_hdcp_repeater_reg_t repeater;
	repeater.value = hdmi_read32(MDFLD_HDCP_REP_REG);
	repeater.present = present;
	hdmi_write32(MDFLD_HDCP_REP_REG, repeater.value);
	return true;
}

bool ips_hdcp_start_authentication(void)
{
	ips_hdcp_enable_port(true);
	ips_hdcp_set_config(HDCP_AUTHENTICATE_AND_ENCRYPT);
	return true;
}

bool ips_hdcp_is_r0_ready(void)
{
	struct ips_hdcp_status_reg_t status;
	status.value = ips_hdcp_get_status();

	if (status.ri_ready) {
		/* Set Ri to 0 */
		ips_hdcp_write_rx_ri(0);
		/* Set Repeater to Not Present */
		ips_hdcp_set_repeater(0);
		return true;
	}
	return false;
}

bool ips_hdcp_enable_encryption(void)
{
	struct ips_hdcp_status_reg_t status;
	uint32_t hdmib_reg = hdmi_read32(MDFLD_HDMIB_CNTRL_REG);
	status.value = ips_hdcp_get_status();

	if (ips_hdcp_is_hdcp_on() &&
	    ips_hdcp_config_is_encrypting() &&
	    status.ri_match &&
	    (hdmib_reg & MDFLD_HDMIB_HDCP_PORT_SEL))
		return true;
	return false;
}


bool ips_hdcp_does_ri_match(uint16_t rx_ri)
{
	struct ips_hdcp_status_reg_t status;

	ips_hdcp_write_rx_ri(rx_ri);
	status.value = ips_hdcp_get_status();
	if (status.ri_match)
		return true;
	return false;
}

bool ips_hdcp_disable(void)
{
	ips_hdcp_off();
	/* Set Rx_Ri to 0 */
	ips_hdcp_write_rx_ri(0);
	/* Set Repeater to Not Present */
	ips_hdcp_set_repeater(false);
	/* Disable HDCP on this Port */
	/* ips_hdcp_enable_port(false); */
	return true;
}

bool ips_hdcp_init(void)
{
	return true;
}

bool ips_hdcp_device_can_authenticate(void)
{
	return true;
}

