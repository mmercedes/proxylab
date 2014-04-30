/*
 *  PROXY LAB
 *
 * Matt Mercedes (mmercede)
 * Nathan Wu (chenwu)
 *
 *
 * A multithreaded web proxy with caching.
 * 
 * To implement a cache we created our own header file called pcache.h
 * It provides functions to add/remove from a linked-list style cache
 * To decide what objects remain in the cache, we have implemented a least 
 * recently used system to remove less frequently accesed data from the cache
 * 
 *
 */

#include "csapp.h"
#include "pcache.h"

// Recommended max cache and object sizes 
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
 // 8kb is the max header size accepted by Apache servers
#define MAX_HEADER_SIZE 8192  

#ifndef DEBUG
#define debug_printf(...) {}
#else
#define debug_printf(...) printf(__VA_ARGS__)
#endif

// a client's request header will have these fields overwritten
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

// function called immediately after thread creation
void *proxy_thread(void *vargp);

// takes a client connection file descriptor and handles their request
void service_request(int connfd);

// reads the first line sent by the client and sets the hostname, path, and
// port variables, returns 1 if not a GET request and 0 otherwise
int parse_input(char *buffer, char *hostname, char *path, int *port);

// reads from the client and forms a header to send to the requested server
void get_request_header(int cfd, rio_t *client, char *header);

// makes a GET request on behalf of the client to the requested server
int GET_request(char *hostname, char *path, int port, char *unparsed,
                int *serverfd, rio_t *server, int connfd);

// feeds the response from the server back to the client
// will also attempt to cache the server's response if possible
void respond_to_client(rio_t *server, int serverfd,
                       int clientfd, char *cache_key);

// a wrapper for rio_readlineb that will safely close a thread upon an error
int p_Rio_readlineb(int sfd, int cfd, rio_t *conn, char *buffer, size_t size);

// a wrapper for rio_writen that will safely close a thread upon an error
void p_Rio_writen(int sfd, int cfd, const char *buf, size_t len);

// reader lock for the cache to allow multiple readers safely access the cache
// readers have priority over writers and block them
void cache_r_lock();
void cache_r_unlock();

// writer lock for the cache to allow a single writer to safely alter the cache
// all other readers and writers are blocked
void inline cache_w_lock();
void inline cache_w_unlock();


/*
 *  ======================================================================== 
 *   Declare Global Variables
 *  ========================================================================
 */


int readcnt;
sem_t mutex;
sem_t w;
cache* p_cache;


/*
 *  ======================================================================== 
 *   Begin Proxy
 *  ========================================================================
 */


/*
 * The proxy serves as a client and a server at the same time
 * the main program itself is a server, the running threads that
 * handle a client request should become clients to internet servers
 */

int main(int argc, char **argv)
{    
    int listenfd, connfd, *clientfd, port, clientlen;
    struct sockaddr_in clientaddr;

    // ignore broken pipe signals, we don't want to terminate the process
    // due to SIGPIPE signal
    Signal(SIGPIPE, SIG_IGN);

    if (argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]); exit(0);
    }

    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    
    // initialize semaphores to 1
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    readcnt = 0;

    p_cache = cache_new();

    // main thread enters infinite loop to process requests
    while (1){
        pthread_t tid;
        clientfd = Malloc(sizeof(int));    // avoid race condition

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        *clientfd = connfd;

        // spawn a thread to handle each request
        Pthread_create(&tid, NULL, proxy_thread, (void *)clientfd);
    }

    // control should never reach here
    cache_free(p_cache);
    return 1;
}


void *proxy_thread(void *vargp){
    Pthread_detach(Pthread_self());
    // detached thread gets terminated by the system once it's done

    int fd = *(int *)vargp;

    // done with fd, free the memory allocated in main
    Free(vargp);

    service_request(fd); 

    Close(fd);
    return NULL;
}


void service_request(int clientfd){
    char buffer[MAXLINE];
    char cache_key[MAXLINE];
    char hostname[MAXLINE];
    char path[MAXLINE];
    char *header;
    int port;
    int serverfd;
    rio_t client;
    rio_t server;
    char *error = "ERROR 404 Not Found";
    object* cache_obj;
    int cache_hit = 0;

    // initialize the request entries
    buffer[0] = '\0';
    path[0] = '\0';
    hostname[0] = '\0';
    port = 80;

    // initialize RIO reading
    Rio_readinitb(&client, clientfd);

    // store the first line in client input to buffer
    p_Rio_readlineb(0, clientfd, &client, buffer, MAXLINE);

    if (parse_input(buffer, hostname, path, &port) != 0){
        return; // not a GET request
    }
    // create a key for future cache lookup
    sprintf(cache_key, "%s %s", hostname, path);

    header = malloc(MAX_HEADER_SIZE);
    bzero(header, MAX_HEADER_SIZE);
    get_request_header(clientfd, &client, header);
    
    // search the cache
    cache_r_lock();
    cache_obj = cache_lookup(p_cache, cache_key);
    if(cache_obj != NULL) cache_hit = 1;
    cache_r_unlock();

    // cache hit
    if(cache_hit){

        cache_r_lock();
        rio_writen(clientfd, (void*)cache_obj->data, cache_obj->size);
        cache_r_unlock();

        cache_w_lock();
        cache_update(p_cache, cache_obj);
        cache_w_unlock();
    }
    // send server request if not in cache
    else{
        if(GET_request(hostname, path, port, header, 
                       &serverfd, &server, clientfd) != 0){
            //failed connection to server
            rio_writen(clientfd, error, strlen(error));
            Free(header);
            return;
        }  
        respond_to_client(&server, serverfd, clientfd, cache_key);
        Close(serverfd);
    }
    free(header);
    return;
}


