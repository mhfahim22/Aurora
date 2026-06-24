int add(int a, int b);
double sqrt(double x);
typedef struct { int x; int y; } Point;

#define MAX_BUFFER 4096
#define PI 3.14159
#define DEBUG 1

extern void* malloc(size_t size);
extern void free(void* ptr);

__stdcall int WinMain(void* hInstance, void* hPrevInstance, char* lpCmdLine, int nCmdShow);
int printf(const char* fmt, ...);
