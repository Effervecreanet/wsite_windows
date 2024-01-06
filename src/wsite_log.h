
#define WSITE_LOG_FILENAME_EXT ".log"
#define LOG_DATE_NOW_SIZE  sizeof("01.Jan.1970:00:00:00")

extern char log_date_now[LOG_DATE_NOW_SIZE];
extern FILE *fp_log;
extern struct tm *tmv;

int fopen_log(const char *filename);
int log_listen(const char *server_addr, const unsigned short server_port);
void log_client_hostserv_name(char *hostserv_name);
void set_log_date_now(void);
