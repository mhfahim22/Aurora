#include "std/datetime.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

struct AuroraDateTime {
    int year, month, day, hour, minute, second, weekday;
};

struct AuroraDuration {
    int64_t microseconds;
};

struct AuroraTimeZone {
    std::string name;
    int offset_minutes;
};

extern "C" {

AuroraDateTime* aurora_datetime_now(void) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* gmt = std::gmtime(&t);
    auto* dt = (AuroraDateTime*)malloc(sizeof(AuroraDateTime));
    dt->year = gmt->tm_year + 1900;
    dt->month = gmt->tm_mon + 1;
    dt->day = gmt->tm_mday;
    dt->hour = gmt->tm_hour;
    dt->minute = gmt->tm_min;
    dt->second = gmt->tm_sec;
    dt->weekday = gmt->tm_wday;
    return dt;
}

AuroraDateTime* aurora_datetime_from_stamp(int64_t stamp) {
    std::time_t t = (std::time_t)stamp;
    std::tm* gmt = std::gmtime(&t);
    auto* dt = (AuroraDateTime*)malloc(sizeof(AuroraDateTime));
    dt->year = gmt->tm_year + 1900;
    dt->month = gmt->tm_mon + 1;
    dt->day = gmt->tm_mday;
    dt->hour = gmt->tm_hour;
    dt->minute = gmt->tm_min;
    dt->second = gmt->tm_sec;
    dt->weekday = gmt->tm_wday;
    return dt;
}

int64_t aurora_datetime_to_stamp(AuroraDateTime* dt) {
    if (!dt) return 0;
    std::tm t = {};
    t.tm_year = dt->year - 1900;
    t.tm_mon = dt->month - 1;
    t.tm_mday = dt->day;
    t.tm_hour = dt->hour;
    t.tm_min = dt->minute;
    t.tm_sec = dt->second;
    return (int64_t)std::mktime(&t);
}

AuroraDateTime* aurora_datetime_from_str(const char* s, const char* fmt) {
    if (!s || !fmt) return nullptr;
    std::tm t = {};
    std::istringstream ss(s);
    ss >> std::get_time(&t, fmt);
    if (ss.fail()) return nullptr;
    auto* dt = (AuroraDateTime*)malloc(sizeof(AuroraDateTime));
    dt->year = t.tm_year + 1900;
    dt->month = t.tm_mon + 1;
    dt->day = t.tm_mday;
    dt->hour = t.tm_hour;
    dt->minute = t.tm_min;
    dt->second = t.tm_sec;
    dt->weekday = t.tm_wday;
    return dt;
}

char* aurora_datetime_to_str(AuroraDateTime* dt, const char* fmt) {
    if (!dt || !fmt) return nullptr;
    std::tm t = {};
    t.tm_year = dt->year - 1900;
    t.tm_mon = dt->month - 1;
    t.tm_mday = dt->day;
    t.tm_hour = dt->hour;
    t.tm_min = dt->minute;
    t.tm_sec = dt->second;
    t.tm_wday = dt->weekday;
    char buf[256];
    if (std::strftime(buf, sizeof(buf), fmt, &t) == 0) return nullptr;
    return strdup(buf);
}

void aurora_datetime_free(AuroraDateTime* dt) { free(dt); }

int aurora_datetime_year(AuroraDateTime* dt) { return dt ? dt->year : 0; }
int aurora_datetime_month(AuroraDateTime* dt) { return dt ? dt->month : 0; }
int aurora_datetime_day(AuroraDateTime* dt) { return dt ? dt->day : 0; }
int aurora_datetime_hour(AuroraDateTime* dt) { return dt ? dt->hour : 0; }
int aurora_datetime_minute(AuroraDateTime* dt) { return dt ? dt->minute : 0; }
int aurora_datetime_second(AuroraDateTime* dt) { return dt ? dt->second : 0; }
int aurora_datetime_weekday(AuroraDateTime* dt) { return dt ? dt->weekday : 0; }

AuroraDateTime* aurora_datetime_add(AuroraDateTime* dt, AuroraDuration* dur) {
    if (!dt || !dur) return nullptr;
    int64_t total_secs = dur->microseconds / 1000000;
    int64_t stamp = aurora_datetime_to_stamp(dt) + total_secs;
    return aurora_datetime_from_stamp(stamp);
}

AuroraDuration* aurora_datetime_diff(AuroraDateTime* a, AuroraDateTime* b) {
    if (!a || !b) return nullptr;
    int64_t diff_us = (aurora_datetime_to_stamp(a) - aurora_datetime_to_stamp(b)) * 1000000;
    auto* dur = (AuroraDuration*)malloc(sizeof(AuroraDuration));
    dur->microseconds = diff_us;
    return dur;
}

int aurora_datetime_compare(AuroraDateTime* a, AuroraDateTime* b) {
    if (!a || !b) return 0;
    int64_t sa = aurora_datetime_to_stamp(a);
    int64_t sb = aurora_datetime_to_stamp(b);
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return 0;
}

AuroraDuration* aurora_duration_from_seconds(int64_t s) {
    auto* dur = (AuroraDuration*)malloc(sizeof(AuroraDuration));
    dur->microseconds = s * 1000000;
    return dur;
}

AuroraDuration* aurora_duration_from_millis(int64_t ms) {
    auto* dur = (AuroraDuration*)malloc(sizeof(AuroraDuration));
    dur->microseconds = ms * 1000;
    return dur;
}

AuroraDuration* aurora_duration_from_micros(int64_t us) {
    auto* dur = (AuroraDuration*)malloc(sizeof(AuroraDuration));
    dur->microseconds = us;
    return dur;
}

int64_t aurora_duration_to_seconds(AuroraDuration* dur) { return dur ? dur->microseconds / 1000000 : 0; }
int64_t aurora_duration_to_millis(AuroraDuration* dur) { return dur ? dur->microseconds / 1000 : 0; }
int64_t aurora_duration_to_micros(AuroraDuration* dur) { return dur ? dur->microseconds : 0; }
void aurora_duration_free(AuroraDuration* dur) { free(dur); }

AuroraTimeZone* aurora_timezone_local(void) {
    auto* tz = (AuroraTimeZone*)malloc(sizeof(AuroraTimeZone));
    std::time_t now = std::time(nullptr);
    std::tm local = {};
    std::tm utc = {};
#ifdef _WIN32
    localtime_s(&local, &now);
    gmtime_s(&utc, &now);
#else
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
#endif
    int offset = (int)std::difftime(std::mktime(&local), std::mktime(&utc)) / 60;
    tz->offset_minutes = offset;
    tz->name = "local";
    return tz;
}

AuroraTimeZone* aurora_timezone_utc(void) {
    auto* tz = (AuroraTimeZone*)malloc(sizeof(AuroraTimeZone));
    tz->offset_minutes = 0;
    tz->name = "UTC";
    return tz;
}

const char* aurora_timezone_name(AuroraTimeZone* tz) { return tz ? tz->name.c_str() : ""; }

int aurora_timezone_offset(AuroraTimeZone* tz) { return tz ? tz->offset_minutes : 0; }

void aurora_timezone_free(AuroraTimeZone* tz) { free(tz); }

}
