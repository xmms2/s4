#include <glib.h>

void log_handler (const gchar *log_domain,
		GLogLevelFlags log_level,
		const gchar *message,
		gpointer user_data)
{
	printf ("%s\n", message);
}

void log_init ()
{
	g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_handler, NULL);
}
