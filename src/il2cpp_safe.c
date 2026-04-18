#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

#include "memkit.h"

// ============================================================================
// IL2CPP: IMAGE RESOLUTION HELPER
// ============================================================================

/**
 * Get the Il2CppImage* for a named assembly.
 * Handles the full chain: domain_get_assemblies → find by name → assembly_get_image.
 * Falls back to image name matching if assembly_get_name is not exported.
 */
void* memkit_il2cpp_get_image(const char* assembly_name) {
    if (!assembly_name) {
        return NULL;
    }

    // Resolve required IL2CPP functions
    typedef void* (*il2cpp_domain_get_fn)(void);
    typedef void* (*il2cpp_domain_get_assemblies_fn)(void* domain, uint32_t* size);
    typedef const char* (*il2cpp_assembly_get_name_fn)(void* assembly);
    typedef void* (*il2cpp_assembly_get_image_fn)(void* assembly);
    typedef void* (*il2cpp_image_get_name_fn)(void* image);

    il2cpp_domain_get_fn domain_get =
        (il2cpp_domain_get_fn)memkit_il2cpp_resolve("il2cpp_domain_get");
    il2cpp_domain_get_assemblies_fn domain_get_assemblies =
        (il2cpp_domain_get_assemblies_fn)memkit_il2cpp_resolve("il2cpp_domain_get_assemblies");
    il2cpp_assembly_get_image_fn assembly_get_image =
        (il2cpp_assembly_get_image_fn)memkit_il2cpp_resolve("il2cpp_assembly_get_image");

    if (!domain_get || !domain_get_assemblies || !assembly_get_image) {
        return NULL;
    }

    // Get domain and assemblies
    void* domain = domain_get();
    if (!domain) {
        return NULL;
    }

    uint32_t count = 0;
    void** assemblies = domain_get_assemblies(domain, &count);
    if (!assemblies || count == 0) {
        return NULL;
    }

    // Try to find assembly by name and get its image
    il2cpp_assembly_get_name_fn assembly_get_name =
        (il2cpp_assembly_get_name_fn)memkit_il2cpp_resolve("il2cpp_assembly_get_name");

    for (uint32_t i = 0; i < count; i++) {
        void* assembly = assemblies[i];
        bool match = false;

        if (assembly_get_name) {
            const char* name = assembly_get_name(assembly);
            if (name && strcmp(name, assembly_name) == 0) {
                match = true;
            }
        }

        if (match) {
            return assembly_get_image(assembly);
        }
    }

    // Fallback: iterate images by name if assembly name lookup failed
    // This requires il2cpp_image_get_count and il2cpp_image_get_name
    typedef uint32_t (*il2cpp_image_get_count_fn)(void);
    il2cpp_image_get_count_fn image_get_count =
        (il2cpp_image_get_count_fn)memkit_il2cpp_resolve("il2cpp_image_get_count");
    il2cpp_image_get_name_fn image_get_name =
        (il2cpp_image_get_name_fn)memkit_il2cpp_resolve("il2cpp_image_get_name");

    if (image_get_count && image_get_name) {
        uint32_t image_count = image_get_count();
        for (uint32_t i = 0; i < image_count; i++) {
            // We need a function to get image by index - il2cpp_image_get
            typedef void* (*il2cpp_image_get_fn)(uint32_t index);
            il2cpp_image_get_fn image_get =
                (il2cpp_image_get_fn)memkit_il2cpp_resolve("il2cpp_image_get");
            if (!image_get) {
                break;
            }

            void* image = image_get(i);
            if (image) {
                const char* name = image_get_name(image);
                if (name && strstr(name, assembly_name) != NULL) {
                    return image;
                }
            }
        }
    }

    return NULL;
}

// ============================================================================
// IL2CPP: SAFE CALL WITH CRASH PROTECTION
// ============================================================================

static sigjmp_buf g_safe_call_env;
static volatile bool g_safe_call_caught = false;

static void safe_call_signal_handler(int sig) {
    (void)sig;
    g_safe_call_caught = true;
    siglongjmp(g_safe_call_env, 1);
}

/**
 * Safely call an IL2CPP runtime API with crash protection.
 * If the call crashes (SIGSEGV/SIGBUS), the function returns false instead
 * of killing the process.
 */
bool memkit_il2cpp_safe_call(void* (*fn)(void*), void* arg, void** out_result) {
    if (!fn) {
        return false;
    }

    struct sigaction old_segv, old_bus;
    struct sigaction sa;
    sa.sa_handler = safe_call_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Save old handlers and install ours
    sigaction(SIGSEGV, &sa, &old_segv);
    sigaction(SIGBUS, &sa, &old_bus);

    g_safe_call_caught = false;

    if (sigsetjmp(g_safe_call_env, 1) == 0) {
        // Normal execution
        void* result = fn(arg);

        // Restore old handlers
        sigaction(SIGSEGV, &old_segv, NULL);
        sigaction(SIGBUS, &old_bus, NULL);

        if (out_result) {
            *out_result = result;
        }

        return result != NULL;
    } else {
        // Jumped here from signal handler (crash caught)
        // Restore old handlers
        sigaction(SIGSEGV, &old_segv, NULL);
        sigaction(SIGBUS, &old_bus, NULL);

        if (out_result) {
            *out_result = NULL;
        }

        return false;
    }
}

// ============================================================================
// IL2CPP: READINESS DETECTION
// ============================================================================

/**
 * Wait until the IL2CPP runtime is ready.
 * Polls il2cpp_domain_get() until it returns non-NULL or timeout.
 */
void* memkit_il2cpp_wait_ready(int timeout_ms) {
    typedef void* (*il2cpp_domain_get_fn)(void);

    il2cpp_domain_get_fn domain_get =
        (il2cpp_domain_get_fn)memkit_il2cpp_resolve("il2cpp_domain_get");

    if (!domain_get) {
        return NULL;
    }

    int elapsed_ms = 0;
    const int poll_interval_ms = 50; // Poll every 50ms

    while (elapsed_ms < timeout_ms) {
        void* domain = domain_get();
        if (domain != NULL) {
            return domain;
        }

        // Sleep for poll interval (or remaining time)
        int sleep_ms = poll_interval_ms;
        if (elapsed_ms + sleep_ms > timeout_ms) {
            sleep_ms = timeout_ms - elapsed_ms;
        }

        struct timespec ts;
        ts.tv_sec = sleep_ms / 1000;
        ts.tv_nsec = (sleep_ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);

        elapsed_ms += sleep_ms;
    }

    return NULL; // Timeout
}

// ============================================================================
// IL2CPP: THREAD ATTACHMENT HELPER
// ============================================================================

/**
 * Attach the current thread to the IL2CPP domain.
 */
void* memkit_il2cpp_attach_thread(void* domain) {
    if (!domain) {
        return NULL;
    }

    typedef void* (*il2cpp_thread_attach_fn)(void* domain);

    il2cpp_thread_attach_fn thread_attach =
        (il2cpp_thread_attach_fn)memkit_il2cpp_resolve("il2cpp_thread_attach");

    if (!thread_attach) {
        return NULL;
    }

    return thread_attach(domain);
}

/**
 * Detach the current thread from the IL2CPP domain.
 */
void memkit_il2cpp_detach_thread(void* thread) {
    if (!thread) {
        return;
    }

    typedef void (*il2cpp_thread_detach_fn)(void* thread);

    il2cpp_thread_detach_fn thread_detach =
        (il2cpp_thread_detach_fn)memkit_il2cpp_resolve("il2cpp_thread_detach");

    if (thread_detach) {
        thread_detach(thread);
    }
}
