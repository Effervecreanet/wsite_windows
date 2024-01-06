#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "wsite_content.h"


extern struct entry entries[];

#include "wsite_log.h"
#include "wsite_main.h"
#include "wsite_nv.h"
#include "wsite_request.h"
#include "wsite_response.h"
#include "wsite_send.h"
#include "wsite_util.h"
#include "wsite_handler.h"

extern FILE *fp_log; /* File wsite records connections */

char log_filename[NI_MAXHOST + 4]; /* Filename wsite uses to log connections */
char wwwhostname[NI_MAXHOST];      /* Website internet domain name */
char nakedhostname[NI_MAXHOST];
/* XXX: Service port wsite waits connections */
char listen_port[sizeof("65535")];

int bind_socket_server(int *s, struct sockaddr_in *sinaddr);
ssize_t bind_server(int *s);
int create_socket_server(int *s);
int accept_connection(int s, struct sockaddr_in *saddrin_client);

/*
 * Accept an incoming connection and set socket recv timeout to 2.4 secs
 */

int accept_connection(int s, struct sockaddr_in *saddrin_client) {
  int infdusr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  memset(saddrin_client, 0, sizeof(struct sockaddr_in));

  infdusr = accept(s, (struct sockaddr *)saddrin_client, &addrlen);

  if (infdusr < 0 && errno != EAGAIN) {
    fprintf(stderr, "Error in file %s at line %d with accept (%s).\n", __FILE__,
            __LINE__, strerror(errno));
    return -1;
  }

  return infdusr;
}

/*
 * Bind socket to IPv4 provided address
 */

int bind_socket_server(int *s, struct sockaddr_in *sinaddr) {
  int ret_ecode;
  struct timeval tval;

  ret_ecode = bind(*s, (struct sockaddr *)sinaddr, sizeof(struct sockaddr_in));
  if (ret_ecode < 0) {
    fprintf(stderr, "Can't bind server socket (%s).\n", strerror(errno));
    return -1;
  }

  memset(&tval, 0, sizeof(struct timeval));
  tval.tv_sec = 6;
  setsockopt(*s, SOL_SOCKET, SO_RCVTIMEO, &tval, sizeof(struct timeval));
  setsockopt(*s, SOL_SOCKET, SO_SNDTIMEO, &tval, sizeof(struct timeval));

  return 1;
}

/*
 * Create a socket server, TCP/IP one
 */

int create_socket_server(int *s) {
  *s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (*s < 0) {
    fprintf(stderr, "Can't create server socket (%s).\n", strerror(errno));
    return -1;
  }

  return 1;
}

static void tcp_close(int s) {
  shutdown(s, SHUT_RDWR);
  close(s);
  return;
}

void usage(const char *progname) {
  fprintf(stderr,
          "usage: %s [-v] [-d] -a listen_addr -h http_hostname -n"
          " naked_domain_name -p listen_port -l log_filename\n",
          progname);

  return;
}

void version(void) {
  fprintf(stderr, "%s: %s ( %s )\n", PACKAGE_STRING,
                                PACKAGE_URL,
				PACKAGE_BUGREPORT);
  exit(4);
}

