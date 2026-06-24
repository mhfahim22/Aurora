/* Auto-generated QuickJS npm bridge DLL for got */
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
static char g_last_error[4096]="";
static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}
/* Performance counters */
static struct{unsigned require_calls;unsigned bc_hits;unsigned bc_misses;
  unsigned exports_lookups;unsigned eval_time_ms;unsigned resolve_time_ms;}g_perf={0};

static const char node_builtins_js[] = "// Node.js built-in polyfills for QuickJS bridge\n// These are registered into __node_builtins so the C require() can intercept them.\n\n(function() {\n  var builtins = {};\n\n  // ── path ──────────────────────────────────────────────────\n  builtins.path = (function() {\n    var sep = '/', delimiter = ':';\n    function normalize(p) {\n      if (!p) return '.';\n      var isAbs = p[0] === '/';\n      var parts = p.split('/'), out = [];\n      for (var i = 0; i < parts.length; i++) {\n        if (parts[i] === '' || parts[i] === '.') continue;\n        if (parts[i] === '..') { if (out.length && out[out.length-1] !== '..') out.pop(); else if (!isAbs) out.push('..'); }\n        else out.push(parts[i]);\n      }\n      var r = isAbs ? '/' + out.join('/') : out.join('/');\n      return r || (isAbs ? '/' : '.');\n    }\n    function join() { return normalize(Array.prototype.slice.call(arguments).filter(function(x){return x!=null}).join('/')); }\n    function resolve() {\n      var parts = Array.prototype.slice.call(arguments).filter(function(x){return x!=null});\n      if (!parts.length) return __dirname || '/';\n      if (parts[0][0] === '/') return normalize(parts.join('/'));\n      return normalize((__dirname || '/') + '/' + parts.join('/'));\n    }\n    function dirname(p) {\n      if (!p) return '.';\n      var i = p.lastIndexOf('/');\n      return i === -1 ? '.' : (i === 0 ? '/' : p.substring(0, i));\n    }\n    function basename(p, ext) {\n      if (!p) return '';\n      var base = p.substring(p.lastIndexOf('/') + 1);\n      return ext && base.endsWith(ext) ? base.substring(0, base.length - ext.length) : base;\n    }\n    function extname(p) {\n      if (!p) return '';\n      var base = p.substring(p.lastIndexOf('/') + 1);\n      var d = base.lastIndexOf('.');\n      return d <= 0 ? '' : base.substring(d);\n    }\n    return { sep:sep, delimiter:delimiter, normalize:normalize, join:join, resolve:resolve,\n             dirname:dirname, basename:basename, extname:extname, isAbsolute:function(p){return p&&p[0]=='/'},\n             relative:function(f,t){var a=normalize(f).split('/'),b=normalize(t).split('/'),i=0;\n               while(i<a.length&&i<b.length&&a[i]===b[i])i++;\n               var u=[];for(var j=i;j<a.length;j++)u.push('..');\n               return u.join('/')+(u.length&&b.slice(i).length?'/':'')+b.slice(i).join('/');},\n             parse:function(p){var r={root:'/',dir:dirname(p),base:basename(p),ext:extname(p),name:basename(p,extname(p))};return r;} };\n  })();\n\n  // ── buffer ────────────────────────────────────────────────\n  builtins.buffer = (function() {\n    function Buffer(arg, encoding) {\n      if (typeof arg === 'number') { this._data=new Array(arg);for(var i=0;i<arg;i++)this._data[i]=0;this.length=arg; }\n      else if (typeof arg === 'string') {\n        this._data=encoding==='hex'?(function(h){var b=[];for(var i=0;i<h.length;i+=2)b.push(parseInt(h.substring(i,i+2),16));return b;})(arg)\n          :(function(s){var b=[];for(var i=0;i<s.length;i++){var c=s.charCodeAt(i);\n            if(c<0x80)b.push(c);else if(c<0x800){b.push(0xC0|(c>>6));b.push(0x80|(c&0x3F));}else{b.push(0xE0|(c>>12));b.push(0x80|((c>>6)&0x3F));b.push(0x80|(c&0x3F));}}\n          return b;})(arg);\n        this.length=this._data.length;\n      } else if (arg && arg.length !== undefined) { this._data=[];for(var i=0;i<arg.length;i++)this._data[i]=arg[i]&0xFF;this.length=arg.length; }\n    }\n    Buffer.prototype.toString=function(enc,s,e){\n      s=s||0;e=e||this.length;\n      if(enc==='hex'){var h='';for(var i=s;i<e;i++){var v=this._data[i];h+=(v<16?'0':'')+v.toString(16);}return h;}\n      var out='';for(var i=s;i<e;i++)out+=String.fromCharCode(this._data[i]);return out;\n    };\n    Buffer.prototype.slice=function(s,e){return new Buffer(this._data.slice(s,e));};\n    Buffer.prototype.equals=function(o){if(!o||this.length!==o.length)return false;for(var i=0;i<this.length;i++)if(this._data[i]!==o._data[i])return false;return true;};\n    Buffer.prototype.write=function(s,off,len,enc){\n      off=off||0;var b=Buffer.from(s,enc);for(var i=0;i<b.length&&i<(len||b.length);i++)this._data[off+i]=b._data[i];\n    };\n    Buffer.from=function(a,e){return new Buffer(a,e);};\n    Buffer.alloc=function(s,f){var b=new Buffer(s);if(f!==undefined){var c=typeof f==='number'?f:f.charCodeAt(0);for(var i=0;i<s;i++)b._data[i]=c;}return b;};\n    Buffer.concat=function(l){var t=0;for(var i=0;i<l.length;i++)t+=l[i].length;var d=new Array(t),p=0;for(var i=0;i<l.length;i++){for(var j=0;j<l[i].length;j++)d[p++]=l[i]._data[j];}var r=new Buffer(0);r._data=d;r.length=t;return r;};\n    Buffer.isBuffer=function(o){return o instanceof Buffer;};\n    Buffer.isEncoding=function(enc){if(!enc)return false;enc=enc.toLowerCase();return['utf8','utf-8','utf16le','ucs2','ucs-2','base64','base64url','hex','ascii','latin1','binary'].indexOf(enc)!==-1;};\n    Buffer.compare=function(a,b){if(!a||!b)return 0;var len=Math.min(a.length,b.length);for(var i=0;i<len;i++){if(a._data[i]!==b._data[i])return a._data[i]-b._data[i];}return a.length-b.length;};\n    Buffer.transcode=function(source,fromEnc,toEnc){var str=source.toString(fromEnc);return Buffer.from(str,toEnc);};\n    Buffer.prototype.copy=function(t,tS,sS,eS){sS=sS||0;eS=eS||this.length;for(var i=sS;i<eS;i++)t._data[tS++]=this._data[i];};\n    return {Buffer:Buffer,SlowBuffer:Buffer,INSPECT_MAX_BYTES:50};\n  })();\n\n  // ── events ────────────────────────────────────────────────\n  builtins.events = (function() {\n    function EventEmitter(){this._events={};}\n    EventEmitter.prototype.on=EventEmitter.prototype.addListener=function(ev,fn){if(!this._events[ev])this._events[ev]=[];this._events[ev].push(fn);return this;};\n    EventEmitter.prototype.once=function(ev,fn){var s=this;function w(){fn.apply(this,arguments);s.removeListener(ev,w);}w._orig=fn;return this.on(ev,w);};\n    EventEmitter.prototype.emit=function(ev){var f=this._events[ev];if(!f||!f.length)return false;var a=Array.prototype.slice.call(arguments,1);for(var i=0;i<f.length;i++)f[i].apply(this,a);return true;};\n    EventEmitter.prototype.removeListener=function(ev,fn){var f=this._events[ev];if(!f)return this;this._events[ev]=f.filter(function(x){return x!==fn&&x._orig!==fn;});return this;};\n    EventEmitter.prototype.removeAllListeners=function(ev){if(ev)delete this._events[ev];else this._events={};return this;};\n    EventEmitter.prototype.listeners=function(ev){return(this._events[ev]||[]).slice();};\n    EventEmitter.prototype.eventNames=function(){return Object.keys(this._events);};\n    EventEmitter.prototype.listenerCount=function(ev){return(this._events[ev]||[]).length;};\n    return {EventEmitter:EventEmitter,usingDomains:false};\n  })();\n\n  // ── util ──────────────────────────────────────────────────\n  builtins.util = (function() {\n    function format(f) {\n      if(typeof f!=='string')return inspect(f);\n      var a=Array.prototype.slice.call(arguments,1);\n      return f.replace(/%[sdifjoO%]/g,function(m){\n        if(m==='%%')return'%';if(!a.length)return m;\n        var v=a.shift();\n        if(m==='%s')return String(v);if(m==='%d'||m==='%i')return parseInt(v,10);\n        if(m==='%f')return parseFloat(v);return inspect(v);\n      });\n    }\n    function inspect(o,d){if(o===null)return'null';if(o===undefined)return'undefined';\n      if(typeof o==='string')return \"'\"+o.replace(/'/g,\"\\\\'\")+\"'\";\n      if(typeof o==='number'||typeof o==='boolean')return String(o);\n      if(Array.isArray(o))return'['+o.map(function(x){return inspect(x);}).join(', ')+']';\n      if(typeof o==='object'){var k=Object.keys(o);return'{'+k.map(function(x){return x+': '+inspect(o[x]);}).join(', ')+'}';}\n      return String(o);\n    }\n    function inherits(c,s){c.prototype=Object.create(s.prototype,{constructor:{value:c,writable:true,configurable:true}});}\n    return {format:format,inspect:inspect,inherits:inherits,deprecate:function(f){return f;},\n      isArray:Array.isArray,isString:function(s){return typeof s==='string';},isNumber:function(n){return typeof n==='number';},\n      isFunction:function(f){return typeof f==='function';},isObject:function(o){return o!==null&&typeof o==='object';},\n      isNull:function(o){return o===null;},isUndefined:function(o){return o===undefined;},\n      isBuffer:function(o){return o&&o.constructor&&o.constructor.name==='Buffer';},\n      isDate:function(o){return o instanceof Date;},isRegExp:function(o){return o instanceof RegExp;},\n      promisify:function(fn){return function(){var args=Array.prototype.slice.call(arguments);var self=this;return new Promise(function(resolve,reject){args.push(function(err,val){if(err)reject(err);else resolve(val);});fn.apply(self,args);});};},\n      callbackify:function(fn){return function(){var args=Array.prototype.slice.call(arguments);var cb=args.pop();fn.apply(this,args).then(function(v){cb(null,v);},function(e){cb(e);});};}};\n  })();\n\n  // ── assert ────────────────────────────────────────────────\n  builtins.assert = (function() {\n    function AssertionError(o){this.name='AssertionError';this.message=o.message||'';this.actual=o.actual;this.expected=o.expected;this.stack=(new Error()).stack;}\n    AssertionError.prototype=Object.create(Error.prototype);\n    function ok(v,m){if(!v)throw new AssertionError({message:m||'assertion failed',actual:v,expected:true});}\n    function equal(a,b,m){if(a!=b)throw new AssertionError({message:m||'not equal',actual:a,expected:b});}\n    function strictEqual(a,b,m){if(a!==b)throw new AssertionError({message:m||'strict not equal',actual:a,expected:b});}\n    function deepEqual(a,b,m){\n      if(a===b)return;\n      if(!a||!b||typeof a!==typeof b)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});\n      if(Array.isArray(a)&&Array.isArray(b)){if(a.length!==b.length)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});for(var i=0;i<a.length;i++)deepEqual(a[i],b[i]);return;}\n      if(typeof a==='object'){var ka=Object.keys(a),kb=Object.keys(b);if(ka.length!==kb.length)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});for(var i=0;i<ka.length;i++)deepEqual(a[ka[i]],b[ka[i]]);return;}\n      if(a!==b)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});\n    }\n    function throws(fn,err){try{fn();}catch(e){if(err&&!err(e))throw e;return e;}throw new AssertionError({message:'did not throw'});}\n    function ifError(e){if(e)throw e;}\n    return {AssertionError:AssertionError,ok:ok,equal:equal,strictEqual:strictEqual,deepEqual:deepEqual,\n      notEqual:function(a,b,m){if(a==b)throw new AssertionError({message:m||'equal',actual:a,expected:b});},\n      notStrictEqual:function(a,b,m){if(a===b)throw new AssertionError({message:m||'strict equal',actual:a,expected:b});},\n      notDeepEqual:function(a,b,m){try{deepEqual(a,b);}catch(e){return;}throw new AssertionError({message:m||'deep equal',actual:a,expected:b});},\n      throws:throws,ifError:ifError,doesNotThrow:function(fn,m){try{fn();}catch(e){throw new AssertionError({message:m||'threw',actual:e});}}}};\n  })();\n\n  // ── os ────────────────────────────────────────────────────\n  builtins.os = (function() {\n    var p = typeof process!=='undefined'&&process.platform?process.platform:\n            typeof navigator!=='undefined'?'browser':'linux';\n    return {hostname:function(){return'localhost';},homedir:function(){return typeof process!=='undefined'&&process.env?process.env.HOME||'/home/user':'/home/user';},\n      platform:function(){return p;},type:function(){return p==='win32'?'Windows_NT':'Linux';},\n      release:function(){return'1.0.0';},arch:function(){return'x64';},tmpdir:function(){return'/tmp';},\n      EOL:'\\n',cpus:function(){return[{model:'QuickJS',speed:1000,times:{user:0,nice:0,sys:0,idle:0,irq:0}}];},\n      totalmem:function(){return 1073741824;},freemem:function(){return 536870912;},uptime:function(){return 0;},\n      loadavg:function(){return[0,0,0];},endianness:function(){return'LE';},networkInterfaces:function(){return{lo:[{address:'127.0.0.1',netmask:'255.0.0.0',family:'IPv4',internal:true}]};}};\n  })();\n\n  // ── querystring ───────────────────────────────────────────\n  builtins.querystring = (function() {\n    function parse(s){if(!s)return{};var r={};s.split('&').forEach(function(p){var kv=p.split('=');if(kv[0])r[decodeURIComponent(kv[0].replace(/\\+/g,' '))]=kv[1]?decodeURIComponent(kv[1].replace(/\\+/g,' ')):'';});return r;}\n    function stringify(o){return Object.keys(o).map(function(k){return encodeURIComponent(k)+'='+encodeURIComponent(String(o[k]));}).join('&');}\n    return {parse:parse,stringify:stringify,encode:stringify,decode:parse,escape:encodeURIComponent,unescape:decodeURIComponent};\n  })();\n\n  // ── url (WHATWG URL) ───────────────────────────────────────\n  builtins.url = (function() {\n    function URL(url, base) {\n      if (!url) throw new TypeError('Invalid URL');\n      if (base && typeof base==='string'&&base!==''){var b=new URL(base);if(url[0]==='/')url=b.protocol+'//'+b.host+url;\n        else if(url.indexOf('://')===-1){var bp=b.pathname;var l=Math.max(bp.lastIndexOf('/'),bp.lastIndexOf('\\\\'));if(l>0)bp=bp.substring(0,l);else bp='/';url=b.protocol+'//'+b.host+bp+'/'+url;}}\n      var m=url.match(/^([a-zA-Z][a-zA-Z0-9+.-]*:)?(\\/\\/)?([^\\/?#]*)?([^?#]*)?(\\?[^#]*)?(#.*)?$/);\n      if(!m)throw new TypeError('Invalid URL: '+url);\n      this.protocol=m[1]||'https:';var auth='',host='',port='';\n      if(m[3]){var at=m[3].indexOf('@');if(at>=0){auth=m[3].substring(0,at);host=m[3].substring(at+1);}else host=m[3];}\n      var hc=host.lastIndexOf(':');if(hc>=0&&host.indexOf(']')<hc){port=host.substring(hc+1);host=host.substring(0,hc);}\n      this.hostname=host;this.port=port||'';this.host=host+(port?':'+port:'');\n      this.href=url;this.origin=this.protocol+'//'+this.host;\n      this.pathname=m[4]||'/';this.search=m[5]||'';this.hash=m[6]||'';\n      this.username='';this.password='';\n      if(auth){var c=auth.indexOf(':');if(c>=0){this.username=auth.substring(0,c);this.password=auth.substring(c+1);}else this.username=auth;}\n      var sp=new (builtins.url.URLSearchParams||function(){})();\n      if(this.search.length>1)this.search.substring(1).split('&').forEach(function(p){var kv=p.split('=');sp.append(decodeURIComponent(kv[0]),kv[1]?decodeURIComponent(kv[1]):'');});\n      this.searchParams=sp;\n    }\n    URL.prototype.toString=function(){return this.href;};\n    URL.prototype.toJSON=function(){return this.href;};\n    function URLSearchParams(q){this._p={};this._keys=[];if(q){var s=typeof q==='string'?q:'';if(s.length>0)s.split('&').forEach(function(p){var kv=p.split('=');var k=decodeURIComponent(kv[0]);var v=kv[1]?decodeURIComponent(kv[1]):'';this.append(k,v);},this);}}\n    URLSearchParams.prototype.append=function(k,v){if(!this._p[k]){this._p[k]=[];this._keys.push(k);}this._p[k].push(v);};\n    URLSearchParams.prototype.get=function(k){return this._p[k]?this._p[k][0]:null;};\n    URLSearchParams.prototype.getAll=function(k){return this._p[k]||[];};\n    URLSearchParams.prototype.has=function(k){return !!this._p[k];};\n    URLSearchParams.prototype.set=function(k,v){if(!this._p[k])this._keys.push(k);this._p[k]=[v];};\n    URLSearchParams.prototype.delete=function(k){delete this._p[k];var idx=this._keys.indexOf(k);if(idx>=0)this._keys.splice(idx,1);};\n    URLSearchParams.prototype.toString=function(){var self=this;var parts=[];this._keys.forEach(function(k){self._p[k].forEach(function(v){parts.push(encodeURIComponent(k)+(v?'='+encodeURIComponent(v):''));});});return parts.join('&');};\n    URLSearchParams.prototype.forEach=function(fn){var self=this;this._keys.forEach(function(k){self._p[k].forEach(function(v){fn(v,k,self);});});};\n    URLSearchParams.prototype.keys=function(){var self=this;return{next:function(){return{value:self._keys[self._idx++]||undefined,done:self._idx>self._keys.length};},[Symbol.iterator]:function(){return this;}};};\n    URLSearchParams.prototype.values=function(){/* stub */return[];};\n    URLSearchParams.prototype.entries=function(){/* stub */return[];};\n    return {URL:URL,URLSearchParams:URLSearchParams,urlObject:URL,format:function(u){return u.href||String(u);},\n      resolve:function(f,t){return (new URL(t,f)).href;},\n      parse:function(u,p){try{var r=new URL(u);return r;}catch(e){return null;}},\n      fileURLToPath:function(u){return u.pathname;},pathToFileURL:function(p){return new URL('file://'+p);}};\n  })();\n\n  // ── crypto (real SHA256/SHA1/MD5 via Windows CNG, real CSPRNG) ───\n  builtins.crypto = (function() {\n    var bH = typeof __bridge_crypto_hash !== 'undefined';\n    function createHash(algo) {\n      var state = '';\n      return {update:function(data){state+=typeof data==='string'?data:data.toString();return this;},\n        digest:function(enc){\n          if(bH){\n            var hex = __bridge_crypto_hash(algo.toLowerCase(), state);\n            if(enc==='hex') return hex;\n            return Buffer.from(hex, 'hex');\n          }\n          // Fallback: FNV-1a if native hash unavailable\n          var hash=0x811C9DC5|0;\n          for(var i=0;i<state.length;i++){hash^=state.charCodeAt(i);hash=(hash*0x01000193)|0;}\n          var hex=(hash>>>0).toString(16);\n          while(hex.length<8)hex='0'+hex;\n          return enc==='hex'?hex:Buffer.from(hex,'hex');\n        },copy:function(){var n=createHash(algo);n.state=state;return n;}\n      };\n    }\n    function randomBytes(size){\n      if(typeof __bridge_crypto_random !== 'undefined'){\n        var hex = __bridge_crypto_random(size);\n        return Buffer.from(hex, 'hex');\n      }\n      var b=Buffer.alloc(size);for(var i=0;i<size;i++)b._data[i]=Math.floor(Math.random()*256);return b;\n    }\n    return {createHash:createHash,createHmac:function(a,k){return createHash(a);},\n      randomBytes:randomBytes,pseudoRandomBytes:randomBytes,\n      randomFill:function(b,o,s,cb){var r=randomBytes(s);for(var i=0;i<s;i++)b._data[o+i]=r._data[i];if(cb)cb(null,b);},\n      getCiphers:function(){return[];},getHashes:function(){return['sha1','sha256','md5'];}};\n  })();\n\n  // ── stream (with backpressure) ────────────────────────────\n  builtins.stream = (function() {\n    var EE = builtins.events.EventEmitter;\n    var hwm = 16384;\n    function Readable(opts){\n      EE.call(this);\n      opts=opts||{};\n      this._readableState={highWaterMark:opts.highWaterMark||hwm,buffer:[],flowing:null,ended:false,endedEmitted:false};\n    }\n    Readable.prototype=Object.create(EE.prototype);\n    Readable.prototype._read=function(n){};\n    Readable.prototype.read=function(n){\n      var st=this._readableState;\n      if(st.ended&&!st.buffer.length){if(!st.endedEmitted){st.endedEmitted=true;this.emit('end');}return null;}\n      if(st.buffer.length){var c=st.buffer.shift();this.emit('data',c);return c;}\n      return null;\n    };\n    Readable.prototype.push=function(c){\n      if(c===null){this._readableState.ended=true;return;}\n      this._readableState.buffer.push(c);\n    };\n    Readable.prototype.pipe=function(dest,opts){\n      var self=this;\n      dest.emit('pipe',self);\n      function ondata(c){var r=dest.write(c);if(r===false)self.pause();}\n      function ondrain(){self.resume();}\n      function onend(){dest.end();}\n      self.on('data',ondata);\n      dest.on('drain',ondrain);\n      dest.on('close',function(){self.removeListener('data',ondata);dest.removeListener('drain',ondrain);});\n      self.on('end',onend);\n      return dest;\n    };\n    Readable.prototype.pause=function(){this._readableState.flowing=false;};\n    Readable.prototype.resume=function(){\n      if(this._readableState.flowing)return;\n      this._readableState.flowing=true;\n      var self=this;\n      function flow(){\n        while(self._readableState.flowing&&self._readableState.buffer.length){\n          var c=self._readableState.buffer.shift();\n          self.emit('data',c);\n        }\n        if(!self._readableState.buffer.length&&self._readableState.ended){\n          self._readableState.endedEmitted=true;self.emit('end');return;\n        }\n      }\n      flow();\n    };\n    Readable.prototype.isPaused=function(){return this._readableState.flowing===false;};\n    Readable.prototype.setEncoding=function(e){};\n    Readable.prototype.destroy=function(){this.emit('close');};\n    function Writable(opts){\n      EE.call(this);\n      opts=opts||{};\n      this._writableState={highWaterMark:opts.highWaterMark||hwm,length:0,writing:false,ended:false,needDrain:false};\n    }\n    Writable.prototype=Object.create(EE.prototype);\n    Writable.prototype._write=function(c,e,cb){cb();};\n    Writable.prototype.write=function(c,e,cb){\n      if(typeof e==='function'){cb=e;e='utf8';}\n      var st=this._writableState;\n      if(st.ended){if(cb)process.nextTick(cb);return false;}\n      var self=this;\n      st.writing=true;\n      st.length+=c.length||1;\n      this._write(c,e,function(err){\n        if(err)self.emit('error',err);\n        st.writing=false;\n        st.length-=c.length||1;\n        if(st.length<=st.highWaterMark&&st.needDrain){st.needDrain=false;self.emit('drain');}\n        if(cb)process.nextTick(function(){cb(err);});\n      });\n      var ret=st.length<st.highWaterMark;\n      if(!ret)st.needDrain=true;\n      return ret;\n    };\n    Writable.prototype.cork=function(){};\n    Writable.prototype.uncork=function(){};\n    Writable.prototype.end=function(c,e,cb){\n      if(typeof c==='function'){cb=c;c=null;}\n      if(typeof e==='function'){cb=e;e='utf8';}\n      var st=this._writableState;\n      st.ended=true;\n      var self=this;\n      function finish(){\n        self.emit('finish');\n        if(cb)process.nextTick(cb);\n      }\n      if(c)this.write(c,e,function(){finish();});\n      else finish();\n    };\n    Writable.prototype.destroy=function(){this.emit('close');};\n    function Transform(opts){\n      Readable.call(this,opts);Writable.call(this,opts);\n      this._transformState={pending:null,readable:true};\n    }\n    Transform.prototype=Object.create(Readable.prototype);\n    Object.keys(Writable.prototype).forEach(function(k){Transform.prototype[k]=Writable.prototype[k];});\n    Transform.prototype._transform=function(c,e,cb){cb(null,c);};\n    Transform.prototype._flush=function(cb){cb();};\n    Transform.prototype.push=function(c,e){\n      if(c===null){this._readableState.ended=true;return false;}\n      this._readableState.buffer.push(c);\n      if(this._readableState.flowing)this.emit('data',c);\n      return this._writableState.length<this._writableState.highWaterMark;\n    };\n    Transform.prototype.write=function(c,e,cb){\n      if(typeof e==='function'){cb=e;e='buffer';}\n      var self=this;\n      this._transform(c,e||'buffer',function(err,d){\n        if(err){self.emit('error',err);if(cb)process.nextTick(function(){cb(err);});return;}\n        if(d!==null&&d!==undefined)self.push(d);\n        if(cb)process.nextTick(function(){cb(null);});\n      });\n      return this._writableState.length<this._writableState.highWaterMark;\n    };\n    function PassThrough(opts){Transform.call(this,opts);}\n    PassThrough.prototype=Object.create(Transform.prototype);\n    PassThrough.prototype._transform=function(c,e,cb){cb(null,c);};\n    function finished(stream,opts,cb){\n      if(typeof opts==='function'){cb=opts;opts={};}\n      var ended=false;\n      function onfinish(){cleanup();if(!ended){ended=true;if(cb)process.nextTick(cb);}}\n      function onerror(err){cleanup();if(!ended){ended=true;if(cb)process.nextTick(function(){cb(err);});}}\n      function onclose(){if(!ended){cleanup();ended=true;if(cb)process.nextTick(cb);}}\n      function cleanup(){stream.removeListener('finish',onfinish);stream.removeListener('end',onfinish);stream.removeListener('error',onerror);stream.removeListener('close',onclose);}\n      stream.on('finish',onfinish);stream.on('end',onfinish);stream.on('error',onerror);stream.on('close',onclose);\n    }\n    function pipeline(){\n      var streams=Array.prototype.slice.call(arguments);\n      var cb=typeof streams[streams.length-1]==='function'?streams.pop():null;\n      var i=0;\n      function next(err){\n        if(err){if(cb)process.nextTick(function(){cb(err);});return;}\n        if(i>=streams.length){if(cb)process.nextTick(cb);return;}\n        var s=streams[i++];\n        if(i<streams.length){var dest=streams[i];s.pipe(dest,{end:true});\n          s.on('error',function(e){dest.destroy();next(e);});\n          dest.on('error',function(e){s.destroy();next(e);});\n          s.on('end',function(){next();});}else{if(cb)process.nextTick(cb);}\n      }\n      next();return streams[0];\n    }\n    return {Readable:Readable,Writable:Writable,Transform:Transform,DynamicStream:Transform,PassThrough:PassThrough,Stream:Readable,finished:finished,pipeline:pipeline};\n  })();\n\n  // ── fs (sync only) ─────────────────────────────────────────\n  builtins.fs = (function() {\n    var bF = typeof __bridge_fs_readFile !== 'undefined';\n    function readFileSync(path, enc) {\n      if (!bF) throw new Error('fs not available');\n      var data = __bridge_fs_readFile(path);\n      if (data === null) throw new Error('ENOENT: ' + path);\n      return enc === 'utf8' || enc === 'utf-8' || !enc ? data : Buffer.from(data);\n    }\n    function writeFileSync(path, data, enc) {\n      if (!bF) throw new Error('fs not available');\n      var s = typeof data === 'string' ? data : data.toString();\n      return __bridge_fs_writeFile(path, s);\n    }\n    function existsSync(path) { return bF ? __bridge_fs_exists(path) : false; }\n    function mkdirSync(path, opts) { return bF ? __bridge_fs_mkdir(path) : false; }\n    function readdirSync(path) { return bF ? __bridge_fs_readdir(path) : null; }\n    function statSync(path) { return bF ? __bridge_fs_stat(path) : null; }\n    function unlinkSync(path) { return bF ? __bridge_fs_unlink(path) : false; }\n    function rmdirSync(path) { return bF ? __bridge_fs_unlink(path) : false; }\n    function appendFileSync(path, data, enc) {\n      var existing = '';\n      try { existing = readFileSync(path, 'utf8') || ''; } catch(e) {}\n      writeFileSync(path, existing + (typeof data === 'string' ? data : data.toString()), enc);\n    }\n    function realpathSync(path) { return path; }\n    var constants = { F_OK:0, R_OK:4, W_OK:2, X_OK:1 };\n    function accessSync(path, mode) {\n      if (!bF || !__bridge_fs_exists(path)) throw new Error('ENOENT: ' + path);\n    }\n    return {\n      readFileSync: readFileSync, writeFileSync: writeFileSync, existsSync: existsSync,\n      mkdirSync: mkdirSync, readdirSync: readdirSync, statSync: statSync,\n      unlinkSync: unlinkSync, rmdirSync: rmdirSync, appendFileSync: appendFileSync,\n      realpathSync: realpathSync, accessSync: accessSync, constants: constants,\n      promises: { access: function(p,m){try{accessSync(p,m);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        appendFile: function(p,d,o){try{appendFileSync(p,d,o&&o.encoding);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        copyFile: function(s,d){try{var data=readFileSync(s);writeFileSync(d,data);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        readFile: function(p,o){try{var enc=(o&&(o.encoding||o))||'utf8';return Promise.resolve(readFileSync(p,enc));}catch(e){return Promise.reject(e);}},\n        writeFile: function(p,d,o){try{var enc=o&&(o.encoding||o);writeFileSync(p,d,enc);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        mkdir: function(p,o){try{mkdirSync(p,o);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        readdir: function(p,o){try{return Promise.resolve(readdirSync(p));}catch(e){return Promise.reject(e);}},\n        stat: function(p){try{return Promise.resolve(statSync(p));}catch(e){return Promise.reject(e);}},\n        lstat: function(p){try{return Promise.resolve(statSync(p));}catch(e){return Promise.reject(e);}},\n        unlink: function(p){try{unlinkSync(p);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        rmdir: function(p,o){try{rmdirSync(p);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        rm: function(p,o){try{unlinkSync(p);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        rename: function(o,n){try{var d=readFileSync(o);writeFileSync(n,d);unlinkSync(o);return Promise.resolve();}catch(e){return Promise.reject(e);}},\n        realpath: function(p,o){try{return Promise.resolve(realpathSync(p));}catch(e){return Promise.reject(e);}},\n        symlink: function(t,p){return Promise.resolve();},link: function(e,p){return Promise.resolve();},\n        chmod: function(p,m){return Promise.resolve();} } };\n  })();\n\n  // ── net (TCP via WinSock2 C helpers) ──────────────────────\n  builtins.net = (function() {\n    var EE = builtins.events.EventEmitter;\n    var bN = typeof __bridge_net_connect !== 'undefined';\n    function Socket(opts){\n      EE.call(this);\n      opts=opts||{};\n      this._sockfd=null;\n      this._readableState={buffer:[]};\n      this.connecting=false;\n      this.destroyed=false;\n      this.bytesRead=0;\n      this.bytesWritten=0;\n      this.localAddress='';\n      this.localPort=0;\n      this.remoteAddress='';\n      this.remotePort=0;\n    }\n    Socket.prototype=Object.create(EE.prototype);\n    Socket.prototype.connect=function(port,host,cb){\n      var self=this;\n      if(typeof host==='function'){cb=host;host='localhost';}\n      if(typeof port==='object'){var o=port;port=o.port;host=o.host||'localhost';}\n      self.connecting=true;\n      self.remotePort=port;\n      self.remoteAddress=host;\n      try {\n        var fd=__bridge_net_connect(host,port);\n        if(fd<0){self.emit('error',new Error('connect ECONNREFUSED '+host+':'+port));return;}\n        self._sockfd=fd;\n        self.connecting=false;\n        self.emit('connect');\n        if(cb)cb();\n        // Start polling for data\n        self._poll();\n      }catch(e){self.emit('error',e);}\n    };\n    Socket.prototype._poll=function(){\n      var self=this;\n      function readLoop(){\n        if(self.destroyed||self._sockfd===null)return;\n        try {\n          var data=__bridge_net_read(self._sockfd);\n          if(data!==null&&data.length>0){\n            self.bytesRead+=data.length;\n            var b=Buffer.from(data,'utf8');\n            self._readableState.buffer.push(b);\n            if(self._readableState.flowing||self.listenerCount('data')>0){\n              self._readableState.buffer.shift();\n              self.emit('data',b);\n            }\n          }\n        }catch(e){self.emit('error',e);}\n        process.nextTick(readLoop);\n      }\n      readLoop();\n    };\n    Socket.prototype.write=function(c,e,cb){\n      if(self.destroyed){if(cb)cb(new Error('socket destroyed'));return false;}\n      if(typeof e==='function'){cb=e;e='utf8';}\n      var s=typeof c==='string'?c:c.toString(e||'utf8');\n      try {\n        var n=__bridge_net_write(this._sockfd,s);\n        this.bytesWritten+=n;\n        if(cb)process.nextTick(cb);\n      }catch(e){if(cb)process.nextTick(function(){cb(e);});return false;}\n      return true;\n    };\n    Socket.prototype.end=function(c,e,cb){\n      if(typeof c==='function'){cb=c;c=null;}\n      if(c)this.write(c,function(){this.destroy();if(cb)cb();});\n      else{this.destroy();if(cb)process.nextTick(cb);}\n    };\n    Socket.prototype.destroy=function(){\n      if(this.destroyed)return;\n      this.destroyed=true;\n      if(this._sockfd!==null){try{__bridge_net_close(this._sockfd);}catch(e){}this._sockfd=null;}\n      this.emit('close');\n    };\n    Socket.prototype.setEncoding=function(e){};\n    Socket.prototype.setNoDelay=function(n){};\n    Socket.prototype.setKeepAlive=function(n,d){};\n    Socket.prototype.pause=function(){this._readableState.flowing=false;};\n    Socket.prototype.resume=function(){this._readableState.flowing=true;};\n    Socket.prototype.address=function(){return{address:this.remoteAddress,family:'IPv4',port:this.remotePort};};\n    Socket.prototype.unref=function(){};\n    Socket.prototype.ref=function(){};\n    Socket.prototype.setTimeout=function(n,cb){if(cb)this.on('timeout',cb);};\n    function connect(port,host,cb){var s=new Socket();s.connect(port,host,cb);return s;}\n    function createConnection(port,host,cb){return connect(port,host,cb);}\n    return {Socket:Socket,connect:connect,createConnection:createConnection,createServer:function(){var s=new EE();s.listen=function(){};return s;},isIP:function(s){return 0;},isIPv4:function(s){return false;},isIPv6:function(s){return false;}};\n  })();\n\n  // ── dns (via getaddrinfo C helper) ────────────────────────\n  builtins.dns = (function() {\n    var bD = typeof __bridge_dns_lookup !== 'undefined';\n    function lookup(hostname, opts, cb){\n      if(typeof opts==='function'){cb=opts;opts={};}\n      if(!cb)return;\n      if(!bD){process.nextTick(function(){cb(new Error('dns not available'));});return;}\n      try {\n        var addr=__bridge_dns_lookup(hostname);\n        if(addr===null||addr===''){process.nextTick(function(){cb(new Error('ENOTFOUND '+hostname));});return;}\n        process.nextTick(function(){cb(null,addr,4);});\n      }catch(e){process.nextTick(function(){cb(e);});}\n    }\n    function resolve(hostname, rrtype, cb){\n      if(typeof rrtype==='function'){cb=rrtype;rrtype='A';}\n      lookup(hostname,function(err,addr,family){\n        if(err)cb(err);\n        else cb(null,[addr]);\n      });\n    }\n    function reverse(ip, cb){\n      if(!cb)return;\n      process.nextTick(function(){cb(new Error('ENOTSUP reverse lookup'));});\n    }\n    return {lookup:lookup,resolve:resolve,reverse:reverse,\n      resolve4:function(h,cb){resolve(h,'A',cb);},\n      resolve6:function(h,cb){resolve(h,'AAAA',cb);},\n      resolveMx:function(h,cb){resolve(h,'MX',cb);},\n      resolveTxt:function(h,cb){resolve(h,'TXT',cb);},\n      resolveSrv:function(h,cb){resolve(h,'SRV',cb);},\n      resolveNaptr:function(h,cb){resolve(h,'NAPTR',cb);},\n      resolveCname:function(h,cb){resolve(h,'CNAME',cb);},\n      setServers:function(s){}};\n  })();\n\n  // ── tls (stub — falls back to net) ────────────────────────\n  builtins.tls = (function() {\n    var net = builtins.net;\n    function TLSSocket(socket, opts){\n      var s = socket || new net.Socket();\n      s.authorized = true;\n      s.encrypted = true;\n      s.getCipher = function(){return{name:'TLS',version:'TLSv1.3'};};\n      s.getPeerCertificate = function(){return{};};\n      return s;\n    }\n    function connect(port,host,cb){\n      if(typeof host==='function'){cb=host;host=undefined;}\n      if(typeof port==='object'){var o=port;port=o.port;host=o.host;}\n      var s = net.connect(port,host||'localhost',function(){\n        if(cb)cb();\n      });\n      s.authorized=true;\n      s.encrypted=true;\n      return s;\n    }\n    return {TLSSocket:TLSSocket,connect:connect,createServer:function(){return{listen:function(){}};},getCiphers:function(){return[];},DEFAULT_ECDH_CURVE:'auto'};\n  })();\n\n  // ── child_process (minimal execSync) ──────────────────────\n  builtins.child_process = (function() {\n    var bE = typeof __bridge_exec !== 'undefined';\n    function execSync(cmd, opts) {\n      if (!bE) throw new Error('child_process not available');\n      var stdout = __bridge_exec(cmd);\n      if (stdout === null) throw new Error('exec failed: ' + cmd);\n      return { stdout: stdout, stderr: '', status: 0 };\n    }\n    function exec(cmd, opts, cb) {\n      try {\n        var r = execSync(cmd, opts);\n        if (cb) cb(null, r);\n        else return Promise.resolve(r);\n      } catch(e) { if (cb) cb(e); else return Promise.reject(e); }\n    }\n    return { execSync: execSync, exec: exec };\n  })();\n\n  // ── http (basic get/request) ──────────────────────────────\n  builtins.http = (function() {\n    var bH = typeof __bridge_http_get !== 'undefined';\n    var events = builtins.events;\n    function get(url, opts, cb) {\n      if (typeof opts === 'function') { cb = opts; opts = {}; }\n      if (!bH) { if (cb) cb(new Error('http not available')); return; }\n      try {\n        var body = __bridge_http_get(url);\n        if (body === null) { if (cb) cb(new Error('http get failed: ' + url)); return; }\n        var res = new events.EventEmitter();\n        res.statusCode = 200;\n        res.headers = { 'content-type': 'text/plain' };\n        res.setEncoding = function() {};\n        res.on = function(ev, fn) {\n          if (ev === 'data' && body) { fn(body); res.emit('end'); }\n          events.EventEmitter.prototype.on.call(res, ev, fn);\n          return res;\n        };\n        if (cb) cb(res);\n        return res;\n      } catch(e) { if (cb) cb(e); }\n    }\n    function request(url, opts, cb) { return get(url, opts, cb); }\n    return { get: get, request: request };\n  })();\n\n  builtins.https = builtins.http;\n\n  // ── module (full) ─────────────────────────────────────────\n  builtins.module = (function() {\n    var builtinMods = Object.keys(builtins);\n    var cache = {};\n    function Module(id, parent) {\n      this.id = id || '.';\n      this.exports = {};\n      this.filename = id || __filename || '';\n      this.loaded = false;\n      this.parent = parent || null;\n      this.children = [];\n      this.paths = [];\n    }\n    Module._cache = cache;\n    Module._extensions = {\n      '.js': function(m, fn) {},\n      '.json': function(m, fn) { try { m.exports = JSON.parse(fn); } catch(e) { m.exports = {}; } }\n    };\n    Module.globalPaths = [];\n    Module.builtinModules = builtinMods;\n    Module._resolveFilename = function(request, parent) {\n      if (builtinMods.indexOf(request) !== -1) return request;\n      return request;\n    };\n    return { Module: Module, builtinModules: builtinMods,\n             _cache: cache, _resolveFilename: Module._resolveFilename,\n             _extensions: Module._extensions };\n  })();\n\n  // ── Enhance C-level require with Node.js properties ──────\n  if (typeof require !== 'undefined') {\n    if (!require.cache) {\n      var _origRequire = require;\n      require = function(name) {\n        if (typeof name === 'string' && name.indexOf('rust:') === 0) {\n          var crateName = name.slice(5);\n          if (require.cache['rust:' + crateName]) return require.cache['rust:' + crateName];\n          var mod = _origRequire(name);\n          if (mod) require.cache['rust:' + crateName] = mod;\n          return mod;\n        }\n        return _origRequire(name);\n      };\n      require.cache = {};\n      require.resolve = function(name, opts) {\n        if (typeof name !== 'string') throw new Error('name must be string');\n        if (builtins[name] || __node_builtins[name]) return name;\n        var paths = opts && opts.paths;\n        var checks = paths ? paths.slice() : [];\n        if (!paths) {\n          checks.push('./node_modules/' + name + '/package.json');\n          checks.push('./node_modules/' + name + '.js');\n          checks.push('./node_modules/' + name + '/index.js');\n          checks.push('./node_modules/' + name + '/index.mjs');\n          checks.push('./node_modules/' + name + '/index.cjs');\n        }\n        if (typeof __bridge_fs_exists !== 'undefined') {\n          for (var i = 0; i < checks.length; i++) {\n            if (__bridge_fs_exists(checks[i])) return checks[i];\n          }\n        }\n        return name;\n      };\n      require.resolve.paths = function(name) {\n        if (name[0] === '.' || name[0] === '/') return null;\n        return ['./node_modules'];\n      };\n      require.main = { id: '.', exports: {}, loaded: true, filename: __filename || '', paths: [] };\n      require.extensions = { '.js': function(m, fn) {}, '.json': function(m, fn) { try { m.exports = JSON.parse(fn); } catch(e) { m.exports = {}; } } };\n    }\n  }\n\n  // ── process extensions (already partially in C) ───────────\n  // If process doesn't have cwd/env/argv, add them\n  if (typeof process !== 'undefined') {\n    if (!process.cwd) process.cwd = function() { return __dirname || '/'; };\n    if (!process.env) process.env = { PATH: '/usr/bin', HOME: '/home/user', NODE_ENV: 'production' };\n    if (!process.argv) process.argv = ['qjs', '.', ''];\n    if (!process.exit) process.exit = function(code) { throw new Error('exit:'+code); };\n    if (!process.nextTick) process.nextTick = function(fn) { fn(); };\n    if (!process.browser) process.browser = false;\n    if (!process.version) process.version = 'v16.0.0';\n    if (!process.versions) process.versions = { node: '16.0.0', v8: '9.0', uv: '1.0', zlib: '1.0' };\n    if (!process.stderr) process.stderr = { write: function(s) { } };\n    if (!process.stdin) process.stdin = { isTTY: false, setEncoding: function(){}, on: function(){} };\n    if (!process.pid) process.pid = 1;\n    if (!process.uptime) process.uptime = function() { return 0; };\n    if (!process.memoryUsage) process.memoryUsage = function() { return { rss: 0, heapTotal: 0, heapUsed: 0, external: 0 }; };\n    if (!process.cpuUsage) process.cpuUsage = function() { return { user: 0, system: 0 }; };\n    if (!process.umask) process.umask = function() { return 0o022; };\n    if (!process.kill) process.kill = function(pid, sig) {};\n    if (!process.hrtime) process.hrtime = function(t) { var n = Date.now(); return t ? [0, (n - (t[0]*1e3 + t[1]/1e6))*1e6] : [0, n*1e6]; };\n    if (!process.getuid) process.getuid = function() { return 0; };\n    if (!process.getgid) process.getgid = function() { return 0; };\n  }\n\n  // ── Register all builtins into global registry ────────────\n  globalThis.__node_builtins = builtins;\n})();\n";

