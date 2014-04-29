#ifndef PCACHE_H_
#define PCACHE_H_

typedef struct object
{
	char* key;
	char* data;
	struct object* next;
	struct object* prev;
} object;

typedef struct cache
{
	struct object* start;
	struct object* end;
	size_t size;
} cache;


cache* cache_new();
void cache_free(cache* c);
void cache_add(cache* c, char* key, char* data);
void cache_update(cache* c, object* obj);
object* cache_lookup(cache* c, char* key);


#endif