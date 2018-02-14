/*
 Riley Marzka
 ImFromMarzKa
 CS1550
 Project 2 (Syscalls and IPC)
 Due: 2/26/17 (Sun) @ 11:59
*/

//Semaphore struct
struct semaphore{
	int value;
	//PROCESS QUEUE
	struct Node *queue;
};

//Represents a process put to sleep in down()
struct task_struct{
	int pID;
};


//global to get current process' task_struct
//WILL NEED TO SAVE THESE SOMEWHERE
struct task_struct *current;


int main(int argc, char** argv){
	int numCons = atoi(**argv);
	argv++;

	int numProds = atoi(**argv);
	argv++;

	int bufSize = atoi(**argv);
	
	//Need to make buffer and semaphores
	//>need multiple processes to share same memory region
	int N; // = number of bytes we need to map
	//Need enough memory for 3 semaphores and a buffer
	N = 3*sizeof(struct semaphore);
	N += bufSize;

	void *ptr = mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	//Return value is the address to the start of this page in RAM

	struct semaphore *empty;
	struct semaphore *full;
	struct semaphore *mutex;
	int *buffer;

	empty = ptr;
	full = empty + sizeof(struct semaphore);
	mutex = full + sizeof(struct semaphore);
	buffer = mutex + sizeof(struct semaphore);

	//fork() must be after this point

}

//Puts process to sleep
void down(semaphore *sem){
	
	//1) Mark the task as not ready (but can be woken up by signals)
	set_current_state(TASK_INTERRUPTABLE);

	//2) Invoke scheduler to pick a ready task
	schedule();

	syscall();
}


//Wakes up sleeping process
void up(){
	//Attempt wakeup of sleeping process
	wake_up_process(sleeping_task);
}