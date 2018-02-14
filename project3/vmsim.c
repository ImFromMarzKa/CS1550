//Riley Marzka
//CS1550
//Project3 (Virtual Memory Simulator)
//Due: 4/2/17 (Sun)

/*Version Notes:
 	* Opt fully functional on both trace files!!!!
 	* Clock fully functional on both trace files!!!!
 	* NRU is functional on both files!!!!
 		* Playing with refresh
 			* 45 best on bzip
 			* 25 best on gcc
 		* Considering faster implementation using booleans
 	* Random functional
*/

/*Description:
	* vmsim.c is a program which simulates virtual memory.
	* The overall purpose of this project is to simulate and compare 4
		page replacement algorithms: Optimal, Clock, Not Recently Used, and Random
	* The program simulates a 32-bit Virtual Address space with 4KB sized pages
	* The number of frames to be used in our simulated RAM is passed as a
		command line argument at execution time
	* The program reads a series of memory accesses from a file (also passed
		as a command line argument). After each memory address within the file, 
		there is either a 'W' or an 'R' indicating whether the memory access
		was a read or a write
	* The program then performs these simulated memory accesses
	* When page faults occur, the program simulates one of the page replacement 
		algorithms (passed as command line argument) and collects statistics as it runs. 
	* After all memory accesses have been completed, the program prints out
		the statistics it collected so that quantifiable comparisons may
		be performed across the various page replacement algorithms. 
*/

/*Commandline Argument>
	* ./vmsim –n <num_frames> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>
 
 *Prints statistics in this format>
	* Algorithm: Clock
	* Number of frames:       8
	* Total memory accesses:  1000000
	* Total page faults:      181856
	* Total writes to disk:   29401
*/

#include "vmsim.h"

// Comment below to see more logs
#undef ALL
//#undef DEBUG
#undef DEBUG_A
#undef DEBUG_O
#undef DEBUG_C
#undef DEBUG_N
#undef DEBUG_R
#undef INFO

int num_frames;
int refresh_limit = 0;
unsigned int *phys_frames;
char alg;

//Page Table
unsigned int *page_table = NULL;
//Page Table Entry
struct pte_32 *pte = NULL;

//Array of linked lists, indexed by page number for OPT
struct opt_list *call_lists[PT_SIZE_1MB] = {NULL}; 

//Linked list for Clock Algorithm
struct clock_node *c_12 = NULL; //Head
struct clock_node *c_hand = NULL; //Clock hand

