#include <sys/prctl.h> /* for PR_SET_TIMERSLACK */
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "my_signal.h"
#include "my_socket.h"
#include "get_num.h"
#include "logUtil.h"
#include "bz_usleep.h"

int debug = 0;
volatile sig_atomic_t has_usr1 = 0;

void sig_usr1()
{
    has_usr1 = 1;
    return;
}

int set_so_cork(int sockfd, int value)
{
    int on_off = value;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_CORK , &on_off, sizeof(on_off)) < 0) {
        warn("setsockopt cork");
        return -1;
    }

    return 0;
}

int child_proc(int connfd, long bufsize, useconds_t usleep_sec, int use_bz_usleep)
{
    if (usleep_sec > 0) {
        if (prctl(PR_SET_TIMERSLACK, 1) < 0) {
            err(1, "prctl PR_SET_TIMERSLACK");
        }
    }

    fprintfwt(stderr, "server: start\n");

    if (set_so_nodelay(connfd) < 0) {
        errx(1, "set_so_nodelay");
    }

    char *buf = malloc(bufsize);
    if (buf == NULL) {
        err(1, "malloc");
    }
    memset(buf, 0, bufsize);
    fprintfwt(stderr, "server: bufsize: %ld\n", bufsize);

    for ( ; ; ) {
        //if (set_so_nodelay(connfd) < 0) {
        //    errx(1, "set_so_nodelay");
        //}

        if (set_so_cork(connfd, 1) < 0) {
            errx(1, "set_so_cork(, 1)");
        }

        int n = write(connfd, buf, bufsize);
        if (n < 0) {
            if (errno == ECONNRESET) {
                int sndbuf = get_so_sndbuf(connfd);
                fprintfwt(stderr, "server: sndbuf: last: %d\n", sndbuf);
                fprintfwt(stderr, "server: connection reset by client (ECONNRESET)\n");
                break;
            }
            else if (errno == EPIPE) {
                int sndbuf = get_so_sndbuf(connfd);
                fprintfwt(stderr, "server: sndbuf: last: %d\n", sndbuf);
                fprintfwt(stderr, "server: connection reset by client (EPIPE)\n");
                break;
            }
            else {
                err(1, "write");
            }
        }

        if (set_so_cork(connfd, 0) < 0) {
            errx(1, "set_so_cork(, 0)");
        }
        
        if (usleep_sec > 0) {
            if (use_bz_usleep) {
                bz_usleep(usleep_sec);
            }
            else {
                usleep(usleep_sec);
            }
        }
    }

    return 0;
}

void sig_chld(int signo)
{
    pid_t pid;
    int   stat;

    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        ;
    }
    return;
}

int usage(void)
{
    char *msg = "Usage: server [-b bufsize (100)] [-p port (1234)] [-s usec (0)] [-B]\n"
"-b bufsize: send data size (100)\n"
"-p port:    port number (1234)\n"
"-s usec:    usleep usec (don't usleep())\n"
"-B:         use busy sleep (excessive CPU consumption)\n";

    fprintf(stderr, "%s", msg);

    return 0;
}

int main(int argc, char *argv[])
{
    int port = 1234;
    pid_t pid;
    struct sockaddr_in remote;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int listenfd;
    long bufsize = 100;
    useconds_t usleep_sec = 0;
    int use_bz_usleep = 0;

    int c;
    while ( (c = getopt(argc, argv, "b:Bdhp:s:")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'B':
                use_bz_usleep = 1;
                break;
            case 'd':
                debug += 1;
                break;
            case 'h':
                usage();
                exit(0);
            case 'p':
                port = strtol(optarg, NULL, 0);
                break;
            case 's':
                usleep_sec = strtol(optarg, NULL, 0);
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;

    my_signal(SIGCHLD, sig_chld);
    my_signal(SIGPIPE, SIG_IGN);

    listenfd = tcp_listen(port);
    if (listenfd < 0) {
        errx(1, "tcp_listen");
    }
    //int sndbuf = get_so_sndbuf(listenfd);
    //fprintfwt(stderr, "server: main: listenfd: sndbuf: %d\n", sndbuf);

    for ( ; ; ) {
        int connfd = accept(listenfd, (struct sockaddr *)&remote, &addr_len);
        if (connfd < 0) {
            err(1, "accept");
        }
        
        pid = fork();
        if (pid == 0) { // child process
            if (close(listenfd) < 0) {
                err(1, "close listenfd");
            }
            if (child_proc(connfd, bufsize, usleep_sec, use_bz_usleep) < 0) {
                errx(1, "child_proc");
            }
            exit(0);
        }
        else { // parent process
            if (close(connfd) < 0) {
                err(1, "close for accept socket of parent");
            }
        }
    }
        
    return 0;
}
