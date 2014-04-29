#include "pcache.h"

// removes the last element from the cache
static void cache_evict(cache* c){
    object* temp = c->end;
    c->size -= temp->size;
    c->end = c->end->prev;
    c->end->next = NULL;
    free(temp->key);
    free(temp->data);
    free(temp);
    return;
}

// creates a new cache struct and initializes it
cache* cache_new(){
    cache* c;
    c = malloc(sizeof(cache));
    c->start = NULL;
    c->end = NULL;
    c->size = 0;
    return c;
}

// frees the cache struct and any objects it points to
void cache_free(cache* c){
    object* current = c->start;
    object* next = NULL;

    while(current != NULL){
        next = current->next;
        free(current->key);
        free(current->data);
        free(current);
        current = next;
    }
    free(c);
    return;
}

// creates and adds a new object to the cache
// also remove elements from the cache to keep size(cache) < MAX_SIZE
void cache_add(cache* c, char* key, char* data, int size){
    object* obj = malloc(sizeof(object));
    obj->key = malloc(strlen(key)+1);
    obj->data = malloc(size*sizeof(char));
    obj->size = size;

    strcpy(obj->key, key);
    memcpy(obj->data, data, size);

    obj->next = c->start;
    obj->prev = NULL;
    if(c->start != NULL) c->start->prev = obj;
    c->start = obj;
    c->size += size;

    if(c->end == NULL) c->end = obj;

    // kick out last object
    while(c->size > MAX_SIZE){
        cache_evict(c);
    }
    return;
}

// moves an object to the front of the cache
// this symbolizes it being the most recently accessed
void cache_update(cache* c, object* obj){
    if(c->start == obj) return;

    if(obj->next != NULL) obj->next->prev = obj->prev;
    if(obj->prev != NULL){
        obj->prev->next = obj->next;
        if(c->end == obj) c->end = obj->prev;
    }
    obj->next = c->start;
    obj->prev = NULL;
    if(c->start != NULL) c->start->prev = obj;
    c->start = obj;
    return;
}

// searches and returns a pointer to an object in the cache
object* cache_lookup(cache* c, char* key){
    object* current = c->start;

    while(current != NULL){
        if(!strcmp(current->key, key)) return current;
        current = current->next;
    }
    return NULL;
}

