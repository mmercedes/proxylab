#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pcache.h"

#define MAX_SIZE 1049000
#define MAX_OBJ_SIZE 102400


static void cache_evict(cache* c){
    object* temp = c->end;
    c->size = c->size - (strlen(temp->data) + 1);
    c->end = c->end->prev;
    c->end->next = NULL;
    free(temp->key);
    free(temp->data);
    free(temp);
    return;
}

cache* cache_new(){
    cache* c;
    c = malloc(sizeof(cache));
    c->start = NULL;
    c->end = NULL;
    c->size = 0;
    return c;
}

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

void cache_add(cache* c, char* key, char* data, int size){
    object* obj = malloc(sizeof(object));
    obj->key = malloc(strlen(key)+1);
    obj->data = malloc(size*sizeof(char));

    strcpy(obj->key, key);
    memcpy(obj->data, data, size);

    obj->next = c->start;
    obj->prev = NULL;
    if(c->start != NULL) c->start->prev = obj;
    c->start = obj;
    c->size += strlen(data)+1;

    if(c->end == NULL) c->end = obj;

    // kick out last object
    while(c->size > MAX_SIZE){
        printf("%d\n", (int)c->size);
        cache_evict(c);
    }
    return;
}

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

object* cache_lookup(cache* c, char* key){
    object* current = c->start;

    while(current != NULL){
        if(!strcmp(current->key, key)) return current;
        current = current->next;
    }
    return NULL;
}

