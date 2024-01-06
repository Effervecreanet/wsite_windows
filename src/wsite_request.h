#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_HEAD "HEAD"

#define REQUEST_LINE_METHOD_MAX_SIZE 5
#define REQUEST_LINE_VERSION_MAX_SIZE 9
#define REQUEST_LINE_RESOURCE_MAX_SIZE 512

#define LOCAL_RESOURCE_MAX_SIZE 366

struct request_line {
  char method[REQUEST_LINE_METHOD_MAX_SIZE];
  char resource[REQUEST_LINE_RESOURCE_MAX_SIZE];
  char version[REQUEST_LINE_VERSION_MAX_SIZE];
  uint8_t absURIin : 1;
  uint8_t facebook : 1; /* qmark should be */
  uint8_t pad : 6;
};

int recv_request_line(int infdusr, struct request_line *request_line);
int recv_header_nv(int infdusr, struct header_nv hdr_nv[HEADER_NV_MAX_SIZE]);
int absoluteURI_in(char resource[REQUEST_LINE_RESOURCE_MAX_SIZE]);