int main(int argc, char *argv[]) {
  struct sockaddr_in usrinaddr;
  struct header_nv hdr_nv[HEADER_NV_MAX_SIZE];
  uint8_t usrfdcnt = 0;
  struct wsite_response wsresp;
  struct request_line rline;
  int32_t infdusr1, infdusr2;
  int_fast16_t i;
  int ch;
  int s, s_http2https;
  pid_t pid = 1;
  char logbuff[2048];
  bool daemonize = false;
  struct sockaddr_in sain_addr;
  struct sockaddr_in sain_addr_http2https;
  int ret;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

  memset(&sain_addr, 0, sizeof(struct sockaddr_in));
  memset(&sain_addr_http2https, 0, sizeof(struct sockaddr_in));

  if (argc < 11) {
    usage(argv[0]);
    exit(3);
  }

  while ((ch = getopt(argc, argv, "vda:h:n:p:l:")) != -1) {
    switch (ch) {
    case 'v':
      version();
      break;
    case 'd':
      daemonize = true;
      break;
    case 'a':
      inet_aton(optarg, &sain_addr.sin_addr);
      inet_aton(optarg, &sain_addr_http2https.sin_addr);
      break;
    case 'h':
      strncpy(wwwhostname, optarg, NI_MAXHOST - 1);
      break;
    case 'n':
      strncpy(nakedhostname, optarg, NI_MAXHOST - 1);
      break;
    case 'p':
      sain_addr.sin_port = htons(atoi(optarg));
      sain_addr_http2https.sin_port = htons(80);
      break;
    case 'l':
      memset(log_filename, 0, NI_MAXHOST + 4);
      strncpy(log_filename, optarg, NI_MAXHOST + 3);
      if ((*(log_filename + strlen(log_filename) - 1)) == '/') {
	usage(argv[0]);
	exit(4);
      }
      break;
    default:
      usage(argv[0]);
      exit(1);
    }
  }

  if (create_socket_server(&s) < 0)
    exit(2);
  /*
  if (create_socket_server(&s_http2https) < 0)
    exit(2);
  */

  sain_addr.sin_family = AF_INET;
  sain_addr_http2https.sin_family = AF_INET;

  if (bind_socket_server(&s, &sain_addr) < 0)
    exit(3);

  /*
  if (bind_socket_server(&s_http2https, &sain_addr_http2https) < 0)
    exit(3);
  */

  listen(s, 24);
  /* listen(s_http2https, 24); */

  fprintf(stdout, "Listening on %s with port %d\n",
          inet_ntoa(sain_addr.sin_addr), ntohs(sain_addr.sin_port));
  fprintf(stdout, "Listening on %s with port %d\n",
          inet_ntoa(sain_addr_http2https.sin_addr),
          ntohs(sain_addr_http2https.sin_port));


  fopen_log(log_filename);

  fprintf(fp_log, "Listening on %s with port %d\n",
          inet_ntoa(sain_addr.sin_addr), ntohs(sain_addr.sin_port));
  fflush(fp_log);

  if (daemonize)
    daemon(1, 0);

  /*
  if (fork() == 0) {
    create_status_response = create_status_response_http2https;

    for (;;) {
      if (((usrfdcnt++ + 7) % 44) == 0) {
        system("clear");
        usrfdcnt = 0;
      }

      infdusr1 = accept_connection(s_http2https, &usrinaddr);
      if (infdusr1 < 0) {
        continue;
      } else {
        pid = fork();
        if (pid != 0) {
          close(infdusr1);
          continue;
        }
      }

    next_http_req_http2https:
      memset(logbuff, 0, sizeof(logbuff));
      set_log_date_now();
      sprintf(logbuff, "%s:oncoming:", log_date_now);

      memset(&rline, 0, sizeof(struct request_line));
      if (recv_request_line(infdusr1, &rline) < 0) {
        strcat(logbuff, "bad_request:cannot_receive_request_line\n");
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr1);
        exit(0);
      }
      if (http_version_ok(rline.version) < 0) {
        strcat(logbuff, "bad_request:wrong_http_version\n");
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr1);
        exit(0);
      }
      if (absoluteURI_in(rline.resource) > 0)
        rline.absURIin = 1;

      if (facebook(rline.resource) > 0)
        rline.facebook = 1;

      if ((i = match_resource(rline.resource)) < 0) {
        strcat(logbuff, "bad_request:unknown_requested_resource\n");
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr1);
        exit(0);
      }

      printf("%s Oncoming connection for %s\n", log_date_now, rline.resource);
      fflush(stdout);

      memset(&wsresp, 0, sizeof(struct wsite_response));
      wsresp.entry = &entries[i];

      if (strcmp(rline.method, HTTP_METHOD_GET) == 0) {
        wsresp.send_content = 1;
      } else if (strcmp(rline.method, HTTP_METHOD_HEAD) == 0) {
        wsresp.send_content = 0;
      } else {
        strcat(logbuff, "bad_request:unknown_method\n");
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr1);
        exit(0);
      }

      memset(&hdr_nv, 0, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
      if (recv_header_nv(infdusr1, hdr_nv) < 0) {
        strcat(logbuff, "bad_request:cannot_receive_http_header\n");
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr1);
        exit(0);
      }
      if (wsite_handle(&wsresp, hdr_nv, &rline) > 0) {
        ret = wsite_send(infdusr1, &wsresp);
        if (ret > 0) {
          int iname;

          strcat(logbuff, wsresp.entry->resource);
          strcat(logbuff, "sent_ok \"");
          strcat(logbuff, wsresp.entry->resource);
          strcat(logbuff, "\" \"");

          if (ret > 2)
            sprintf(&logbuff[strlen(logbuff)], "range(%d)\" ", ret);

          if ((iname = nv_find_name_client(hdr_nv, "User-Agent")) > -1) {
            strcat(logbuff, hdr_nv[iname].value.v);
            strcat(logbuff, "\"");
            if ((iname = nv_find_name_client(hdr_nv, "Referer")) > -1) {
              strcat(logbuff, " \"");
              strcat(logbuff, hdr_nv[iname].value.v);
              strcat(logbuff, "\"");
            }
          }

          strcat(logbuff, "\n");
          fprintf(fp_log, "%s", logbuff);
        } else {
          strcat(logbuff, "bad_request:send_error\n");
          fprintf(fp_log, "%s", logbuff);
        }
      } else {
        strcat(logbuff, "bad_request:handle_error\n");
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr1);
        exit(0);
      }

      fflush(fp_log);
      goto next_http_req_http2https;
    }
  } else {
  */
    /* create_status_response = create_status_response_https; */
    for (;;) {
      if (((usrfdcnt++ + 7) % 44) == 0) {
        system("clear");
        usrfdcnt = 0;
      }

      infdusr2 = accept_connection(s, &usrinaddr);
      if (infdusr2 < 0) {
        continue;
      } else {
        pid = fork();
        if (pid != 0) {
          close(infdusr2);
          continue;
        }
      }

    next_http_req_https:
      memset(&rline, 0, sizeof(struct request_line));
      if (recv_request_line(infdusr2, &rline) < 0) {
        memset(logbuff, 0, sizeof(logbuff));
	set_log_date_now();
	sprintf(logbuff, "%s - - [%s] \"\" 400 0\n",

			inet_ntoa(usrinaddr.sin_addr),
			log_date_now);
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr2);
        exit(0);
      }
      if (http_version_ok(rline.version) < 0) {
        memset(logbuff, 0, sizeof(logbuff));
	set_log_date_now();
	sprintf(logbuff, "%s - - [%s] \"\" 400 0\n",

			inet_ntoa(usrinaddr.sin_addr),
			log_date_now);
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr2);
        exit(0);
      }
      if (absoluteURI_in(rline.resource) > 0)
        rline.absURIin = 1;

      if (facebook(rline.resource) > 0)
        rline.facebook = 1;

      if ((i = match_resource(rline.resource)) < 0) {
        memset(logbuff, 0, sizeof(logbuff));
	set_log_date_now();
        sprintf(logbuff, "%s - - [%s] \"%s %s %s\" 404 0\n",
			inet_ntoa(usrinaddr.sin_addr),
			log_date_now,
			rline.method,
			rline.resource,
			rline.version);
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr2);
        exit(0);
      }

      printf("%s Oncoming connection for %s\n", log_date_now, rline.resource);
      fflush(stdout);

      memset(&wsresp, 0, sizeof(struct wsite_response));
      wsresp.entry = &entries[i];

      if (strcmp(rline.method, HTTP_METHOD_GET) == 0) {
        wsresp.send_content = 1;
      } else if (strcmp(rline.method, HTTP_METHOD_HEAD) == 0) {
        wsresp.send_content = 0;
      } else {
        memset(logbuff, 0, sizeof(logbuff));
	set_log_date_now();
        sprintf(logbuff, "%s - - [%s] \"%s %s %s\" 405 0\n",

			inet_ntoa(usrinaddr.sin_addr),
			rline.method,
			rline.resource,
			rline.version);
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr2);
        exit(0);
      }

      memset(&hdr_nv, 0, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
      if (recv_header_nv(infdusr2, hdr_nv) < 0) {
	memset(logbuff, 0, sizeof(logbuff));
	set_log_date_now();

        sprintf(logbuff, "%s - - [%s] \"%s %s %s\" 400 0\n",

			inet_ntoa(usrinaddr.sin_addr),
			log_date_now,
			rline.method,
			rline.resource,
			rline.version);
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr2);
        exit(0);
      }
      if (wsite_handle(&wsresp, hdr_nv, &rline) > 0) {
        ret = wsite_send(infdusr2, &wsresp);

        if (ret > 0) {
		int iname;
		memset(logbuff, 0, sizeof(logbuff));
		set_log_date_now();
		sprintf(logbuff, "%s - - [%s] \"%s %s %s\" 200 %d",
			inet_ntoa(usrinaddr.sin_addr),
			log_date_now,
			rline.method,
			rline.resource,
			rline.version,
			ret);
		strcat(logbuff, "\n");
		fprintf(fp_log, "%s", logbuff);
        }
      } else {
	memset(logbuff, 0, sizeof(logbuff));
	set_log_date_now();
        sprintf(logbuff, "%s - - [%s] \"%s %s %s\" 500 0\n",

			inet_ntoa(usrinaddr.sin_addr),
			log_date_now,
			rline.method,
			rline.resource,
			rline.version);
        fprintf(fp_log, "%s", logbuff);
        tcp_close(infdusr2);
        exit(0);
      }

      fflush(fp_log);
      goto next_http_req_https;
    }
  /* } */
  return 1;
}
