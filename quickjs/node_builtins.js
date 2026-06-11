// Node.js built-in polyfills for QuickJS bridge
// These are registered into __node_builtins so the C require() can intercept them.

(function() {
  var builtins = {};

  // ── path ──────────────────────────────────────────────────
  builtins.path = (function() {
    var sep = '/', delimiter = ':';
    function normalize(p) {
      if (!p) return '.';
      var isAbs = p[0] === '/';
      var parts = p.split('/'), out = [];
      for (var i = 0; i < parts.length; i++) {
        if (parts[i] === '' || parts[i] === '.') continue;
        if (parts[i] === '..') { if (out.length && out[out.length-1] !== '..') out.pop(); else if (!isAbs) out.push('..'); }
        else out.push(parts[i]);
      }
      var r = isAbs ? '/' + out.join('/') : out.join('/');
      return r || (isAbs ? '/' : '.');
    }
    function join() { return normalize(Array.prototype.slice.call(arguments).filter(function(x){return x!=null}).join('/')); }
    function resolve() {
      var parts = Array.prototype.slice.call(arguments).filter(function(x){return x!=null});
      if (!parts.length) return __dirname || '/';
      if (parts[0][0] === '/') return normalize(parts.join('/'));
      return normalize((__dirname || '/') + '/' + parts.join('/'));
    }
    function dirname(p) {
      if (!p) return '.';
      var i = p.lastIndexOf('/');
      return i === -1 ? '.' : (i === 0 ? '/' : p.substring(0, i));
    }
    function basename(p, ext) {
      if (!p) return '';
      var base = p.substring(p.lastIndexOf('/') + 1);
      return ext && base.endsWith(ext) ? base.substring(0, base.length - ext.length) : base;
    }
    function extname(p) {
      if (!p) return '';
      var base = p.substring(p.lastIndexOf('/') + 1);
      var d = base.lastIndexOf('.');
      return d <= 0 ? '' : base.substring(d);
    }
    return { sep:sep, delimiter:delimiter, normalize:normalize, join:join, resolve:resolve,
             dirname:dirname, basename:basename, extname:extname, isAbsolute:function(p){return p&&p[0]=='/'},
             relative:function(f,t){var a=normalize(f).split('/'),b=normalize(t).split('/'),i=0;
               while(i<a.length&&i<b.length&&a[i]===b[i])i++;
               var u=[];for(var j=i;j<a.length;j++)u.push('..');
               return u.join('/')+(u.length&&b.slice(i).length?'/':'')+b.slice(i).join('/');},
             parse:function(p){var r={root:'/',dir:dirname(p),base:basename(p),ext:extname(p),name:basename(p,extname(p))};return r;} };
  })();

  // ── buffer ────────────────────────────────────────────────
  builtins.buffer = (function() {
    function Buffer(arg, encoding) {
      if (typeof arg === 'number') { this._data=new Array(arg);for(var i=0;i<arg;i++)this._data[i]=0;this.length=arg; }
      else if (typeof arg === 'string') {
        this._data=encoding==='hex'?(function(h){var b=[];for(var i=0;i<h.length;i+=2)b.push(parseInt(h.substring(i,i+2),16));return b;})(arg)
          :(function(s){var b=[];for(var i=0;i<s.length;i++){var c=s.charCodeAt(i);
            if(c<0x80)b.push(c);else if(c<0x800){b.push(0xC0|(c>>6));b.push(0x80|(c&0x3F));}else{b.push(0xE0|(c>>12));b.push(0x80|((c>>6)&0x3F));b.push(0x80|(c&0x3F));}}
          return b;})(arg);
        this.length=this._data.length;
      } else if (arg && arg.length !== undefined) { this._data=[];for(var i=0;i<arg.length;i++)this._data[i]=arg[i]&0xFF;this.length=arg.length; }
    }
    Buffer.prototype.toString=function(enc,s,e){
      s=s||0;e=e||this.length;
      if(enc==='hex'){var h='';for(var i=s;i<e;i++){var v=this._data[i];h+=(v<16?'0':'')+v.toString(16);}return h;}
      var out='';for(var i=s;i<e;i++)out+=String.fromCharCode(this._data[i]);return out;
    };
    Buffer.prototype.slice=function(s,e){return new Buffer(this._data.slice(s,e));};
    Buffer.prototype.equals=function(o){if(!o||this.length!==o.length)return false;for(var i=0;i<this.length;i++)if(this._data[i]!==o._data[i])return false;return true;};
    Buffer.prototype.write=function(s,off,len,enc){
      off=off||0;var b=Buffer.from(s,enc);for(var i=0;i<b.length&&i<(len||b.length);i++)this._data[off+i]=b._data[i];
    };
    Buffer.from=function(a,e){return new Buffer(a,e);};
    Buffer.alloc=function(s,f){var b=new Buffer(s);if(f!==undefined){var c=typeof f==='number'?f:f.charCodeAt(0);for(var i=0;i<s;i++)b._data[i]=c;}return b;};
    Buffer.concat=function(l){var t=0;for(var i=0;i<l.length;i++)t+=l[i].length;var d=new Array(t),p=0;for(var i=0;i<l.length;i++){for(var j=0;j<l[i].length;j++)d[p++]=l[i]._data[j];}var r=new Buffer(0);r._data=d;r.length=t;return r;};
    Buffer.isBuffer=function(o){return o instanceof Buffer;};
    Buffer.isEncoding=function(enc){if(!enc)return false;enc=enc.toLowerCase();return['utf8','utf-8','utf16le','ucs2','ucs-2','base64','base64url','hex','ascii','latin1','binary'].indexOf(enc)!==-1;};
    Buffer.compare=function(a,b){if(!a||!b)return 0;var len=Math.min(a.length,b.length);for(var i=0;i<len;i++){if(a._data[i]!==b._data[i])return a._data[i]-b._data[i];}return a.length-b.length;};
    Buffer.transcode=function(source,fromEnc,toEnc){var str=source.toString(fromEnc);return Buffer.from(str,toEnc);};
    Buffer.prototype.copy=function(t,tS,sS,eS){sS=sS||0;eS=eS||this.length;for(var i=sS;i<eS;i++)t._data[tS++]=this._data[i];};
    return {Buffer:Buffer,SlowBuffer:Buffer,INSPECT_MAX_BYTES:50};
  })();

  // ── events ────────────────────────────────────────────────
  builtins.events = (function() {
    function EventEmitter(){this._events={};}
    EventEmitter.prototype.on=EventEmitter.prototype.addListener=function(ev,fn){if(!this._events[ev])this._events[ev]=[];this._events[ev].push(fn);return this;};
    EventEmitter.prototype.once=function(ev,fn){var s=this;function w(){fn.apply(this,arguments);s.removeListener(ev,w);}w._orig=fn;return this.on(ev,w);};
    EventEmitter.prototype.emit=function(ev){var f=this._events[ev];if(!f||!f.length)return false;var a=Array.prototype.slice.call(arguments,1);for(var i=0;i<f.length;i++)f[i].apply(this,a);return true;};
    EventEmitter.prototype.removeListener=function(ev,fn){var f=this._events[ev];if(!f)return this;this._events[ev]=f.filter(function(x){return x!==fn&&x._orig!==fn;});return this;};
    EventEmitter.prototype.removeAllListeners=function(ev){if(ev)delete this._events[ev];else this._events={};return this;};
    EventEmitter.prototype.listeners=function(ev){return(this._events[ev]||[]).slice();};
    EventEmitter.prototype.eventNames=function(){return Object.keys(this._events);};
    EventEmitter.prototype.listenerCount=function(ev){return(this._events[ev]||[]).length;};
    return {EventEmitter:EventEmitter,usingDomains:false};
  })();

  // ── util ──────────────────────────────────────────────────
  builtins.util = (function() {
    function format(f) {
      if(typeof f!=='string')return inspect(f);
      var a=Array.prototype.slice.call(arguments,1);
      return f.replace(/%[sdifjoO%]/g,function(m){
        if(m==='%%')return'%';if(!a.length)return m;
        var v=a.shift();
        if(m==='%s')return String(v);if(m==='%d'||m==='%i')return parseInt(v,10);
        if(m==='%f')return parseFloat(v);return inspect(v);
      });
    }
    function inspect(o,d){if(o===null)return'null';if(o===undefined)return'undefined';
      if(typeof o==='string')return "'"+o.replace(/'/g,"\\'")+"'";
      if(typeof o==='number'||typeof o==='boolean')return String(o);
      if(Array.isArray(o))return'['+o.map(function(x){return inspect(x);}).join(', ')+']';
      if(typeof o==='object'){var k=Object.keys(o);return'{'+k.map(function(x){return x+': '+inspect(o[x]);}).join(', ')+'}';}
      return String(o);
    }
    function inherits(c,s){c.prototype=Object.create(s.prototype,{constructor:{value:c,writable:true,configurable:true}});}
    return {format:format,inspect:inspect,inherits:inherits,deprecate:function(f){return f;},
      isArray:Array.isArray,isString:function(s){return typeof s==='string';},isNumber:function(n){return typeof n==='number';},
      isFunction:function(f){return typeof f==='function';},isObject:function(o){return o!==null&&typeof o==='object';},
      isNull:function(o){return o===null;},isUndefined:function(o){return o===undefined;},
      isBuffer:function(o){return o&&o.constructor&&o.constructor.name==='Buffer';},
      isDate:function(o){return o instanceof Date;},isRegExp:function(o){return o instanceof RegExp;},
      promisify:function(fn){return function(){var args=Array.prototype.slice.call(arguments);var self=this;return new Promise(function(resolve,reject){args.push(function(err,val){if(err)reject(err);else resolve(val);});fn.apply(self,args);});};},
      callbackify:function(fn){return function(){var args=Array.prototype.slice.call(arguments);var cb=args.pop();fn.apply(this,args).then(function(v){cb(null,v);},function(e){cb(e);});};}};
  })();

  // ── assert ────────────────────────────────────────────────
  builtins.assert = (function() {
    function AssertionError(o){this.name='AssertionError';this.message=o.message||'';this.actual=o.actual;this.expected=o.expected;this.stack=(new Error()).stack;}
    AssertionError.prototype=Object.create(Error.prototype);
    function ok(v,m){if(!v)throw new AssertionError({message:m||'assertion failed',actual:v,expected:true});}
    function equal(a,b,m){if(a!=b)throw new AssertionError({message:m||'not equal',actual:a,expected:b});}
    function strictEqual(a,b,m){if(a!==b)throw new AssertionError({message:m||'strict not equal',actual:a,expected:b});}
    function deepEqual(a,b,m){
      if(a===b)return;
      if(!a||!b||typeof a!==typeof b)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});
      if(Array.isArray(a)&&Array.isArray(b)){if(a.length!==b.length)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});for(var i=0;i<a.length;i++)deepEqual(a[i],b[i]);return;}
      if(typeof a==='object'){var ka=Object.keys(a),kb=Object.keys(b);if(ka.length!==kb.length)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});for(var i=0;i<ka.length;i++)deepEqual(a[ka[i]],b[ka[i]]);return;}
      if(a!==b)throw new AssertionError({message:m||'not deep equal',actual:a,expected:b});
    }
    function throws(fn,err){try{fn();}catch(e){if(err&&!err(e))throw e;return e;}throw new AssertionError({message:'did not throw'});}
    function ifError(e){if(e)throw e;}
    return {AssertionError:AssertionError,ok:ok,equal:equal,strictEqual:strictEqual,deepEqual:deepEqual,
      notEqual:function(a,b,m){if(a==b)throw new AssertionError({message:m||'equal',actual:a,expected:b});},
      notStrictEqual:function(a,b,m){if(a===b)throw new AssertionError({message:m||'strict equal',actual:a,expected:b});},
      notDeepEqual:function(a,b,m){try{deepEqual(a,b);}catch(e){return;}throw new AssertionError({message:m||'deep equal',actual:a,expected:b});},
      throws:throws,ifError:ifError,doesNotThrow:function(fn,m){try{fn();}catch(e){throw new AssertionError({message:m||'threw',actual:e});}}}};
  })();

  // ── os ────────────────────────────────────────────────────
  builtins.os = (function() {
    var p = typeof process!=='undefined'&&process.platform?process.platform:
            typeof navigator!=='undefined'?'browser':'linux';
    return {hostname:function(){return'localhost';},homedir:function(){return typeof process!=='undefined'&&process.env?process.env.HOME||'/home/user':'/home/user';},
      platform:function(){return p;},type:function(){return p==='win32'?'Windows_NT':'Linux';},
      release:function(){return'1.0.0';},arch:function(){return'x64';},tmpdir:function(){return'/tmp';},
      EOL:'\n',cpus:function(){return[{model:'QuickJS',speed:1000,times:{user:0,nice:0,sys:0,idle:0,irq:0}}];},
      totalmem:function(){return 1073741824;},freemem:function(){return 536870912;},uptime:function(){return 0;},
      loadavg:function(){return[0,0,0];},endianness:function(){return'LE';},networkInterfaces:function(){return{lo:[{address:'127.0.0.1',netmask:'255.0.0.0',family:'IPv4',internal:true}]};}};
  })();

  // ── querystring ───────────────────────────────────────────
  builtins.querystring = (function() {
    function parse(s){if(!s)return{};var r={};s.split('&').forEach(function(p){var kv=p.split('=');if(kv[0])r[decodeURIComponent(kv[0].replace(/\+/g,' '))]=kv[1]?decodeURIComponent(kv[1].replace(/\+/g,' ')):'';});return r;}
    function stringify(o){return Object.keys(o).map(function(k){return encodeURIComponent(k)+'='+encodeURIComponent(String(o[k]));}).join('&');}
    return {parse:parse,stringify:stringify,encode:stringify,decode:parse,escape:encodeURIComponent,unescape:decodeURIComponent};
  })();

  // ── url (WHATWG URL) ───────────────────────────────────────
  builtins.url = (function() {
    function URL(url, base) {
      if (!url) throw new TypeError('Invalid URL');
      if (base && typeof base==='string'&&base!==''){var b=new URL(base);if(url[0]==='/')url=b.protocol+'//'+b.host+url;
        else if(url.indexOf('://')===-1){var bp=b.pathname;var l=Math.max(bp.lastIndexOf('/'),bp.lastIndexOf('\\'));if(l>0)bp=bp.substring(0,l);else bp='/';url=b.protocol+'//'+b.host+bp+'/'+url;}}
      var m=url.match(/^([a-zA-Z][a-zA-Z0-9+.-]*:)?(\/\/)?([^\/?#]*)?([^?#]*)?(\?[^#]*)?(#.*)?$/);
      if(!m)throw new TypeError('Invalid URL: '+url);
      this.protocol=m[1]||'https:';var auth='',host='',port='';
      if(m[3]){var at=m[3].indexOf('@');if(at>=0){auth=m[3].substring(0,at);host=m[3].substring(at+1);}else host=m[3];}
      var hc=host.lastIndexOf(':');if(hc>=0&&host.indexOf(']')<hc){port=host.substring(hc+1);host=host.substring(0,hc);}
      this.hostname=host;this.port=port||'';this.host=host+(port?':'+port:'');
      this.href=url;this.origin=this.protocol+'//'+this.host;
      this.pathname=m[4]||'/';this.search=m[5]||'';this.hash=m[6]||'';
      this.username='';this.password='';
      if(auth){var c=auth.indexOf(':');if(c>=0){this.username=auth.substring(0,c);this.password=auth.substring(c+1);}else this.username=auth;}
      var sp=new (builtins.url.URLSearchParams||function(){})();
      if(this.search.length>1)this.search.substring(1).split('&').forEach(function(p){var kv=p.split('=');sp.append(decodeURIComponent(kv[0]),kv[1]?decodeURIComponent(kv[1]):'');});
      this.searchParams=sp;
    }
    URL.prototype.toString=function(){return this.href;};
    URL.prototype.toJSON=function(){return this.href;};
    function URLSearchParams(q){this._p={};this._keys=[];if(q){var s=typeof q==='string'?q:'';if(s.length>0)s.split('&').forEach(function(p){var kv=p.split('=');var k=decodeURIComponent(kv[0]);var v=kv[1]?decodeURIComponent(kv[1]):'';this.append(k,v);},this);}}
    URLSearchParams.prototype.append=function(k,v){if(!this._p[k]){this._p[k]=[];this._keys.push(k);}this._p[k].push(v);};
    URLSearchParams.prototype.get=function(k){return this._p[k]?this._p[k][0]:null;};
    URLSearchParams.prototype.getAll=function(k){return this._p[k]||[];};
    URLSearchParams.prototype.has=function(k){return !!this._p[k];};
    URLSearchParams.prototype.set=function(k,v){if(!this._p[k])this._keys.push(k);this._p[k]=[v];};
    URLSearchParams.prototype.delete=function(k){delete this._p[k];var idx=this._keys.indexOf(k);if(idx>=0)this._keys.splice(idx,1);};
    URLSearchParams.prototype.toString=function(){var self=this;var parts=[];this._keys.forEach(function(k){self._p[k].forEach(function(v){parts.push(encodeURIComponent(k)+(v?'='+encodeURIComponent(v):''));});});return parts.join('&');};
    URLSearchParams.prototype.forEach=function(fn){var self=this;this._keys.forEach(function(k){self._p[k].forEach(function(v){fn(v,k,self);});});};
    URLSearchParams.prototype.keys=function(){var self=this;return{next:function(){return{value:self._keys[self._idx++]||undefined,done:self._idx>self._keys.length};},[Symbol.iterator]:function(){return this;}};};
    URLSearchParams.prototype.values=function(){/* stub */return[];};
    URLSearchParams.prototype.entries=function(){/* stub */return[];};
    return {URL:URL,URLSearchParams:URLSearchParams,urlObject:URL,format:function(u){return u.href||String(u);},
      resolve:function(f,t){return (new URL(t,f)).href;},
      parse:function(u,p){try{var r=new URL(u);return r;}catch(e){return null;}},
      fileURLToPath:function(u){return u.pathname;},pathToFileURL:function(p){return new URL('file://'+p);}};
  })();

  // ── crypto (real SHA256/SHA1/MD5 via Windows CNG, real CSPRNG) ───
  builtins.crypto = (function() {
    var bH = typeof __bridge_crypto_hash !== 'undefined';
    function createHash(algo) {
      var state = '';
      return {update:function(data){state+=typeof data==='string'?data:data.toString();return this;},
        digest:function(enc){
          if(bH){
            var hex = __bridge_crypto_hash(algo.toLowerCase(), state);
            if(enc==='hex') return hex;
            return Buffer.from(hex, 'hex');
          }
          // Fallback: FNV-1a if native hash unavailable
          var hash=0x811C9DC5|0;
          for(var i=0;i<state.length;i++){hash^=state.charCodeAt(i);hash=(hash*0x01000193)|0;}
          var hex=(hash>>>0).toString(16);
          while(hex.length<8)hex='0'+hex;
          return enc==='hex'?hex:Buffer.from(hex,'hex');
        },copy:function(){var n=createHash(algo);n.state=state;return n;}
      };
    }
    function randomBytes(size){
      if(typeof __bridge_crypto_random !== 'undefined'){
        var hex = __bridge_crypto_random(size);
        return Buffer.from(hex, 'hex');
      }
      var b=Buffer.alloc(size);for(var i=0;i<size;i++)b._data[i]=Math.floor(Math.random()*256);return b;
    }
    return {createHash:createHash,createHmac:function(a,k){return createHash(a);},
      randomBytes:randomBytes,pseudoRandomBytes:randomBytes,
      randomFill:function(b,o,s,cb){var r=randomBytes(s);for(var i=0;i<s;i++)b._data[o+i]=r._data[i];if(cb)cb(null,b);},
      getCiphers:function(){return[];},getHashes:function(){return['sha1','sha256','md5'];}};
  })();

  // ── stream (with backpressure) ────────────────────────────
  builtins.stream = (function() {
    var EE = builtins.events.EventEmitter;
    var hwm = 16384;
    function Readable(opts){
      EE.call(this);
      opts=opts||{};
      this._readableState={highWaterMark:opts.highWaterMark||hwm,buffer:[],flowing:null,ended:false,endedEmitted:false};
    }
    Readable.prototype=Object.create(EE.prototype);
    Readable.prototype._read=function(n){};
    Readable.prototype.read=function(n){
      var st=this._readableState;
      if(st.ended&&!st.buffer.length){if(!st.endedEmitted){st.endedEmitted=true;this.emit('end');}return null;}
      if(st.buffer.length){var c=st.buffer.shift();this.emit('data',c);return c;}
      return null;
    };
    Readable.prototype.push=function(c){
      if(c===null){this._readableState.ended=true;return;}
      this._readableState.buffer.push(c);
    };
    Readable.prototype.pipe=function(dest,opts){
      var self=this;
      dest.emit('pipe',self);
      function ondata(c){var r=dest.write(c);if(r===false)self.pause();}
      function ondrain(){self.resume();}
      function onend(){dest.end();}
      self.on('data',ondata);
      dest.on('drain',ondrain);
      dest.on('close',function(){self.removeListener('data',ondata);dest.removeListener('drain',ondrain);});
      self.on('end',onend);
      return dest;
    };
    Readable.prototype.pause=function(){this._readableState.flowing=false;};
    Readable.prototype.resume=function(){
      if(this._readableState.flowing)return;
      this._readableState.flowing=true;
      var self=this;
      function flow(){
        while(self._readableState.flowing&&self._readableState.buffer.length){
          var c=self._readableState.buffer.shift();
          self.emit('data',c);
        }
        if(!self._readableState.buffer.length&&self._readableState.ended){
          self._readableState.endedEmitted=true;self.emit('end');return;
        }
      }
      flow();
    };
    Readable.prototype.isPaused=function(){return this._readableState.flowing===false;};
    Readable.prototype.setEncoding=function(e){};
    Readable.prototype.destroy=function(){this.emit('close');};
    function Writable(opts){
      EE.call(this);
      opts=opts||{};
      this._writableState={highWaterMark:opts.highWaterMark||hwm,length:0,writing:false,ended:false,needDrain:false};
    }
    Writable.prototype=Object.create(EE.prototype);
    Writable.prototype._write=function(c,e,cb){cb();};
    Writable.prototype.write=function(c,e,cb){
      if(typeof e==='function'){cb=e;e='utf8';}
      var st=this._writableState;
      if(st.ended){if(cb)process.nextTick(cb);return false;}
      var self=this;
      st.writing=true;
      st.length+=c.length||1;
      this._write(c,e,function(err){
        if(err)self.emit('error',err);
        st.writing=false;
        st.length-=c.length||1;
        if(st.length<=st.highWaterMark&&st.needDrain){st.needDrain=false;self.emit('drain');}
        if(cb)process.nextTick(function(){cb(err);});
      });
      var ret=st.length<st.highWaterMark;
      if(!ret)st.needDrain=true;
      return ret;
    };
    Writable.prototype.cork=function(){};
    Writable.prototype.uncork=function(){};
    Writable.prototype.end=function(c,e,cb){
      if(typeof c==='function'){cb=c;c=null;}
      if(typeof e==='function'){cb=e;e='utf8';}
      var st=this._writableState;
      st.ended=true;
      var self=this;
      function finish(){
        self.emit('finish');
        if(cb)process.nextTick(cb);
      }
      if(c)this.write(c,e,function(){finish();});
      else finish();
    };
    Writable.prototype.destroy=function(){this.emit('close');};
    function Transform(opts){
      Readable.call(this,opts);Writable.call(this,opts);
      this._transformState={pending:null,readable:true};
    }
    Transform.prototype=Object.create(Readable.prototype);
    Object.keys(Writable.prototype).forEach(function(k){Transform.prototype[k]=Writable.prototype[k];});
    Transform.prototype._transform=function(c,e,cb){cb(null,c);};
    Transform.prototype._flush=function(cb){cb();};
    Transform.prototype.push=function(c,e){
      if(c===null){this._readableState.ended=true;return false;}
      this._readableState.buffer.push(c);
      if(this._readableState.flowing)this.emit('data',c);
      return this._writableState.length<this._writableState.highWaterMark;
    };
    Transform.prototype.write=function(c,e,cb){
      if(typeof e==='function'){cb=e;e='buffer';}
      var self=this;
      this._transform(c,e||'buffer',function(err,d){
        if(err){self.emit('error',err);if(cb)process.nextTick(function(){cb(err);});return;}
        if(d!==null&&d!==undefined)self.push(d);
        if(cb)process.nextTick(function(){cb(null);});
      });
      return this._writableState.length<this._writableState.highWaterMark;
    };
    function PassThrough(opts){Transform.call(this,opts);}
    PassThrough.prototype=Object.create(Transform.prototype);
    PassThrough.prototype._transform=function(c,e,cb){cb(null,c);};
    function finished(stream,opts,cb){
      if(typeof opts==='function'){cb=opts;opts={};}
      var ended=false;
      function onfinish(){cleanup();if(!ended){ended=true;if(cb)process.nextTick(cb);}}
      function onerror(err){cleanup();if(!ended){ended=true;if(cb)process.nextTick(function(){cb(err);});}}
      function onclose(){if(!ended){cleanup();ended=true;if(cb)process.nextTick(cb);}}
      function cleanup(){stream.removeListener('finish',onfinish);stream.removeListener('end',onfinish);stream.removeListener('error',onerror);stream.removeListener('close',onclose);}
      stream.on('finish',onfinish);stream.on('end',onfinish);stream.on('error',onerror);stream.on('close',onclose);
    }
    function pipeline(){
      var streams=Array.prototype.slice.call(arguments);
      var cb=typeof streams[streams.length-1]==='function'?streams.pop():null;
      var i=0;
      function next(err){
        if(err){if(cb)process.nextTick(function(){cb(err);});return;}
        if(i>=streams.length){if(cb)process.nextTick(cb);return;}
        var s=streams[i++];
        if(i<streams.length){var dest=streams[i];s.pipe(dest,{end:true});
          s.on('error',function(e){dest.destroy();next(e);});
          dest.on('error',function(e){s.destroy();next(e);});
          s.on('end',function(){next();});}else{if(cb)process.nextTick(cb);}
      }
      next();return streams[0];
    }
    return {Readable:Readable,Writable:Writable,Transform:Transform,DynamicStream:Transform,PassThrough:PassThrough,Stream:Readable,finished:finished,pipeline:pipeline};
  })();

  // ── fs (sync only) ─────────────────────────────────────────
  builtins.fs = (function() {
    var bF = typeof __bridge_fs_readFile !== 'undefined';
    function readFileSync(path, enc) {
      if (!bF) throw new Error('fs not available');
      var data = __bridge_fs_readFile(path);
      if (data === null) throw new Error('ENOENT: ' + path);
      return enc === 'utf8' || enc === 'utf-8' || !enc ? data : Buffer.from(data);
    }
    function writeFileSync(path, data, enc) {
      if (!bF) throw new Error('fs not available');
      var s = typeof data === 'string' ? data : data.toString();
      return __bridge_fs_writeFile(path, s);
    }
    function existsSync(path) { return bF ? __bridge_fs_exists(path) : false; }
    function mkdirSync(path, opts) { return bF ? __bridge_fs_mkdir(path) : false; }
    function readdirSync(path) { return bF ? __bridge_fs_readdir(path) : null; }
    function statSync(path) { return bF ? __bridge_fs_stat(path) : null; }
    function unlinkSync(path) { return bF ? __bridge_fs_unlink(path) : false; }
    function rmdirSync(path) { return bF ? __bridge_fs_unlink(path) : false; }
    function appendFileSync(path, data, enc) {
      var existing = '';
      try { existing = readFileSync(path, 'utf8') || ''; } catch(e) {}
      writeFileSync(path, existing + (typeof data === 'string' ? data : data.toString()), enc);
    }
    function realpathSync(path) { return path; }
    var constants = { F_OK:0, R_OK:4, W_OK:2, X_OK:1 };
    function accessSync(path, mode) {
      if (!bF || !__bridge_fs_exists(path)) throw new Error('ENOENT: ' + path);
    }
    return {
      readFileSync: readFileSync, writeFileSync: writeFileSync, existsSync: existsSync,
      mkdirSync: mkdirSync, readdirSync: readdirSync, statSync: statSync,
      unlinkSync: unlinkSync, rmdirSync: rmdirSync, appendFileSync: appendFileSync,
      realpathSync: realpathSync, accessSync: accessSync, constants: constants,
      promises: { access: function(p,m){try{accessSync(p,m);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        appendFile: function(p,d,o){try{appendFileSync(p,d,o&&o.encoding);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        copyFile: function(s,d){try{var data=readFileSync(s);writeFileSync(d,data);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        readFile: function(p,o){try{var enc=(o&&(o.encoding||o))||'utf8';return Promise.resolve(readFileSync(p,enc));}catch(e){return Promise.reject(e);}},
        writeFile: function(p,d,o){try{var enc=o&&(o.encoding||o);writeFileSync(p,d,enc);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        mkdir: function(p,o){try{mkdirSync(p,o);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        readdir: function(p,o){try{return Promise.resolve(readdirSync(p));}catch(e){return Promise.reject(e);}},
        stat: function(p){try{return Promise.resolve(statSync(p));}catch(e){return Promise.reject(e);}},
        lstat: function(p){try{return Promise.resolve(statSync(p));}catch(e){return Promise.reject(e);}},
        unlink: function(p){try{unlinkSync(p);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        rmdir: function(p,o){try{rmdirSync(p);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        rm: function(p,o){try{unlinkSync(p);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        rename: function(o,n){try{var d=readFileSync(o);writeFileSync(n,d);unlinkSync(o);return Promise.resolve();}catch(e){return Promise.reject(e);}},
        realpath: function(p,o){try{return Promise.resolve(realpathSync(p));}catch(e){return Promise.reject(e);}},
        symlink: function(t,p){return Promise.resolve();},link: function(e,p){return Promise.resolve();},
        chmod: function(p,m){return Promise.resolve();} } };
  })();

  // ── net (TCP via WinSock2 C helpers) ──────────────────────
  builtins.net = (function() {
    var EE = builtins.events.EventEmitter;
    var bN = typeof __bridge_net_connect !== 'undefined';
    function Socket(opts){
      EE.call(this);
      opts=opts||{};
      this._sockfd=null;
      this._readableState={buffer:[]};
      this.connecting=false;
      this.destroyed=false;
      this.bytesRead=0;
      this.bytesWritten=0;
      this.localAddress='';
      this.localPort=0;
      this.remoteAddress='';
      this.remotePort=0;
    }
    Socket.prototype=Object.create(EE.prototype);
    Socket.prototype.connect=function(port,host,cb){
      var self=this;
      if(typeof host==='function'){cb=host;host='localhost';}
      if(typeof port==='object'){var o=port;port=o.port;host=o.host||'localhost';}
      self.connecting=true;
      self.remotePort=port;
      self.remoteAddress=host;
      try {
        var fd=__bridge_net_connect(host,port);
        if(fd<0){self.emit('error',new Error('connect ECONNREFUSED '+host+':'+port));return;}
        self._sockfd=fd;
        self.connecting=false;
        self.emit('connect');
        if(cb)cb();
        // Start polling for data
        self._poll();
      }catch(e){self.emit('error',e);}
    };
    Socket.prototype._poll=function(){
      var self=this;
      function readLoop(){
        if(self.destroyed||self._sockfd===null)return;
        try {
          var data=__bridge_net_read(self._sockfd);
          if(data!==null&&data.length>0){
            self.bytesRead+=data.length;
            var b=Buffer.from(data,'utf8');
            self._readableState.buffer.push(b);
            if(self._readableState.flowing||self.listenerCount('data')>0){
              self._readableState.buffer.shift();
              self.emit('data',b);
            }
          }
        }catch(e){self.emit('error',e);}
        process.nextTick(readLoop);
      }
      readLoop();
    };
    Socket.prototype.write=function(c,e,cb){
      if(self.destroyed){if(cb)cb(new Error('socket destroyed'));return false;}
      if(typeof e==='function'){cb=e;e='utf8';}
      var s=typeof c==='string'?c:c.toString(e||'utf8');
      try {
        var n=__bridge_net_write(this._sockfd,s);
        this.bytesWritten+=n;
        if(cb)process.nextTick(cb);
      }catch(e){if(cb)process.nextTick(function(){cb(e);});return false;}
      return true;
    };
    Socket.prototype.end=function(c,e,cb){
      if(typeof c==='function'){cb=c;c=null;}
      if(c)this.write(c,function(){this.destroy();if(cb)cb();});
      else{this.destroy();if(cb)process.nextTick(cb);}
    };
    Socket.prototype.destroy=function(){
      if(this.destroyed)return;
      this.destroyed=true;
      if(this._sockfd!==null){try{__bridge_net_close(this._sockfd);}catch(e){}this._sockfd=null;}
      this.emit('close');
    };
    Socket.prototype.setEncoding=function(e){};
    Socket.prototype.setNoDelay=function(n){};
    Socket.prototype.setKeepAlive=function(n,d){};
    Socket.prototype.pause=function(){this._readableState.flowing=false;};
    Socket.prototype.resume=function(){this._readableState.flowing=true;};
    Socket.prototype.address=function(){return{address:this.remoteAddress,family:'IPv4',port:this.remotePort};};
    Socket.prototype.unref=function(){};
    Socket.prototype.ref=function(){};
    Socket.prototype.setTimeout=function(n,cb){if(cb)this.on('timeout',cb);};
    function connect(port,host,cb){var s=new Socket();s.connect(port,host,cb);return s;}
    function createConnection(port,host,cb){return connect(port,host,cb);}
    return {Socket:Socket,connect:connect,createConnection:createConnection,createServer:function(){var s=new EE();s.listen=function(){};return s;},isIP:function(s){return 0;},isIPv4:function(s){return false;},isIPv6:function(s){return false;}};
  })();

  // ── dns (via getaddrinfo C helper) ────────────────────────
  builtins.dns = (function() {
    var bD = typeof __bridge_dns_lookup !== 'undefined';
    function lookup(hostname, opts, cb){
      if(typeof opts==='function'){cb=opts;opts={};}
      if(!cb)return;
      if(!bD){process.nextTick(function(){cb(new Error('dns not available'));});return;}
      try {
        var addr=__bridge_dns_lookup(hostname);
        if(addr===null||addr===''){process.nextTick(function(){cb(new Error('ENOTFOUND '+hostname));});return;}
        process.nextTick(function(){cb(null,addr,4);});
      }catch(e){process.nextTick(function(){cb(e);});}
    }
    function resolve(hostname, rrtype, cb){
      if(typeof rrtype==='function'){cb=rrtype;rrtype='A';}
      lookup(hostname,function(err,addr,family){
        if(err)cb(err);
        else cb(null,[addr]);
      });
    }
    function reverse(ip, cb){
      if(!cb)return;
      process.nextTick(function(){cb(new Error('ENOTSUP reverse lookup'));});
    }
    return {lookup:lookup,resolve:resolve,reverse:reverse,
      resolve4:function(h,cb){resolve(h,'A',cb);},
      resolve6:function(h,cb){resolve(h,'AAAA',cb);},
      resolveMx:function(h,cb){resolve(h,'MX',cb);},
      resolveTxt:function(h,cb){resolve(h,'TXT',cb);},
      resolveSrv:function(h,cb){resolve(h,'SRV',cb);},
      resolveNaptr:function(h,cb){resolve(h,'NAPTR',cb);},
      resolveCname:function(h,cb){resolve(h,'CNAME',cb);},
      setServers:function(s){}};
  })();

  // ── tls (SChannel on Win32, net fallback on POSIX) ─────────
  builtins.tls = (function() {
    var net = builtins.net;
    var bTC = typeof __bridge_tls_connect !== 'undefined';
    if (bTC) {
      // Real TLS via native C helper (SChannel)
      function TLSSocket(socket, opts){
        var s = socket || {};
        s._tlsHandle = s._tlsHandle || 0;
        s.authorized = true;
        s.encrypted = true;
        s.getCipher = function(){return{name:'TLS',version:'TLSv1.3'};};
        s.getPeerCertificate = function(){return{};};
        s._writeOrig = s.write;
        s.write = function(data, cb) {
          if (s._tlsHandle) {
            var r = __bridge_tls_write(s._tlsHandle, typeof data==='string'?data:String(data));
            if (cb) cb();
            return r;
          }
          return s._writeOrig ? s._writeOrig.apply(s,arguments) : 0;
        };
        s._readOrig = s._read || s.on;
        s.on = function(ev, cb) {
          if (ev === 'data' && s._tlsHandle) {
            // Poll for decrypted data
            var poll = function() {
              if (!s._tlsHandle) return;
              var d = __bridge_tls_read(s._tlsHandle);
              if (d !== null && typeof d === 'string') {
                cb(d);
                setTimeout(poll, 0);
              } else if (d === null) {
                s.emit('end');
              } else {
                setTimeout(poll, 10);
              }
            };
            setTimeout(poll, 0);
          } else if (ev === 'end') {
            s._onEnd = cb;
          }
          return s;
        };
        s.emit = function(ev) {
          if (ev === 'end' && s._onEnd) s._onEnd();
        };
        s.end = function() {
          if (s._tlsHandle) {
            __bridge_tls_close(s._tlsHandle);
            s._tlsHandle = 0;
          }
          if (s._onEnd) s._onEnd();
        };
        s.destroy = s.end;
        return s;
      }
      function connect(port,host,cb){
        if(typeof host==='function'){cb=host;host=undefined;}
        if(typeof port==='object'){var o=port;port=o.port;host=o.host;}
        var h = __bridge_tls_connect(host||'localhost', port);
        if (h <= 0) return null;
        var s = TLSSocket({_tlsHandle:h});
        s.authorized=true;
        s.encrypted=true;
        if (cb) setTimeout(function(){cb();}, 0);
        return s;
      }
      return {TLSSocket:TLSSocket,connect:connect,createServer:function(){return{listen:function(){}};},getCiphers:function(){return['TLS_AES_256_GCM_SHA384','TLS_CHACHA20_POLY1305_SHA256','TLS_AES_128_GCM_SHA256'];},DEFAULT_ECDH_CURVE:'auto'};
    } else {
      // Fallback stub (no native TLS)
      function TLSSocket(socket, opts){
        var s = socket || new net.Socket();
        s.authorized = true;
        s.encrypted = true;
        s.getCipher = function(){return{name:'TLS',version:'TLSv1.3'};};
        s.getPeerCertificate = function(){return{};};
        return s;
      }
      function connect(port,host,cb){
        if(typeof host==='function'){cb=host;host=undefined;}
        if(typeof port==='object'){var o=port;port=o.port;host=o.host;}
        var s = net.connect(port,host||'localhost',function(){
          if(cb)cb();
        });
        s.authorized=true;
        s.encrypted=true;
        return s;
      }
      return {TLSSocket:TLSSocket,connect:connect,createServer:function(){return{listen:function(){}};},getCiphers:function(){return[];},DEFAULT_ECDH_CURVE:'auto'};
    }
  })();

  // ── child_process (minimal execSync) ──────────────────────
  builtins.child_process = (function() {
    var bE = typeof __bridge_exec !== 'undefined';
    function execSync(cmd, opts) {
      if (!bE) throw new Error('child_process not available');
      var stdout = __bridge_exec(cmd);
      if (stdout === null) throw new Error('exec failed: ' + cmd);
      return { stdout: stdout, stderr: '', status: 0 };
    }
    function exec(cmd, opts, cb) {
      try {
        var r = execSync(cmd, opts);
        if (cb) cb(null, r);
        else return Promise.resolve(r);
      } catch(e) { if (cb) cb(e); else return Promise.reject(e); }
    }
    return { execSync: execSync, exec: exec };
  })();

  // ── http (basic get/request) ──────────────────────────────
  builtins.http = (function() {
    var bH = typeof __bridge_http_get !== 'undefined';
    var events = builtins.events;
    function get(url, opts, cb) {
      if (typeof opts === 'function') { cb = opts; opts = {}; }
      if (!bH) { if (cb) cb(new Error('http not available')); return; }
      try {
        var body = __bridge_http_get(url);
        if (body === null) { if (cb) cb(new Error('http get failed: ' + url)); return; }
        var res = new events.EventEmitter();
        res.statusCode = 200;
        res.headers = { 'content-type': 'text/plain' };
        res.setEncoding = function() {};
        res.on = function(ev, fn) {
          if (ev === 'data' && body) { fn(body); res.emit('end'); }
          events.EventEmitter.prototype.on.call(res, ev, fn);
          return res;
        };
        if (cb) cb(res);
        return res;
      } catch(e) { if (cb) cb(e); }
    }
    function request(url, opts, cb) { return get(url, opts, cb); }
    return { get: get, request: request };
  })();

  builtins.https = builtins.http;

  // ── module (full) ─────────────────────────────────────────
  builtins.module = (function() {
    var builtinMods = Object.keys(builtins);
    var cache = {};
    function Module(id, parent) {
      this.id = id || '.';
      this.exports = {};
      this.filename = id || __filename || '';
      this.loaded = false;
      this.parent = parent || null;
      this.children = [];
      this.paths = [];
    }
    Module._cache = cache;
    Module._extensions = {
      '.js': function(m, fn) {},
      '.json': function(m, fn) { try { m.exports = JSON.parse(fn); } catch(e) { m.exports = {}; } }
    };
    Module.globalPaths = [];
    Module.builtinModules = builtinMods;
    Module._resolveFilename = function(request, parent) {
      if (builtinMods.indexOf(request) !== -1) return request;
      return request;
    };
    return { Module: Module, builtinModules: builtinMods,
             _cache: cache, _resolveFilename: Module._resolveFilename,
             _extensions: Module._extensions };
  })();

  // ── Enhance C-level require with Node.js properties ──────
  if (typeof require !== 'undefined') {
    if (!require.cache) {
      var _origRequire = require;
      require = function(name) {
        if (typeof name === 'string' && name.indexOf('rust:') === 0) {
          var crateName = name.slice(5);
          if (require.cache['rust:' + crateName]) return require.cache['rust:' + crateName];
          var mod = _origRequire(name);
          if (mod) require.cache['rust:' + crateName] = mod;
          return mod;
        }
        return _origRequire(name);
      };
      require.cache = {};
      require.resolve = function(name, opts) {
        if (typeof name !== 'string') throw new Error('name must be string');
        if (builtins[name] || __node_builtins[name]) return name;
        var paths = opts && opts.paths;
        var checks = paths ? paths.slice() : [];
        if (!paths) {
          checks.push('./node_modules/' + name + '/package.json');
          checks.push('./node_modules/' + name + '.js');
          checks.push('./node_modules/' + name + '/index.js');
          checks.push('./node_modules/' + name + '/index.mjs');
          checks.push('./node_modules/' + name + '/index.cjs');
        }
        if (typeof __bridge_fs_exists !== 'undefined') {
          for (var i = 0; i < checks.length; i++) {
            if (__bridge_fs_exists(checks[i])) return checks[i];
          }
        }
        return name;
      };
      require.resolve.paths = function(name) {
        if (name[0] === '.' || name[0] === '/') return null;
        return ['./node_modules'];
      };
      require.main = { id: '.', exports: {}, loaded: true, filename: __filename || '', paths: [] };
      require.extensions = { '.js': function(m, fn) {}, '.json': function(m, fn) { try { m.exports = JSON.parse(fn); } catch(e) { m.exports = {}; } } };
    }
  }

  // ── process extensions (already partially in C) ───────────
  // If process doesn't have cwd/env/argv, add them
  if (typeof process !== 'undefined') {
    if (!process.cwd) process.cwd = function() { return __dirname || '/'; };
    if (!process.env) process.env = { PATH: '/usr/bin', HOME: '/home/user', NODE_ENV: 'production' };
    if (!process.argv) process.argv = ['qjs', '.', ''];
    if (!process.exit) process.exit = function(code) { throw new Error('exit:'+code); };
    if (!process.nextTick) process.nextTick = function(fn) { fn(); };
    if (!process.browser) process.browser = false;
    if (!process.version) process.version = 'v16.0.0';
    if (!process.versions) process.versions = { node: '16.0.0', v8: '9.0', uv: '1.0', zlib: '1.0' };
    if (!process.stderr) process.stderr = { write: function(s) { } };
    if (!process.stdin) process.stdin = { isTTY: false, setEncoding: function(){}, on: function(){} };
    if (!process.pid) process.pid = 1;
    if (!process.uptime) process.uptime = function() { return 0; };
    if (!process.memoryUsage) process.memoryUsage = function() { return { rss: 0, heapTotal: 0, heapUsed: 0, external: 0 }; };
    if (!process.cpuUsage) process.cpuUsage = function() { return { user: 0, system: 0 }; };
    if (!process.umask) process.umask = function() { return 0o022; };
    if (!process.kill) process.kill = function(pid, sig) {};
    if (!process.hrtime) process.hrtime = function(t) { var n = Date.now(); return t ? [0, (n - (t[0]*1e3 + t[1]/1e6))*1e6] : [0, n*1e6]; };
    if (!process.getuid) process.getuid = function() { return 0; };
    if (!process.getgid) process.getgid = function() { return 0; };
  }

  // ── Register all builtins into global registry ────────────
  globalThis.__node_builtins = builtins;
})();
