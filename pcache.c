#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pcache.h"

#define MAX_SIZE 1049000
#define MAX_OBJ_SIZE 102400

static int cache_length(cache* c){
	object* current = c->start;
	int i = 0;

	while(current != NULL){
		current = current->next;
		i++;
	}
	return i;
}

static void cache_evict(cache* c){
	printf("%d\n", cache_length(c));
	printf("END SIZE: %d\n", (int)strlen(c->end->data));
	if(c->end == NULL){
		printf("END WAS NULL\n");
		exit(0);
	}
	if(c->end->prev == NULL){
		printf("PREV WAS NULL\n");
		if(c->end == c->start) printf("END == START\n");
		exit(0);
	}
	if(c->end->next != NULL){
		printf("NEXT WASNT NULL\n");
		exit(0);
	}
	object* temp = c->end;
	c->size = c->size - (strlen(temp->data) + 1);
	c->end = c->end->prev;
	c->end->next = NULL;
	free(temp->key);
	free(temp->data);
	free(temp);
	return;
}

static void cache_check(cache* c){
	if(c == NULL){
		printf("C IS NULL\n");
		exit(0);
	}
	if(c->start == NULL && c->end != NULL){
		printf("S NULL E NOT\n");
		exit(0);
	}
	if(c->end == NULL && c->start != NULL){
		printf("E NULL S NOT\n");
		exit(0);
	}
	if(c->size > MAX_SIZE){
		printf("SIZE LIMIT EXCEEDED\n");
		exit(0);
	}
	object* current = c->start;
	object* prev = NULL;

	while(current != NULL){
		if(current->prev != prev){
			printf("PREV NOT MATCHED\n");
			exit(0);
		}
		prev = current;
		current = current->next;

	}
	if(prev != c->end){
		printf("END NOT REACHED\n");
		exit(0);
	}
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

void cache_add(cache* c, char* key, char* data){
	cache_check(c);
	object* obj = malloc(sizeof(object));
	obj->key = malloc(strlen(key)*sizeof(char)+1);
	obj->data = malloc(strlen(data)*sizeof(char)+1);

	strcpy(obj->key, key);
	strcpy(obj->data, data);

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
	cache_check(c);
	return;
}

void cache_update(cache* c, object* obj){
	cache_check(c);
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
	cache_check(c);
	return;
}

object* cache_lookup(cache* c, char* key){
	cache_check(c);
	object* current = c->start;

	while(current != NULL){
		if(!strcmp(current->key, key)) return current;
		current = current->next;
	}
	cache_check(c);
	return NULL;
}

