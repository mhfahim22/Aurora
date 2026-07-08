// Auto-generated Cargo bridge for json@0.12.4
// Provides free-function + type-instance FFI API

use std::ffi::{CStr, CString, c_void};
use std::collections::HashMap;
use std::sync::Mutex;
use std::cell::RefCell;

thread_local! {
    static LAST_ERROR: RefCell<String> = const { RefCell::new(String::new()) };
}

fn set_last_error(s: String) {
    LAST_ERROR.with(|e| *e.borrow_mut() = s);
}

fn __deser<T: serde::de::DeserializeOwned>(v: serde_json::Value) -> Result<T, String> {
    serde_json::from_value(v).map_err(|e| e.to_string())
}

type RustFn = fn(Vec<serde_json::Value>) -> Result<serde_json::Value, String>;
type MethodFn = fn(*mut c_void, Vec<serde_json::Value>) -> Result<serde_json::Value, String>;
type CtorFn = fn(Vec<serde_json::Value>) -> Result<*mut c_void, String>;
type DropFn = unsafe fn(*mut c_void);

fn registry() -> &'static Mutex<HashMap<String, RustFn>> {
    static REG: std::sync::OnceLock<Mutex<HashMap<String, RustFn>>> = std::sync::OnceLock::new();
    REG.get_or_init(|| {
        let m: HashMap<String, RustFn> = HashMap::new();
        Mutex::new(m)
    })
}

