#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sbmem.h"
#include <semaphore.h>

// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
char shared_name[] = "/shared";

// Define semaphore(s)
sem_t * semap;

// Define your stuctures and variables. 
const int MAX_PROCESS = 10;
const int MIN_SIZE = 32768;
const int MAX_LEVEL = 12;
const int MIN_LEVEL = 7;
const int MAX_BUDDY = 128; //MORE THAN ENOUGH	

int fd;
void* start;
struct shared *data_ptr;
struct stat buffer;

struct processInfo{
	pid_t pid;
	void* ptr;
};

struct processInfo processes[10];
int processExists[10] = {0};
int processCount = 0;

struct buddy {
	int level;
	int tag;
	int size;
	void* loc;
	struct buddy* next;
	struct buddy* prev;
};

const int MAPPED_OFFSET = 13 * sizeof(struct buddy*) + MAX_BUDDY*sizeof(struct buddy);
int buddyCount = 0;
int segSize = 0;
int internFrag = 0;

int sbmem_init(int segmentsize)
{	
    if(segSize >= (int)pow(2,10)*32 && segSize <= (int)pow(2,10)*256){
    	printf("Segment size must be between 32KB and 256KB!\n");	
    	return -1;
    }
    
    if(!(segmentsize % 2 == 0)){
   		printf("Segment Size Must Be Power of Two!\n");	
   		return -1;
    }
    
    fd = shm_open(shared_name,O_RDWR | O_CREAT,0660); 
    
    if(fd < 0){
    	shm_unlink(shared_name);
    	fd = shm_open(shared_name,O_RDWR | O_CREAT,0660);
    	
    	if(fd < 0){
    		printf("Shared memory creation failed!\n");
    		return -1;
    	}
    }
    
   	if(ftruncate(fd,segmentsize) < 0){
   		printf("Shared memory truncate failed!\n");
   		return -1;
   	}
    
    sem_unlink("/semap");
    semap = sem_open("/semap", O_RDWR | O_CREAT,0660,MAX_PROCESS);
    
    return 0; 
}

int sbmem_remove()
{	
	processCount = 0;
	
	for(int i = 0; i < MAX_PROCESS; i++){
		processExists[i] = -1;
	}
	
	sem_close(semap);
	shm_unlink(shared_name);
    return (0); 
}

int sbmem_open()
{
	semap = sem_open("/semap", O_RDWR | O_CREAT,0660,MAX_PROCESS);
	sem_wait(semap);
	fd = shm_open(shared_name,O_RDWR | O_CREAT,0660); 
	fstat(fd,&buffer);
    segSize = buffer.st_size;
    
	if(processCount < MAX_PROCESS){
		
		processes[processCount].pid = getpid();
		processes[processCount].ptr = mmap(NULL, segSize, PROT_READ | PROT_WRITE,MAP_SHARED,fd, 0);
		
		//MARK AS NOT INITIALIZED YET
	 	*((int *)(processes[processCount].ptr + MAPPED_OFFSET)) = 0;

		if(processes[processCount].ptr < 0){
			printf("Memory mapping failed!\n");
			sem_post(semap);
			return -1;
		}else{
			processExists[processCount] = 1;
			processCount++;
			sem_post(semap);
			return 0;
		}
	}
	
    sem_post(semap);
    return -1; 
}


void *sbmem_alloc (int size)
{
	sem_wait(semap);
	void* processPtr;
	pid_t requestPid = getpid();
	
	for(int i = 0; i < MAX_PROCESS; i++){
		if(processExists[i] == 1 && processes[i].pid == requestPid){
			processPtr = processes[i].ptr;
			break;
		}
	}
	
	if(processPtr == NULL){
		sem_post(semap);
		return NULL;
	}
	
	int allocedSize = 1;
	int level = 0;
	
	while(allocedSize <= size){
		allocedSize *= 2;
	}
	
	level = log(allocedSize)/log(2);
	
	if(allocedSize/2 == size)
		level--;
	
	internFrag += (int)(pow(2,level)-size);
	printf("Internal Fragmentation caused by allocation: %d\n",(int)(pow(2,level)-size));
	printf("Total Internal Fragmentation: %d\n",internFrag);
	
	if(allocedSize >= segSize){
		printf("%ld",buffer.st_size);
		printf("No suitable location for this size!\n");
		return NULL;
	}
	
	void* ptr = allocateBuddy(level,processPtr);
	
	sem_post(semap);
	return ptr;
}