static void json_escape(const char *in, char *out, size_t outsz) {
  size_t j=0; for(size_t i=0;in[i]&&j+6<outsz;i++){
    unsigned char c=(unsigned char)in[i];
    if(c=='\\'||c=='"'){out[j++]='\\';out[j++]=c;}
    else if(c=='\n'){out[j++]='\\';out[j++]='n';}
    else if(c=='\r'){out[j++]='\\';out[j++]='r';}
    else if(c=='\t'){out[j++]='\\';out[j++]='t';}
    else if(c<32){j+=snprintf(out+j,outsz-j,"\\u%04x",c);}
    else out[j++]=c;}
  out[j]='\0';}

static void* store_json(const char* json){
  if(!json)return NULL;char**pp=(char**)malloc(sizeof(char*));if(!pp)return NULL;
  *pp=(char*)malloc(strlen(json)+1);if(!*pp){free(pp);return NULL;}strcpy(*pp,json);
  return (void*)((intptr_t)pp | 1);}
#define UNTAG(h) ((void*)((intptr_t)(h) & ~1))
static const char* get_json(void* handle){
  if(!handle)return"null";return*((const char**)UNTAG(handle));}

static char* jsval_to_json(JSContext*ctx,JSValue val){
  JSValue j=JS_JSONStringify(ctx,val,JS_NULL,JS_NULL);
  if(JS_IsException(j))return strdup("null");
  const char*s=JS_ToCString(ctx,j);char*r=s?strdup(s):strdup("null");
  if(s)JS_FreeCString(ctx,s);JS_FreeValue(ctx,j);return r;}