fn type_registry() -> &'static Mutex<HashMap<String, HashMap<String, MethodFn>>> {
    static REG: std::sync::OnceLock<Mutex<HashMap<String, HashMap<String, MethodFn>>>> = std::sync::OnceLock::new();
    REG.get_or_init(|| {
        let mut type_map: HashMap<String, HashMap<String, MethodFn>> = HashMap::new();
    // ── json::JsonValue methods ──
    {
        let tn = "JsonValue".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("dump".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.dump();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("pretty".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.pretty(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_string".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_string();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_number".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_number();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_boolean".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_boolean();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_null".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_null();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_object".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_object();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_array".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_array();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_empty".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.is_empty();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_str".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_str();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_number".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_number();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_f64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_f64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_f32".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_f32();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_u64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u32".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_u32();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u16".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_u16();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u8".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_u8();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_usize".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_usize();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_i64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i32".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_i32();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i16".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_i16();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i8".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_i8();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_isize".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_isize();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_bool".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.as_bool();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_fixed_point_u64".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.as_fixed_point_u64(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_fixed_point_i64".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.as_fixed_point_i64(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("take".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let __result = this_ref.take();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("take_string".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let __result = this_ref.take_string();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("pop".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let __result = this_ref.pop();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("has_key".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.has_key(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("len".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.len();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("members".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.members();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("members_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let __result = this_ref.members_mut();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("entries".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::JsonValue) };
            let __result = this_ref.entries();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("entries_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let __result = this_ref.entries_mut();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("remove".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.remove(a0);
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("array_remove".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.array_remove(a0);
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("clear".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::JsonValue) };
            let __result = this_ref.clear();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── json::Number methods ──
    {
        let tn = "Number".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("as_parts".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let __result = this_ref.as_parts();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("is_sign_positive".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let __result = this_ref.is_sign_positive();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_zero".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let __result = this_ref.is_zero();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_nan".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let __result = this_ref.is_nan();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_empty".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let __result = this_ref.is_empty();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_fixed_point_u64".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.as_fixed_point_u64(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_fixed_point_i64".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Number) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.as_fixed_point_i64(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── json::Object methods ──
    {
        let tn = "Object".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("insert".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::Object) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let __result = this_ref.insert(a0, a1);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("override_last".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::Object) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.override_last(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("get".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Object) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.get(a0);
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("get_mut".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::Object) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.get_mut(a0);
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("remove".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::Object) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.remove(a0);
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("len".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Object) };
            let __result = this_ref.len();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_empty".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Object) };
            let __result = this_ref.is_empty();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("clear".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::Object) };
            let __result = this_ref.clear();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("iter".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Object) };
            let __result = this_ref.iter();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("iter_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut json::Object) };
            let __result = this_ref.iter_mut();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("dump".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Object) };
            let __result = this_ref.dump();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("pretty".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Object) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.pretty(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── json::Short methods ──
    {
        let tn = "Short".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("as_str".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const json::Short) };
            let __result = this_ref.as_str();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }

        Mutex::new(type_map)
    })
}

fn ctor_registry() -> &'static Mutex<HashMap<String, HashMap<String, CtorFn>>> {
    static REG: std::sync::OnceLock<Mutex<HashMap<String, HashMap<String, CtorFn>>>> = std::sync::OnceLock::new();
    REG.get_or_init(|| {
        let mut ctor_map: HashMap<String, HashMap<String, CtorFn>> = HashMap::new();
    // ── json::DumpGenerator constructors ──
    {
        let tn = "DumpGenerator".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("new".to_string(), |_args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let __val = json::DumpGenerator::new();
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── json::Error constructors ──
    {
        let tn = "Error".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("wrong_type".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __val = json::Error::wrong_type(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── json::JsonValue constructors ──
    {
        let tn = "JsonValue".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("new_object".to_string(), |_args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let __val = json::JsonValue::new_object();
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("new_array".to_string(), |_args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let __val = json::JsonValue::new_array();
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── json::Number constructors ──
    {
        let tn = "Number".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("from_parts".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let __val = json::Number::from_parts(a0, a1, a2);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── json::Object constructors ──
    {
        let tn = "Object".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("new".to_string(), |_args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let __val = json::Object::new();
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("with_capacity".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = json::Object::with_capacity(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── json::PrettyGenerator constructors ──
    {
        let tn = "PrettyGenerator".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("new".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = json::PrettyGenerator::new(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
        Mutex::new(ctor_map)
    })
}

fn drop_registry() -> &'static Mutex<HashMap<String, DropFn>> {
    static REG: std::sync::OnceLock<Mutex<HashMap<String, DropFn>>> = std::sync::OnceLock::new();
    REG.get_or_init(|| {
        let mut m: HashMap<String, DropFn> = HashMap::new();
    m.insert("DumpGenerator".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::DumpGenerator)); };
        f
    });
    m.insert("Error".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::Error)); };
        f
    });
    m.insert("JsonValue".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::JsonValue)); };
        f
    });
    m.insert("Number".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::Number)); };
        f
    });
    m.insert("Object".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::Object)); };
        f
    });
    m.insert("PrettyGenerator".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::PrettyGenerator)); };
        f
    });
    m.insert("Short".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut json::Short)); };
        f
    });
        Mutex::new(m)
    })
}

fn store(v: serde_json::Value) -> *mut c_void {
    Box::into_raw(Box::new(v)) as *mut c_void
}

unsafe fn retrieve<'a>(ptr: *mut c_void) -> &'a mut serde_json::Value {
    &mut *(ptr as *mut serde_json::Value)
}

#[no_mangle]
pub extern "C" fn rust_bridge_get_fns() -> *mut HashMap<String, RustFn> {
    Box::into_raw(Box::new(registry().lock().unwrap().clone()))
}

#[no_mangle]
pub extern "C" fn rust_bridge_type_registry() -> *mut HashMap<String, HashMap<String, MethodFn>> {
    Box::into_raw(Box::new(type_registry().lock().unwrap().clone()))
}

#[no_mangle]
pub extern "C" fn json_import() -> *mut c_void {
    registry(); // ensure initialized
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn json_free(ptr: *mut c_void) {
    if !ptr.is_null() {
        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }
    }
}

#[no_mangle]
pub extern "C" fn json_free_cstr(ptr: *mut std::ffi::c_char) {
    if !ptr.is_null() {
        unsafe { drop(CString::from_raw(ptr)); }
    }
}

#[no_mangle]
pub extern "C" fn json_str(s: *const std::ffi::c_char) -> *mut c_void {
    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();
    store(serde_json::Value::String(s))
}

