use std::ffi::{CString, c_void};

unsafe extern "C" {
    fn bridge_create_patch_hex(addr: usize, hex: *const i8) -> *mut c_void;
    fn bridge_patch_apply(patch: *mut c_void) -> i32;
    fn bridge_patch_restore(patch: *mut c_void) -> i32;
    fn bridge_patch_delete(patch: *mut c_void);
    fn bridge_get_module_base(name: *const i8) -> usize;
}

pub struct MemoryPatch {
    inner: *mut c_void,
}

unsafe impl Send for MemoryPatch {}
unsafe impl Sync for MemoryPatch {}

impl MemoryPatch {
    pub fn from_hex(address: usize, hex: &str) -> Result<Self, String> {
        let c_hex = CString::new(hex).map_err(|_| "Invalid hex string")?;

        unsafe {
            let ptr = bridge_create_patch_hex(address, c_hex.as_ptr() as *const i8);
            if ptr.is_null() {
                return Err("Failed to create patch (Invalid Hex or Address)".into());
            }
            Ok(Self { inner: ptr })
        }
    }

    pub fn apply(&self) -> bool {
        unsafe { bridge_patch_apply(self.inner) != 0 }
    }

    pub fn restore(&self) -> bool {
        unsafe { bridge_patch_restore(self.inner) != 0 }
    }
}

impl Drop for MemoryPatch {
    fn drop(&mut self) {
        if !self.inner.is_null() {
            unsafe { bridge_patch_delete(self.inner); }
        }
    }
}

pub fn get_lib_base(lib_name: &str) -> usize {
    let c_name = CString::new(lib_name).unwrap_or_default();
    unsafe { bridge_get_module_base(c_name.as_ptr() as *const i8) }
}