static void strip_dots(char*out,const char*md,const char*path){
  if(path[0]=='.'&&path[1]=='/')snprintf(out,4096,"%s/%s",md,path+2);
  else snprintf(out,4096,"%s/%s",md,path);}

/* Resolve exports key against requested subpath, handling wildcard patterns */
static void resolve_exports_key(const char*key,const char*subpath,JSValue val,
  const char*md,int*match,char*matched_path,int path_sz,JSContext*ctx){
  *match=0;matched_path[0]=0;
  /* Check if key matches subpath */
  int key_match=0;char wildcard[4096]="";wildcard[0]=0;
  if(strcmp(key,subpath)==0){key_match=1;}
  else{
    /* Wildcard pattern: key contains '*' */
    const char*star=strchr(key,'*');
    if(star){
      size_t prefix_len=(size_t)(star-key);
      const char*suffix=star+1;
      size_t suffix_len=strlen(suffix);
      size_t sub_len=strlen(subpath);
      if(sub_len>=prefix_len+suffix_len&&
         strncmp(key,subpath,prefix_len)==0&&
         strncmp(suffix,subpath+sub_len-suffix_len,suffix_len)==0){
        key_match=1;
        size_t wl=sub_len-prefix_len-suffix_len;
        if(wl<sizeof(wildcard)-1){memcpy(wildcard,subpath+prefix_len,wl);wildcard[wl]=0;}
      }
    }
  }
  if(!key_match)return;
  /* Resolve value: could be string, condition object, or nested pattern */
  if(JS_IsString(val)){
    const char*vs=JS_ToCString(ctx,val);
    if(vs){char buf[4096];snprintf(buf,sizeof(buf),"%s/%s",md,vs[0]=='.'&&vs[1]=='/'?vs+2:vs);
      /* Substitute wildcard */
      if(wildcard[0]){char*wp=strstr(buf,"*");if(wp){*wp=0;snprintf(matched_path,path_sz,"%s%s%s",buf,wildcard,wp+1);}else snprintf(matched_path,path_sz,"%s",buf);}
      else snprintf(matched_path,path_sz,"%s",buf);
      *match=1;JS_FreeCString(ctx,vs);}
  }else if(JS_IsObject(val)){
    /* Try conditions: require > node > default > import */
    const char*conds[]={"require","node","default","import",NULL};
    for(int ci=0;conds[ci]&&!*match;ci++){
      JSValue cv=JS_GetPropertyStr(ctx,val,conds[ci]);
      if(!JS_IsUndefined(cv)){
        /* Nested condition object */
        if(JS_IsObject(cv)&&!JS_IsString(cv)){
          resolve_exports_key(key,subpath,cv,md,match,matched_path,path_sz,ctx);
        }else if(JS_IsString(cv)){
          const char*cs=JS_ToCString(ctx,cv);
          if(cs){strip_dots(matched_path,md,cs);*match=1;JS_FreeCString(ctx,cs);}
        }
      }
      JS_FreeValue(ctx,cv);
    }
  }
}

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

