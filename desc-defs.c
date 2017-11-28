/*****************************************************************************/

/*
 *      desc-defs.c  --  USB descriptor definitions
 *
 *      Copyright (C) 2017 Michael Drake <michael.drake@codethink.co.uk>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "desc-defs.h"

static const char * const uac2_interface_header_bmcontrols[] = {
	"Latency control",
	NULL
};

static const char * const uac_feature_unit_bmcontrols[] = {
	"Mute",
	"Volume",
	"Bass",
	"Mid",
	"Treble",
	"Graphic Equalizer",
	"Automatic Gain",
	"Delay",
	"Bass Boost",
	"Loudness",
	"Input gain",
	"Input gain pad",
	"Phase inverter",
	NULL
};

static const char * const uac2_input_term_bmcontrols[] = {
	"Copy Protect",
	"Connector",
	"Overload",
	"Cluster",
	"Underflow",
	"Overflow",
	NULL
};

static const char * const uac2_output_term_bmcontrols[] = {
	"Copy Protect",
	"Connector",
	"Overload",
	"Underflow",
	"Overflow",
	NULL
};

static const char * const uac2_mixer_unit_bmcontrols[] = {
	"Cluster",
	"Underflow",
	"Overflow",
	NULL
};

static const char * const uac2_extension_unit_bmcontrols[] = {
	"Enable",
	"Cluster",
	"Underflow",
	"Overflow",
	NULL
};

static const char * const uac2_clock_source_bmcontrols[] = {
	"Clock Frequency",
	"Clock Validity",
	NULL
};

static const char * const uac2_clock_selector_bmcontrols[] = {
	"Clock Selector",
	NULL
};

static const char * const uac2_clock_multiplier_bmcontrols[] = {
	"Clock Numerator",
	"Clock Denominator",
	NULL
};

static const char * const uac2_selector_bmcontrols[] = {
	"Selector",
	NULL
};

static const char * const uac1_channel_names[] = {
	"Left Front (L)", "Right Front (R)", "Center Front (C)",
	"Low Frequency Enhancement (LFE)", "Left Surround (LS)",
	"Right Surround (RS)", "Left of Center (LC)", "Right of Center (RC)",
	"Surround (S)", "Side Left (SL)", "Side Right (SR)", "Top (T)"
};

static const char * const uac2_channel_names[] = {
	"Front Left (FL)", "Front Right (FR)", "Front Center (FC)",
	"Low Frequency Effects (LFE)", "Back Left (BL)", "Back Right (BR)",
	"Front Left of Center (FLC)", "Front Right of Center (FRC)",
	"Back Center (BC)", "Side Left (SL)", "Side Right (SR)",
	"Top Center (TC)", "Top Front Left (TFL)", "Top Front Center (TFC)",
	"Top Front Right (TFR)", "Top Back Left (TBL)", "Top Back Center (TBC)",
	"Top Back Right (TBR)", "Top Front Left of Center (TFLC)",
	"Top Front Right of Center (TFRC)", "Left Low Frequency Effects (LLFE)",
	"Right Low Frequency Effects (RLFE)", "Top Side Left (TSL)",
	"Top Side Right (TSR)", "Bottom Center (BC)",
	"Back Left of Center (BLC)", "Back Right of Center (BRC)"
};

/** UAC1: 4.3.2 Class-Specific AC Interface Descriptor; Table 4-2. */
static const struct desc desc_audio_1_ac_header[] = {
	{ .field = "bcdADC",        .size = 2, .type = DESC_BCD },
	{ .field = "wTotalLength",  .size = 2, .type = DESC_CONSTANT },
	{ .field = "bInCollection", .size = 1, .type = DESC_CONSTANT },
	{ .field = "baInterfaceNr", .size = 1, .type = DESC_NUMBER,
			.array = { .array = true } },
	{ .field = NULL }
};
/** UAC2: 4.7.2 Class-Specific AC Interface Descriptor; Table 4-5. */
static const struct desc desc_audio_2_ac_header[] = {
	{ .field = "bcdADC",       .size = 2, .type = DESC_BCD },
	{ .field = "bCategory",    .size = 1, .type = DESC_CONSTANT },
	{ .field = "wTotalLength", .size = 2, .type = DESC_NUMBER },
	{ .field = "bmControls",   .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_interface_header_bmcontrols },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_header[3] = {
	desc_audio_1_ac_header,
	desc_audio_2_ac_header,
	NULL, /* UAC3 not implemented yet */
};

/** UAC2: 4.7.2.10 Effect Unit Descriptor; Table 4-15. */
static const struct desc desc_audio_2_ac_effect_unit[] = {
	{ .field = "bUnitID",     .size = 1, .type = DESC_NUMBER },
	{ .field = "wEffectType", .size = 2, .type = DESC_CONSTANT },
	{ .field = "bSourceID",   .size = 1, .type = DESC_CONSTANT },
	{ .field = "bmaControls", .size = 4, .type = DESC_BITMAP,
			.array = { .array = true } },
	{ .field = "iEffects",    .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_effect_unit[3] = {
	NULL, /* UAC1 not supported */
	desc_audio_2_ac_effect_unit,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.1 Input Terminal Descriptor; Table 4-3. */
static const struct desc desc_audio_1_ac_input_terminal[] = {
	{ .field = "bTerminalID",    .size = 1, .type = DESC_NUMBER },
	{ .field = "wTerminalType",  .size = 2, .type = DESC_TERMINAL_STR },
	{ .field = "bAssocTerminal", .size = 1, .type = DESC_CONSTANT },
	{ .field = "bNrChannels",    .size = 1, .type = DESC_NUMBER },
	{ .field = "wChannelConfig", .size = 2, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac1_channel_names, .count = 12 } },
	{ .field = "iChannelNames",  .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "iTerminal",      .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
/** UAC2: 4.7.2.4 Input Terminal Descriptor; Table 4-9. */
static const struct desc desc_audio_2_ac_input_terminal[] = {
	{ .field = "bTerminalID",     .size = 1, .type = DESC_NUMBER },
	{ .field = "wTerminalType",   .size = 2, .type = DESC_TERMINAL_STR },
	{ .field = "bAssocTerminal",  .size = 1, .type = DESC_CONSTANT },
	{ .field = "bCSourceID",      .size = 1, .type = DESC_CONSTANT },
	{ .field = "bNrChannels",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bmChannelConfig", .size = 4, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac2_channel_names, .count = 26 } },
	{ .field = "iChannelNames",   .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bmControls",      .size = 2, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_input_term_bmcontrols },
	{ .field = "iTerminal",       .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_input_terminal[3] = {
	desc_audio_1_ac_input_terminal,
	desc_audio_2_ac_input_terminal,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.2 Output Terminal Descriptor; Table 4-4. */
static const struct desc desc_audio_1_ac_output_terminal[] = {
	{ .field = "bTerminalID",    .size = 1, .type = DESC_NUMBER },
	{ .field = "wTerminalType",  .size = 2, .type = DESC_TERMINAL_STR },
	{ .field = "bAssocTerminal", .size = 1, .type = DESC_NUMBER },
	{ .field = "bSourceID",      .size = 1, .type = DESC_NUMBER },
	{ .field = "iTerminal",      .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
/** UAC2: 4.7.2.5 Output Terminal Descriptor; Table 4-10. */
static const struct desc desc_audio_2_ac_output_terminal[] = {
	{ .field = "bTerminalID",    .size = 1, .type = DESC_NUMBER },
	{ .field = "wTerminalType",  .size = 2, .type = DESC_TERMINAL_STR },
	{ .field = "bAssocTerminal", .size = 1, .type = DESC_NUMBER },
	{ .field = "bSourceID",      .size = 1, .type = DESC_NUMBER },
	{ .field = "bCSourceID",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bmControls",     .size = 2, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_output_term_bmcontrols },
	{ .field = "iTerminal",      .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_output_terminal[3] = {
	desc_audio_1_ac_output_terminal,
	desc_audio_2_ac_output_terminal,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.3 Mixer Unit Descriptor; Table 4-5. */
static const struct desc desc_audio_1_ac_mixer_unit[] = {
	{ .field = "bUnitID",        .size = 1, .type = DESC_NUMBER },
	{ .field = "bNrInPins",      .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID",     .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bNrChannels",    .size = 1, .type = DESC_NUMBER },
	{ .field = "wChannelConfig", .size = 2, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac1_channel_names, .count = 12 } },
	{ .field = "iChannelNames",  .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bmControls",     .size = 1, .type = DESC_BITMAP,
			.array = { .array = true, .bits = true,
					.length_field1 = "bNrInPins",
					.length_field2 = "bNrChannels" } },
	{ .field = "iMixer",         .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
/** UAC2: 4.7.2.6 Mixer Unit Descriptor; Table 4-11. */
static const struct desc desc_audio_2_ac_mixer_unit[] = {
	{ .field = "bUnitID",        .size = 1, .type = DESC_NUMBER },
	{ .field = "bNrInPins",      .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID",     .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bNrChannels",    .size = 1, .type = DESC_NUMBER },
	{ .field = "bmChannelConfig",.size = 4, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac2_channel_names, .count = 26 } },
	{ .field = "iChannelNames",  .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bmMixerControls",.size = 1, .type = DESC_BITMAP,
			.array = { .array = true, .bits = true,
					.length_field1 = "bNrInPins",
					.length_field2 = "bNrChannels" } },
	{ .field = "bmControls",     .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_mixer_unit_bmcontrols },
	{ .field = "iMixer",         .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_mixer_unit[3] = {
	desc_audio_1_ac_mixer_unit,
	desc_audio_2_ac_mixer_unit,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.4 Selector Unit Descriptor; Table 4-6. */
static const struct desc desc_audio_1_ac_selector_unit[] = {
	{ .field = "bUnitID",    .size = 1, .type = DESC_NUMBER },
	{ .field = "bNrInPins",  .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID", .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "iSelector",  .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
/** UAC2: 4.7.2.7 Selector Unit Descriptor; Table 4-12. */
static const struct desc desc_audio_2_ac_selector_unit[] = {
	{ .field = "bUnitID",    .size = 1, .type = DESC_NUMBER },
	{ .field = "bNrInPins",  .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID", .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bmControls", .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_selector_bmcontrols },
	{ .field = "iSelector",  .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_selector_unit[3] = {
	desc_audio_1_ac_selector_unit,
	desc_audio_2_ac_selector_unit,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.6 Processing Unit Descriptor; Table 4-8. */
static const struct desc desc_audio_1_ac_processing_unit[] = {
	{ .field = "bUnitID",          .size = 1, .type = DESC_NUMBER },
	{ .field = "wProcessType",     .size = 2, .type = DESC_CONSTANT },
	{ .field = "bNrInPins",        .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID",       .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bNrChannels",      .size = 1, .type = DESC_NUMBER },
	{ .field = "wChannelConfig",   .size = 2, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac1_channel_names, .count = 12 } },
	{ .field = "iChannelNames",    .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bControlSize",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bmControls",       .size = 1, .type = DESC_BITMAP,
			.array = { .array = true, .length_field1 = "bControlSize" } },
	{ .field = "iProcessing",      .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "Process-specific", .size = 1, .type = DESC_BITMAP,
			.array = { .array = true } },
	{ .field = NULL }
};
/** UAC2: 4.7.2.11 Processing Unit Descriptor; Table 4-20. */
static const struct desc desc_audio_2_ac_processing_unit[] = {
	{ .field = "bUnitID",          .size = 1, .type = DESC_NUMBER },
	{ .field = "wProcessType",     .size = 2, .type = DESC_CONSTANT },
	{ .field = "bNrInPins",        .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID",       .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bNrChannels",      .size = 1, .type = DESC_NUMBER },
	{ .field = "bmChannelConfig",  .size = 4, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac2_channel_names, .count = 26 } },
	{ .field = "iChannelNames",    .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bControlSize",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bmControls",       .size = 2, .type = DESC_BITMAP,
			.array = { .array = true, .length_field1 = "bControlSize" } },
	{ .field = "iProcessing",      .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "Process-specific", .size = 1, .type = DESC_BITMAP,
			.array = { .array = true } },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_processing_unit[3] = {
	desc_audio_1_ac_processing_unit,
	desc_audio_2_ac_processing_unit,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.5 Feature Unit Descriptor; Table 4-7. */
static const struct desc desc_audio_1_ac_feature_unit[] = {
	{ .field = "bUnitID",      .size = 1,                    .type = DESC_NUMBER },
	{ .field = "bSourceID",    .size = 1,                    .type = DESC_CONSTANT },
	{ .field = "bControlSize", .size = 1,                    .type = DESC_NUMBER },
	{ .field = "bmaControls",  .size_field = "bControlSize", .type = DESC_BMCONTROL_1,
			.bmcontrol = uac_feature_unit_bmcontrols, .array = { .array = true } },
	{ .field = "iFeature",     .size = 1,                    .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
/** UAC2: 4.7.2.8 Feature Unit Descriptor; Table 4-13. */
static const struct desc desc_audio_2_ac_feature_unit[] = {
	{ .field = "bUnitID",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bSourceID",   .size = 1, .type = DESC_CONSTANT },
	{ .field = "bmaControls", .size = 4, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac_feature_unit_bmcontrols, .array = { .array = true } },
	{ .field = "iFeature",    .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_feature_unit[3] = {
	desc_audio_1_ac_feature_unit,
	desc_audio_2_ac_feature_unit,
	NULL, /* UAC3 not implemented yet */
};


/** UAC1: 4.3.2.7 Extension Unit Descriptor; Table 4-15. */
static const struct desc desc_audio_1_ac_extension_unit[] = {
	{ .field = "bUnitID",        .size = 1, .type = DESC_NUMBER },
	{ .field = "wExtensionCode", .size = 2, .type = DESC_CONSTANT },
	{ .field = "bNrInPins",      .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID",     .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bNrChannels",    .size = 1, .type = DESC_NUMBER },
	{ .field = "wChannelConfig", .size = 2, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac1_channel_names, .count = 12 } },
	{ .field = "iChannelNames",  .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bControlSize",   .size = 1, .type = DESC_NUMBER },
	{ .field = "bmControls",     .size = 1, .type = DESC_BITMAP,
			.array = { .array = true, .length_field1 = "bControlSize" } },
	{ .field = "iExtension",     .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
/** UAC2: 4.7.2.12 Extension Unit Descriptor; Table 4-24. */
static const struct desc desc_audio_2_ac_extension_unit[] = {
	{ .field = "bUnitID",         .size = 1, .type = DESC_NUMBER },
	{ .field = "wExtensionCode",  .size = 2, .type = DESC_CONSTANT },
	{ .field = "bNrInPins",       .size = 1, .type = DESC_NUMBER },
	{ .field = "baSourceID",      .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bNrChannels",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bmChannelConfig", .size = 4, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac2_channel_names, .count = 26 } },
	{ .field = "iChannelNames",   .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = "bmControls",      .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_extension_unit_bmcontrols },
	{ .field = "iExtension",      .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_extension_unit[3] = {
	desc_audio_1_ac_extension_unit,
	desc_audio_2_ac_extension_unit,
	NULL, /* UAC3 not implemented yet */
};


static const char * const uac2_clk_src_bmattr[] = {
	"External",
	"Internal fixed",
	"Internal variable",
	"Internal programmable"
};
static const char * const uac3_clk_src_bmattr[] = {
	"External",
	"Internal",
	"(asynchronous)",
	"(synchronized to SOF)"
};

/** Special rendering function for UAC2 clock source bmAttributes */
static void desc_snowflake_dump_uac2_clk_src_bmattr(
		unsigned long long value,
		unsigned int indent)
{
	printf(" %s clock %s\n",
			uac2_clk_src_bmattr[value & 0x3],
			(value & 0x4) ? uac3_clk_src_bmattr[3] : "");
}

/** UAC2: 4.7.2.1 Clock Source Descriptor; Table 4-6. */
static const struct desc desc_audio_2_ac_clock_source[] = {
	{ .field = "bClockID",       .size = 1, .type = DESC_CONSTANT },
	{ .field = "bmAttributes",   .size = 1, .type = DESC_SNOWFLAKE,
			.snowflake = desc_snowflake_dump_uac2_clk_src_bmattr },
	{ .field = "bmControls",     .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_clock_source_bmcontrols },
	{ .field = "bAssocTerminal", .size = 1, .type = DESC_CONSTANT },
	{ .field = "iClockSource",   .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_clock_source[3] = {
	NULL, /* UAC1 not supported */
	desc_audio_2_ac_clock_source,
	NULL, /* UAC3 not implemented yet */
};


/** UAC2: 4.7.2.2 Clock Selector Descriptor; Table 4-7. */
static const struct desc desc_audio_2_ac_clock_selector[] = {
	{ .field = "bClockID",       .size = 1, .type = DESC_NUMBER },
	{ .field = "bNrInPins",      .size = 1, .type = DESC_NUMBER },
	{ .field = "baCSourceID",    .size = 1, .type = DESC_NUMBER,
			.array = { .array = true, .length_field1 = "bNrInPins" } },
	{ .field = "bmControls",     .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_clock_selector_bmcontrols },
	{ .field = "iClockSelector", .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_clock_selector[3] = {
	NULL, /* UAC1 not supported */
	desc_audio_2_ac_clock_selector,
	NULL, /* UAC3 not implemented yet */
};


/** UAC2: 4.7.2.3 Clock Multiplier Descriptor; Table 4-8. */
static const struct desc desc_audio_2_ac_clock_multiplier[] = {
	{ .field = "bClockID",         .size = 1, .type = DESC_CONSTANT },
	{ .field = "bCSourceID",       .size = 1, .type = DESC_NUMBER },
	{ .field = "bmControls",       .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_clock_multiplier_bmcontrols },
	{ .field = "iClockMultiplier", .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_clock_multiplier[3] = {
	NULL, /* UAC1 not supported */
	desc_audio_2_ac_clock_multiplier,
	NULL, /* UAC3 not implemented yet */
};


/** UAC2: 4.7.2.9 Sampling Rate Converter Descriptor; Table 4-14. */
static const struct desc desc_audio_2_ac_sample_rate_converter[] = {
	{ .field = "bUnitID",       .size = 1, .type = DESC_CONSTANT },
	{ .field = "bSourceID",     .size = 1, .type = DESC_CONSTANT },
	{ .field = "bCSourceInID",  .size = 1, .type = DESC_CONSTANT },
	{ .field = "bCSourceOutID", .size = 1, .type = DESC_CONSTANT },
	{ .field = "iSRC",          .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_ac_sample_rate_converter[3] = {
	NULL, /* UAC1 not supported */
	desc_audio_2_ac_sample_rate_converter,
	NULL, /* UAC3 not implemented yet */
};


static const char * const uac2_as_interface_bmcontrols[] = {
	"Active Alternate Setting",
	"Valid Alternate Setting",
	NULL
};
static const char * const audio_data_format_type_i[] = {
	"TYPE_I_UNDEFINED",
	"PCM",
	"PCM8",
	"IEEE_FLOAT",
	"ALAW",
	"MULAW"
};
static const char * const audio_data_format_type_ii[] = {
	"TYPE_II_UNDEFINED",
	"MPEG",
	"AC-3"
};
static const char * const audio_data_format_type_iii[] = {
	"TYPE_III_UNDEFINED",
	"IEC1937_AC-3",
	"IEC1937_MPEG-1_Layer1",
	"IEC1937_MPEG-Layer2/3/NOEXT",
	"IEC1937_MPEG-2_EXT",
	"IEC1937_MPEG-2_Layer1_LS",
	"IEC1937_MPEG-2_Layer2/3_LS"
};

/** Special rendering function for UAC1 AS interface wFormatTag */
static void desc_snowflake_dump_uac1_as_interface_wformattag(
		unsigned long long value,
		unsigned int indent)
{
	const char *format_string = "undefined";

	if (value <= 5) {
		format_string = audio_data_format_type_i[value];

	} else if (value >= 0x1000 && value <= 0x1002) {
		format_string = audio_data_format_type_ii[value & 0xfff];

	} else if (value >= 0x2000 && value <= 0x2006) {
		format_string = audio_data_format_type_iii[value & 0xfff];
	}

	printf(" %s\n", format_string);
}

/** Special rendering function for UAC2 AS interface bmFormats */
static void desc_snowflake_dump_uac2_as_interface_bmformats(
		unsigned long long value,
		unsigned int indent)
{
	unsigned int i;

	printf("\n");
	for (i = 0; i < 5; i++) {
		if ((value >> i) & 0x1) {
			printf("%*s%s\n", indent * 2, "",
					audio_data_format_type_i[i + 1]);
		}
	}

}

/** UAC1: 4.5.2 Class-Specific AS Interface Descriptor; Table 4-19. */
static const struct desc desc_audio_1_as_interface[] = {
	{ .field = "bTerminalLink", .size = 1, .type = DESC_CONSTANT },
	{ .field = "bDelay",        .size = 1, .type = DESC_NUMBER_POSTFIX,
			.number_postfix = " frames" },
	{ .field = "wFormatTag",    .size = 2, .type = DESC_SNOWFLAKE,
			.snowflake = desc_snowflake_dump_uac1_as_interface_wformattag },
	{ .field = NULL }
};
/** UAC2: 4.9.2 Class-Specific AS Interface Descriptor; Table 4-27. */
static const struct desc desc_audio_2_as_interface[] = {
	{ .field = "bTerminalLink",   .size = 1, .type = DESC_NUMBER },
	{ .field = "bmControls",      .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_as_interface_bmcontrols },
	{ .field = "bFormatType",     .size = 1, .type = DESC_CONSTANT },
	{ .field = "bmFormats",       .size = 4, .type = DESC_SNOWFLAKE,
			.snowflake = desc_snowflake_dump_uac2_as_interface_bmformats },
	{ .field = "bNrChannels",     .size = 1, .type = DESC_NUMBER },
	{ .field = "bmChannelConfig", .size = 4, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac2_channel_names, .count = 26 } },
	{ .field = "iChannelNames",   .size = 1, .type = DESC_STR_DESC_INDEX },
	{ .field = NULL }
};
const struct desc * const desc_audio_as_interface[3] = {
	desc_audio_1_as_interface,
	desc_audio_2_as_interface,
	NULL, /* UAC3 not implemented yet */
};


static const char * const uac1_as_endpoint_bmattributes[] = {
	[0] = "Sampling Frequency",
	[1] = "Pitch",
	[2] = "Audio Data Format Control",
	[7] = "MaxPacketsOnly"
};
static const char * const uac2_as_endpoint_bmattributes[] = {
	[7] = "MaxPacketsOnly",
};
static const char * const uac2_as_isochronous_audio_data_endpoint_bmcontrols[] = {
	"Pitch",
	"Data Overrun",
	"Data Underrun",
	NULL
};
static const char * const uac_as_isochronous_audio_data_endpoint_blockdelayunits[] = {
	"Undefined",
	"Milliseconds",
	"Decoded PCM samples",
	NULL
};

/** UAC1: 4.6.1.2 Class-Specific AS Isochronous Audio Data Endpoint Descriptor; Table 4-21. */
static const struct desc desc_audio_1_as_isochronous_audio_data_endpoint[] = {
	{ .field = "bmAttributes",    .size = 1, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac1_as_endpoint_bmattributes, .count = 8 } },
	{ .field = "bLockDelayUnits", .size = 1, .type = DESC_NUMBER_STRINGS,
			.number_strings = uac_as_isochronous_audio_data_endpoint_blockdelayunits },
	{ .field = "wLockDelay",      .size = 2, .type = DESC_NUMBER },
	{ .field = NULL }
};
/** UAC2: 4.10.1.2 Class-Specific AS Isochronous Audio Data Endpoint Descriptor; Table 4-34. */
static const struct desc desc_audio_2_as_isochronous_audio_data_endpoint[] = {
	{ .field = "bmAttributes",    .size = 1, .type = DESC_BITMAP_STRINGS,
			.bitmap_strings = { .strings = uac2_as_endpoint_bmattributes, .count = 8 } },
	{ .field = "bmControls",      .size = 1, .type = DESC_BMCONTROL_2,
			.bmcontrol = uac2_as_isochronous_audio_data_endpoint_bmcontrols },
	{ .field = "bLockDelayUnits", .size = 1, .type = DESC_NUMBER_STRINGS,
			.number_strings = uac_as_isochronous_audio_data_endpoint_blockdelayunits },
	{ .field = "wLockDelay",      .size = 2, .type = DESC_NUMBER },
	{ .field = NULL }
};
const struct desc * const desc_audio_as_isochronous_audio_data_endpoint[3] = {
	desc_audio_1_as_isochronous_audio_data_endpoint,
	desc_audio_2_as_isochronous_audio_data_endpoint,
	NULL, /* UAC3 not implemented yet */
};
