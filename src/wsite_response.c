#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <netinet/in.h>

#include "wsite_content.h"
#include "wsite_nv.h"
#include "wsite_request.h"
#include "wsite_response.h"
#include "wsite_util.h"

extern struct tm *tmv;

/* Create the server HTTP2HTTPS or HTTPS status line */

void create_status_response_http2https(struct status_line *statusline) {
  switch (statusline->code_h) {
  case HTTP_CODE_STATUS_NOT_MODIFIED:
    statusline->code_s = HTTP_CODE_STATUS_NOT_MODIFIED_STR;
    statusline->string = HTTP_STRING_STATUS_NOT_MODIFIED;
    break;
  case HTTP_CODE_STATUS_MOVED_PERMANENTLY:
    statusline->code_s = HTTP_CODE_STATUS_MOVED_PERMANENTLY_STR;
    statusline->string = HTTP_STRING_STATUS_MOVED_PERMANENTLY;
    break;
  case HTTP_CODE_STATUS_PARTIAL_CONTENT:
    statusline->code_s = HTTP_CODE_STATUS_PARTIAL_CONTENT_STR;
    statusline->string = HTTP_STRING_STATUS_PARTIAL_CONTENT;
    break;
  default:
    statusline->code_s = HTTP_CODE_STATUS_OK_STR;
    statusline->string = HTTP_STRING_STATUS_OK;
    statusline->code_h = HTTP_CODE_STATUS_OK;
    break;
  }

  statusline->version = HTTP_VERSION_1_1;

  return;
}

void create_status_response_https(struct status_line *statusline) {
  switch (statusline->code_h) {
  case HTTP_CODE_STATUS_NOT_MODIFIED:
    statusline->code_s = HTTP_CODE_STATUS_NOT_MODIFIED_STR;
    statusline->string = HTTP_STRING_STATUS_NOT_MODIFIED;
    break;
  case HTTP_CODE_STATUS_MOVED_PERMANENTLY:
    statusline->code_s = HTTP_CODE_STATUS_MOVED_PERMANENTLY_STR;
    statusline->string = HTTP_STRING_STATUS_MOVED_PERMANENTLY;
    break;
  case HTTP_CODE_STATUS_PARTIAL_CONTENT:
    statusline->code_s = HTTP_CODE_STATUS_PARTIAL_CONTENT_STR;
    statusline->string = HTTP_STRING_STATUS_PARTIAL_CONTENT;
    break;
  default:
    statusline->code_s = HTTP_CODE_STATUS_OK_STR;
    statusline->string = HTTP_STRING_STATUS_OK;
    statusline->code_h = HTTP_CODE_STATUS_OK;
    break;
  }

  statusline->version = HTTP_VERSION_1_1;

  return;
}

/* Create wanted_resource for mobile or desktop or whatever device */

int create_wanted_resource(struct wanted_resource *wanted_res,
                           struct header_nv hdr_nv[HEADER_NV_MAX_SIZE],
                           const char *type) {
  int iname;

  if ((iname = nv_find_name_client(hdr_nv, "User-Agent")) < 0)
    return -1;

  if (strcmp(type, "text/html") == 0) {
    if (device_mobile(hdr_nv[iname].value.v) > 0)
      wanted_res->pdevdir = HTML_DIRECTORY_MOBILE;
    else
      wanted_res->pdevdir = HTML_DIRECTORY_DESKTOP;
  } else
    wanted_res->pdevdir = DIRECTORY_ALL;

  if (strcmp(type, "video/mp4") == 0) {

    /* Treat the Range case espcially for ms edge browser */

    if ((iname = nv_find_name_client(hdr_nv, "Range")) > 0) {
      if (strcmp(hdr_nv[iname].value.v, "bytes=0-") == 0) {
        wanted_res->range_start = 0;
        wanted_res->range_end = 1024;
        wanted_res->range = true;
      } else if (strncmp(hdr_nv[iname].value.v,
                         "bytes=", sizeof("bytes=") - 1) == 0) {
        char *p, *q;

        p = hdr_nv[iname].value.v + 6;
        if (*p == '\0' || *(p + 1) == '\0')
          return -1;

        q = strchr(p, '-');
        if (q == NULL) {
          return -1;
        }
        *q++ = '\0';

        wanted_res->range_start = (off_t)strtoul(p, NULL, 10);

        if (wanted_res->range_start == 0)
          return -1;

        if (*q != '\0') {
          wanted_res->range_end = (off_t)strtoul(q, NULL, 10);

          if (wanted_res->range_end == 0)
            return -1;

          ++(wanted_res->range_end);
        }
        wanted_res->range = true;
      } else {
        wanted_res->range = false;
      }
    } else {
      wanted_res->range = false;
    }
  }

  /* Client header request contains If-Modified-Since field */

  if ((iname = nv_find_name_client(hdr_nv, "If-Modified-Since")) > -1)
    modified_since(hdr_nv[iname].value.v, &wanted_res->modified_i);

  strcpy(wanted_res->local_resource, wanted_res->pdevdir);

  return 1;
}