int main(int argc, char *argv[]){

	//Sanity check for valid arguments
	if(!checkArgs(argc, argv)){
		fprintf(stderr, "USAGE: %s -n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>\n", argv[0]);
		exit(1);
	}

	//Set algorithm character
	//(avoids repeated calls to strcmp())
	if(!strcmp(argv[4], "opt")){
		alg = 'o';
	}
	else if(!strcmp(argv[4], "clock")){
		alg = 'c';
	}
	else if(!strcmp(argv[4], "nru")){
		alg = 'n';
	}
	else{
		alg = 'r';
	}

	//Open memory trace file
	FILE *file;
	file = fopen(argv[argc - 1], "rb");
	if(!file){
		fprintf(stderr, "Error on opening the trace file\n");
		exit(1);
	}

	unsigned int num_accesses = 0;
	unsigned int addr = 0;
	unsigned char mode;

	//Calculate number of lines in trace file
	while(fscanf(file, "%x %c", &addr, &mode) == 2){
		num_accesses++;
	}
	rewind(file);

	#ifdef DEBUG
		printf("Number of memory accesses = %d\n\n", num_accesses);
	#endif

	unsigned int addr_arr[num_accesses]; //Holds memory addresses
	unsigned char mode_arr[num_accesses]; //Holds mode for each access ('W' or 'R')
	unsigned int i = 0;

	//Store memory accesses to arrays
	while(fscanf(file, "%x %c", &addr_arr[i], &mode_arr[i]) == 2){
		#ifdef ALL
			printf("\'0x%08x %c\'\n", addr_arr[i], mode_arr[i]);
		#endif
		i++;
	}

	//Close trace file
	if(!fclose(file)){
		;
	}
	else{
		fprintf(stderr, "Error on closing the trace file\n");
		exit(1);
	}

	//Initialize physical memory address space
	phys_frames = malloc(PAGE_SIZE_4KB * num_frames);
	if(!phys_frames){
		fprintf(stderr, "Error on mallocing physical frames\n");
		exit(1);
	}
	memset(phys_frames, 0, PAGE_SIZE_4KB * num_frames);

	//Create first frame of frames linked list
	struct frame_struct *frame = malloc(sizeof(struct frame_struct));
	if(!frame){
		fprintf(stderr, "Error on mallocing frame struct\n");
		exit(1);
	}
	memset(frame, 0, sizeof(struct frame_struct));

	//Store head of frames linked list
	struct frame_struct *head = frame;
	struct frame_struct *curr;

	//Populatee frames linked list
	for(i = 0; i < num_frames; i++){
		frame->frame_number = i;
		frame->physical_address = phys_frames + (i * PAGE_SIZE_4KB) / PAGE_SIZE_BYTES;
		frame->virtual_address = 0;
		frame->pte_pointer = NULL;
		#ifdef INFO
         printf("Frame#%d: Adding a new frame at memory address %ld(0x%08x)\n", i, frame->physical_address, frame->physical_address);
		#endif

        frame->next = malloc(sizeof(struct frame_struct));
        if(!frame->next){
        	fprintf(stderr, "Error on mallocing frame struct for list\n");
			exit(1);
        }

        frame = frame->next;
        memset(frame, 0, sizeof(struct frame_struct));
	}

	//Initialize clock linked list
	if(alg == 'c'){
		//Initialize first node in clock list
		c_12 = (struct clock_node *)malloc(sizeof(struct clock_node));
		c_12->frame = head;

		c_hand = c_12;
		curr = head->next;

		//Loop for number of frames in frames linked list
		for(i = 1; i < num_frames; i++){
			c_hand->next = (struct clock_node *)malloc(sizeof(struct clock_node));
			c_hand = c_hand->next;
			c_hand->frame = curr;
			curr = curr->next;
		}

		//Circular list
		c_hand->next = c_12;
		c_hand = c_12; //Reset hand to beginning of list
	}

	//Initialize page table
	page_table = malloc(PT_SIZE_1MB * PTE_SIZE_BYTES);
	if(!page_table){
		fprintf(stderr, "Error on mallocing page table\n");
		exit(1);
	}
	memset(page_table, 0, PT_SIZE_1MB * PTE_SIZE_BYTES);

	//Initialize linked list array for opt
	if(alg == 'o'){
		/*Preprocess address array to create 
	  		a linked list of instruction numbers
	  		of calls for each page*/

		#ifdef DEBUG_O
			printf("Populating call lists:\n");
		#endif
		
		//Counter and index variables
		int j, page_ind;
		for(j = 0; j < num_accesses; j++){

			//Index = address / page size
			page_ind = addr_arr[j] / PAGE_SIZE_4KB;

			//If no list at index, create a new list
			if(!call_lists[page_ind]){
				call_lists[page_ind] = (struct opt_list *)malloc(sizeof(struct opt_list));
				if(!call_lists[page_ind]){
					fprintf(stderr, "Error on mallocing opt list\n");
					exit(1);
				}

				//Create new node and add as head and tail of list
				call_lists[page_ind]->head = (struct opt_node *)malloc(sizeof(struct opt_node));
				if(!call_lists[page_ind]->head){
					fprintf(stderr, "Error on mallocing node for opt list\n");
					exit(1);
				}

				call_lists[page_ind]->tail = call_lists[page_ind]->head;
			}
			else{
				//If list exists, create a new node and add it to the end 
				call_lists[page_ind]->tail->next = (struct opt_node *)malloc(sizeof(struct opt_node));
				if(!call_lists[page_ind]->tail->next){
					fprintf(stderr, "Error on mallocing node for opt list\n");
					exit(1);
				}

				call_lists[page_ind]->tail = call_lists[page_ind]->tail->next;
			}
			
			//Set the instruction number for the new node
			call_lists[page_ind]->tail->inst_num = j;
			call_lists[page_ind]->tail->next = NULL;
		}

		#ifdef DEBUG_O
			printf(">Call lists populated\n");
		#endif
	}

	struct pte_32 *new_pte = NULL;

	unsigned int fault_addr = 0;
	unsigned int prev_fault_addr = 0;
	unsigned char mode_type;
	int hit = 0;
	int page_2_evict = 0;
	int num_faults = 0;
	int num_writes = 0;

	//Counter for NRU refresh
	int refs_since_refresh = 0;

	#ifdef DEBUG_N
		printf("Refresh = %d memory references\n", refresh_limit);
	#endif

	#ifdef DEBUG_A
		printf("Entering Main Loop:\n");
	#endif

	//Main loop to process memory accesses
	for(i = 0; i < num_accesses; i++){

		//Check refresh
		if(alg == 'n' && refs_since_refresh >= refresh_limit){
			#ifdef DEBUG_N
				printf("............refs_since_refresh = %d\n", refs_since_refresh);
			#endif
			refresh(head);
			refs_since_refresh = 0;
		}

		#ifdef DEBUG_A
			printf("Accessing address and mode arrays:\n");
		#endif

		fault_addr = addr_arr[i];
		mode_type = mode_arr[i];
		hit = 0;

		#ifdef DEBUG_A
			printf("Performing page walk:\n");
		#endif

		//Perform page walk for fault address
		new_pte = (struct pte_32 *) handle_page_fault(fault_addr);

		#ifdef DEBUG_A
			printf("Searching Frames List:\n");
		#endif

		//Search frames linked list for requested page
		curr = head;
		while(curr->next){
			if(curr->physical_address == new_pte->physical_address){
				if(new_pte->present){
					curr->virtual_address = fault_addr;
					hit = 1;
				}
				break;
			}
			else{
				curr = curr->next;
			}
		}

		//If requested page not in frames linked list
		if(!hit){

			#ifdef DEBUG_A
				printf("Page Fault:\n");
			#endif

			//Check page replacement algorithm and act accordingly
			if(alg == 'o'){
				#ifdef DEBUG_O
					printf("\nCalling opt()\n");
				#endif

				page_2_evict = opt(head);
			}

			else if(alg == 'c'){
				#ifdef DEBUG_C
					printf("\nCalling clock()\n");
				#endif

				page_2_evict = clock();
			}

			else if(alg == 'n'){
				#ifdef DEBUG_N
					printf("\nCalling nru()\n");
				#endif

				page_2_evict = nru(head);
			}

			//Random Algorithm
			else{
				//Generate random number between 0 and num_frames
				//Evict page in that frame number
				page_2_evict = rand() % num_frames;
			}

			#ifdef DEBUG_A
				printf(">>>>Evicting page %d\n\n", page_2_evict);
				fflush(stdout);
			#endif

			//Search frames linked list for victim frame
			curr = head;
			while(curr->next){
				//If page found, set present, referenced, and dirty bits, 
				//and collect statistics
				if(curr->frame_number == page_2_evict){
					prev_fault_addr = curr->virtual_address;
					num_faults++;

					if(curr->pte_pointer){
						curr->pte_pointer->present = 0;
						curr->pte_pointer->referenced = 0;
						if(curr->pte_pointer->dirty){
							curr->pte_pointer->dirty = 0;
							num_writes++;
							#ifdef DEBUG
                        		printf("%5d: page fault – evict dirty(0x%08x)accessed(0x%08x)\n", i, prev_fault_addr, fault_addr);
							#endif 
						}
						else{
							#ifdef DEBUG
                        		printf("%5d: page fault – evict clean(0x%08x)accessed(0x%08x)\n", i, prev_fault_addr, fault_addr);
							#endif
						}
					}
					else{
						#ifdef DEBUG
							printf("%5d: page fault - no eviction\n", i);
						#endif
					}

					curr->pte_pointer = (struct pte_32 *) new_pte;
					new_pte->physical_address = curr->physical_address;
					new_pte->present = 1;
					curr->virtual_address = fault_addr;

					if(mode_type == 'W'){
						new_pte->dirty = 1;
					}
				}
				curr = curr->next;
			}
		}
		else{
			#ifdef DEBUG
           		printf("%5d: page hit   – no eviction(0x%08x)\n", i, fault_addr);
            	printf("%5d: page hit   - keep page  (0x%08x)accessed(0x%08x)\n", i, new_pte->physical_address, curr->virtual_address);
			#endif 
		}

		//Increment number of memory accesses since refresh
		refs_since_refresh++;
	}

	//All memory accesses complete > 
	// print statistics
   printf("Algorithm:             %s\n", argv[4]);
   printf("Number of frames:      %d\n", num_frames);
   printf("Total memory accesses: %d\n", i);
   printf("Total page faults:     %d\n", num_faults);
   printf("Total writes to disk:  %d\n", num_writes);

	return(0);
}









