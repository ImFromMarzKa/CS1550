//Riley Marzka
//CS1550
//Project 4 (File System)
//Due: 4/23/17


/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

//Debug levels
// #define READDIR
// #define MKDIR
// #define MKNOD
// #define READ
// #define WRITE

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;

typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

#define MAX_FAT_ENTRIES (BLOCK_SIZE/sizeof(short))
#define EOF_SENT -2
#define FREE_SENT -1

struct cs1550_fat_block {
	short table[MAX_FAT_ENTRIES];
};

typedef struct cs1550_fat_block cs1550_fat_block;

//METHODS/////////////////////////////////////////////////////////////////////////

void parse_path(const char *path, char *dir, char *file, char *ext);
int get_directory(char *directory, struct cs1550_directory_entry *entry);
void print_fat(struct cs1550_fat_block *f);

/*Function to finds a requested directory
	* Parameters:
		* directory = String representing requested directory
		* entry = pointer to struct where information will be returned
	* Returns:
		* if(directory exists) 
			* result = location on disk
			* entry = requested directory
		*else()
			* result = -1
			* entry = null
*/
int get_directory(char *directory, struct cs1550_directory_entry *entry){
	FILE *file;
	file = fopen(".disk", "rb");

	int result = -1;

	//If .directories file fails to open
	if(!file){
		fprintf(stderr, "\nError on opening .disk file\n");	
	}

	else{

		//Get root directory from disk
		struct cs1550_root_directory *root_dir = malloc(sizeof(struct cs1550_root_directory));
		if(!fread(root_dir, sizeof(struct cs1550_root_directory), 1, file)){
			fprintf(stderr, "Error on reading root directory from .disk\n");
			fclose(file);
			return -1;
		}

		//Iterate through directories
		int i;
		for(i = 0; i < root_dir->nDirectories; i++){
			//Check for requested directory
			if(!strcmp(root_dir->directories[i].dname, directory)){
				//Store disk location of directory to result
				result = root_dir->directories[i].nStartBlock;

				//Calculate offset on disk
				int off = result * BLOCK_SIZE;
				//Go to disk block where directory begins
				if(fseek(file, off, SEEK_SET)){
					fprintf(stderr, "Error on seeking to directory on .disk\n");
					fclose(file);
					result = -1;
				}
				else{
					//Make entry point to directory
					if(!fread(entry, sizeof(struct cs1550_directory_entry), 1, file)){
						fprintf(stderr, "Error on readin directory from .disk\n");
						fclose(file);
						result = -1;
					}
				}
				break;
			}
		}
	}

	fclose(file);
	return result;
}

/*Function to parse a path into directory, filename, and extension
	* All passed as arguments
*/
void parse_path(const char *path, char *dir, char *file, char *ext){

	//Clear strings
	dir[0] = '\0';
	file[0] = '\0';
	ext[0] = '\0';

	//Parse path
	int err = sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	//Verify successful parse
	if(err == 0){
		fprintf(stderr, "Error on parsing path\n");

	}

	//Add null term to end of strings
	dir[MAX_FILENAME] = '\0';
	file[MAX_FILENAME] = '\0';
	ext[MAX_FILENAME] = '\0';

	return;
}

