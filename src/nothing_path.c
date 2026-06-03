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

static pthread_mutex_t g_nothing_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static char *g_nothing_path = NULL;
static bool g_extracted = false;

#ifndef NDEBUG
/// Debug assertion: verify the calling thread holds g_nothing_lock.
///
/// NOTE: There is a benign TOCTOU window between pthread_mutex_unlock()
/// (when trylock succeeds, meaning lock was NOT held) and the assert().
/// Another thread could acquire the lock in that window, causing the
/// assertion to false-positive pass. This is debug-only and non-security;
/// the worst case is a missed assertion in a race condition that is itself
/// extremely unlikely (debug builds under test).
static inline void assert_lock_held(void) {
    int ret = pthread_mutex_trylock(&g_nothing_lock);
    if (ret == 0)
        pthread_mutex_unlock(&g_nothing_lock);
    assert((ret == EDEADLK || ret == EBUSY) &&
           "Called without lock held by current thread "
           "(lock not held at all, or held by another thread)");
}
#endif

// ============================================================================
// HELPERS
// ============================================================================

/// Validate that a temp directory path is safe to use.
/// Rejects paths containing ".." and paths not under trusted prefixes.
static bool is_safe_temp_dir(const char *path) {
    if (strstr(path, "..")) return false;
    if (strncmp(path, "/data/data/", 11) == 0) return true;
    if (strncmp(path, "/data/user/", 11) == 0) return true;
    if (strcmp(path, "/data/local/tmp") == 0) return true;
    return false;
}

/**
 * Safely get the temp directory path.
 * Validates TMPDIR environment variable to prevent path traversal issues.
 * Falls back to /data/local/tmp if TMPDIR is unset or invalid.
 */
static const char *get_temp_dir(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir && tmpdir[0] == '/' && is_safe_temp_dir(tmpdir)) {
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

    // Write loop for ~1.5KB blob. POSIX allows short writes on regular
    // files when interrupted by signals — loop until all bytes are written.
    const unsigned char *ptr = g_nothing_so_blob;
    size_t remaining = g_nothing_so_size;
    while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n <= 0) {
            // write() returned 0 is rare but means no progress; treat as error
            if (n == 0) errno = ENOSPC;
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
// INTERNAL: Extract embedded blob to a temp file and return strdup'd path
// ============================================================================

static char *extract_to_temp_path(void) {
    const char *tmpdir = get_temp_dir();
    char template[256];
    size_t max_tmpdir_len = sizeof(template) - sizeof("/.sh_nothing_XXXXXX");
    if (strlen(tmpdir) > max_tmpdir_len) {
        __android_log_print(ANDROID_LOG_ERROR, "memkit",
            "TMPDIR path too long: %zu >= %zu", strlen(tmpdir), max_tmpdir_len - 1);
        return NULL;
    }
    int written = snprintf(template, sizeof(template), "%s/.sh_nothing_XXXXXX", tmpdir);
    if (written < 0 || (size_t)written >= sizeof(template)) {
        __android_log_print(ANDROID_LOG_ERROR, "memkit",
            "TMPDIR path truncated for template buffer");
        return NULL;
    }
    if (extract_blob_to_file(template) != 0) return NULL;
    return strdup(template);
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
//
// Returns a strdup'd copy of the nothing library path. The caller owns the
// returned pointer and MUST free() it when done.
// ============================================================================

char *memkit_ensure_nothing_path(void) {
    pthread_mutex_lock(&g_nothing_lock);

    if (g_extracted) {
        char *ret = g_nothing_path ? strdup(g_nothing_path) : NULL;
        pthread_mutex_unlock(&g_nothing_lock);
        return ret;
    }

    if (g_nothing_path) {
        char *ret = strdup(g_nothing_path);
        g_extracted = true;
        pthread_mutex_unlock(&g_nothing_lock);
        return ret;
    }

    char *ret = extract_to_temp_path();
    if (ret) {
        g_nothing_path = ret;
        g_extracted = true;
    }
    pthread_mutex_unlock(&g_nothing_lock);
    return ret ? strdup(ret) : NULL;
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
