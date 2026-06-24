// Auto-generated Cargo bridge for serde_json@1.0.150
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
    // ── serde_json::Deserializer<serde_json::Value> methods ──
    {
        let tn = "Deserializer".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("end".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Deserializer<serde_json::Value>) };
            let __result = this_ref.end();
            match __result {
                Ok(v) => Ok(serde_json::json!(Box::into_raw(Box::new(v)) as usize)),
                Err(e) => Err(e.to_string()),
            }
        });
        methods.insert("disable_recursion_limit".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Deserializer<serde_json::Value>) };
            let __result = this_ref.disable_recursion_limit();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── serde_json::Error methods ──
    {
        let tn = "Error".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("line".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.line();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("column".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.column();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("classify".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.classify();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("is_io".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.is_io();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_syntax".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.is_syntax();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_data".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.is_data();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_eof".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.is_eof();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("io_error_kind".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Error) };
            let __result = this_ref.io_error_kind();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        type_map.insert(tn, methods);
    }
    // ── serde_json::de::LineColIterator methods ──
    {
        let tn = "LineColIterator".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("line".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::de::LineColIterator) };
            let __result = this_ref.line();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("col".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::de::LineColIterator) };
            let __result = this_ref.col();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("byte_offset".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::de::LineColIterator) };
            let __result = this_ref.byte_offset();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── serde_json::Map<String, serde_json::Value> methods ──
    {
        let tn = "Map".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("clear".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.clear();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("len".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.len();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_empty".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.is_empty();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("iter".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.iter();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("iter_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.iter_mut();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("keys".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.keys();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("values".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.values();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("values_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.values_mut();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("sort_keys".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Map<String, serde_json::Value>) };
            let __result = this_ref.sort_keys();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── serde_json::Number methods ──
    {
        let tn = "Number".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("is_i64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.is_i64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_u64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.is_u64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_f64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.is_f64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.as_i64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.as_u64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_f64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.as_f64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i128".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.as_i128();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u128".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.as_u128();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_str".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Number) };
            let __result = this_ref.as_str();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── serde_json::Value methods ──
    {
        let tn = "Value".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("is_object".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_object();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_object".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_object();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_object_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Value) };
            let __result = this_ref.as_object_mut();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_array".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_array();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_array".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_array();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_array_mut".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Value) };
            let __result = this_ref.as_array_mut();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_string".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_string();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_str".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_str();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_number".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_number();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_number".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_number();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_i64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_i64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_u64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_u64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_f64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_f64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_i64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_i64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_u64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_u64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_f64".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_f64();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_boolean".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_boolean();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_bool".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_bool();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("is_null".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.is_null();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("as_null".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let __result = this_ref.as_null();
            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))
        });
        methods.insert("pointer".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const serde_json::Value) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.pointer(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("pointer_mut".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Value) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __result = this_ref.pointer_mut(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("take".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Value) };
            let __result = this_ref.take();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("sort_all_objects".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut serde_json::Value) };
            let __result = this_ref.sort_all_objects();
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
    // ── serde_json::Deserializer<serde_json::Value> constructors ──
    {
        let tn = "Deserializer".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("from_slice".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: Vec<u8> = __deser(a0)?;
            let a0 = &a0_owned[..];
            let __val = serde_json::Deserializer::<serde_json::Value>::from_slice(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("from_str".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0_owned: String = __deser(a0)?;
            let a0 = &a0_owned;
            let __val = serde_json::Deserializer::<serde_json::Value>::from_str(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── serde_json::Map<String, serde_json::Value> constructors ──
    {
        let tn = "Map".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("new".to_string(), |_args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let __val = serde_json::Map::<String, serde_json::Value>::new();
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("with_capacity".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = serde_json::Map::<String, serde_json::Value>::with_capacity(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── serde_json::Number constructors ──
    {
        let tn = "Number".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("from_f64".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = serde_json::Number::from_f64(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("from_i128".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = serde_json::Number::from_i128(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("from_u128".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = serde_json::Number::from_u128(a0);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("from_string_unchecked".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __val = serde_json::Number::from_string_unchecked(a0);
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
    m.insert("Deserializer".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut serde_json::Deserializer<serde_json::Value>)); };
        f
    });
    m.insert("Error".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut serde_json::Error)); };
        f
    });
    m.insert("LineColIterator".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut serde_json::de::LineColIterator)); };
        f
    });
    m.insert("Map".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut serde_json::Map<String, serde_json::Value>)); };
        f
    });
    m.insert("Number".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut serde_json::Number)); };
        f
    });
    m.insert("Value".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); };
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
pub extern "C" fn serde_json_import() -> *mut c_void {
    registry(); // ensure initialized
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn serde_json_free(ptr: *mut c_void) {
    if !ptr.is_null() {
        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }
    }
}

#[no_mangle]
pub extern "C" fn serde_json_free_cstr(ptr: *mut std::ffi::c_char) {
    if !ptr.is_null() {
        unsafe { drop(CString::from_raw(ptr)); }
    }
}

#[no_mangle]
pub extern "C" fn serde_json_str(s: *const std::ffi::c_char) -> *mut c_void {
    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();
    store(serde_json::Value::String(s))
}

#[no_mangle]
pub extern "C" fn serde_json_int(v: i64) -> *mut c_void {
    store(serde_json::Value::Number(v.into()))
}

#[no_mangle]
pub extern "C" fn serde_json_float(v: f64) -> *mut c_void {
    store(serde_json::Value::Number(
        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))
}

#[no_mangle]
pub extern "C" fn serde_json_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {
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
pub extern "C" fn serde_json_list(items: *mut *mut c_void, count: i32) -> *mut c_void {
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
pub extern "C" fn serde_json_tuple2(a: *mut c_void, b: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn serde_json_tuple3(a: *mut c_void, b: *mut c_void, c: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn serde_json_tuple4(a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
        if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn serde_json_list2(a: *mut c_void, b: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn serde_json_list3(a: *mut c_void, b: *mut c_void, c: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn serde_json_list4(a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
        if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn serde_json_dict() -> *mut c_void {
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn serde_json_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {
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
pub extern "C" fn serde_json_to_cstr(obj: *mut c_void) -> *mut std::ffi::c_char {
    if obj.is_null() { return std::ptr::null_mut(); }
    let val = unsafe { retrieve(obj) };
    let s = serde_json::to_string(val).unwrap_or_else(|_| "null".to_string());
    CString::new(s).unwrap_or_default().into_raw()
}

#[no_mangle]
pub extern "C" fn serde_json_getattr(obj: *mut c_void, name: *const std::ffi::c_char) -> *mut c_void {
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
pub extern "C" fn serde_json_call(
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
pub extern "C" fn serde_json_call1(
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
pub extern "C" fn serde_json_call2(
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
pub extern "C" fn serde_json_call3(
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
pub extern "C" fn serde_json_call4(
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
pub extern "C" fn serde_json_call5(
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
pub extern "C" fn serde_json_call6(
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
pub extern "C" fn serde_json_call_kw(
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
pub extern "C" fn serde_json_construct(
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
pub extern "C" fn serde_json_call_method(
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
pub extern "C" fn serde_json_free_type(handle: *mut c_void, type_name: *const std::ffi::c_char) {
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
pub extern "C" fn serde_json_last_error() -> *mut std::ffi::c_char {
    LAST_ERROR.with(|e| {
        let s = e.borrow().clone();
        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }
    })
}