//Helper to print FAT
void print_fat(struct cs1550_fat_block *f){
	int i;
	fprintf(stderr, "FAT = [");
	for(i = 0; i < MAX_FAT_ENTRIES; i++){
		fprintf(stderr, "%d,", f->table[i]);
	}
	fprintf(stderr, "]\n");
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
   
	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = (S_IFDIR | 0755);
		stbuf->st_nlink = 2;
	} 
	else {

		//Parse path 
		char directory[MAX_FILENAME + 1]; //Allow for null term
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];

		parse_path(path, directory, filename, extension);

		//Get subdirectory
		struct cs1550_directory_entry *dir_entry = malloc(sizeof(struct cs1550_directory_entry));
		int diskLoc = get_directory(directory, dir_entry);

		//If directory exists
		if(diskLoc != -1){
			//Check for blank filename 
			if(strlen(filename) == 0){
				//Path leads to subdirectory
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
			}
			//Look for file
			else{
				int i;
				int found = 0;
				for(i = 0; i < dir_entry->nFiles; i++){
					if(!strcmp(dir_entry->files[i].fname, filename)){
						found = 1;
						break;
					}
				}

				//Check if file was found
				if(found){
					stbuf->st_mode = S_IFREG | 0666; 
					stbuf->st_nlink = 1; //file links
					stbuf->st_size = dir_entry->files[i].fsize; //file size
				}
				else{
					res = -ENOENT;
				}
			}
		}

		//Else path doesn't exist
		else{
			res = -ENOENT;
		}	
	}

	return res;
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	#ifdef READDIR
		fprintf(stderr, "path = %s\n", path);
	#endif

	//Check if path is root
	if (!strcmp(path, "/")){

		#ifdef READDIR
			fprintf(stderr, "Path is root\n");
		#endif

		//the filler function allows us to add entries to the listing
		//read the fuse.h file for a description (in the ../include dir)
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		//Find subdirectories and add to buffer
		FILE *file;
		file = fopen(".disk", "rb");
		//If file does not exist
		if(!file){
			fprintf(stderr, "Error on opening .disk file\n");
		}
		else{

			//Load root directory from .disk
			struct cs1550_root_directory *root_dir = malloc(sizeof(cs1550_root_directory));
			if(!fread(root_dir, sizeof(struct cs1550_root_directory), 1, file)){
				fprintf(stderr, "Error on reading root directory from .disk\n");
				fclose(file);
				return -ENOENT;
			}

			#ifdef READDIR
				fprintf(stderr, "nDirectories = %d\n", root_dir->nDirectories);
			#endif
			//Iterate through subdirectories and add them to buffer
			int i;
			for(i = 0; i < root_dir->nDirectories; i++){
				#ifdef READDIR
					fprintf(stderr, "%s\n", root_dir->directories[i].dname);
				#endif
				filler(buf, root_dir->directories[i].dname, NULL, 0);
			}
		}

		fclose(file);
	}
	//Else, find files in subdirectory
	else{
		//Parse path 
		char directory[MAX_FILENAME + 1]; //Allow for null term
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];

		parse_path(path, directory, filename, extension);

		//Get subdirectory
		struct cs1550_directory_entry *dir_entry = malloc(sizeof(struct cs1550_directory_entry));
		int diskLoc = get_directory(directory, dir_entry);

		//Check if directory exists
		if(diskLoc != -1){
			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);

			//Iterate through files and add to buffer
			int i;
			for(i = 0; i < dir_entry->nFiles; i++){
				//Construct full filename
				char full_filename[MAX_FILENAME + MAX_EXTENSION + 2];
				strcpy(full_filename, dir_entry->files[i].fname);
				//Check if file has extension 
				if(strlen(dir_entry->files[i].fext)){
					//Add extension to filename
					strcat(full_filename, ".");
					strcat(full_filename, dir_entry->files[i].fext);
				}

				//Add file to buffer
				filler(buf, full_filename, NULL, 0);
			}	 
		}
		//Directory not found
		else{
			return -ENOENT;
		}
	}
	return 0;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	//Parse path
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	parse_path(path, directory, filename, extension);

	#ifdef MKDIR
		printf("\n\n>>>mkdir()\n");
		printf("Directory: %s\n", directory);
		printf("Filename: %s\n", filename);
		printf("Extention: %s\n\n", extension);
	#endif

	//Check directory is added under root
	if(strlen(filename)){
		fprintf(stderr, "\nDirectories can only be added under root\n");
		return -EPERM;
	}

	//Check directory name length
	if(strlen(directory) > MAX_FILENAME){
		fprintf(stderr, "\nDirectory name too long, mais 8\n");
		return -ENAMETOOLONG;
	}

	//Check if directory exists
	struct cs1550_directory_entry *dir_entry = malloc(sizeof(struct cs1550_directory_entry));
	int diskLoc = get_directory(directory, dir_entry);
	if(diskLoc != -1){
		return -EEXIST;
	}
	//Append new path to file
	else{
		//Open .disk
		FILE *file;
		file = fopen(".disk", "rb+");
		if(!file){
			fprintf(stderr, "\nError on opening .disk file\n");
			fclose(file);
			return -1;
		}


		//Get root directory from .disk
		struct cs1550_root_directory *root_dir = malloc(sizeof(cs1550_root_directory));
		if(!fread(root_dir, sizeof(struct cs1550_root_directory), 1, file)){
			fprintf(stderr, "\nError on reading root directory from .disk\n");
			fclose(file);
			return -1;
		}

		//Check that we have not reached max number of directories
		if(root_dir->nDirectories == MAX_DIRS_IN_ROOT){
			fprintf(stderr, "\nRoot directory has max number of subdirectory\n");
			fclose(file);
			return -EPERM; 
		}

		//Create new directory under root
		strcpy(root_dir->directories[root_dir->nDirectories].dname, directory);

		//Seek to FAT on .disk (last block on disk)
		if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
			fprintf(stderr, "\nError on seeking to FAT\n");
			fclose(file);
			return -1;
		}

		//Read FAT from .disk
		struct cs1550_fat_block *fat = (struct cs1550_fat_block *)malloc(BLOCK_SIZE);
		if(!fread(fat, sizeof(struct cs1550_fat_block), 1, file)){
			fprintf(stderr, "\nError on reading FAT from .disk\n");
			fclose(file);
			return -1;
		}

		#ifdef MKDIR
			print_fat(fat);
		#endif

		//Check that FAT has been initialized
		if(fat->table[0] == 0){
			#ifdef MKDIR
				printf("\nInitializing FAT\n");
			#endif

			//If not, mark all blocks as free
			int i;
			for(i = 0; i < MAX_FAT_ENTRIES; i++){
				fat->table[i] = (short)FREE_SENT;
			}

			//Mark block zero as EOF for root directory
			fat->table[0] = (short)EOF_SENT;

			
		}

		//Search FAT for first free block
		int i, free_block = -1;
		for(i = 0; i < MAX_FAT_ENTRIES; i++){
			if(fat->table[i] == (short)FREE_SENT){
				free_block = i;
				fat->table[i] = (short)EOF_SENT;
				break;
			}
		}

		//Ensure valid block
		if(free_block == -1){
			fprintf(stderr, "\nNo blocks marked as free in FAT\n");
			fclose(file);
			return -1;
		}

		#ifdef MKDIR
			printf("\nfree_block = %d\n", free_block);
		#endif

		//Set starting location of new directory
		root_dir->directories[root_dir->nDirectories].nStartBlock = free_block;

		//Mark location of new directory as EOF in FAT
		fat->table[free_block] = (short)EOF_SENT;

		#ifdef MKDIR
			print_fat(fat);
		#endif

		//Seek back to FAT location
		if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
			fprintf(stderr, "\nError on seeking back to FAT location on .disk\n");
			return -1;
		}

		//Write updated FAT back to .disk
		if(!fwrite(fat, sizeof(struct cs1550_fat_block), 1, file)){
			fprintf(stderr, "\nError on writing FAT back to .disk after update\n");
			fclose(file);
			return -1;
		}

		//Seek to free location
		int off = free_block * BLOCK_SIZE;
		if(fseek(file, off, SEEK_SET)){
			fprintf(stderr, "Error on seeking to free block in .disk\n");
			fclose(file);
			return -1;
		}


		//Write new directory to disk
		struct cs1550_directory_entry *new_dir = (struct cs1550_directory_entry *)malloc(sizeof(struct cs1550_directory_entry));
		memset(new_dir, 0, sizeof(struct cs1550_directory_entry));
		if(!fwrite(new_dir, sizeof(struct cs1550_directory_entry), 1, file)){
			fprintf(stderr, "Failed to write to .disk\n");
			fclose(file);
			return -1;
		}

		#ifdef MKDIR
			printf("nFiles = %d\n\n", new_dir->nFiles);
		#endif

		root_dir->nDirectories++; //Increment number of directories in root

		//Write root directory back to .disk
		if(fseek(file, 0, SEEK_SET)){
			fprintf(stderr, "\nError on seeking back to root on .disk\n");
			fclose(file);
			return -1;
		}
		if(!fwrite(root_dir, sizeof(struct cs1550_root_directory), 1, file)){
			fprintf(stderr, "\nError on writing root back to .disk\n");
			fclose(file);
			return -1;
		}
		fclose(file);
	}

	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}


