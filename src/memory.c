/* Feature-test macro for fseeko64/ftello64/off64_t on older NDK / non-Android POSIX */
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#ifdef __ANDROID__
#include <android/log.h>
#else
/* Stub for non-Android builds — logs are compiled out */
#define ANDROID_LOG_WARN   3
#define ANDROID_LOG_ERROR  6
#define __android_log_print(prio, tag, ...) ((void)0)
#endif

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
// MEMORY PATCHING: GET LIBRARY BASE ADDRESS (from /proc/self/maps)
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

// ============================================================================
// ENHANCED LIBRARY DISCOVERY: Find lib by scanning APK ZIP files in maps
//
// On modern Android (12+), native libraries from split APKs may be loaded
// directly in-place from the APK without extraction. They appear in maps
// as offsets within the APK file rather than separate .so entries.
//
// This function:
//   1. Scans /proc/self/maps for executable APK mappings
//   2. Parses the ZIP central directory to find the library entry offset
//   3. Matches the offset in maps to calculate the base address
//
// Returns base address or 0 if not found.
// ============================================================================

#define ZIP_LOCAL_HEADER_MAGIC  0x04034b50
#define ZIP_CENTRAL_MAGIC       0x02014b50
#define ZIP_EOCD_MAGIC          0x06054b50
#define ZIP_EOCD_MAX_COMMENT    65535

/* Custom little-endian helpers (kept for cross-platform portability rather
 * than using Android NDK's <endian.h> le32toh/le16toh macros) */
static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * Locate the End of Central Directory record by reading the file tail into a
 * heap buffer (one I/O operation), then scanning backward in memory for the
 * EOCD magic.  This reduces syscalls from O(n) to O(1), matching the pattern
 * used by libzip's find_eocd / _zip_find_central_dir.
 *
 * NOTE: Allocates up to (ZIP_EOCD_MAX_COMMENT + 22) = 65,557 bytes on the heap
 * via malloc(buf_size). This is intentional — a single heap allocation avoids
 * large stack usage. The buffer is freed before returning.
 *
 * Returns the file offset of the EOCD, or -1 if not found.
 */
static off64_t zip_find_eocd(FILE* apk, off64_t file_size) {
    if (file_size < 22) return -1;

    /* Read the last (ZIP_EOCD_MAX_COMMENT + 22) bytes — or the whole file
     * if smaller — into a heap buffer with a single fread. */
    size_t buf_size = (file_size < ZIP_EOCD_MAX_COMMENT + 22)
                          ? (size_t)file_size
                          : (size_t)(ZIP_EOCD_MAX_COMMENT + 22);

    uint8_t* buf = (uint8_t*)malloc(buf_size);
    if (!buf) return -1;

    off64_t search_start = file_size - (off64_t)buf_size;
    if (fseeko64(apk, search_start, SEEK_SET)) { free(buf); return -1; }
    if (fread(buf, 1, buf_size, apk) != buf_size) { free(buf); return -1; }

    /* Scan backward for EOCD magic (EOCD_MAGIC is always at least 22 bytes) */
    for (off64_t i = (off64_t)buf_size - 22; i >= 0; i--) {
        if (read_le32(buf + (size_t)i) == ZIP_EOCD_MAGIC) {
            off64_t result = search_start + i;
            free(buf);
            return result;
        }
    }

    free(buf);
    return -1;
}

/**
 * Validate ZIP64 EOCD locator and record.
 * Called when cd_offset32 == 0xFFFFFFFF (ZIP64 sentinel).
 * Reads the EOCD64 locator (at eocd_offset - 20), validates its magic,
 * then reads the EOCD64 record and extracts 64-bit cd offset and entry count.
 *
 * Reference pattern: libzip's _zip_read_eocd64()
 */
