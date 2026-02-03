#include <stdint.h>
#include <string>
#include <vector>

#include "MemoryPatch.hpp"
#include "KittyMemory.hpp"
#include "xdl.h"

extern "C" {

    // --- WRAPPER KITTYMEMORY (PATCH) ---
    void* bridge_create_patch_hex(uintptr_t absolute_address, const char* hex_code) {
        if (!hex_code) return nullptr;
        
        MemoryPatch* patch = new MemoryPatch();
        *patch = MemoryPatch::createWithHex(absolute_address, std::string(hex_code));
        
        if (!patch->isValid()) {
            delete patch;
            return nullptr;
        }
        return (void*)patch;
    }

    int bridge_patch_apply(void* patch_ptr) {
        if (!patch_ptr) return 0;
        MemoryPatch* patch = (MemoryPatch*)patch_ptr;
        return patch->Modify() ? 1 : 0;
    }

    int bridge_patch_restore(void* patch_ptr) {
        if (!patch_ptr) return 0;
        MemoryPatch* patch = (MemoryPatch*)patch_ptr;
        return patch->Restore() ? 1 : 0;
    }

    void bridge_patch_delete(void* patch_ptr) {
        if (patch_ptr) {
            delete static_cast<MemoryPatch*>(patch_ptr);
        }
    }

    // --- ANDROID: ITERASI MAPS MANUAL ---
    uintptr_t bridge_get_module_base(const char* lib_name) {
        if (lib_name == nullptr) return 0;
        std::string target_name(lib_name);

        // Ambil semua maps (Return type: std::vector<KittyMemory::ProcMap>)
        std::vector<KittyMemory::ProcMap> maps = KittyMemory::getAllMaps();
        
        // Loop manual untuk mencari library yang cocok
        for (const auto& map : maps) {
            // Cek apakah nama map mengandung string lib_name yang dicari
            // pakai 'find' karena map.name biasanya full path
            if (map.pathname.find(target_name) != std::string::npos) {
                return (uintptr_t)map.startAddress;
            }
        }
        
        return 0; // Tidak ketemu
    }

    // --- WRAPPER XDL ---
    
    void* bridge_xdl_sym(void* handle, const char* symbol) {
        return xdl_sym(handle, symbol, nullptr);
    }
    
    void* bridge_xdl_open(const char* filename) {
        return xdl_open(filename, XDL_DEFAULT);
    }
}