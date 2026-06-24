/* Auto-generated QuickJS npm bridge DLL for left-pad */
#define _GNU_SOURCE
#include "quickjs_config.h"
#include "quickjs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#define EXPORT __declspec(dllexport)

static JSRuntime *g_rt = NULL;
static JSContext *g_ctx = NULL;
static int g_inited = 0;
static int g_loaded = 0;
#define MAX_OBJS 65536
static JSValue g_objs[MAX_OBJS];
static int g_next_id = 2;

static const char node_builtins_js[] = "// Node.js built-in polyfills for QuickJS bridge\n// These are registered into __node_builtins so the C require() can intercept them.\n\n(function() {\n  var builtins = {};\n\n  // ── path ──────────────────────────────────────────────────\n  builtins.path = (function() {\n    var sep = '/', delimiter = ':';\n    function normalize(p) {\n      if (!p) return '.';\n      var isAbs = p[0] === '/';\n      var parts = p.split('/'), out = [];\n      for (var i = 0; i < parts.length; i++) {\n        if (parts[i] === '' || parts[i] === '.') continue;\n        if (parts[i] === '..') { if (out.length && out[out.length-1] !== '..') out.pop(); else if (!isAbs) out.push('..'); }\n        else out.push(parts[i]);\n      }\n      var r = isAbs ? '/' + out.join('/') : out.join('/');\n      return r || (isAbs ? '/' : '.');\n    }\n    function join() { return normalize(Array.prototype.slice.call(arguments).filter(function(x){return x!=null}).join('/')); }\n    function resolve() {\n      var parts = Array.prototype.slice.call(arguments).filter(function(x){return x!=null});\n      if (!parts.length) return __dirname || '/';\n      if (parts[0][0] === '/') return normalize(parts.join('/'));\n      return normalize((__dirname || '/') + '/' + parts.join('/'));\n    }\n    function dirname(p) {\n      if (!p) return '.';\n      var i = p.lastIndexOf('/');\n      return i === -1 ? '.' : (i === 0 ? '/' : p.substring(0, i));\n    }\n    function basename(p, ext) {\n      if (!p) return '';\n      var base = p.substring(p.lastIndexOf('/') + 1);\n      return ext && base.endsWith(ext) ? base.substring(0, base.length - ext.length) : base;\n    }\n    function extname(p) {\n      if (!p) return '';\n      var base = p.substring(p.lastIndexOf('/') + 1);\n      var d = base.lastIndexOf('.');\n      return d <= 0 ? '' : base.substring(d);\n    }\n    return { sep:sep, delimiter:delimiter, normalize:normalize, join:join, resolve:resolve,\n             dirname:dirname, basename:basename, extname:extname, isAbsolute:function(p){return p&&p[0]=='/'},\n             relative:function(f,t){var a=normalize(f).split('/'),b=normalize(t).split('/'),i=0;\n               while(i<a.length&&i<b.length&&a[i]===b[i])i++;\n               var u=[];for(var j=i;j<a.length;j++)u.push('..');\n               return u.join('/')+(u.length&&b.slice(i).length?'/':'')+b.slice(i).join('/');},\n             parse:function(p){var r={root:'/',dir:dirname(p),base:basename(p),ext:extname(p),name:basename(p,extname(p))};return r;} };\n  })();\n\n  // ── buffer ────────────────────────────────────────────────\n  builtins.buffer = (function() {\n    function Buffer(arg, encoding) {\n      if (typeof arg === 'number') { this._data=new Array(arg);for(var i=0;i<arg;i++)this._data[i]=0;this.length=arg; }\n      else if (typeof arg === 'string') {\n        this._data=encoding==='hex'?(function(h){var b=[];for(var i=0;i<h.length;i+=2)b.push(parseInt(h.substring(i,i+2),16));return b;})(arg)\n          :(function(s){var b=[];for(var i=0;i<s.length;i++){var c=s.charCodeAt(i);\n            if(c<0x80)b.push(c);else if(c<0x800){b.push(0xC0|(c>>6));b.push(0x80|(c&0x3F));}else{b.push(0xE0|(c>>12));b.push(0x80|((c>>6)&0x3F));b.push(0x80|(c&0x3F));}}\n          return b;})(arg);\n        this.length=this._data.length;\n      } else if (arg && arg.length !== undefined) { this._data=[];for(var i=0;i<arg.length;i++)this._data[i]=arg[i]&0xFF;this.length=arg.length; }\n    }\n    Buffer.prototype.toString=function(enc,s,e){\n      s=s||0;e=e||this.length;\n      if(enc==='hex'){var h='';for(var i=s;i<e;i++){var v=this._data[i];h+=(v<16?'0':'')+v.toString(16);}return h;}\n      var out='';for(var i=s;i<e;i++)out+=String.fromCharCode(this._data[i]);return out;\n    };\n    Buffer.prototype.slice=function(s,e){return new Buffer(this._data.slice(s,e));};\n    Buffer.prototype.equals=function(o){if(!o||this.length!==o.length)return false;for(var i=0;i<this.length;i++)if(this._data[i]!==o._data[i])return false;return true;};\n    Buffer.prototype.write=function(s,off,len,enc){\n      off=off||0;var b=Buffer.from(s,enc);for(var i=0;i<b.length&&i<(len||b.length);i++)this._data[off+i]=b._data[i];\n    };\n    Buffer.from=function(a,e){return new Buffer(a,e);};\n    Buffer.alloc=function(s,f){var b=new Buffer(s);if(f!==undefined){var c=typeof f==='number'?f:f.charCodeAt(0);for(var i=0;i<s;i++)b._data[i]=c;}return b;};\n    Buffer.concat=function(l){var t=0;for(var i=0;i<l.length;i++)t+=l[i].length;var d=new Array(t),p=0;for(var i=0;i<l.length;i++){for(var j=0;j<l[i].length;j++)d[p++]=l[i]._data[j];}var r=new Buffer(0);r._data=d;r.length=t;return r;};\n    Buffer.isBuffer=function(o){return o instanceof Buffer;};\n    Buffer.prototype.copy=function(t,tS,sS,eS){sS=sS||0;eS=eS||this.length;for(var i=sS;i<eS;i++)t._data[tS++]=this._data[i];};\n    return {Buffer:Buffer,SlowBuffer:Buffer,INSPECT_MAX_BYTES:50};\n  })();\n\n  // ── events ────────────────────────────────────────────────\n  builtins.events = (function() {\n    function EventEmitter(){this._events={};}\n    EventEmitter.prototype.on=EventEmitter.prototype.addListener=function(ev,fn){if(!this._events[ev])this._events[ev]=[];this._events[ev].push(fn);return this;};\n    EventEmitter.prototype.once=function(ev,fn){var s=this;function w(){fn.apply(this,arguments);s.removeListener(ev,w);}w._orig=fn;return this.on(ev,w);};\n    EventEmitter.prototype.emit=function(ev){var f=this._events[ev];if(!f||!f.length)return false;var a=Array.prototype.slice.call(arguments,1);for(var i=0;i<f.length;i++)f[i].apply(this,a);return true;};\n    EventEmitter.prototype.removeListener=function(ev,fn){var f=this._events[ev];if(!f)return this;this._events[ev]=f.filter(function(x){return x!==fn&&x._orig!==fn;});return this;};\n    EventEmitter.prototype.removeAllListeners=function(ev){if(ev)delete this._events[ev];else this._events={};return this;};\n    EventEmitter.prototype.listeners=function(ev){return(this._events[ev]||[]).slice();};\n    EventEmitter.prototype.eventNames=function(){return Object.keys(this._events);};\n    EventEmitter.prototype.listenerCount=function(ev){return(this._events[ev]||[]).length;};\n    return {EventEmitter:EventEmitter,usingDomains:false};\n  })();\n\n  // ── util ──────────────────────────────────────────────────\n  builtins.util = (function() {\n    function format(f) {\n      if(typeof f!=='string')return inspect(f);\n      var a=Array.prototype.slice.call(arguments,1);\n      return f.replace(/%[sdifjoO%]/g,function(m){\n        if(m==='%%')return'%';if(!a.length)return m;\n        var v=a.shift();\n        if(m==='%s')return String(v);if(m==='%d'||m==='%i')return parseInt(v,10);\n        if(m==='%f')return parseFloat(v);return inspect(v);\n      });\n    }\n    function inspect(o,d){if(o===null)return'null';if(o===undefined)return'undefined';\n      if(typeof o==='string')return \"'\"+o.replace(/'/g,\"\\\\'\")+\"'\";\n      if(typeof o==='number'||typeof o==='boolean')return String(o);\n      if(Array.isArray(o))return'['+o.map(function(x){return inspect(x);}).join(', ')+']';\n      if(typeof o==='object'){var k=Object.keys(o);return'{'+k.map(function(x){return x+': '+inspect(o[x]);}).join(', ')+'}';}\n      return String(o);\n    }\n    function inherits(c,s){c.prototype=Object.create(s.prototype,{constructor:{value:c,writable:true,configurable:true}});}\n    return {format:format,inspect:inspect,inherits:inherits,deprecate:function(f){return f;},\n      isArray:Array.isArray,isString:function(s){return typeof s==='string';},isNumber:function(n){return typeof n==='number';},\n      isFunction:function(f){return typeof f==='function';},isObject:function(o){return o!==null&&typeof o==='object';},\n      isNull:function(o){return o===null;},isUndefined:function(o){return o===undefined;},\n      isBuffer:function(o){return o&&o.constructor&&o.constructor.name==='Buffer';},\n      isDate:function(o){return o instanceof Date;},isRegExp:function(o){return o instanceof RegExp;},\n      promisify:function(fn){return function(){var args=Array.prototype.slice.call(arguments);var self=this;return new Promise(function(resolve,reject){args.push(function(err,val){if(err)reject(err);else resolve(val);});fn.apply(self,args);});};},\n      callbackify:function(fn){return function(){var args=Array.prototype.slice.call(arguments);var cb=args.pop();fn.apply(this,args).then(function(v){cb(null,v);},function(e){cb(e);});};}};\n  })();\n\n  // ── assert ────────────────────────────────────────────────\n  builtins.assert = (function() {\n    function AssertionError(o){this.name='AssertionError';this.message=o.message||'';this.actual=o.actual;this.expected=o.expected;this.stack=(new Error()).stack;}\n    AssertionError.prototype=Object.create(Error.prototype);\n    function ok(v,m){if(!v)throw new AssertionError({message:m||'assertion failed',actual:v,expected:true});}\n    function equal(a,b,m){if(a!=b)throw new AssertionError({message:m||'not equal',actual:a,expected:b});}\n    function strictEqual(a,b,m){if(a!==b)throw new AssertionError({message:m||'strict not equal',actual:a,expected:b});}\n    function deepEqual(a,b,m){\n      if(a===b)return;\n      if(!a||!b||typeof a!==typeof b)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});\n      if(Array.isArray(a)&&Array.isArray(b)){if(a.length!==b.length)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});for(var i=0;i<a.length;i++)deepEqual(a[i],b[i]);return;}\n      if(typeof a==='object'){var ka=Object.keys(a),kb=Object.keys(b);if(ka.length!==kb.length)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});for(var i=0;i<ka.length;i++)deepEqual(a[ka[i]],b[ka[i]]);return;}\n      if(a!==b)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});\n    }\n    function throws(fn,err){try{fn();}catch(e){if(err&&!err(e))throw e;return e;}throw new AssertionError({message:'did not throw'});}\n    function ifError(e){if(e)throw e;}\n    return {AssertionError:AssertionError,ok:ok,equal:equal,strictEqual:strictEqual,deepEqual:deepEqual,\n      notEqual:function(a,b,m){if(a==b)throw new AssertionError({message:m||'equal',actual:a,expected:b});},\n      notStrictEqual:function(a,b,m){if(a===b)throw new AssertionError({message:m||'strict equal',actual:a,expected:b});},\n      notDeepEqual:function(a,b,m){try{deepEqual(a,b);}catch(e){return;}throw new AssertionError({message:m||'deep equal',actual:a,expected:b});},\n      throws:throws,ifError:ifError,doesNotThrow:function(fn,m){try{fn();}catch(e){throw new AssertionError({message:m||'threw',actual:e});}}}};\n  })();\n\n  // ── os ────────────────────────────────────────────────────\n  builtins.os = (function() {\n    var p = typeof process!=='undefined'&&process.platform?process.platform:\n            typeof navigator!=='undefined'?'browser':'linux';\n    return {hostname:function(){return'localhost';},homedir:function(){return typeof process!=='undefined'&&process.env?process.env.HOME||'/home/user':'/home/user';},\n      platform:function(){return p;},type:function(){return p==='win32'?'Windows_NT':'Linux';},\n      release:function(){return'1.0.0';},arch:function(){return'x64';},tmpdir:function(){return'/tmp';},\n      EOL:'\\n',cpus:function(){return[{model:'QuickJS',speed:1000,times:{user:0,nice:0,sys:0,idle:0,irq:0}}];},\n      totalmem:function(){return 1073741824;},freemem:function(){return 536870912;},uptime:function(){return 0;},\n      loadavg:function(){return[0,0,0];},endianness:function(){return'LE';},networkInterfaces:function(){return{lo:[{address:'127.0.0.1',netmask:'255.0.0.0',family:'IPv4',internal:true}]};}};\n  })();\n\n  // ── querystring ───────────────────────────────────────────\n  builtins.querystring = (function() {\n    function parse(s){if(!s)return{};var r={};s.split('&').forEach(function(p){var kv=p.split('=');if(kv[0])r[decodeURIComponent(kv[0].replace(/\\+/g,' '))]=kv[1]?decodeURIComponent(kv[1].replace(/\\+/g,' ')):'';});return r;}\n    function stringify(o){return Object.keys(o).map(function(k){return encodeURIComponent(k)+'='+encodeURIComponent(String(o[k]));}).join('&');}\n    return {parse:parse,stringify:stringify,encode:stringify,decode:parse,escape:encodeURIComponent,unescape:decodeURIComponent};\n  })();\n\n  // ── crypto (minimal: hash only) ───────────────────────────\n  builtins.crypto = (function() {\n    var algos = {};\n    // Simple string hash using built-in QuickJS hash if available\n    function createHash(algo) {\n      var state = '';\n      return {update:function(data){state+=typeof data==='string'?data:data.toString();return this;},\n        digest:function(enc){\n          var h=0,i=0;\n          if(algo==='md5'||algo==='md4'){var a=0x67452301,b=0xEFCDAB89,c=0x98BADCFE,d=0x10325476;/*simple placeholder*/}\n          // Use a simple FNV-1a hash for non-cryptographic use\n          var hash=0x811C9DC5|0;\n          for(i=0;i<state.length;i++){hash^=state.charCodeAt(i);hash=(hash*0x01000193)|0;}\n          var hex=(hash>>>0).toString(16);\n          while(hex.length<8)hex='0'+hex;\n          return enc==='hex'?hex:Buffer.from(hex,'hex');\n        },copy:function(){var n=createHash(algo);n.state=state;return n;}};\n    }\n    function randomBytes(size){var b=Buffer.alloc(size);for(var i=0;i<size;i++)b._data[i]=Math.floor(Math.random()*256);return b;}\n    return {createHash:createHash,createHmac:function(a,k){return createHash(a);},\n      randomBytes:randomBytes,pseudoRandomBytes:randomBytes,\n      randomFill:function(b,o,s,cb){var r=randomBytes(s);for(var i=0;i<s;i++)b._data[o+i]=r._data[i];if(cb)cb(null,b);},\n      getCiphers:function(){return[];},getHashes:function(){return['sha1','sha256','md5'];}};\n  })();\n\n  // ── stream (minimal) ──────────────────────────────────────\n  builtins.stream = (function() {\n    var EE = builtins.events.EventEmitter;\n    function Readable(opts){EE.call(this);this._readableState={highWaterMark:16384,buffer:[],flowing:null,pipes:[],pipesCount:0};}\n    Readable.prototype=Object.create(EE.prototype);\n    Readable.prototype._read=function(){};\n    Readable.prototype.read=function(n){return null;};\n    Readable.prototype.pipe=function(dest,opts){dest.write(this.read());return dest;};\n    Readable.prototype.setEncoding=function(e){};\n    function Writable(opts){EE.call(this);this._writableState={highWaterMark:16384,length:0,writing:false};}\n    Writable.prototype=Object.create(EE.prototype);\n    Writable.prototype._write=function(c,e,cb){cb();};\n    Writable.prototype.write=function(c,e,cb){this._write(c,e,cb||function(){});return true;};\n    Writable.prototype.end=function(c,e,cb){if(c)this.write(c,e);if(cb)cb();this.emit('finish');};\n    function Transform(opts){Readable.call(this,opts);Writable.call(this,opts);}\n    Transform.prototype=Object.create(Readable.prototype);\n    Object.keys(Writable.prototype).forEach(function(k){Transform.prototype[k]=Writable.prototype[k];});\n    Transform.prototype._transform=function(c,e,cb){cb(null,c);};\n    Transform.prototype._flush=function(cb){cb();};\n    Transform.prototype.push=function(c,e){this.emit('data',c);};\n    Transform.prototype.write=function(c,e,cb){var self=this;this._transform(c,e||'buffer',function(err,d){if(err)self.emit('error',err);else if(d!==null&&d!==undefined)self.push(d);if(cb)cb(err);});return true;};\n    return {Readable:Readable,Writable:Writable,Transform:Transform,DynamicStream:Transform,PassThrough:Transform,Stream:Readable};\n  })();\n\n  // ── fs (sync only) ─────────────────────────────────────────\n  builtins.fs = (function() {\n    var bF = typeof __bridge_fs_readFile !== 'undefined';\n    function readFileSync(path, enc) {\n      if (!bF) throw new Error('fs not available');\n      var data = __bridge_fs_readFile(path);\n      if (data === null) throw new Error('ENOENT: ' + path);\n      return enc === 'utf8' || enc === 'utf-8' || !enc ? data : Buffer.from(data);\n    }\n    function writeFileSync(path, data, enc) {\n      if (!bF) throw new Error('fs not available');\n      var s = typeof data === 'string' ? data : data.toString();\n      return __bridge_fs_writeFile(path, s);\n    }\n    function existsSync(path) { return bF ? __bridge_fs_exists(path) : false; }\n    function mkdirSync(path, opts) { return bF ? __bridge_fs_mkdir(path) : false; }\n    function readdirSync(path) { return bF ? __bridge_fs_readdir(path) : null; }\n    function statSync(path) { return bF ? __bridge_fs_stat(path) : null; }\n    function unlinkSync(path) { return bF ? __bridge_fs_unlink(path) : false; }\n    function rmdirSync(path) { return bF ? __bridge_fs_unlink(path) : false; }\n    function appendFileSync(path, data, enc) {\n      var existing = '';\n      try { existing = readFileSync(path, 'utf8') || ''; } catch(e) {}\n      writeFileSync(path, existing + (typeof data === 'string' ? data : data.toString()), enc);\n    }\n    function realpathSync(path) { return path; }\n    var constants = { F_OK:0, R_OK:4, W_OK:2, X_OK:1 };\n    function accessSync(path, mode) {\n      if (!bF || !__bridge_fs_exists(path)) throw new Error('ENOENT: ' + path);\n    }\n    return {\n      readFileSync: readFileSync, writeFileSync: writeFileSync, existsSync: existsSync,\n      mkdirSync: mkdirSync, readdirSync: readdirSync, statSync: statSync,\n      unlinkSync: unlinkSync, rmdirSync: rmdirSync, appendFileSync: appendFileSync,\n      realpathSync: realpathSync, accessSync: accessSync, constants: constants,\n      promises: { readFile: function(p) { try { return Promise.resolve(readFileSync(p)); } catch(e) { return Promise.reject(e); } },\n                  writeFile: function(p,d) { try { return Promise.resolve(writeFileSync(p,d)); } catch(e) { return Promise.reject(e); } },\n                  stat: function(p) { try { return Promise.resolve(statSync(p)); } catch(e) { return Promise.reject(e); } },\n                  readdir: function(p) { try { return Promise.resolve(readdirSync(p)); } catch(e) { return Promise.reject(e); } },\n                  mkdir: function(p) { try { return Promise.resolve(mkdirSync(p)); } catch(e) { return Promise.reject(e); } },\n                  unlink: function(p) { try { return Promise.resolve(unlinkSync(p)); } catch(e) { return Promise.reject(e); } } } };\n  })();\n\n  // ── child_process (minimal execSync) ──────────────────────\n  builtins.child_process = (function() {\n    var bE = typeof __bridge_exec !== 'undefined';\n    function execSync(cmd, opts) {\n      if (!bE) throw new Error('child_process not available');\n      var stdout = __bridge_exec(cmd);\n      if (stdout === null) throw new Error('exec failed: ' + cmd);\n      return { stdout: stdout, stderr: '', status: 0 };\n    }\n    function exec(cmd, opts, cb) {\n      try {\n        var r = execSync(cmd, opts);\n        if (cb) cb(null, r);\n        else return Promise.resolve(r);\n      } catch(e) { if (cb) cb(e); else return Promise.reject(e); }\n    }\n    return { execSync: execSync, exec: exec };\n  })();\n\n  // ── http (basic get/request) ──────────────────────────────\n  builtins.http = (function() {\n    var bH = typeof __bridge_http_get !== 'undefined';\n    var events = builtins.events;\n    function get(url, opts, cb) {\n      if (typeof opts === 'function') { cb = opts; opts = {}; }\n      if (!bH) { if (cb) cb(new Error('http not available')); return; }\n      try {\n        var body = __bridge_http_get(url);\n        if (body === null) { if (cb) cb(new Error('http get failed: ' + url)); return; }\n        var res = new events.EventEmitter();\n        res.statusCode = 200;\n        res.headers = { 'content-type': 'text/plain' };\n        res.setEncoding = function() {};\n        res.on = function(ev, fn) {\n          if (ev === 'data' && body) { fn(body); res.emit('end'); }\n          events.EventEmitter.prototype.on.call(res, ev, fn);\n          return res;\n        };\n        if (cb) cb(res);\n        return res;\n      } catch(e) { if (cb) cb(e); }\n    }\n    function request(url, opts, cb) { return get(url, opts, cb); }\n    return { get: get, request: request };\n  })();\n\n  builtins.https = builtins.http;\n\n  // ── module (full) ─────────────────────────────────────────\n  builtins.module = (function() {\n    var builtinMods = Object.keys(builtins);\n    var cache = {};\n    function Module(id, parent) {\n      this.id = id || '.';\n      this.exports = {};\n      this.filename = id || __filename || '';\n      this.loaded = false;\n      this.parent = parent || null;\n      this.children = [];\n      this.paths = [];\n    }\n    Module._cache = cache;\n    Module._extensions = {\n      '.js': function(m, fn) {},\n      '.json': function(m, fn) { try { m.exports = JSON.parse(fn); } catch(e) { m.exports = {}; } }\n    };\n    Module.globalPaths = [];\n    Module.builtinModules = builtinMods;\n    Module._resolveFilename = function(request, parent) {\n      if (builtinMods.indexOf(request) !== -1) return request;\n      return request;\n    };\n    return { Module: Module, builtinModules: builtinMods,\n             _cache: cache, _resolveFilename: Module._resolveFilename,\n             _extensions: Module._extensions };\n  })();\n\n  // ── Enhance C-level require with Node.js properties ──────\n  if (typeof require !== 'undefined') {\n    if (!require.cache) {\n      var _origRequire = require;\n      require = function(name) {\n        if (typeof name === 'string' && name.indexOf('rust:') === 0) {\n          var crateName = name.slice(5);\n          if (require.cache['rust:' + crateName]) return require.cache['rust:' + crateName];\n          var mod = _origRequire(name);\n          if (mod) require.cache['rust:' + crateName] = mod;\n          return mod;\n        }\n        return _origRequire(name);\n      };\n      require.cache = {};\n      require.resolve = function(name) {\n        if (builtins[name] || __node_builtins[name]) return name;\n        if (typeof __bridge_fs_exists !== 'undefined') {\n          var checks = ['./node_modules/' + name + '/package.json',\n                        './node_modules/' + name + '.js',\n                        './node_modules/' + name + '/index.js'];\n          for (var i = 0; i < checks.length; i++) {\n            if (__bridge_fs_exists(checks[i])) return checks[i];\n          }\n        }\n        return name;\n      };\n      require.main = { id: '.', exports: {}, loaded: true, filename: __filename || '', paths: [] };\n      require.extensions = { '.js': function(m, fn) {}, '.json': function(m, fn) { try { m.exports = JSON.parse(fn); } catch(e) { m.exports = {}; } } };\n    }\n  }\n\n  // ── process extensions (already partially in C) ───────────\n  // If process doesn't have cwd/env/argv, add them\n  if (typeof process !== 'undefined') {\n    if (!process.cwd) process.cwd = function() { return __dirname || '/'; };\n    if (!process.env) process.env = { PATH: '/usr/bin', HOME: '/home/user', NODE_ENV: 'production' };\n    if (!process.argv) process.argv = ['qjs', '.', ''];\n    if (!process.exit) process.exit = function(code) { throw new Error('exit:'+code); };\n    if (!process.nextTick) process.nextTick = function(fn) { fn(); };\n    if (!process.browser) process.browser = false;\n    if (!process.version) process.version = 'v16.0.0';\n    if (!process.versions) process.versions = { node: '16.0.0', v8: '9.0', uv: '1.0', zlib: '1.0' };\n    if (!process.stderr) process.stderr = { write: function(s) { } };\n    if (!process.stdin) process.stdin = { isTTY: false, setEncoding: function(){}, on: function(){} };\n    if (!process.pid) process.pid = 1;\n    if (!process.uptime) process.uptime = function() { return 0; };\n    if (!process.memoryUsage) process.memoryUsage = function() { return { rss: 0, heapTotal: 0, heapUsed: 0, external: 0 }; };\n    if (!process.cpuUsage) process.cpuUsage = function() { return { user: 0, system: 0 }; };\n    if (!process.umask) process.umask = function() { return 0o022; };\n    if (!process.kill) process.kill = function(pid, sig) {};\n    if (!process.hrtime) process.hrtime = function(t) { var n = Date.now(); return t ? [0, (n - (t[0]*1e3 + t[1]/1e6))*1e6] : [0, n*1e6]; };\n    if (!process.getuid) process.getuid = function() { return 0; };\n    if (!process.getgid) process.getgid = function() { return 0; };\n  }\n\n  // ── Register all builtins into global registry ────────────\n  globalThis.__node_builtins = builtins;\n})();\n";

