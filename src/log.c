#include "s4_priv.h"
#include "logging.h"
#include <string.h>

/**
 * Wrapper for midb_log, writes a LOG_STRING_INSERT entry
 */
void _log_string_insert (s4_t *be, int32_t id, const char *string)
{
	log_entry_t entry;
	entry.type = LOG_STRING_INSERT;
	entry.data.str.str = string;
	entry.data.str.id = id;
	_log (be, &entry);
}

/**
 * Wrapper for midb_log, writes a LOG_PAIR_INSERT entry
 */
void _log_pair_insert (s4_t *be, s4_intpair_t *rec)
{
	log_entry_t entry;
	entry.type = LOG_PAIR_INSERT;
	entry.data.pair = rec;
	_log (be, &entry);
}

/**
 * Wrapper for midb_log, writes a LOG_PAIR_REMOVE entry
 */
void _log_pair_remove (s4_t *be, s4_intpair_t *rec)
{
	log_entry_t entry;
	entry.type = LOG_PAIR_REMOVE;
	entry.data.pair = rec;
	_log (be, &entry);
}

/**
 * Write a log entry.
 */
void _log (s4_t *be, log_entry_t *entry)
{
	int32_t tmp;

	if (be->logfile == NULL)
		return;

	fwrite (&entry->type, sizeof(int32_t), 1, be->logfile);
	switch (entry->type) {
		case LOG_STRING_INSERT:
			tmp = strlen (entry->data.str.str);
			fwrite (&entry->data.str.id, sizeof (int32_t), 1, be->logfile);
			fwrite (&tmp, sizeof (int32_t), 1, be->logfile);
			fwrite (entry->data.str.str, 1, tmp, be->logfile);
			break;
		case LOG_PAIR_INSERT:
		case LOG_PAIR_REMOVE:
			fwrite (entry->data.pair, sizeof (s4_intpair_t), 1, be->logfile);
			break;
		default:
			S4_DBG ("Trying to write a log entry with invalid type");
			break;
	}
}
