//Riley Marzka
//CS1550
//Project3 (Virtual Memory Simulator)
//Due: 4/2/17 (Sun)

//Header File for vmsim.c

/* Level 1 Page Table   PAGE FRAME
 * 31------------- 12 | 11 ------------- 0
 * |PAGE TABLE ENTRY  | PHYSICAL OFFSET  |
 * -------------------------------------
 * <-------20-------> | <-------12------->
 *
*/

#ifndef _VMSIM_INCLUDED_H
#define _VMSIM_INCLUDED_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Debug levels
#define ALL
#define DEBUG
#define DEBUG_A
#define DEBUG_O
#define DEBUG_C
#define DEBUG_N	
#define DEBUG_R				
#define INFO

// 32-Bit page table constants
#define PAGE_SIZE_4KB   4096
#define PT_SIZE_1MB     1048576
#define PAGE_SIZE_BYTES 4
#define PTE_SIZE_BYTES  4

// Macros to extract pte/frame index
#define PTE32_INDEX(x)  (((x) >> 12) & 0xfffff)
#define FRAME_INDEX(x)  ( (x)        &   0xfff)

// 32-Bit memory frame data structure
struct frame_struct
{
   unsigned int frame_number;
   unsigned int *physical_address;
   unsigned int virtual_address;
   struct pte_32 *pte_pointer;
   struct frame_struct *next;
};

// 32-Bit Root level Page Table Entry (PTE) 
struct pte_32
{
   unsigned int present;
   unsigned int dirty;
   unsigned int referenced;
   unsigned int *physical_address;
};

//Linked list data structure for OPT
struct opt_list{
	struct opt_node *head;
	struct opt_node *tail;
};

//Node for OPT
struct opt_node{
	unsigned int inst_num;
	struct opt_node *next;
};

//Node/linked list for Clock 
struct clock_node{
	struct frame_struct *frame;
	struct clock_node *next;
};

// Handle page fault function
struct frame_struct * handle_page_fault(unsigned int);

//Optimal Algorithm
int opt(struct frame_struct *frame);
//Clock Algorithm
int clock();
//Not Recently Used Algorithm
int nru(struct frame_struct *head);

//Refresh function
void refresh(struct frame_struct *frame);
//Sanity check on command-line args
int checkArgs(int num, char *args[]);

//Prints frames list for debugging
void print_frames(struct frame_struct *frame, char alg);

#endif