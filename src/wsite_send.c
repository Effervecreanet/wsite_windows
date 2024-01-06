#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wsite_content.h"

#include "wsite_nv.h"
#include "wsite_request.h"
#include "wsite_response.h"
#include "wsite_send.h"

#define SEND_CRLF(S)                                                           \
  do {                                                                         \
    if (send(S, "\r\n", 2, 0) < 0)                                             \
      return -1;                                                               \
  } while (0)

/* Set Content-Lenght to the size of the file we send */

static off_t set_content_length(char content_length[HEADER_VALUE_MAX_SIZE],
                                const char *content_filename) {
  struct stat sb;

  memset(&sb, 0, sizeof(struct stat));
  if (stat(content_filename, &sb) < 0) {
    fprintf(stderr, "Error in file %s at line %d with stat (%s (%s)).\n",
            __FILE__, __LINE__, strerror(errno), content_filename);
    return -1;
  }
  memset(content_length, 0, HEADER_VALUE_MAX_SIZE);
  sprintf(content_length, "%lu", sb.st_size);

  return sb.st_size;
}

/* Send server header response */

static int wsite_send_header(int infdusr, struct status_line *statusline,
                             struct header_nv hdr_nv[HEADER_NV_MAX_SIZE]) {
  int8_t iname;

  if (send(infdusr, statusline->version, sizeof(SERVER_HTTP_VERSION) - 1, 0) <
      0)
    return -1;

  if (send(infdusr, " ", 1, 0) != 1)
    return -1;

  if (send(infdusr, statusline->code_s, strlen(statusline->code_s), 0) < 0)
    return -1;

  if (send(infdusr, " ", 1, 0) != 1)
    return -1;

  if (send(infdusr, statusline->string, strlen(statusline->string), 0) < 0)
    return -1;

  SEND_CRLF(infdusr);

  for (iname = 0; iname < HEADER_NV_MAX_SIZE; iname++) {
    if (hdr_nv[iname].name.wsite == NULL || *hdr_nv[iname].name.wsite == '\0')
      continue;
    if (send(infdusr, hdr_nv[iname].name.wsite,
             strlen(hdr_nv[iname].name.wsite), 0) < 0)
      return -1;

    if (send(infdusr, ": ", 2, 0) < 0)
      return -1;

    if (hdr_nv[iname].value.pv == NULL) {
      if (send(infdusr, hdr_nv[iname].value.v, strlen(hdr_nv[iname].value.v),
               0) < 0)
        return -1;
    } else {
      if (send(infdusr, hdr_nv[iname].value.pv, strlen(hdr_nv[iname].value.pv),
               0) < 0)
        return -1;
    }

    SEND_CRLF(infdusr);
  }

  SEND_CRLF(infdusr);

  return 1;
}

/* Send range data for specified resource */

static int wsite_send_content_range(int infdusr,
                                    struct wsite_response *wsresp) {
  ssize_t readb;
  int fd;
  char buffer[1024];
  ssize_t sent;
  ssize_t total_sent = 0;
  int8_t ret = 0;

  fd = open(wsresp->entry->resource, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error in file %s at line %d with open (%s).\n", __FILE__,
            __LINE__, strerror(errno));
    return -1;
  }

  if (lseek(fd, wsresp->wanted_resource.range_start, SEEK_SET) < 0) {
    close(fd);
    return -1;
  }

  do {
    readb = read(fd, buffer, 1024);
    if (readb == 0)
      break;

    if (wsresp->wanted_resource.range_end <=
        wsresp->wanted_resource.range_start) {
      close(fd);
      return 1;
    }

    wsresp->wanted_resource.range_start += readb;

    sent = send(infdusr, buffer, (size_t)readb, 0);
    total_sent += sent;
  } while (sent > 0);

  close(fd);

  if (total_sent > 0)
    return total_sent;

  return -1;
}

/* Send i) Desktop resource ii) mobile resource iii) device independent resource
 */