void sbmem_free (void *p)
{
	sem_wait(semap);
	void* processPtr;
	pid_t requestPid = getpid();
	
	for(int i = 0; i < MAX_PROCESS; i++){
		if(processExists[i] == 1 && processes[i].pid == requestPid){
			processPtr = processes[i].ptr;
			break;
		}
	}
	
	if(processPtr == NULL){
		return;
	}
	
	
	deallocateBuddy(p,processPtr);
 	sem_post(semap);
}

int sbmem_close()
{
	sem_wait(semap);
   	pid_t closingPid = getpid();
	
	for(int i = 0; i < MAX_PROCESS; i++){
		if(processExists[i] == 1 && processes[i].pid == closingPid){
			void* processPtr = processes[i].ptr;
			struct buddy** avaibles = processPtr;
			
			for(int i = 0; i < MAX_LEVEL; i++){
				if(avaibles[i] != NULL){
					struct buddy* curr = avaibles[i];
		
					while(curr->next != NULL){
						curr = curr->next;
					}
					
					while(curr != NULL){
						deallocateBuddy(curr,processPtr);
						curr = curr->prev;
					}
				}	
			}
			
			munmap(processes[i].ptr,segSize);
		}
	}
	
	sem_post(semap);
    return (0); 
}


void* allocateBuddy(int level, void* ptr){
	
	int mapped = *((int *)(ptr + MAPPED_OFFSET)); 
	struct buddy** avaibles = ptr;
    
    if(!mapped){
    	
    	//SETUP LISTS
    	for(int i = 0; i < MAX_LEVEL;i++){
    		if(avaibles[i] != NULL){
    		avaibles[i] = NULL;
    		}
    		
    	}
    	
    	struct buddy* head = ptr + 13 * sizeof(struct buddy*);
    	head->level = MAX_LEVEL;
    	head->tag = 1;
    	head->loc = ptr + 13 * sizeof(struct buddy*) + MAX_BUDDY*sizeof(struct buddy) + sizeof(int);
    	head->size = segSize;
    	head->next = NULL;
    	head->prev = NULL;
    	avaibles[MAX_LEVEL] = head;
    	buddyCount++;
    	*((int *)(ptr + MAPPED_OFFSET)) = 1;
    }
    
	
		
	int minAvaibleLevel = -1;
	
	for(int i = level; i <= MAX_LEVEL; i++){
		if(avaibles[i] != NULL){
			struct buddy* curr = avaibles[i];
			int freeSpace = 0;
		
			while(curr != NULL){
				if(curr->tag == 1){
					freeSpace = 1;
					break;
				}
				
				curr = curr->next;
			}
			
			
			if(freeSpace){
				minAvaibleLevel = i;
				break;
			}
			
		}
	}
	
	if(minAvaibleLevel == -1){
		return NULL;
	}
	
	if(minAvaibleLevel == level){
			struct buddy* curr = avaibles[minAvaibleLevel];
			
			while(curr != NULL){
				if(curr->tag == 1){
					void* ptr = curr->loc;
					curr->tag = 0;
					curr = NULL;
					return ptr;
				}
						
				curr = curr->next;
			}	
	}
	
	int j = minAvaibleLevel;
	
	while(j != level){
		struct buddy* curr = avaibles[j];
		int freeSpace = -1;
			
		while(curr != NULL){
			if(curr->tag == 1){
				freeSpace = 1;
				break;
			}
					
			curr = curr->next;
		}
		
		int prevSize = curr->size;
		void* prevLoc = curr->loc;
	
		//Delete the Node
		if(freeSpace == -1){
			return NULL;
		}else{
			curr->tag = 0;
			
			if(curr->prev != NULL){
				curr->prev->next = curr->next;
				curr = NULL;
			}else{
				if(curr->next != NULL){
					avaibles[j] = avaibles[j]->next;
				}else{
					avaibles[j] = NULL;
				}
				
			}
		}
		
		j--;
		
		for(int i = 0; i < 2; i++){
			struct buddy* newBuddy = ptr + 13 * sizeof(struct buddy*) + buddyCount*sizeof(struct buddy);
			buddyCount++;
			
			newBuddy->level = j;
	    	newBuddy->tag = 1;
	    	
	    	if(i == 0){
	    		newBuddy->loc = prevLoc;
	    	}else if(i == 1){
	    		newBuddy->loc = (void *)(prevLoc + prevSize/2);
	    	}
	    	
	   		newBuddy->size = pow(2,j);
	   		newBuddy->next = NULL;
	   		newBuddy->prev = NULL;
			
			struct buddy* curr = avaibles[j];
			
			if(curr == NULL){
				avaibles[j] = newBuddy;
			}else{
				while(curr->next != NULL){
					curr = curr->next;
				}
				
				newBuddy->prev = curr;
				curr->next = newBuddy; 
			}
		}
		
			
	}
	
	struct buddy *curr = avaibles[j];
		
	while(curr != NULL){
		if(curr->tag == 1){
			curr->tag = 0;
			return curr->loc;
		} 
				
		curr = curr->next;
	}
	
	
	return NULL;
}

