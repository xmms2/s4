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

#endif  /* _S4_LOG_H */
