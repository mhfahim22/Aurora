#include "bridge_npm_impl.hpp"
#include "bridge_shared.h"
#include <string>
#include <fstream>
#include <cstdlib>
void gen_quickjs_npm_wrapper(const std::string& pkg, const std::string& dir)
{
    /* ── Pre-transpile .mjs files via esbuild (if available) ── */
    {
        std::vector<std::string> search_dirs;
        search_dirs.push_back(dir + "/node_modules/" + pkg);
        search_dirs.push_back("node_modules/" + pkg);
        if (fs::is_directory(dir + "/" + pkg))
            search_dirs.push_back(dir + "/" + pkg);
        for (const auto& sd : search_dirs) {
            if (!fs::is_directory(sd)) continue;
            std::error_code ec;
            for (auto& de : fs::recursive_directory_iterator(
                     sd, fs::directory_options::skip_permission_denied, ec)) {
                if (de.is_regular_file()) {
                    const std::string path = de.path().string();
                    if (path.size() >= 4 &&
                        path.compare(path.size() - 4, 4, ".mjs") == 0) {
                        run_esbuild_transpile(path);
                    }
                }
            }
        }
    }

    /* Generate the C bridge DLL using embedded QuickJS engine */
    {
        std::ostringstream cw;
        std::string pid = sanitize_ident(pkg);  /* safe C identifier */
        std::string pkg_upper = pid;
        for (auto& c : pkg_upper) c = (char)toupper((unsigned char)c);

        cw << "/* Auto-generated QuickJS npm bridge DLL for " << pkg << " */\n";
        cw << "#define _GNU_SOURCE\n";
        cw << "#include \"quickjs_config.h\"\n";
        cw << "#include \"quickjs.h\"\n";
        cw << "#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n#include <stdint.h>\n";
#ifdef _WIN32
        cw << "#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include <direct.h>\n#include <sys/stat.h>\n#include <io.h>\n#define EXPORT __declspec(dllexport)\n";
#else
        cw << "#include <unistd.h>\n#include <sys/stat.h>\n#include <dirent.h>\n#include <dlfcn.h>\n#define EXPORT __attribute__((visibility(\"default\")))\n";
#endif
        cw << "\nstatic JSRuntime *g_rt = NULL;\nstatic JSContext *g_ctx = NULL;\nstatic int g_inited = 0;\nstatic int g_loaded = 0;\n";
        cw << "#define MAX_OBJS 65536\nstatic JSValue g_objs[MAX_OBJS];\nstatic int g_next_id = 2;\n";
        cw << "static char g_last_error[4096]=\"\";\n";
        cw << "static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n";
        cw << "/* Performance counters */\n";
        cw << "static struct{unsigned require_calls;unsigned bc_hits;unsigned bc_misses;\n";
        cw << "  unsigned exports_lookups;unsigned eval_time_ms;unsigned resolve_time_ms;}g_perf={0};\n\n";

        /* ── Embedded Node.js builtin polyfills ── */
        {
            std::string bjs;
            std::string bjs_path = quickjs_dir() + "/node_builtins.js";
            std::ifstream bjs_f(bjs_path);
            if (bjs_f.is_open()) {
                bjs.assign((std::istreambuf_iterator<char>(bjs_f)),
                           std::istreambuf_iterator<char>());
            }
            cw << "static const char node_builtins_js[] = \"";
            for (char c : bjs) {
                switch (c) {
                    case '\\': cw << "\\\\"; break;
                    case '"':  cw << "\\\""; break;
                    case '\n': cw << "\\n";  break;
                    case '\r': cw << "\\r";  break;
                    case '\t': cw << "\\t";  break;
                    default:   cw << c;      break;
                }
            }
            cw << "\";\n\n";
        }

        /* JSON helpers */
        cw << "static void json_escape(const char *in, char *out, size_t outsz) {\n";
        cw << "  size_t j=0; for(size_t i=0;in[i]&&j+6<outsz;i++){\n";
        cw << "    unsigned char c=(unsigned char)in[i];\n";
        cw << "    if(c=='\\\\'||c=='\"'){out[j++]='\\\\';out[j++]=c;}\n";
        cw << "    else if(c=='\\n'){out[j++]='\\\\';out[j++]='n';}\n";
        cw << "    else if(c=='\\r'){out[j++]='\\\\';out[j++]='r';}\n";
        cw << "    else if(c=='\\t'){out[j++]='\\\\';out[j++]='t';}\n";
        cw << "    else if(c<32){j+=snprintf(out+j,outsz-j,\"\\\\u%04x\",c);}\n";
        cw << "    else out[j++]=c;}\n";
        cw << "  out[j]='\\0';}\n\n";

        cw << "static void* store_json(const char* json){\n";
        cw << "  if(!json)return NULL;char**pp=(char**)malloc(sizeof(char*));if(!pp)return NULL;\n";
        cw << "  *pp=(char*)malloc(strlen(json)+1);if(!*pp){free(pp);return NULL;}strcpy(*pp,json);\n";
        cw << "  return (void*)((intptr_t)pp | 1);}\n";
        cw << "#define UNTAG(h) ((void*)((intptr_t)(h) & ~1))\n";
        cw << "static const char* get_json(void* handle){\n";
        cw << "  if(!handle)return\"null\";return*((const char**)UNTAG(handle));}\n\n";

        cw << "static char* jsval_to_json(JSContext*ctx,JSValue val){\n";
        cw << "  JSValue j=JS_JSONStringify(ctx,val,JS_NULL,JS_NULL);\n";
        cw << "  if(JS_IsException(j))return strdup(\"null\");\n";
        cw << "  const char*s=JS_ToCString(ctx,j);char*r=s?strdup(s):strdup(\"null\");\n";
        cw << "  if(s)JS_FreeCString(ctx,s);JS_FreeValue(ctx,j);return r;}\n\n";

        cw << "static void strip_dots(char*out,const char*md,const char*path){\n";
        cw << "  if(path[0]=='.'&&path[1]=='/')snprintf(out,4096,\"%s/%s\",md,path+2);\n";
        cw << "  else snprintf(out,4096,\"%s/%s\",md,path);}\n\n";

        cw << "/* Resolve exports key against requested subpath, handling wildcard patterns */\n";
        cw << "static void resolve_exports_key(const char*key,const char*subpath,JSValue val,\n";
        cw << "  const char*md,int*match,char*matched_path,int path_sz,JSContext*ctx){\n";
        cw << "  *match=0;matched_path[0]=0;\n";
        cw << "  /* Check if key matches subpath */\n";
        cw << "  int key_match=0;char wildcard[4096]=\"\";wildcard[0]=0;\n";
        cw << "  if(strcmp(key,subpath)==0){key_match=1;}\n";
        cw << "  else{\n";
        cw << "    /* Wildcard pattern: key contains '*' */\n";
        cw << "    const char*star=strchr(key,'*');\n";
        cw << "    if(star){\n";
        cw << "      size_t prefix_len=(size_t)(star-key);\n";
        cw << "      const char*suffix=star+1;\n";
        cw << "      size_t suffix_len=strlen(suffix);\n";
        cw << "      size_t sub_len=strlen(subpath);\n";
        cw << "      if(sub_len>=prefix_len+suffix_len&&\n";
        cw << "         strncmp(key,subpath,prefix_len)==0&&\n";
        cw << "         strncmp(suffix,subpath+sub_len-suffix_len,suffix_len)==0){\n";
        cw << "        key_match=1;\n";
        cw << "        size_t wl=sub_len-prefix_len-suffix_len;\n";
        cw << "        if(wl<sizeof(wildcard)-1){memcpy(wildcard,subpath+prefix_len,wl);wildcard[wl]=0;}\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  if(!key_match)return;\n";
        cw << "  /* Resolve value: could be string, condition object, or nested pattern */\n";
        cw << "  if(JS_IsString(val)){\n";
        cw << "    const char*vs=JS_ToCString(ctx,val);\n";
        cw << "    if(vs){char buf[4096];snprintf(buf,sizeof(buf),\"%s/%s\",md,vs[0]=='.'&&vs[1]=='/'?vs+2:vs);\n";
        cw << "      /* Substitute wildcard */\n";
        cw << "      if(wildcard[0]){char*wp=strstr(buf,\"*\");if(wp){*wp=0;snprintf(matched_path,path_sz,\"%s%s%s\",buf,wildcard,wp+1);}else snprintf(matched_path,path_sz,\"%s\",buf);}\n";
        cw << "      else snprintf(matched_path,path_sz,\"%s\",buf);\n";
        cw << "      *match=1;JS_FreeCString(ctx,vs);}\n";
        cw << "  }else if(JS_IsObject(val)){\n";
        cw << "    /* Try conditions: require > node > default > import */\n";
        cw << "    const char*conds[]={\"require\",\"node\",\"default\",\"import\",NULL};\n";
        cw << "    for(int ci=0;conds[ci]&&!*match;ci++){\n";
        cw << "      JSValue cv=JS_GetPropertyStr(ctx,val,conds[ci]);\n";
        cw << "      if(!JS_IsUndefined(cv)){\n";
        cw << "        /* Nested condition object */\n";
        cw << "        if(JS_IsObject(cv)&&!JS_IsString(cv)){\n";
        cw << "          resolve_exports_key(key,subpath,cv,md,match,matched_path,path_sz,ctx);\n";
        cw << "        }else if(JS_IsString(cv)){\n";
        cw << "          const char*cs=JS_ToCString(ctx,cv);\n";
        cw << "          if(cs){strip_dots(matched_path,md,cs);*match=1;JS_FreeCString(ctx,cs);}\n";
        cw << "        }\n";
        cw << "      }\n";
        cw << "      JS_FreeValue(ctx,cv);\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "}\n\n";
        cw << "static JSValue js_rust_load(JSContext*ctx,const char*name);\n";
        cw << "static JSValue bridge_npm_install(JSContext*ctx,const char*name);\n";
        cw << "static JSValue js_bridge_npm_install(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_console_log(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_pstdout(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_readFile(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_writeFile(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_exists(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_mkdir(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_readdir(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_stat(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_unlink(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_exec(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_http_get(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "/* Current working directory for module resolution */\n";
        cw << "static char g_current_dir[4096]=\".\";\n\n";

        /* ── ESM→CJS transpiler ── */
        cw << "static char* bridge_esm_to_cjs(const char*s,size_t sl,size_t*ol){\n";
        cw << "  char*d=(char*)malloc(sl*3+8192);if(!d)return NULL;size_t di=0,i=0;\n";
        cw << "  int q1=0,q2=0,mlc=0;char pending[8192];size_t pi=0;\n";
        cw << "  #define P(s) do{const char*_p_=(s);size_t _pl_=strlen(_p_);memcpy(d+di,_p_,_pl_);di+=_pl_;}while(0)\n";
        cw << "  #define PC(s) do{P(s);pi=0;}while(0)\n";
        cw << "  #define FL(s) do{if(pi+(int)strlen(s)+1<(int)sizeof(pending)){memcpy(pending+pi,s,strlen(s));pi+=strlen(s);}}while(0)\n";
        cw << "  pending[0]=0;\n";
        cw << "  while(i<sl){\n";
        cw << "    if(mlc){d[di++]=s[i];if(s[i]=='*'&&i+1<sl&&s[i+1]=='/'){d[di++]=s[++i];mlc=0;}i++;continue;}\n";
        cw << "    if(q1){d[di++]=s[i];if(s[i]=='\\\\'&&i+1<sl)d[di++]=s[++i];else if(s[i]=='\\'')q1=0;i++;continue;}\n";
        cw << "    if(q2){d[di++]=s[i];if(s[i]=='\\\\'&&i+1<sl)d[di++]=s[++i];else if(s[i]=='\"')q2=0;i++;continue;}\n";
        cw << "    if(s[i]=='/'&&i+1<sl){if(s[i+1]=='/'){d[di++]=s[i++];d[di++]=s[i++];\n";
        cw << "      while(i<sl&&s[i]!='\\n')d[di++]=s[i++];continue;}\n";
        cw << "      if(s[i+1]=='*'){d[di++]=s[i++];d[di++]=s[i++];mlc=1;continue;}}\n";
        cw << "    if(s[i]=='\\''){q1=1;d[di++]=s[i++];continue;}\n";
        cw << "    if(s[i]=='\"'){q2=1;d[di++]=s[i++];continue;}\n";
        cw << "    if(s[i]=='`'){d[di++]=s[i++];while(i<sl&&s[i]!='`'){d[di++]=s[i++];}if(i<sl)d[di++]=s[i++];continue;}\n";
        cw << "    /* Detect 'import ' at line start (after whitespace) */\n";
        cw << "    {int ws=i;while(ws<(int)sl&&(s[ws]==' '||s[ws]=='\\t'))ws++;\n";
        cw << "    if(ws+6<(int)sl&&strncmp(s+ws,\"import\",6)==0&&s[ws+6]<=' '){\n";
        cw << "      int si=ws+7;while(si<(int)sl&&s[si]<=' ')si++;\n";
        cw << "      if(si<(int)sl&&s[si]=='\"'){/* import \"x\" */\n";
        cw << "        int ei=si+1;while(ei<(int)sl&&s[ei]!='\"')ei++;\n";
        cw << "        P(\"require(\");for(int j=si;j<=ei;j++)d[di++]=s[j];P(\");\");\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;\n";
        cw << "        continue;\n";
        cw << "      }\n";
        cw << "      /* import X from 'y' or import {X} from 'y' or import * as X from 'y' */\n";
        cw << "      int br=0;char bind[4096];int bi=0;\n";
        cw << "      while(si<(int)sl&&s[si]!=';'&&s[si]!='\\n'&&s[si]!='\\r'){\n";
        cw << "        if(s[si]=='{'||s[si]=='*'){br=1;}\n";
        cw << "        if(bi<(int)sizeof(bind)-1)bind[bi++]=s[si];si++;\n";
        cw << "      }\n";
        cw << "      bind[bi]=0;\n";
        cw << "      /* Find 'from' keyword */\n";
        cw << "      char*fr=strstr(bind,\"from\");if(fr){*fr=0;fr+=4;}\n";
        cw << "      /* Trim bind and from */\n";
        cw << "      int bj=(int)strlen(bind);while(bj>0&&bind[bj-1]<=' ')bind[--bj]=0;\n";
        cw << "      const char*mod=fr?fr:bind;\n";
        cw << "      while(*mod<=' ')mod++;\n";
        cw << "      /* Strip quotes from module */\n";
        cw << "      char modq[4096];int mqi=0;\n";
        cw << "      if(*mod=='\"'||*mod=='\\''){mod++;while(mod[mqi]&&mod[mqi]!='\"'&&mod[mqi]!='\\''){modq[mqi]=mod[mqi];mqi++;}modq[mqi]=0;}else snprintf(modq,sizeof(modq),\"%s\",mod);\n";
        cw << "      /* Check if it's a namespace import: * as X */\n";
        cw << "      char*star=strstr(bind,\"*\");\n";
        cw << "      if(star){char*as=strstr(star+1,\"as\");if(as){as+=2;while(*as<=' ')as++;\n";
        cw << "        P(\"const \");while(*as>' ')d[di++]=*as++;P(\"=require(\\\"\");P(modq);P(\"\\\")\");}\n";
        cw << "      }else if(!br){/* default import: X from 'y' */\n";
        cw << "        P(\"const \");P(bind);P(\"=require(\\\"\");P(modq);P(\"\\\").default||require(\\\"\");P(modq);P(\"\\\")\");\n";
        cw << "      }else{/* named import: {X,Y} from 'y' */\n";
        cw << "        P(\"const \");P(bind);P(\"=require(\\\"\");P(modq);P(\"\\\")\");}\n";
        cw << "      while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n'&&s[i]!='\\r')i++;if(s[i]==';')i++;\n";
        cw << "      continue;\n";
        cw << "    }}\n";
        cw << "    /* Detect 'export ' at line start (after whitespace) */\n";
        cw << "    {int ws=i;while(ws<(int)sl&&(s[ws]==' '||s[ws]=='\\t'))ws++;\n";
        cw << "    if(ws+7<(int)sl&&strncmp(s+ws,\"export \",7)==0){\n";
        cw << "      int si=ws+7;while(si<(int)sl&&s[si]<=' ')si++;\n";
        cw << "      if(strncmp(s+si,\"default\",7)==0){/* export default */\n";
        cw << "        si+=7;while(si<(int)sl&&s[si]<=' ')si++;\n";
        cw << "        int ei=si,dp=1;while(ei<(int)sl){if(s[ei]=='{')dp++;if(s[ei]=='}')dp--;if(s[ei]==';'&&dp==1)break;ei++;}\n";
        cw << "        P(\"module.exports.default=(\");for(int j=si;j<ei;j++)d[di++]=s[j];P(\")\");if(s[ei]==';'){d[di++]=s[ei];}else P(\";\");\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;continue;\n";
        cw << "      }\n";
        cw << "      if(s[si]=='{'){/* export {X,Y} */\n";
        cw << "        int ei=si+1;while(ei<(int)sl&&s[ei]!='}')ei++;\n";
        cw << "        char exps[4096];int eii=0;for(int j=si+1;j<ei;j++)if(s[j]>' ')exps[eii++]=s[j];exps[eii]=0;\n";
        cw << "        char*tk=strtok(exps,\",\");while(tk){\n";
        cw << "          P(\"module.exports.\");P(tk);P(\"=\");P(tk);P(\";\");tk=strtok(NULL,\",\");}\n";
        cw << "        /* Check for 'from' */\n";
        cw << "        char*ff=strstr(s+ei+1,\"from\");\n";
        cw << "        if(ff){while(*ff<=' ')ff++;ff+=4;while(*ff<=' ')ff++;\n";
        cw << "          char modq[256];int mi=0;\n";
        cw << "          if(*ff=='\"'||*ff=='\\''){ff++;while(*ff&&*ff!='\"'&&*ff!='\\'')modq[mi++]=*ff++;}modq[mi]=0;\n";
        cw << "          P(\"Object.assign(module.exports,require(\\\"\");P(modq);P(\"\\\"));\");}\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;continue;\n";
        cw << "      }\n";
        cw << "      if(s[si]=='*'){/* export * from 'y' */\n";
        cw << "        char*ff=strstr(s+si,\"from\");\n";
        cw << "        if(ff){ff+=4;while(*ff<=' ')ff++;\n";
        cw << "          char modq[256];int mi=0;\n";
        cw << "          if(*ff=='\"'||*ff=='\\''){ff++;while(*ff&&*ff!='\"'&&*ff!='\\'')modq[mi++]=*ff++;}modq[mi]=0;\n";
        cw << "          P(\"Object.assign(module.exports,require(\\\"\");P(modq);P(\"\\\"));\");}\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;continue;\n";
        cw << "      }\n";
        cw << "      /* export function/const/class/let/var Name ... */\n";
        cw << "      int sni=si;char name[256];int ni=0;\n";
        cw << "      while(sni<(int)sl&&s[sni]!=' '&&s[sni]!='\\t'&&s[sni]!='(')sni++;\n";
        cw << "      while(sni<(int)sl&&(s[sni]==' '||s[sni]=='\\t'))sni++;\n";
        cw << "      while(sni<(int)sl&&(s[sni]>' '&&s[sni]!='('&&s[sni]!='='&&s[sni]!='{')){if(ni<255)name[ni++]=s[sni];sni++;}name[ni]=0;\n";
        cw << "      P(\"module.exports.\");P(name);P(\"=(\");\n";
        cw << "      for(int j=si;j<(int)sl&&s[j]!=';'&&s[j]!='\\n';j++)d[di++]=s[j];\n";
        cw << "      P(\");\");while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';'){d[di++]=s[i];i++;}continue;\n";
        cw << "    }}\n";
        cw << "    d[di++]=s[i++];\n";
        cw << "  }\n";
        cw << "  d[di]=0;*ol=di;return d;}\n\n";

        /* Module loader */
        cw << "static JSValue load_module(JSContext*ctx,const char*path,const char*name){\n";
        cw << "  FILE*f=fopen(path,\"rb\");if(!f){set_last_error(\"load_module: fopen failed\");return JS_ThrowReferenceError(ctx,\"mod %s\",name);}\n";
        cw << "  fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);\n";
        cw << "  char*code=(char*)malloc((size_t)len+1);\n";
        cw << "  if(!code){fclose(f);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  fread(code,1,len,f);code[len]=0;fclose(f);\n";
        cw << "  JSValue ex=JS_NewObject(ctx),mo=JS_NewObject(ctx);\n";
        cw << "  JS_SetPropertyStr(ctx,mo,\"exports\",JS_DupValue(ctx,ex));\n";
        cw << "  char dir[4096];strncpy(dir,path,sizeof(dir)-1);dir[sizeof(dir)-1]=0;\n";
        cw << "  char*p=strrchr(dir,'/');\n";
#ifdef _WIN32
        cw << "  char*p2=strrchr(dir,'\\\\');if(p2>p)p=p2;\n";
#endif
        cw << "  if(p)*p=0;\n";
        cw << "  /* Prefer pre-transpiled .cjs file (generated by esbuild at bridge-generation time) */\n";
        cw << "  int need_transpile=0;\n";
        cw << "  if(strlen(path)>4&&strcmp(path+strlen(path)-4,\".mjs\")==0){\n";
        cw << "    need_transpile=1;\n";
        cw << "    char cjs_path[4096];\n";
        cw << "    strncpy(cjs_path,path,sizeof(cjs_path)-1);cjs_path[sizeof(cjs_path)-1]=0;\n";
        cw << "    strcpy(cjs_path+strlen(cjs_path)-4,\".cjs\");\n";
        cw << "    FILE*cjs_f=fopen(cjs_path,\"rb\");\n";
        cw << "    if(cjs_f){\n";
        cw << "      fclose(cjs_f);free(code);\n";
        cw << "      cjs_f=fopen(cjs_path,\"rb\");if(!cjs_f){set_last_error(\"load_module: .cjs fopen failed\");JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowReferenceError(ctx,\"mod %s\",name);}\n";
        cw << "      fseek(cjs_f,0,SEEK_END);len=ftell(cjs_f);fseek(cjs_f,0,SEEK_SET);\n";
        cw << "      code=(char*)malloc((size_t)len+1);\n";
        cw << "      if(!code){fclose(cjs_f);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "      fread(code,1,len,cjs_f);code[len]=0;fclose(cjs_f);need_transpile=0;\n";
        cw << "    }\n";
        cw << "  }else if(strlen(path)>3&&strcmp(path+strlen(path)-3,\".js\")==0){\n";
        cw << "    /* Check nearest package.json for \"type\":\"module\" */\n";
        cw << "    char pj_check[4096];strncpy(pj_check,path,sizeof(pj_check)-1);\n";
        cw << "    while(1){\n";
        cw << "      char*ls=strrchr(pj_check,'/');\n";
#ifdef _WIN32
        cw << "      char*bs=strrchr(pj_check,'\\\\');if(bs>ls)ls=bs;\n";
#endif
        cw << "      if(!ls)break;*ls=0;\n";
        cw << "      char pjp[4096];snprintf(pjp,sizeof(pjp),\"%s/package.json\",pj_check);\n";
        cw << "      FILE*pjf=fopen(pjp,\"rb\");\n";
        cw << "      if(pjf){\n";
        cw << "        fseek(pjf,0,SEEK_END);long pjl=ftell(pjf);fseek(pjf,0,SEEK_SET);\n";
        cw << "        char*pj=(char*)malloc((size_t)pjl+1);\n";
        cw << "        if(pj){fread(pj,1,pjl,pjf);pj[pjl]=0;\n";
        cw << "          char*tp=strstr(pj,\"\\\"type\\\"\");\n";
        cw << "          if(tp){tp+=6;while(*tp&&*tp<=' ')tp++;if(*tp==':'){tp++;while(*tp&&*tp<=' ')tp++;\n";
        cw << "            if(*tp=='\"'){tp++;if(strncmp(tp,\"module\",6)==0)need_transpile=1;}}\n";
        cw << "          }\n";
        cw << "          free(pj);\n";
        cw << "        }\n";
        cw << "        fclose(pjf);break;\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  if(need_transpile){\n";
        cw << "    size_t slen=(size_t)len;\n";
        cw << "    char*tjs=bridge_esm_to_cjs(code,slen,&slen);\n";
        cw << "    if(tjs){free(code);code=tjs;len=(long)slen;}else{free(code);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  }\n";
        cw << "  /* Set current dir for module resolution */\n";
        cw << "  char prev_dir[4096];strncpy(prev_dir,g_current_dir,sizeof(prev_dir)-1);\n";
        cw << "  strncpy(g_current_dir,dir,sizeof(g_current_dir)-1);\n";
        cw << "  JSValue g=JS_GetGlobalObject(ctx);\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"exports\",JS_DupValue(ctx,ex));\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"module\",JS_DupValue(ctx,mo));\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"__filename\",JS_NewString(ctx,path));\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"__dirname\",JS_NewString(ctx,dir));\n";
        cw << "  JS_FreeValue(ctx,g);\n";
        cw << "  /* Bytecode cache: check for .qbc file */\n";
        cw << "  char qbc_path[4096];snprintf(qbc_path,sizeof(qbc_path),\"%s.qbc\",path);\n";
        cw << "  FILE*qbc_f=fopen(qbc_path,\"rb\");JSValue r;\n";
        cw << "  if(qbc_f){\n";
        cw << "    fseek(qbc_f,0,SEEK_END);long qbc_len=ftell(qbc_f);fseek(qbc_f,0,SEEK_SET);\n";
        cw << "    uint8_t*qbc_buf=(uint8_t*)malloc((size_t)qbc_len);\n";
        cw << "    if(qbc_buf){fread(qbc_buf,1,qbc_len,qbc_f);r=JS_ReadObject(ctx,qbc_buf,qbc_len,JS_READ_OBJ_BYTECODE);free(qbc_buf);}\n";
        cw << "    else{r=JS_ThrowOutOfMemory(ctx);}\n";
        cw << "    fclose(qbc_f);free(code);g_perf.bc_hits++;\n";
        cw << "  }else{\n";
        cw << "    g_perf.bc_misses++;\n";
        cw << "#ifdef _WIN32\n";
        cw << "    LARGE_INTEGER e0,ef,efreq;QueryPerformanceFrequency(&efreq);QueryPerformanceCounter(&e0);\n";
        cw << "#else\n";
        cw << "    struct timespec e0,ef;clock_gettime(CLOCK_MONOTONIC,&e0);\n";
        cw << "#endif\n";
        cw << "    r=JS_Eval(ctx,code,(size_t)len,path,JS_EVAL_TYPE_GLOBAL);free(code);\n";
        cw << "#ifdef _WIN32\n";
        cw << "    QueryPerformanceCounter(&ef);g_perf.eval_time_ms+=(unsigned)(((ef.QuadPart-e0.QuadPart)*1000)/efreq.QuadPart);\n";
        cw << "#else\n";
        cw << "    clock_gettime(CLOCK_MONOTONIC,&ef);g_perf.eval_time_ms+=(unsigned)(((ef.tv_sec-e0.tv_sec)*1000+(ef.tv_nsec-e0.tv_nsec)/1000000));\n";
        cw << "#endif\n";
        cw << "    if(!JS_IsException(r)){\n";
        cw << "      size_t out_len;\n";
        cw << "      uint8_t*out_buf=JS_WriteObject(ctx,&out_len,r,JS_WRITE_OBJ_BYTECODE);\n";
        cw << "      if(out_buf){\n";
        cw << "        FILE*qbc_w=fopen(qbc_path,\"wb\");\n";
        cw << "        if(qbc_w){fwrite(out_buf,1,out_len,qbc_w);fclose(qbc_w);}\n";
        cw << "        js_free(ctx,out_buf);\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Restore previous dir */\n";
        cw << "  strncpy(g_current_dir,prev_dir,sizeof(g_current_dir)-1);\n";
        cw << "  if(JS_IsException(r)){JS_FreeValue(ctx,r);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_EXCEPTION;}\n";
        cw << "  JS_FreeValue(ctx,r);\n";
        cw << "  JSValue fe=JS_GetPropertyStr(ctx,mo,\"exports\");\n";
        cw << "  JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return fe;}\n\n";

        /* Node module resolution */
        cw << "static char* resolve_mod(const char*name,const char*from){\n";
        cw << "  g_perf.exports_lookups++;\n";
        cw << "  static char buf[4096];char dir[4096];\n";
        cw << "  strncpy(dir,from?from:\".\",sizeof(dir)-1);dir[sizeof(dir)-1]=0;\n";
        cw << "  /* Relative path: try direct .js/.cjs/.mjs, /index.*, and /package.json */\n";
        cw << "  if(name[0]=='.'){\n";
        cw << "    snprintf(buf,sizeof(buf),\"%s/%s\",dir,name);\n";
        cw << "    /* Try exact match first */\n";
        cw << "    FILE*f=fopen(buf,\"rb\");if(f){fclose(f);return buf;}\n";
        cw << "    /* Try .js, .cjs, .mjs */\n";
        cw << "    char t[4096];\n";
        cw << "    const char*exts[]={\".js\",\".cjs\",\".mjs\",NULL};\n";
        cw << "    for(int ei=0;exts[ei];ei++){snprintf(t,sizeof(t),\"%s%s\",buf,exts[ei]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    /* Try index.js, index.mjs, index.cjs */\n";
        cw << "    const char*idx[]={\"index.js\",\"index.mjs\",\"index.cjs\",NULL};\n";
        cw << "    for(int ii=0;idx[ii];ii++){snprintf(t,sizeof(t),\"%s/%s\",buf,idx[ii]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    /* Try package.json */\n";
        cw << "    snprintf(t,sizeof(t),\"%s/package.json\",buf);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}\n";
        cw << "    return NULL;\n";
        cw << "  }\n";
        cw << "  /* Two-phase resolution for nested paths (a/b/c) */\n";
        cw << "  const char*slash_in_name=strchr(name,'/');\n";
        cw << "  if(slash_in_name){\n";
        cw << "    /* Phase 1: extract package name */\n";
        cw << "    size_t pkg_len=(size_t)(slash_in_name-name);\n";
        cw << "    char pkg_name[256];if(pkg_len<sizeof(pkg_name)){\n";
        cw << "      memcpy(pkg_name,name,pkg_len);pkg_name[pkg_len]=0;\n";
        cw << "      const char*subpath=slash_in_name+1;\n";
        cw << "      char pkg_root[4096];char tmp[4096];strncpy(tmp,dir,sizeof(tmp)-1);\n";
        cw << "      while(tmp[0]){\n";
        cw << "        snprintf(pkg_root,sizeof(pkg_root),\"%s/node_modules/%s/package.json\",tmp,pkg_name);\n";
        cw << "        FILE*f=fopen(pkg_root,\"rb\");\n";
        cw << "        if(f){fclose(f);\n";
        cw << "          snprintf(pkg_root,sizeof(pkg_root),\"%s/node_modules/%s\",tmp,pkg_name);\n";
        cw << "          /* Phase 2: resolve subpath relative to package root */\n";
        cw << "          snprintf(buf,sizeof(buf),\"%s/%s\",pkg_root,subpath);\n";
        cw << "          FILE*tf=fopen(buf,\"rb\");if(tf){fclose(tf);return buf;}\n";
        cw << "          /* Try with .js, .cjs, .mjs */\n";
        cw << "          const char*exts2[]={\".js\",\".cjs\",\".mjs\",NULL};\n";
        cw << "          for(int ei=0;exts2[ei];ei++){snprintf(tmp,sizeof(tmp),\"%s/%s%s\",pkg_root,subpath,exts2[ei]);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}}\n";
        cw << "          /* Try /index.js, /index.mjs, /index.cjs */\n";
        cw << "          snprintf(tmp,sizeof(tmp),\"%s/%s/index.js\",pkg_root,subpath);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}\n";
        cw << "          snprintf(tmp,sizeof(tmp),\"%s/%s/index.mjs\",pkg_root,subpath);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}\n";
        cw << "          snprintf(tmp,sizeof(tmp),\"%s/%s/index.cjs\",pkg_root,subpath);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}\n";
        cw << "          /* Not found via two-phase — fall through to flat search */\n";
        cw << "          break;\n";
        cw << "        }\n";
        cw << "        char*p=strrchr(tmp,'/');\n";
#ifdef _WIN32
        cw << "        char*p2=strrchr(tmp,'\\\\');if(p2>p)p=p2;\n";
#endif
        cw << "        if(!p||p==tmp)break;*p=0;\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Flat search (original algorithm) */\n";
        cw << "  while(1){\n";
        cw << "    snprintf(buf,sizeof(buf),\"%s/node_modules/%s/package.json\",dir,name);\n";
        cw << "    FILE*f=fopen(buf,\"rb\");if(f){fclose(f);snprintf(buf,sizeof(buf),\"%s/node_modules/%s\",dir,name);return buf;}\n";
        cw << "    const char*exts[]={\".js\",\".cjs\",\".mjs\",NULL};\n";
        cw << "    char t[4096];\n";
        cw << "    for(int ei=0;exts[ei];ei++){snprintf(t,sizeof(t),\"%s/node_modules/%s%s\",dir,name,exts[ei]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    const char*idx2[]={\"index.js\",\"index.mjs\",\"index.cjs\",NULL};\n";
        cw << "    for(int ii=0;idx2[ii];ii++){snprintf(t,sizeof(t),\"%s/node_modules/%s/%s\",dir,name,idx2[ii]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    char*p=strrchr(dir,'/');\n";
#ifdef _WIN32
        cw << "    char*p2=strrchr(dir,'\\\\');if(p2>p)p=p2;\n";
#endif
        cw << "    if(!p||p==dir)break;*p=0;}\n";
        cw << "  return NULL;}\n\n";

        /* js_require implementation */
        /* require.cache in C (simple array of {name, exports}) */
        cw << "#define MAX_CACHED 256\n";
        cw << "static const char* g_cache_names[MAX_CACHED];\n";
        cw << "static JSValue g_cache_exports[MAX_CACHED];\n";
        cw << "static int g_cache_count=0;\n\n";

        cw << "static JSValue js_require(JSContext*ctx,JSValueConst t,int argc,JSValueConst*argv){\n";
        cw << "  g_perf.require_calls++;\n";
        cw << "  if(argc<1)return JS_ThrowTypeError(ctx,\"require:missing arg\");\n";
        cw << "  const char*name=JS_ToCString(ctx,argv[0]);if(!name)return JS_ThrowTypeError(ctx,\"bad arg\");\n";
        cw << "  /* Check require.cache first */\n";
        cw << "  for(int i=0;i<g_cache_count;i++){\n";
        cw << "    if(strcmp(g_cache_names[i],name)==0){\n";
        cw << "      JS_FreeCString(ctx,name);return JS_DupValue(ctx,g_cache_exports[i]);\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Check Node.js builtins first */\n";
        cw << "  JSValue glob=JS_GetGlobalObject(ctx);\n";
        cw << "  JSValue bm=JS_GetPropertyStr(ctx,glob,\"__node_builtins\");\n";
        cw << "  if(!JS_IsUndefined(bm)){\n";
        cw << "    JSValue bmod=JS_GetPropertyStr(ctx,bm,name);\n";
        cw << "    if(!JS_IsUndefined(bmod)){\n";
        cw << "      JS_FreeCString(ctx,name);JS_FreeValue(ctx,bm);JS_FreeValue(ctx,glob);\n";
        cw << "      return bmod;\n";
        cw << "    }\n";
        cw << "    JS_FreeValue(ctx,bmod);\n";
        cw << "  }\n";
        cw << "  JS_FreeValue(ctx,bm);JS_FreeValue(ctx,glob);\n";
        cw << "  char*md=resolve_mod(name,g_current_dir);\n";
        cw << "  /* If not found on disk, auto-install npm or load rust crate */\n";
        cw << "  if(!md){\n";
        cw << "    if(strncmp(name,\"rust:\",5)==0){\n";
        cw << "      JSValue rmod=js_rust_load(ctx,name+5);\n";
        cw << "      JS_FreeCString(ctx,name);\n";
        cw << "      if(!JS_IsException(rmod)&&g_cache_count<MAX_CACHED){\n";
        cw << "        char* dn=strdup(name);if(dn){g_cache_names[g_cache_count]=dn;g_cache_exports[g_cache_count]=JS_DupValue(ctx,rmod);g_cache_count++;}\n";
        cw << "      }\n";
        cw << "      return rmod;\n";
        cw << "    }\n";
        cw << "    /* Auto-install missing npm package */\n";
        cw << "    JSValue bni=bridge_npm_install(ctx,name);\n";
        cw << "    if(JS_IsException(bni)||(JS_IsBool(bni)&&!JS_VALUE_GET_BOOL(bni))){\n";
        cw << "      JS_FreeValue(ctx,bni);\n";
        cw << "      JS_FreeCString(ctx,name);\n";
        cw << "      return JS_ThrowReferenceError(ctx,\"module %s not found and auto-install failed\",name);\n";
        cw << "    }\n";
        cw << "    md=resolve_mod(name,g_current_dir);\n";
        cw << "    if(!md){JS_FreeCString(ctx,name);return JS_ThrowReferenceError(ctx,\"module %s installed but not resolved\",name);}\n";
        cw << "  }\n";
        cw << "  char entry[4096],pj[4096];snprintf(pj,sizeof(pj),\"%s/package.json\",md);\n";
        cw << "  FILE*f=fopen(pj,\"rb\");\n";
        cw << "  if(f){\n";
        cw << "    fseek(f,0,SEEK_END);long jl=ftell(f);fseek(f,0,SEEK_SET);\n";
        cw << "    char*js=(char*)malloc((size_t)jl+1);if(!js){fclose(f);JS_FreeCString(ctx,name);return JS_ThrowReferenceError(ctx,\"OOM reading package.json\");}fread(js,1,jl,f);js[jl]=0;fclose(f);\n";
        cw << "    JSValue po=JS_ParseJSON(ctx,js,(size_t)jl,\"<pkg>\");free(js);\n";
        cw << "    if(!JS_IsException(po)){\n";
        cw << "      int resolved=0;\n";
        cw << "      /* Try 'exports' map before 'main' */\n";
        cw << "      JSValue ev=JS_GetPropertyStr(ctx,po,\"exports\");\n";
        cw << "      if(JS_IsString(ev)){\n";
        cw << "        const char*es=JS_ToCString(ctx,ev);\n";
        cw << "        if(es){strip_dots(entry,md,es);resolved=1;JS_FreeCString(ctx,es);}\n";
        cw << "        JS_FreeValue(ctx,ev);\n";
        cw << "      }else if(JS_IsObject(ev)){\n";
        cw << "        /* Determine the subpath requested */\n";
        cw << "        const char*subpath=\".\";\n";
        cw << "        char subbuf[4096];\n";
        cw << "        const char*slash=strchr(name,'/');\n";
        cw << "        if(slash&&slash>name){snprintf(subbuf,sizeof(subbuf),\"./%s\",slash+1);subpath=subbuf;}\n";
        cw << "        char best_match[4096];best_match[0]=0;\n";
        cw << "        /* Iterate exports keys using JS_GetOwnPropertyNames */\n";
        cw << "        {\n";
        cw << "          JSPropertyEnum*ptab=NULL;uint32_t plen=0;\n";
        cw << "          if(JS_GetOwnPropertyNames(ctx,&ptab,&plen,ev,JS_GPN_STRING_MASK)==0){\n";
        cw << "            for(uint32_t ki=0;ki<plen;ki++){\n";
        cw << "              const char*key=JS_AtomToCString(ctx,ptab[ki].atom);\n";
        cw << "              if(key){\n";
        cw << "                JSValue val=JS_GetPropertyStr(ctx,ev,key);\n";
        cw << "                int match=0;char matched_path[4096]=\"\";\n";
        cw << "                resolve_exports_key(key,subpath,val,md,&match,matched_path,sizeof(matched_path),ctx);\n";
        cw << "                if(match&&(!best_match[0]||strlen(key)>strlen(best_match))){\n";
        cw << "                  strncpy(best_match,key,sizeof(best_match)-1);\n";
        cw << "                  strncpy(entry,matched_path,sizeof(entry)-1);resolved=1;\n";
        cw << "                }\n";
        cw << "                JS_FreeValue(ctx,val);JS_FreeCString(ctx,key);\n";
        cw << "              }\n";
        cw << "            }\n";
        cw << "            JS_FreePropertyEnum(ctx,ptab,plen);\n";
        cw << "          }\n";
        cw << "        }\n";
        cw << "        JS_FreeValue(ctx,ev);\n";
        cw << "      }else JS_FreeValue(ctx,ev);\n";
        cw << "      if(!resolved){\n";
        cw << "        JSValue mv=JS_GetPropertyStr(ctx,po,\"main\");\n";
        cw << "        if(!JS_IsUndefined(mv)){\n";
        cw << "          const char*ms=JS_ToCString(ctx,mv);\n";
        cw << "          if(ms){strip_dots(entry,md,ms);JS_FreeCString(ctx,ms);}\n";
        cw << "          else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "          JS_FreeValue(ctx,mv);\n";
        cw << "        }else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "      }\n";
        cw << "      JS_FreeValue(ctx,po);\n";
        cw << "    }else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "  }else{\n";
        cw << "    if(strstr(md,\".js\"))snprintf(entry,sizeof(entry),\"%s\",md);\n";
        cw << "    else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "  }\n";
        cw << "  /* Pre-register in cache BEFORE eval to support circular dependencies */\n";
        cw << "  int cache_slot=-1;\n";
        cw << "  for(int i=0;i<g_cache_count;i++){if(strcmp(g_cache_names[i],name)==0){cache_slot=i;break;}}\n";
        cw << "  if(cache_slot<0&&g_cache_count<MAX_CACHED){\n";
        cw << "    char* dn=strdup(name);if(!dn){JS_FreeCString(ctx,name);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "    JSValue ce=JS_NewObject(ctx);if(JS_IsException(ce)){free(dn);JS_FreeCString(ctx,name);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "    cache_slot=g_cache_count++;\n";
        cw << "    g_cache_names[cache_slot]=dn;\n";
        cw << "    g_cache_exports[cache_slot]=ce;\n";
        cw << "  }\n";
        cw << "  JSValue ret=load_module(ctx,entry,name);\n";
        cw << "  if(!JS_IsException(ret)&&cache_slot>=0){\n";
        cw << "    JS_FreeValue(ctx,g_cache_exports[cache_slot]);\n";
        cw << "    g_cache_exports[cache_slot]=JS_DupValue(ctx,ret);\n";
        cw << "  }\n";
        cw << "  JS_FreeCString(ctx,name);return ret;}\n\n";

        /* ── Forward declarations used by init_qjs ── */
        cw << "static JSValue js_process_cwd(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_process_exit(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_process_nextTick(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_process_env(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_crypto_hash(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_crypto_random(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_connect(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_write(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_read(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_close(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_connect(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_write(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_read(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_close(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_dns_lookup(JSContext*,JSValueConst,int,JSValueConst*);\n";

        /* QuickJS init */
        cw << "static int init_qjs(const char*dir){\n";
        cw << "  if(g_inited)return 1;g_rt=JS_NewRuntime();if(!g_rt){set_last_error(\"init: JS_NewRuntime failed\");return 0;}\n";
        cw << "  g_ctx=JS_NewContext(g_rt);if(!g_ctx){JS_FreeRuntime(g_rt);g_rt=NULL;return 0;}\n";
        cw << "  if(dir){\n";
#ifdef _WIN32
        cw << "    char dll_name[256];snprintf(dll_name,sizeof(dll_name),\"%s.dll\",dir);\n";
        cw << "    HMODULE hm=GetModuleHandleA(dll_name);\n";
        cw << "    if(hm){char dll_path[1024];GetModuleFileNameA(hm,dll_path,sizeof(dll_path));\n";
        cw << "      char*last=strrchr(dll_path,'\\\\');if(last)*last='\\0';\n";
        cw << "      SetCurrentDirectoryA(dll_path);}\n";
#else
        cw << "    /* POSIX: resolve dir relative to /proc/self/exe or dladdr */\n";
        cw << "    chdir(dir);\n";
#endif
        cw << "  }\n";
        cw << "  JSValue g=JS_GetGlobalObject(g_ctx);\n";
        cw << "  JSValue c=JS_NewObject(g_ctx);\n";
        cw << "  JS_SetPropertyStr(g_ctx,c,\"log\",JS_NewCFunction(g_ctx,js_console_log,\"log\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,c,\"warn\",JS_NewCFunction(g_ctx,js_console_log,\"warn\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,c,\"error\",JS_NewCFunction(g_ctx,js_console_log,\"error\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"console\",c);\n";
        cw << "  JSValue p=JS_NewObject(g_ctx);JSValue so=JS_NewObject(g_ctx);\n";
        cw << "  JS_SetPropertyStr(g_ctx,so,\"write\",JS_NewCFunction(g_ctx,js_pstdout,\"write\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"stdout\",so);\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"cwd\",JS_NewCFunction(g_ctx,js_process_cwd,\"cwd\",0));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"exit\",JS_NewCFunction(g_ctx,js_process_exit,\"exit\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"nextTick\",JS_NewCFunction(g_ctx,js_process_nextTick,\"nextTick\",1));\n";
        cw << "  {JSValue ev=js_process_env(g_ctx,JS_UNDEFINED,0,NULL);JS_SetPropertyStr(g_ctx,p,\"env\",ev);}\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"platform\",JS_NewString(g_ctx,\"win32\"));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"arch\",JS_NewString(g_ctx,\"x64\"));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"version\",JS_NewString(g_ctx,\"v18.17.0\"));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"process\",p);\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"require\",JS_NewCFunction(g_ctx,js_require,\"require\",1));\n";
        cw << "  /* Register bridge I/O helpers */\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_readFile\",JS_NewCFunction(g_ctx,bridge_fs_readFile,\"__bridge_fs_readFile\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_writeFile\",JS_NewCFunction(g_ctx,bridge_fs_writeFile,\"__bridge_fs_writeFile\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_exists\",JS_NewCFunction(g_ctx,bridge_fs_exists,\"__bridge_fs_exists\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_mkdir\",JS_NewCFunction(g_ctx,bridge_fs_mkdir,\"__bridge_fs_mkdir\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_readdir\",JS_NewCFunction(g_ctx,bridge_fs_readdir,\"__bridge_fs_readdir\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_stat\",JS_NewCFunction(g_ctx,bridge_fs_stat,\"__bridge_fs_stat\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_unlink\",JS_NewCFunction(g_ctx,bridge_fs_unlink,\"__bridge_fs_unlink\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_exec\",JS_NewCFunction(g_ctx,bridge_exec,\"__bridge_exec\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_http_get\",JS_NewCFunction(g_ctx,bridge_http_get,\"__bridge_http_get\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_npm_install\",JS_NewCFunction(g_ctx,js_bridge_npm_install,\"__bridge_npm_install\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_crypto_hash\",JS_NewCFunction(g_ctx,bridge_crypto_hash,\"__bridge_crypto_hash\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_crypto_random\",JS_NewCFunction(g_ctx,bridge_crypto_random,\"__bridge_crypto_random\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_connect\",JS_NewCFunction(g_ctx,bridge_net_connect,\"__bridge_net_connect\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_write\",JS_NewCFunction(g_ctx,bridge_net_write,\"__bridge_net_write\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_read\",JS_NewCFunction(g_ctx,bridge_net_read,\"__bridge_net_read\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_close\",JS_NewCFunction(g_ctx,bridge_net_close,\"__bridge_net_close\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_connect\",JS_NewCFunction(g_ctx,bridge_tls_connect,\"__bridge_tls_connect\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_write\",JS_NewCFunction(g_ctx,bridge_tls_write,\"__bridge_tls_write\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_read\",JS_NewCFunction(g_ctx,bridge_tls_read,\"__bridge_tls_read\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_close\",JS_NewCFunction(g_ctx,bridge_tls_close,\"__bridge_tls_close\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_dns_lookup\",JS_NewCFunction(g_ctx,bridge_dns_lookup,\"__bridge_dns_lookup\",1));\n";
        cw << "  JS_FreeValue(g_ctx,g);\n";
        cw << "  /* Evaluate Node.js builtin polyfills */\n";
        cw << "  {JSValue _ev=JS_Eval(g_ctx,node_builtins_js,strlen(node_builtins_js),\"<builtins>\",JS_EVAL_TYPE_GLOBAL);JS_FreeValue(g_ctx,_ev);}\n";
        cw << "  g_inited=1;return 1;}\n\n";

        /* console.log & process.stdout.write */
        cw << "static JSValue js_console_log(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  for(int i=0;i<ac;i++){if(i)fputc(' ',stderr);const char*s=JS_ToCString(ctx,av[i]);if(s){fputs(s,stderr);JS_FreeCString(ctx,s);}}\n";
        cw << "  fputc('\\n',stderr);return JS_UNDEFINED;}\n";
        cw << "static JSValue js_pstdout(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac>0){const char*s=JS_ToCString(ctx,av[0]);if(s){printf(\"%s\",s);fflush(stdout);JS_FreeCString(ctx,s);}}\n";
        cw << "  return JS_UNDEFINED;}\n\n";

        /* ── process polyfills ── */
        cw << "static JSValue js_process_cwd(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
#ifdef _WIN32
        cw << "  char buf[4096];GetCurrentDirectoryA(sizeof(buf),buf);return JS_NewString(ctx,buf);}\n";
#else
        cw << "  char buf[4096];return getcwd(buf,sizeof(buf))?JS_NewString(ctx,buf):JS_NULL;}\n";
#endif
        cw << "static JSValue js_process_exit(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  int code=0;if(ac>0)JS_ToInt32(ctx,&code,av[0]);exit(code);return JS_UNDEFINED;}\n";
        cw << "static JSValue js_process_nextTick(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1||!JS_IsFunction(ctx,av[0]))return JS_ThrowTypeError(ctx,\"nextTick:need fn\");\n";
        cw << "  JSValue r=JS_Call(ctx,av[0],JS_UNDEFINED,ac-1,ac>1?av+1:NULL);\n";
        cw << "  return JS_IsException(r)?r:JS_UNDEFINED;}\n";
        cw << "static JSValue js_process_env(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  JSValue o=JS_NewObject(ctx);if(JS_IsException(o))return o;\n";
#ifdef _WIN32
        cw << "  wchar_t*ew=GetEnvironmentStringsW();if(ew){wchar_t*ep=ew;\n";
        cw << "    while(*ep){size_t el=wcslen(ep);char*mb=(char*)malloc(el*4+1);if(mb){WideCharToMultiByte(CP_UTF8,0,ep,-1,mb,(int)(el*4),NULL,NULL);\n";
        cw << "      char*eq=strchr(mb,'=');if(eq){*eq=0;JS_SetPropertyStr(ctx,o,mb,JS_NewString(ctx,eq+1));}free(mb);}ep+=el+1;}\n";
        cw << "    FreeEnvironmentStringsW(ew);}\n";
#else
        cw << "  extern char**environ;if(environ){for(char**e=environ;*e;e++){char*cp=strchr(*e,'=');\n";
        cw << "    if(cp){*cp=0;JS_SetPropertyStr(ctx,o,*e,JS_NewString(ctx,cp+1));*cp='=';}}}\n";
#endif
        cw << "  return o;}\n\n";

        /* ── Bridge I/O helpers (used by Node.js polyfills) ── */
        /* bridge_fs_readFile */
        cw << "static JSValue bridge_fs_readFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"readFile:missing path\");\n";
        cw << "  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,\"bad path\");\n";
        cw << "  FILE*f=fopen(path,\"rb\");if(!f){set_last_error(\"readFile: fopen failed\");JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);\n";
        cw << "  char*buf=(char*)malloc((size_t)sz+1);if(!buf){fclose(f);JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  fread(buf,1,sz,f);buf[sz]=0;fclose(f);\n";
        cw << "  JSValue r=JS_NewStringLen(ctx,buf,(size_t)sz);free(buf);JS_FreeCString(ctx,path);return r;}\n\n";

        /* bridge_fs_writeFile */
        cw << "static JSValue bridge_fs_writeFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"writeFile:missing args\");\n";
        cw << "  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,\"bad path\");\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,path);return JS_ThrowTypeError(ctx,\"bad data\");}\n";
        cw << "  FILE*f=fopen(path,\"wb\");if(!f){set_last_error(\"writeFile: fopen failed\");JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_FALSE;}\n";
        cw << "  fwrite(data,1,strlen(data),f);fclose(f);\n";
        cw << "  JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_TRUE;}\n\n";

        /* bridge_fs_exists */
        cw << "static JSValue bridge_fs_exists(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;\n";
        cw << "  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  FILE*f=fopen(path,\"rb\");int ok=(f!=NULL);if(f)fclose(f);\n";
        cw << "  JS_FreeCString(ctx,path);return ok?JS_TRUE:JS_FALSE;}\n\n";

        /* bridge_fs_mkdir */
        cw << "static JSValue bridge_fs_mkdir(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
#ifdef _WIN32
        cw << "  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  int r=_mkdir(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}\n\n";
#else
        cw << "  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  int r=mkdir(path,0755);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}\n\n";
#endif

        /* bridge_fs_readdir */
        cw << "static JSValue bridge_fs_readdir(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
#ifdef _WIN32
        cw << "  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;\n";
        cw << "  char pat[4096];snprintf(pat,sizeof(pat),\"%s/*\",path);WIN32_FIND_DATAA fd;\n";
        cw << "  HANDLE h=FindFirstFileA(pat,&fd);if(h==INVALID_HANDLE_VALUE){JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  JSValue arr=JS_NewArray(ctx);if(JS_IsException(arr)){FindClose(h);JS_FreeCString(ctx,path);return JS_EXCEPTION;}int idx=0;\n";
        cw << "  do{JSValue nm=JS_NewString(ctx,fd.cFileName);JS_SetPropertyUint32(ctx,arr,idx++,nm);}while(FindNextFileA(h,&fd));\n";
        cw << "  FindClose(h);JS_FreeCString(ctx,path);return arr;}\n\n";
#else
        cw << "  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;\n";
        cw << "  DIR*d=opendir(path);if(!d){JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  JSValue arr=JS_NewArray(ctx);if(JS_IsException(arr)){closedir(d);JS_FreeCString(ctx,path);return JS_EXCEPTION;}int idx=0;struct dirent*e;\n";
        cw << "  while((e=readdir(d))!=NULL){JSValue nm=JS_NewString(ctx,e->d_name);JS_SetPropertyUint32(ctx,arr,idx++,nm);}\n";
        cw << "  closedir(d);JS_FreeCString(ctx,path);return arr;}\n\n";
#endif

        /* bridge_fs_stat */
        cw << "static JSValue bridge_fs_stat(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;\n";
        cw << "  struct stat st;if(stat(path,&st)!=0){JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  JSValue o=JS_NewObject(ctx);if(JS_IsException(o)){JS_FreeCString(ctx,path);return JS_EXCEPTION;}\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"size\",JS_NewInt64(ctx,st.st_size));\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"mode\",JS_NewInt64(ctx,st.st_mode));\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"isFile\",JS_NewBool(ctx,S_ISREG(st.st_mode)));\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"isDirectory\",JS_NewBool(ctx,S_ISDIR(st.st_mode)));\n";
        cw << "  JS_FreeCString(ctx,path);return o;}\n\n";

        /* bridge_fs_unlink */
        cw << "static JSValue bridge_fs_unlink(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  int r=remove(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}\n\n";

#ifdef _WIN32
        /* bridge_crypto_hash — Windows CNG real SHA256/SHA1/MD5 + CryptGenRandom */
        cw << "#include <bcrypt.h>\n";
        cw << "#pragma comment(lib,\"bcrypt.lib\")\n";
        cw << "#pragma comment(lib,\"crypt32.lib\")\n";
        cw << "static JSValue bridge_crypto_hash(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"crypto_hash:need algo+data\");\n";
        cw << "  const char*algo=JS_ToCString(ctx,av[0]);if(!algo)return JS_ThrowTypeError(ctx,\"bad algo\");\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,algo);return JS_ThrowTypeError(ctx,\"bad data\");}\n";
        cw << "  LPCWSTR walgo=NULL;ULONG hash_len=0;\n";
        cw << "  if(strcmp(algo,\"sha256\")==0){walgo=BCRYPT_SHA256_ALGORITHM;hash_len=32;}\n";
        cw << "  else if(strcmp(algo,\"sha1\")==0){walgo=BCRYPT_SHA1_ALGORITHM;hash_len=20;}\n";
        cw << "  else if(strcmp(algo,\"md5\")==0){walgo=BCRYPT_MD5_ALGORITHM;hash_len=16;}\n";
        cw << "  else{JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"unsupported algo\");}\n";
        cw << "  BCRYPT_ALG_HANDLE hAlg=NULL;\n";
        cw << "  if(BCryptOpenAlgorithmProvider(&hAlg,walgo,NULL,0)!=0||!hAlg){\n";
        cw << "    JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"CNG init failed\");}\n";
        cw << "  BCRYPT_HASH_HANDLE hHash=NULL;\n";
        cw << "  if(BCryptCreateHash(hAlg,&hHash,NULL,0,NULL,0,0)!=0||!hHash){\n";
        cw << "    BCryptCloseAlgorithmProvider(hAlg,0);JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"CNG hash failed\");}\n";
        cw << "  BCryptHashData(hHash,(PUCHAR)data,(ULONG)strlen(data),0);\n";
        cw << "  UCHAR hash[64];\n";
        cw << "  BCryptFinishHash(hHash,hash,hash_len,0);\n";
        cw << "  BCryptDestroyHash(hHash);BCryptCloseAlgorithmProvider(hAlg,0);\n";
        cw << "  char hex[129];for(ULONG i=0;i<hash_len;i++)snprintf(hex+i*2,3,\"%02x\",hash[i]);\n";
        cw << "  JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);\n";
        cw << "  return JS_NewString(ctx,hex);}\n\n";

        /* bridge_crypto_random — CryptGenRandom */
        cw << "#include <wincrypt.h>\n";
        cw << "static JSValue bridge_crypto_random(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"random:need size\");\n";
        cw << "  int32_t sz=0;JS_ToInt32(ctx,&sz,av[0]);if(sz<1||sz>65536)sz=32;\n";
        cw << "  HCRYPTPROV prov=0;\n";
        cw << "  if(!CryptAcquireContextA(&prov,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)){\n";
        cw << "    return JS_ThrowTypeError(ctx,\"CSP failed\");}\n";
        cw << "  BYTE*buf=(BYTE*)malloc((size_t)sz);if(!buf){CryptReleaseContext(prov,0);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  CryptGenRandom(prov,sz,buf);CryptReleaseContext(prov,0);\n";
        cw << "  /* Return as hex string */\n";
        cw << "  char*hex=(char*)malloc((size_t)sz*2+1);if(!hex){free(buf);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  for(int32_t i=0;i<sz;i++)snprintf(hex+i*2,3,\"%02x\",buf[i]);\n";
        cw << "  free(buf);JSValue r=JS_NewString(ctx,hex);free(hex);return r;}\n\n";
#else
        cw << "/* No crypto on non-Windows yet */\n";
        cw << "static JSValue bridge_crypto_hash(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return JS_ThrowTypeError(ctx,\"crypto not available on this platform\");}\n";
        cw << "static JSValue bridge_crypto_random(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return JS_ThrowTypeError(ctx,\"crypto not available on this platform\");}\n\n";
#endif

        /* bridge_exec (child_process.execSync) */
        cw << "static JSValue bridge_exec(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_NULL;const char*cmd=JS_ToCString(ctx,av[0]);if(!cmd)return JS_NULL;\n";
#ifdef _WIN32
        cw << "  FILE*pipe=_popen(cmd,\"r\");\n";
#else
        cw << "  FILE*pipe=popen(cmd,\"r\");\n";
#endif
        cw << "  if(!pipe){set_last_error(\"exec: popen failed\");JS_FreeCString(ctx,cmd);return JS_NULL;}\n";
        cw << "  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out[0]=0;\n";
        cw << "  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}\n";
#ifdef _WIN32
        cw << "  _pclose(pipe);\n";
#else
        cw << "  pclose(pipe);\n";
#endif
        cw << "  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,cmd);return r;}\n\n";

        /* bridge_http_get (basic HTTP GET via system shell) */
        cw << "static JSValue bridge_http_get(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_NULL;const char*url=JS_ToCString(ctx,av[0]);if(!url)return JS_NULL;\n";
        cw << "  char tmpfile[256]=\"\";const char* tmpdir=getenv(\"TMPDIR\");if(!tmpdir)tmpdir=\"/tmp\";\n";
        cw << "  snprintf(tmpfile,sizeof(tmpfile),\"%s/qjs_http_XXXXXX\",tmpdir);\n";
#ifdef _WIN32
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"powershell -Command \\\"try{$r=Invoke-WebRequest -Uri '%s' -UseBasicParsing;if($r.Content){$r.Content}else{'{}'}}catch{'{}'}\\\" 2>nul\",url);\n";
        cw << "  FILE*pipe=_popen(cmd,\"r\");\n";
#else
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"curl -sL '%s' 2>/dev/null\",url);\n";
        cw << "  FILE*pipe=popen(cmd,\"r\");\n";
#endif
        cw << "  if(!pipe){set_last_error(\"http_get: popen failed\");JS_FreeCString(ctx,url);return JS_NULL;}\n";
        cw << "  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out[0]=0;\n";
        cw << "  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}\n";
#ifdef _WIN32
        cw << "  _pclose(pipe);\n";
#else
        cw << "  pclose(pipe);\n";
#endif
        cw << "  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,url);return r;}\n\n";

        cw << "/* ── net helpers (WinSock2 via dynamic loading of ws2_32.dll) ── */\n";
        cw << "#include <winsock2.h>\n#include <ws2tcpip.h>\n";
        cw << "static int (__stdcall *pWSAStartup)(WORD,LPWSADATA);\n";
        cw << "static int (__stdcall *psocket)(int,int,int);\n";
        cw << "static int (__stdcall *pconnect)(int,const struct sockaddr*,int);\n";
        cw << "static int (__stdcall *psend)(int,const char*,int,int);\n";
        cw << "static int (__stdcall *precv)(int,char*,int,int);\n";
        cw << "static int (__stdcall *pclosesocket)(int);\n";
        cw << "static int (__stdcall *pgetaddrinfo)(const char*,const char*,const struct addrinfo*,struct addrinfo**);\n";
        cw << "static void(__stdcall *pfreeaddrinfo)(struct addrinfo*);\n";
        cw << "static const char*(__stdcall *pinet_ntop)(int,const void*,char*,socklen_t);\n";
        cw << "static int net_init(void){\n";
        cw << "  static int n=0;if(n)return n;\n";
        cw << "  HMODULE h=LoadLibraryA(\"ws2_32.dll\");if(!h){n=-1;return n;}\n";
        cw << "  pWSAStartup=(void*)GetProcAddress(h,\"WSAStartup\");\n";
        cw << "  psocket=(void*)GetProcAddress(h,\"socket\");\n";
        cw << "  pconnect=(void*)GetProcAddress(h,\"connect\");\n";
        cw << "  psend=(void*)GetProcAddress(h,\"send\");\n";
        cw << "  precv=(void*)GetProcAddress(h,\"recv\");\n";
        cw << "  pclosesocket=(void*)GetProcAddress(h,\"closesocket\");\n";
        cw << "  pgetaddrinfo=(void*)GetProcAddress(h,\"getaddrinfo\");\n";
        cw << "  pfreeaddrinfo=(void*)GetProcAddress(h,\"freeaddrinfo\");\n";
        cw << "  pinet_ntop=(void*)GetProcAddress(h,\"inet_ntop\");\n";
        cw << "  if(!pWSAStartup||!psocket||!pconnect||!psend||!precv||!pclosesocket||!pgetaddrinfo||!pfreeaddrinfo||!pinet_ntop){n=-1;return n;}\n";
        cw << "  WSADATA wd;n=(pWSAStartup(MAKEWORD(2,2),&wd)==0)?1:-1;return n;}\n";
        cw << "static JSValue bridge_net_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"net_connect:need host port\");\n";
        cw << "  if(net_init()<0)return JS_ThrowTypeError(ctx,\"WSAStartup failed\");\n";
        cw << "  const char*host=JS_ToCString(ctx,av[0]);if(!host)return JS_ThrowTypeError(ctx,\"bad host\");\n";
        cw << "  int32_t port=0;JS_ToInt32(ctx,&port,av[1]);if(port<1||port>65535){JS_FreeCString(ctx,host);return JS_ThrowRangeError(ctx,\"bad port\");}\n";
        cw << "  struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;\n";
        cw << "  char port_str[16];snprintf(port_str,sizeof(port_str),\"%d\",(int)port);\n";
        cw << "  int gai=pgetaddrinfo(host,port_str,&hints,&res);JS_FreeCString(ctx,host);\n";
        cw << "  if(gai!=0||!res){if(res)pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,\"getaddrinfo failed\");}\n";
        cw << "  int fd=(int)psocket(res->ai_family,res->ai_socktype,res->ai_protocol);\n";
        cw << "  if(fd<0){pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,\"socket failed\");}\n";
        cw << "  if(pconnect(fd,res->ai_addr,(int)res->ai_addrlen)<0){\n";
        cw << "    pclosesocket(fd);pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,\"connect failed\");}\n";
        cw << "  pfreeaddrinfo(res);return JS_NewInt32(ctx,fd);}\n\n";
        cw << "static JSValue bridge_net_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"net_write:need fd data\");\n";
        cw << "  int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data)return JS_ThrowTypeError(ctx,\"bad data\");\n";
        cw << "  size_t len=strlen(data);\n";
        cw << "  int n=(int)psend(fd,data,(int)len,0);JS_FreeCString(ctx,data);\n";
        cw << "  if(n<0)return JS_ThrowTypeError(ctx,\"send failed\");\n";
        cw << "  return JS_NewInt32(ctx,n);}\n\n";
        cw << "static JSValue bridge_net_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"net_read:need fd\");\n";
        cw << "  int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);\n";
        cw << "  char buf[4096];\n";
        cw << "  int n=(int)precv(fd,buf,(int)sizeof(buf)-1,0);\n";
        cw << "  if(n<=0){if(n==0)return JS_NULL;return JS_ThrowTypeError(ctx,\"recv failed\");}\n";
        cw << "  buf[n]=0;return JS_NewString(ctx,buf);}\n\n";
        cw << "static JSValue bridge_net_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);\n";
        cw << "  pclosesocket(fd);\n";
        cw << "  return JS_TRUE;}\n\n";

        /* ── TLS helper (SChannel on Win32, stub on POSIX) ── */
#ifdef _WIN32
        cw << "#define MAX_TLS_SESS 32\n";
        cw << "/* SSPI function typedefs */\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pAcquireCredentialsHandleA_t)(SEC_CHAR*,SEC_CHAR*,ULONG,void*,void*,void*,void*,CredHandle*,TimeStamp*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pInitializeSecurityContextA_t)(CredHandle*,CtxtHandle*,SEC_CHAR*,ULONG,ULONG,ULONG,SecBufferDesc*,ULONG,CtxtHandle*,SecBufferDesc*,ULONG*,TimeStamp*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pDeleteSecurityContext_t)(CtxtHandle*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pFreeCredentialsHandle_t)(CredHandle*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pEncryptMessage_t)(CtxtHandle*,ULONG,SecBufferDesc*,ULONG);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pDecryptMessage_t)(CtxtHandle*,SecBufferDesc*,ULONG,void*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pQueryContextAttributesA_t)(CtxtHandle*,ULONG,void*);\n";
        cw << "static pAcquireCredentialsHandleA_t pAcquireCredentialsHandleA=NULL;\n";
        cw << "static pInitializeSecurityContextA_t pInitializeSecurityContextA=NULL;\n";
        cw << "static pDeleteSecurityContext_t pDeleteSecurityContext=NULL;\n";
        cw << "static pFreeCredentialsHandle_t pFreeCredentialsHandle=NULL;\n";
        cw << "static pEncryptMessage_t pEncryptMessage=NULL;\n";
        cw << "static pDecryptMessage_t pDecryptMessage=NULL;\n";
        cw << "static pQueryContextAttributesA_t pQueryContextAttributesA=NULL;\n";
        cw << "static int tls_init(void){\n";
        cw << "  static int n=0;if(n)return n;\n";
        cw << "  HMODULE h=LoadLibraryA(\"secur32.dll\");if(!h){n=-1;return n;}\n";
        cw << "  pAcquireCredentialsHandleA=(void*)GetProcAddress(h,\"AcquireCredentialsHandleA\");\n";
        cw << "  pInitializeSecurityContextA=(void*)GetProcAddress(h,\"InitializeSecurityContextA\");\n";
        cw << "  pDeleteSecurityContext=(void*)GetProcAddress(h,\"DeleteSecurityContext\");\n";
        cw << "  pFreeCredentialsHandle=(void*)GetProcAddress(h,\"FreeCredentialsHandle\");\n";
        cw << "  pEncryptMessage=(void*)GetProcAddress(h,\"EncryptMessage\");\n";
        cw << "  pDecryptMessage=(void*)GetProcAddress(h,\"DecryptMessage\");\n";
        cw << "  pQueryContextAttributesA=(void*)GetProcAddress(h,\"QueryContextAttributesA\");\n";
        cw << "  if(!pAcquireCredentialsHandleA||!pInitializeSecurityContextA||!pDeleteSecurityContext||!pFreeCredentialsHandle||!pEncryptMessage||!pDecryptMessage||!pQueryContextAttributesA){n=-1;return n;}\n";
        cw << "  n=1;return 1;}\n";
        cw << "  static struct tls_ses{int fd;CredHandle cred;CtxtHandle ctx;BOOL ok;\n";
        cw << "  SecPkgContext_StreamSizes sz;}g_tls_ses[MAX_TLS_SESS];\n";
        cw << "static int tls_handshake(int fd,CredHandle*cred,CtxtHandle*ctx,SecPkgContext_StreamSizes*sz){\n";
        cw << "  if(tls_init()<0)return-10;\n";
        cw << "  SCHANNEL_CRED sc={sizeof(sc),SCH_CRED_NO_DEFAULT_CREDS};\n";
        cw << "  sc.grbitEnabledProtocols=SP_PROT_TLS1_2_CLIENT|SP_PROT_TLS1_3_CLIENT;\n";
        cw << "  TimeStamp ts;SECURITY_STATUS ss;\n";
        cw << "  ss=pAcquireCredentialsHandleA(NULL,UNISP_NAME,SECPKG_CRED_OUTBOUND,NULL,&sc,NULL,NULL,cred,&ts);\n";
        cw << "  if(ss<0)return-1;\n";
        cw << "  SecBuffer outb[1];SecBufferDesc outd;outd.ulVersion=SECBUFFER_VERSION;outd.cBuffers=1;outd.pBuffers=outb;\n";
        cw << "  SecBuffer inb[2];SecBufferDesc ind;ind.ulVersion=SECBUFFER_VERSION;ind.cBuffers=2;ind.pBuffers=inb;\n";
        cw << "  DWORD ctxFlags=ISC_REQ_STREAM|ISC_REQ_CONFIDENTIALITY;\n";
        cw << "  char obuf[16384];outb[0].cbBuffer=sizeof(obuf);outb[0].BufferType=SECBUFFER_TOKEN;outb[0].pvBuffer=obuf;\n";
        cw << "  inb[0].cbBuffer=0;inb[0].BufferType=SECBUFFER_TOKEN;inb[0].pvBuffer=NULL;\n";
        cw << "  inb[1].cbBuffer=0;inb[1].BufferType=SECBUFFER_EMPTY;inb[1].pvBuffer=NULL;\n";
        cw << "  ULONG attr;ss=pInitializeSecurityContextA(cred,NULL,NULL,ctxFlags,0,0,NULL,0,ctx,&outd,&attr,&ts);\n";
        cw << "  if(ss!=SEC_I_CONTINUE_NEEDED&&ss<0){pFreeCredentialsHandle(cred);return-2;}\n";
        cw << "  if(outb[0].cbBuffer>0&&psend(fd,(char*)outb[0].pvBuffer,outb[0].cbBuffer,0)<0){pFreeCredentialsHandle(cred);return-3;}\n";
        cw << "  char ibuf[16384];int n=(int)precv(fd,ibuf,sizeof(ibuf)-1,0);\n";
        cw << "  if(n<=0){pFreeCredentialsHandle(cred);return-4;}\n";
        cw << "  inb[0].cbBuffer=n;inb[0].pvBuffer=ibuf;inb[0].BufferType=SECBUFFER_TOKEN;\n";
        cw << "  inb[1].cbBuffer=0;inb[1].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  int max_loop=50;while(--max_loop){\n";
        cw << "    ss=pInitializeSecurityContextA(cred,ctx,NULL,ctxFlags,0,0,&ind,0,NULL,&outd,&attr,&ts);\n";
        cw << "    if(ss<0&&ss!=SEC_I_CONTINUE_NEEDED&&ss!=SEC_I_INCOMPLETE_CREDENTIALS){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-5;}\n";
        cw << "    if(outb[0].cbBuffer>0&&psend(fd,(char*)outb[0].pvBuffer,outb[0].cbBuffer,0)<0){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-6;}\n";
        cw << "    if(ss==SEC_E_OK){ss=pQueryContextAttributesA(ctx,SECPKG_ATTR_STREAM_SIZES,sz);if(ss<0){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-7;}return 0;}\n";
        cw << "    n=(int)precv(fd,ibuf,sizeof(ibuf)-1,0);if(n<=0){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-8;}\n";
        cw << "    inb[0].cbBuffer=n;inb[0].pvBuffer=ibuf;inb[0].BufferType=SECBUFFER_TOKEN;\n";
        cw << "  }return-9;}\n";
        cw << "static JSValue bridge_tls_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"tls_connect:need host port\");\n";
        cw << "  int32_t port;JS_ToInt32(ctx,&port,av[1]);if(port<1)return JS_ThrowTypeError(ctx,\"bad port\");\n";
        cw << "  JSValue fdv=bridge_net_connect(ctx,t,ac,av);\n";
        cw << "  if(JS_IsException(fdv))return JS_EXCEPTION;\n";
        cw << "  int32_t fd;JS_ToInt32(ctx,&fd,fdv);JS_FreeValue(ctx,fdv);\n";
        cw << "  int slot=-1;for(int i=0;i<MAX_TLS_SESS;i++){if(!g_tls_ses[i].ok){slot=i;break;}}\n";
        cw << "  if(slot<0){pclosesocket(fd);return JS_ThrowTypeError(ctx,\"tls:no sessions\");}\n";
        cw << "  memset(&g_tls_ses[slot],0,sizeof(g_tls_ses[slot]));\n";
        cw << "  g_tls_ses[slot].fd=fd;\n";
        cw << "  int r=tls_handshake(fd,&g_tls_ses[slot].cred,&g_tls_ses[slot].ctx,&g_tls_ses[slot].sz);\n";
        cw << "  if(r<0){memset(&g_tls_ses[slot],0,sizeof(g_tls_ses[slot]));pclosesocket(fd);return JS_ThrowTypeError(ctx,\"tls:handshake failed\");}\n";
        cw << "  g_tls_ses[slot].ok=1;return JS_NewInt32(ctx,slot+1);}\n\n";
        cw << "static JSValue bridge_tls_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"tls_write:need handle data\");\n";
        cw << "  int32_t h;JS_ToInt32(ctx,&h,av[0]);if(h<1||h>MAX_TLS_SESS||!g_tls_ses[h-1].ok)return JS_ThrowTypeError(ctx,\"bad handle\");\n";
        cw << "  struct tls_ses*ss=&g_tls_ses[h-1];\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data)return JS_ThrowTypeError(ctx,\"bad data\");\n";
        cw << "  size_t dlen=strlen(data);\n";
        cw << "  if(dlen>ss->sz.cbMaximumMessage){JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"data too large\");}\n";
        cw << "  size_t bufsz=ss->sz.cbHeader+ss->sz.cbMaximumMessage+ss->sz.cbTrailer;\n";
        cw << "  char*buf=(char*)malloc(bufsz);if(!buf){JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"OOM\");}\n";
        cw << "  memcpy(buf+ss->sz.cbHeader,data,dlen);\n";
        cw << "  SecBuffer eb[4];SecBufferDesc ed;ed.ulVersion=SECBUFFER_VERSION;ed.cBuffers=4;ed.pBuffers=eb;\n";
        cw << "  eb[0].cbBuffer=bufsz;eb[0].BufferType=SECBUFFER_STREAM;eb[0].pvBuffer=buf;\n";
        cw << "  eb[1].cbBuffer=0;eb[1].BufferType=SECBUFFER_DATA;eb[1].pvBuffer=NULL;\n";
        cw << "  eb[2].cbBuffer=0;eb[2].BufferType=SECBUFFER_DATA;eb[2].pvBuffer=NULL;\n";
        cw << "  eb[3].cbBuffer=0;eb[3].BufferType=SECBUFFER_DATA;eb[3].pvBuffer=NULL;\n";
        cw << "  SECURITY_STATUS ss=pEncryptMessage(&ss->ctx,0,&ed,0);\n";
        cw << "  if(ss<0){free(buf);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"encrypt failed\");}\n";
        cw << "  int n=(int)psend(ss->fd,buf+ss->sz.cbHeader,eb[0].cbBuffer,0);\n";
        cw << "  free(buf);JS_FreeCString(ctx,data);\n";
        cw << "  if(n<0)return JS_ThrowTypeError(ctx,\"send failed\");return JS_NewInt32(ctx,n);}\n\n";
        cw << "static JSValue bridge_tls_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"tls_read:need handle\");\n";
        cw << "  int32_t h;JS_ToInt32(ctx,&h,av[0]);if(h<1||h>MAX_TLS_SESS||!g_tls_ses[h-1].ok)return JS_ThrowTypeError(ctx,\"bad handle\");\n";
        cw << "  struct tls_ses*ss=&g_tls_ses[h-1];\n";
        cw << "  char ibuf[16384];int n=(int)precv(ss->fd,ibuf,sizeof(ibuf)-1,0);\n";
        cw << "  if(n<=0){if(n==0)return JS_NULL;return JS_ThrowTypeError(ctx,\"recv failed\");}\n";
        cw << "  SecBuffer rb[4];SecBufferDesc rd;rd.ulVersion=SECBUFFER_VERSION;rd.cBuffers=4;rd.pBuffers=rb;\n";
        cw << "  rb[0].cbBuffer=n;rb[0].BufferType=SECBUFFER_DATA;rb[0].pvBuffer=ibuf;\n";
        cw << "  rb[1].cbBuffer=0;rb[1].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  rb[2].cbBuffer=0;rb[2].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  rb[3].cbBuffer=0;rb[3].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  SECURITY_STATUS ss=pDecryptMessage(&ss->ctx,&rd,0,NULL);\n";
        cw << "  if(ss<0)return JS_ThrowTypeError(ctx,\"decrypt failed\");\n";
        cw << "  for(int i=0;i<4;i++){if(rb[i].BufferType==SECBUFFER_DATA&&rb[i].cbBuffer>0){return JS_NewStringLen(ctx,(const char*)rb[i].pvBuffer,rb[i].cbBuffer);}}\n";
        cw << "  return JS_NULL;}\n\n";
        cw << "static JSValue bridge_tls_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;int32_t h;JS_ToInt32(ctx,&h,av[0]);\n";
        cw << "  if(h<1||h>MAX_TLS_SESS||!g_tls_ses[h-1].ok)return JS_FALSE;\n";
        cw << "  struct tls_ses*ss=&g_tls_ses[h-1];\n";
        cw << "  pDeleteSecurityContext(&ss->ctx);pFreeCredentialsHandle(&ss->cred);\n";
        cw << "  pclosesocket(ss->fd);memset(ss,0,sizeof(*ss));return JS_TRUE;}\n\n";
#else
        cw << "/* TLS stub for POSIX — falls back to raw TCP */\n";
        cw << "static JSValue bridge_tls_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_connect(ctx,t,ac,av);}\n";
        cw << "static JSValue bridge_tls_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_write(ctx,t,ac,av);}\n";
        cw << "static JSValue bridge_tls_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_read(ctx,t,ac,av);}\n";
        cw << "static JSValue bridge_tls_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_close(ctx,t,ac,av);}\n\n";
#endif

        /* ── dns helper (getaddrinfo via dynamic loading) ── */
        cw << "static JSValue bridge_dns_lookup(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"dns_lookup:need hostname\");\n";
        cw << "  if(net_init()<0)return JS_ThrowTypeError(ctx,\"WSAStartup failed\");\n";
        cw << "  const char*host=JS_ToCString(ctx,av[0]);if(!host)return JS_ThrowTypeError(ctx,\"bad host\");\n";
        cw << "  struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;\n";
        cw << "  int gai=pgetaddrinfo(host,\"80\",&hints,&res);JS_FreeCString(ctx,host);\n";
        cw << "  if(gai!=0||!res){if(res)pfreeaddrinfo(res);return JS_NULL;}\n";
        cw << "  char addr[64]=\"\";\n";
        cw << "  struct sockaddr_in*sin=(struct sockaddr_in*)res->ai_addr;\n";
        cw << "  pinet_ntop(AF_INET,&sin->sin_addr,addr,sizeof(addr));\n";
        cw << "  pfreeaddrinfo(res);return addr[0]?JS_NewString(ctx,addr):JS_NULL;}\n\n";

        /* JS-callable wrapper for bridge_npm_install */
        cw << "static JSValue js_bridge_npm_install(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;const char*n=JS_ToCString(ctx,av[0]);if(!n)return JS_FALSE;\n";
        cw << "  JSValue r=bridge_npm_install(ctx,n);JS_FreeCString(ctx,n);return r;}\n\n";

        /* ── CJS version fallback for ESM-only packages ── */
        cw << "static const char* cjs_version(const char*name){\n";
        cw << "  static const struct{const char*n;const char*v;}esm[]={\n";
        cw << "    {\"chalk\",\"4\"},{\"nanocolors\",\"0\"},{\"got\",\"11\"},\n";
        cw << "    {\"ky\",\"0\"},{\"execa\",\"5\"},{\"globby\",\"11\"},\n";
        cw << "    {\"p-map\",\"4\"},{\"p-limit\",\"3\"},{NULL,NULL}\n";
        cw << "  };\n";
        cw << "  for(int i=0;esm[i].n;i++)if(strcmp(name,esm[i].n)==0)return esm[i].v;\n";
        cw << "  return NULL;}\n\n";
        cw << "/* ── bridge_npm_install — auto-download & extract npm package ── */\n";
        cw << "static JSValue bridge_npm_install(JSContext*ctx,const char*name){\n";
        cw << "  /* Support scoped packages: @scope/pkg → URL-encode as @scope%2Fpkg */\n";
        cw << "  char scope_encoded[4096];\n";
        cw << "  const char*reg_name=name;\n";
        cw << "  const char*slash_pos=strchr(name,'/');\n";
        cw << "  if(slash_pos&&name[0]=='@'){\n";
        cw << "    size_t slen=(size_t)(slash_pos-name);\n";
        cw << "    if(slen+8<sizeof(scope_encoded)){\n";
        cw << "      memcpy(scope_encoded,name,slen);\n";
        cw << "      memcpy(scope_encoded+slen,\"%2F\",3);\n";
        cw << "      strcpy(scope_encoded+slen+3,slash_pos+1);\n";
        cw << "      reg_name=scope_encoded;\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Check CJS fallback for ESM-only packages */\n";
        cw << "  const char*pin=cjs_version(name);\n";
        cw << "  /* Fetch package metadata from npm registry */\n";
        cw << "  char url[4096];\n";
        cw << "  if(pin)snprintf(url,sizeof(url),\"https://registry.npmjs.org/%s/%s\",reg_name,pin);\n";
        cw << "  else snprintf(url,sizeof(url),\"https://registry.npmjs.org/%s/latest\",reg_name);\n";
        cw << "  /* Retry loop for HTTP fetch (up to 3 attempts with backoff) */\n";
        cw << "  JSValue meta=JS_NULL;\n";
        cw << "  for(int attempt=0;attempt<3;attempt++){\n";
        cw << "    if(attempt>0){int ms=500<<(attempt-1);\n";
#ifdef _WIN32
        cw << "      Sleep(ms);\n";
#else
        cw << "      usleep(ms*1000);\n";
#endif
        cw << "    }\n";
        cw << "    JSValueConst meta_args[1];meta_args[0]=JS_NewString(ctx,url);\n";
        cw << "    meta=bridge_http_get(ctx,JS_NULL,1,meta_args);\n";
        cw << "    JS_FreeValue(ctx,meta_args[0]);\n";
        cw << "    if(!JS_IsException(meta)&&!JS_IsNull(meta))break;\n";
        cw << "    JS_FreeValue(ctx,meta);meta=JS_NULL;\n";
        cw << "  }\n";
        cw << "  if(JS_IsNull(meta)||JS_IsException(meta))return JS_FALSE;\n";
        cw << "  const char*ms=JS_ToCString(ctx,meta);if(!ms){JS_FreeValue(ctx,meta);return JS_FALSE;}\n";
        cw << "  JSValue po=JS_ParseJSON(ctx,ms,strlen(ms),\"<meta>\");JS_FreeCString(ctx,ms);JS_FreeValue(ctx,meta);\n";
        cw << "  if(JS_IsException(po))return JS_FALSE;\n";
        cw << "  JSValue tv=JS_GetPropertyStr(ctx,po,\"tarball\");\n";
        cw << "  if(JS_IsUndefined(tv)){JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);return JS_FALSE;}\n";
        cw << "  const char*tarball=JS_ToCString(ctx,tv);\n";
        cw << "  if(!tarball){JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);return JS_FALSE;}\n";
        cw << "  /* Download tarball (with retry) */\n";
        cw << "  JSValue dl=JS_NULL;\n";
        cw << "  for(int attempt=0;attempt<3;attempt++){\n";
        cw << "    if(attempt>0){int ms=500<<(attempt-1);\n";
#ifdef _WIN32
        cw << "      Sleep(ms);\n";
#else
        cw << "      usleep(ms*1000);\n";
#endif
        cw << "    }\n";
        cw << "    JSValueConst dl_args[1];dl_args[0]=JS_NewString(ctx,tarball);\n";
        cw << "    dl=bridge_http_get(ctx,JS_NULL,1,dl_args);\n";
        cw << "    JS_FreeValue(ctx,dl_args[0]);\n";
        cw << "    if(!JS_IsException(dl)&&!JS_IsNull(dl))break;\n";
        cw << "    JS_FreeValue(ctx,dl);dl=JS_NULL;\n";
        cw << "  }\n";
        cw << "  JS_FreeCString(ctx,tarball);JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);\n";
        cw << "  if(JS_IsNull(dl)||JS_IsException(dl))return JS_FALSE;\n";
        cw << "  const char*tgz=JS_ToCString(ctx,dl);if(!tgz){JS_FreeValue(ctx,dl);return JS_FALSE;}\n";
        cw << "  /* Write tarball to temp file */\n";
        cw << "  char tmpfile[1024]=\"\";const char*td=getenv(\"TMPDIR\");if(!td)td=\"/tmp\";\n";
#ifdef _WIN32
        cw << "  snprintf(tmpfile,sizeof(tmpfile),\"%s/qjs_npm_XXXXXX\",td);_mktemp_s(tmpfile,sizeof(tmpfile));\n";
#else
        cw << "  snprintf(tmpfile,sizeof(tmpfile),\"%s/qjs_npm_XXXXXX\",td);close(mkstemp(tmpfile));\n";
#endif
        cw << "  FILE*fw=fopen(tmpfile,\"wb\");if(fw){fwrite(tgz,1,strlen(tgz),fw);fclose(fw);}\n";
        cw << "  JS_FreeCString(ctx,tgz);JS_FreeValue(ctx,dl);\n";
        cw << "  /* Ensure node_modules and extract */\n";
#ifdef _WIN32
        cw << "  _mkdir(\"node_modules\");\n";
        cw << "  /* Create scope subdirectory for scoped packages */\n";
        cw << "  if(slash_pos&&name[0]=='@'){\n";
        cw << "    char scopedir[4096];snprintf(scopedir,sizeof(scopedir),\"node_modules/%.*s\",(int)(slash_pos-name),name);\n";
        cw << "    _mkdir(scopedir);\n";
        cw << "  }\n";
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"tar xzf \\\"%s\\\" -C node_modules 2>nul\",tmpfile);\n";
#else
        cw << "  mkdir(\"node_modules\",0755);\n";
        cw << "  /* Create scope subdirectory for scoped packages */\n";
        cw << "  if(slash_pos&&name[0]=='@'){\n";
        cw << "    char scopedir[4096];snprintf(scopedir,sizeof(scopedir),\"node_modules/%.*s\",(int)(slash_pos-name),name);\n";
        cw << "    mkdir(scopedir,0755);\n";
        cw << "  }\n";
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"tar xzf '%s' -C node_modules 2>/dev/null\",tmpfile);\n";
#endif
        cw << "  int rc=system(cmd);remove(tmpfile);\n";
        cw << "  if(rc!=0)return JS_FALSE;\n";
        cw << "  /* Rename node_modules/package -> node_modules/<name> */\n";
        cw << "  char oldp[4096],newp[4096];\n";
        cw << "  snprintf(oldp,sizeof(oldp),\"node_modules/package\");\n";
        cw << "  snprintf(newp,sizeof(newp),\"node_modules/%s\",name);\n";
#ifdef _WIN32
        cw << "  _unlink(newp);_rmdir(newp);\n";
#else
        cw << "  unlink(newp);rmdir(newp);\n";
#endif
        cw << "  rename(oldp,newp);\n";
        cw << "  return JS_TRUE;}\n\n";

        /* ── js_rust_load — compile and load a Rust crate as JS object ── */
        cw << "static JSValue js_rust_load(JSContext*ctx,const char*name){\n";
        cw << "  char tmpdir[1024]=\"\";const char*td=getenv(\"TMPDIR\");if(!td)td=\"/tmp\";\n";
        cw << "  snprintf(tmpdir,sizeof(tmpdir),\"%s/qjs_rust_XXXXXX\",td);\n";
#ifdef _WIN32
        cw << "  _mktemp_s(tmpdir,sizeof(tmpdir));_mkdir(tmpdir);\n";
#else
        cw << "  close(mkstemp(tmpdir));unlink(tmpdir);mkdir(tmpdir,0755);\n";
#endif
        cw << "  char dir[4096];snprintf(dir,sizeof(dir),\"%s\",tmpdir);\n";
        cw << "  /* Write Cargo.toml */\n";
        cw << "  char tp[4096];snprintf(tp,sizeof(tp),\"%s/Cargo.toml\",dir);\n";
        cw << "  FILE*f=fopen(tp,\"w\");if(!f){rmdir(dir);return JS_NULL;}\n";
        cw << "  fprintf(f,\"[package]\\nname = \\\"%s_bridge\\\"\\nversion = \\\"0.1.0\\\"\\nedition = \\\"2021\\\"\\n\\n\",name);\n";
        cw << "  fprintf(f,\"[lib]\\ncrate-type = [\\\"cdylib\\\"]\\n\\n\");\n";
        cw << "  fprintf(f,\"[dependencies]\\nserde_json = \\\"1\\\"\\n%s = \\\"*\\\"\\n\",name);\n";
        cw << "  fprintf(f,\"futures = \\\"0.3\\\"\\n\");fclose(f);\n";
        cw << "  /* Write src/lib.rs */\n";
        cw << "  snprintf(tp,sizeof(tp),\"%s/src\",dir);\n";
#ifdef _WIN32
        cw << "  _mkdir(tp);\n";
#else
        cw << "  mkdir(tp,0755);\n";
#endif
        cw << "  snprintf(tp,sizeof(tp),\"%s/src/lib.rs\",dir);\n";
        cw << "  f=fopen(tp,\"w\");if(!f){rmdir(dir);return JS_NULL;}\n";
        cw << "  fprintf(f,\"use std::collections::HashMap;\\n\");\n";

        cw << "  fprintf(f,\"type RustFn = fn(Vec<serde_json::Value>)->Result<serde_json::Value,String>;\\n\");\n";

        cw << "  fprintf(f,\"#[no_mangle]\\npub extern \\\"C\\\" fn rust_bridge_get_fns()->*mut HashMap<String,RustFn>{\\n\");\n";

        cw << "  fprintf(f,\"  let mut m:HashMap<String,RustFn> = HashMap::new();\\n\");\n";

        cw << "  fprintf(f,\"  Box::into_raw(Box::new(m))\\n}\");\n";
        cw << "  fclose(f);\n";
        cw << "  /* Cargo build */\n";
        cw << "  snprintf(tp,sizeof(tp),\"cd %s && cargo build --release 2>&1\",dir);\n";
        cw << "  if(system(tp)!=0){rmdir(dir);return JS_NULL;}\n";
        cw << "  /* Load the .dll/.so */\n";
        cw << "  char libpath[4096];\n";
#ifdef _WIN32
        cw << "  snprintf(libpath,sizeof(libpath),\"%s/target/release/%s_bridge.dll\",dir,name);\n";
        cw << "  HMODULE lib=LoadLibraryA(libpath);\n";
        cw << "  if(!lib){rmdir(dir);return JS_NULL;}\n";
        cw << "  typedef void*(*GetFns)();GetFns gf=(GetFns)GetProcAddress(lib,\"rust_bridge_get_fns\");\n";
        cw << "  if(!gf){FreeLibrary(lib);rmdir(dir);return JS_NULL;}\n";
#else
        cw << "  snprintf(libpath,sizeof(libpath),\"%s/target/release/lib%s_bridge.so\",dir,name);\n";
        cw << "  void*lib=dlopen(libpath,RTLD_NOW|RTLD_LOCAL);\n";
        cw << "  if(!lib){rmdir(dir);return JS_NULL;}\n";
        cw << "  typedef void*(*GetFns)();GetFns gf=(GetFns)dlsym(lib,\"rust_bridge_get_fns\");\n";
        cw << "  if(!gf){dlclose(lib);rmdir(dir);return JS_NULL;}\n";
#endif
        cw << "  JSValue obj=JS_NewObject(ctx);if(JS_IsException(obj)){";
#ifdef _WIN32
        cw << "FreeLibrary(lib);";
#else
        cw << "dlclose(lib);";
#endif
        cw << "rmdir(dir);return JS_EXCEPTION;}\n";
        cw << "  JS_SetPropertyStr(ctx,obj,\"__rust_crate\",JS_NewString(ctx,libpath));\n";
        cw << "  return obj;}\n\n";

        /* Cleanup */
        cw << "static void cleanup_qjs(void){\n";
        cw << "  /* Free cached module names and exports (JS refs) before destroying context */\n";
        cw << "  for(int i=0;i<g_cache_count;i++){free((void*)g_cache_names[i]);JS_FreeValue(g_ctx,g_cache_exports[i]);}\n";
        cw << "  g_cache_count=0;\n";
        cw << "  if(g_ctx){for(int i=0;i<MAX_OBJS;i++){if(!JS_IsNull(g_objs[i])&&!JS_IsUndefined(g_objs[i]))JS_FreeValue(g_ctx,g_objs[i]);g_objs[i]=JS_NULL;}\n";
        cw << "    JS_FreeContext(g_ctx);g_ctx=NULL;}\n";
        cw << "  if(g_rt){JS_FreeRuntime(g_rt);g_rt=NULL;}\n";
        cw << "  g_inited=0;g_next_id=2;}\n\n";

        /* qjs_call_impl — core call dispatcher */
        cw << "static void* qjs_call(void*mod,const char*fn,const char*ajs){\n";
        cw << "  if(!mod||!g_ctx||!g_inited)return NULL;\n";
        cw << "  int oid=(int)(intptr_t)mod;\n";
        cw << "  if(oid<0||oid>=MAX_OBJS||JS_IsNull(g_objs[oid])||JS_IsUndefined(g_objs[oid]))return NULL;\n";
        cw << "  JSValue obj=g_objs[oid];\n";
        cw << "  JSValue func=fn&&fn[0]?JS_GetPropertyStr(g_ctx,obj,fn):JS_DupValue(g_ctx,obj);\n";
        cw << "  if(JS_IsException(func)){JS_FreeValue(g_ctx,func);return NULL;}\n";
        cw << "  if(!JS_IsFunction(g_ctx,func)){\n";
        cw << "    if(JS_IsObject(func)){int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,func);return NULL;}g_objs[nid]=func;return(void*)(intptr_t)nid;}\n";
        cw << "    char*j=jsval_to_json(g_ctx,func);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,func);return h;}\n";
        cw << "  JSValue av=JS_NULL;JSValue*argv=NULL;int argc=0;\n";
        cw << "  if(ajs&&ajs[0]&&strcmp(ajs,\"null\")!=0){\n";
        cw << "    av=JS_ParseJSON(g_ctx,ajs,strlen(ajs),\"<a>\");\n";
        cw << "    if(!JS_IsException(av)&&JS_IsArray(g_ctx,av)){\n";
        cw << "      JSValue lv=JS_GetPropertyStr(g_ctx,av,\"length\");JS_ToInt32(g_ctx,&argc,lv);JS_FreeValue(g_ctx,lv);\n";
        cw << "      if(argc>0){argv=(JSValue*)malloc((size_t)argc*sizeof(JSValue));if(!argv){argc=0;}else{for(int i=0;i<argc;i++)argv[i]=JS_GetPropertyUint32(g_ctx,av,i);}}\n";
        cw << "      JS_FreeValue(g_ctx,av);\n";
        cw << "    }else if(!JS_IsException(av)){argc=1;argv=(JSValue*)malloc(sizeof(JSValue));if(argv)argv[0]=av;else{argc=0;JS_FreeValue(g_ctx,av);}}\n";
        cw << "  }\n";
        cw << "  JSValue r=JS_Call(g_ctx,func,obj,argc,argv);JS_FreeValue(g_ctx,func);\n";
        cw << "  if(argv){for(int i=0;i<argc;i++)JS_FreeValue(g_ctx,argv[i]);free(argv);}\n";
        cw << "  if(JS_IsException(r))return NULL;\n";
        cw << "  if(JS_IsObject(r)||JS_IsFunction(g_ctx,r)){\n";
        cw << "    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,r);return NULL;}\n";
        cw << "    g_objs[nid]=JS_DupValue(g_ctx,r);JS_FreeValue(g_ctx,r);return(void*)(intptr_t)nid;}\n";
        cw << "  char*j=jsval_to_json(g_ctx,r);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,r);return h;}\n\n";

        /* ── Per-package exposed API ── */

        /* _require */
        cw << "EXPORT void* " << pid << "_require(void){\n";
        cw << "  if(!g_inited&&!init_qjs(\"" << dir << "\"))return NULL;\n";
        cw << "  if(g_ctx&&!g_loaded){\n";
        cw << "    for(int i=0;i<MAX_OBJS;i++)g_objs[i]=JS_NULL;\n";
        cw << "    JSValue g=JS_GetGlobalObject(g_ctx);\n";
        cw << "    JSValue rv=JS_GetPropertyStr(g_ctx,g,\"require\");\n";
        cw << "    if(JS_IsFunction(g_ctx,rv)){\n";
        cw << "      JSValue nm=JS_NewString(g_ctx,\"" << pkg << "\");\n";
        cw << "      JSValue md=JS_Call(g_ctx,rv,JS_UNDEFINED,1,&nm);JS_FreeValue(g_ctx,nm);\n";
        cw << "      if(!JS_IsException(md)){g_objs[1]=JS_DupValue(g_ctx,md);JS_FreeValue(g_ctx,md);g_loaded=1;}\n";
        cw << "    }\n";
        cw << "    JS_FreeValue(g_ctx,rv);JS_FreeValue(g_ctx,g);\n";
        cw << "  }\n";
        cw << "  return g_inited&&g_loaded?(void*)(intptr_t)1:NULL;}\n\n";

        /* _free */
cw << "EXPORT void " << pid << "_free(void*h){\n";
        cw << "  if(!h||(intptr_t)h<2)return;\n";
        cw << "  /* JSON handles (store_json) are heap ptrs with bit 0 set; object IDs are < MAX_OBJS */\n";
        cw << "  if((intptr_t)h > MAX_OBJS && ((intptr_t)h & 1) != 0){\n";
        cw << "    char**pp=(char**)UNTAG(h);free(*pp);free(pp);return;}\n";
        cw << "  int id=(int)(intptr_t)h;\n";
        cw << "  if(id<MAX_OBJS){if(!JS_IsNull(g_objs[id])&&!JS_IsUndefined(g_objs[id]))JS_FreeValue(g_ctx,g_objs[id]);g_objs[id]=JS_NULL;}}\n\n";

        /* _free_cstr */
        cw << "EXPORT void " << pid << "_free_cstr(void*s){if(s)free(s);}\n";
        cw << "EXPORT char* " << pid << "_get_last_error(void){return g_last_error;}\n";
        cw << "EXPORT const char* " << pid << "_get_perf_stats(void){\n";
        cw << "  static char buf[1024];\n";
        cw << "  snprintf(buf,sizeof(buf),\"{\\\"require_calls\\\":%u,\\\"bc_hits\\\":%u,\\\"bc_misses\\\":%u,\"\n";
        cw << "    \"\\\"exports_lookups\\\":%u,\\\"eval_time_ms\\\":%u,\\\"resolve_time_ms\\\":%u}\",\n";
        cw << "    g_perf.require_calls,g_perf.bc_hits,g_perf.bc_misses,\n";
        cw << "    g_perf.exports_lookups,g_perf.eval_time_ms,g_perf.resolve_time_ms);\n";
        cw << "  return buf;\n";
        cw << "}\n\n";

        /* _str */
        cw << "EXPORT void* " << pid << "_str(const char*s){\n";
        cw << "  if(!s)return store_json(\"null\");char esc[8192];json_escape(s,esc,sizeof(esc));\n";
        cw << "  char buf[8194];snprintf(buf,sizeof(buf),\"\\\"%s\\\"\",esc);return store_json(buf);}\n\n";

        /* _int */
        cw << "EXPORT void* " << pid << "_int(long long v){\n";
        cw << "  char buf[64];snprintf(buf,sizeof(buf),\"%lld\",v);return store_json(buf);}\n\n";

        /* _float */
        cw << "EXPORT void* " << pid << "_float(double v){\n";
        cw << "  char buf[64];snprintf(buf,sizeof(buf),\"%g\",v);return store_json(buf);}\n\n";

        /* _to_cstr */
        cw << "EXPORT char* " << pid << "_to_cstr(void*obj){\n";
        cw << "  if(!obj)return NULL;const char*j=get_json(obj);\n";
        cw << "  char*out=(char*)malloc(strlen(j)+1);if(!out)return NULL;strcpy(out,j);return out;}\n\n";

        /* _tuple2-4, _tuple, _list2-4, _list */
        cw << "EXPORT void* " << pid << "_tuple2(void*a,void*b){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s]\",get_json(a),get_json(b));return store_json(buf);}\n";
        cw << "EXPORT void* " << pid << "_tuple3(void*a,void*b,void*c){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s]\",get_json(a),get_json(b),get_json(c));return store_json(buf);}\n";
        cw << "EXPORT void* " << pid << "_tuple4(void*a,void*b,void*c,void*d){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d));return store_json(buf);}\n";
        cw << "EXPORT void* " << pid << "_tuple(void**items,int count){\n";
        cw << "  if(count<=0)return store_json(\"[]\");char*b=(char*)malloc(16384);if(!b)return NULL;\n";
        cw << "  int p=0,r=16384;\n";
        cw << "#define T_APPEND(...) do{int w=snprintf(b+p,r,__VA_ARGS__);if(w>0){p+=w;if(p>=16383)p=16383;r=16384-p;}}while(0)\n";
        cw << "  T_APPEND(\"[\");for(int i=0;i<count&&p<16383;i++){if(i>0)T_APPEND(\",\");T_APPEND(\"%s\",get_json(items[i]));}\n";
        cw << "  T_APPEND(\"]\");#undef T_APPEND\n";
        cw << "  void*h=store_json(b);free(b);return h;}\n";
        cw << "EXPORT void* " << pid << "_list2(void*a,void*b){return " << pid << "_tuple2(a,b);}\n";
        cw << "EXPORT void* " << pid << "_list3(void*a,void*b,void*c){return " << pid << "_tuple3(a,b,c);}\n";
        cw << "EXPORT void* " << pid << "_list4(void*a,void*b,void*c,void*d){return " << pid << "_tuple4(a,b,c,d);}\n";
        cw << "EXPORT void* " << pid << "_list(void**items,int count){return " << pid << "_tuple(items,count);}\n";

        /* _dict, _dict_set */
        cw << "EXPORT void* " << pid << "_dict(void){return store_json(\"{}\");}\n";
cw << "EXPORT int " << pid << "_dict_set(void*d,const char*key,void*val){\n";
        cw << "  if(!d||!key)return -1;char**pp=(char**)UNTAG(d);const char*vj=get_json(val);\n";
        cw << "  char ek[512];json_escape(key,ek,sizeof(ek));char entry[2048];\n";
        cw << "  int el=(int)snprintf(entry,sizeof(entry),\"\\\"%s\\\":%s\",ek,vj);if(el<0)return -1;\n";
        cw << "  size_t ol=strlen(*pp);\n";
        cw << "  if(ol<=2){char*ns=(char*)malloc((size_t)el+3);if(!ns)return -1;snprintf(ns,(size_t)el+3,\"{%s}\",entry);free(*pp);*pp=ns;}\n";
        cw << "  else{size_t nl=ol+(size_t)el+2;char*ns=(char*)malloc(nl);if(!ns)return -1;snprintf(ns,nl,\"%.*s,%s}\",(int)(ol-1),*pp,entry);free(*pp);*pp=ns;}\n";
        cw << "  return 0;}\n\n";

        /* _call */
        cw << "EXPORT void* " << pid << "_call(void*mod,const char*fn,void*args){\n";
        cw << "  if(!args)return qjs_call(mod,fn,\"[]\");return qjs_call(mod,fn,get_json(args));}\n\n";

        /* _call1..6, _call_kw */
        cw << "EXPORT void* " << pid << "_call1(void*mod,const char*fn,void*a){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s]\",get_json(a));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call2(void*mod,const char*fn,void*a,void*b){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s]\",get_json(a),get_json(b));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call3(void*mod,const char*fn,void*a,void*b,void*c){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s]\",get_json(a),get_json(b),get_json(c));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call4(void*mod,const char*fn,void*a,void*b,void*c,void*d){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call5(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call6(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e,void*f){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e),get_json(f));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call_kw(void*mod,const char*fn,void*args,void*kwargs){\n";
        cw << "  const char*as=get_json(args);const char*ks=get_json(kwargs);size_t al=strlen(as);char buf[65536];\n";
        cw << "  if(al>=2&&al<=32768)snprintf(buf,sizeof(buf),\"%.*s,%s]\",(int)(al-1),as,ks);\n";
        cw << "  else snprintf(buf,sizeof(buf),\"[%s]\",ks);return qjs_call(mod,fn,buf);}\n\n";

        /* _getattr */
        cw << "EXPORT void* " << pid << "_getattr(void*obj,const char*name){\n";
        cw << "  if(!obj||!name||!g_ctx||!g_inited)return NULL;\n";
        cw << "  int oid=(int)(intptr_t)obj;\n";
        cw << "  if(oid<0||oid>=MAX_OBJS||JS_IsNull(g_objs[oid])||JS_IsUndefined(g_objs[oid]))return NULL;\n";
        cw << "  JSValue v=JS_GetPropertyStr(g_ctx,g_objs[oid],name);\n";
        cw << "  if(JS_IsException(v))return NULL;\n";
        cw << "  if(JS_IsObject(v)||JS_IsFunction(g_ctx,v)){\n";
        cw << "    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,v);return NULL;}\n";
        cw << "    g_objs[nid]=JS_DupValue(g_ctx,v);JS_FreeValue(g_ctx,v);return(void*)(intptr_t)nid;}\n";
        cw << "  char*j=jsval_to_json(g_ctx,v);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,v);return h;}\n\n";

        /* DllMain */
#ifdef _WIN32
        cw << "BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID l){if(r==DLL_PROCESS_DETACH)cleanup_qjs();return TRUE;}\n";
#else
        cw << "__attribute__((destructor)) static void dll_cleanup(void){cleanup_qjs();}\n";
#endif

        std::string wrapper_path = dir + "/" + pkg + "_npm.c";
        if (write_file(wrapper_path, cw.str()))
            std::cout << "[bridge]   " << wrapper_path << " (QuickJS)\n";
    }
}

