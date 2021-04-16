/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 markb139 <you@example.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"
#include "scpi.h"

SR_PRIV int rasp_pico_logic_receive_data(int fd, int revents, void *cb_data)
{
	GByteArray *data;
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;

	int esr_value;
	gboolean opc_response;

	(void)fd;
	
	data = NULL;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (revents == G_IO_IN) {
		/* TODO */
	}
	
	if(sr_scpi_get_bool(sdi->conn,"*opc?", &opc_response) != SR_OK)
	{
		sr_err("failed to read OPC");
		return SR_ERR;
	}
	if(opc_response)
	{
		if(sr_scpi_get_int(sdi->conn, "*ESR?", &esr_value) != SR_OK) 
		{
			sr_err("failed to read ESR");
			sr_dev_acquisition_stop((struct sr_dev_inst *)sdi);
			return SR_ERR;
		}
		else
		{
			if(esr_value & 0x00000001) 
			{
				sr_dev_acquisition_stop((struct sr_dev_inst *)sdi);
				return SR_ERR;
			}
		}

		if (sr_scpi_get_block(sdi->conn, "DATA?", &data) != SR_OK) 
		{
			if (data)
				g_byte_array_free(data, TRUE);
			return TRUE;
		}

		logic.length=data->len;
		logic.unitsize = 1;
		logic.data = data->data;
		
		devc->sent_samples += data->len;

		packet.payload = &logic;
		packet.type = SR_DF_LOGIC;
		sr_session_send(sdi, &packet);

		g_byte_array_free(data, TRUE);
		if (devc->limit_samples > 0 && devc->sent_samples >= devc->limit_samples)
		{
			sr_dev_acquisition_stop((struct sr_dev_inst *)sdi);
		}
		else
		{
			sr_scpi_send(sdi->conn, "L:CAPTURE %d", devc->limit_samples - devc->sent_samples);
		}
	}
	return G_SOURCE_CONTINUE;
}
