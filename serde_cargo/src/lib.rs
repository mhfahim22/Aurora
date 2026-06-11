// Manual bridge scaffold for serde@1.0.228
// Fill in each #[no_mangle] extern "C" fn below with real logic.
// Each fn returns *mut c_void → a heap-allocated serde_json::Value.

use std::ffi::{CStr, CString, c_void};
use std::sync::Mutex;
use std::collections::HashMap;

thread_local! {
    static LAST_ERROR: std::cell::RefCell<String> = const { std::cell::RefCell::new(String::new()) };
}
fn set_last_error(s: String) {
    LAST_ERROR.with(|e| *e.borrow_mut() = s);
}

fn store(v: serde_json::Value) -> *mut c_void {
    Box::into_raw(Box::new(v)) as *mut c_void
}
unsafe fn retrieve<'a>(ptr: *mut c_void) -> &'a mut serde_json::Value {
    &mut *(ptr as *mut serde_json::Value)
}

#[no_mangle]
pub extern "C" fn serde_import() -> *mut c_void {
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn serde_free(ptr: *mut c_void) {
    if !ptr.is_null() {
        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }
    }
}

#[no_mangle]
pub extern "C" fn serde_str(s: *const std::ffi::c_char) -> *mut c_void {
    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();
    store(serde_json::Value::String(s))
}

#[no_mangle]
pub extern "C" fn serde_int(v: i64) -> *mut c_void {
    store(serde_json::Value::Number(v.into()))
}

#[no_mangle]
pub extern "C" fn serde_float(v: f64) -> *mut c_void {
    store(serde_json::Value::Number(
        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))
}

#[no_mangle]
pub extern "C" fn serde_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {
    let mut vec = Vec::with_capacity(count as usize);
    for i in 0..count {
        let ptr = unsafe { *items.offset(i as isize) };
        vec.push(if ptr.is_null() { serde_json::Value::Null } else { unsafe { retrieve(ptr) }.clone() });
    }
    store(serde_json::Value::Array(vec))
}

#[no_mangle]
pub extern "C" fn serde_dict() -> *mut c_void {
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn serde_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {
    if d.is_null() || val.is_null() { return 0; }
    let k = match unsafe { CStr::from_ptr(key) }.to_str() { Ok(s) => s.to_string(), Err(_) => return 0 };
    let obj = unsafe { retrieve(d) };
    let v = unsafe { retrieve(val) }.clone();
    obj.as_object_mut().map(|o| { o.insert(k, v); 1 }).unwrap_or(0)
}

/* ════════════════════════════════════════════════════════════
   MANUAL WRAPPERS — Replace these stubs with real FFI calls
   ════════════════════════════════════════════════════════════ */

/// Example: call a function that takes (a: i32, b: String) -> bool
/// Delete once you've adapted it to your actual API.
pub fn serde_add_user(args: Vec<serde_json::Value>) -> Result<serde_json::Value, String> {
    // Deserialize positional args
    let name: String = serde_json::from_value(args.get(0).ok_or("arg0 missing")?.clone())
        .map_err(|e| e.to_string())?;
    let age: i32 = serde_json::from_value(args.get(1).ok_or("arg1 missing")?.clone())
        .map_err(|e| e.to_string())?;
    // TODO: call serde::some_function(name, age);
    // For now, return a placeholder:
    Ok(serde_json::json!({"id": 1, "name": name, "age": age}))
}

#[no_mangle]
pub extern "C" fn serde_call(_mod: *mut c_void, fn_name: *const std::ffi::c_char, args: *mut c_void) -> *mut c_void {
    let fname = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s, Err(_) => { set_last_error("invalid fn_name".into()); return std::ptr::null_mut(); }
    };
    let vargs = if args.is_null() { Vec::new() } else { unsafe { retrieve(args) }.as_array().cloned().unwrap_or_default() };
    let result: Result<serde_json::Value, String> = match fname {
        "add_user" => serde_add_user(vargs),
        // TODO: add more function name -> handler mappings
        _ => Err(format!("unknown function: {}", fname)),
    };
    match result {
        Ok(v) => store(v),
        Err(e) => { set_last_error(e); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn serde_last_error() -> *mut std::ffi::c_char {
    LAST_ERROR.with(|e| {
        let s = e.borrow().clone();
        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }
    })
}