/* Create the server header name/value fields */

uint8_t create_header_nv_response(struct header_nv hdr_nv[HEADER_NV_MAX_SIZE],
                                  time_t lastmod, bool range, const char *type,
                                  const char *lang, char *cookie) {
  uint8_t i = 0;
  time_t now = time(NULL);

  hdr_nv[i].name.wsite = HTTP_HEADER_CACHE_CONTROL;
  hdr_nv[i++].value.pv = HTTP_HEADER_CACHE_CONTROL_VALUE;

  hdr_nv[i].name.wsite = HTTP_HEADER_CONNECTION;
  hdr_nv[i++].value.pv = HTTP_HEADER_CONNECTION_VALUE;

  hdr_nv[i].name.wsite = HTTP_HEADER_DATE;
  strftime(hdr_nv[i++].value.v, HTTP_STRING_DATE_SIZE,
			"%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

  hdr_nv[i].name.wsite = HTTP_HEADER_SERVER;
  hdr_nv[i++].value.pv = HTTP_HEADER_SERVER_VALUE;

  if (strcmp(type, "text/html") == 0) {
    hdr_nv[i].name.wsite = HTTP_HEADER_VARY;
    hdr_nv[i++].value.pv = HTTP_HEADER_VARY_VALUE;
  }

  hdr_nv[i].name.wsite = HTTP_HEADER_ALLOW;
  hdr_nv[i++].value.pv = HTTP_HEADER_ALLOW_VALUE;

  hdr_nv[i].name.wsite = HTTP_HEADER_KEEP_ALIVE;
  hdr_nv[i++].value.pv = HTTP_HEADER_KEEP_ALIVE_VALUE;

  if (lang) {
    hdr_nv[i].name.wsite = HTTP_HEADER_CONTENT_LANGUAGE;
    hdr_nv[i++].value.pv = lang;
  }

  hdr_nv[i++].name.wsite = HTTP_HEADER_CONTENT_LENGTH;

  if (range == true)
    hdr_nv[i++].name.wsite = HTTP_HEADER_CONTENT_RANGE;

  hdr_nv[i].name.wsite = HTTP_HEADER_CONTENT_TYPE;
  strcpy(hdr_nv[i++].value.v, type);

  hdr_nv[i].name.wsite = HTTP_HEADER_EXPIRES;
  time(&now);
  now += 3600 * 24 * 360;
  time_to_httpdate(hdr_nv[i++].value.v, &now);

  if (cookie != NULL && *cookie != '\0') {
    hdr_nv[i].name.wsite = HTTP_HEADER_SET_COOKIE;
    strcpy(hdr_nv[i++].value.v, cookie);
  }

  if (lastmod > 0) {
    hdr_nv[i].name.wsite = HTTP_HEADER_LAST_MODIFIED;
    time_to_httpdate(hdr_nv[i++].value.v, &lastmod);
  } else {
    hdr_nv[i++].name.wsite = NULL;
  }

  return i;
}