static void json_escape(const char *in, char *out, size_t outsz) {
  size_t j=0; for(size_t i=0;in[i]&&j+6<outsz;i++){
    unsigned char c=(unsigned char)in[i];
    if(c=='\\'||c=='"'){out[j++]='\\';out[j++]=c;}
    else if(c=='
'){out[j++]='\\';out[j++]='n';}
    else if(c=='\r'){out[j++]='\\';out[j++]='r';}
    else if(c=='\t'){out[j++]='\\';out[j++]='t';}
    else if(c<32){j+=snprintf(out+j,outsz-j,"\\u%04x",c);}
    else out[j++]=c;}
  out[j]='\0';}

static void* store_json(const char* json){
  if(!json)return NULL;char**pp=(char**)malloc(sizeof(char*));
  *pp=(char*)malloc(strlen(json)+1);strcpy(*pp,json);return(void*)pp;}
static const char* get_json(void* handle){
  if(!handle)return"null";return*((const char**)handle);}

static char* jsval_to_json(JSContext*ctx,JSValue val){
  JSValue j=JS_JSONStringify(ctx,val,JS_NULL,JS_NULL);
  if(JS_IsException(j))return strdup("null");
  const char*s=JS_ToCString(ctx,j);char*r=s?strdup(s):strdup("null");
  if(s)JS_FreeCString(ctx,s);JS_FreeValue(ctx,j);return r;}

static JSValue js_require(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue js_rust_load(JSContext*ctx,const char*name);
static JSValue bridge_npm_install(JSContext*ctx,const char*name);
static JSValue js_bridge_npm_install(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue js_console_log(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue js_pstdout(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_readFile(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_writeFile(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_exists(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_mkdir(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_readdir(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_stat(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_fs_unlink(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_exec(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_http_get(JSContext*,JSValueConst,int,JSValueConst*);
/* Current working directory for module resolution */
static char g_current_dir[4096]=".";

static JSValue load_module(JSContext*ctx,const char*path,const char*name){
  FILE*f=fopen(path,"rb");if(!f)return JS_ThrowReferenceError(ctx,"mod %s",name);
  fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);
  char*code=(char*)malloc((size_t)len+1);
  if(!code){fclose(f);return JS_ThrowOutOfMemory(ctx);}
  fread(code,1,len,f);code[len]=0;fclose(f);
  JSValue ex=JS_NewObject(ctx),mo=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,mo,"exports",JS_DupValue(ctx,ex));
  char dir[4096];strncpy(dir,path,sizeof(dir)-1);dir[sizeof(dir)-1]=0;
  char*p=strrchr(dir,'/');
  char*p2=strrchr(dir,'\\');if(p2>p)p=p2;
  if(p)*p=0;
  /* Set current dir for module resolution */
  char prev_dir[4096];strncpy(prev_dir,g_current_dir,sizeof(prev_dir)-1);
  strncpy(g_current_dir,dir,sizeof(g_current_dir)-1);
  JSValue g=JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx,g,"exports",JS_DupValue(ctx,ex));
  JS_SetPropertyStr(ctx,g,"module",JS_DupValue(ctx,mo));
  JS_SetPropertyStr(ctx,g,"__filename",JS_NewString(ctx,path));
  JS_SetPropertyStr(ctx,g,"__dirname",JS_NewString(ctx,dir));
  JS_FreeValue(ctx,g);
  JSValue r=JS_Eval(ctx,code,(size_t)len,path,JS_EVAL_TYPE_GLOBAL);free(code);
  /* Restore previous dir */
  strncpy(g_current_dir,prev_dir,sizeof(g_current_dir)-1);
  if(JS_IsException(r)){JS_FreeValue(ctx,r);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_EXCEPTION;}
  JS_FreeValue(ctx,r);
  JSValue fe=JS_GetPropertyStr(ctx,mo,"exports");
  JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return fe;}

static char* resolve_mod(const char*name,const char*from){
  static char buf[4096];char dir[4096];
  strncpy(dir,from?from:".",sizeof(dir)-1);dir[sizeof(dir)-1]=0;
  /* Relative path: try direct .js, /index.js, and /package.json */
  if(name[0]=='.'){
    snprintf(buf,sizeof(buf),"%s/%s",dir,name);
    /* Try .js */
    char t[4096];snprintf(t,sizeof(t),"%s.js",buf);FILE*f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}
    /* Try index.js */
    snprintf(t,sizeof(t),"%s/index.js",buf);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}
    /* Try package.json */
    snprintf(t,sizeof(t),"%s/package.json",buf);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}
    return NULL;
  }
  while(1){
    snprintf(buf,sizeof(buf),"%s/node_modules/%s/package.json",dir,name);
    FILE*f=fopen(buf,"rb");if(f){fclose(f);snprintf(buf,sizeof(buf),"%s/node_modules/%s",dir,name);return buf;}
    snprintf(buf,sizeof(buf),"%s/node_modules/%s.js",dir,name);
    f=fopen(buf,"rb");if(f){fclose(f);return buf;}
    char*p=strrchr(dir,'/');
    char*p2=strrchr(dir,'\\');if(p2>p)p=p2;
    if(!p||p==dir)break;*p=0;}
  return NULL;}

#define MAX_CACHED 256
static const char* g_cache_names[MAX_CACHED];
static JSValue g_cache_exports[MAX_CACHED];
static int g_cache_count=0;

static JSValue js_require(JSContext*ctx,JSValueConst t,int argc,JSValueConst*argv){
  if(argc<1)return JS_ThrowTypeError(ctx,"require:missing arg");
  const char*name=JS_ToCString(ctx,argv[0]);if(!name)return JS_ThrowTypeError(ctx,"bad arg");
  /* Check require.cache first */
  for(int i=0;i<g_cache_count;i++){
    if(strcmp(g_cache_names[i],name)==0){
      JS_FreeCString(ctx,name);return JS_DupValue(ctx,g_cache_exports[i]);
    }
  }
  /* Check Node.js builtins first */
  JSValue glob=JS_GetGlobalObject(ctx);
  JSValue bm=JS_GetPropertyStr(ctx,glob,"__node_builtins");
  if(!JS_IsUndefined(bm)){
    JSValue bmod=JS_GetPropertyStr(ctx,bm,name);
    if(!JS_IsUndefined(bmod)){
      JS_FreeCString(ctx,name);JS_FreeValue(ctx,bm);JS_FreeValue(ctx,glob);
      return bmod;
    }
    JS_FreeValue(ctx,bmod);
  }
  JS_FreeValue(ctx,bm);JS_FreeValue(ctx,glob);
  char*md=resolve_mod(name,g_current_dir);
  /* If not found on disk, auto-install npm or load rust crate */
  if(!md){
    if(strncmp(name,"rust:",5)==0){
      JSValue rmod=js_rust_load(ctx,name+5);
      JS_FreeCString(ctx,name);
      if(!JS_IsException(rmod)&&g_cache_count<MAX_CACHED){
        g_cache_names[g_cache_count]=strdup(name);
        g_cache_exports[g_cache_count]=JS_DupValue(ctx,rmod);
        g_cache_count++;
      }
      return rmod;
    }
    /* Auto-install missing npm package */
    JSValue bni=bridge_npm_install(ctx,name);
    if(JS_IsException(bni)||(JS_IsBool(bni)&&!JS_VALUE_GET_BOOL(bni))){
      JS_FreeValue(ctx,bni);
      JS_FreeCString(ctx,name);
      return JS_ThrowReferenceError(ctx,"module %s not found and auto-install failed",name);
    }
    md=resolve_mod(name,g_current_dir);
    if(!md){JS_FreeCString(ctx,name);return JS_ThrowReferenceError(ctx,"module %s installed but not resolved",name);}
  }
  char entry[4096],pj[4096];snprintf(pj,sizeof(pj),"%s/package.json",md);
  FILE*f=fopen(pj,"rb");
  if(f){
    fseek(f,0,SEEK_END);long jl=ftell(f);fseek(f,0,SEEK_SET);
    char*js=(char*)malloc((size_t)jl+1);fread(js,1,jl,f);js[jl]=0;fclose(f);
    JSValue po=JS_ParseJSON(ctx,js,(size_t)jl,"<pkg>");free(js);
    if(!JS_IsException(po)){
      int resolved=0;
      /* Try 'exports' map before 'main' */
      JSValue ev=JS_GetPropertyStr(ctx,po,"exports");
      if(JS_IsString(ev)){
        const char*es=JS_ToCString(ctx,ev);
        if(es){snprintf(entry,sizeof(entry),"%s/%s",md,es[0]=='.'&&es[1]=='/'?es+2:es);JS_FreeCString(ctx,es);resolved=1;JS_FreeValue(ctx,ev);}
      }else if(JS_IsObject(ev)){
        JSValue dot=JS_GetPropertyStr(ctx,ev,".");
        if(!JS_IsUndefined(dot)){
          if(JS_IsString(dot)){
            const char*ds=JS_ToCString(ctx,dot);
            if(ds){snprintf(entry,sizeof(entry),"%s/%s",md,ds[0]=='.'&&ds[1]=='/'?ds+2:ds);JS_FreeCString(ctx,ds);resolved=1;JS_FreeValue(ctx,ev);}
          }else if(JS_IsObject(dot)){
            const char*conds[]={"require","node","default","import",NULL};
            for(int ci=0;conds[ci]&&!resolved;ci++){
              JSValue cv=JS_GetPropertyStr(ctx,dot,conds[ci]);
              if(JS_IsString(cv)){
                const char*cs=JS_ToCString(ctx,cv);
                if(cs){snprintf(entry,sizeof(entry),"%s/%s",md,cs[0]=='.'&&cs[1]=='/'?cs+2:cs);JS_FreeCString(ctx,cs);resolved=1;JS_FreeValue(ctx,ev);}
              }
              JS_FreeValue(ctx,cv);
            }
          }
          JS_FreeValue(ctx,dot);
        }
        /* Subpath exports like {"./feature":"./lib/feature.js"} not yet supported */
        JS_FreeValue(ctx,ev);
      }else JS_FreeValue(ctx,ev);
      if(!resolved){
        JSValue mv=JS_GetPropertyStr(ctx,po,"main");
        if(!JS_IsUndefined(mv)){
          const char*ms=JS_ToCString(ctx,mv);
          if(ms){snprintf(entry,sizeof(entry),"%s/%s",md,ms[0]=='.'&&ms[1]=='/'?ms+2:ms);JS_FreeCString(ctx,ms);}
          else snprintf(entry,sizeof(entry),"%s/index.js",md);
          JS_FreeValue(ctx,mv);
        }else snprintf(entry,sizeof(entry),"%s/index.js",md);
      }
      JS_FreeValue(ctx,po);
    }else snprintf(entry,sizeof(entry),"%s/index.js",md);
  }else{
    if(strstr(md,".js"))snprintf(entry,sizeof(entry),"%s",md);
    else snprintf(entry,sizeof(entry),"%s/index.js",md);
  }
  /* Pre-register in cache BEFORE eval to support circular dependencies */
  int cache_slot=-1;
  for(int i=0;i<g_cache_count;i++){if(strcmp(g_cache_names[i],name)==0){cache_slot=i;break;}}
  if(cache_slot<0&&g_cache_count<MAX_CACHED){
    cache_slot=g_cache_count++;
    g_cache_names[cache_slot]=strdup(name);
    g_cache_exports[cache_slot]=JS_NewObject(ctx);
  }
  JSValue ret=load_module(ctx,entry,name);
  if(!JS_IsException(ret)&&cache_slot>=0){
    JS_FreeValue(ctx,g_cache_exports[cache_slot]);
    g_cache_exports[cache_slot]=JS_DupValue(ctx,ret);
  }
  JS_FreeCString(ctx,name);return ret;}

static int init_qjs(const char*dir){
  if(g_inited)return 1;g_rt=JS_NewRuntime();if(!g_rt)return 0;
  g_ctx=JS_NewContext(g_rt);if(!g_ctx){JS_FreeRuntime(g_rt);g_rt=NULL;return 0;}
  if(dir){
    SetCurrentDirectoryA(dir);
  }
  JSValue g=JS_GetGlobalObject(g_ctx);
  JSValue c=JS_NewObject(g_ctx);
  JS_SetPropertyStr(g_ctx,c,"log",JS_NewCFunction(g_ctx,js_console_log,"log",1));
  JS_SetPropertyStr(g_ctx,c,"warn",JS_NewCFunction(g_ctx,js_console_log,"warn",1));
  JS_SetPropertyStr(g_ctx,c,"error",JS_NewCFunction(g_ctx,js_console_log,"error",1));
  JS_SetPropertyStr(g_ctx,g,"console",c);
  JSValue p=JS_NewObject(g_ctx);JSValue so=JS_NewObject(g_ctx);
  JS_SetPropertyStr(g_ctx,so,"write",JS_NewCFunction(g_ctx,js_pstdout,"write",1));
  JS_SetPropertyStr(g_ctx,p,"stdout",so);
  JS_SetPropertyStr(g_ctx,g,"process",p);
  JS_SetPropertyStr(g_ctx,g,"require",JS_NewCFunction(g_ctx,js_require,"require",1));
  /* Register bridge I/O helpers */
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_readFile",JS_NewCFunction(g_ctx,bridge_fs_readFile,"__bridge_fs_readFile",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_writeFile",JS_NewCFunction(g_ctx,bridge_fs_writeFile,"__bridge_fs_writeFile",2));
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_exists",JS_NewCFunction(g_ctx,bridge_fs_exists,"__bridge_fs_exists",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_mkdir",JS_NewCFunction(g_ctx,bridge_fs_mkdir,"__bridge_fs_mkdir",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_readdir",JS_NewCFunction(g_ctx,bridge_fs_readdir,"__bridge_fs_readdir",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_stat",JS_NewCFunction(g_ctx,bridge_fs_stat,"__bridge_fs_stat",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_fs_unlink",JS_NewCFunction(g_ctx,bridge_fs_unlink,"__bridge_fs_unlink",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_exec",JS_NewCFunction(g_ctx,bridge_exec,"__bridge_exec",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_http_get",JS_NewCFunction(g_ctx,bridge_http_get,"__bridge_http_get",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_npm_install",JS_NewCFunction(g_ctx,js_bridge_npm_install,"__bridge_npm_install",1));
  JS_FreeValue(g_ctx,g);
  /* Evaluate Node.js builtin polyfills */
  {JSValue _ev=JS_Eval(g_ctx,node_builtins_js,strlen(node_builtins_js),"<builtins>",JS_EVAL_TYPE_GLOBAL);JS_FreeValue(g_ctx,_ev)}
  g_inited=1;return 1;}

static JSValue js_console_log(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  for(int i=0;i<ac;i++){if(i)fputc(' ',stderr);const char*s=JS_ToCString(ctx,av[i]);if(s){fputs(s,stderr);JS_FreeCString(ctx,s);}}
  fputc('
',stderr);return JS_UNDEFINED;}
static JSValue js_pstdout(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac>0){const char*s=JS_ToCString(ctx,av[0]);if(s){printf("%s",s);fflush(stdout);JS_FreeCString(ctx,s);}}
  return JS_UNDEFINED;}

static JSValue bridge_fs_readFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_ThrowTypeError(ctx,"readFile:missing path");
  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,"bad path");
  FILE*f=fopen(path,"rb");if(!f){JS_FreeCString(ctx,path);return JS_NULL;}
  fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
  char*buf=(char*)malloc((size_t)sz+1);if(!buf){fclose(f);JS_FreeCString(ctx,path);return JS_NULL;}
  fread(buf,1,sz,f);buf[sz]=0;fclose(f);
  JSValue r=JS_NewStringLen(ctx,buf,(size_t)sz);free(buf);JS_FreeCString(ctx,path);return r;}

static JSValue bridge_fs_writeFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<2)return JS_ThrowTypeError(ctx,"writeFile:missing args");
  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,"bad path");
  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,path);return JS_ThrowTypeError(ctx,"bad data");}
  FILE*f=fopen(path,"wb");if(!f){JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_FALSE;}
  fwrite(data,1,strlen(data),f);fclose(f);
  JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_TRUE;}

static JSValue bridge_fs_exists(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_FALSE;
  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;
  FILE*f=fopen(path,"rb");int ok=(f!=NULL);if(f)fclose(f);
  JS_FreeCString(ctx,path);return ok?JS_TRUE:JS_FALSE;}

static JSValue bridge_fs_mkdir(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;
  int r=_mkdir(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}

static JSValue bridge_fs_readdir(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;
  char pat[4096];snprintf(pat,sizeof(pat),"%s/*",path);WIN32_FIND_DATAA fd;
  HANDLE h=FindFirstFileA(pat,&fd);if(h==INVALID_HANDLE_VALUE){JS_FreeCString(ctx,path);return JS_NULL;}
  JSValue arr=JS_NewArray(ctx);int idx=0;
  do{JSValue nm=JS_NewString(ctx,fd.cFileName);JS_SetPropertyUint32(ctx,arr,idx++,nm);}while(FindNextFileA(h,&fd));
  FindClose(h);JS_FreeCString(ctx,path);return arr;}

static JSValue bridge_fs_stat(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;
  struct stat st;if(stat(path,&st)!=0){JS_FreeCString(ctx,path);return JS_NULL;}
  JSValue o=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,o,"size",JS_NewInt64(ctx,st.st_size));
  JS_SetPropertyStr(ctx,o,"mode",JS_NewInt64(ctx,st.st_mode));
  JS_SetPropertyStr(ctx,o,"isFile",JS_NewBool(ctx,S_ISREG(st.st_mode)));
  JS_SetPropertyStr(ctx,o,"isDirectory",JS_NewBool(ctx,S_ISDIR(st.st_mode)));
  JS_FreeCString(ctx,path);return o;}

static JSValue bridge_fs_unlink(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;
  int r=remove(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}

static JSValue bridge_exec(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*cmd=JS_ToCString(ctx,av[0]);if(!cmd)return JS_NULL;
  FILE*pipe=_popen(cmd,"r");
  if(!pipe){JS_FreeCString(ctx,cmd);return JS_NULL;}
  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);out[0]=0;
  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);out=(char*)realloc(out,tot+l+1);memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}
  int rc=_pclose(pipe);
  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,cmd);return r;}

