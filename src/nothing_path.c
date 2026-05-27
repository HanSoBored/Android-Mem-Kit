#include "memkit.h"
#include "nothing_embed.h"
#include "nothing_path.h"

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <android/log.h>

// ============================================================================
// NOTHING LIBRARY PATH MANAGEMENT
//
// Manages the path to libshadowhook_nothing.so, which ShadowHook dlopen()s
// during initialization. When the user doesn't provide a path, the embedded
// blob is auto-extracted to a temporary file.
// ============================================================================

static pthread_mutex_t g_nothing_lock = PTHREAD_MUTEX_INITIALIZER;
static char *g_nothing_path = NULL;
static bool g_extracted = false;

#ifndef NDEBUG
static inline void assert_lock_held(void) {
    int ret = pthread_mutex_trylock(&g_nothing_lock);
    if (ret == 0)
        pthread_mutex_unlock(&g_nothing_lock);
    assert(ret == EDEADLK && "extract_blob_to_file() called without holding g_nothing_lock");
}
#endif

// ============================================================================
// HELPERS
// ============================================================================

/**
 * Safely get the temp directory path.
 * Validates TMPDIR environment variable to prevent path traversal issues.
 * Falls back to /data/local/tmp if TMPDIR is unset or invalid.
 */
static const char *get_temp_dir(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir && tmpdir[0] == '/') {
        __android_log_print(ANDROID_LOG_DEBUG, "memkit",
            "temp dir: %s (from TMPDIR)", tmpdir);
        return tmpdir;
    }
    __android_log_print(ANDROID_LOG_DEBUG, "memkit",
        "temp dir: /data/local/tmp (default fallback)");
    return "/data/local/tmp";
}

// ============================================================================
// INTERNAL: Extract embedded blob to a temp file at given path.
//
// ⚠️ CALLER MUST HOLD g_nothing_lock ⚠️
// This function accesses g_nothing_so_blob and g_nothing_so_size which
// are invariants set before any concurrent access, but the function itself
// is only called from memkit_ensure_nothing_path() which holds the lock.
// Do NOT call from any other context without holding the lock.
//
// The path template is modified in-place by mkstemp.
// Returns 0 on success, -1 on failure (path is unlinked on failure).
// ============================================================================
static int extract_blob_to_file(char *path) {
#ifndef NDEBUG
    assert_lock_held();
#endif
    int fd = -1;
    int ret = -1;

    fd = mkstemp(path);
    if (fd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "memkit",
            "mkstemp failed for nothing temp file: %s", strerror(errno));
        return -1;
    }

    const unsigned char *ptr = g_nothing_so_blob;
    size_t remaining = g_nothing_so_size;

    // Defensive: loop handles partial writes (common for large blobs,
    // harmless for our ~1.5KB blob).
    while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n <= 0) {
            __android_log_print(ANDROID_LOG_ERROR, "memkit",
                "write failed for nothing temp file: %s", strerror(errno));
            goto cleanup;
        }
        ptr += (size_t)n;
        remaining -= (size_t)n;
    }

    if (close(fd) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "memkit",
            "close failed for nothing temp file: %s", strerror(errno));
        fd = -1;
        goto cleanup;
    }
    fd = -1;
    ret = 0;

cleanup:
    if (fd >= 0)
        close(fd);
    if (ret != 0)
        unlink(path);
    return ret;
}

// ============================================================================
// PUBLIC API
// ============================================================================

void memkit_set_nothing_path(const char *path) {
    pthread_mutex_lock(&g_nothing_lock);

    free(g_nothing_path);
    g_nothing_path = NULL;
    g_extracted = false;

    if (path) {
        g_nothing_path = strdup(path);
        if (!g_nothing_path) {
            __android_log_print(ANDROID_LOG_ERROR, "memkit",
                "strdup failed in memkit_set_nothing_path: %s", strerror(errno));
            pthread_mutex_unlock(&g_nothing_lock);
            return;
        }
    }

    pthread_mutex_unlock(&g_nothing_lock);
}

char *memkit_get_nothing_path(void) {
    pthread_mutex_lock(&g_nothing_lock);
    char *ret = g_nothing_path ? strdup(g_nothing_path) : NULL;
    pthread_mutex_unlock(&g_nothing_lock);
    return ret;
}

// ============================================================================
// INTERNAL: Called by __wrap_dlopen in shadowhook_override.c
// ============================================================================

char *memkit_ensure_nothing_path(void) {
    pthread_mutex_lock(&g_nothing_lock);

    // If already extracted, return copy of path (may be NULL if consumed)
    if (g_extracted) {
        char *ret = g_nothing_path ? strdup(g_nothing_path) : NULL;
        pthread_mutex_unlock(&g_nothing_lock);
        return ret;
    }

    // If user-set path is available, return a copy
    if (g_nothing_path) {
        char *ret = strdup(g_nothing_path);
        pthread_mutex_unlock(&g_nothing_lock);
        return ret;
    }

    // Auto-extract the embedded blob to a temp file
    const char *tmpdir = get_temp_dir();

    char template[256];
    int written = snprintf(template, sizeof(template), "%s/.sh_nothing_XXXXXX", tmpdir);
    if (written < 0 || (size_t)written >= sizeof(template)) {
        pthread_mutex_unlock(&g_nothing_lock);
        return NULL;
    }

    if (extract_blob_to_file(template) != 0) {
        pthread_mutex_unlock(&g_nothing_lock);
        return NULL;
    }

    // Save the path
    g_nothing_path = strdup(template);
    if (!g_nothing_path) {
        unlink(template);
        pthread_mutex_unlock(&g_nothing_lock);
        return NULL;
    }
    g_extracted = true;

    __android_log_print(ANDROID_LOG_DEBUG, "memkit",
        "nothing library extracted to: %s", g_nothing_path);

    char *ret = strdup(g_nothing_path);
    pthread_mutex_unlock(&g_nothing_lock);
    return ret;
}

// ============================================================================
// INTERNAL: Called by __wrap_dlopen after successful load to clean up temp
// ============================================================================

void memkit_consume_nothing_path(void) {
    pthread_mutex_lock(&g_nothing_lock);
    if (g_extracted) {
        if (g_nothing_path) {
            unlink(g_nothing_path);
            free(g_nothing_path);
            g_nothing_path = NULL;
        } else {
            __android_log_print(ANDROID_LOG_WARN, "memkit",
                "consume: extracted flag set but path is NULL");
        }
        // Keep g_extracted = true — prevents re-extraction on second call
    }
    pthread_mutex_unlock(&g_nothing_lock);
}
