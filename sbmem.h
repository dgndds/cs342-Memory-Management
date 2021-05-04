


int sbmem_init(int segmentsize); 
int sbmem_remove(); 

void* allocateBuddy(int level, void* ptr);
void deallocateBuddy(void* p, void* ptr);
int sbmem_open(); 
void *sbmem_alloc (int size);
void sbmem_free(void *p);
int sbmem_close();
void debug();




    
    
