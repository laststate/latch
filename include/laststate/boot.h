#ifndef LASTSTATE_BOOT_H
#define LASTSTATE_BOOT_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"
void ls_reset_mark_expected(bool expected);
void ls_boot_mark_successful(void);
bool ls_safe_mode_requested(void);
void ls_release_mark_pending(const char *release);
void ls_release_confirm(void);
void ls_release_mark_rollback(void);
uint32_t ls_boot_count(void);
#endif
