// SPDX-License-Identifier: GPL-2.0+
/*
 * USB name database manipulation routines
 *
 * Copyright (C) 1999, 2000 Thomas Sailer (sailer@ife.ee.ethz.ch)
 */

#ifndef _NAMES_H
#define _NAMES_H

#include <sys/types.h>

/* ---------------------------------------------------------------------- */

extern const char *names_vendor(u_int16_t vendorid);
extern const char *names_product(u_int16_t vendorid, u_int16_t productid);
extern const char *names_class(u_int8_t classid);
extern const char *names_subclass(u_int8_t classid, u_int8_t subclassid);
extern const char *names_protocol(u_int8_t classid, u_int8_t subclassid,
				  u_int8_t protocolid);
extern const char *names_audioterminal(u_int16_t termt);
extern const char *names_videoterminal(u_int16_t termt);
extern const char *names_hid(u_int8_t hidd);
extern const char *names_reporttag(u_int8_t rt);
extern const char *names_huts(unsigned int data);
extern const char *names_hutus(unsigned int data);
extern const char *names_langid(u_int16_t langid);
extern const char *names_physdes(u_int8_t ph);
extern const char *names_bias(u_int8_t b);
extern const char *names_countrycode(unsigned int countrycode);

extern int get_vendor_string(char *buf, size_t size, u_int16_t vid);
extern int get_product_string(char *buf, size_t size, u_int16_t vid, u_int16_t pid);
extern int get_class_string(char *buf, size_t size, u_int8_t cls);
extern int get_subclass_string(char *buf, size_t size, u_int8_t cls, u_int8_t subcls);

extern int names_init(void);
extern void names_exit(void);

/* ---------------------------------------------------------------------- */
#endif /* _NAMES_H */
