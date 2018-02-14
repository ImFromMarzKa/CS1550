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
*/

// #include <linux/kernel.h>
#include <linux/prodcons.h>
#include <stdlib.h>
#include <stdio.h>
// #include <string.h>

int numCons = 0;
int numProds = 0;
int bufSize = 0;
void *ptr;
void *share_buff;

int main(int argc, char** argv){
	numCons = atoi(argv[0]);

	numProds = atoi(argv[1]);

	bufSize = atoi(argv[2]);
	
	//Need to make buffer and semaphores
	//>need multiple processes to share same memory region
	int N; // = number of bytes we need to map
	//Need enough memory for 3 semaphores and a buffer
	N = 3*sizeof(struct cs1550_sem);
	// N += bufSize;

	ptr = mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	//Return value is the address to the start of this page in RAM

	struct cs1550_sem *empty;
	struct cs1550_sem *full;
	struct cs1550_sem *mutex;
	// int buffer[bufSize];

	empty = ptr;
	full = empty + sizeof(struct cs1550_sem);
	mutex = full + sizeof(struct cs1550_sem);
	// buffer = mutex + sizeof(struct cs1550_sem);

	empty->value = bufSize;
	empty->front = NULL;
	empty->end = NULL;

	full->value = 0;
	full->front = NULL;
	full->end = NULL;

	mutex->value = 1;
	mutex->front = NULL;
	mutex->end = NULL;

	//Allocate space for shared buffer
	N = sizeof(int)*(bufSize + 3);
	share_buff = mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	int *size = (int *)share_buff;
	int *prod_ind = (int *)share_buff + 1;
	int *cons_ind = (int *)share_buff + 2;
	int *buff_ptr = (int *)share_buff + 3;

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
				down(empty);
				down(mutex);

				pitem = *prod_ind;
				buff_ptr[*prod_ind] = pitem;
				printf("Producer %c Produced: %d", i+65, pitem);
				*prod_ind = (*prod_ind + 1) % *size;

				//leave critical region
				up(mutex);
				up(full);
			}
		}
	}

	for(j = 0; j < numCons; j++){
		//child
		if(fork() == 0){

			while(1){
				//enter critical region
				down(full);
				down(mutex);

				citem = buff_ptr[*cons_ind];
				printf("Consumer %c Consumed: %d", j+65, citem);
				*cons_ind = (*cons_ind+ 1) % *size;

				//leave critical region
				up(mutex);
				up(empty);
			}
		}
	}

}


//Puts process to sleep
void cs1550_down(cs1550_sem *sem){
	syscall(__NR_cs1550_down, sem);
}


//Wakes up sleeping process
void cs1550_up(cs1550_sem *sem){
	syscall(__NR_cs1550_up, sem);
}