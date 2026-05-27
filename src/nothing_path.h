#ifndef NOTHING_PATH_H
#define NOTHING_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// NOTHING LIBRARY PATH — INTERNAL HEADER
//
// Exposes internal functions used by shadowhook_override.c's __wrap_dlopen.
// ============================================================================

/**
 * Ensure the nothing library path is available.
 *
 * If memkit_set_nothing_path() was called, returns that path.
 * Otherwise, auto-extracts the embedded blob to a temp file.
 *
 * Thread-safe. Called from __wrap_dlopen during shadowhook_init().
 *
 * @return Path to libshadowhook_nothing.so (caller must free), or NULL on failure.
 */
char *memkit_ensure_nothing_path(void);

/**
 * @brief Consume (clean up) the auto-extracted temp file for nothing library.
 *
 * Should be called after dlopen() of the nothing library when the path was
 * auto-extracted (not user-set). Unlinks the temp file and frees the path.
 *
 * @note This is a ONE-SHOT consume — subsequent calls to ensure() will NOT
 * re-extract the blob. It returns the user-set path (if any) or NULL.
 * This prevents re-extraction on repeated dlopen calls.
 *
 * Thread-safe.
 */
void memkit_consume_nothing_path(void);

#ifdef __cplusplus
}
#endif

#endif // NOTHING_PATH_H