static char* bridge_esm_to_cjs(const char*s,size_t sl,size_t*ol){
  char*d=(char*)malloc(sl*3+8192);if(!d)return NULL;size_t di=0,i=0;
  int q1=0,q2=0,mlc=0;char pending[8192];size_t pi=0;
  #define P(s) do{const char*_p_=(s);size_t _pl_=strlen(_p_);memcpy(d+di,_p_,_pl_);di+=_pl_;}while(0)
  #define PC(s) do{P(s);pi=0;}while(0)
  #define FL(s) do{if(pi+(int)strlen(s)+1<(int)sizeof(pending)){memcpy(pending+pi,s,strlen(s));pi+=strlen(s);}}while(0)
  pending[0]=0;
  while(i<sl){
    if(mlc){d[di++]=s[i];if(s[i]=='*'&&i+1<sl&&s[i+1]=='/'){d[di++]=s[++i];mlc=0;}i++;continue;}
    if(q1){d[di++]=s[i];if(s[i]=='\\'&&i+1<sl)d[di++]=s[++i];else if(s[i]=='\'')q1=0;i++;continue;}
    if(q2){d[di++]=s[i];if(s[i]=='\\'&&i+1<sl)d[di++]=s[++i];else if(s[i]=='"')q2=0;i++;continue;}
    if(s[i]=='/'&&i+1<sl){if(s[i+1]=='/'){d[di++]=s[i++];d[di++]=s[i++];
      while(i<sl&&s[i]!='\n')d[di++]=s[i++];continue;}
      if(s[i+1]=='*'){d[di++]=s[i++];d[di++]=s[i++];mlc=1;continue;}}
    if(s[i]=='\''){q1=1;d[di++]=s[i++];continue;}
    if(s[i]=='"'){q2=1;d[di++]=s[i++];continue;}
    if(s[i]=='`'){d[di++]=s[i++];while(i<sl&&s[i]!='`'){d[di++]=s[i++];}if(i<sl)d[di++]=s[i++];continue;}
    /* Detect 'import ' at line start (after whitespace) */
    {int ws=i;while(ws<(int)sl&&(s[ws]==' '||s[ws]=='\t'))ws++;
    if(ws+6<(int)sl&&strncmp(s+ws,"import",6)==0&&s[ws+6]<=' '){
      int si=ws+7;while(si<(int)sl&&s[si]<=' ')si++;
      if(si<(int)sl&&s[si]=='"'){/* import "x" */
        int ei=si+1;while(ei<(int)sl&&s[ei]!='"')ei++;
        P("require(");for(int j=si;j<=ei;j++)d[di++]=s[j];P(");");
        while(i<(int)sl&&s[i]!=';'&&s[i]!='\n')i++;if(s[i]==';')i++;
        continue;
      }
      /* import X from 'y' or import {X} from 'y' or import * as X from 'y' */
      int br=0;char bind[4096];int bi=0;
      while(si<(int)sl&&s[si]!=';'&&s[si]!='\n'&&s[si]!='\r'){
        if(s[si]=='{'||s[si]=='*'){br=1;}
        if(bi<(int)sizeof(bind)-1)bind[bi++]=s[si];si++;
      }
      bind[bi]=0;
      /* Find 'from' keyword */
      char*fr=strstr(bind,"from");if(fr){*fr=0;fr+=4;}
      /* Trim bind and from */
      int bj=(int)strlen(bind);while(bj>0&&bind[bj-1]<=' ')bind[--bj]=0;
      const char*mod=fr?fr:bind;
      while(*mod<=' ')mod++;
      /* Strip quotes from module */
      char modq[4096];int mqi=0;
      if(*mod=='"'||*mod=='\''){mod++;while(mod[mqi]&&mod[mqi]!='"'&&mod[mqi]!='\''){modq[mqi]=mod[mqi];mqi++;}modq[mqi]=0;}else snprintf(modq,sizeof(modq),"%s",mod);
      /* Check if it's a namespace import: * as X */
      char*star=strstr(bind,"*");
      if(star){char*as=strstr(star+1,"as");if(as){as+=2;while(*as<=' ')as++;
        P("const ");while(*as>' ')d[di++]=*as++;P("=require(\"");P(modq);P("\")");}
      }else if(!br){/* default import: X from 'y' */
        P("const ");P(bind);P("=require(\"");P(modq);P("\").default||require(\"");P(modq);P("\")");
      }else{/* named import: {X,Y} from 'y' */
        P("const ");P(bind);P("=require(\"");P(modq);P("\")");}
      while(i<(int)sl&&s[i]!=';'&&s[i]!='\n'&&s[i]!='\r')i++;if(s[i]==';')i++;
      continue;
    }}
    /* Detect 'export ' at line start (after whitespace) */
    {int ws=i;while(ws<(int)sl&&(s[ws]==' '||s[ws]=='\t'))ws++;
    if(ws+7<(int)sl&&strncmp(s+ws,"export ",7)==0){
      int si=ws+7;while(si<(int)sl&&s[si]<=' ')si++;
      if(strncmp(s+si,"default",7)==0){/* export default */
        si+=7;while(si<(int)sl&&s[si]<=' ')si++;
        int ei=si,dp=1;while(ei<(int)sl){if(s[ei]=='{')dp++;if(s[ei]=='}')dp--;if(s[ei]==';'&&dp==1)break;ei++;}
        P("module.exports.default=(");for(int j=si;j<ei;j++)d[di++]=s[j];P(")");if(s[ei]==';'){d[di++]=s[ei];}else P(";");
        while(i<(int)sl&&s[i]!=';'&&s[i]!='\n')i++;if(s[i]==';')i++;continue;
      }
      if(s[si]=='{'){/* export {X,Y} */
        int ei=si+1;while(ei<(int)sl&&s[ei]!='}')ei++;
        char exps[4096];int eii=0;for(int j=si+1;j<ei;j++)if(s[j]>' ')exps[eii++]=s[j];exps[eii]=0;
        char*tk=strtok(exps,",");while(tk){
          P("module.exports.");P(tk);P("=");P(tk);P(";");tk=strtok(NULL,",");}
        /* Check for 'from' */
        char*ff=strstr(s+ei+1,"from");
        if(ff){while(*ff<=' ')ff++;ff+=4;while(*ff<=' ')ff++;
          char modq[256];int mi=0;
          if(*ff=='"'||*ff=='\''){ff++;while(*ff&&*ff!='"'&&*ff!='\'')modq[mi++]=*ff++;}modq[mi]=0;
          P("Object.assign(module.exports,require(\"");P(modq);P("\"));");}
        while(i<(int)sl&&s[i]!=';'&&s[i]!='\n')i++;if(s[i]==';')i++;continue;
      }
      if(s[si]=='*'){/* export * from 'y' */
        char*ff=strstr(s+si,"from");
        if(ff){ff+=4;while(*ff<=' ')ff++;
          char modq[256];int mi=0;
          if(*ff=='"'||*ff=='\''){ff++;while(*ff&&*ff!='"'&&*ff!='\'')modq[mi++]=*ff++;}modq[mi]=0;
          P("Object.assign(module.exports,require(\"");P(modq);P("\"));");}
        while(i<(int)sl&&s[i]!=';'&&s[i]!='\n')i++;if(s[i]==';')i++;continue;
      }
      /* export function/const/class/let/var Name ... */
      int sni=si;char name[256];int ni=0;
      while(sni<(int)sl&&s[sni]!=' '&&s[sni]!='\t'&&s[sni]!='(')sni++;
      while(sni<(int)sl&&(s[sni]==' '||s[sni]=='\t'))sni++;
      while(sni<(int)sl&&(s[sni]>' '&&s[sni]!='('&&s[sni]!='='&&s[sni]!='{')){if(ni<255)name[ni++]=s[sni];sni++;}name[ni]=0;
      P("module.exports.");P(name);P("=(");
      for(int j=si;j<(int)sl&&s[j]!=';'&&s[j]!='\n';j++)d[di++]=s[j];
      P(");");while(i<(int)sl&&s[i]!=';'&&s[i]!='\n')i++;if(s[i]==';'){d[di++]=s[i];i++;}continue;
    }}
    d[di++]=s[i++];
  }
  d[di]=0;*ol=di;return d;}