/*Optimum Algorithm > Simulates optimal page replacement algorithm with perfect knowledge
	* Evicts the page whose next use is farthest in the future
	* Uses an array of linked lists 
		* Indexed by (VM address of each page / page size)
			* Effectively "page number"
	* Each index of the array contains a linked list of nodes (one linked list for every page)
		* Each node contains the line number of the next access to that page's VM address
			as well as a reference to the next node in the list
	* The algorithm looks at the first node of each linked list belonging to a page that is
		currently loaded into a frame, searching for the maximum line number
	* When the max is found, the number of the frame that contains that page is returned,
		so that page may be evicted.

	* Parameters:
		* frame = head of frames linked list
	* Returns:
		* Frame number of page to be evicted
*/
int opt(struct frame_struct *frame){
	unsigned int max = 0; //Tracks farthest distance of call
	int page_ind;
	int to_evict = -1; //Represents frame number to evict

	//For each frame in linked list;
	while(frame->next){

		//Compulsory Miss:
		if(!frame->pte_pointer){
			return frame->frame_number;
		}

		//Capacity Miss:

		//Index = VA of page in current frame / page size
		page_ind = frame->virtual_address / PAGE_SIZE_4KB;

		//Last call to this page has passed, making it the optimal
		// page to evict b/c it will never be used again 
		if(!call_lists[page_ind]->head){
			return frame->frame_number;
		}

		//Check line number of next call against max
		if(max < call_lists[page_ind]->head->inst_num){
			to_evict = frame->frame_number;
			max = call_lists[page_ind]->head->inst_num;
		}

		frame = frame->next;
	}

	return to_evict;
}