static bool zip_validate_eocd_zip64(FILE* apk, off64_t eocd_offset,
                                     off64_t file_size,
                                     uint64_t* out_cd_offset,
                                     uint32_t* out_cd_entries) {
    uint8_t loc_buf[20];
    if (eocd_offset < 20) return false;
    if (fseeko64(apk, eocd_offset - 20, SEEK_SET)) return false;
    if (fread(loc_buf, 1, 20, apk) != 20) return false;

    /* Validate ZIP64 EOCD Locator magic: 0x07064b50 */
    if (read_le32(loc_buf) != 0x07064b50) return false;

    /* Read 64-bit offset of ZIP64 EOCD record (bytes 8-15 of locator) */
    uint64_t zip64_eocd_offset = (uint64_t)read_le32(loc_buf + 8)
                               | ((uint64_t)read_le32(loc_buf + 12) << 32);

    if (zip64_eocd_offset >= (uint64_t)file_size) return false;
    if (fseeko64(apk, (off64_t)zip64_eocd_offset, SEEK_SET)) return false;

    /* Read ZIP64 EOCD record (need 56 bytes to reach cd_offset at +48) */
    uint8_t eocd64[56];
    if (fread(eocd64, 1, 56, apk) != 56) return false;

    /* Validate ZIP64 EOCD magic: 0x06064b50 */
    if (read_le32(eocd64) != 0x06064b50) return false;

    /* Read 64-bit values from ZIP64 EOCD record:
     *   +32: total number of entries (8 bytes)
     *   +40: size of central directory (8 bytes)
     *   +48: offset of central directory (8 bytes) */
    uint64_t cd_entries64 = (uint64_t)read_le32(eocd64 + 32)
                          | ((uint64_t)read_le32(eocd64 + 36) << 32);
    uint64_t cd_size64 = (uint64_t)read_le32(eocd64 + 40)
                       | ((uint64_t)read_le32(eocd64 + 44) << 32);
    uint64_t cd_offset64 = (uint64_t)read_le32(eocd64 + 48)
                         | ((uint64_t)read_le32(eocd64 + 52) << 32);

    if (cd_offset64 >= (uint64_t)file_size) return false;
    if (cd_offset64 + cd_size64 > (uint64_t)eocd_offset) return false;

    *out_cd_offset = cd_offset64;
    *out_cd_entries = (uint32_t)cd_entries64;
    return true;
}

/**
 * Validate EOCD header and extract central directory location + entry count.
 * Uses cd_offset + cd_size <= eocd_offset (minizip/libzip standard pattern).
 * Supports ZIP64 (APKs >4 GiB) by detecting the 0xFFFFFFFF sentinel and
 * parsing the ZIP64 EOCD Locator + ZIP64 EOCD Record to obtain 64-bit values.
 * Returns true if EOCD (or ZIP64 EOCD) is valid.
 */
static bool zip_validate_eocd(FILE* apk, off64_t eocd_offset,
                               off64_t file_size,
                               uint64_t* out_cd_offset,
                               uint32_t* out_cd_entries) {
    uint8_t buf[22];
    if (fseeko64(apk, eocd_offset, SEEK_SET)) return false;
    if (fread(buf, 1, 22, apk) != 22) return false;

    /* Validate: comment bytes may contain false-positive magic (0x06054b50) */
    if (eocd_offset + 22 + read_le16(buf + 20) != file_size) return false;

    uint32_t cd_offset32 = read_le32(buf + 16);
    uint32_t cd_size = read_le32(buf + 12);
    uint16_t cd_entries = read_le16(buf + 10);

    uint16_t num_this_disk = read_le16(buf + 4);
    uint16_t num_disk_cd = read_le16(buf + 6);
    if (num_this_disk != 0 || num_disk_cd != 0) return false;  /* No multi-disk support */

    /* ZIP64 support: when cd_offset32 == 0xFFFFFFFF, the standard EOCD's
     * 32-bit offset is a sentinel; the real values are in the ZIP64 EOCD
     * record, located via the ZIP64 EOCD Locator (20 bytes before EOCD).
     * Delegate to zip_validate_eocd_zip64() for the detailed validation. */
    if (cd_offset32 == 0xFFFFFFFF) {
        return zip_validate_eocd_zip64(apk, eocd_offset, file_size,
                                       out_cd_offset, out_cd_entries);
    }

    /* Standard (non-ZIP64) validation */
    if (cd_offset32 >= (uint64_t)file_size) return false;

    /* Validate Central Directory fits entirely before EOCD */
    if ((uint64_t)cd_offset32 + cd_size > (uint64_t)eocd_offset) return false;

    *out_cd_offset = cd_offset32;
    *out_cd_entries = cd_entries;
    return true;
}

