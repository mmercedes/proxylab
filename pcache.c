#include <stdlib.h>
#include <string.h>
#include "pcache.h"

#define MAX_SIZE 1049000

int cache_check(cache* c);

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

void cache_add(cache* c, char* key, char* data, size_t size){
	object* obj = malloc(sizeof(object));
	obj->key = malloc(strlen(key)*sizeof(char)+1);
	obj->data = malloc(strlen(data)*sizeof(char)+1);

	strcpy(obj->key, key);
	strcpy(obj->data, data);

	obj->next = c->start;
	obj->prev = NULL;
	if(c->start != NULL) c->start->prev = obj;
	c->start = obj;
	c->size += size;

	// kick out last object
	while(c->size > MAX_SIZE){
		c->size -= strlen(c->end->data);
		free(c->end->key);
		free(c->end->data);
		c->end = c->end->prev;
		free(c->end->next);
		c->end->next = NULL;
	}
	return;
}

void cache_update(cache* c, object* obj){
	if(obj->next != NULL) obj->next->prev = obj->prev;
	if(obj->prev != NULL) obj->prev->next = obj->next;
	obj->next = c->start;
	obj->prev = NULL;
	c->start->prev = obj;
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

