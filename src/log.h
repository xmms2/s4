/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009 Sivert Berg
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef _S4_LOG_H
#define _S4_LOG_H

#include <glib.h>

#define DEBUG

#ifdef DEBUG
#define _STR(x) #x
#define STR(x) _STR(x)
#define S4_DBG(fmt, ...) g_debug (__FILE__ ":" STR(__LINE__) ": " fmt, ##__VA_ARGS__)
#define S4_INFO(fmt, ...) g_message (__FILE__ ":" STR(__LINE__) ": " fmt, ##__VA_ARGS__)
#define S4_ERROR(fmt, ...) g_warning (__FILE__ ":" STR(__LINE__) ": " fmt, ##__VA_ARGS__)
#define S4_FATAL(fmt, ...) g_error (__FILE__ ":" STR(__LINE__) ": " fmt, ##__VA_ARGS__)
#else
#define S4_DBG(...)
#define S4_INFO g_message
#define S4_ERROR g_warning
#define S4_FATAL g_error
#endif /* DEBUG */

void log_init (GLogLevelFlags log_lev);

#endif  /* _S4_LOG_H */
