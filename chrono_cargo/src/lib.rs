// Auto-generated Cargo bridge for chrono@0.4.45
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
    // ── Date<()> methods ──
    {
        let tn = "Date".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("and_time".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.and_time(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let __result = this_ref.and_hms(a0, a1, a2);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_opt".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let __result = this_ref.and_hms_opt(a0, a1, a2);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_milli".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let a3: serde_json::Value = args.get(3).ok_or_else(|| "3: missing arg".to_string())?.clone();
            let a3 = __deser(a3)?;
            let __result = this_ref.and_hms_milli(a0, a1, a2, a3);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_milli_opt".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let a3: serde_json::Value = args.get(3).ok_or_else(|| "3: missing arg".to_string())?.clone();
            let a3 = __deser(a3)?;
            let __result = this_ref.and_hms_milli_opt(a0, a1, a2, a3);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_micro".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let a3: serde_json::Value = args.get(3).ok_or_else(|| "3: missing arg".to_string())?.clone();
            let a3 = __deser(a3)?;
            let __result = this_ref.and_hms_micro(a0, a1, a2, a3);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_micro_opt".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let a3: serde_json::Value = args.get(3).ok_or_else(|| "3: missing arg".to_string())?.clone();
            let a3 = __deser(a3)?;
            let __result = this_ref.and_hms_micro_opt(a0, a1, a2, a3);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_nano".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let a3: serde_json::Value = args.get(3).ok_or_else(|| "3: missing arg".to_string())?.clone();
            let a3 = __deser(a3)?;
            let __result = this_ref.and_hms_nano(a0, a1, a2, a3);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("and_hms_nano_opt".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let a2: serde_json::Value = args.get(2).ok_or_else(|| "2: missing arg".to_string())?.clone();
            let a2 = __deser(a2)?;
            let a3: serde_json::Value = args.get(3).ok_or_else(|| "3: missing arg".to_string())?.clone();
            let a3 = __deser(a3)?;
            let __result = this_ref.and_hms_nano_opt(a0, a1, a2, a3);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("succ".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.succ();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("succ_opt".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.succ_opt();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("pred".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.pred();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("pred_opt".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.pred_opt();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("offset".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.offset();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("timezone".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.timezone();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("naive_utc".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.naive_utc();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("naive_local".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Date<()>) };
            let __result = this_ref.naive_local();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── DateTime<()> methods ──
    {
        let tn = "DateTime".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("date".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::DateTime<()>) };
            let __result = this_ref.date();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("date_naive".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::DateTime<()>) };
            let __result = this_ref.date_naive();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("time".to_string(), |this_ptr: *mut c_void, _args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::DateTime<()>) };
            let __result = this_ref.time();
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── Month methods ──
    {
        let tn = "Month".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("num_days".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &*(this_ptr as *const chrono::Month) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.num_days(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        type_map.insert(tn, methods);
    }
    // ── WeekdaySet methods ──
    {
        let tn = "WeekdaySet".to_string();
        let mut methods: HashMap<String, MethodFn> = HashMap::new();
        methods.insert("insert".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut chrono::WeekdaySet) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.insert(a0);
            serde_json::to_value(&__result).map_err(|e| e.to_string())
        });
        methods.insert("remove".to_string(), |this_ptr: *mut c_void, args: Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {
            let this_ref = unsafe { &mut *(this_ptr as *mut chrono::WeekdaySet) };
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let __result = this_ref.remove(a0);
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
    // ── Date<()> constructors ──
    {
        let tn = "Date".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("from_utc".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let __val = chrono::Date::<()>::from_utc(a0, a1);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctor_map.insert(tn, ctors);
    }
    // ── DateTime<()> constructors ──
    {
        let tn = "DateTime".to_string();
        let mut ctors: HashMap<String, CtorFn> = HashMap::new();
        ctors.insert("from_utc".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let __val = chrono::DateTime::<()>::from_utc(a0, a1);
            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)
        });
        ctors.insert("from_local".to_string(), |args: Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {
            let a0: serde_json::Value = args.get(0).ok_or_else(|| "0: missing arg".to_string())?.clone();
            let a0 = __deser(a0)?;
            let a1: serde_json::Value = args.get(1).ok_or_else(|| "1: missing arg".to_string())?.clone();
            let a1 = __deser(a1)?;
            let __val = chrono::DateTime::<()>::from_local(a0, a1);
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
    m.insert("Date".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut chrono::Date<()>)); };
        f
    });
    m.insert("DateTime".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut chrono::DateTime<()>)); };
        f
    });
    m.insert("Month".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut chrono::Month)); };
        f
    });
    m.insert("WeekdaySet".to_string(), {
        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut chrono::WeekdaySet)); };
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
pub extern "C" fn chrono_import() -> *mut c_void {
    registry(); // ensure initialized
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn chrono_free(ptr: *mut c_void) {
    if !ptr.is_null() {
        unsafe { drop(Box::from_raw(ptr as *mut serde_json::Value)); }
    }
}

#[no_mangle]
pub extern "C" fn chrono_free_cstr(ptr: *mut std::ffi::c_char) {
    if !ptr.is_null() {
        unsafe { drop(CString::from_raw(ptr)); }
    }
}

#[no_mangle]
pub extern "C" fn chrono_str(s: *const std::ffi::c_char) -> *mut c_void {
    let s = unsafe { CStr::from_ptr(s) }.to_string_lossy().to_string();
    store(serde_json::Value::String(s))
}

#[no_mangle]
pub extern "C" fn chrono_int(v: i64) -> *mut c_void {
    store(serde_json::Value::Number(v.into()))
}

#[no_mangle]
pub extern "C" fn chrono_float(v: f64) -> *mut c_void {
    store(serde_json::Value::Number(
        serde_json::Number::from_f64(v).unwrap_or(serde_json::Number::from(0))))
}

#[no_mangle]
pub extern "C" fn chrono_tuple(items: *mut *mut c_void, count: i32) -> *mut c_void {
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
pub extern "C" fn chrono_list(items: *mut *mut c_void, count: i32) -> *mut c_void {
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
pub extern "C" fn chrono_tuple2(a: *mut c_void, b: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn chrono_tuple3(a: *mut c_void, b: *mut c_void, c: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn chrono_tuple4(a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
        if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn chrono_list2(a: *mut c_void, b: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn chrono_list3(a: *mut c_void, b: *mut c_void, c: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn chrono_list4(a: *mut c_void, b: *mut c_void, c: *mut c_void, d: *mut c_void) -> *mut c_void {
    store(serde_json::Value::Array(vec![
        if a.is_null() { serde_json::Value::Null } else { unsafe { retrieve(a) }.clone() },
        if b.is_null() { serde_json::Value::Null } else { unsafe { retrieve(b) }.clone() },
        if c.is_null() { serde_json::Value::Null } else { unsafe { retrieve(c) }.clone() },
        if d.is_null() { serde_json::Value::Null } else { unsafe { retrieve(d) }.clone() },
    ]))
}

#[no_mangle]
pub extern "C" fn chrono_dict() -> *mut c_void {
    store(serde_json::Value::Object(serde_json::Map::new()))
}

#[no_mangle]
pub extern "C" fn chrono_dict_set(d: *mut c_void, key: *const std::ffi::c_char, val: *mut c_void) -> i32 {
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
pub extern "C" fn chrono_to_cstr(obj: *mut c_void) -> *mut std::ffi::c_char {
    if obj.is_null() { return std::ptr::null_mut(); }
    let val = unsafe { retrieve(obj) };
    let s = serde_json::to_string(val).unwrap_or_else(|_| "null".to_string());
    CString::new(s).unwrap_or_default().into_raw()
}

#[no_mangle]
pub extern "C" fn chrono_getattr(obj: *mut c_void, name: *const std::ffi::c_char) -> *mut c_void {
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
pub extern "C" fn chrono_call(
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
pub extern "C" fn chrono_call1(
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
pub extern "C" fn chrono_call2(
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
pub extern "C" fn chrono_call3(
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
pub extern "C" fn chrono_call4(
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
pub extern "C" fn chrono_call5(
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
pub extern "C" fn chrono_call6(
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
pub extern "C" fn chrono_call_kw(
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
pub extern "C" fn chrono_construct(
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
pub extern "C" fn chrono_call_method(
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
pub extern "C" fn chrono_free_type(handle: *mut c_void, type_name: *const std::ffi::c_char) {
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
pub extern "C" fn chrono_last_error() -> *mut std::ffi::c_char {
    LAST_ERROR.with(|e| {
        let s = e.borrow().clone();
        if s.is_empty() { std::ptr::null_mut() } else { CString::new(s).unwrap_or_default().into_raw() }
    })
}