/**
 * Search the Central Directory for a specific entry name.
 * Returns the local file header offset and compression method.
 */
static bool zip_find_entry_in_cd(FILE* apk, uint64_t cd_offset,
                                  uint32_t cd_entries,
                                  const char* entry_name,
                                  uint64_t* out_local_off,
                                  uint16_t* out_comp_method) {
    uint8_t cd_entry[46];
    uint64_t current = cd_offset;

    for (uint32_t i = 0; i < cd_entries; i++) {
        if (fseeko64(apk, current, SEEK_SET)) return false;
        if (fread(cd_entry, 1, 46, apk) != 46) return false;
        if (read_le32(cd_entry) != ZIP_CENTRAL_MAGIC) return false;

        uint16_t fname_len = read_le16(cd_entry + 28);
        uint16_t extra_len = read_le16(cd_entry + 30);
        uint16_t comment_len = read_le16(cd_entry + 32);
        current += 46 + fname_len + extra_len + comment_len;

        char fname[1024];
        if (fname_len >= sizeof(fname)) {
            __android_log_print(ANDROID_LOG_WARN, "MemKit",
                                "ZIP entry filename too long (%u bytes, max %zu), skipping",
                                fname_len, sizeof(fname));
            continue;  /* skip entry and continue searching */
        }
        if (fread(fname, 1, fname_len, apk) != fname_len) return false;
        fname[fname_len] = 0;

        if (strcmp(fname, entry_name) == 0) {
            *out_local_off = read_le32(cd_entry + 42);
            *out_comp_method = read_le16(cd_entry + 10);
            return true;
        }
    }

    return false;
}

/**
 * Read the Local File Header at the given offset and compute the data offset.
 * Returns 0 on failure.
 */
static uint64_t zip_read_local_data_offset(FILE* apk, uint64_t local_off,
                                            off64_t file_size) {
    if (fseeko64(apk, local_off, SEEK_SET)) return 0;

    uint8_t lfh[30];
    if (fread(lfh, 1, 30, apk) != 30) return 0;
    if (read_le32(lfh) != ZIP_LOCAL_HEADER_MAGIC) return 0;

    uint16_t lfname_len = read_le16(lfh + 26);
    uint16_t lextra_len = read_le16(lfh + 28);
    uint64_t data_file_off = local_off + 30 + lfname_len + lextra_len;
    if (data_file_off >= (uint64_t)file_size) return 0;

    return data_file_off;
}

/**
 * Find the data offset of a ZIP entry by parsing the APK's central directory.
 * Returns the offset of the entry's file data within the ZIP, or 0 if not found
 * or if the entry is deflated (not mappable in-place).
 */