#[no_mangle]
pub extern "C" fn json_int(v: i64) -> *mut c_void {
    store(serde_json::Value::Number(v.into()))
}

#[no_mangle]
pub extern "C" fn json_float(v: f64) -> *mut c_void {
    store(serde_json::Value::Number(
        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))
}

#[no_mangle]
pub extern "C" fn json_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {
    let mut vec = Vec::with_capacity(count as usize);
    for i in 0..count {
        let ptr = unsafe { *items.offset(i as isize) };
        if ptr.is_null() {
            vec.push(serde_json::Value::Null);
        } else {
            vec.push(unsafe { retrieve(ptr) }.clone());
        }
    }
    store(serde_json::Value::Array(vec))
}

#[no_mangle]
pub extern "C" fn json_list(items: *mut *mut c_void, count: i32) -> *mut c_void {
    let mut vec = Vec::with_capacity(count as usize);
    for i in 0..count {
        let ptr = unsafe { *items.offset(i as isize) };
        if ptr.is_null() {
            vec.push(serde_json::Value::Null);
        } else {
            vec.push(unsafe { retrieve(ptr) }.clone());
        }
    }
    store(serde_json::Value::Array(vec))
}

#[no_mangle]
pub extern "C" fn json_tuple2(a: *mut c_void, b: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn json_tuple3(a: *mut c_void, b: *mut c_void, c: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn json_tuple4(a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
        if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn json_list2(a: *mut c_void, b: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn json_list3(a: *mut c_void, b: *mut c_void, c: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn json_list4(a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
        if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn json_dict() -> *mut c_void {
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn json_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {
    if d.is_null() || key.is_null() { return -1; }
    let k = match unsafe { CStr::from_ptr(key) }.to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return -1,
    };
    let v = if val.is_null() { serde_json::Value::Null } else { unsafe { retrieve(val) }.clone() };
    match unsafe { retrieve(d) } {
        serde_json::Value::Object(map) => { map.insert(k, v); 0 },
        _ => -1,
    }
}

#[no_mangle]
pub extern "C" fn json_to_cstr(obj: *mut c_void) -> *mut std::ffi::c_char {
    if obj.is_null() { return std::ptr::null_mut(); }
    let val = unsafe { retrieve(obj) };
    let s = serde_json::to_string(val).unwrap_or_else(|_| "null".to_string());
    CString::new(s).unwrap_or_default().into_raw()
}

#[no_mangle]
pub extern "C" fn json_getattr(obj: *mut c_void, name: *const std::ffi::c_char) -> *mut c_void {
    if obj.is_null() || name.is_null() { return std::ptr::null_mut(); }
    let key = match unsafe { CStr::from_ptr(name) }.to_str() { Ok(s) => s, Err(_) => return std::ptr::null_mut() };
    match unsafe { retrieve(obj) } {
        serde_json::Value::Object(map) => match map.get(key) {
            Some(v) => store(v.clone()),
            None => std::ptr::null_mut(),
        },
        _ => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn json_call(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    args: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = if args.is_null() { vec![] } else { match unsafe { retrieve(args) } { serde_json::Value::Array(a) => a.clone(), _ => { set_last_error("args not array".into()); return std::ptr::null_mut(); } } };
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call1(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    a: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }];
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call2(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    a: *mut c_void, b: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }];
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call3(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    a: *mut c_void, b: *mut c_void, c: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }];
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call4(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }];
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call5(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void, e: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }, if e.is_null() { serde_json::Value::Null } else { unsafe { retrieve(e) }.clone() }];
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call6(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void, e: *mut c_void, f: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let args_vec = vec![if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() }, if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() }, if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() }, if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() }, if e.is_null() { serde_json::Value::Null } else { unsafe { retrieve(e) }.clone() }, if f.is_null() { serde_json::Value::Null } else { unsafe { retrieve(f) }.clone() }];
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call_kw(
    _handle: *mut c_void,
    fn_name: *const std::ffi::c_char,
    args: *mut c_void,
    kwargs: *mut c_void,
) -> *mut c_void {
    let name = match unsafe { CStr::from_ptr(fn_name) }.to_str() {
        Ok(s) => s,
        Err(_) => { set_last_error("invalid fn_name".into()); return std::ptr::null_mut(); }
    };
    let mut args_vec = if args.is_null() {
        vec![]
    } else {
        match unsafe { retrieve(args) } {
            serde_json::Value::Array(a) => a.clone(),
            _ => { set_last_error("args not array".into()); return std::ptr::null_mut(); }
        }
    };
    if !kwargs.is_null() {
        args_vec.push(unsafe { retrieve(kwargs) }.clone());
    }
    let reg = registry().lock().unwrap();
    match reg.get(name) {
        Some(f) => match f(args_vec) { Ok(v) => store(v), Err(e) => { set_last_error(e); std::ptr::null_mut() } },
        None => { set_last_error(format!("unknown function: {}", name)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_construct(
    type_name: *const std::ffi::c_char,
    ctor_name: *const std::ffi::c_char,
    args: *mut c_void,
) -> *mut c_void {
    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {
        Ok(s) => s, Err(_) => { set_last_error("invalid type_name".into()); return std::ptr::null_mut(); }
    };
    let cn = match unsafe { CStr::from_ptr(ctor_name) }.to_str() {
        Ok(s) => s, Err(_) => { set_last_error("invalid ctor_name".into()); return std::ptr::null_mut(); }
    };
    let args_vec = if args.is_null() { vec![] } else {
        match unsafe { retrieve(args) } {
            serde_json::Value::Array(a) => a.clone(),
            _ => { set_last_error("args not array".into()); return std::ptr::null_mut(); }
        }
    };
    let reg = ctor_registry().lock().unwrap();
    match reg.get(tn).and_then(|ctors| ctors.get(cn)) {
        Some(f) => match f(args_vec) {
            Ok(ptr) => ptr,
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("no constructor {}::{}", tn, cn)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_call_method(
    handle: *mut c_void,
    type_name: *const std::ffi::c_char,
    method: *const std::ffi::c_char,
    args: *mut c_void,
) -> *mut c_void {
    if handle.is_null() { set_last_error("null handle".into()); return std::ptr::null_mut(); }
    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {
        Ok(s) => s, Err(_) => { set_last_error("invalid type_name".into()); return std::ptr::null_mut(); }
    };
    let mn = match unsafe { CStr::from_ptr(method) }.to_str() {
        Ok(s) => s, Err(_) => { set_last_error("invalid method".into()); return std::ptr::null_mut(); }
    };
    let args_vec = if args.is_null() { vec![] } else {
        match unsafe { retrieve(args) } {
            serde_json::Value::Array(a) => a.clone(),
            _ => { set_last_error("args not array".into()); return std::ptr::null_mut(); }
        }
    };
    let reg = type_registry().lock().unwrap();
    match reg.get(tn).and_then(|methods| methods.get(mn)) {
        Some(f) => match f(handle, args_vec) {
            Ok(v) => store(v),
            Err(e) => { set_last_error(e); std::ptr::null_mut() }
        },
        None => { set_last_error(format!("no method {}::{}", tn, mn)); std::ptr::null_mut() }
    }
}

#[no_mangle]
pub extern "C" fn json_free_type(handle: *mut c_void, type_name: *const std::ffi::c_char) {
    if handle.is_null() { return; }
    let tn = match unsafe { CStr::from_ptr(type_name) }.to_str() {
        Ok(s) => s,
        Err(_) => { set_last_error("invalid type_name".into()); return; }
    };
    let reg = drop_registry().lock().unwrap();
    if let Some(drop_fn) = reg.get(tn) {
        unsafe { drop_fn(handle); }
    } else {
        set_last_error(format!("unknown type for drop: {}", tn));
    }
}

#[no_mangle]
pub extern "C" fn json_last_error() -> *mut std::ffi::c_char {
    LAST_ERROR.with(|e| {
        let s = e.borrow().clone();
        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }
    })
}
