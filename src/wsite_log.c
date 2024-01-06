#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "wsite_log.h"

struct tm *tmv;
FILE *fp_log;
char log_date_now[LOG_DATE_NOW_SIZE];

void log_client_hostserv_name(char *hostserv_name);
int log_listen(const char *server_addr, const unsigned short server_port);

/* Set log_date_now to now in a short wsite log format still human readable */

void set_log_date_now(void) {
  time_t now;

  memset(&now, 0, sizeof(time_t));
  time(&now);
  tmv = gmtime(&now);

  memset(log_date_now, 0, LOG_DATE_NOW_SIZE);
  strftime(log_date_now, LOG_DATE_NOW_SIZE, "%d/%b/%Y:%T -0600", tmv);

  return;
}

/* Write a start message to log file, flush it. */

int log_listen(const char *server_addr, const unsigned short server_port) {
  int retv;

  retv = fprintf(fp_log, "\nListening on address %s with port %d\n",
                 server_addr, server_port);

  if (retv < 0) {
    fprintf(stderr, "Error in file %s at line %d with fprintf (%s)\n", __FILE__,
            __LINE__, strerror(errno));
    return -1;
  }

  fflush(fp_log);

  return 1;
}

/* fopen log file, return -1 if unable to */

int fopen_log(const char *filename) {
  fp_log = fopen(filename, "a+");
  if (fp_log == NULL) {
    fprintf(stderr, "Can't open log filename (%s) in %s at %d (%s).\n",
            filename, __FILE__, __LINE__, strerror(errno));
    return -1;
  }

  return 1;
}
