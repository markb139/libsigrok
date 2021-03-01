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
#include <stdlib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include "scpi.h"

static struct sr_dev_driver rasp_pico_logic_driver_info;

static const char *manufacturers[] = {
	"TinyUSB",
};

static const uint32_t scanopts[] = {
	SR_CONF_NUM_LOGIC_CHANNELS,
	SR_CONF_LIMIT_FRAMES,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
	SR_CONF_OSCILLOSCOPE,
};
static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
};
static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
	SR_TRIGGER_EDGE,
};

static const uint64_t samplerates[] = {
	SR_HZ(1),
	SR_KHZ(1),
	SR_KHZ(2),
	SR_KHZ(10),
	SR_KHZ(50),
	SR_KHZ(100),
	SR_KHZ(200),
	SR_KHZ(500),
	SR_MHZ(1),
	SR_MHZ(10),
	SR_MHZ(100),
	SR_HZ(1),
};



static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;
	struct sr_channel *ch;
	struct sr_channel_group *cg, *acg;
	int i;
	char channel_name[16];

	int num_logic_channels;
	num_logic_channels=8;
	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_info("Couldn't get IDN response.");
		goto fail;
	}

	if (std_str_idx_s(hw_info->manufacturer, ARRAY_AND_SIZE(manufacturers)) < 0)
	{
		sr_dbg("couldn't get info");
		goto fail;
	}

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &rasp_pico_logic_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;
	if (num_logic_channels > 0) {
		/* Logic channels, all in one channel group. */
		cg = g_malloc0(sizeof(struct sr_channel_group));
		cg->name = g_strdup("Logic");
		for (i = 0; i < num_logic_channels; i++) {
			sprintf(channel_name, "D%d", i);
			ch = sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, channel_name);
			cg->channels = g_slist_append(cg->channels, ch);
		}
		sdi->channel_groups = g_slist_append(NULL, cg);
	}

	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	devc->cur_samplerate = 1;
	sdi->priv = devc;

	/*if (lecroy_xstream_init_device(sdi) != SR_OK)
		goto fail;
*/
	return sdi;

fail:
	sr_scpi_hw_info_free(hw_info);
	sr_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}


static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	sr_warn("SCAN");
	return sr_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	GSList *l;
	
	sr_warn("OPEN->");
	if (sr_scpi_open(sdi->conn) != SR_OK)
		return SR_ERR;

	for (l = sdi->channels; l; l = l->next)
	{
		sr_dbg("channel 0x%08x",l);
	}

/*	if (lecroy_xstream_state_get(sdi) != SR_OK)
		return SR_ERR;
*/
	sr_warn("OPEN<-");
	return SR_OK;

}

static int dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;
	sr_warn("CLOSE");

	/* TODO: get handle from sdi->conn and close it. */

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
	struct dev_context *devc;
	sr_dbg("config_get 0x%08x",key);
	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (key) {
	case SR_CONF_SAMPLERATE:
		sr_dbg("get sample rate %ld",devc->cur_samplerate);
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
	case SR_CONF_LIMIT_FRAMES:
		*data = g_variant_new_uint64(devc->limit_frames);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;

}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
	struct dev_context *devc;
	sr_dbg("config_set 0x%08x %d",key, data);

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		devc->cur_samplerate = g_variant_get_uint64(data);
		sr_dbg("set sample rate %ld",devc->cur_samplerate);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		devc->limit_msec = 0;
		devc->limit_samples = g_variant_get_uint64(data);
		break;
	case SR_CONF_LIMIT_MSEC:
		devc->limit_msec = g_variant_get_uint64(data);
		devc->limit_samples = 0;
		break;
	case SR_CONF_LIMIT_FRAMES:
		devc->limit_frames = g_variant_get_uint64(data);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;

}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	sr_dbg("config_list 0x%0x", key);
	struct sr_channel *ch;

		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
		case SR_CONF_SAMPLERATE:
			*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
			break;
		case SR_CONF_LIMIT_SAMPLES:
			*data = std_gvar_tuple_u64(1, 200000);
			break;
		case SR_CONF_TRIGGER_MATCH:
			*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
			break;
		default:
			return SR_ERR_NA;
		}
	return SR_OK;

}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;


	sr_dbg("dev_acquisition_start->");

	devc = sdi->priv;
	scpi = sdi->conn;

	ch = 0; //devc->current_channel->data;

	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 50, rasp_pico_logic_receive_data, (void *)sdi);

	std_session_send_df_header(sdi);

	sr_scpi_send(sdi->conn, "rate %d", devc->cur_samplerate);
	return sr_scpi_send(sdi->conn, "L:CAPTURE %d", devc->limit_samples);
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	sr_dbg("dev_acquisition_stop->");

	std_session_send_df_end(sdi);
	sr_dbg("A");
	devc = sdi->priv;

	/*devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	*/
	scpi = sdi->conn;
	sr_scpi_source_remove(sdi->session, scpi);

	sr_dbg("dev_acquisition_stop<-");

	return SR_OK;
}

static struct sr_dev_driver rasp_pico_logic_driver_info = {
	.name = "rasp-pico-logic",
	.longname = "Rasp Pico Logic",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(rasp_pico_logic_driver_info);
