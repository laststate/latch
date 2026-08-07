#ifndef LASTSTATE_FINGERPRINT_H
#define LASTSTATE_FINGERPRINT_H

#include <stdint.h>

#include "event.h"

/* Stable diagnostic fingerprint for crash clustering. It deliberately uses
 * addresses/register state plus the build identity and is not a cryptographic
 * identifier. */
uint32_t ls_crash_fingerprint(const ls_arch_context_t *context);
uint32_t ls_event_fingerprint(const char *domain, int32_t code, uint32_t detail);

#endif
