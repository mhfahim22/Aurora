/* ════════════════════════════════════════════════════════════
   Aurora WASM JS Glue (Phase 40)
   Instantiate app.wasm and bridge:
     - env imports (console + DOM + abort) used by wasm_rt.c/dom.cpp
     - element-id registry <-> real DOM nodes
     - Aurora event-callback dispatch (single active callback)
   Generated app.js from this template by scripts/build_wasm.ps1/.sh.
   ════════════════════════════════════════════════════════════ */

(function () {
  "use strict";

  var Module = {
    wasmUrl: "app.wasm",
    onReady: function () {}
  };

  /* ── Element registry ── */
  var elById = [null];          /* index 0 reserved */
  var idByEl = new WeakMap();
  var nextId = 1;
  var bodyId = -1;

  function regEl(el) {
    if (idByEl.has(el)) return idByEl.get(el);
    var id = nextId++;
    elById.push(el);
    idByEl.set(el, id);
    return id;
  }
  function elOf(id) { return elById[id] || null; }

  /* ── Aurora event callback table ── */
  var eventCache = [null];
  var listenerFns = {}; /* key "id|type" -> fnPtr table index */

  var textDecoder = new TextDecoder("utf-8");
  var textEncoder = new TextEncoder();

  function readCString(ptr, memory) {
    if (!ptr) return "";
    var bytes = new Uint8Array(memory.buffer, ptr, 1024 * 16);
    var n = 0;
    while (n < bytes.length && bytes[n] !== 0) n++;
    return textDecoder.decode(new Uint8Array(memory.buffer, ptr, n));
  }

  /* import a JS string into wasm memory via an AuroraStr */
  function writeString(exports, str) {
    var astr = exports.aurora_wasm_alloc_str(str.length);
    var bytes = textEncoder.encode(str);
    var mem = new Uint8Array(exports.memory.buffer);
    var buf = exports.aurora_wasm_import_buf(astr);
    for (var i = 0; i < bytes.length; i++) mem[buf + i] = bytes[i];
    mem[buf + bytes.length] = 0;
    exports.aurora_wasm_import_setlen(astr, bytes.length);
    return astr;
  }

  function readAuroraStr(exports, astr) {
    var len = exports.aurora_wasm_str_len(astr);
    var buf = exports.aurora_wasm_str_buf(astr);
    if (len <= 0) return "";
    return textDecoder.decode(new Uint8Array(exports.memory.buffer, buf, len));
  }

  var imports = {
    env: {
      aurora_js_abort: function () {
        throw new Error("Aurora wasm aborted (aurora_panic)");
      },
      aurora_js_console_log_str: function (p) {
        console.log(readCString(p, imports.__memory));
      },
      aurora_js_console_log_int: function (v) {
        console.log(String(v));
      },
      aurora_js_console_log_float: function (v) {
        console.log(String(v));
      },
      aurora_js_console_log_bool: function (v) {
        console.log(v ? "true" : "false");
      },
      aurora_js_console_log_raw: function () {
        console.log("");
      },

      /* ── DOM ── */
      aurora_js_dom_init: function () {
        if (bodyId < 0) bodyId = regEl(document.body);
        return 0;
      },
      aurora_js_dom_create_element: function (tag) {
        var el = document.createElement(readCString(tag, imports.__memory) || "div");
        return regEl(el);
      },
      aurora_js_dom_remove_element: function (id) {
        var el = elOf(id);
        if (el && el.parentNode) el.parentNode.removeChild(el);
        return 0;
      },
      aurora_js_dom_document_body: function () {
        if (bodyId < 0) bodyId = regEl(document.body);
        return bodyId;
      },
      aurora_js_dom_append_child: function (pid, cid) {
        var p = elOf(pid), c = elOf(cid);
        if (p && c) p.appendChild(c);
        return 0;
      },
      aurora_js_dom_insert_before: function (pid, cid, rid) {
        var p = elOf(pid), c = elOf(cid), r = elOf(rid);
        if (p && c) p.insertBefore(c, r || null);
        return 0;
      },
      aurora_js_dom_replace_child: function (pid, nid, oid) {
        var p = elOf(pid), n = elOf(nid), o = elOf(oid);
        if (p && n && o) p.replaceChild(n, o);
        return 0;
      },
      aurora_js_dom_set_text: function (id, p) {
        var el = elOf(id);
        if (el) el.textContent = readCString(p, imports.__memory);
        return 0;
      },
      aurora_js_dom_get_text: function (id) {
        var el = elOf(id);
        return writeString(Module.exports, el ? el.textContent : "");
      },
      aurora_js_dom_set_html: function (id, p) {
        var el = elOf(id);
        if (el) el.innerHTML = readCString(p, imports.__memory);
        return 0;
      },
      aurora_js_dom_set_attr: function (id, np, vp) {
        var el = elOf(id);
        if (el) el.setAttribute(readCString(np, imports.__memory), readCString(vp, imports.__memory));
        return 0;
      },
      aurora_js_dom_get_attr: function (id, np) {
        var el = elOf(id);
        return writeString(Module.exports, el ? (el.getAttribute(readCString(np, imports.__memory)) || "") : "");
      },
      aurora_js_dom_set_prop: function (id, np, vp) {
        var el = elOf(id);
        if (el) el[readCString(np, imports.__memory)] = readCString(vp, imports.__memory);
        return 0;
      },
      aurora_js_dom_set_value: function (id, vp) {
        var el = elOf(id);
        if (el) el.value = readCString(vp, imports.__memory);
        return 0;
      },
      aurora_js_dom_get_value: function (id) {
        var el = elOf(id);
        return writeString(Module.exports, el ? (el.value || "") : "");
      },
      aurora_js_dom_clear_children: function (id) {
        var el = elOf(id);
        if (el) el.textContent = "";
        return 0;
      },
      aurora_js_dom_set_style: function (id, pp, vp) {
        var el = elOf(id);
        if (el) el.style[readCString(pp, imports.__memory)] = readCString(vp, imports.__memory);
        return 0;
      },
      aurora_js_dom_get_style: function (id, pp) {
        var el = elOf(id);
        return writeString(Module.exports, el ? (el.style[readCString(pp, imports.__memory)] || "") : "");
      },
      aurora_js_dom_add_class: function (id, cp) {
        var el = elOf(id);
        if (el) el.classList.add(readCString(cp, imports.__memory));
        return 0;
      },
      aurora_js_dom_remove_class: function (id, cp) {
        var el = elOf(id);
        if (el) el.classList.remove(readCString(cp, imports.__memory));
        return 0;
      },
      aurora_js_dom_toggle_class: function (id, cp, force) {
        var el = elOf(id);
        if (el) el.classList.toggle(readCString(cp, imports.__memory), !!force);
        return 0;
      },
      aurora_js_dom_has_class: function (id, cp) {
        var el = elOf(id);
        return el && el.classList.contains(readCString(cp, imports.__memory)) ? 1 : 0;
      },
      aurora_js_dom_add_event_listener: function (id, tp, fnPtr) {
        var el = elOf(id);
        if (!el) return -1;
        var type = readCString(tp, imports.__memory);
        var table = Module.exports.__indirect_function_table;
        var fn = table.get(fnPtr);
        el.addEventListener(type, function (ev) {
          var idx = eventCache.push(ev) - 1;
          fn(BigInt(idx));
        });
        return 0;
      },
      aurora_js_dom_remove_event_listener: function (id, tp, fnPtr) {
        var el = elOf(id);
        if (el) {
          var type = readCString(tp, imports.__memory);
          /* can't easily remove an anonymous wrapper; refresh listeners is fine */
        }
        return 0;
      },
      aurora_js_dom_dispatch_event: function (id, tp) {
        var el = elOf(id);
        if (el) el.dispatchEvent(new Event(readCString(tp, imports.__memory)));
        return 0;
      },
      aurora_js_dom_prevent_default: function (idx) {
        var ev = eventCache[idx];
        if (ev && ev.preventDefault) ev.preventDefault();
        return 0;
      },
      aurora_js_dom_event_target: function (idx) {
        var ev = eventCache[idx];
        return ev && ev.target ? regEl(ev.target) : -1;
      },
      aurora_js_dom_get_element_by_id: function (ip) {
        var el = document.getElementById(readCString(ip, imports.__memory));
        return el ? regEl(el) : -1;
      },
      aurora_js_dom_query_selector: function (sp) {
        var el = document.querySelector(readCString(sp, imports.__memory));
        return el ? regEl(el) : -1;
      },
      aurora_js_dom_child_count: function (id) {
        var el = elOf(id);
        return el ? el.childNodes.length : 0;
      },
      aurora_js_dom_child_at: function (id, idx) {
        var el = elOf(id);
        return el && el.childNodes[idx] ? regEl(el.childNodes[idx]) : -1;
      },
      aurora_js_dom_parent: function (id) {
        var el = elOf(id);
        return el && el.parentNode ? regEl(el.parentNode) : -1;
      },
      aurora_js_dom_window_inner_width: function () {
        return window.innerWidth;
      },
      aurora_js_dom_window_inner_height: function () {
        return window.innerHeight;
      },
      aurora_js_dom_set_title: function (tp) {
        document.title = readCString(tp, imports.__memory);
        return 0;
      },
      aurora_js_dom_set_body_style: function (pp, vp) {
        document.body.style[readCString(pp, imports.__memory)] = readCString(vp, imports.__memory);
        return 0;
      },
      aurora_js_dom_focus: function (id) {
        var el = elOf(id);
        if (el && el.focus) el.focus();
        return 0;
      },
      aurora_js_dom_set_cb: function (fnPtr) {
        /* single global Aurora callback (used by aurora_dom_set_event_cb) */
        Module.exports.aurora_wasm_set_cb(fnPtr);
        return 0;
      },
      aurora_js_dom_cb: function () {
        return Module.exports.aurora_wasm_get_cb();
      }
    }
  };

  var wasmBinary = null;

  function fetchWasm(url) {
    return fetch(url).then(function (r) { return r.arrayBuffer(); });
  }

  function instantiate() {
    return fetchWasm(Module.wasmUrl).then(function (bytes) {
      return WebAssembly.instantiate(bytes, imports);
    });
  }

  instantiate().then(function (result) {
    var instance = result.instance;
    Module.exports = instance.exports;
    imports.__memory = Module.exports.memory;
    Module.exports.aurora_dom_init();
    if (typeof Module.exports.main === "function") {
      Module.exports.main(0, 0);
    }
    Module.onReady(Module.exports);
  }).catch(function (err) {
    console.error("Aurora wasm failed to load:", err);
  });

  if (typeof window !== "undefined") {
    window.AuroraModule = Module;
  }
})();