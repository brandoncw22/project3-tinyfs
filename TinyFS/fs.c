
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
    union tfs_block block;
    int i;

    // Read and check superblock
    disk_read(0, block.data);
    if (block.super.signature == TFS_MAGIC)
        printf("superblock:\n      signature is valid\n");
    else
        printf("superblock:\n      signature is invalid\n");

    printf("      %d blocks total\n", block.super.num_blocks);
    printf("      %d inodes total\n", block.super.num_inodes);
    printf("      root inode = %d\n", block.super.root_inode);

    // Read and count block/inode usage from bitmap
    disk_read(1, block.data);

    int b_in_use = 0, i_in_use = 0;
    for (i = 0; i < NUM_BLOCKS; i++)
        if (block.bmap.block_in_use[i / BITS_PER_UINT] & (1 << (i % BITS_PER_UINT)))
            b_in_use++;

    for (i = 0; i < NUM_INODES; i++)
        if (block.bmap.inode_in_use[i / BITS_PER_UINT] & (1 << (i % BITS_PER_UINT)))
            i_in_use++;

    printf("      %d blocks in use\n", b_in_use);
    printf("      %d inodes in use\n", i_in_use);

    // Loop through inode blocks
    for (int b = 2; b < NUM_BLOCKS; b++) {
        disk_read(b, block.data);
        for (int j = 0; j < INODES_PER_BLOCK; j++) {
            struct tfs_inode *inode = &block.inode[j];

            if (inode->type == REGULAR || inode->type == DIR) {
                int inum = (b - 2) * INODES_PER_BLOCK + j;
                printf("inode %d:\n", inum);
                printf("      type = %s\n", inode->type == REGULAR ? "REGULAR" : "DIR");
                printf("      size = %d\n", inode->size);

                for (int k = 0; k < POINTERS_PER_INODE; k++)
                    if (inode->direct[k])
                        printf("      direct[%d] = %d\n", k, inode->direct[k]);

                if (inode->indirect) {
                    printf("      indirect = %d\n", inode->indirect);

                    union tfs_block indirect_block;
                    disk_read(inode->indirect, indirect_block.data);
                    for (int k = 0; k < POINTERS_PER_BLOCK; k++) {
                        if (indirect_block.pointers[k])
                            printf("        indirect[%d] = %d\n", k, indirect_block.pointers[k]);
                    }
                }
            }
        }
    }
}


int tfs_delete(const  char *filename )
{
	return 0;
}

int tfs_get_inumber(const char *filename)
{
    union tfs_block block;
    union tfs_block dblock;

    // Load the inode block containing root inode (inode 1 is at block 2)
    disk_read(2, block.data);
    struct tfs_inode root = block.inode[1];  // inode 1 = root

    printf("DEBUG: root direct[0] = %d\n", root.direct[0]);

    // Read the first directory block (like block 3)
    disk_read(root.direct[0], dblock.data);

    for (int j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) {
        printf("DEBUG: entry[%d] valid = %d, name = %s, inum = %d\n",
               j, dblock.dentry[j].valid, dblock.dentry[j].fname, dblock.dentry[j].inum);
    }

    // Actual file lookup
    for (int i = 0; i < POINTERS_PER_INODE; i++) {
        if (root.direct[i] == 0)
            continue;

        disk_read(root.direct[i], dblock.data);
        for (int j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) {
            if (dblock.dentry[j].valid &&
                strcmp(dblock.dentry[j].fname, filename) == 0) {
                return dblock.dentry[j].inum;
            }
        }
    }

    return -1;  // not found
}


int tfs_getsize(const char *filename)
{
    int inum = tfs_get_inumber(filename);
    if (inum < 0)
        return -1;

    int block_num = (inum / INODES_PER_BLOCK) + 2;
    int index = inum % INODES_PER_BLOCK;

    union tfs_block block;
    disk_read(block_num, block.data);

    return block.inode[index].size;
}


int tfs_read( int inumber,  char *data, int length, int offset )
{
	return 0;
}

