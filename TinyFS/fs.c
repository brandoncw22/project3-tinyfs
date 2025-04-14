
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

// for the 4M file system
#define TFS_MAGIC  0xc3450545

#define NUM_BLOCKS 1024
#define NUM_INODES 128
#define NUM_DENTRIES_PER_BLOCK 128

#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

#define BITS_PER_UINT 32

// file type
#define REGULAR 1
#define DIR 2

struct tfs_superblock {
	int signature ; // Signature 
	int num_blocks; // number of blocks; same as NUM_BLOCKS in this project
	int num_inodes; // number of inodes; saem as NUM_INODES in this project
	int root_inode; // inode number of root directory ; use 1
};

struct tfs_bmapblock {
	unsigned int block_in_use[NUM_BLOCKS/BITS_PER_UINT];
	unsigned int inode_in_use[NUM_INODES/BITS_PER_UINT]; 
};

struct tfs_dir_entry {
	int valid; 
	char fname[24];
        int inum;
};

struct tfs_inode {
	int type;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union tfs_block {
	struct tfs_superblock super;
	struct tfs_bmapblock bmap;
	struct tfs_inode inode[INODES_PER_BLOCK];
	struct tfs_dir_entry dentry[NUM_DENTRIES_PER_BLOCK]; 
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};


void tfs_debug()
{ 
        int i;
        int b_in_use = 0;

	union tfs_block block;

	disk_read(0,block.data);

        // check signature
        if(block.super.signature  == TFS_MAGIC)
		printf("      signature is valid\n");
	else
		printf("      signature is invalid\n");

	disk_read(1,block.data);
	for(i=0; i<NUM_BLOCKS; i++)
           if(block.bmap.block_in_use[i/BITS_PER_UINT] & (1 <<(i%BITS_PER_UINT)))
		 b_in_use++ ;  
        printf("      %d blocks in use \n", b_in_use); 

        // count inodes in use 
	

        
	// explore root directory


}

int tfs_delete(const  char *filename )
{
	return 0;
}

int tfs_get_inumber(const  char *filename )
{
	return 0;
}

int tfs_getsize(const  char *filename )
{
	return -1;
}

int tfs_read( int inumber,  char *data, int length, int offset )
{
	return 0;
}