///END DEADLINE 1////////////////////////////////////////////////////////////////

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	//Ignore
	(void) mode;
	(void) dev;

	//Parse path
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	parse_path(path, directory, filename, extension);

	#ifdef MKNOD
		printf("\n\n>>>mknode()\n");
		printf("Directory: %s\n", directory);
		printf("Filename: %s\n", filename);
		printf("Extention: %s\n\n", extension);
	#endif

	//Check length of filename
	if(strlen(filename) > MAX_FILENAME + 1){
		return -ENAMETOOLONG;
	}
	if(strlen(extension) > MAX_EXTENSION + 1){
		return -ENAMETOOLONG;
	}

	//Check that directory exists
	struct cs1550_directory_entry *dir_entry = malloc(sizeof(struct cs1550_directory_entry));
	int dir_loc = get_directory(directory, dir_entry);
	if(dir_loc == -1){
		fprintf(stderr, "\nDirectory does not exist, or is root\n");
		return -EPERM;
	}

	#ifdef MKNOD
		printf("dir_loc = %d\n", dir_loc);
	#endif

	//Check that we have not maxed out number of files in directory
	if(dir_entry->nFiles == MAX_FILES_IN_DIR){
		fprintf(stderr, "\nDirectory is at max number of files\n");
		return -1;
	}

	//Check that file does not already exist
	int i;
	for(i = 0; i < dir_entry->nFiles; i++){
		if(strcmp(dir_entry->files[i].fname, filename) && strcmp(dir_entry->files[i].fext, extension)){
			fprintf(stderr, "File exists\n");
			return -EEXIST;
		}
	}

	//Open .disk
	FILE *file;
	file = fopen(".disk", "rb+");
	if(!file){
		fprintf(stderr, "\nError on opening .disk file\n");
		fclose(file);
		return -1;
	}

	//Seek to FAT on .disk (last block on disk)
	if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
		fprintf(stderr, "\nError on seeking to FAT\n");
		fclose(file);
		return -1;
	}

	//Read FAT from .disk
	struct cs1550_fat_block *fat = (struct cs1550_fat_block *)malloc(BLOCK_SIZE);
	if(!fread(fat, sizeof(struct cs1550_fat_block), 1, file)){
		fprintf(stderr, "\nError on reading FAT from .disk\n");
		fclose(file);
		return -1;
	}

	//Search FAT for first free block
	int free_block = -1;
	for(i = 0; i < MAX_FAT_ENTRIES; i++){
		if(fat->table[i] == (short)FREE_SENT){
			free_block = i;
			fat->table[i] = (short)EOF_SENT;
			break;
		}
	}
	if(free_block == -1){
		fprintf(stderr, "\nNo blocks marked as free in FAT\n");
		fclose(file);
		return -1;
	}

	#ifdef MKNOD
		printf("free_block = %d\n", free_block);
		printf("nFiles = %d\n", dir_entry->nFiles);
	#endif	

	//Create new file under directory
	strcpy(dir_entry->files[dir_entry->nFiles].fname, filename);

	#ifdef MKNOD
		printf("Check 1\n");
		fflush(stdout);
	#endif	

	strcpy(dir_entry->files[dir_entry->nFiles].fext, extension);

	#ifdef MKNOD
		printf("Check 2\n");
		fflush(stdout);
	#endif

	dir_entry->files[dir_entry->nFiles].nStartBlock = free_block;

	#ifdef MKNOD
		printf("Check 3\n");
		fflush(stdout);
	#endif

	dir_entry->files[dir_entry->nFiles].fsize = 0;

	#ifdef MKNOD
		printf("Created new file\n");
		fflush(stdout);
	#endif	

	//Mark starting location of new file as EOF in FAT
	fat->table[free_block] = (short)EOF_SENT;

	//Seek back to FAT location
	if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
		fprintf(stderr, "\nError on seeking back to FAT location on .disk\n");
		fclose(file);
		return -1;
	}

	//Write updated FAT back to .disk
	if(!fwrite(fat, sizeof(struct cs1550_fat_block), 1, file)){
		fprintf(stderr, "\nError on writing FAT back to .disk after update\n");
		fclose(file);
		return -1;
	}

	#ifdef MKNOD
		printf("Wrote FAT\n");
	#endif	

	//Seek to free location
	int off = free_block * BLOCK_SIZE;
	if(fseek(file, off, SEEK_SET)){
		fprintf(stderr, "Error on seeking to free block in .disk\n");
		fclose(file);
		return -1;
	}


	//Create new "file" in a disk block, initialized to all 0's
	struct cs1550_disk_block *block = malloc(sizeof(cs1550_disk_block));
	for(i = 0; i < MAX_DATA_IN_BLOCK; i++){
		block->data[i] = 0;
	}
	//Write new file to .disk
	if(!fwrite(block, sizeof(struct cs1550_file_directory), 1, file)){
		fprintf(stderr, "Failed to write to .disk\n");
		fclose(file);
		return -1;
	}

	//Increment number of files in directory
	dir_entry->nFiles++;

	#ifdef MKNOD
		printf("nFiles = %d\n", dir_entry->nFiles);
	#endif	

	//Write updated directory back to disk
	off = dir_loc * BLOCK_SIZE;
	if(fseek(file, off, SEEK_SET)){
		fprintf(stderr, "\nError on seeking to directory location on .disk\n");
		fclose(file);
		return -1;
	}
	if(!fwrite(dir_entry, sizeof(struct cs1550_directory_entry), 1, file)){
		fprintf(stderr, "\nError on writing directory back to .disk\n");
		fclose(file);
		return -1;
	}

	#ifdef MKNOD
		printf("Wrote updated directory -> about to return\n\n", free_block);
	#endif	

	fclose(file);
	return 0;
}

