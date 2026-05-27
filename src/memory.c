#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>

#include "memkit.h"

// ============================================================================
// PAGE ALIGNMENT HELPERS (Cached page size)
// ============================================================================

static long get_page_size(void) {
    static long cached_page_size = 0;
    if (cached_page_size <= 0) {
        cached_page_size = sysconf(_SC_PAGESIZE);
        if (cached_page_size <= 0) cached_page_size = 4096;
    }
    return cached_page_size;
}

static inline uintptr_t page_mask(void) {
    return (uintptr_t)get_page_size() - 1;
}

static inline uintptr_t page_align_down(uintptr_t addr) {
    return addr & ~page_mask();
}

static inline uintptr_t page_align_up(uintptr_t addr, size_t size) {
    uintptr_t m = page_mask();
    return (addr + size + m) & ~m;
}

// ============================================================================
// HELPER: READ ORIGINAL MEMORY PERMISSIONS FROM /proc/self/maps
//
// Reads /proc/self/maps to determine the original memory protection flags
// for the page range covering [address, address + size).
//
// Returns protection flags (PROT_READ | PROT_WRITE | PROT_EXEC combination)
// on success, or -1 if the mapping cannot be determined.
// ============================================================================

static int get_prot_from_maps(uintptr_t address, size_t size) {
    uintptr_t page_start = page_align_down(address);
    uintptr_t page_end = page_align_up(address, size);

    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return -1;

    char line[1024];
    int result_prot = -1;

    while (fgets(line, sizeof(line), fp)) {
        uintptr_t map_start = 0;
        uintptr_t map_end = 0;
        char perm[8] = {0};

        if (sscanf(line, "%" PRIxPTR "-%" PRIxPTR " %7s", &map_start, &map_end, perm) < 3)
            continue;

        // Check if this mapping covers our full page range
        if (map_start <= page_start && map_end >= page_end) {
            int prot = 0;
            if (perm[0] == 'r') prot |= PROT_READ;
            if (perm[1] == 'w') prot |= PROT_WRITE;
            if (perm[2] == 'x') prot |= PROT_EXEC;
            result_prot = prot;
            break;
        }
    }

    fclose(fp);
    return result_prot;
}

// ============================================================================
// HELPER: CROSS-PAGE SAFE MPROTECT
// Fixes CRITICAL BUG: Handle patches that span multiple memory pages
//
// Sets pages to RWX and returns the original protection flags.
// Returns original prot on success, -1 on failure.
// ============================================================================

static int unprotect_memory(uintptr_t address, size_t size) {
    uintptr_t page_start = page_align_down(address);
    uintptr_t page_end = page_align_up(address, size);
    size_t mprotect_len = page_end - page_start;
    
    // Save original permissions before changing to RWX
    int orig_prot = get_prot_from_maps(page_start, mprotect_len);
    
    if (mprotect((void*)page_start, mprotect_len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return -1;
    }
    
    return orig_prot;
}

// ============================================================================
// HELPER: RESTORE ORIGINAL MEMORY PERMISSIONS
//
// Restores memory protection flags for the page range covering
// [address, address + size). Call after patching is complete.
//
// Returns true on success, false on failure.
// ============================================================================

static bool protect_memory(uintptr_t address, size_t size, int prot) {
    if (prot < 0) return false; // Original permissions unknown — skip restore

    uintptr_t page_start = page_align_down(address);
    uintptr_t page_end = page_align_up(address, size);

    return mprotect((void*)page_start, page_end - page_start, prot) == 0;
}

// ============================================================================
// HELPER: SAFE HEX PARSER (No sscanf, dynamic allocation)
// Fixes: Buffer overflow risk and 256-byte limit
// ============================================================================

static size_t hex2bin(const char* hex, uint8_t** out_buffer) {
    // Validate inputs
    if (!hex || !out_buffer) {
        errno = EINVAL;
        return 0;
    }
    
    *out_buffer = NULL;
    
    size_t hex_len = strlen(hex);
    if (hex_len == 0) {
        errno = EINVAL;  // Empty hex string
        return 0;
    }
    
    // Allocate buffer dynamically (max possible: hex_len / 2)
    *out_buffer = (uint8_t*)malloc((hex_len / 2) + 1);
    if (!*out_buffer) {
        errno = ENOMEM;
        return 0;
    }
    
    size_t byte_count = 0;
    const char* pos = hex;
    
    while (*pos) {
        // Skip whitespace
        if (isspace((unsigned char)*pos)) {
            pos++;
            continue;
        }
        
        // Parse high nibble
        char high = *pos++;
        uint8_t val = 0;
        
        if (high >= '0' && high <= '9') {
            val = (uint8_t)((high - '0') << 4);
        } else if (high >= 'a' && high <= 'f') {
            val = (uint8_t)((high - 'a' + 10) << 4);
        } else if (high >= 'A' && high <= 'F') {
            val = (uint8_t)((high - 'A' + 10) << 4);
        } else {
            // Invalid character
            errno = EINVAL;
            free(*out_buffer);
            *out_buffer = NULL;
            return 0;
        }
        
        // Skip whitespace between nibbles
        while (*pos && isspace((unsigned char)*pos)) {
            pos++;
        }
        
        // Parse low nibble
        if (!*pos) {
            // Odd number of hex digits - incomplete byte
            free(*out_buffer);
            *out_buffer = NULL;
            return 0;
        }
        
        char low = *pos++;
        
        if (low >= '0' && low <= '9') {
            val |= (uint8_t)(low - '0');
        } else if (low >= 'a' && low <= 'f') {
            val |= (uint8_t)(low - 'a' + 10);
        } else if (low >= 'A' && low <= 'F') {
            val |= (uint8_t)(low - 'A' + 10);
        } else {
            // Invalid character
            errno = EINVAL;
            free(*out_buffer);
            *out_buffer = NULL;
            return 0;
        }
        
        (*out_buffer)[byte_count++] = val;
    }
    
    return byte_count;
}

// ============================================================================
// MEMORY PATCHING: GET LIBRARY BASE ADDRESS
// FIXED: Returns lowest base address (handles multiple segments)
// ============================================================================

uintptr_t memkit_get_lib_base(const char* lib_name) {
    if (!lib_name) {
        errno = EINVAL;
        return 0;
    }
    
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        return 0;
    }
    
    char line[1024]; // Increased buffer size for Android modern
    uintptr_t lowest_base = ~(uintptr_t)0; // MAX uintptr
    bool found = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // Check if line contains the library name
        if (strstr(line, lib_name) != NULL) {
            // Parse the starting address
            uintptr_t start = 0;
            if (sscanf(line, "%" PRIxPTR, &start) == 1) {
                // Keep the lowest base address
                // (.so files have multiple segments: r-xp, r--p, rw-p, etc.)
                if (start < lowest_base) {
                    lowest_base = start;
                    found = true;
                }
            }
        }
    }
    
    fclose(fp);
    return found ? lowest_base : 0;
}

