/*
    Proxy Lab
 */

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#ifndef DEBUG
#define debug_printf(...) {}
#else
#define debug_printf(...) printf(__VA_ARGS__)
#endif

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr ="User-Agent: Mozilla/5.0 (X11; \
Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,\
pplication/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";


/*
 *  ======================================================================== 
 *   Function Overview 
 *  ========================================================================
 */
// the main function of creating each new proxy thread
// used in: main
void *proxy_thread(void *vargp);
// the function for each thread that services the requesting client
// used in: proxy_thread
void service_request(int connfd);
// the function that takes the command (buffer), and return the
// host name, port by overwriting the where they are stored in memory
// return 0 upon request
// used in: service_request
int parse_input(char *buffer, char *hostname, char *path, int *port);

/*
 *  ======================================================================== 
 *   Begin Proxy
 *  ========================================================================
 */
int main(int argc, char **argv)
{
    // The proxy serves as a client and a server at the same time
    // the main program itself is a server, the running threads that
    // handle the detailed activities should become clients to internet servers
    
    // ignore broken pipe signals, we don't want to terminate the process
    // due to SIGPIPE signal
    Signal(SIGPIPE, SIG_IGN);

    printf("%s%s%s", user_agent_hdr, accept_hdr, accept_encoding_hdr);

    // just standard setup here
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    if (argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]); exit(0);
    }
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    
    // semaphores: not sure what to do yet 

    // main thread enters infinite loop to process requests
    while (1){
        pthread_t tid;
        int *fdptr; // the file the client reads from

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);

        fdptr = Malloc(sizeof(int));    // avoid race condition
        Pthread_create(&tid, NULL, proxy_thread, (void *)fdptr);
    }

    // end of main thread
    return 0;
}

void *proxy_thread(void *vargp){
    Pthread_detach(Pthread_self());
    // detached thread gets terminated by the system once it's done

    while (1) {
        int fd;
        fd = *(int *)vargp;
        Free(vargp);    // done with fd, free the memory allocated in main

        service_request(fd); 
        // Service client by writing the the file fd, from which
        // the client reads the data

        Close(fd);
    }
}

void service_request(int clientfd){
    // this is where most of the server work gets done, such as 
    // parsing commands, reading and writing to cache, fetch objects,
    // and ...
    char buffer[MAXLINE];
    char hostname[MAXLINE];
    char path[MAXLINE];
    int port;
    rio_t client;

    buffer[MAXLINE - 1] = '\0';     // initialize the end of char-array
    Rio_readinitb(&client, clientfd);   // initialize RIO reading

    Rio_readlineb(&client, buffer, MAXLINE);
    // store the first line in client input to buffer
    if (parse_input(buffer, hostname, path, &port) != 0){
        fprintf(stderr, "Bad input\n");
    }




    return;
}

int parse_input(char *buffer, char *hostname, char *path, int *port){
    if (strncmp(buffer, "GET http://", strlen("GET http://")) != 0){
        // the command is not "GET", or the url does not begin correctly
        return -1;
    }
    buffer += strlen("GET http://");

    // get the domain name, 
    // e.g. www.cs.cmu.edu from www.cs.cmu.edu:80/~213 or www.cs.cmu.edu/~213
    

    return 0;
}






































































































































































