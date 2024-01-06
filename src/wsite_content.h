#define DOMAIN_NAME_MAX_SIZE 1025
#define WEBSITE_DEFAULT_LISTEN_PORT 80

#define DIRECTORY_ALL "./"
#define HTML_DIRECTORY_DESKTOP "./html/"
#define HTML_DIRECTORY_MOBILE "./html_mobile/"
#define SERVER_HTTP_VERSION "HTTP/1.1"

#define ROOT_PAGE "Accueil"

struct entry {
  const char *resource;
  const char *type;
  const char *lang;
  const char *type_opt;
};

extern struct entry entries[];