static uint64_t zip_find_entry_data_offset(const char* apk_path, const char* entry_name) {
    FILE* apk = fopen(apk_path, "rb");
    if (!apk) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: cannot open", apk_path);
        return 0;
    }

    if (fseeko64(apk, 0, SEEK_END)) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: seek failed", apk_path);
        goto fail;
    }
    off64_t file_size = ftello64(apk);
    if (file_size < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "MemKit",
                            "APK %s: ftello64 failed, errno=%d", apk_path, errno);
        goto fail;
    }

    off64_t eocd_offset = zip_find_eocd(apk, file_size);
    if (eocd_offset < 0) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: EOCD not found", apk_path);
        goto fail;
    }

    uint64_t cd_offset = 0;
    uint32_t cd_entries = 0;
    if (!zip_validate_eocd(apk, eocd_offset, file_size, &cd_offset, &cd_entries)) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: invalid EOCD at offset %" PRId64,
                            apk_path, (int64_t)eocd_offset);
        goto fail;
    }

    uint64_t local_off = 0;
    uint16_t comp_method = 0;
    if (!zip_find_entry_in_cd(apk, cd_offset, cd_entries, entry_name,
                               &local_off, &comp_method)) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: entry '%s' not found in central directory",
                            apk_path, entry_name);
        goto fail;
    }

    uint64_t data_file_off = zip_read_local_data_offset(apk, local_off, file_size);
    if (data_file_off == 0) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: invalid local header at offset %" PRIu64,
                            apk_path, local_off);
        goto fail;
    }

    /* Deflated libs are NOT mapped in-place in the APK */
    if (comp_method != 0) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "APK %s: entry '%s' is compressed (method %u), skipping",
                            apk_path, entry_name, comp_method);
        fclose(apk);
        return 0;
    }

    fclose(apk);
    return data_file_off;

fail:
    fclose(apk);
    return 0;
}

/**
 * Extract the next line from a /proc/self/maps buffer.
 * Updates @p pos to point past the newline (or NULL if end of buffer).
 * Returns true if a line was extracted, false if end of buffer.
 */
static bool next_maps_line(const char** pos, const char* end,
                            char* line_out, size_t line_sz) {
    if (!pos || !*pos || *pos >= end) return false;
    const char* nl = strchr(*pos, '\n');
    size_t line_len = nl ? (size_t)(nl - *pos) : (size_t)(end - *pos);
    if (line_len >= line_sz) {
        line_len = line_sz - 1;
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "maps line truncated at %zu bytes", line_sz - 1);
    }
    memcpy(line_out, *pos, line_len);
    line_out[line_len] = '\0';
    *pos = nl ? nl + 1 : NULL;
    return true;
}

/**
 * Match an APK file offset against a maps buffer to find the base address.
 * Given an APK path and a data offset within the APK, scans a pre-read
 * /proc/self/maps buffer for the corresponding executable mapping and
 * calculates the runtime base address.
 *
 * @param maps_buf    Pre-read /proc/self/maps buffer (avoids TOCTOU race)
 * @param maps_len    Length of maps_buf (avoids O(n) strlen re-scan)
 */
static uintptr_t match_apk_offset_in_maps(const char* apk_path,
                                            uint64_t data_offset,
                                            const char* maps_buf,
                                            size_t maps_len) {
    if (!maps_buf) return 0;

    const char* p = maps_buf;
    const char* end = maps_buf + maps_len;
    char line[1024];
    uintptr_t base = 0;

    while (next_maps_line(&p, end, line, sizeof(line))) {
        if (strstr(line, apk_path) && strstr(line, "r-xp")) {
            uintptr_t map_start = 0, map_end = 0, map_off = 0;
            if (sscanf(line, "%" PRIxPTR "-%" PRIxPTR " %*s %" PRIxPTR, &map_start, &map_end, &map_off) == 3) {
                uintptr_t aligned_off = (uintptr_t)(data_offset & ~(uint64_t)0xFFF);
                if (map_off == aligned_off) {
                    /*
                     * Maps entry: "72000000-73000000 r-xp 00005000 ... base.apk"
                     * VA 0x72000000 maps to APK file offset 0x5000 (page-aligned).
                     * For data at APK offset 0x5234:
                     *   aligned_off = 0x5000,  data_offset - aligned_off = 0x234
                     *   base = 0x72000000 + 0x234 = 0x72000234  ✓
                     * Subtraction (old code) gave 0x71FFFFDC — outside any valid
                     * mapping and would cause SIGSEGV.
                     * Reference: AOSP linker ElfReader::MapSegment always uses
                     * addition for file-offset-to-VA translation.
                     */
                    base = map_start + (uintptr_t)(data_offset - aligned_off);
                    break;
                }
            }
        }
    }

    return base;
}