static JSValue load_module(JSContext*ctx,const char*path,const char*name){
  FILE*f=fopen(path,"rb");if(!f){set_last_error("load_module: fopen failed");return JS_ThrowReferenceError(ctx,"mod %s",name);}
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
  /* Prefer pre-transpiled .cjs file (generated by esbuild at bridge-generation time) */
  int need_transpile=0;
  if(strlen(path)>4&&strcmp(path+strlen(path)-4,".mjs")==0){
    need_transpile=1;
    char cjs_path[4096];
    strncpy(cjs_path,path,sizeof(cjs_path)-1);cjs_path[sizeof(cjs_path)-1]=0;
    strcpy(cjs_path+strlen(cjs_path)-4,".cjs");
    FILE*cjs_f=fopen(cjs_path,"rb");
    if(cjs_f){
      fclose(cjs_f);free(code);
      cjs_f=fopen(cjs_path,"rb");if(!cjs_f){set_last_error("load_module: .cjs fopen failed");JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowReferenceError(ctx,"mod %s",name);}
      fseek(cjs_f,0,SEEK_END);len=ftell(cjs_f);fseek(cjs_f,0,SEEK_SET);
      code=(char*)malloc((size_t)len+1);
      if(!code){fclose(cjs_f);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowOutOfMemory(ctx);}
      fread(code,1,len,cjs_f);code[len]=0;fclose(cjs_f);need_transpile=0;
    }
  }else if(strlen(path)>3&&strcmp(path+strlen(path)-3,".js")==0){
    /* Check nearest package.json for "type":"module" */
    char pj_check[4096];strncpy(pj_check,path,sizeof(pj_check)-1);
    while(1){
      char*ls=strrchr(pj_check,'/');
      char*bs=strrchr(pj_check,'\\');if(bs>ls)ls=bs;
      if(!ls)break;*ls=0;
      char pjp[4096];snprintf(pjp,sizeof(pjp),"%s/package.json",pj_check);
      FILE*pjf=fopen(pjp,"rb");
      if(pjf){
        fseek(pjf,0,SEEK_END);long pjl=ftell(pjf);fseek(pjf,0,SEEK_SET);
        char*pj=(char*)malloc((size_t)pjl+1);
        if(pj){fread(pj,1,pjl,pjf);pj[pjl]=0;
          char*tp=strstr(pj,"\"type\"");
          if(tp){tp+=6;while(*tp&&*tp<=' ')tp++;if(*tp==':'){tp++;while(*tp&&*tp<=' ')tp++;
            if(*tp=='"'){tp++;if(strncmp(tp,"module",6)==0)need_transpile=1;}}
          }
          free(pj);
        }
        fclose(pjf);break;
      }
    }
  }
  if(need_transpile){
    size_t slen=(size_t)len;
    char*tjs=bridge_esm_to_cjs(code,slen,&slen);
    if(tjs){free(code);code=tjs;len=(long)slen;}else{free(code);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowOutOfMemory(ctx);}
  }
  /* Set current dir for module resolution */
  char prev_dir[4096];strncpy(prev_dir,g_current_dir,sizeof(prev_dir)-1);
  strncpy(g_current_dir,dir,sizeof(g_current_dir)-1);
  JSValue g=JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx,g,"exports",JS_DupValue(ctx,ex));
  JS_SetPropertyStr(ctx,g,"module",JS_DupValue(ctx,mo));
  JS_SetPropertyStr(ctx,g,"__filename",JS_NewString(ctx,path));
  JS_SetPropertyStr(ctx,g,"__dirname",JS_NewString(ctx,dir));
  JS_FreeValue(ctx,g);
  /* Bytecode cache: check for .qbc file */
  char qbc_path[4096];snprintf(qbc_path,sizeof(qbc_path),"%s.qbc",path);
  FILE*qbc_f=fopen(qbc_path,"rb");JSValue r;
  if(qbc_f){
    fseek(qbc_f,0,SEEK_END);long qbc_len=ftell(qbc_f);fseek(qbc_f,0,SEEK_SET);
    uint8_t*qbc_buf=(uint8_t*)malloc((size_t)qbc_len);
    if(qbc_buf){fread(qbc_buf,1,qbc_len,qbc_f);r=JS_ReadObject(ctx,qbc_buf,qbc_len,JS_READ_OBJ_BYTECODE);free(qbc_buf);}
    else{r=JS_ThrowOutOfMemory(ctx);}
    fclose(qbc_f);free(code);g_perf.bc_hits++;
  }else{
    g_perf.bc_misses++;
#ifdef _WIN32
    LARGE_INTEGER e0,ef,efreq;QueryPerformanceFrequency(&efreq);QueryPerformanceCounter(&e0);
#else
    struct timespec e0,ef;clock_gettime(CLOCK_MONOTONIC,&e0);
#endif
    r=JS_Eval(ctx,code,(size_t)len,path,JS_EVAL_TYPE_GLOBAL);free(code);
#ifdef _WIN32
    QueryPerformanceCounter(&ef);g_perf.eval_time_ms+=(unsigned)(((ef.QuadPart-e0.QuadPart)*1000)/efreq.QuadPart);
#else
    clock_gettime(CLOCK_MONOTONIC,&ef);g_perf.eval_time_ms+=(unsigned)(((ef.tv_sec-e0.tv_sec)*1000+(ef.tv_nsec-e0.tv_nsec)/1000000));
#endif
    if(!JS_IsException(r)){
      size_t out_len;
      uint8_t*out_buf=JS_WriteObject(ctx,&out_len,r,JS_WRITE_OBJ_BYTECODE);
      if(out_buf){
        FILE*qbc_w=fopen(qbc_path,"wb");
        if(qbc_w){fwrite(out_buf,1,out_len,qbc_w);fclose(qbc_w);}
        js_free(ctx,out_buf);
      }
    }
  }
  /* Restore previous dir */
  strncpy(g_current_dir,prev_dir,sizeof(g_current_dir)-1);
  if(JS_IsException(r)){JS_FreeValue(ctx,r);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_EXCEPTION;}
  JS_FreeValue(ctx,r);
  JSValue fe=JS_GetPropertyStr(ctx,mo,"exports");
  JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return fe;}

