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
    union tfs_block bitmap;
    int i;

    /*
        Reads and validates the superblock, which is block 0. This block
        Stores overall file system metadata like signature, total blocks/inodes,
        and root inode.
    */
    disk_read(0, block.data);
    if (block.super.signature == TFS_MAGIC)
        printf("superblock:\n      signature is valid\n");
    else
        printf("superblock:\n      signature is invalid\n");
    // Prints basic file systems stats from the superblock
    printf("      %d blocks total\n", block.super.num_blocks);
    printf("      %d inodes total\n", block.super.num_inodes);
    printf("      root inode = %d\n", block.super.root_inode);

    // Read and count block/inode usage from bitmap (block 1)
    disk_read(1, bitmap.data);

    // Counts used blocks based on the bitmap
    int b_in_use = 0, i_in_use = 0;
    for (i = 0; i < NUM_BLOCKS; i++)
        if (bitmap.bmap.block_in_use[i / BITS_PER_UINT] & (1 << (i % BITS_PER_UINT)))
            b_in_use++;

    // Counts used inodes based on the bitmap
    for (i = 0; i < NUM_INODES; i++)
        if (bitmap.bmap.inode_in_use[i / BITS_PER_UINT] & (1 << (i % BITS_PER_UINT)))
            i_in_use++;

    printf("      %d blocks in use\n", b_in_use);
    printf("      %d inodes in use\n", i_in_use);

    /*
        Loops through all inode blocks (starting at 2), in the file system to find and display valid inodes.
        Each block starting from 2 may contain up to INODES_PER_BLOCK inodes.
        For every inode that comes back valid, either an inode or a directory, this loop:
        - Calculates inode number
        - Prints inode metadata: type and size
        - Lists valid direct block pointers (which point to file content blocks)
        - If an indirect pointer exists, loads that block and prints the extra pointers it holds
    */
    for (int b = 2; b < NUM_BLOCKS; b++) {
        // Load the current block to access its inode contents
        disk_read(b, block.data);
    
        // Each block contains multiple inodes
        for (int j = 0; j < INODES_PER_BLOCK; j++) {
            int inum = (b - 2) * INODES_PER_BLOCK + j;

            // Skip unused inodes using the inode bitmap
            if (!(bitmap.bmap.inode_in_use[inum / BITS_PER_UINT] & (1 << (inum % BITS_PER_UINT))))
                continue;

            struct tfs_inode *inode = &block.inode[j];

            // Only print metadata for valid files or directories
            if (inode->type == REGULAR || inode->type == DIR) {
                // Print general metadata
                // Look up file name for inode
                char label[32];
                if (inum == 1) {
                    snprintf(label, sizeof(label), "root inode %d", inum);
                } else {
                    // Search directory entries for name
                    strcpy(label, "");
                    struct tfs_inode root = block.inode[1];
                    union tfs_block dirblock;

                    for (int i = 0; i < POINTERS_PER_INODE; i++) {
                        if (root.direct[i] == 0) continue;
                        disk_read(root.direct[i], dirblock.data);
                        for (int j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) {
                            if (dirblock.dentry[j].valid && dirblock.dentry[j].inum == inum) {
                                snprintf(label, sizeof(label), "%s inode %d", dirblock.dentry[j].fname, inum);
                                break;
                            }
                        }
                    }

                    if (strlen(label) == 0) // fallback
                        snprintf(label, sizeof(label), "inode %d", inum);
                }

                printf("%s:\n", label);
                printf("      type = %s\n", inode->type == REGULAR ? "REGULAR" : "DIR");
                printf("      size = %d bytes\n", inode->size);

                // Print direct pointers
                printf("      direct blocks:");
                bool first = true;
                for (int k = 0; k < POINTERS_PER_INODE; k++) {
                    if (inode->direct[k]) {
                        if (!first) printf(", ");
                        printf("%d", inode->direct[k]);
                        first = false;
                    }
                }
                if (first) printf(" (none)");
                printf("\n");

                union tfs_block indirect_block;
                disk_read(inode->indirect, indirect_block.data);

                // If indirect pointer is used, load and print its block list
                if (inode->indirect) {
                    printf("      indirect data blocks:");
                    bool first = true;
                    for (int k = 0; k < POINTERS_PER_BLOCK; k++) {
                        int ptr = indirect_block.pointers[k];
                        if (ptr == 0) continue;

                        // Check if ptr is within valid block range
                        if (ptr >= 0 && ptr < NUM_BLOCKS &&
                            (bitmap.bmap.block_in_use[ptr / BITS_PER_UINT] & (1 << (ptr % BITS_PER_UINT)))) {
                            
                            if (!first) printf(", ");
                            printf("%d", ptr);
                            first = false;
                        }
                    }
                    if (first) printf(" (none)");
                    printf("\n");

                }                
            }
        }
    }
}

