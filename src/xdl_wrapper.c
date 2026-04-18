//
// Android-Mem-Kit: xDL Wrapper Layer
// Generic dynamic linking toolkit for library discovery and symbol resolution
//

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <link.h>

#include "memkit.h"
#include "xdl.h"

// ============================================================================
// PHASE 1: CORE DISCOVERY API
// ============================================================================

// Internal callback adapter for library iteration
typedef struct {
    memkit_lib_iter_cb_t user_callback;
    void* user_data;
    int flags;
    int count;
} scan_callback_ctx_t;

// Thread-local storage for basename buffer (prevents race conditions)
// Each thread gets its own buffer, making memkit_xdl_iterate() thread-safe
static __thread char tls_basename_buf[PATH_MAX];

static int scan_callback_adapter(struct dl_phdr_info* info, size_t size, void* data) {
    (void)size;
    scan_callback_ctx_t* ctx = (scan_callback_ctx_t*)data;

    MemKitLibInfo lib_info = {0};

    if (ctx->flags & XDL_FULL_PATHNAME) {
        lib_info.path = info->dlpi_name;
        if (info->dlpi_name && info->dlpi_name[0] != '\0') {
            strncpy(tls_basename_buf, info->dlpi_name, sizeof(tls_basename_buf) - 1);
            tls_basename_buf[sizeof(tls_basename_buf) - 1] = '\0';
            lib_info.name = basename(tls_basename_buf);
        } else {
            lib_info.name = NULL;
        }
    } else {
        lib_info.name = info->dlpi_name;
        lib_info.path = NULL;
    }

    lib_info.base = info->dlpi_addr;

    lib_info.size = 0;
    for (size_t i = 0; i < info->dlpi_phnum; i++) {
        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
            lib_info.size += info->dlpi_phdr[i].p_memsz;
        }
    }

    if (ctx->user_callback(&lib_info, ctx->user_data)) {
        ctx->count++;
        return 0;
    } else {
        return 1;
    }
}

int memkit_xdl_iterate(memkit_lib_iter_cb_t callback, void* user_data, int flags) {
    if (!callback) {
        return -1;
    }

    // Validate flags - only XDL_DEFAULT and XDL_FULL_PATHNAME are valid
    if (flags != XDL_DEFAULT && flags != XDL_FULL_PATHNAME) {
        return -1;
    }

    scan_callback_ctx_t ctx = {
        .user_callback = callback,
        .user_data = user_data,
        .flags = flags,
        .count = 0
    };

    int result = xdl_iterate_phdr(scan_callback_adapter, &ctx, flags);
    return (result < 0) ? -1 : ctx.count;
}

void* memkit_xdl_open(const char* name, int flags) {
    if (!name) {
        return NULL;
    }
    return xdl_open(name, flags);
}

bool memkit_xdl_close(void* handle) {
    if (!handle) {
        return false;
    }
    return xdl_close(handle) == 0;  // xdl_close returns 0 on success
}

void* memkit_xdl_sym(void* handle, const char* symbol, size_t* out_size) {
    if (!handle || !symbol) {
        return NULL;
    }
    return xdl_sym(handle, symbol, out_size);
}

void* memkit_xdl_dsym(void* handle, const char* symbol, size_t* out_size) {
    if (!handle || !symbol) {
        return NULL;
    }
    return xdl_dsym(handle, symbol, out_size);
}

bool memkit_xdl_get_lib_info(void* handle, MemKitLibInfo* out) {
    if (!handle || !out) {
        return false;
    }

    xdl_info_t info = {0};

    int result = xdl_info(handle, XDL_DI_DLINFO, &info);
    if (result != 0) {
        return false;
    }

    out->name = info.dli_fname ? info.dli_fname : "<unknown>";
    out->path = info.dli_fname ? info.dli_fname : "<unknown>";
    out->base = (uintptr_t)info.dli_fbase;

    // Note: phdr/phnum not exposed in this version for simplicity
    // Can be added later if needed

    return true;
}

// ============================================================================
// PHASE 2: DEBUG INTROSPECTION API
// ============================================================================

// Internal structure (opaque to users)
struct memkit_addr_ctx {
    void* cache;  // Internal xdl_addr() cache
};

memkit_addr_ctx_t* memkit_xdl_addr_ctx_create(void) {
    memkit_addr_ctx_t* ctx = (memkit_addr_ctx_t*)calloc(1, sizeof(memkit_addr_ctx_t));
    if (!ctx) {
        return NULL;
    }
    ctx->cache = NULL;  // Initialize to NULL (first xdl_addr call will allocate)
    return ctx;
}

void memkit_xdl_addr_ctx_destroy(memkit_addr_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    // Clean xdl_addr cache to prevent memory leak
    if (ctx->cache) {
        xdl_addr_clean(&ctx->cache);
    }

    free(ctx);
}

bool memkit_xdl_addr_to_symbol(void* addr, MemKitSymInfo* out, memkit_addr_ctx_t* ctx) {
    if (!addr || !out) {
        return false;
    }

    xdl_info_t info = {0};

    int result = xdl_addr(addr, &info, (ctx) ? &ctx->cache : NULL);
    if (result == 0) {
        return false;
    }

    out->lib_name = info.dli_fname ? info.dli_fname : "<unknown>";
    out->lib_base = (uintptr_t)info.dli_fbase;
    out->sym_name = info.dli_sname;  // May be NULL if no symbol found
    out->sym_offset = info.dli_saddr 
        ? (uintptr_t)addr - (uintptr_t)info.dli_saddr 
        : 0;
    out->sym_size = info.dli_ssize;

    return true;
}

bool memkit_xdl_addr_to_symbol4(void* addr, MemKitSymInfo* out, memkit_addr_ctx_t* ctx, int flags) {
    if (!addr || !out) {
        return false;
    }

    xdl_info_t info = {0};

    int result = xdl_addr4(addr, &info, (ctx) ? &ctx->cache : NULL, flags);
    if (result == 0) {
        return false;
    }

    out->lib_name = info.dli_fname ? info.dli_fname : "<unknown>";
    out->lib_base = (uintptr_t)info.dli_fbase;
    out->sym_name = info.dli_sname;  // May be NULL if no symbol found
    out->sym_offset = info.dli_saddr 
        ? (uintptr_t)addr - (uintptr_t)info.dli_saddr 
        : 0;
    out->sym_size = info.dli_ssize;

    return true;
}

// ============================================================================
// PHASE 3: ADVANCED FEATURES
// ============================================================================

void* memkit_xdl_open_from_phdr(struct dl_phdr_info* info);

void* memkit_xdl_open_from_phdr(struct dl_phdr_info* info) {
    if (!info) {
        return NULL;
    }
    return xdl_open2(info);
}