// ============================================================================
// MEMORY PATCHING: CREATE PATCH
// FIXED: Safely backs up original bytes (handles XOM)
// FIXED: Dynamic buffer allocation (no 256-byte limit)
// ============================================================================

MemPatch* memkit_patch_create(uintptr_t address, const char* hex_string) {
    if (address == 0 || !hex_string) {
        errno = EINVAL;
        return NULL;
    }

    MemPatch* patch = (MemPatch*)calloc(1, sizeof(MemPatch));
    if (!patch) {
        errno = ENOMEM;
        return NULL;
    }

    patch->address = address;

    // Parse hex string (dynamically allocates patch->patch_bytes)
    patch->size = hex2bin(hex_string, &patch->patch_bytes);

    if (patch->size == 0 || !patch->patch_bytes) {
        // errno already set by hex2bin
        free(patch);
        return NULL;
    }

    // Allocate buffer for original bytes
    patch->orig_bytes = (uint8_t*)malloc(patch->size);
    if (!patch->orig_bytes) {
        errno = ENOMEM;
        free(patch->patch_bytes);
        free(patch);
        return NULL;
    }
    
    // CRITICAL FIX: Unprotect memory BEFORE reading original bytes
    // This bypasses Execute-Only-Memory (XOM) protections on Android modern
    int orig_prot = unprotect_memory(patch->address, patch->size);
    if (orig_prot < 0) {
        free(patch->orig_bytes);
        free(patch->patch_bytes);
        free(patch);
        return NULL;
    }
    
    // Safe to read original bytes now
    memcpy(patch->orig_bytes, (void*)patch->address, patch->size);
    
    // Restore original permissions — minimizes RWX window
    protect_memory(patch->address, patch->size, orig_prot);
    
    return patch;
}

// ============================================================================
// MEMORY PATCHING: APPLY PATCH
// ============================================================================

bool memkit_patch_apply(MemPatch* patch) {
    if (!patch || !patch->patch_bytes || patch->size == 0) {
        errno = EINVAL;
        return false;
    }

    // Unprotect memory (cross-page safe) — saves original permissions
    int orig_prot = unprotect_memory(patch->address, patch->size);
    if (orig_prot < 0) {
        errno = EACCES;  // Permission denied
        return false;
    }
    
    // Apply the patch
    memcpy((void*)patch->address, patch->patch_bytes, patch->size);
    
    // Flush instruction cache (critical for ARM/Android)
    __builtin___clear_cache(
        (char*)patch->address, 
        (char*)(patch->address + patch->size)
    );
    
    // Restore original permissions — prevents leaving pages RWX
    protect_memory(patch->address, patch->size, orig_prot);
    
    return true;
}

// ============================================================================
// MEMORY PATCHING: RESTORE ORIGINAL
// ============================================================================

bool memkit_patch_restore(MemPatch* patch) {
    if (!patch || !patch->orig_bytes || patch->size == 0) {
        errno = EINVAL;
        return false;
    }

    // Unprotect memory (cross-page safe) — saves original permissions
    int orig_prot = unprotect_memory(patch->address, patch->size);
    if (orig_prot < 0) {
        errno = EACCES;  // Permission denied
        return false;
    }
    
    // Restore original bytes
    memcpy((void*)patch->address, patch->orig_bytes, patch->size);
    
    // Flush instruction cache
    __builtin___clear_cache(
        (char*)patch->address, 
        (char*)(patch->address + patch->size)
    );
    
    // Restore original permissions — prevents leaving pages RWX
    protect_memory(patch->address, patch->size, orig_prot);
    
    return true;
}

// ============================================================================
// MEMORY PATCHING: FREE RESOURCES
// ============================================================================

void memkit_patch_free(MemPatch* patch) {
    if (patch) {
        free(patch->orig_bytes);
        free(patch->patch_bytes);
        free(patch);
    }
}