/*Clock Algorithm > Better implementation of second-chance algorithm
	* Uses circular queue
		* Maintains total ordering, traversing clockwise
		* Newest page always one step CCW from oldest
		* No enqueue or dequeue
	* Uses R-Bit
		* Give second chance if referenced
			* Unset R-Bit
		* Try eviction again, looping until unreferenced
	* Evict oldest unreferenced page

	* Parameters:
		* None
	* Returns:
		* Frame number of page to be evicted
*/
int clock(){
	int p2e; //Page to evict
	
	//Compulsory Miss:
	if(c_hand->frame->pte_pointer == NULL){
		p2e = c_hand->frame->frame_number;
		c_hand = c_hand->next;
	}
	
	//Capacity Miss:
	else{
		//Loop until unreferenced page
		while(c_hand->frame->pte_pointer->referenced){
			c_hand->frame->pte_pointer->referenced = 0;
			c_hand = c_hand->next;
		}
		p2e = c_hand->frame->frame_number;
	}

	return p2e;	
}


/*Not Recently Used Algorithm > Picks not recently used page using referenced and dirty bits
	* At page fault, find all pages such that R = 0 and choose one to evict
		* Order of Preference:
			1) R = 0 && D = 0
			2) R = 0 && D = 1
			3) R = 1 && D = 0
			4) R = 1 && D = 1
		* In case of tie, choice is arbitrary
	* After refresh period elapses, all R-Bits are set to 0
		* This is managed within main()
		* Number of memory accesses between refreshes passed as command line argument

	* Parameters:
		* head = pointer to head of frames linked list
	* Returns:
		* Frame number of page to evict
*/
int nru(struct frame_struct *head){
	//Boolean value to determine if any of the frames
	// contain a page whose reference bit is 0
	int exists_unref = 0; 
	struct frame_struct *curr = head;

	//Frame struct array to store candidates for eviction
	struct frame_struct *cndts[num_frames];
	int i = 0;

	//For each frame in frames linked list;
	while(curr->next){
		//Compulsory Miss:
		if(curr->pte_pointer == NULL){
			#ifdef DEBUG_N
				printf("Compulsory Miss\n");
			#endif

			return curr->frame_number;
		}

		//Capacity Miss:

		//If the current frame contains a page which is unreferenced
		if(!curr->pte_pointer->referenced){
			//Add frame to candidates array
			cndts[i++] = curr;
			//Set boolean to true, indicating an unreferenced page
			exists_unref = 1;
		}
		curr = curr->next;
	}

	//If all frames referenced
	if(exists_unref == 0){
		curr = head;

		//Traverse frames again, searching for clean page
		while(curr->next){
			//If current frame contains clean page,
			if(!curr->pte_pointer->dirty){
				//Add frame to candidates array
				cndts[i++] = curr;
			}
			curr = curr->next;
		}

		//All frames referenced and dirty (last choice)
		//>Evict last frame
		if(i == 0){
			return num_frames - 1;
		}
	}

//////DEBUGGING
	// print_frames(head, 'n');
	// int k;
	// printf("Candidates by Frame Number:\n[");
	// for(k = 0; k < i; k++){
	// 	printf("%d,", cndts[k]->frame_number);
	// }
	// printf("]\n");
//////END

	int j;

	//If at least one page is unreferenced
	if(exists_unref){
		//Check for first choice (R = 0 && D = 0)
		for(j = 0; j < i; j++){
			if(!cndts[j]->pte_pointer->referenced && !cndts[j]->pte_pointer->dirty){
				return cndts[j]->frame_number;
			}
		}

		//Check for second choice (R = 0 && D = 1)
		for(j = 0; j < i; j++){
			if(!cndts[j]->pte_pointer->referenced){
				return cndts[j]->frame_number;
			}
		}
	}

	//Check for third choice (R = 1 && D = 0)
	for(j = 0; j <= i; j++){
		if(!cndts[j]->pte_pointer->dirty){
			return cndts[j]->frame_number;
		}
	}
}





