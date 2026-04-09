#include "memkit.h"
#include "shadowhook.h"

// ============================================================================
// RUNTIME CONFIG: MODE
// ============================================================================

int memkit_get_mode(void) {
    return (int)shadowhook_get_mode();
}

// ============================================================================
// RUNTIME CONFIG: DEBUGGABLE
// ============================================================================

void memkit_set_debuggable(bool debuggable) {
    shadowhook_set_debuggable(debuggable);
}

bool memkit_get_debuggable(void) {
    return shadowhook_get_debuggable();
}

// ============================================================================
// RUNTIME CONFIG: RECORDABLE
// ============================================================================

void memkit_set_recordable(bool recordable) {
    shadowhook_set_recordable(recordable);
}

bool memkit_get_recordable(void) {
    return shadowhook_get_recordable();
}

// ============================================================================
// RUNTIME CONFIG: DISABLE
// ============================================================================

void memkit_set_disable(bool disable) {
    shadowhook_set_disable(disable);
}

bool memkit_get_disable(void) {
    return shadowhook_get_disable();
}