static uintptr_t find_base_in_apk(const char* apk_path, const char* lib_entry,
                                   const char* maps_buf, size_t maps_len) {
    uint64_t data_offset = zip_find_entry_data_offset(apk_path, lib_entry);
    if (!data_offset) return 0;
    return match_apk_offset_in_maps(apk_path, data_offset, maps_buf, maps_len);
}

// ============================================================================
// ENHANCED: Find library base via APK ZIP parsing
//
// Scans all APK files in process maps (base.apk, split_*.apk) for the
// requested library. This handles games that load .so files directly
// from split APKs without extraction (Android 12+ behavior).
//
// @param lib_entry    Library entry path within APK (e.g., "lib/arm64-v8a/libMyGame.so")
// @param out_base     Output: base address if found
// @return             true if found, false otherwise
// ============================================================================

/* Reduced from 128 to 32: real-world processes have 1-10 APK mappings;
 * 32 is more than enough and keeps stack usage at 8 KB. */
enum { SEEN_APK_MAX = 32 };

/**
 * Scan a pre-read /proc/self/maps buffer for executable APK mappings,
 * deduplicate APK paths, and attempt to find the requested library
 * in each unique APK via ZIP central directory parsing.
 *
 * Extracted as a helper to keep memkit_get_lib_base_in_apk focused on
 * orchestration (read maps → scan → return).
 */
static bool scan_apk_maps(const char* maps_buf, size_t maps_len,
                           const char* lib_entry, uintptr_t* out_base) {
    char seen_apks[SEEN_APK_MAX][256];
    int seen_count = 0;

    const char* p = maps_buf;
    const char* end = maps_buf + maps_len;
    char line[1024];

    while (next_maps_line(&p, end, line, sizeof(line))) {
        if (strstr(line, ".apk") && strstr(line, "r-xp")) {
            char apk_path[256] = {0};
            /* Path is the last field in /proc/self/maps lines; extract via strrchr
             * instead of a fragile sscanf with many %* skipped fields. */
            const char* last_space = strrchr(line, ' ');
            if (!last_space) continue;
            snprintf(apk_path, sizeof(apk_path), "%s", last_space + 1);

            /* Skip if already scanned */
            bool skip = false;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen_apks[i], apk_path) == 0) { skip = true; break; }
            }
            if (skip) continue;

            /* Record as scanned (via snprintf — safe, always null-terminates) */
            if (seen_count < SEEN_APK_MAX) {
                snprintf(seen_apks[seen_count], sizeof(seen_apks[0]), "%s", apk_path);
                seen_count++;
            } else {
                __android_log_print(ANDROID_LOG_WARN, "MemKit",
                                    "seen_apks overflow (>%d APKs)", SEEN_APK_MAX);
                continue;  /* Dedup array full — skip ZIP parsing for remaining APKs */
            }

            /* Try to find the library in this APK */
            uintptr_t base = find_base_in_apk(apk_path, lib_entry, maps_buf, maps_len);
            if (base) {
                *out_base = base;
                return true;
            }
        }
    }

    return false;
}

/**
 * Find library base by parsing APK ZIP files in process maps.
 * Handles games that load .so files directly from split APKs
 * without extraction (Android 12+ behavior).
 *
 * @param lib_entry  Library entry path within APK (e.g., "lib/arm64-v8a/libMyGame.so")
 * @param out_base   Output: base address if found
 * @return           true if found, false otherwise
 */
