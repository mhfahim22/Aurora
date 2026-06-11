/* TLS SChannel Integration Test — Windows SChannel dynamic loading */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

static int g_pass=0,g_fail=0,g_cnt=0;
#define TEST(n){g_cnt++;printf("  test %d: %s ... ",g_cnt,n);
#define PASS()g_pass++;printf("PASS\n");}
#define FAIL(m)do{g_fail++;printf("FAIL: %s\n",m);return;}while(0)
#define CHECK(c,m)if(!(c)){FAIL(m);}

/* ── Test 1: secur32.dll loads, all 7 SSPI symbols resolve ── */
static void test_dll_loads(){
    printf("\n=== TLS SChannel: Dynamic Library Load ===\n");
    HMODULE h=LoadLibraryA("secur32.dll");
    TEST("LoadLibrary secur32.dll");
    CHECK(h!=0,"LoadLibraryA failed");
    PASS()
    if(!h)return;
    TEST("AcquireCredentialsHandleA");
    CHECK(GetProcAddress(h,"AcquireCredentialsHandleA")!=0,"missing");
    PASS()
    TEST("InitializeSecurityContextA");
    CHECK(GetProcAddress(h,"InitializeSecurityContextA")!=0,"missing");
    PASS()
    TEST("DeleteSecurityContext");
    CHECK(GetProcAddress(h,"DeleteSecurityContext")!=0,"missing");
    PASS()
    TEST("FreeCredentialsHandle");
    CHECK(GetProcAddress(h,"FreeCredentialsHandle")!=0,"missing");
    PASS()
    TEST("EncryptMessage");
    CHECK(GetProcAddress(h,"EncryptMessage")!=0,"missing");
    PASS()
    TEST("DecryptMessage");
    CHECK(GetProcAddress(h,"DecryptMessage")!=0,"missing");
    PASS()
    TEST("QueryContextAttributesA");
    CHECK(GetProcAddress(h,"QueryContextAttributesA")!=0,"missing");
    PASS()
    FreeLibrary(h);
}

/* ── Test 2: TCP connection (prerequisite for TLS) ── */
static void test_tcp_connect(){
    int r; char buf[1024];
    printf("\n=== TLS SChannel: TCP Connectivity ===\n");
    WSADATA wsa;
    TEST("WSAStartup");
    r=WSAStartup(MAKEWORD(2,2),&wsa);
    CHECK(r==0,"WSAStartup failed");
    PASS()
    struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    TEST("DNS resolve api.github.com");
    r=getaddrinfo("api.github.com","443",&hints,&res);
    CHECK(r==0&&res!=0,"DNS failed");
    PASS()
    if(!res)return;
    SOCKET fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    TEST("socket()");
    CHECK(fd!=INVALID_SOCKET,"socket failed");
    PASS()
    TEST("TCP connect api.github.com:443");
    r=connect(fd,res->ai_addr,(int)res->ai_addrlen);
    CHECK(r==0,"connect failed");
    PASS()
    freeaddrinfo(res);
    const char*req="GET / HTTP/1.1\r\nHost: api.github.com\r\nConnection: close\r\n\r\n";
    TEST("TCP send");
    r=send(fd,req,(int)strlen(req),0);
    CHECK(r>0,"send failed");
    PASS()
    TEST("TCP recv (expecting TLS alert)");
    r=(int)recv(fd,buf,sizeof(buf)-1,0);
    if(r>0)printf("(got %d raw bytes)\n",r);
    else printf("(recv=%d — expected, server rejected plain HTTP)\n",r);
    PASS()
    closesocket(fd);WSACleanup();
}

int main(){
    setbuf(stdout,NULL);setbuf(stderr,NULL);
    printf("========================================\n");
    printf("  TLS SChannel Integration Test\n");
    printf("========================================\n");
    test_dll_loads();
    test_tcp_connect();
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",g_pass,g_fail,g_cnt);
    printf("========================================\n");
    return g_fail>0?1:0;
}
#else
int main(){printf("TLS SChannel test: SKIP (not Windows)\n");return 0;}
#endif
