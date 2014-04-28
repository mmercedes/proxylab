#include <stdlib.c>
#include <string.h>
#include "csapp.h"

cache* cache_new(){
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
	obj->key = malloc(strlen(key)*sizeof(char));
	obj->data = malloc(size*sizeof(char));

	strcpy(obj->key, key);
	strcpy(obj->data, data);

	obj->next = c->start;
	obj->prev = NULL;
	c->start->prev = obj;
	c->start = obj;
	c->size += size;
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
	current = c->start;

	while(current != NULL){
		if(!strcmp(current->key, key)) return current;
		current = current->next;
	}
	return NULL;
}