static char* resolve_mod(const char*name,const char*from){
  g_perf.exports_lookups++;
  static char buf[4096];char dir[4096];
  strncpy(dir,from?from:".",sizeof(dir)-1);dir[sizeof(dir)-1]=0;
  /* Relative path: try direct .js/.cjs/.mjs, /index.*, and /package.json */
  if(name[0]=='.'){
    snprintf(buf,sizeof(buf),"%s/%s",dir,name);
    /* Try exact match first */
    FILE*f=fopen(buf,"rb");if(f){fclose(f);return buf;}
    /* Try .js, .cjs, .mjs */
    char t[4096];
    const char*exts[]={".js",".cjs",".mjs",NULL};
    for(int ei=0;exts[ei];ei++){snprintf(t,sizeof(t),"%s%s",buf,exts[ei]);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}
    /* Try index.js, index.mjs, index.cjs */
    const char*idx[]={"index.js","index.mjs","index.cjs",NULL};
    for(int ii=0;idx[ii];ii++){snprintf(t,sizeof(t),"%s/%s",buf,idx[ii]);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}
    /* Try package.json */
    snprintf(t,sizeof(t),"%s/package.json",buf);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}
    return NULL;
  }
  /* Two-phase resolution for nested paths (a/b/c) */
  const char*slash_in_name=strchr(name,'/');
  if(slash_in_name){
    /* Phase 1: extract package name */
    size_t pkg_len=(size_t)(slash_in_name-name);
    char pkg_name[256];if(pkg_len<sizeof(pkg_name)){
      memcpy(pkg_name,name,pkg_len);pkg_name[pkg_len]=0;
      const char*subpath=slash_in_name+1;
      char pkg_root[4096];char tmp[4096];strncpy(tmp,dir,sizeof(tmp)-1);
      while(tmp[0]){
        snprintf(pkg_root,sizeof(pkg_root),"%s/node_modules/%s/package.json",tmp,pkg_name);
        FILE*f=fopen(pkg_root,"rb");
        if(f){fclose(f);
          snprintf(pkg_root,sizeof(pkg_root),"%s/node_modules/%s",tmp,pkg_name);
          /* Phase 2: resolve subpath relative to package root */
          snprintf(buf,sizeof(buf),"%s/%s",pkg_root,subpath);
          FILE*tf=fopen(buf,"rb");if(tf){fclose(tf);return buf;}
          /* Try with .js, .cjs, .mjs */
          const char*exts2[]={".js",".cjs",".mjs",NULL};
          for(int ei=0;exts2[ei];ei++){snprintf(tmp,sizeof(tmp),"%s/%s%s",pkg_root,subpath,exts2[ei]);tf=fopen(tmp,"rb");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}}
          /* Try /index.js, /index.mjs, /index.cjs */
          snprintf(tmp,sizeof(tmp),"%s/%s/index.js",pkg_root,subpath);tf=fopen(tmp,"rb");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}
          snprintf(tmp,sizeof(tmp),"%s/%s/index.mjs",pkg_root,subpath);tf=fopen(tmp,"rb");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}
          snprintf(tmp,sizeof(tmp),"%s/%s/index.cjs",pkg_root,subpath);tf=fopen(tmp,"rb");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}
          /* Not found via two-phase — fall through to flat search */
          break;
        }
        char*p=strrchr(tmp,'/');
        char*p2=strrchr(tmp,'\\');if(p2>p)p=p2;
        if(!p||p==tmp)break;*p=0;
      }
    }
  }
  /* Flat search (original algorithm) */
  while(1){
    snprintf(buf,sizeof(buf),"%s/node_modules/%s/package.json",dir,name);
    FILE*f=fopen(buf,"rb");if(f){fclose(f);snprintf(buf,sizeof(buf),"%s/node_modules/%s",dir,name);return buf;}
    const char*exts[]={".js",".cjs",".mjs",NULL};
    char t[4096];
    for(int ei=0;exts[ei];ei++){snprintf(t,sizeof(t),"%s/node_modules/%s%s",dir,name,exts[ei]);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}
    const char*idx2[]={"index.js","index.mjs","index.cjs",NULL};
    for(int ii=0;idx2[ii];ii++){snprintf(t,sizeof(t),"%s/node_modules/%s/%s",dir,name,idx2[ii]);f=fopen(t,"rb");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}
    char*p=strrchr(dir,'/');
    char*p2=strrchr(dir,'\\');if(p2>p)p=p2;
    if(!p||p==dir)break;*p=0;}
  return NULL;}

#define MAX_CACHED 256
static const char* g_cache_names[MAX_CACHED];
static JSValue g_cache_exports[MAX_CACHED];
static int g_cache_count=0;

