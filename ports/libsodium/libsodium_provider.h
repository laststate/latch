#ifndef LASTSTATE_LIBSODIUM_PROVIDER_H
#define LASTSTATE_LIBSODIUM_PROVIDER_H
#include "laststate/security.h"
/* Returns a provider backed by libsodium. The provider is marked externally
 * audited because libsodium documents a completed third-party security audit.
 * Integrators remain responsible for pinning/qualifying the exact version used. */
ls_crypto_provider_t ls_libsodium_crypto_provider(void);
#endif
