#include <stdint.h>
#include <time.h>

#define HTTP_STRING_DATE_SIZE sizeof("Mon, 01 Jan 1970 00:00:00 GMT")
#define HTTP_VERSION_1_0 "HTTP/1.0"
#define HTTP_VERSION_1_1 "HTTP/1.1"

int device_mobile(char *user_agent);
int modified_since(char *usrsince, time_t *since_i);
int http_version_ok(char *version);
time_t resource_lastmod(char *filename);
int facebook(char *request_resource);
int_fast16_t match_resource(char *request_resource);
int time_to_httpdate(char *http_date, time_t *intime);
