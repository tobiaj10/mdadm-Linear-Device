#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int num_queries = 0;
static int num_hits = 0;
static int accessed = 0;

int cache_create(int num_entries) {
   if(cache != NULL || num_entries < 3 || num_entries > 4096)
  {
     return -1;
  }
  //create cache
  cache = (cache_entry_t*)calloc(num_entries,sizeof(cache_entry_t)* num_entries);
  cache_size = num_entries;
  //initialize cache
  for(int x = 0; x < cache_size; x++)
  {
    cache[x].disk_num = -1;
  }
  return 1;
}
int cache_destroy(void) {
  if(cache == NULL)
  {
    return -1;
  }
  //Free cache
  free(cache);
  cache = NULL;
  cache_size = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if(cache == NULL || cache[0].disk_num == -1|| cache_size == 0 || buf == NULL || disk_num < 0 || block_num < 0)
  {
    return -1;
  }
  num_queries ++;

  //find equivalent cache and set num accesses to that access time and copy into buffer.
   for(int x = 0; x < cache_size; x++)
   {
     if(cache[x].disk_num == disk_num && cache[x].block_num== block_num && cache[x].valid == 1)
     {
       cache[x].num_accesses = ++accessed;
       memcpy(buf,cache[x].block, JBOD_BLOCK_SIZE);
       num_hits++;
       return 1;
     }
   }

  return -1;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if(cache == NULL || cache_size == 0 || buf == NULL || disk_num > JBOD_NUM_DISKS || block_num > JBOD_NUM_BLOCKS_PER_DISK || disk_num < 0 || block_num < 0)
    {
      return -1;
    }
   int minimum_access = cache[cache_size -1].num_accesses;
   int minimum_index = 0;

   for(int x = 0; x < cache_size; x++)
   {
     if(cache[x].disk_num == disk_num && cache[x].block_num == block_num) //if cache is already there, insert same entry possibly with different data
     {
        return -1;
     }
     if(cache[x].disk_num == -1)
     {
       //initialize based on function pars
       cache[x].valid = true;
       cache[x].disk_num =disk_num;
       cache[x].block_num = block_num;
       cache[x].num_accesses = ++accessed;
       memcpy(cache[x].block,buf,JBOD_BLOCK_SIZE);
     
       return 1;
     }

      //keep track of min accessed and min index for lfu implementation
      if(cache[x].num_accesses < minimum_access)
      {
        minimum_access = cache[x].num_accesses;
        minimum_index = x;
      }
   }
      //lfu to insert in whatever cache is least frequently used
  cache[minimum_index].valid = true;
  cache[minimum_index].disk_num =disk_num;
  cache[minimum_index].block_num =block_num;
  memcpy(cache[minimum_index].block,buf,JBOD_BLOCK_SIZE);
  num_hits +=1;
  accessed +=1;
  cache[minimum_access].num_accesses = accessed;
     
  return 1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for(int x = 0; x < cache_size; x++)
  {
    //if cache is present, insert same entry possibly with different data
    if(cache[x].disk_num == disk_num && cache[x].block_num == block_num)
      {
        //update block content with new data in buf
        memcpy(cache[x].block,buf,JBOD_BLOCK_SIZE);
        cache[x].valid = 1;
        cache[x].num_accesses = ++accessed;
      }
  }
}

bool cache_enabled(void) {
 if(cache != NULL) //when cache not empty
  {
    return true;
  }

  return false;}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
