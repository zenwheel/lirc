/****************************************************************************
 ** hw_arduinohttp.c ********************************************************
 ****************************************************************************
 *
 * Send signals to arduino over http
 * 
 * Copyright (C) 2010 Scott Jann <sjann@knight-rider.org>
 *
 * Distribute under GPL version 2 or later.
 *
 * The transmission over HTTP is a single GET of
 * /send
 * with a query string of a comma delimited list of pulse and
 * space pair durations in usec
 * i.e. http://<host>/send?9000,4500,600,300,600,300...
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "transmit.h"
#include "hw_default.h"

int arduinohttp_init()
{
	logprintf(LOG_INFO,"Initializing ArduinoHTTP: %s",hw.device);
	
    init_send_buffer();
	
	return(1);
}

int arduinohttp_deinit(void)
{
	return(1);
}

int arduinohttp_send(struct ir_remote *remote,struct ir_ncode *code)
{
	int length;
	lirc_t *signals;
	int i = 0;

	if(!init_send(remote,code)) {
		return 0;
	}

	length = send_buffer.wptr;
	signals = send_buffer.data;
    lirc_t freq = remote->freq ? remote->freq : DEFAULT_FREQ;
    
	if (length <= 0 || signals == NULL) {
		return 0;
	}

    char pairs[1024];
    *pairs = 0;
	for(i = 0; i < length; i++) {
        char tmp[16];
		int len = signals[i];
        sprintf(tmp, "%s%d", i == 0 ? "" : ",", len);
        strcat(pairs, tmp);
	}
	//if(length % 2 == 1)
    //    strcat(pairs, ",0");
    
    // send HTTP request
	int port = 80;
	char host[255];
    strcpy(host, hw.device);
	char *p = strchr(host, ':');
	if(p) {
		*(p++) = 0;
		port = atoi(p);
	}

	struct hostent *hp = gethostbyname(host);
	struct sockaddr_in sin;
	int httpStatus = 500;

	if(hp && hp->h_length == sizeof(sin.sin_addr)) {
		int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
                sin.sin_port = htons(port);
		if(connect(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) >= 0) {
			char buf[2048];
			sprintf(buf, "GET /send?freq=%d&data=%s HTTP/1.0\r\nUser-Agent: lirc/0.8.7\r\nHost: %s:%d\r\n\r\n", freq / 1000, pairs, host, port);
			write(sock, buf, strlen(buf));
			FILE *f = fdopen(sock, "rb");
			int line = 0;
			while(fgets(buf, sizeof(buf), f)) {
				line++;
				while(isspace(buf[strlen(buf) - 1]))
					buf[strlen(buf) - 1] = 0;
				if(line == 1) {
					char *c = strchr(buf, ' ');
					if(c) {
						c++;
						httpStatus = atoi(c);
					}
				}
			}
			fclose(f);
		}
		close(sock);
	}
	
	if(httpStatus != 200)
        return 0;

	return 1;
}

struct hardware hw_arduinohttp=
{
	"192.168.0.10",	    /* "device" (hostname[:port]) */
	-1,                 /* fd (socket) */
	LIRC_CAN_SEND_PULSE, /* features */
	LIRC_MODE_PULSE,    /* send_mode */
	0,                  /* rec_mode */
	0,                  /* code_length */
	arduinohttp_init,	/* init_func */
	arduinohttp_deinit, /* deinit_func */
	arduinohttp_send,	/* send_func */
	NULL,               /* rec_func */
	NULL,               /* decode_func */
	NULL,               /* ioctl_func */
	NULL,               /* readdata */
	"arduinohttp"
};