bool memkit_get_lib_base_in_apk(const char* lib_entry, uintptr_t* out_base) {
    if (!lib_entry || !out_base) return false;

    /* Read /proc/self/maps once into a buffer to eliminate TOCTOU race.
     * Heap-allocated to avoid ~64 KB stack allocation (signal-handler stacks
     * can be as low as 80 KB; Android NDK best practices mandate heap for
     * buffers this large). */
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) return false;

    /* Determine file size for dynamic allocation */
    if (fseeko64(maps, 0, SEEK_END) != 0) { fclose(maps); return false; }
    off64_t maps_fsize = ftello64(maps);
    if (maps_fsize <= 0) { fclose(maps); return false; }
    rewind(maps);

    char* maps_buf = (char*)malloc((size_t)maps_fsize + 1);
    if (!maps_buf) { fclose(maps); return false; }

    size_t maps_len = fread(maps_buf, 1, (size_t)maps_fsize, maps);
    fclose(maps);
    if (maps_len == 0) { free(maps_buf); return false; }
    maps_buf[maps_len] = '\0';

    /* Warn if the buffer may have been truncated */
    if (maps_len < (size_t)maps_fsize) {
        __android_log_print(ANDROID_LOG_WARN, "MemKit",
                            "/proc/self/maps may be truncated (read %zu of %lld bytes)",
                            maps_len, (long long)maps_fsize);
    }

    bool found = scan_apk_maps(maps_buf, maps_len, lib_entry, out_base);
    free(maps_buf);
    return found;
}

// ============================================================================
// ENHANCED: Get library base address (v2 — tries all methods)
//
// Tries three methods in order:
//   1. /proc/self/maps (fast, works for normal .so loading)
//   2. xdl_iterate_phdr (linker internals, finds libs from APKs)
//      Uses dlsym(RTLD_DEFAULT) — gracefully skipped if xdl_wrapper.c
//      is not linked.
//   3. APK ZIP parsing (last resort, for libs mapped in-place from split APKs)
//
// @param lib_name   Library name pattern (e.g., "libMyGame.so")
//                   For APK fallback, the pattern is used as-is in strstr.
//                   If not found, the function also tries each ABI prefix
//                   in priority order: arm64-v8a, armeabi-v7a, x86_64, x86,
//                   riscv64 (see abi_priorities below).
// @return           Base address, or 0 if not found
// ============================================================================

/* Weak symbol for optional xdl-based lookup (defined in xdl_wrapper.c).
 * Resolves to NULL at link time if xdl_wrapper.c is not linked;
 * checked before calling in Method 2 below. */
__attribute__((weak)) uintptr_t memkit_get_lib_base_from_xdl(const char* lib_name);

uintptr_t memkit_get_lib_base_v2(const char* lib_name) {
    if (!lib_name) return 0;

    /* Method 1: /proc/self/maps (existing, fast path) */
    uintptr_t base = memkit_get_lib_base(lib_name);
    if (base) return base;

    /* Method 2: xdl_iterate_phdr (linker internals) — optional dependency.
     * Weak symbol resolves to NULL if memkit_get_lib_base_from_xdl is not
     * linked (e.g., if xdl_wrapper.c is excluded at build time). */
    if (memkit_get_lib_base_from_xdl) {
        base = memkit_get_lib_base_from_xdl(lib_name);
        if (base) return base;
    }

    /* Method 3: APK ZIP parsing — try ABIs in priority order */
    /* ABI priorities in order — matching Android's preferred ABI selection.
     * riscv64 is included as forward-looking for Android RISC-V support. */
    static const char* abi_priorities[] = {
        "lib/arm64-v8a",
        "lib/armeabi-v7a",
        "lib/x86_64",
        "lib/x86",
        "lib/riscv64",
    };
    char entry[512];
    for (size_t i = 0; i < sizeof(abi_priorities) / sizeof(abi_priorities[0]); i++) {
        int n = snprintf(entry, sizeof(entry), "%s/%s", abi_priorities[i], lib_name);
        if (n > 0 && n < (int)sizeof(entry)) {
            if (memkit_get_lib_base_in_apk(entry, &base)) return base;
        }
    }

    return 0;
}
