#include <cstdio>
#include <cstring>
#include <cstdlib>

#define MAX_COVERAGE_POINTS 65536

struct CoverageRecord {
    const char* file;
    int line;
    int count;
};

static CoverageRecord g_records[MAX_COVERAGE_POINTS];
static int g_record_count = 0;

extern "C" void aurora_coverage_trace(const char* file, int line) {
    if (!file) return;
    for (int i = 0; i < g_record_count; i++) {
        if (g_records[i].line == line && strcmp(g_records[i].file, file) == 0) {
            g_records[i].count++;
            return;
        }
    }
    if (g_record_count < MAX_COVERAGE_POINTS) {
        g_records[g_record_count].file = file;
        g_records[g_record_count].line = line;
        g_records[g_record_count].count = 1;
        g_record_count++;
    }
}

extern "C" void aurora_coverage_report(void) {
    printf("\n=== Aurora Coverage Report ===\n");
    printf("Total distinct points hit: %d\n", g_record_count);
    int total_hits = 0;
    for (int i = 0; i < g_record_count; i++) {
        printf("  %s:%d  hit %d\n", g_records[i].file, g_records[i].line, g_records[i].count);
        total_hits += g_records[i].count;
    }
    printf("Total hits: %d\n", total_hits);
    printf("==============================\n");
}
