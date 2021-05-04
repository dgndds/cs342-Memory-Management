
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h> 
#include <math.h> 
#include "sbmem.h"

int main()
{
	srand(time(NULL));
    int i, ret; 
    char *p,*p2,*p3,*p4;  	

    ret = sbmem_open(); 
    if (ret == -1)
	exit (1); 
    
    int randomAlloc1 = rand() % 4096 + 1;
    p = sbmem_alloc (randomAlloc1); 
   
	int randomAlloc2 = rand() % 4096 + 1;
	p2 = sbmem_alloc (randomAlloc2);
	
	int randomAlloc3 = rand() % 4096 + 1;
	p3 = sbmem_alloc (randomAlloc3);
	
	int randomAlloc4 = rand() % 4096 + 1;
	p4 = sbmem_alloc(randomAlloc4);
	
	printf("p: %p\n",p);
	printf("p2: %p\n",p2);
	printf("p3: %p\n",p3);
	printf("p4: %p\n",p4);
	
	if(p2 != NULL){
    for (i = 0; i < randomAlloc2; ++i)
		p2[i] = 'b'; 
	}
	
	if(p3 != NULL){
	for (i = 0; i < randomAlloc3; ++i)
		p3[i] = 'c';
	}
	
	if(p4 != NULL){
		for (i = 0; i < randomAlloc4; ++i)
		p4[i] = 'd';
	}
	
    
   if(p2 != NULL){
   	for (i = 0; i < randomAlloc2; ++i)
		printf("%c",p2[i]);
   }
   
   printf("\n");
   if(p != NULL){
	   for (i = 0; i < randomAlloc1; ++i)
			printf("%c",p[i]);
   }
   
   printf("\n");
   if(p3 != NULL){
   	
	for (i = 0; i < randomAlloc3; ++i)
		printf("%c",p3[i]);
   }       
   
   printf("\n");
   if(p4 != NULL){
	for (i = 0; i < randomAlloc4; ++i)
		printf("%c",p4[i]);
   }

	printf("\n");
		
    sbmem_free (p);
    sbmem_free (p2);
    sbmem_free (p3);
    sbmem_free (p4);

    sbmem_close(); 
    
    return (0); 
}
