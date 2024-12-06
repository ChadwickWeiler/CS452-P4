#include "lab.h"
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>



/*
*Amit Suggests printing buddy list for debugging. 
*/

/* Buddy calculations only work if the base address is zero. Subract the address value, then add it back after XOR funciton
*/

size_t btok(size_t bytes){
unsigned int count = 0;
if(bytes > 1){bytes --;}
while (bytes > 0) {bytes >>= 1; count++;}
return count;
}

struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy){
  size_t block_kval = buddy->kval;
  //shifting left to get new kvalue
  block_kval  <<= 1;

  uintptr_t address = (((uintptr_t)buddy - (uintptr_t)(pool->base)) ^ block_kval);
  //adding address back post XOR calculation
  struct avail *bp = (struct avail *) (pool->base + address);
  return bp;
}

void *buddy_malloc(struct buddy_pool *pool, size_t size){
  printf("malloc start/n");
  if(size == 0){
    return NULL;
  }

  size_t kval = btok(size);
  //ensuring base kvalue is accounted for. have to accoutn for size of existing data. 
  if(kval <=SMALLEST_K){ kval = SMALLEST_K;}
  struct avail *free_block = NULL;

  for(size_t i = kval; i <= pool->kval_m; i++){

    if(pool->avail[i].next != &pool->avail[i]){
      free_block = pool->avail[i].next;
      break;
    }

  }

  if((free_block)){

      //splitting blocks til optimal block found
      while(free_block->kval > kval){
      struct avail *buddy_block = buddy_calc(pool, free_block);
      free_block->kval--;



      buddy_block->tag = BLOCK_AVAIL;
      buddy_block->kval = free_block->kval;

      //this was necessary to pass tests. Avoids seg fault
      if (buddy_block->next == NULL || buddy_block->prev == NULL) {
        errno = ENOMEM;
        return NULL;
      }
      
      buddy_block->next = pool->avail[free_block->kval].next;
      buddy_block->prev = &pool->avail[free_block->kval];

      pool->avail[free_block->kval].next = buddy_block;
      buddy_block->next->prev =buddy_block;
      free_block = buddy_block;

    }
    //reserving the block
    free_block->tag = BLOCK_RESERVED;
    free_block->prev->next =free_block->next;
    free_block->next->prev =free_block->prev;

    
  
    printf("malloc success");
    return (void *)(free_block+1);  

  }
  else{
    errno = ENOMEM;
    return NULL;
  }


}

void buddy_free(struct buddy_pool *pool, void *ptr){
  if(ptr == NULL){return;}

  struct avail *block = (struct avail *)ptr - 1; 
  block->tag = BLOCK_AVAIL; 

  struct avail *buddy_block = buddy_calc(pool, block);
  bool buddy_flag = true; 
  while(buddy_flag == true){
    
    if(buddy_block->kval == block->kval && buddy_block->tag != BLOCK_AVAIL){
      //block merging functions
      if(buddy_block < block){
        block->prev = buddy_block->prev;
        block->next= buddy_block->next;

        buddy_block->prev->next = block;
        buddy_block->next->prev = block;
        

        block->kval++;
      }

      if(buddy_block> block){
        buddy_block->next = block->next;
        buddy_block->prev = block->prev;
        
        
        block->next->prev = buddy_block;
        block->prev->next = buddy_block;
        
        buddy_block-> kval++;
      }
      buddy_block = buddy_calc(pool, buddy_block);
  }
  
  else{
  buddy_flag = false;
}
}
//insert the block back into list
struct avail *list = &pool-> avail[block->kval];

block->prev = list;
block->next = list->next;

list-> next->prev = block;
list->next = block;
}

void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size){
  
  if(size == 0 && ptr != NULL){
    buddy_free(pool, ptr);

    return NULL;
 }

 if(ptr == NULL){
    return buddy_malloc(pool, size);

 }

 struct avail *block = (struct avail *)ptr -1;

 size_t new_k = btok(size);
//ensuring minimum size accounted for
 if(new_k <=SMALLEST_K){new_k =SMALLEST_K;} 

 if(block->kval >= new_k){
  bool kval_flag = true;
  //splitting blocks
  while ((kval_flag = true)){
    struct avail *buddy_split = buddy_calc(pool, block);

    buddy_split->tag = BLOCK_AVAIL;
    buddy_split-> kval = block->kval-1;

    buddy_split->prev = &pool->avail[buddy_split->kval];
    buddy_split->next = pool->avail[buddy_split->kval].next;

    pool->avail[buddy_split->kval].next = buddy_split;
    buddy_split->next->prev = buddy_split;

    block->kval--;
    if(block->kval < new_k){kval_flag = false;}
    
  }
  return ptr;
 }

//new block created if too small
 else{
    void *newblock_ptr = buddy_malloc(pool, size);
    if(newblock_ptr == NULL){ return NULL;}

    size_t prev_size = 1 << block->kval;

    size_t byte_copy;
    if(prev_size >= size){byte_copy = size;}
    else{byte_copy = prev_size;}

    for(size_t i = 0; i < byte_copy; i++){
      ((unsigned char *)newblock_ptr)[i]= ((unsigned char *)ptr)[i];

    }
    buddy_free(pool,ptr);

    return newblock_ptr;
 }
}
 


void buddy_init(struct buddy_pool *pool, size_t size){
  if(size == 0) {size = UINT64_C(1) << DEFAULT_K;}
  pool->kval_m = btok(size);
  pool->numbytes = UINT64_C(1) << pool->kval_m;
  
  pool->base = mmap(NULL, pool->numbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(pool->base == MAP_FAILED){
    perror("buddy: could not allocate memory pool");
  }
  
  for(size_t i = 0; i <+ pool->kval_m; i++){
    pool->avail[i].next = &pool->avail[i];
    pool->avail[i].prev = &pool->avail[i];
    pool->avail[i].kval = i;
    pool->avail[i].tag = BLOCK_UNUSED;
  } 

  pool->avail[pool->kval_m].next = pool->base;
  pool->avail[pool->kval_m].prev = pool->base;
  struct avail *ptr = (struct avail *)pool->base;
  ptr->tag = BLOCK_AVAIL;
  ptr->kval = pool->kval_m;
  ((struct avail *)ptr)->next = &pool->avail[pool->kval_m];
  ((struct avail *)ptr)->prev = &pool->avail[pool->kval_m];

  }

void buddy_destroy(struct buddy_pool *pool){
  int status = munmap(pool->base, pool->numbytes);
  if (status == -1) {
    perror("buddy: destroy Failed");
  }
}


int myMain(int argc, char** argv);