int parse_input(char *buffer, char *hostname, char *path, int *port)
{
    // not a request we can handle
    if (strncmp(buffer, "GET http://", strlen("GET http://")) != 0) return 1;

    int offset = strlen("GET http://");
    int i = offset;

    // add hostname to buffer
    while(buffer[i] != '\0')
    {
        if(buffer[i] == ':') break;
        if(buffer[i] == '/') break;

        hostname[i-offset] = buffer[i];
        i++;
    }

    // end of host, obtain path and port if possible
    if(buffer[i] == ':') sscanf(&buffer[i+1], "%d%s", port, path);
    else sscanf(&buffer[i], "%s", path);

    hostname[i-offset] = '\0';
    return 0;
}


void get_request_header(int cfd, rio_t *client, char *header){
    int bytes;
    int total_bytes = 0;
    char buffer[MAXLINE];

    while((bytes = p_Rio_readlineb(0, cfd, client, buffer, MAXLINE))){
        if(buffer[0] == '\r'){
            strncat(header, buffer, bytes);
            return;
        }
        // proxy overwrites these fields so skip reading them from client
        if(strstr(buffer, "Host:") != NULL) continue;
        if(strstr(buffer, "User-Agent:") != NULL) continue;
        if(strstr(buffer, "Accept:") != NULL) continue;
        if(strstr(buffer, "Connection:") != NULL) continue;
        if(strstr(buffer, "Proxy-Connection:") != NULL) continue;

        // make sure we don't exceed the header size
        total_bytes += bytes;
        if(total_bytes > MAX_HEADER_SIZE) break;
        strncat(header, buffer, bytes);
    }
    return;
}


int p_Rio_readlineb(int sfd, int cfd, rio_t *conn, char *buffer, size_t size){
    int bytes;

    if((bytes = rio_readlineb(conn, buffer, size)) < 0){
        // exit thread
        if(sfd) close(sfd);
        close(cfd);
        Pthread_exit(NULL);
    }
    return bytes;
}


void p_Rio_writen(int sfd, int cfd, const char* buf, size_t len){
    if(rio_writen(sfd, (void*)buf, len) < 0){
        // exit thread
        close(sfd);
        close(cfd);
        Pthread_exit(NULL);
    }
}


int GET_request(char *hostname, char *path, int port, char *header,
                int *serverfd, rio_t *server, int connfd){

    *serverfd = open_clientfd_r(hostname, port);

    // couldn't connect to server
    if(*serverfd < 0) return 1;
                                    
    // send server an edited verision of the client's header                                         
    Rio_readinitb(server, *serverfd);
    p_Rio_writen(*serverfd, connfd, "GET ", strlen("GET "));
    p_Rio_writen(*serverfd, connfd, path, strlen(path));
    p_Rio_writen(*serverfd, connfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
    p_Rio_writen(*serverfd, connfd, "Host: ", strlen("Host: "));
    p_Rio_writen(*serverfd, connfd, hostname, strlen(hostname));
    p_Rio_writen(*serverfd, connfd, "\r\n", strlen("\r\n"));
    p_Rio_writen(*serverfd, connfd, user_agent_hdr, strlen(user_agent_hdr));
    p_Rio_writen(*serverfd, connfd, accept_hdr, strlen(accept_hdr));
    p_Rio_writen(*serverfd, connfd, accept_encoding_hdr,
                 strlen(accept_encoding_hdr));
    p_Rio_writen(*serverfd, connfd, connection_hdr, strlen(connection_hdr));
    p_Rio_writen(*serverfd, connfd, proxy_hdr, strlen(proxy_hdr));
    p_Rio_writen(*serverfd, connfd, header, strlen(header));
    p_Rio_writen(*serverfd, connfd, "\r\n", strlen("\r\n"));

    return 0;
}


void respond_to_client(rio_t *server, int serverfd,
                       int clientfd, char *cache_key){

    char buffer[MAXLINE];
    char cache_data[MAX_OBJECT_SIZE];
    char *ptr = cache_data;
    int offset = 0;
    int bytes = 0;

    bzero(buffer, MAXLINE);

    // read data from the server
    while((bytes = rio_readnb(server, buffer, MAXLINE)) > 0){
        p_Rio_writen(clientfd, serverfd, buffer, bytes);

        // attempt to save data for cache
        if(offset+bytes < MAX_OBJECT_SIZE){
            memcpy(ptr+offset, buffer, bytes);
        }
        offset += bytes;
        bzero(buffer, MAXLINE);
    }
    if(bytes == -1) return; // failed reading from server

    // cache the data received from the server
    if(offset < MAX_OBJECT_SIZE && offset > 0){
        cache_w_lock();
        cache_add(p_cache, cache_key, cache_data, offset);
        cache_w_unlock();
    }
    return;
}


void cache_r_lock(){
    P(&mutex);
    readcnt++;
    if(readcnt == 1) P(&w);
    V(&mutex);
    return;
}


void cache_r_unlock(){
    P(&mutex);
    readcnt--;
    if(readcnt == 0) V(&w);
    V(&mutex);
    return;
}


void inline cache_w_lock(){
    P(&w);
}


void inline cache_w_unlock(){
    V(&w);
}

