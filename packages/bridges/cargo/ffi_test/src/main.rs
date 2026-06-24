use libloading::{Library, Symbol};
use std::ffi::{c_void, CStr, CString};

fn main() {
    unsafe {
        let lib = Library::new("../uuid_cargo/uuid_cargo.dll").unwrap();

        let import: Symbol<unsafe fn() -> *mut c_void> = lib.get(b"uuid_import").unwrap();
        let handle = import();

        type CtorFn = unsafe fn(*const u8, *const u8, *mut c_void) -> *mut c_void;
        let ctor: Symbol<CtorFn> = lib.get(b"uuid_construct").unwrap();
        let tn = CString::new("Uuid").unwrap();
        let cn = CString::new("new_v4").unwrap();
        let uuid = ctor(tn.as_ptr() as *const u8, cn.as_ptr() as *const u8, std::ptr::null_mut());
        println!("uuid ptr: {:p}", uuid);

        let last_err: Symbol<unsafe fn() -> *mut u8> = lib.get(b"uuid_last_error").unwrap();
        let err_ptr = last_err();
        if !err_ptr.is_null() {
            let err = CStr::from_ptr(err_ptr as *const i8).to_str().unwrap().to_string();
            println!("Error: {}", err);
            let free_cstr: Symbol<unsafe fn(*mut u8)> = lib.get(b"uuid_free_cstr").unwrap();
            free_cstr(err_ptr);
        }

        type MethodFn = unsafe fn(*mut c_void, *const u8, *const u8, *mut c_void) -> *mut c_void;
        let method: Symbol<MethodFn> = lib.get(b"uuid_call_method").unwrap();
        let mn = CString::new("as_fields").unwrap();
        let result = method(uuid, tn.as_ptr() as *const u8, mn.as_ptr() as *const u8, std::ptr::null_mut());

        let err_ptr2 = last_err();
        if !err_ptr2.is_null() {
            let err = CStr::from_ptr(err_ptr2 as *const i8).to_str().unwrap().to_string();
            println!("Error: {}", err);
            let free_cstr: Symbol<unsafe fn(*mut u8)> = lib.get(b"uuid_free_cstr").unwrap();
            free_cstr(err_ptr2);
        }

        if !result.is_null() {
            let to_cstr: Symbol<unsafe fn(*mut c_void) -> *mut u8> = lib.get(b"uuid_to_cstr").unwrap();
            let s = CStr::from_ptr(to_cstr(result) as *const i8).to_str().unwrap().to_string();
            println!("UUID: {}", s);
            let free_cstr: Symbol<unsafe fn(*mut u8)> = lib.get(b"uuid_free_cstr").unwrap();
            free_cstr(to_cstr(result));
            let free: Symbol<unsafe fn(*mut c_void)> = lib.get(b"uuid_free").unwrap();
            free(result);
        }

        let free_type: Symbol<unsafe fn(*mut c_void, *const u8)> = lib.get(b"uuid_free_type").unwrap();
        free_type(uuid, tn.as_ptr() as *const u8);
        let free: Symbol<unsafe fn(*mut c_void)> = lib.get(b"uuid_free").unwrap();
        free(handle);
        println!("FFI test PASSED");
    }
}