static JSValue bridge_http_get(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*url=JS_ToCString(ctx,av[0]);if(!url)return JS_NULL;
  char tmpfile[256]="";const char* tmpdir=getenv("TMPDIR");if(!tmpdir)tmpdir="/tmp";
  snprintf(tmpfile,sizeof(tmpfile),"%s/qjs_http_XXXXXX",tmpdir);
  char cmd[65536];snprintf(cmd,sizeof(cmd),"powershell -Command \"try{$r=Invoke-WebRequest -Uri '%s' -UseBasicParsing;if($r.Content){$r.Content}else{'{}'}}catch{'{}'}\" 2>nul",url);
  FILE*pipe=_popen(cmd,"r");
  if(!pipe){JS_FreeCString(ctx,url);return JS_NULL;}
  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);out[0]=0;
   while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}
   _pclose(pipe);
   JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,cmd);return r;}

static JSValue bridge_http_get(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
   if(ac<1)return JS_NULL;const char*url=JS_ToCString(ctx,av[0]);if(!url)return JS_NULL;
   char tmpfile[256]="";const char* tmpdir=getenv("TMPDIR");if(!tmpdir)tmpdir="/tmp";
   snprintf(tmpfile,sizeof(tmpfile),"%s/qjs_http_XXXXXX",tmpdir);
   char cmd[65536];snprintf(cmd,sizeof(cmd),"powershell -Command \"try{$r=Invoke-WebRequest -Uri '%s' -UseBasicParsing;if($r.Content){$r.Content}else{'{}'}}catch{'{}'}\" 2>nul",url);
   FILE*pipe=_popen(cmd,"r");
   if(!pipe){JS_FreeCString(ctx,url);return JS_NULL;}
   char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out[0]=0;
   while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}
   _pclose(pipe);
   JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,url);return r;}