static int wsite_send_content(int infdusr, struct wsite_response *wsresp) {
  ssize_t readb;
  int fd;
  char buffer[1024];
  int ret = 0;
  int sent = 0;

  if (strcmp(wsresp->entry->type, "text/html") == 0) {
    fd = open(wsresp->wanted_resource.local_resource, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Error in file %s at line %d with open (%s).\n", __FILE__,
              __LINE__, strerror(errno));
      return -1;
    }
  } else {
    fd = open(wsresp->entry->resource, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Error in file %s at line %d with open (%s).\n", __FILE__,
              __LINE__, strerror(errno));
      return -1;
    }
  }

  do {
    readb = read(fd, buffer, 1024);
    if (readb <= 0)
      break;
    ret += readb;
    sent = send(infdusr, buffer, (size_t)readb, 0); 
  } while (sent > -1);

  close(fd);

  return ret;
}

/* Prepare to send full content or only header content according to
 * wsite_handler.c */

int wsite_send(int infdusr, struct wsite_response *wsresp) {
  int iname = 0, iname1 = 0, iname2 = 0;
  size_t rhn_len;

  iname1 = nv_find_name_wsite(wsresp->hdr_nv, "Content-Type");

  if (iname1 && wsresp->entry->type_opt != NULL)
    strcat(wsresp->hdr_nv[iname1].value.v, wsresp->entry->type_opt);

  switch (wsresp->statusline.code_h) {
  case HTTP_CODE_STATUS_MOVED_PERMANENTLY:
    iname = nv_find_name_wsite(wsresp->hdr_nv, "Content-Length");
    if (iname < 0)
      return -1;

    rhn_len = strlen(wsresp->redirect_hypertext_note);

    sprintf(wsresp->hdr_nv[iname].value.v, "%lu", rhn_len);

    if (wsite_send_header(infdusr, &wsresp->statusline, wsresp->hdr_nv) < 0)
      return -1;

    if (send(infdusr, wsresp->redirect_hypertext_note, rhn_len, 0) < 0)
      return -1;

    return 1;
  case HTTP_CODE_STATUS_PARTIAL_CONTENT:
    if ((iname1 = nv_find_name_wsite(wsresp->hdr_nv, "Content-Range")) > -1) {
      char filesize[HEADER_VALUE_MAX_SIZE];
      off_t fsize;

      iname2 = nv_find_name_wsite(wsresp->hdr_nv, "Content-Length");

      if (iname2 < 0)
        return -1;

      fsize = set_content_length(filesize, wsresp->entry->resource);
      if (fsize < 0)
        return -1;

      if (fsize < wsresp->wanted_resource.range_end)
        return -1;

      if (!wsresp->wanted_resource.range_end)
        wsresp->wanted_resource.range_end = fsize;

      memset(wsresp->hdr_nv[iname1].value.v, 0, HEADER_VALUE_MAX_SIZE);
      sprintf(wsresp->hdr_nv[iname1].value.v, "bytes %lu-%lu/",
              wsresp->wanted_resource.range_start,
              wsresp->wanted_resource.range_end - 1);

      strncat(wsresp->hdr_nv[iname1].value.v, filesize, HEADER_VALUE_MAX_SIZE);

      sprintf(wsresp->hdr_nv[iname2].value.v, "%lu",
              wsresp->wanted_resource.range_end -
                  wsresp->wanted_resource.range_start);
    } else
      return -1;
    break;
  case HTTP_CODE_STATUS_NOT_MODIFIED:
    wsresp->send_content = 0;
    break;
  case HTTP_CODE_STATUS_OK:
    iname1 = nv_find_name_wsite(wsresp->hdr_nv, "Content-Length");

    if (iname1 < 0)
      return -1;

    if (set_content_length(
            wsresp->hdr_nv[iname1].value.v,
            (const char *)wsresp->wanted_resource.local_resource) < 0)
      return -1;

    break;
  default:
    return -1;
  }

  if (wsite_send_header(infdusr, &wsresp->statusline, wsresp->hdr_nv) < 0)
    return -1;

  if (wsresp->send_content) {
    if (wsresp->wanted_resource.range == true) {
      return wsite_send_content_range(infdusr, wsresp);
    } else {
      return wsite_send_content(infdusr, wsresp); 
    }
  }

  return 1;
}