/*Helper function to handle page faults
 * Parameters:
 	* fault_addr = the fault address
 * Returns:
 	* a frame struct pointer for pte
*/
struct frame_struct * handle_page_fault(unsigned int fault_addr){
	//Pull page, with VM address = fault_addr, from page table 
	pte = (struct pte_32 *) page_table[PTE32_INDEX(fault_addr)];

	//If page is not yet in page table
	if(!pte){
		//Create a new page table entry
		pte = (struct pte_32 *)malloc(sizeof(struct pte_32));
		if(!pte){
			fprintf(stderr, "Error on mallocing page table entry\n");
			exit(1);
		}
		memset(pte, 0, sizeof(struct pte_32));

		//Set values and add page to page table
		pte->present = 0;
		pte->physical_address = NULL;
		page_table[PTE32_INDEX(fault_addr)] = (unsigned int) pte;
	}

	#ifdef INFO
      printf("\nPage fault handler\n");
      printf("Fault address %d(0x%08x)\n", (unsigned int) fault_addr, fault_addr);
      printf("Page table base address %ld(0x%08x)\n", (unsigned int) page_table, page_table);
      printf("PTE offset %ld(0x%03x)\n", PTE32_INDEX(fault_addr), PTE32_INDEX(fault_addr));
      printf("PTE index %ld(0x%03x)\n",  PTE32_INDEX(fault_addr) * PTE_SIZE_BYTES, PTE32_INDEX(fault_addr) * PTE_SIZE_BYTES);
      printf("PTE virtual address %ld(0x%08x)\n", (unsigned int) page_table + PTE32_INDEX(fault_addr), page_table + PTE32_INDEX(fault_addr));

