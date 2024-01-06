#include <netdb.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

struct status_line {
  const char *code_s;
  const char *string;
  const char *version;
  unsigned short code_h;
  char pad[6];
};

struct wanted_resource {
  time_t modified_i;
  time_t lastmodified_i;
  off_t range_start;
  off_t range_end;
  char *pdevdir;
  char local_resource[LOCAL_RESOURCE_MAX_SIZE];
  bool range;
  char pad;
};

struct wsite_response {
  struct entry *entry;
  struct status_line statusline;
  struct wanted_resource wanted_resource;
  struct header_nv hdr_nv[HEADER_NV_MAX_SIZE];
  char redirect_hypertext_note[sizeof("<a href=\"%s\">%s</i></a>") +
                               NI_MAXHOST + HEADER_VALUE_MAX_SIZE];
  uint8_t send_content : 1;
  unsigned long long pad : 31;
};

/* void (*create_status_response)(struct status_line *statusline); */
/* void create_status_response_http2https(struct status_line *statusline); */
void create_status_response_https(struct status_line *statusline);

int create_wanted_resource(struct wanted_resource *wanted_res,
                           struct header_nv hdr_nv[HEADER_NV_MAX_SIZE],
                           const char *type);

uint8_t create_header_nv_response(struct header_nv hdr_nv[HEADER_NV_MAX_SIZE],
                                  time_t lastmod, bool range, const char *lang,
                                  const char *type, char *cookie);
