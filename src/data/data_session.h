#ifndef NK_DATA_SESSION_H
#define NK_DATA_SESSION_H

#include <stdbool.h>
#include <stdint.h>

/* History refers to finished AI-1 session rows written by the acquisition code.
 * Use data.h/session.h for insertion. Scores belong to the analysis component. */
int nk_data_session_count(void);
bool nk_data_session_exists(int64_t session_id);
bool nk_data_session_delete_by_id(int64_t session_id);
bool nk_data_session_delete_all(void);

#endif