static JSValue js_require(JSContext*ctx,JSValueConst t,int argc,JSValueConst*argv){
  g_perf.require_calls++;
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
        char* dn=strdup(name);if(dn){g_cache_names[g_cache_count]=dn;g_cache_exports[g_cache_count]=JS_DupValue(ctx,rmod);g_cache_count++;}
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
    char*js=(char*)malloc((size_t)jl+1);if(!js){fclose(f);JS_FreeCString(ctx,name);return JS_ThrowReferenceError(ctx,"OOM reading package.json");}fread(js,1,jl,f);js[jl]=0;fclose(f);
    JSValue po=JS_ParseJSON(ctx,js,(size_t)jl,"<pkg>");free(js);
    if(!JS_IsException(po)){
      int resolved=0;
      /* Try 'exports' map before 'main' */
      JSValue ev=JS_GetPropertyStr(ctx,po,"exports");
      if(JS_IsString(ev)){
        const char*es=JS_ToCString(ctx,ev);
        if(es){strip_dots(entry,md,es);resolved=1;JS_FreeCString(ctx,es);}
        JS_FreeValue(ctx,ev);
      }else if(JS_IsObject(ev)){
        /* Determine the subpath requested */
        const char*subpath=".";
        char subbuf[4096];
        const char*slash=strchr(name,'/');
        if(slash&&slash>name){snprintf(subbuf,sizeof(subbuf),"./%s",slash+1);subpath=subbuf;}
        char best_match[4096];best_match[0]=0;
        /* Iterate exports keys using JS_GetOwnPropertyNames */
        {
          JSPropertyEnum*ptab=NULL;uint32_t plen=0;
          if(JS_GetOwnPropertyNames(ctx,&ptab,&plen,ev,JS_GPN_STRING_MASK)==0){
            for(uint32_t ki=0;ki<plen;ki++){
              const char*key=JS_AtomToCString(ctx,ptab[ki].atom);
              if(key){
                JSValue val=JS_GetPropertyStr(ctx,ev,key);
                int match=0;char matched_path[4096]="";
                resolve_exports_key(key,subpath,val,md,&match,matched_path,sizeof(matched_path),ctx);
                if(match&&(!best_match[0]||strlen(key)>strlen(best_match))){
                  strncpy(best_match,key,sizeof(best_match)-1);
                  strncpy(entry,matched_path,sizeof(entry)-1);resolved=1;
                }
                JS_FreeValue(ctx,val);JS_FreeCString(ctx,key);
              }
            }
            JS_FreePropertyEnum(ctx,ptab,plen);
          }
        }
        JS_FreeValue(ctx,ev);
      }else JS_FreeValue(ctx,ev);
      if(!resolved){
        JSValue mv=JS_GetPropertyStr(ctx,po,"main");
        if(!JS_IsUndefined(mv)){
          const char*ms=JS_ToCString(ctx,mv);
          if(ms){strip_dots(entry,md,ms);JS_FreeCString(ctx,ms);}
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
    char* dn=strdup(name);if(!dn){JS_FreeCString(ctx,name);return JS_ThrowOutOfMemory(ctx);}
    JSValue ce=JS_NewObject(ctx);if(JS_IsException(ce)){free(dn);JS_FreeCString(ctx,name);return JS_ThrowOutOfMemory(ctx);}
    cache_slot=g_cache_count++;
    g_cache_names[cache_slot]=dn;
    g_cache_exports[cache_slot]=ce;
  }
  JSValue ret=load_module(ctx,entry,name);
  if(!JS_IsException(ret)&&cache_slot>=0){
    JS_FreeValue(ctx,g_cache_exports[cache_slot]);
    g_cache_exports[cache_slot]=JS_DupValue(ctx,ret);
  }
  JS_FreeCString(ctx,name);return ret;}

static JSValue js_process_cwd(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue js_process_exit(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue js_process_nextTick(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue js_process_env(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_crypto_hash(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_crypto_random(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_net_connect(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_net_write(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_net_read(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_net_close(JSContext*,JSValueConst,int,JSValueConst*);
static JSValue bridge_dns_lookup(JSContext*,JSValueConst,int,JSValueConst*);
static int init_qjs(const char*dir){
  if(g_inited)return 1;g_rt=JS_NewRuntime();if(!g_rt){set_last_error("init: JS_NewRuntime failed");return 0;}
  g_ctx=JS_NewContext(g_rt);if(!g_ctx){JS_FreeRuntime(g_rt);g_rt=NULL;return 0;}
  if(dir){
    char dll_name[256];snprintf(dll_name,sizeof(dll_name),"%s.dll",dir);
    HMODULE hm=GetModuleHandleA(dll_name);
    if(hm){char dll_path[1024];GetModuleFileNameA(hm,dll_path,sizeof(dll_path));
      char*last=strrchr(dll_path,'\\');if(last)*last='\0';
      SetCurrentDirectoryA(dll_path);}
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
  JS_SetPropertyStr(g_ctx,p,"cwd",JS_NewCFunction(g_ctx,js_process_cwd,"cwd",0));
  JS_SetPropertyStr(g_ctx,p,"exit",JS_NewCFunction(g_ctx,js_process_exit,"exit",1));
  JS_SetPropertyStr(g_ctx,p,"nextTick",JS_NewCFunction(g_ctx,js_process_nextTick,"nextTick",1));
  {JSValue ev=js_process_env(g_ctx,JS_UNDEFINED,0,NULL);JS_SetPropertyStr(g_ctx,p,"env",ev);}
  JS_SetPropertyStr(g_ctx,p,"platform",JS_NewString(g_ctx,"win32"));
  JS_SetPropertyStr(g_ctx,p,"arch",JS_NewString(g_ctx,"x64"));
  JS_SetPropertyStr(g_ctx,p,"version",JS_NewString(g_ctx,"v18.17.0"));
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
  JS_SetPropertyStr(g_ctx,g,"__bridge_crypto_hash",JS_NewCFunction(g_ctx,bridge_crypto_hash,"__bridge_crypto_hash",2));
  JS_SetPropertyStr(g_ctx,g,"__bridge_crypto_random",JS_NewCFunction(g_ctx,bridge_crypto_random,"__bridge_crypto_random",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_net_connect",JS_NewCFunction(g_ctx,bridge_net_connect,"__bridge_net_connect",2));
  JS_SetPropertyStr(g_ctx,g,"__bridge_net_write",JS_NewCFunction(g_ctx,bridge_net_write,"__bridge_net_write",2));
  JS_SetPropertyStr(g_ctx,g,"__bridge_net_read",JS_NewCFunction(g_ctx,bridge_net_read,"__bridge_net_read",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_net_close",JS_NewCFunction(g_ctx,bridge_net_close,"__bridge_net_close",1));
  JS_SetPropertyStr(g_ctx,g,"__bridge_dns_lookup",JS_NewCFunction(g_ctx,bridge_dns_lookup,"__bridge_dns_lookup",1));
  JS_FreeValue(g_ctx,g);
  /* Evaluate Node.js builtin polyfills */
  {JSValue _ev=JS_Eval(g_ctx,node_builtins_js,strlen(node_builtins_js),"<builtins>",JS_EVAL_TYPE_GLOBAL);JS_FreeValue(g_ctx,_ev);}
  g_inited=1;return 1;}

static JSValue js_console_log(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  for(int i=0;i<ac;i++){if(i)fputc(' ',stderr);const char*s=JS_ToCString(ctx,av[i]);if(s){fputs(s,stderr);JS_FreeCString(ctx,s);}}
  fputc('\n',stderr);return JS_UNDEFINED;}
static JSValue js_pstdout(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac>0){const char*s=JS_ToCString(ctx,av[0]);if(s){printf("%s",s);fflush(stdout);JS_FreeCString(ctx,s);}}
  return JS_UNDEFINED;}

static JSValue js_process_cwd(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  char buf[4096];GetCurrentDirectoryA(sizeof(buf),buf);return JS_NewString(ctx,buf);}
static JSValue js_process_exit(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  int code=0;if(ac>0)JS_ToInt32(ctx,&code,av[0]);exit(code);return JS_UNDEFINED;}
static JSValue js_process_nextTick(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1||!JS_IsFunction(ctx,av[0]))return JS_ThrowTypeError(ctx,"nextTick:need fn");
  JSValue r=JS_Call(ctx,av[0],JS_UNDEFINED,ac-1,ac>1?av+1:NULL);
  return JS_IsException(r)?r:JS_UNDEFINED;}
static JSValue js_process_env(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  JSValue o=JS_NewObject(ctx);if(JS_IsException(o))return o;
  wchar_t*ew=GetEnvironmentStringsW();if(ew){wchar_t*ep=ew;
    while(*ep){size_t el=wcslen(ep);char*mb=(char*)malloc(el*4+1);if(mb){WideCharToMultiByte(CP_UTF8,0,ep,-1,mb,(int)(el*4),NULL,NULL);
      char*eq=strchr(mb,'=');if(eq){*eq=0;JS_SetPropertyStr(ctx,o,mb,JS_NewString(ctx,eq+1));}free(mb);}ep+=el+1;}
    FreeEnvironmentStringsW(ew);}
  return o;}

static JSValue bridge_fs_readFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_ThrowTypeError(ctx,"readFile:missing path");
  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,"bad path");
  FILE*f=fopen(path,"rb");if(!f){set_last_error("readFile: fopen failed");JS_FreeCString(ctx,path);return JS_NULL;}
  fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
  char*buf=(char*)malloc((size_t)sz+1);if(!buf){fclose(f);JS_FreeCString(ctx,path);return JS_NULL;}
  fread(buf,1,sz,f);buf[sz]=0;fclose(f);
  JSValue r=JS_NewStringLen(ctx,buf,(size_t)sz);free(buf);JS_FreeCString(ctx,path);return r;}

static JSValue bridge_fs_writeFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<2)return JS_ThrowTypeError(ctx,"writeFile:missing args");
  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,"bad path");
  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,path);return JS_ThrowTypeError(ctx,"bad data");}
  FILE*f=fopen(path,"wb");if(!f){set_last_error("writeFile: fopen failed");JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_FALSE;}
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
  JSValue arr=JS_NewArray(ctx);if(JS_IsException(arr)){FindClose(h);JS_FreeCString(ctx,path);return JS_EXCEPTION;}int idx=0;
  do{JSValue nm=JS_NewString(ctx,fd.cFileName);JS_SetPropertyUint32(ctx,arr,idx++,nm);}while(FindNextFileA(h,&fd));
  FindClose(h);JS_FreeCString(ctx,path);return arr;}

static JSValue bridge_fs_stat(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;
  struct stat st;if(stat(path,&st)!=0){JS_FreeCString(ctx,path);return JS_NULL;}
  JSValue o=JS_NewObject(ctx);if(JS_IsException(o)){JS_FreeCString(ctx,path);return JS_EXCEPTION;}
  JS_SetPropertyStr(ctx,o,"size",JS_NewInt64(ctx,st.st_size));
  JS_SetPropertyStr(ctx,o,"mode",JS_NewInt64(ctx,st.st_mode));
  JS_SetPropertyStr(ctx,o,"isFile",JS_NewBool(ctx,S_ISREG(st.st_mode)));
  JS_SetPropertyStr(ctx,o,"isDirectory",JS_NewBool(ctx,S_ISDIR(st.st_mode)));
  JS_FreeCString(ctx,path);return o;}

static JSValue bridge_fs_unlink(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;
  int r=remove(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}

#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"crypt32.lib")
static JSValue bridge_crypto_hash(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<2)return JS_ThrowTypeError(ctx,"crypto_hash:need algo+data");
  const char*algo=JS_ToCString(ctx,av[0]);if(!algo)return JS_ThrowTypeError(ctx,"bad algo");
  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,algo);return JS_ThrowTypeError(ctx,"bad data");}
  LPCWSTR walgo=NULL;ULONG hash_len=0;
  if(strcmp(algo,"sha256")==0){walgo=BCRYPT_SHA256_ALGORITHM;hash_len=32;}
  else if(strcmp(algo,"sha1")==0){walgo=BCRYPT_SHA1_ALGORITHM;hash_len=20;}
  else if(strcmp(algo,"md5")==0){walgo=BCRYPT_MD5_ALGORITHM;hash_len=16;}
  else{JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,"unsupported algo");}
  BCRYPT_ALG_HANDLE hAlg=NULL;
  if(BCryptOpenAlgorithmProvider(&hAlg,walgo,NULL,0)!=0||!hAlg){
    JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,"CNG init failed");}
  BCRYPT_HASH_HANDLE hHash=NULL;
  if(BCryptCreateHash(hAlg,&hHash,NULL,0,NULL,0,0)!=0||!hHash){
    BCryptCloseAlgorithmProvider(hAlg,0);JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,"CNG hash failed");}
  BCryptHashData(hHash,(PUCHAR)data,(ULONG)strlen(data),0);
  UCHAR hash[64];
  BCryptFinishHash(hHash,hash,hash_len,0);
  BCryptDestroyHash(hHash);BCryptCloseAlgorithmProvider(hAlg,0);
  char hex[129];for(ULONG i=0;i<hash_len;i++)snprintf(hex+i*2,3,"%02x",hash[i]);
  JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);
  return JS_NewString(ctx,hex);}

#include <wincrypt.h>
static JSValue bridge_crypto_random(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_ThrowTypeError(ctx,"random:need size");
  int32_t sz=0;JS_ToInt32(ctx,&sz,av[0]);if(sz<1||sz>65536)sz=32;
  HCRYPTPROV prov=0;
  if(!CryptAcquireContextA(&prov,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)){
    return JS_ThrowTypeError(ctx,"CSP failed");}
  BYTE*buf=(BYTE*)malloc((size_t)sz);if(!buf){CryptReleaseContext(prov,0);return JS_ThrowOutOfMemory(ctx);}
  CryptGenRandom(prov,sz,buf);CryptReleaseContext(prov,0);
  /* Return as hex string */
  char*hex=(char*)malloc((size_t)sz*2+1);if(!hex){free(buf);return JS_ThrowOutOfMemory(ctx);}
  for(int32_t i=0;i<sz;i++)snprintf(hex+i*2,3,"%02x",buf[i]);
  free(buf);JSValue r=JS_NewString(ctx,hex);free(hex);return r;}

static JSValue bridge_exec(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*cmd=JS_ToCString(ctx,av[0]);if(!cmd)return JS_NULL;
  FILE*pipe=_popen(cmd,"r");
  if(!pipe){set_last_error("exec: popen failed");JS_FreeCString(ctx,cmd);return JS_NULL;}
  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out[0]=0;
  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}
  _pclose(pipe);
  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,cmd);return r;}

static JSValue bridge_http_get(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_NULL;const char*url=JS_ToCString(ctx,av[0]);if(!url)return JS_NULL;
  char tmpfile[256]="";const char* tmpdir=getenv("TMPDIR");if(!tmpdir)tmpdir="/tmp";
  snprintf(tmpfile,sizeof(tmpfile),"%s/qjs_http_XXXXXX",tmpdir);
  char cmd[65536];snprintf(cmd,sizeof(cmd),"powershell -Command \"try{$r=Invoke-WebRequest -Uri '%s' -UseBasicParsing;if($r.Content){$r.Content}else{'{}'}}catch{'{}'}\" 2>nul",url);
  FILE*pipe=_popen(cmd,"r");
  if(!pipe){set_last_error("http_get: popen failed");JS_FreeCString(ctx,url);return JS_NULL;}
  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out[0]=0;
  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}
  _pclose(pipe);
  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,url);return r;}

/* ── net helpers (WinSock2 via dynamic loading of ws2_32.dll) ── */
#include <winsock2.h>
#include <ws2tcpip.h>
static int (__stdcall *pWSAStartup)(WORD,LPWSADATA);
static int (__stdcall *psocket)(int,int,int);
static int (__stdcall *pconnect)(int,const struct sockaddr*,int);
static int (__stdcall *psend)(int,const char*,int,int);
static int (__stdcall *precv)(int,char*,int,int);
static int (__stdcall *pclosesocket)(int);
static int (__stdcall *pgetaddrinfo)(const char*,const char*,const struct addrinfo*,struct addrinfo**);
static void(__stdcall *pfreeaddrinfo)(struct addrinfo*);
static const char*(__stdcall *pinet_ntop)(int,const void*,char*,socklen_t);
static int net_init(void){
  static int n=0;if(n)return n;
  HMODULE h=LoadLibraryA("ws2_32.dll");if(!h){n=-1;return n;}
  pWSAStartup=(void*)GetProcAddress(h,"WSAStartup");
  psocket=(void*)GetProcAddress(h,"socket");
  pconnect=(void*)GetProcAddress(h,"connect");
  psend=(void*)GetProcAddress(h,"send");
  precv=(void*)GetProcAddress(h,"recv");
  pclosesocket=(void*)GetProcAddress(h,"closesocket");
  pgetaddrinfo=(void*)GetProcAddress(h,"getaddrinfo");
  pfreeaddrinfo=(void*)GetProcAddress(h,"freeaddrinfo");
  pinet_ntop=(void*)GetProcAddress(h,"inet_ntop");
  if(!pWSAStartup||!psocket||!pconnect||!psend||!precv||!pclosesocket||!pgetaddrinfo||!pfreeaddrinfo||!pinet_ntop){n=-1;return n;}
  WSADATA wd;n=(pWSAStartup(MAKEWORD(2,2),&wd)==0)?1:-1;return n;}
