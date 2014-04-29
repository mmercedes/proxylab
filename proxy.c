/*
    Proxy Lab
 */

#include "csapp.h"
#include "pcache.h"

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

char *get_request_header(rio_t *client, char *header);

int GET_request(char *hostname, char *path, int port, char *unparsed,
                int *serverfd, rio_t *server, int connfd);

void respond_to_client(rio_t* server, int serverfd, int clientfd, char *cache_key);

void cache_r_lock();
void cache_r_unlock();
void cache_w_lock();
void cache_w_unlock();

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

// The proxy serves as a client and a server at the same time
// the main program itself is a server, the running threads that
// handle the detailed activities should become clients to internet servers
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

        Pthread_create(&tid, NULL, proxy_thread, (void *)clientfd);
    }

    cache_free(p_cache);
    return 1;
}


void *proxy_thread(void *vargp){
    Pthread_detach(Pthread_self());
    // detached thread gets terminated by the system once it's done

    int fd;
    fd = *(int *)vargp;
    Free(vargp);    // done with fd, free the memory allocated in main

    service_request(fd); 

    Close(fd);
    return NULL;
}

// this is where most of the server work gets done, such as 
// parsing commands, reading and writing to cache, fetch objects,
// and ...
void service_request(int clientfd){
    char buffer[MAXLINE];
    char cache_key[MAXLINE];
    char hostname[MAXLINE];
    char path[MAXLINE];
    int port;
    int serverfd;
    rio_t client;
    rio_t server;
    char *error = "ERROR 404 Not Found";
    char *header;
    object* cache_obj;

    // initialize the request entries
    buffer[0] = '\0';
    path[0] = '\0';
    hostname[0] = '\0';
    port = 80;

    Rio_readinitb(&client, clientfd);   // initialize RIO reading

    // store the first line in client input to buffer
    if (Rio_readlineb(&client, buffer, MAXLINE) < 0){ 
        return;
    }
    strcpy(cache_key, buffer);

    if (parse_input(buffer, hostname, path, &port) != 0){
        fprintf(stderr, "Not a GET request: Abort\n");
        return;
    }

    header = malloc(MAXLINE*sizeof(char));
    bzero(header, MAXLINE);
    header = get_request_header(&client, header);
    
    // search the cache
    cache_r_lock();
    cache_obj = cache_lookup(p_cache, cache_key);
    cache_r_unlock();

    // cache hit
    if(cache_obj != NULL){
        Rio_writen(clientfd, (void*)cache_obj->data, strlen(cache_obj->data));
        cache_w_lock();
        cache_update(p_cache, cache_obj);
        cache_w_unlock();
    }
    // send server request if not in cache
    else{
        if(GET_request(hostname, path, port, header, &serverfd, &server, clientfd) != 0){
            Rio_writen(clientfd, error, strlen(error));
            Free(header);
            return;
        }  
        respond_to_client(&server, serverfd, clientfd, cache_key);
        Close(serverfd);
    }
    Free(header);
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
    i += strlen("GET http://");
    j = 0;
    while (i < strlen(buffer) - 1 && buffer[i] != ':' && 
           buffer[i] != '/' && buffer[i] != ' '){
        // write host name which ends at / or : or end of url
        hostname[j] = buffer[i];
        i++;
        j++;
    }
    hostname[j] = '\0';   // end of string

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
        path[j] = '\0';   // end of string
    }
    return 0;
}

char *get_request_header(rio_t *client, char *header){
    size_t size = MAXLINE;
    int bytes;
    char buffer[MAXLINE];

    while((bytes = Rio_readlineb(client, buffer, MAXLINE))){
        if(buffer[0] == '\r'){
            strcat(header, buffer);
            return header;
        }
        if(strstr(buffer, "Host:") != NULL) continue;
        if(strstr(buffer, "User-Agent:") != NULL) continue;
        if(strstr(buffer, "Accept:") != NULL) continue;
        if(strstr(buffer, "Connection:") != NULL) continue;
        if(strstr(buffer, "Proxy-Connection:") != NULL) continue;
        strcat(header, buffer);
        size += MAXLINE;
        header = realloc(header, size);
    }
    return header;
}

void p_Rio_writen(int sfd, int cfd, const char* buf, size_t len){
    if(rio_writen(sfd, (void*)buf, len) < 0){
        close(sfd);
        close(cfd);
        Pthread_exit(NULL);
    }
}

int GET_request(char* hostname, char* path, int port, char *header,
                int* serverfd, rio_t* server, int connfd){

    *serverfd = open_clientfd_r(hostname, port);

    // couldn't connect to server
    if(*serverfd < 0) return 1;
                                                        
    Rio_readinitb(server, *serverfd);
    p_Rio_writen(*serverfd, connfd, "GET ", strlen("GET "));
    p_Rio_writen(*serverfd, connfd, path, strlen(path));
    p_Rio_writen(*serverfd, connfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
    p_Rio_writen(*serverfd, connfd, "Host: ", strlen("Host: "));
    p_Rio_writen(*serverfd, connfd, hostname, strlen(hostname));
    p_Rio_writen(*serverfd, connfd, "\r\n", strlen("\r\n"));
    p_Rio_writen(*serverfd, connfd, user_agent_hdr, strlen(user_agent_hdr));
    p_Rio_writen(*serverfd, connfd, accept_hdr, strlen(accept_hdr));
    p_Rio_writen(*serverfd, connfd, accept_encoding_hdr, strlen(accept_encoding_hdr));
    p_Rio_writen(*serverfd, connfd, connection_hdr, strlen(connection_hdr));
    p_Rio_writen(*serverfd, connfd, proxy_hdr, strlen(proxy_hdr));
    p_Rio_writen(*serverfd, connfd, header, strlen(header));
    p_Rio_writen(*serverfd, connfd, "\r\n", strlen("\r\n"));

    return 0;
}

void respond_to_client(rio_t *server, int serverfd, int clientfd, char* cache_key){
    char buffer[MAXLINE];
    char cache_data[MAX_OBJECT_SIZE];
    char *current_pos = cache_data;
    size_t data_size = 0;
    size_t bytes = 0;
    int cacheable = 0;

    bzero(buffer, MAXLINE);

    while((bytes = Rio_readlineb(server, buffer, MAXLINE)) > 0){
        p_Rio_writen(clientfd, serverfd, buffer, bytes);
        data_size += bytes;

        if(data_size > MAX_OBJECT_SIZE) cacheable = 0;
        else cacheable = 1;

        if(cacheable){
            memcpy(current_pos, buffer, bytes);
            current_pos += bytes;
        }
        bzero(buffer, MAXLINE);
    }
    if(cacheable){
        cache_w_lock();
        cache_add(p_cache, cache_key, cache_data, data_size);
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