static JSValue js_bridge_npm_install(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_FALSE;const char*n=JS_ToCString(ctx,av[0]);if(!n)return JS_FALSE;
  JSValue r=bridge_npm_install(ctx,n);JS_FreeCString(ctx,n);return r;}

static const char* cjs_version(const char*name){
  static const struct{const char*n;const char*v;}esm[]={
    {"chalk","4"},{"nanocolors","0"},{"got","11"},
    {"ky","0"},{"execa","5"},{"globby","11"},
    {"p-map","4"},{"p-limit","3"},{NULL,NULL}
  };
  for(int i=0;esm[i].n;i++)if(strcmp(name,esm[i].n)==0)return esm[i].v;
  return NULL;}

/* ── bridge_npm_install — auto-download & extract npm package ── */
static JSValue bridge_npm_install(JSContext*ctx,const char*name){
  /* Check CJS fallback for ESM-only packages */
  const char*pin=cjs_version(name);
  /* Fetch package metadata from npm registry */
  char url[4096];
  if(pin)snprintf(url,sizeof(url),"https://registry.npmjs.org/%s/%s",name,pin);
  else snprintf(url,sizeof(url),"https://registry.npmjs.org/%s/latest",name);
  JSValueConst meta_args[1];meta_args[0]=JS_NewString(ctx,url);
  JSValue meta=bridge_http_get(ctx,JS_NULL,1,meta_args);
  JS_FreeValue(ctx,meta_args[0]);
  if(JS_IsException(meta)||JS_IsNull(meta))return JS_FALSE;
  const char*ms=JS_ToCString(ctx,meta);if(!ms){JS_FreeValue(ctx,meta);return JS_FALSE;}
  JSValue po=JS_ParseJSON(ctx,ms,strlen(ms),"<meta>");JS_FreeCString(ctx,ms);JS_FreeValue(ctx,meta);
  if(JS_IsException(po))return JS_FALSE;
  JSValue tv=JS_GetPropertyStr(ctx,po,"tarball");
  if(JS_IsUndefined(tv)){JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);return JS_FALSE;}
  const char*tarball=JS_ToCString(ctx,tv);
  if(!tarball){JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);return JS_FALSE;}
  /* Download tarball */
  JSValueConst dl_args[1];dl_args[0]=JS_NewString(ctx,tarball);
  JSValue dl=bridge_http_get(ctx,JS_NULL,1,dl_args);
  JS_FreeValue(ctx,dl_args[0]);JS_FreeCString(ctx,tarball);JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);
  if(JS_IsException(dl)||JS_IsNull(dl))return JS_FALSE;
  const char*tgz=JS_ToCString(ctx,dl);if(!tgz){JS_FreeValue(ctx,dl);return JS_FALSE;}
  /* Write tarball to temp file */
  char tmpfile[1024]="";const char*td=getenv("TMPDIR");if(!td)td="/tmp";
  snprintf(tmpfile,sizeof(tmpfile),"%s/qjs_npm_XXXXXX",td);_mktemp_s(tmpfile,sizeof(tmpfile));
  FILE*fw=fopen(tmpfile,"wb");if(fw){fwrite(tgz,1,strlen(tgz),fw);fclose(fw);}
  JS_FreeCString(ctx,tgz);JS_FreeValue(ctx,dl);
  /* Ensure node_modules and extract */
  _mkdir("node_modules");
  char cmd[65536];snprintf(cmd,sizeof(cmd),"tar xzf \"%s\" -C node_modules 2>nul",tmpfile);
  int rc=system(cmd);remove(tmpfile);
  if(rc!=0)return JS_FALSE;
  /* Rename node_modules/package -> node_modules/<name> */
  char oldp[4096],newp[4096];
  snprintf(oldp,sizeof(oldp),"node_modules/package");
  snprintf(newp,sizeof(newp),"node_modules/%s",name);
  _unlink(newp);_rmdir(newp);
  rename(oldp,newp);
  return JS_TRUE;}

