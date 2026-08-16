#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraDateTime AuroraDateTime;
typedef struct AuroraDuration AuroraDuration;
typedef struct AuroraTimeZone AuroraTimeZone;

AuroraDateTime* aurora_datetime_now(void);
AuroraDateTime* aurora_datetime_from_stamp(int64_t stamp);
int64_t         aurora_datetime_to_stamp(AuroraDateTime* dt);
AuroraDateTime* aurora_datetime_from_str(const char* s, const char* fmt);
char*           aurora_datetime_to_str(AuroraDateTime* dt, const char* fmt);
void            aurora_datetime_free(AuroraDateTime* dt);

int   aurora_datetime_year(AuroraDateTime* dt);
int   aurora_datetime_month(AuroraDateTime* dt);
int   aurora_datetime_day(AuroraDateTime* dt);
int   aurora_datetime_hour(AuroraDateTime* dt);
int   aurora_datetime_minute(AuroraDateTime* dt);
int   aurora_datetime_second(AuroraDateTime* dt);
int   aurora_datetime_weekday(AuroraDateTime* dt);

AuroraDateTime* aurora_datetime_add(AuroraDateTime* dt, AuroraDuration* dur);
AuroraDuration* aurora_datetime_diff(AuroraDateTime* a, AuroraDateTime* b);
int             aurora_datetime_compare(AuroraDateTime* a, AuroraDateTime* b);

AuroraDuration* aurora_duration_from_seconds(int64_t s);
AuroraDuration* aurora_duration_from_millis(int64_t ms);
AuroraDuration* aurora_duration_from_micros(int64_t us);
int64_t         aurora_duration_to_seconds(AuroraDuration* dur);
int64_t         aurora_duration_to_millis(AuroraDuration* dur);
int64_t         aurora_duration_to_micros(AuroraDuration* dur);
void            aurora_duration_free(AuroraDuration* dur);

AuroraTimeZone* aurora_timezone_local(void);
AuroraTimeZone* aurora_timezone_utc(void);
const char*     aurora_timezone_name(AuroraTimeZone* tz);
int             aurora_timezone_offset(AuroraTimeZone* tz);
void            aurora_timezone_free(AuroraTimeZone* tz);

#ifdef __cplusplus
}
#endif