void deallocateBuddy(void* p, void* ptr){

	int mapped = *((int *)(ptr + MAPPED_OFFSET)); 
	struct buddy** avaibles = ptr;
	struct buddy* loc = NULL;
	int level = -1;
	
	if(!mapped){
		return;
	}
	
	for(int i = 0; i < MAX_LEVEL; i++){
		if(avaibles[i] != NULL){
			struct buddy* curr = avaibles[i];
			
			while(curr != NULL){
				if(curr->loc == p){
					loc = curr;
					level = i;
					break;
				}
						
				curr = curr->next;
			}	
		}
	}
	
	if(loc != NULL || level != -1){
		if(loc->prev != NULL){
			if(loc->prev->tag == 1){
				//Combine two buddies
				struct buddy* newBuddy = ptr + 13 * sizeof(struct buddy*) + buddyCount*sizeof(struct buddy);
				buddyCount++;
				
				newBuddy->level = level + 1;
				newBuddy->tag = 1;
				newBuddy->loc = loc->prev->loc;
		   		newBuddy->size = pow(2,level + 1);
		   		newBuddy->next = NULL;
		   		newBuddy->prev = NULL;
		   		
		   		struct buddy* curr = avaibles[level + 1];
				
				if(curr == NULL){ 
					avaibles[level + 1] = newBuddy;
				}else{
					while(curr->next != NULL){
						curr = curr->next;
					}
					
					newBuddy->prev = curr;
					curr->next = newBuddy; 
				}
			}
		}
		
		loc->tag = 1;
	}
}

// USED FOR DEBUG PURPOSES
void debug(struct buddy** avaibles){
	printf("==================================================================\n");
	printf("========================== DEBUG =================================\n");
	printf("==================================================================\n");
	for(int i = 0; i <= MAX_LEVEL;i++){
		if(avaibles[i] == NULL){
			printf("LEVEL %d is NULL\n",i);
		}else{
			printf("Free Mems of the LEVEL %d\n",i);
			
			struct buddy* curr = avaibles[i];
			
			while(curr != NULL){
				printf("Node: %p\n",curr);
				printf("level: %d\n",curr->level);
				printf("tag: %d\n",curr->tag);
				printf("loc: %p\n", curr->loc);
				printf("Next: %p\n", curr->next);
				printf("Prev: %p\n", curr->prev);
				printf("==================================================================\n");
					
				curr = curr->next;
			}
		}
	}
}