      printf("PAGE table base address %ld(0x%08x)\n", pte->physical_address, pte->physical_address);
      printf("PAGE offset %ld(0x%08x)\n", FRAME_INDEX(fault_addr), FRAME_INDEX(fault_addr));
      printf("PAGE index %ld(0x%08x)\n", FRAME_INDEX(fault_addr) * PTE_SIZE_BYTES, FRAME_INDEX(fault_addr) * PTE_SIZE_BYTES);
      printf("PAGE physical address %ld(0x%08x)\n", pte->physical_address + FRAME_INDEX(fault_addr), pte->physical_address  + FRAME_INDEX(fault_addr));
	#endif

    //Set referenced bit
    pte->referenced = 1;

    //Remove this call from the call list for opt
    if(alg == 'o'){
    	int page_ind = fault_addr / PAGE_SIZE_4KB;
    	call_lists[page_ind]->head = call_lists[page_ind]->head->next;
    }

    //Return the page
    return ((struct frame_struct *) pte);
}


/*Helper function to perform sanity check on arguments
 * Checks that all command line arguments are valid
 * Sets num_frames and refresh values
 * Prints applicable error reports to stderr

 * Parameters:
 	* num = number of arguments (argc)
 	* args = array of arguments (argv[])
 * Returns:
 	* 0 if invalid arguments
 	* 1 if valid arguments
*/
int checkArgs(int num, char *args[]){
	//Check for -n and -a
	if(strcmp(args[1], "-n")){
		return 0;
	}
	if(strcmp(args[3], "-a")){
		return 0;
	}

	//Parse number of frames from commandline
	num_frames = atoi(args[2]);
	if(num_frames <= 0){
		fprintf(stderr, "Number of frames must be a positive integer\n");
		return 0;
	}

	//Check for valid file names at appropriate index
	int file_ind = num - 1;
	if(strcmp(args[file_ind], "gcc.trace") && strcmp(args[file_ind], "bzip.trace")){
		fprintf(stderr, "Invalid file passed, must be either 'gcc.trace' or 'bzip.trace'\n");
		return 0;
	}

	//Check algorithm name based on number of arguments passed
	if(num == 6){
		if(strcmp(args[4], "opt") && strcmp(args[4], "clock") && strcmp(args[4], "rand")){
			fprintf(stderr, "Invalid algorithm passed\n");
			if(!strcmp(args[4], "nru")){
				fprintf(stderr, "Must pass refresh argument to run with NRU algorithm\n");
			}
			return 0;
		}
		refresh_limit = -1;
	}
	else if(num == 8){
		if(strcmp(args[4], "nru")){
			fprintf(stderr, "Only pass refresh argument when running with NRU algorithm\n");
			return 0;
		}
		//Check refresh argument
		if(strcmp(args[5], "-r")){
			fprintf(stderr, "No refresh argument passed for NRU algorithm\n");
			return 0;
		}
		//Parse refresh value from commandline
		refresh_limit = atoi(args[6]);
		if(refresh_limit <= 0){
			fprintf(stderr, "Refresh argument must be a positive integer representing\n");
			fprintf(stderr, "the number of memory references to perform before a refresh for NRU\n");
			return 0;
		}
	}
	//Invalid number of arguments
	else{
		fprintf(stderr, "Invalid number of arguments\n");
		return 0;
	}

	//If this point is reached, 
	//then all arguments are valid > return true
	return 1;
}

/*Helper Function to reset referenced bits on refresh
	* Parameters:
		* frame = pointer to head of frames linked list
*/
void refresh(struct frame_struct *frame){
	//Traverse frames list, setting all R-bits to 0
	while(frame->next){
		//If an empty frame is reached, we are done
		if(!frame->pte_pointer){
			return;
		}
		frame->pte_pointer->referenced = 0;
		frame = frame->next;
	}
}

//For Debugging
void print_frames(struct frame_struct *head, char alg){
	if(alg == 'n'){
		printf(">>>>Printing frames for NRU:\n");
		printf(">>>>[Frame #|Referenced|Dirty]\n");
		while(head->next){
			printf("[%d|%d|%d]->", head->frame_number, head->pte_pointer->referenced, head->pte_pointer->dirty);
			head = head->next;
		}
		printf("\n\n");
	}
}