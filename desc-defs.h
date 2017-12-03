/*****************************************************************************/

/*
 *      desc-defs.h  --  USB descriptor definitions
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

#ifndef _DESC_DEFS_H
#define _DESC_DEFS_H

/* ---------------------------------------------------------------------- */

/**
 * Descriptor field value type.
 *
 * Note that there are more types here than exist in the descriptor definitions
 * in the specifications.  This is because the type here is used to do `lsusb`
 * specific rendering of certain fields.
 *
 * Note that the use of certain types mandates the setting of various entries
 * in the type-specific anonymous union in `struct desc`.
 */
enum desc_type {
	DESC_CONSTANT,       /** Plain numerical value; no annotation. */
	DESC_NUMBER,         /** Plain numerical value; no annotation. */
	DESC_NUMBER_POSTFIX, /**< Number with a postfix string. */
	DESC_BITMAP,         /**< Plain hex rendered value; no annotation. */
	DESC_BCD,            /**< Binary coded decimal */
	DESC_BMCONTROL_1,    /**< UAC1 style bmControl field */
	DESC_BMCONTROL_2,    /**< UAC2/UAC3 style bmControl field */
	DESC_STR_DESC_INDEX, /**< String index. */
	DESC_TERMINAL_STR,   /**< Audio terminal string. */
	DESC_BITMAP_STRINGS, /**< Bitfield with string per bit. */
	DESC_NUMBER_STRINGS, /**< Use for enum-style value to string. */
	DESC_SNOWFLAKE,  /**< Value with custom annotation callback function. */
};

/**
 * Callback function for the DESC_SNOWFLAKE descriptor field value type.
 *
 * This is used when some special rendering of the value is required, which
 * is specific to the field in question, so no generic type's rendering is
 * suitable.
 *
 * The core descriptor dumping code will have already dumped the numerical
 * value for the field, but not the trailing newline character.  It is up
 * to the callback function to ensure it always finishes by writing a '\n'
 * character to stdout.
 *
 * \param[in] value    The value to dump a human-readable representation of.
 * \param[in] indent   The current indent level.
 */
typedef void (*desc_snowflake_dump_fn)(
		unsigned long long value,
		unsigned int indent);


/**
 * Descriptor field definition.
 *
 * Whole descriptors can be defined as NULL terminated arrays of these
 * structures.
 */
struct desc {
	const char *field;   /**< Field's name */
	unsigned int size;   /**< Byte size of field, if (size_field == NULL) */
	const char *size_field; /**< Name of field specifying field size. */
	enum desc_type type; /**< Field's value type. */

	/** Anonymous union containing type-specific data. */
	union {
		/**
		 * Corresponds to types DESC_BMCONTROL_1 and DESC_BMCONTROL_2.
		 *
		 * Must be a NULL terminated array of '\0' terminated strings.
		 */
		const char * const *bmcontrol;
		/** Corresponds to type DESC_BITMAP_STRINGS */
		struct {
			/** Must contain '\0' terminated strings. */
			const char * const *strings;
			/** Number of strings in strings array. */
			unsigned int count;
		} bitmap_strings;
		/**
		 * Corresponds to type DESC_NUMBER_STRINGS.
		 *
		 * Must be a NULL terminated array of '\0' terminated strings.
		 */
		const char * const *number_strings;
		/**
		 * Corresponds to type DESC_NUMBER_POSTFIX.
		 *
		 * Must be a '\0' terminated string.
		 */
		const char *number_postfix;
		/**
		 * Corresponds to type DESC_SNOWFLAKE.
		 *
		 * Callback function called to annotate snowflake value type.
		 */
		desc_snowflake_dump_fn snowflake;
	};

	/** Grouping of array-specific fields. */
	struct {
		bool array; /**< True if entry is an array. */
		bool bits;  /**< True if array length is specified in bits */
		/** Name of field specifying the array entry count. */
		const char *length_field1;
		/** Name of field specifying multiplier for array entry count. */
		const char *length_field2;
	} array;
};

/* ---------------------------------------------------------------------- */

/* Audio Control (AC) descriptor definitions */
extern const struct desc * const desc_audio_ac_header[3];
extern const struct desc * const desc_audio_ac_effect_unit[3];
extern const struct desc * const desc_audio_ac_input_terminal[3];
extern const struct desc * const desc_audio_ac_output_terminal[3];
extern const struct desc * const desc_audio_ac_mixer_unit[3];
extern const struct desc * const desc_audio_ac_selector_unit[3];
extern const struct desc * const desc_audio_ac_processing_unit[3];
extern const struct desc * const desc_audio_ac_feature_unit[3];
extern const struct desc * const desc_audio_ac_extension_unit[3];
extern const struct desc * const desc_audio_ac_clock_source[3];
extern const struct desc * const desc_audio_ac_clock_selector[3];
extern const struct desc * const desc_audio_ac_clock_multiplier[3];
extern const struct desc * const desc_audio_ac_sample_rate_converter[3];

/* Audio Streaming (AS) descriptor definitions */
extern const struct desc * const desc_audio_as_interface[3];
extern const struct desc * const desc_audio_as_isochronous_audio_data_endpoint[3];

/* ---------------------------------------------------------------------- */

#endif /* _DESC_DEFS_H */