int tfs_delete(const char *filename)
/*
    tfs_delete - Deletes a file from the file system.
    Clears the inode and block bitmap bits, resets the inode structure,
    and removes the directory entry in the root.
*/
{
    int inum = tfs_get_inumber(filename);
    if (inum < 0) return -1;

    // Load bitmap and confirm inode is marked as used
    union tfs_block bitmap;
    disk_read(1, bitmap.data);

    // Locate the inode's block and index
    int block_num = (inum / INODES_PER_BLOCK) + 2;
    int index = inum % INODES_PER_BLOCK;

    union tfs_block inode_block, dirblock, indirect_block;
    disk_read(block_num, inode_block.data);
    struct tfs_inode *inode = &inode_block.inode[index];

    // Free direct blocks
    for (int i = 0; i < POINTERS_PER_INODE; i++) {
        if (inode->direct[i]) {
            int b = inode->direct[i];
            bitmap.bmap.block_in_use[b / BITS_PER_UINT] &= ~(1 << (b % BITS_PER_UINT));
        }
    }

    // Free indirect blocks
    if (inode->indirect) {
        disk_read(inode->indirect, indirect_block.data);
        for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
            if (indirect_block.pointers[i]) {
                int b = indirect_block.pointers[i];
                bitmap.bmap.block_in_use[b / BITS_PER_UINT] &= ~(1 << (b % BITS_PER_UINT));
            }
        }
        bitmap.bmap.block_in_use[inode->indirect / BITS_PER_UINT] &= ~(1 << (inode->indirect % BITS_PER_UINT));
    }

    // Clear inode from bitmap
    bitmap.bmap.inode_in_use[inum / BITS_PER_UINT] &= ~(1 << (inum % BITS_PER_UINT));

    // Clear the inode structure itself
    memset(inode, 0, sizeof(struct tfs_inode));
    disk_write(block_num, inode_block.data);

    // Remove from root directory
    disk_read(2, inode_block.data); // root inode is in block 2
    struct tfs_inode root = inode_block.inode[1];
    int removed = 0;

    for (int i = 0; i < POINTERS_PER_INODE; i++) {
        if (root.direct[i] == 0) continue;

        disk_read(root.direct[i], dirblock.data);
        for (int j = 0; j < NUM_DENTRIES_PER_BLOCK; j++) {
            if (dirblock.dentry[j].valid && dirblock.dentry[j].inum == inum) {
                dirblock.dentry[j].valid = 0;
                disk_write(root.direct[i], dirblock.data);
                removed = 1;
                break;
            }
        }
        if (removed) break;
    }

    // Save updated bitmap
    disk_write(1, bitmap.data);

    // Internal confirmation print (since shell.c can’t be touched)
    if (removed) {
        printf("DEBUG: file \"%s\" deleted successfully\n", filename);
    }

    return removed ? 0 : -1;
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

int tfs_read(int inumber, char *data, int length, int offset)
/*
    tfs_read - Reads file data from a given inode number, offset, and length.
    Handles both direct and indirect blocks and fills the provided buffer.
    Returns the number of bytes read.
*/
{
    if (inumber < 0 || inumber >= NUM_INODES) return -1;

    // Locate inode block and index
    int block_num = (inumber / INODES_PER_BLOCK) + 2;
    int index = inumber % INODES_PER_BLOCK;

    union tfs_block block;
    disk_read(block_num, block.data);
    struct tfs_inode inode = block.inode[index];

    // Handle invalid reads
    if (offset >= inode.size) return 0;
    if (offset + length > inode.size)
        length = inode.size - offset;

    int bytes_read = 0;
    int start_block = offset / DISK_BLOCK_SIZE;
    int block_offset = offset % DISK_BLOCK_SIZE;

    // Read from direct blocks
    for (int i = start_block; i < POINTERS_PER_INODE && bytes_read < length; i++) {
        if (inode.direct[i] == 0) break;
        union tfs_block datablock;
        disk_read(inode.direct[i], datablock.data);

        int copy_start = (i == start_block) ? block_offset : 0;
        int to_copy = DISK_BLOCK_SIZE - copy_start;
        if (to_copy > length - bytes_read)
            to_copy = length - bytes_read;

        memcpy(data + bytes_read, datablock.data + copy_start, to_copy);
        bytes_read += to_copy;
    }

    // If file is fully read or has no indirect block, return
    if (bytes_read >= length || inode.indirect == 0)
        return bytes_read;

    // Read from indirect block
    union tfs_block indirect_block;
    disk_read(inode.indirect, indirect_block.data);

    for (int i = 0; i < POINTERS_PER_BLOCK && bytes_read < length; i++) {
        if (indirect_block.pointers[i] == 0) break;
        union tfs_block datablock;
        disk_read(indirect_block.pointers[i], datablock.data);

        int to_copy = DISK_BLOCK_SIZE;
        if (to_copy > length - bytes_read)
            to_copy = length - bytes_read;

        memcpy(data + bytes_read, datablock.data, to_copy);
        bytes_read += to_copy;
    }

    return bytes_read;
}