static JSValue bridge_net_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<2)return JS_ThrowTypeError(ctx,"net_connect:need host port");
  if(net_init()<0)return JS_ThrowTypeError(ctx,"WSAStartup failed");
  const char*host=JS_ToCString(ctx,av[0]);if(!host)return JS_ThrowTypeError(ctx,"bad host");
  int32_t port=0;JS_ToInt32(ctx,&port,av[1]);if(port<1||port>65535){JS_FreeCString(ctx,host);return JS_ThrowRangeError(ctx,"bad port");}
  struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
  char port_str[16];snprintf(port_str,sizeof(port_str),"%d",(int)port);
  int gai=pgetaddrinfo(host,port_str,&hints,&res);JS_FreeCString(ctx,host);
  if(gai!=0||!res){if(res)pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,"getaddrinfo failed");}
  int fd=(int)psocket(res->ai_family,res->ai_socktype,res->ai_protocol);
  if(fd<0){pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,"socket failed");}
  if(pconnect(fd,res->ai_addr,(int)res->ai_addrlen)<0){
    pclosesocket(fd);pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,"connect failed");}
  pfreeaddrinfo(res);return JS_NewInt32(ctx,fd);}

static JSValue bridge_net_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<2)return JS_ThrowTypeError(ctx,"net_write:need fd data");
  int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);
  const char*data=JS_ToCString(ctx,av[1]);if(!data)return JS_ThrowTypeError(ctx,"bad data");
  size_t len=strlen(data);
  int n=(int)psend(fd,data,(int)len,0);JS_FreeCString(ctx,data);
  if(n<0)return JS_ThrowTypeError(ctx,"send failed");
  return JS_NewInt32(ctx,n);}

static JSValue bridge_net_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_ThrowTypeError(ctx,"net_read:need fd");
  int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);
  char buf[4096];
  int n=(int)precv(fd,buf,(int)sizeof(buf)-1,0);
  if(n<=0){if(n==0)return JS_NULL;return JS_ThrowTypeError(ctx,"recv failed");}
  buf[n]=0;return JS_NewString(ctx,buf);}

static JSValue bridge_net_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_FALSE;int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);
  pclosesocket(fd);
  return JS_TRUE;}

static JSValue bridge_dns_lookup(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){
  if(ac<1)return JS_ThrowTypeError(ctx,"dns_lookup:need hostname");
  if(net_init()<0)return JS_ThrowTypeError(ctx,"WSAStartup failed");
  const char*host=JS_ToCString(ctx,av[0]);if(!host)return JS_ThrowTypeError(ctx,"bad host");
  struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
  int gai=pgetaddrinfo(host,"80",&hints,&res);JS_FreeCString(ctx,host);
  if(gai!=0||!res){if(res)pfreeaddrinfo(res);return JS_NULL;}
  char addr[64]="";
  struct sockaddr_in*sin=(struct sockaddr_in*)res->ai_addr;
  pinet_ntop(AF_INET,&sin->sin_addr,addr,sizeof(addr));
  pfreeaddrinfo(res);return addr[0]?JS_NewString(ctx,addr):JS_NULL;}

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
  fprintf(f,"[package]\nname = \"%s_bridge\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n",name);
  fprintf(f,"[lib]\ncrate-type = [\"cdylib\"]\n\n");
  fprintf(f,"[dependencies]\nserde_json = \"1\"\n%s = \"*\"\n",name);
  fprintf(f,"futures = \"0.3\"\n");fclose(f);
  /* Write src/lib.rs */
  snprintf(tp,sizeof(tp),"%s/src",dir);
  _mkdir(tp);
  snprintf(tp,sizeof(tp),"%s/src/lib.rs",dir);
  f=fopen(tp,"w");if(!f){rmdir(dir);return JS_NULL;}
  fprintf(f,"use std::collections::HashMap;\n");
  fprintf(f,"type RustFn = fn(Vec<serde_json::Value>)->Result<serde_json::Value,String>;\n");
  fprintf(f,"#[no_mangle]\npub extern \"C\" fn rust_bridge_get_fns()->*mut HashMap<String,RustFn>{\n");
  fprintf(f,"  let mut m:HashMap<String,RustFn> = HashMap::new();\n");
  fprintf(f,"  Box::into_raw(Box::new(m))\n}");
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
  JSValue obj=JS_NewObject(ctx);if(JS_IsException(obj)){FreeLibrary(lib);rmdir(dir);return JS_EXCEPTION;}
  JS_SetPropertyStr(ctx,obj,"__rust_crate",JS_NewString(ctx,libpath));
  return obj;}

static void cleanup_qjs(void){
  /* Free cached module names and exports (JS refs) before destroying context */
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
      if(argc>0){argv=(JSValue*)malloc((size_t)argc*sizeof(JSValue));if(!argv){argc=0;}else{for(int i=0;i<argc;i++)argv[i]=JS_GetPropertyUint32(g_ctx,av,i);}}
      JS_FreeValue(g_ctx,av);
    }else if(!JS_IsException(av)){argc=1;argv=(JSValue*)malloc(sizeof(JSValue));if(argv)argv[0]=av;else{argc=0;JS_FreeValue(g_ctx,av);}}
  }
  JSValue r=JS_Call(g_ctx,func,obj,argc,argv);JS_FreeValue(g_ctx,func);
  if(argv){for(int i=0;i<argc;i++)JS_FreeValue(g_ctx,argv[i]);free(argv);}
  if(JS_IsException(r))return NULL;
  if(JS_IsObject(r)||JS_IsFunction(g_ctx,r)){
    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,r);return NULL;}
    g_objs[nid]=JS_DupValue(g_ctx,r);JS_FreeValue(g_ctx,r);return(void*)(intptr_t)nid;}
  char*j=jsval_to_json(g_ctx,r);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,r);return h;}

EXPORT void* got_require(void){
  if(!g_inited&&!init_qjs("got_npm"))return NULL;
  if(g_ctx&&!g_loaded){
    for(int i=0;i<MAX_OBJS;i++)g_objs[i]=JS_NULL;
    JSValue g=JS_GetGlobalObject(g_ctx);
    JSValue rv=JS_GetPropertyStr(g_ctx,g,"require");
    if(JS_IsFunction(g_ctx,rv)){
      JSValue nm=JS_NewString(g_ctx,"got");
      JSValue md=JS_Call(g_ctx,rv,JS_UNDEFINED,1,&nm);JS_FreeValue(g_ctx,nm);
      if(!JS_IsException(md)){g_objs[1]=JS_DupValue(g_ctx,md);JS_FreeValue(g_ctx,md);g_loaded=1;}
    }
    JS_FreeValue(g_ctx,rv);JS_FreeValue(g_ctx,g);
  }
  return g_inited&&g_loaded?(void*)(intptr_t)1:NULL;}

EXPORT void got_free(void*h){
  if(!h||(intptr_t)h<2)return;
  if(((intptr_t)h & 1) != 0){
    char**pp=(char**)UNTAG(h);free(*pp);free(pp);return;}
  int id=(int)(intptr_t)h;
  if(id<MAX_OBJS){if(!JS_IsNull(g_objs[id])&&!JS_IsUndefined(g_objs[id]))JS_FreeValue(g_ctx,g_objs[id]);g_objs[id]=JS_NULL;}}

EXPORT void got_free_cstr(void*s){if(s)free(s);}
EXPORT char* got_get_last_error(void){return g_last_error;}
EXPORT const char* got_get_perf_stats(void){
  static char buf[1024];
  snprintf(buf,sizeof(buf),"{\"require_calls\":%u,\"bc_hits\":%u,\"bc_misses\":%u,"
    "\"exports_lookups\":%u,\"eval_time_ms\":%u,\"resolve_time_ms\":%u}",
    g_perf.require_calls,g_perf.bc_hits,g_perf.bc_misses,
    g_perf.exports_lookups,g_perf.eval_time_ms,g_perf.resolve_time_ms);
  return buf;
}

EXPORT void* got_str(const char*s){
  if(!s)return store_json("null");char esc[8192];json_escape(s,esc,sizeof(esc));
  char buf[8194];snprintf(buf,sizeof(buf),"\"%s\"",esc);return store_json(buf);}

EXPORT void* got_int(long long v){
  char buf[64];snprintf(buf,sizeof(buf),"%lld",v);return store_json(buf);}

EXPORT void* got_float(double v){
  char buf[64];snprintf(buf,sizeof(buf),"%g",v);return store_json(buf);}

EXPORT char* got_to_cstr(void*obj){
  if(!obj)return NULL;const char*j=get_json(obj);
  char*out=(char*)malloc(strlen(j)+1);if(!out)return NULL;strcpy(out,j);return out;}

EXPORT void* got_tuple2(void*a,void*b){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s]",get_json(a),get_json(b));return store_json(buf);}
EXPORT void* got_tuple3(void*a,void*b,void*c){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s]",get_json(a),get_json(b),get_json(c));return store_json(buf);}
EXPORT void* got_tuple4(void*a,void*b,void*c,void*d){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d));return store_json(buf);}
EXPORT void* got_tuple(void**items,int count){
  if(count<=0)return store_json("[]");char*b=(char*)malloc(16384);if(!b)return NULL;
  int p=0;p+=snprintf(b+p,16384-p,"[");for(int i=0;i<count;i++){if(i>0)p+=snprintf(b+p,16384-p,",");p+=snprintf(b+p,16384-p,"%s",get_json(items[i]));}
  p+=snprintf(b+p,16384-p,"]");void*h=store_json(b);free(b);return h;}
EXPORT void* got_list2(void*a,void*b){return got_tuple2(a,b);}
EXPORT void* got_list3(void*a,void*b,void*c){return got_tuple3(a,b,c);}
EXPORT void* got_list4(void*a,void*b,void*c,void*d){return got_tuple4(a,b,c,d);}
EXPORT void* got_list(void**items,int count){return got_tuple(items,count);}
EXPORT void* got_dict(void){return store_json("{}");}
EXPORT int got_dict_set(void*d,const char*key,void*val){
  if(!d||!key)return -1;char**pp=(char**)UNTAG(d);const char*vj=get_json(val);
  char ek[512];json_escape(key,ek,sizeof(ek));char entry[2048];
  int el=(int)snprintf(entry,sizeof(entry),"\"%s\":%s",ek,vj);if(el<0)return -1;
  size_t ol=strlen(*pp);
  if(ol<=2){char*ns=(char*)malloc((size_t)el+3);if(!ns)return -1;snprintf(ns,(size_t)el+3,"{%s}",entry);free(*pp);*pp=ns;}
  else{size_t nl=ol+(size_t)el+2;char*ns=(char*)malloc(nl);if(!ns)return -1;snprintf(ns,nl,"%.*s,%s}",(int)(ol-1),*pp,entry);free(*pp);*pp=ns;}
  return 0;}

EXPORT void* got_call(void*mod,const char*fn,void*args){
  if(!args)return qjs_call(mod,fn,"[]");return qjs_call(mod,fn,get_json(args));}

EXPORT void* got_call1(void*mod,const char*fn,void*a){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s]",get_json(a));return qjs_call(mod,fn,buf);}
EXPORT void* got_call2(void*mod,const char*fn,void*a,void*b){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s]",get_json(a),get_json(b));return qjs_call(mod,fn,buf);}
EXPORT void* got_call3(void*mod,const char*fn,void*a,void*b,void*c){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s]",get_json(a),get_json(b),get_json(c));return qjs_call(mod,fn,buf);}
EXPORT void* got_call4(void*mod,const char*fn,void*a,void*b,void*c,void*d){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d));return qjs_call(mod,fn,buf);}
EXPORT void* got_call5(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e));return qjs_call(mod,fn,buf);}
EXPORT void* got_call6(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e,void*f){
  char buf[16384];snprintf(buf,sizeof(buf),"[%s,%s,%s,%s,%s,%s]",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e),get_json(f));return qjs_call(mod,fn,buf);}
EXPORT void* got_call_kw(void*mod,const char*fn,void*args,void*kwargs){
  const char*as=get_json(args);const char*ks=get_json(kwargs);size_t al=strlen(as);char buf[65536];
  if(al>=2&&al<=32768)snprintf(buf,sizeof(buf),"%.*s,%s]",(int)(al-1),as,ks);
  else snprintf(buf,sizeof(buf),"[%s]",ks);return qjs_call(mod,fn,buf);}

EXPORT void* got_getattr(void*obj,const char*name){
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
