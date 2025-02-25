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
	"Rasp Pico Logic"
};

static const char *patterns[] = {
	"None",
	"Square",
	"Count",
	"Random",
};

static const uint32_t scanopts[] = {
	SR_CONF_NUM_LOGIC_CHANNELS
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST ,
	SR_CONF_TRIGGER_SOURCE  | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
};

static const uint64_t samplerates[] = {
	SR_HZ(1),
	SR_MHZ(125),
	SR_HZ(1),
};

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;
	struct sr_channel *ch;
	struct sr_channel_group *cg;
	int i;
	char channel_name[16];

	int num_logic_channels;
	num_logic_channels=8;
	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_err("Couldn't get IDN response.");
		goto fail;
	}

	if (std_str_idx_s(hw_info->manufacturer, ARRAY_AND_SIZE(manufacturers)) < 0)
	{
		sr_err("couldn't get info");
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
	devc->pattern = 0;
	sdi->priv = devc;

	return sdi;

fail:
	sr_scpi_hw_info_free(hw_info);
	sr_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}


static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	return sr_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	//GSList *l;
	
	if (sr_scpi_open(sdi->conn) != SR_OK)
		return SR_ERR;

/*	
	for (l = sdi->channels; l; l = l->next)
	{
		sr_dbg("channel 0x%08x",l);
	}
*/
	return SR_OK;

}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	(void)cg;
	
	sr_dbg("config_get %d",key);
	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (key) {
	case SR_CONF_SAMPLERATE:
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
	case SR_CONF_PATTERN_MODE:
		*data = g_variant_new_string(patterns[devc->pattern]);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;

}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const char *stropt;
	
	(void)cg;

	devc = sdi->priv;
	sr_dbg("config_set %d", key);
	
	switch (key) {
	case SR_CONF_SAMPLERATE:
		devc->cur_samplerate = g_variant_get_uint64(data);
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
	case SR_CONF_PATTERN_MODE:
		stropt = g_variant_get_string(data, NULL);
		for(int i=0;i<4;i++)
		{
			sr_dbg("Trying %d %s == %s",i, stropt,patterns[i]);
			if(strcmp(stropt,patterns[i]) == 0)
			{
				devc->pattern = i;
				sr_dbg("Found %d",i);
				break;
			}
		}
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	(void)cg;
	
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
		case SR_CONF_PATTERN_MODE:
			*data = g_variant_new_strv(ARRAY_AND_SIZE(patterns));
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
	GSList *l;
	GSList *l2;
	struct sr_trigger *trigger;
	struct sr_trigger_stage *stage;
	struct sr_trigger_match * match;
	uint8_t trg_ch=0;
	uint8_t trg_type=0;
	
	/* Setup triggers */
	if ((trigger = sr_session_trigger_get(sdi->session))) 
	{
		for (l = trigger->stages; l; l = l->next)
		{
			stage=l->data;
			for (l2 = stage->matches; l2; l2 = l2->next)
			{
				match = l2->data;
				sr_dbg("trigger name %s %s %d %d", trigger->name, match->channel->name, match->channel->index, match->match);
				trg_ch = match->channel->index;
				trg_type = match->match;
			}
		}
	}
	
	devc = sdi->priv;
	scpi = sdi->conn;
	
	for (l = sdi->channels; l; l = l->next)
	{
		ch = l->data;
		sr_dbg("Channel %s %d",ch->name, ch->enabled);
	}
	ch = 0; //devc->current_channel->data;

	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 50, rasp_pico_logic_receive_data, (void *)sdi);

	std_session_send_df_header(sdi);

	sr_scpi_send(sdi->conn, "L:PAT %d", devc->pattern);
		
	sr_scpi_send(sdi->conn, "rate %d", devc->cur_samplerate);
	sr_scpi_send(sdi->conn, "trig %d %d", trg_ch, trg_type);
	return sr_scpi_send(sdi->conn, "L:CAPTURE %d", devc->limit_samples);
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	//struct dev_context *devc = sdi->priv;
	struct sr_scpi_dev_inst *scpi;

	sr_dbg("dev_acquisition_stop");

	std_session_send_df_end(sdi);

	/*devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	*/
	scpi = sdi->conn;
	sr_scpi_source_remove(sdi->session, scpi);

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
