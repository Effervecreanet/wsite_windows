#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>

#include "wsite_content.h"
#include "wsite_nv.h"
#include "wsite_request.h"
#include "wsite_util.h"
#include "wsite_log.h"

extern struct entry entries[];


/* Transforms a time_t value intime to HTTP date format */

int time_to_httpdate(char *http_date, time_t *intime) {
  struct tm *intmv;

  intmv = gmtime(intime);

  memset(http_date, 0, HTTP_STRING_DATE_SIZE);

  if (strftime((char *)http_date, HTTP_STRING_DATE_SIZE,
               "%a, %d %b %Y %H:%M:%S GMT", intmv) == 0)
    return -1;

  return 1;
}

/* Client device is mobile or not */

int device_mobile(char *user_agent) {

  if (*user_agent == '\0')
    return -1;

  if (strstr(user_agent, " Mobile") == NULL)
    return -1;

  return 1;
}

/* Tranforme If-Modified-Since client value to camparable time value */

int modified_since(char *usrsince, time_t *since_i) {
  struct tm tmv_clt;

  if (strlen(usrsince) > HTTP_STRING_DATE_SIZE)
    return -1;

  memset(&tmv_clt, 0, sizeof(struct tm));

  if (strftime(usrsince, HEADER_VALUE_MAX_SIZE, "%a, %d %b %Y %H:%M:%S GMT",
               &tmv_clt) == 0)
    return -1;

  *since_i = mktime(&tmv_clt);

  return 1;
}

/* Return mtime for specified resource */

time_t resource_lastmod(char *filename) {
  struct stat st;
  int ret;

  memset(&st, 0, sizeof(struct stat));
  ret = stat(filename, &st);
  if (ret < 0) {
    fprintf(stderr, "Error in file %s at %d with stat (%s)\n.", __FILE__,
            __LINE__, strerror(errno));
    return -1;
  }
  return st.st_mtime;
}

/* Should be renamed qmark(char *request_resource) */

int facebook(char *request_resource) {
  char *qmark;

  if ((qmark = strchr(request_resource, '?')) == NULL)
    return 0;

  *qmark = '\0';

  return 1;
}

/* Check whether request resource is a valid server resource ie. firewalling */

int_fast16_t match_resource(char *request_resource) {
  int_fast16_t i = 0;

  if (*request_resource == '/' && *(request_resource + 1) == '\0') {
    strcat(request_resource, ROOT_PAGE);
    return 0;
  }

  ++request_resource;

  do {
    if (strncmp(request_resource, entries[i].resource,
                REQUEST_LINE_RESOURCE_MAX_SIZE) == 0)
      return i;
  } while (entries[++i].resource[0] != '\0');

  return -1;
}

/* Check HTTP client version is right */

int http_version_ok(char *version) {
  if (strncmp(version, HTTP_VERSION_1_1, sizeof(HTTP_VERSION_1_1)) == 0)
    return 1;
  if (strncmp(version, HTTP_VERSION_1_0, sizeof(HTTP_VERSION_1_0)) == 0)
    return 1;
  return -1;
}
