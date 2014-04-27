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
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";

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


int GET_request(char* hostname, char* path, int port, int* serverfd,
                rio_t* server);
/*
 *  ======================================================================== 
 *   Begin Proxy
 *  ========================================================================
 */

// The proxy serves as a client and a server at the same time
// the main program itself is a server, the running threads that
// handle the detailed activities should become clients to internet servers
int main(int argc, char **argv)
{    
    // ignore broken pipe signals, we don't want to terminate the process
    // due to SIGPIPE signal
    Signal(SIGPIPE, SIG_IGN);

    // just standard setup here
    int listenfd, connfd, *clientfd, port, clientlen;
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
        clientfd = Malloc(sizeof(int));    // avoid race condition

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        *clientfd = connfd;

        Pthread_create(&tid, NULL, proxy_thread, (void *)clientfd);
    }

    // end of main thread
    return 1;
}


void *proxy_thread(void *vargp){
    Pthread_detach(Pthread_self());
    // detached thread gets terminated by the system once it's done

    while (1) {
        int fd;
        fd = *(int *)vargp;
        Free(vargp);    // done with fd, free the memory allocated in main

        // Service client by writing the the file fd, from which
        // the client reads the data
        service_request(fd); 

        Close(fd);
    }
}

// this is where most of the server work gets done, such as 
// parsing commands, reading and writing to cache, fetch objects,
// and ...
void service_request(int clientfd){
    char buffer[MAXLINE];
    char hostname[MAXLINE];
    char path[MAXLINE];
    int port;
    int serverfd;
    rio_t client;
    rio_t server;

    // initialize the request entries
    buffer[0] = '\0';
    path[0] = '\0';
    hostname[0] = '\0';
    port = 80;

    Rio_readinitb(&client, clientfd);   // initialize RIO reading

    // store the first line in client input to buffer
    if (Rio_readlineb(&client, buffer, MAXLINE) < 0) Close(clientfd);
    
    if (parse_input(buffer, hostname, path, &port) != 0){
        fprintf(stderr, "Bad input request: Abort\n"); Close(clientfd);
    }
    // search cache here

    // send server request if not in cache
    if(GET_request(hostname, path, port, &serverfd, &server) != 0){
        char *error = "ERROR 404 Not Found";
        Rio_writen(clientfd, error, strlen(error));
        close(clientfd);
        return;
    }  

    return;
}


int parse_input(char *buffer, char *hostname, char *path, int *port){
    size_t i, j;  // index counters

    if (strncmp(buffer, "GET http://", strlen("GET http://")) != 0){
        // the command is not "GET", or the url does not begin correctly
        return -1;
    }

    // get the domain name, port number if any, path from input
    // e.g. www.cs.cmu.edu from www.cs.cmu.edu:80/~213 or www.cs.cmu.edu/~213
    i = 0;
    j = 0;
    while (i < strlen(buffer) - 1 && buffer[i] != ':' && 
           buffer[i] != '/' && buffer[i] != ' '){
        // write host name which ends at / or : or end of url
        hostname[j] = buffer[i];
        i++;
        j++;
    }
    hostname[j+1] = '\0';   // end of string

    if (buffer[i] == ':'){
        // we get the port (integer)
        i++;    // go to the number
        sscanf(buffer + i, "%d", port);
        while (i < strlen(buffer) - 1 && 
               buffer[i] != '/' && buffer[i] != ' '){
            // offset to path or end of url
            i++;
        }
    }

    if (buffer[i] == '/'){
        // we get the path
        j = 0;
        while (i < strlen(buffer) - 1 && buffer[i] != ' '){
            // offset to end of url
            path[j] = buffer[i];
            i++;
            j++;
        }
        path[j+1] = '\0';   // end of string
    }

    // disregard everything that follows
    return 0;
}

void p_Rio_writen(int fd, const char* buf, size_t len){
    Rio_writen(fd, (void*)buf, len);
}

int GET_request(char* hostname, char* path, int port, int* serverfd,
                rio_t* server){
    *serverfd = Open_clientfd_r(hostname, port);

    // couldn't connect to server
    if(*serverfd < 0) return 1;

    Rio_readinitb(server, *serverfd);
    p_Rio_writen(*serverfd, "GET ", strlen("GET "));
    p_Rio_writen(*serverfd, path, strlen(path));
    p_Rio_writen(*serverfd, "HTTP/1.0\r\n", strlen("HTTP/1.0\r\n"));
    p_Rio_writen(*serverfd, user_agent_hdr, strlen(user_agent_hdr));
    p_Rio_writen(*serverfd, accept_hdr, strlen(accept_hdr));
    p_Rio_writen(*serverfd, accept_encoding_hdr, strlen(accept_encoding_hdr));
    p_Rio_writen(*serverfd, connection_hdr, strlen(connection_hdr));
    p_Rio_writen(*serverfd, proxy_hdr, strlen(proxy_hdr));

    return 0;
}




































































































































































