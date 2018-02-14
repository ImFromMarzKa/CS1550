#include <stdio.h>
#include <stdlib.h>

struct Node{
	int grade;
	struct Node *next;
}n;

void printAvg(struct Node *start, int _length);
void cleanUp(struct Node *node, struct Node *prev, int _length, int ogLength);

int main(){
  int inp = 0, length = 0;

	/*Create 3 Node pointers which all point to the same (empty) initial Node*/
	struct Node *first = (struct Node*)malloc(sizeof(struct Node));
	struct Node *last = first;
	struct Node *curr = first;

	/*Loop until user enters (-1)*/
	userInputLoop:
	printf("Please enter an integer (-1 to quit):");
	scanf("%i", &inp);
	
	/*If user did not quit*/
	if(inp != (-1)){

	  /*if this is the first entry > just set first->grade to entry*/
	  if(length == 0){
	    first->grade = inp;
	  }

	  /*If not first entry > create new Node; set new grade to entry; link; update last Node*/
	  else{
	    curr = (struct Node*)malloc(sizeof(struct Node));
	    curr->grade = inp;
	    last->next = curr;
	    last = curr;
	  }

	  /*Increment length of list and loop*/
	  length++;
	  goto userInputLoop;
	}

	/*if user wuit (entered -1) > set last.next to NULL*/
	else{
	  last->next = NULL;
	}
	
	/*Calculate and print the average then free the memory allocated for the list*/
	printAvg(first, length);
	cleanUp(first->next, first, length, length);
}

/*Traverse list, adding each grade to the total, then print the average based on total and length*/
void printAvg(struct Node *start, int _length){
  struct Node *currNode = start;
  int total = 0, i;
  for(i = 0; i < _length; i++){
    total += currNode->grade;
    currNode = currNode->next;
  }
  printf("Average Grade: %d\n", total/_length);
   
}

/*Recursively delete list from last Node to first Node*/
void cleanUp(struct Node *node, struct Node *prev, int _length, int ogLength){
 
  /*Recursive Case: 'node' not at the last Node*/
  if(node->next != NULL){
    cleanUp(node->next, node, _length-1, ogLength); 
  }

  /*Base Case: 'node' is the last Node*/
  free(node);
  prev->next = NULL;

  /*Delete the first Node*/
  if(_length == ogLength){
    free(prev);
  }
}