static JSValue js_rust_load(JSContext*ctx,const char*name){
  char tmpdir[1024]="";const char*td=getenv("TMPDIR");if(!td)td="/tmp";
  snprintf(tmpdir,sizeof(tmpdir),"%s/qjs_rust_XXXXXX",td);
  _mktemp_s(tmpdir,sizeof(tmpdir));_mkdir(tmpdir);
  char dir[4096];snprintf(dir,sizeof(dir),"%s",tmpdir);
  /* Write Cargo.toml */
  char tp[4096];snprintf(tp,sizeof(tp),"%s/Cargo.toml",dir);
  FILE*f=fopen(tp,"w");if(!f){rmdir(dir);return JS_NULL;}
  fprintf(f,"[package]
name = \"%s_bridge\"
version = \"0.1.0\"
edition = \"2021\"

",name);
  fprintf(f,"[lib]
crate-type = [\"cdylib\"]

");
  fprintf(f,"[dependencies]
serde_json = \"1\"
%s = \"*\"
",name);
  fprintf(f,"futures = \"0.3\"
");fclose(f);
  /* Write src/lib.rs */
  snprintf(tp,sizeof(tp),"%s/src",dir);
  _mkdir(tp);
  snprintf(tp,sizeof(tp),"%s/src/lib.rs",dir);
  f=fopen(tp,"w");if(!f){rmdir(dir);return JS_NULL;}
  fprintf(f,"use std::collections::HashMap;
");
  fprintf(f,"type RustFn = fn(Vec<serde_json::Value>)->Result<serde_json::Value,String>;
");
  fprintf(f,"#[no_mangle]
pub extern \"C\" fn rust_bridge_get_fns()->*mut HashMap<String,RustFn>{
");
  fprintf(f,"  let mut m:HashMap<String,RustFn> = HashMap::new();
");
  fprintf(f,"  Box::into_raw(Box::new(m))
}");
  fclose(f);
  /* Cargo build */
  snprintf(tp,sizeof(tp),"cd %s && cargo build --release 2>&1",dir);
  if(system(tp)!=0){rmdir(dir);return JS_NULL;}
  /* Load the .dll/.so */
  char libpath[4096];
  snprintf(libpath,sizeof(libpath),"%s/target/release/%s_bridge.dll",dir,name);
  HMODULE lib=LoadLibraryA(libpath);
  if(!lib){rmdir(dir);return JS_NULL;}
  typedef void*(*GetFns)();GetFns gf=(GetFns)GetProcAddress(lib,"rust_bridge_get_fns");
  if(!gf){FreeLibrary(lib);rmdir(dir);return JS_NULL;}
  JSValue obj=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,obj,"__rust_crate",JS_NewString(ctx,libpath));
  return obj;}

static void cleanup_qjs(void){
  /* Free cached module names */
  for(int i=0;i<g_cache_count;i++){free((void*)g_cache_names[i]);JS_FreeValue(g_ctx,g_cache_exports[i]);}
  g_cache_count=0;
  if(g_ctx){for(int i=0;i<MAX_OBJS;i++){if(!JS_IsNull(g_objs[i])&&!JS_IsUndefined(g_objs[i]))JS_FreeValue(g_ctx,g_objs[i]);g_objs[i]=JS_NULL;}
    JS_FreeContext(g_ctx);g_ctx=NULL;}
  if(g_rt){JS_FreeRuntime(g_rt);g_rt=NULL;}
  g_inited=0;g_next_id=2;}

static void* qjs_call(void*mod,const char*fn,const char*ajs){
  if(!mod||!g_ctx||!g_inited)return NULL;
  int oid=(int)(intptr_t)mod;
  if(oid<0||oid>=MAX_OBJS||JS_IsNull(g_objs[oid])||JS_IsUndefined(g_objs[oid]))return NULL;
  JSValue obj=g_objs[oid];
  JSValue func=fn&&fn[0]?JS_GetPropertyStr(g_ctx,obj,fn):JS_DupValue(g_ctx,obj);
  if(JS_IsException(func))return NULL;
  if(!JS_IsFunction(g_ctx,func)){
    if(JS_IsObject(func)){int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,func);return NULL;}g_objs[nid]=func;return(void*)(intptr_t)nid;}
    char*j=jsval_to_json(g_ctx,func);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,func);return h;}
  JSValue av=JS_NULL;JSValue*argv=NULL;int argc=0;
  if(ajs&&ajs[0]&&strcmp(ajs,"null")!=0){
    av=JS_ParseJSON(g_ctx,ajs,strlen(ajs),"<a>");
    if(!JS_IsException(av)&&JS_IsArray(g_ctx,av)){
      JSValue lv=JS_GetPropertyStr(g_ctx,av,"length");JS_ToInt32(g_ctx,&argc,lv);JS_FreeValue(g_ctx,lv);
      if(argc>0){argv=(JSValue*)malloc((size_t)argc*sizeof(JSValue));for(int i=0;i<argc;i++)argv[i]=JS_GetPropertyUint32(g_ctx,av,i);}
      JS_FreeValue(g_ctx,av);
    }else if(!JS_IsException(av)){argc=1;argv=(JSValue*)malloc(sizeof(JSValue));argv[0]=av;}
  }
  JSValue r=JS_Call(g_ctx,func,obj,argc,argv);JS_FreeValue(g_ctx,func);
  if(argv){for(int i=0;i<argc;i++)JS_FreeValue(g_ctx,argv[i]);free(argv);}
  if(JS_IsException(r))return NULL;
  if(JS_IsObject(r)||JS_IsFunction(g_ctx,r)){
    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,r);return NULL;}
    g_objs[nid]=JS_DupValue(g_ctx,r);JS_FreeValue(g_ctx,r);return(void*)(intptr_t)nid;}
  char*j=jsval_to_json(g_ctx,r);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,r);return h;}

EXPORT void* left_pad_require(void){
  if(!g_inited&&!init_qjs("left-pad_npm"))return NULL;
  if(g_ctx&&!g_loaded){
    for(int i=0;i<MAX_OBJS;i++)g_objs[i]=JS_NULL;
    JSValue g=JS_GetGlobalObject(g_ctx);
    JSValue rv=JS_GetPropertyStr(g_ctx,g,"require");
    if(JS_IsFunction(g_ctx,rv)){
      JSValue nm=JS_NewString(g_ctx,"left-pad");
      JSValue md=JS_Call(g_ctx,rv,JS_UNDEFINED,1,&nm);JS_FreeValue(g_ctx,nm);
      if(!JS_IsException(md)){g_objs[1]=JS_DupValue(g_ctx,md);JS_FreeValue(g_ctx,md);}
    }
    JS_FreeValue(g_ctx,rv);JS_FreeValue(g_ctx,g);`n    g_loaded=1;
  }
  return g_inited?(void*)(intptr_t)1:NULL;}

EXPORT void left_pad_free(void*h){
  if(!h||(intptr_t)h<2)return;int id=(int)(intptr_t)h;
  if(id<MAX_OBJS){if(!JS_IsNull(g_objs[id])&&!JS_IsUndefined(g_objs[id]))JS_FreeValue(g_ctx,g_objs[id]);g_objs[id]=JS_NULL;}}

EXPORT void left_pad_free_cstr(void*s){if(s)free(s);}

EXPORT void* left_pad_str(const char*s){
  if(!s)return store_json("null");char esc[8192];json_escape(s,esc,sizeof(esc));
  char buf[8194];snprintf(buf,sizeof(buf),"\"%s\"",esc);return store_json(buf);}

EXPORT void* left_pad_int(long long v){
  char buf[64];snprintf(buf,sizeof(buf),"%lld",v);return store_json(buf);}

EXPORT void* left_pad_float(double v){
  char buf[64];snprintf(buf,sizeof(buf),"%g",v);return store_json(buf);}

EXPORT char* left_pad_to_cstr(void*obj){
  if(!obj)return NULL;const char*j=get_json(obj);
  char*out=(char*)malloc(strlen(j)+1);strcpy(out,j);return out;}

EXPORT void* left_pad_tuple2(void*a,void*b){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s]",get_json(a),get_json(b));return store_json(buf);}
EXPORT void* left_pad_tuple3(void*a,void*b,void*c){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s]",get_json(a),get_json(b),get_json(c));return store_json(buf);}
EXPORT void* left_pad_tuple4(void*a,void*b,void*c,void*d){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d));return store_json(buf);}
EXPORT void* left_pad_tuple(void**items,int count){
  if(count<=0)return store_json("[]");char*b=(char*)malloc(16384);if(!b)return NULL;
  int p=0;p+=snprintf(b+p,16384-p,"[");for(int i=0;i<count;i++){if(i>0)p+=snprintf(b+p,16384-p,",");p+=snprintf(b+p,16384-p,"%s",get_json(items[i]));}
  p+=snprintf(b+p,16384-p,"]");void*h=store_json(b);free(b);return h;}
EXPORT void* left_pad_list2(void*a,void*b){return left_pad_tuple2(a,b);}
EXPORT void* left_pad_list3(void*a,void*b,void*c){return left_pad_tuple3(a,b,c);}
EXPORT void* left_pad_list4(void*a,void*b,void*c,void*d){return left_pad_tuple4(a,b,c,d);}
EXPORT void* left_pad_list(void**items,int count){return left_pad_tuple(items,count);}
EXPORT void* left_pad_dict(void){return store_json("{}");}
EXPORT int left_pad_dict_set(void*d,const char*key,void*val){
  if(!d||!key)return -1;char**pp=(char**)d;const char*vj=get_json(val);
  char ek[512];json_escape(key,ek,sizeof(ek));char entry[2048];
  int el=(int)snprintf(entry,sizeof(entry),"\"%s\":%s",ek,vj);if(el<0)return -1;
  size_t ol=strlen(*pp);
  if(ol<=2){char*ns=(char*)malloc((size_t)el+3);snprintf(ns,(size_t)el+3,"{%s}",entry);free(*pp);*pp=ns;}
  else{size_t nl=ol+(size_t)el+2;char*ns=(char*)malloc(nl);snprintf(ns,nl,"%.*s,%s}",(int)(ol-1),*pp,entry);free(*pp);*pp=ns;}
  return 0;}

EXPORT void* left_pad_call(void*mod,const char*fn,void*args){
  if(!args)return qjs_call(mod,fn,"[]");return qjs_call(mod,fn,get_json(args));}

EXPORT void* left_pad_call1(void*mod,const char*fn,void*a){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s]",get_json(a));return qjs_call(mod,fn,buf);}
EXPORT void* left_pad_call2(void*mod,const char*fn,void*a,void*b){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s]",get_json(a),get_json(b));return qjs_call(mod,fn,buf);}
EXPORT void* left_pad_call3(void*mod,const char*fn,void*a,void*b,void*c){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s]",get_json(a),get_json(b),get_json(c));return qjs_call(mod,fn,buf);}
EXPORT void* left_pad_call4(void*mod,const char*fn,void*a,void*b,void*c,void*d){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d));return qjs_call(mod,fn,buf);}
EXPORT void* left_pad_call5(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e));return qjs_call(mod,fn,buf);}
EXPORT void* left_pad_call6(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e,void*f){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e),get_json(f));return qjs_call(mod,fn,buf);}
EXPORT void* left_pad_call_kw(void*mod,const char*fn,void*args,void*kwargs){
  const char*as=get_json(args);const char*ks=get_json(kwargs);size_t al=strlen(as);char buf[65536];
  if(al>=2&&al<=32768)snprintf(buf,sizeof(buf),"%.*s,%s]",(int)(al-1),as,ks);
  else snprintf(buf,sizeof(buf),"[%s]",ks);return qjs_call(mod,fn,buf);}

EXPORT void* left_pad_getattr(void*obj,const char*name){
  if(!obj||!name||!g_ctx||!g_inited)return NULL;
  int oid=(int)(intptr_t)obj;
  if(oid<0||oid>=MAX_OBJS||JS_IsNull(g_objs[oid])||JS_IsUndefined(g_objs[oid]))return NULL;
  JSValue v=JS_GetPropertyStr(g_ctx,g_objs[oid],name);
  if(JS_IsException(v))return NULL;
  if(JS_IsObject(v)||JS_IsFunction(g_ctx,v)){
    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,v);return NULL;}
    g_objs[nid]=JS_DupValue(g_ctx,v);JS_FreeValue(g_ctx,v);return(void*)(intptr_t)nid;}
  char*j=jsval_to_json(g_ctx,v);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,v);return h;}

BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID l){if(r==DLL_PROCESS_DETACH)cleanup_qjs();return TRUE;}
