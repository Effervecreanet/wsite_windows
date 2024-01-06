
#define WSITE_FILENAME_CONFIG                                                  \
  "wsite.conf" /* Filename holding the configuration                           \
                * (http hostname, port, ...) wsite will                        \
                * run with.                                                    \
                */

extern char in_hostname[NI_MAXHOST];
extern char nakedhostname[NI_MAXHOST];
extern char listen_port[sizeof("65535")];
extern char log_filename[NI_MAXHOST + 4];
