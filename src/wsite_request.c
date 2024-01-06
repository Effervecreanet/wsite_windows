#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "wsite_content.h"

#include "wsite_nv.h"
#include "wsite_request.h"

#define MAX_SP 254
#define HTTPS_PROTOCOL "https://"

extern struct entry entries[];
extern char wwwhostname[NI_MAXHOST];

static int recv_space(int infdusr, char *nospace);

/* Discards spaces, set nospace to first non-space char received */

static int recv_space(int infdusr, char *nospace) {
  char c;
  uint8_t maxsp;

  for (maxsp = 0; recv(infdusr, &c, 1, 0) == 1 && maxsp < MAX_SP; ++maxsp) {
    if (c != ' ' && c != '\t') {
      *nospace = c;
      break;
    }
  }

  if (maxsp == MAX_SP)
    return -1;

  return 1;
}

/* Mini URL handler for URL sent by HTTP proxy */

int absoluteURI_in(char resource[REQUEST_LINE_RESOURCE_MAX_SIZE]) {
  char *resource_tmp;

  resource_tmp = resource;

  if (strncasecmp(resource_tmp, HTTPS_PROTOCOL, sizeof(HTTPS_PROTOCOL) - 1) !=
      0)
    return -1;

  resource_tmp += sizeof(HTTPS_PROTOCOL) - 1;

  if (*resource_tmp == '\0')
    return -1;

  if (strncasecmp(resource_tmp, wwwhostname, strlen(wwwhostname)) != 0)
    return -1;

  resource_tmp += strlen(wwwhostname);

  if (*resource_tmp == '\0')
    strcpy(resource, "/");
  else
    memmove(resource, resource_tmp, strlen(resource_tmp) + 1);

  /* printf("resource: %s\n", resource); */

  return 1;
}

/* Receive and mem client name/value header fields if rightly sent */

int recv_header_nv(int infdusr, struct header_nv hdr_nv[HEADER_NV_MAX_SIZE]) {
  ssize_t recvb;
  uint8_t maxsp;
  size_t hlen = 0;
  int8_t nvnb = 0;
  char c = 0;

  do {
    for (c = recvb = 0; recvb < HEADER_NAME_MAX_SIZE; recvb++, hlen++) {
      if (recv(infdusr, &c, 1, 0) != 1)
        return -1;

      if (isalpha(c) != 0 || c == '-' || c == '.') {
        hdr_nv[nvnb].name.client[recvb] = c;
        continue;
      }
      switch (c) {
      case ':':
        break;
      case '\n':
        goto crlfcrlf;
      case '\r':
        if (recv(infdusr, &c, 1, 0) == 1 && c == '\n')
          goto crlfcrlf;
        return -1;
      default:
        return -1;
      }
      break;
    }

    if (recvb == HEADER_NAME_MAX_SIZE)
      return -1;

    if (recv(infdusr, &c, 1, 0) == 1 && (c != ' ' && c != '\t'))
      return -1;

    for (maxsp = 0; recv(infdusr, &c, 1, 0) == 1 && maxsp < MAX_SP; ++maxsp) {
      switch (c) {
      case ' ':
      case '\t':
        continue;
      default:
        hdr_nv[nvnb].value.v[0] = c;
        break;
      }
      break;
    }

    if (maxsp == MAX_SP)
      return -1;

    for (c = 0, recvb = 1; recvb < HEADER_VALUE_MAX_SIZE; recvb++, hlen++) {
      if (recv(infdusr, &c, 1, 0) != 1)
        return -1;

      switch (c) {
      case '\r':
      case '\n':
        break;
      default:
        hdr_nv[nvnb].value.v[recvb] = c;
        continue;
      }
      break;
    }

    if (recvb == HEADER_VALUE_MAX_SIZE)
      return -1;

    if (c == '\r' && recv(infdusr, &c, 1, 0) != 1 && c != '\n')
      return -1;
    /*
       printf("header_name: %s header_value: %s\n",
       hdr_nv[nvnb].name.client, hdr_nv[nvnb].value.v);
     */
  } while (hlen < HEADER_MAX_SIZE && ++nvnb < HEADER_NV_MAX_SIZE);

crlfcrlf:
  if (nvnb == HEADER_NV_MAX_SIZE || hlen == HEADER_MAX_SIZE)
    return -1;

  return 1;
}

/* Receive HTTP client request line if rightly sent */

int recv_request_line(int infdusr, struct request_line *rline) {
  int recvb = 0;
  char c = 0;

  do {
    if (recv(infdusr, &c, 1, 0) != 1) {
      memset(rline, 0, sizeof(struct request_line));
      return -1;
    } else if (c == ' ' || c == '\t') {
      if (recv_space(infdusr, &rline->resource[0]) < 0)
        return -1;
      break;
    } else
      rline->method[recvb] = c;
  } while (++recvb <= REQUEST_LINE_METHOD_MAX_SIZE);

  for (c = 0, recvb = 1; recvb < REQUEST_LINE_RESOURCE_MAX_SIZE;) {
    if (recv(infdusr, &c, 1, 0) != 1) {
      memset(rline, 0, sizeof(struct request_line));
      return -1;
    } else if (c == ' ' || c == '\t') {
      if (recv_space(infdusr, &rline->version[0]) < 0)
        return -1;
      break;
    } else
      rline->resource[recvb++] = c;
  }

  if (recv(infdusr, rline->version + 1, REQUEST_LINE_VERSION_MAX_SIZE - 2, 0) !=
      REQUEST_LINE_VERSION_MAX_SIZE - 2)
    return -1;

     /*
     printf("st:%s:%s:%s:\n", rline->method, rline->resource,
     rline->version);
     */

  if (recv(infdusr, &c, 1, 0) != 1 && (c != '\r' && c != '\n'))
    return -1;
  if (c == '\r' && recv(infdusr, &c, 1, 0) != 1 && c != '\n')
    return -1;

  return 1;
}