/*
 * Deletes a file
 	* Do not modify
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//Parse path
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	parse_path(path, directory, filename, extension);

	#ifdef READ
		printf("\n\n>>>read()\n");
		printf("size = %d\n", size);
		printf("offset = %d\n\n", offset);
		printf("Directory: %s\n", directory);
		printf("Filename: %s\n", filename);
		printf("Extention: %s\n\n", extension);
	#endif

	//check to make sure path exists

	//Check non-null filename
	if(!strlen(filename)){
		fprintf(stderr, "\nPath is a directory\n");
		return -EISDIR;
	}

	//Check that directory exists
	struct cs1550_directory_entry *dir_entry = malloc(sizeof(struct cs1550_directory_entry));
	int dir_loc = get_directory(directory, dir_entry);
	if(dir_loc == -1){
		fprintf(stderr, "\nPath does not exist\n");
		return 0;
	}

	//Search directory for the file
	int i;
	int file_loc = -1;
	size_t file_size = 0;
	for(i = 0; i < dir_entry->nFiles; i++){
		if(!strcmp(dir_entry->files[i].fname, filename) && !strcmp(dir_entry->files[i].fext, extension)){
			file_loc = dir_entry->files[i].nStartBlock;
			file_size = dir_entry->files[i].fsize;
			break;
		}
	}

	//Check that file exists under directory
	if(file_loc == -1){
		fprintf(stderr, "\nFile does not exist in directory\n");
		return 0;
	}

	#ifdef READ
		printf("file_loc = %d\n", file_loc);
		printf("file_size = %d\n\n", file_size);
	#endif

	//Check that size is > 0
	if(size <= 0){
		fprintf(stderr, "\nSize cannot be 0\n");
		return 0;
	}

	//Check that offset is <= to the file size
	if(file_size < offset){
		fprintf(stderr, "\nOffset is greater than file size\n");
		return 0;
	}

	//Check that size is <= file size
	if(file_size < (offset + size)){
		size = file_size - offset;

		#ifdef READ
			printf("Updated read size = %d\n", size);
		#endif
	}

	//Open .disk
	FILE *file;
	file = fopen(".disk", "rb");
	if(!file){
		fprintf(stderr, "\nError on opening .disk\n");
		fclose(file);
		return 0;
	}

	//Open FAT
	struct cs1550_fat_block *fat = malloc(sizeof(struct cs1550_fat_block));
	if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
		fprintf(stderr, "\nError on seeking to FAT on .disk\n");
		fclose(file);
		return 0;
	}
	if(!fread(fat, sizeof(struct cs1550_fat_block), 1, file)){
		fprintf(stderr, "\nError on reading FAT from .disk\n");
		fclose(file);
		return 0;
	}

	//Update starting location for read and offset
	while(offset > BLOCK_SIZE){
		offset -= BLOCK_SIZE;
		if(fat->table[file_loc] == EOF_SENT){
			break;
		}
		file_loc = fat->table[file_loc];
	}

	#ifdef READ
		printf("Updated offset = %d\n\n", offset);
	#endif

	//Seek to first block to be read
	int off = file_loc * BLOCK_SIZE;
	if(fseek(file, off, SEEK_SET)){
		fprintf(stderr, "\nError on seeking to file location on disk\n");
		fclose(file);
		return 0;
	}

	//Calculate number of blocks to read
	//	num_blocks = ceiling((offset + size) / BLOCK_SIZE)
	int num_blocks = 1;
	if((offset + size) > BLOCK_SIZE){
		num_blocks = (offset + size) / BLOCK_SIZE;
		if(((offset + size) % BLOCK_SIZE) > 0){
			num_blocks++;
		}
	}

	#ifdef READ
		printf("num_blocks = %d\n\n", num_blocks);
	#endif

	//Read in a block of the file
	struct cs1550_disk_block *block = malloc(sizeof(struct cs1550_disk_block));

	if(!fread(block->data, MAX_DATA_IN_BLOCK, 1, file)){
		fprintf(stderr, "\nError on reading disk block\n");
		fclose(file);
		return 0;
	}

	#ifdef READ
		printf("As read from disk: block->data = %s\n", block->data);
	#endif

	//Check if multiple blocks need to be read
	if(num_blocks > 1){
		
		//Add data from first block to buf
		strncat(buf, &block->data[offset], MAX_DATA_IN_BLOCK);
		size -= MAX_DATA_IN_BLOCK;

		int blocks_read = 1;

		//Read in all subsequent blocks
		while(blocks_read < num_blocks){

			//Calculate read size
			int read_size = MAX_DATA_IN_BLOCK;
			if(size < MAX_DATA_IN_BLOCK){
				read_size = size;
			}

			#ifdef READ
				printf("\nread_size = %d\n", read_size);
			#endif

			//Find next block in FAT
			file_loc = fat->table[file_loc];
			if(file_loc == EOF_SENT){
				fprintf(stderr, "\nHit EOF before completing requested read\n");
				break;
			}

			#ifdef READ
				printf("\nReading block %d\n", blocks_read + 1);
				printf("read_size = %d\n", read_size);
				printf("file_loc = %d\n", file_loc);
			#endif


			//Read in next block
			off = file_loc * BLOCK_SIZE;
			if(fseek(file, off, SEEK_SET)){
				fprintf(stderr, "\nError on seeking to next disk block of file\n");
				break;
			}
			if(!fread(block->data, read_size, 1, file)){
				fprintf(stderr, "\nError on reading disk block\n");
				break;
			}


			//Add data from block to buffer
			strncat(buf, &block->data[0], read_size);

			#ifdef READ
				printf("\nCopied %d characters from block->data, starting at 0, to buf\n", read_size);
				printf("buf = %s\n", buf);
			#endif

			//Increment number of blocks read
			blocks_read++;
			size -= MAX_DATA_IN_BLOCK;
		}
	}
	else{

		#ifdef READ
			printf("Only reading one block:\n");
		#endif

		//Read data from block to buffer
		strncat(buf, &block->data[offset], size);

		#ifdef READ
			printf("\nCopied %d characters from block->data, starting at %d, to buf\n", size, offset);
			printf("buf = %s\n", buf);
		#endif
	}

	//set size and return, or errors
	size = strlen(buf);

	#ifdef READ
		printf("size of read = %d\n\n", size);
	#endif

	strcat(buf, "\n"); //Add new line feed

	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//Parse path
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	parse_path(path, directory, filename, extension);

	#ifdef WRITE
		printf("\n\n>>>write()\n");
		printf("Buffer String = %s\n", buf);
		printf("size = %d\n", size);
		printf("offset = %d\n\n", offset);
		printf("Directory = %s\n", directory);
		printf("Filename = %s\n", filename);
		printf("Extention = %s\n", extension);
	#endif

	//check to make sure path exists

	//Check non-null filename
	if(!strlen(filename)){
		fprintf(stderr, "\nPath is a directory\n");
		return -EISDIR;
	}

	//Check that directory exists
	struct cs1550_directory_entry *dir_entry = malloc(sizeof(struct cs1550_directory_entry));
	int dir_loc = get_directory(directory, dir_entry);
	if(dir_loc == -1){
		fprintf(stderr, "\nPath does not exist\n");
		return 0;
	}

	//Search directory for the file
	int i;
	int file_index = -1;
	int file_loc = -1;
	size_t file_size = 0;
	for(i = 0; i < dir_entry->nFiles; i++){
		if(!strcmp(dir_entry->files[i].fname, filename) && !strcmp(dir_entry->files[i].fext, extension)){
			file_loc = dir_entry->files[i].nStartBlock;
			file_size = dir_entry->files[i].fsize;
			file_index = i;
			break;
		}
	}
	//Check that file exists under directory
	if(file_loc == -1){
		fprintf(stderr, "\nFile does not exist in directory\n");
		return 0;
	}

	#ifdef WRITE
		printf("file_loc = %d\n", file_loc);
	#endif

	//check that size is > 0
	if(size <= 0){
		fprintf(stderr, "\nSize cannot be 0\n");
		return 0;
	}

	//check that offset is <= to the file size
	if(offset > file_size){
		fprintf(stderr, "\nOffset is greater than file size\n");
		return -EFBIG;
	}

	//write data

	//Open .disk
	FILE *file;
	file = fopen(".disk", "rb+");
	if(!file){
		fprintf(stderr, "\nError on opening .disk\n");
		fclose(file);
		return 0;
	}

	//Open FAT
	struct cs1550_fat_block *fat = malloc(sizeof(struct cs1550_fat_block));
	if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
		fprintf(stderr, "\nError on seeking to FAT on .disk\n");
		fclose(file);
		return 0;
	}
	if(!fread(fat, sizeof(struct cs1550_fat_block), 1, file)){
		fprintf(stderr, "\nError on reading FAT from .disk\n");
		fclose(file);
		return 0;
	}

	//Calculate true starting location
	int real_offset = offset;
	while(real_offset > BLOCK_SIZE){
		real_offset -= BLOCK_SIZE;
		if(fat->table[file_loc] == EOF_SENT){
			break;
		}
		file_loc = fat->table[file_loc];
	}

	//Seek to first block 
	int off = file_loc * BLOCK_SIZE;

	#ifdef WRITE
		printf("\ntrue file_loc = %d\n", file_loc);
		printf("off = %d\n", off);
	#endif

	if(fseek(file, off, SEEK_SET)){
		fprintf(stderr, "\nError on seeking to file location on .disk\n");
		fclose(file);
		return 0;
	}

	//Calculate number of blocks to write
	//	num_blocks = ceiling((offset + size) / BLOCK_SIZE)
	int num_blocks = 1;
	if((offset + size) > BLOCK_SIZE){
		num_blocks = (offset + size) / BLOCK_SIZE;
		if(((offset + size) % BLOCK_SIZE) > 0){
			num_blocks++;
		}
	}

	#ifdef WRITE
		printf("\nnum_blocks = %d\n", num_blocks);
	#endif

	//Write data
	struct cs1550_disk_block *block = malloc(sizeof(struct cs1550_disk_block));
	int blocks_written = 0;
	int bytes_written = 0;
	int bytes_remaining = size;
	while(blocks_written < num_blocks){
		//Calculate size of current write
		int write_size = MAX_DATA_IN_BLOCK;
		if(bytes_remaining < MAX_DATA_IN_BLOCK){
			write_size = bytes_remaining;
		}

		#ifdef WRITE
			printf("\nWriting to block %d\n", blocks_written + 1);
			printf("write_size = %d\n\n", write_size);
		#endif

		//Read the block from .disk
		if(!fread(block->data, MAX_DATA_IN_BLOCK, 1, file)){
			fprintf(stderr, "\nError on reading block to write from .disk\n");
			fclose(file);
			return 0;
		}

		#ifdef WRITE
			printf("\nAs read from disk: block->data = %s\n\n", block->data);
		#endif

		//Check for case of first block 
		if(blocks_written == 0){
			//Adjust write size if needed
			if((write_size + offset) > MAX_DATA_IN_BLOCK){
				write_size = MAX_DATA_IN_BLOCK - offset;
			}

			#ifdef WRITE
				printf("\nadjusted write_size = %d\n\n", write_size);
			#endif
		}
		else{
			//Set offset to 0
			real_offset = 0;
		}

		//Put data from buf into data[]
		block->data[real_offset] = 0; //Clear data in block after offset
		strncat(&block->data[real_offset], buf, write_size);

		#ifdef WRITE
			printf("\nConcatenated %d characters from buf to block->data, starting at %d\n", write_size, offset);
			printf("buf = %s\n", buf);
			printf("block->data = %s\n", block->data);
		#endif

		//Update buffer pointer, bytes written, and bytes remaining
		buf += write_size * sizeof(char); 
		bytes_written += write_size;
		bytes_remaining -= write_size;

		#ifdef WRITE
			printf("bytes_remaining = %d\n", bytes_remaining);
		#endif

		//Seek back to beginning of current block
		off = file_loc * BLOCK_SIZE;
		if(fseek(file, off, SEEK_SET)){
			fprintf(stderr, "Error on seeking to beginning of current block\n");
			return 0;
		}

		#ifdef WRITE
			printf("\n>>>writing '%s' to disk at location %d\n\n", block->data, off);
		#endif

		//Write updated data back to disk
		if(!fwrite(block->data, BLOCK_SIZE, 1, file)){
			fprintf(stderr, "\nError on writing block back to .disk\n");
			return 0;
		}

		if(bytes_remaining){

			#ifdef WRITE
				printf("\nFinding next block of file:\n");
			#endif

			//Find next block in FAT
			if(fat->table[file_loc] != EOF_SENT){
				file_loc = fat->table[file_loc];

				#ifdef WRITE
					printf("Next block of file = %d\n", file_loc);
				#endif
			}
			else{

				#ifdef WRITE
					printf("At end of file... Finding free block to expand into\n");
				#endif

				//Find next free block in fat
				int free_block = -1;
				for(i = 0; i < MAX_FAT_ENTRIES; i++){
					if(fat->table[i] == (short)FREE_SENT){
						free_block = i;
						fat->table[file_loc] = i;
						fat->table[i] = (short)EOF_SENT; //Mark as EOF
						break;
					}
				}
				if(free_block == -1){
					fprintf(stderr, "\nDisk full. Could not complete write\n");
					fclose(file);
					return bytes_written;
				}

				#ifdef WRITE
					printf("free_block & file_loc = %d\n\n", free_block);
				#endif

				//Write FAT back to disk
				if(fseek(file, -sizeof(struct cs1550_fat_block), SEEK_END)){
					fprintf(stderr, "\nError on seeking to FAT location on .disk\n");
					fclose(file);
					return 0;
				}
				if(!fwrite(fat, sizeof(struct cs1550_fat_block), 1, file)){
					fprintf(stderr, "\nError on writing FAT back to .disk\n");
					fclose(file);
					return 0;
				}

				file_loc = free_block;
			}

			//Seek to next block to write
			off = file_loc * BLOCK_SIZE;
			if(fseek(file, off, SEEK_SET)){
				fprintf(stderr, "\nError on seeking to next block to write\n");
				return bytes_written;
			}
		}
		else{ //Nothing left to write
			break;
		}
		
		blocks_written++;

	}

	//update file size
	if((offset + size) > file_size){
		dir_entry->files[file_index].fsize += (offset + size) - file_size;
	}
	else if((offset + size) < file_size){
		dir_entry->files[file_index].fsize = offset + size;
	}

	//Check if file size was changed
	if(file_size != dir_entry->files[file_index].fsize){
		#ifdef WRITE
			printf("\nfsize after write = %d\n\n", dir_entry->files[file_index].fsize);
		#endif

		//Write directory back to .disk
		off = dir_loc * BLOCK_SIZE;
		if(fseek(file, off, SEEK_SET)){
			fprintf(stderr, "\nError on seeking to directory location on .disk\n");
			fclose(file);
			return 0;
		}
		if(!fwrite(dir_entry, sizeof(struct cs1550_directory_entry), 1, file)){
			fprintf(stderr, "\nError on writing updated directory to .disk\n");
			fclose(file);
			return 0;
		}
	}

	//set size (should be same as input) and return, or error
	fclose(file);
	return bytes_written;
}

//END METHODS/////////////////////////////////////////////////////////////////////////////

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
