/*
 Riley Marzka
 ImFromMarzKa
 CS1550
 Project 2 (Syscalls and IPC)
 Due: 2/26/17 (Sun) @ 11:59
*/

/*Version Notes:
	- For some reason gcc cannot find prodcons.h in include/linux
	  even though it is definitely in there. Going to try implementing
	  without prodcons.h
	- Implementing without prodcons.h caused more problems than it solved

*/

#include <linux/prodcons.h>
#include <stdlib.h>
#include <stdio.h>


//declare shared globals
int numCons = 0;
int numProds = 0;
int bufSize = 0;
void *ptr;
void *share_buff;

int main(int argc, char** argv){

	//Grab number of consumers, producers, and buffer size
	//from command line
	numCons = atoi(argv[1]);
	numProds = atoi(argv[2]);
	bufSize = atoi(argv[3]);
	
	//Need to make buffer and semaphores
	//>need multiple processes to share same memory region
	int N; // = number of bytes we need to map

	//Need enough memory for 3 semaphores and a buffer
	N = (int)(3*sizeof(struct cs1550_sem));

	ptr = (void *)mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	//Return value is the address to the start of this page in RAM

	//Declare needed semaphores
	struct cs1550_sem *empty;
	struct cs1550_sem *full;
	struct cs1550_sem *mutex;

	//Initialize to locations in shared mem
	empty = ptr;
	full = empty + sizeof(struct cs1550_sem);
	mutex = full + sizeof(struct cs1550_sem);

	//initialize value of empty to buffer size
	empty->value = bufSize;
	empty->front = NULL;
	empty->end = NULL;

	//initialize value of full to 0
	full->value = 0;
	full->front = NULL;
	full->end = NULL;

	//initialize value of mutex to 1
	mutex->value = 1;
	mutex->front = NULL;
	mutex->end = NULL;

	//Allocate space for shared buffer
	N = (int)(sizeof(int)*(bufSize + 3));
	share_buff = (void *)mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	//initialize pointers for the size of the buffer,
	//the producer's working index,
	//the consumer's working index,
	//And the start of the buffer
	//to locations in shared mem
	int *size = (int *)share_buff;
	int *prod_ind = (int *)share_buff + 1;
	int *cons_ind = (int *)share_buff + 2;
	int *buff_ptr = (int *)share_buff + 3;

	//initialize values of pointers so
	//both producer and consumer begins at index 0
	*prod_ind = 0;
	*cons_ind = 0;
	*size = bufSize;

	//fork() must be after this point
	int i = 0;
	int j = 0;
	int pitem;
	int citem;

	//fork producers
	for(i = 0; i < numProds; i++){
		//child
		if(fork() == 0){
			while(1){
				//enter critical region
				cs1550_down(empty);
				cs1550_down(mutex);

				//produce item into buffer
				pitem = *prod_ind;
				buff_ptr[*prod_ind] = pitem;

				printf("Producer %c Produced: %d\n", i+65, pitem);
				
				//TROUBLESHOOTING
				fflush(stdout);

				//traverse to next index in buffer
				*prod_ind = (*prod_ind + 1) % *size;

				//leave critical region
				cs1550_up(mutex);
				cs1550_up(full);
			}
		}
	}

	for(j = 0; j < numCons; j++){
		//child
		if(fork() == 0){
			while(1){
				//enter critical region
				cs1550_down(full);
				cs1550_down(mutex);

				//consume item from buffer
				citem = buff_ptr[*cons_ind];

				printf("Consumer %c Consumed: %d\n", j+65, citem);
				
				//TROUBLESHOOTING
				fflush(stdout);

				//Traverse to next index in buffer
				*cons_ind = (*cons_ind+ 1) % *size;

				//leave critical region
				cs1550_up(mutex);
				cs1550_up(empty);
			}
		}
	}

}

//Wrapper Functions:

//Puts process to sleep
void cs1550_down(struct cs1550_sem *sem){
	syscall(__NR_sys_cs1550_down, sem);
}


//Wakes up sleeping process
void cs1550_up(struct cs1550_sem *sem){
	syscall(__NR_sys_cs1550_up, sem);
}