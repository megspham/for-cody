// NAME: Megan Pham
// EMAIL: megspham@ucla.edu
// ID: 505 313 300

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ext2_fs.h"

#define SUPERBLOCK_OFFSET 1024

int img_fd;
struct ext2_super_block sb;
// struct ext2_inode inode;
struct ext2_group_desc group;
//int totalgroups=0;
int blockbitmap = -1;
int blocknum = 0;
int blocksz = 0;
int inodenum = 0;
int inodebitmap = 0;
int inodespg = 0;
int blockpg = 0;

void superblockSummary()
{

    if (pread(img_fd, &sb, sizeof(struct ext2_super_block), SUPERBLOCK_OFFSET) < 0)
    {
        fprintf(stderr, "pread error with superblock\n");
        exit(1);
    }
    blocknum = sb.s_blocks_count;
    inodenum = sb.s_inodes_count;
    blocksz = 1024 << sb.s_log_block_size;
    int inodesz = sb.s_inode_size;
    blockpg = sb.s_blocks_per_group;
    inodespg = sb.s_inodes_per_group;
    int nonreserved = sb.s_first_ino;

    printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", blocknum,
           inodenum, blocksz, inodesz, blockpg, inodespg, nonreserved);
}

void groupSummary()
{
    //ta said to assume a single group
    if (pread(img_fd, &group, sizeof(struct ext2_group_desc), (SUPERBLOCK_OFFSET + sizeof(struct ext2_super_block))) < 0)
    {
        fprintf(stderr, "pread error with group\n");
        exit(1);
    }

    blocknum = sb.s_blocks_count;
    int inodenum = sb.s_inodes_count;
    int freeblocks = group.bg_free_blocks_count;
    int freeinodes = group.bg_free_inodes_count;
    blockbitmap = group.bg_block_bitmap;
    inodebitmap = group.bg_inode_bitmap;
    int inodesblock = group.bg_inode_table;

    printf("GROUP,0,%d,%d,%d,%d,%d,%d,%d\n", blocknum,
           inodenum, freeblocks, freeinodes, blockbitmap, inodebitmap, inodesblock);
}

void free_block_entries()
{
    // 0 = block is free
    // 1 = block is being used

    for (int i = 0; i < blocksz; i++)
    {
        int bbb = 0;
        pread(img_fd, &bbb, 1, (blocksz * blockbitmap + i));
        // each byte is made up of 8 bits
        for (int j = 0; j < 8; j++)
        {
            int used = bbb & (1 << j);
            if (!used)
            {
                int freeblk = i * 8 + j + 1;
                printf("BFREE,%d\n", freeblk);
            }
        }
    }
}

void free_inode_entries()
{
    // 0 = inode is free
    // 1 = inode is being used
    for (int i = 0; i < blocksz; i++)
    {
        int iii = 0;
        pread(img_fd, &iii, 1, (blocksz * inodebitmap + i));
        // each byte is made up of 8 bits
        for (int j = 0; j < 8; j++)
        {
            int used = iii & (1 << j);
            if (!used)
            {
                int freeblk = i * 8 + j + 1;
                printf("IFREE,%d\n", freeblk);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "error; can only have 2 arguments!\n");
        exit(1);
    }
    img_fd = open(argv[1], O_RDONLY);
    if (img_fd < 0)
    {
        fprintf(stderr, "error, cannot open file\n");
        exit(1);
    }

    superblockSummary();
    groupSummary();
    free_block_entries();
    free_inode_entries();

    exit(0);
}