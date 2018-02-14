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

struct cs1550_file_alloc_table_block {
	short table[MAX_FAT_ENTRIES];
};

typedef struct cs1550_file_alloc_table_block cs1550_fat_block;

//METHODS/////////////////////////////////////////////////////////////////////////

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
static int get_directory(char *directory, struct cs1550_directory_entry *entry){
	FILE *file;
	file = fopen(".directories", "rb");

	int result = -1;

	//If .directories file fails to open
	if(!file){
		//Create a new one
		file = fopen(".directories", "wb");
	}

	else{

		//Iterate through directories
		while(fread(entry, sizeof(struct cs1550_directory_entry), 1, file)){
			
			//Check for requested directory
			if(!strcmp(entry->name, directory)){
			//????????????????????
				//Store disk location of file to result
				//	Disk location is one directory entry size before file posistion
				result = ftell(file) - sizeof(struct cs1550_directory_entry);
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
void parse_path(char *path, char *dir, char *file, char *ext){

	//Clear strings
	dir[0] = '\0';
	file[0] = '\0';
	ext[0] = '\0';

	//Parse path
	int err = sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	//Verify successful parse
	if(err == 0){
		fprintf(stderr, "Error on parsing path\n");
		exit(1);
	}

	//Add null term to end of strings
	dir[MAX_FILENAME] = '\0';
	file[MAX_FILENAME] = '\0';
	ext[MAX_FILENAME] = '\0';

	return;
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
		
		//Ensure input
		if(!directory){
			fprintf(stderr, "Error on parsing path!\n");
			exit(1);	
		}

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

	//Check if path is root
	if (strcmp(path, "/") != 0){
		//the filler function allows us to add entries to the listing
		//read the fuse.h file for a description (in the ../include dir)
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		//Find subdirectories and add to buffer
		FILE *file;
		file = fopen(".directories", "rb");
		//If file does not exist
		if(!file){
			//Create a new one
			file = fopen(".directories", "wb");
		}
		else{
			//Iterate through subdirectories
			struct cs1550_directory_entry curr;

			while(fread(&curr, sizeof(struct cs1550_directory_entry), 1, file)){
				//Add subdirectories to buffer
				filler(buf, curr.dname, NULL, 0);
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
		
		//Ensure input
		if(!directory){
			fprintf(stderr, "Error on parsing path!\n");
			exit(1);	
		}

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
				filler(buff, full_filename, NULL, 0);
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

	//Check directory name length
	if(strlen(directory) > MAX_FILENAME){
		fprintf(stderr, "Directory name too long, mais 8\n");
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
		//Open .directories
		FILE *file;
		file = fopen(".directories", "rb");
		if(!file){
			fprintf(stderr, "Error on opening .directories file\n");
			return -1;
		}

		//Write ne directory to disk
		struct cs1550_directory_entry new_dir;
		strcpy(new_dir.dname, directory);
		if(fwrite(&new_dir, sizeof(struct cs1550_directory_entry), 1, file) < 1){
			fprintf(stderr, "Failed to write to .directories file\n");
			return -1;
		}
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

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	return 0;
}

/*
 * Deletes a file
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

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

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

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	return size;
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
