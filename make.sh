#!/bin/sh
echo gcc -O3 src/wsite_content.c src/wsite_handler.c src/wsite_log.c src/wsite_main.c src/wsite_nv.c src/wsite_request.c src/wsite_response.c src/wsite_send.c src/wsite_util.c -o wsite_https_effervecreanet
gcc -O3 src/wsite_content.c src/wsite_handler.c src/wsite_log.c src/wsite_main.c src/wsite_nv.c src/wsite_request.c src/wsite_response.c src/wsite_send.c src/wsite_util.c -o wsite_https_effervecreanet
