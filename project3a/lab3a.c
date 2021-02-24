// NAME: Megan Pham, Cody Do
// EMAIL: megspham@ucla.edu, D.Codyx@gmail.com
// ID: 505313300, 105140467

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

void handleDirectoryPrint(struct ext2_inode* inode, unsigned int inodeNum) {
    // Prep for while loop
    int k = 0;
    struct ext2_dir_entry directory;
    
    while (k < 12) {
        unsigned int offset = 0;
        
        if (inode->i_block[k] != 0) {
            int offset2 = inode->i_block[k] * 1024;
            
            while (offset < 1024) {
                // pread directory and check for errors
                if (pread(img_fd, &directory, sizeof(struct ext2_dir_entry), offset + offset2) < 0) {
                    fprintf(stderr, "Error handling directory pread: %s\n", strerror(errno));
                    exit(2);
                }
                
                // Print appropriate line
                if (directory.inode != 0) {
                    fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n", inodeNum, offset, directory.inode, directory.rec_len, directory.name_len, directory.name);
                }
                
                // Increment offset
                offset += directory.rec_len;
            }
        }
        
        else
            break;
        
        // Increment before next iteration
        k++;
    }
}

void handleIndirectPrint(int inode, int blockOffset, int level, int blockNumber) {
    // int blockSize = SUPERBLOCK_OFFSET << sb.s_log_block_size; --> global
    
    // Block size divided by size of element in i_block gives us the address
    int address = blocksz/4;
    int *readBlock = malloc(blocksz);
    
    // Use pread with appropriate parameters; check for errors
    if (pread(img_fd, readBlock, blocksz, blockNumber*blocksz) < 0) {
        fprintf(stderr, "Error handling indirect block pread: %s\n", strerror(errno));
        exit(1);
    }
    
    // While loop to handle printing
    int index = 0;
    while (index < address) {
        // If invalid (0), move on to the next block
        if (readBlock[index] == 0 ) {
            if (level == 1)
                blockOffset += 1;
            else if (level == 2)
                blockOffset += 256;
            else if (level == 3)
                blockOffset += 65536;
        }
        // If valid, react appropriately
        else {
            // Print out appropriate line
            fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inode, level, blockOffset, blockNumber, readBlock[index]);
            
            // Depending on level, adjust blockOffset or recurse down a level
            if (level == 1)
                blockOffset += 1;
            else if (level == 2 || level == 3 )
                handleIndirectPrint(inode, blockOffset, level-1, readBlock[index]);
        }
        
        // Increment index before next iteration
        index++;
    }
    
    // Free readBlock before exiting
    free(readBlock);
}

void handleInodes() {
    struct ext2_inode inode;
    int inodeTableOffset = group.bg_inode_table * blocksz;
    
    // Go through I-nodes
    // Must be unsigned or error during build
    for(unsigned int k = 0; k < sb.s_inodes_count; k++) {
        // pread and error checking
        if (pread(img_fd, &inode, sizeof(struct ext2_inode), k * sizeof(struct ext2_inode) + inodeTableOffset) < 0) {
            fprintf(stderr, "Error handling Inode pread: %s\n", strerror(errno));
            exit(1);
        }
        
        // Check for valid conditions, if so, begin processing
        char fType;
        if (inode.i_links_count != 0 && inode.i_mode !=0) {
            // Check file type
            if (S_ISREG(inode.i_mode))
                fType = 'f';
            else if (S_ISDIR(inode.i_mode))
                fType = 'd';
            else if (S_ISLNK(inode.i_mode))
                fType = 's';
            else
                fType = '?';
            
            // Gets time of last I-node change, modification, and access
            // Note: strftime does NOT output anthing, it just changes time format
            // and places it into the array specified
            char cTime[100];
            char mTime[100];
            char aTime[100];
            
            // Get time of last change
            time_t nodeTime = inode.i_ctime;
            struct tm *gmt = gmtime(&nodeTime);
            if (gmt == NULL) {
                fprintf(stderr, "Error decoding I-node change time: %s\n", strerror(errno));
                exit(1);
            }
            strftime(cTime, 100, "%m/%d/%y %H:%M:%S", gmt);
            
            // Get time of last modification
            nodeTime = inode.i_mtime;
            gmt = gmtime(&nodeTime);
            if (gmt == NULL) {
                fprintf(stderr, "Error decoding I-node modification time: %s\n", strerror(errno));
                exit(1);
            }
            strftime(mTime, 100, "%m/%d/%y %H:%M:%S", gmt);
            
            // Get time of last access
            nodeTime = inode.i_atime;
            gmt = gmtime(&nodeTime);
            if (gmt == NULL) {
                fprintf(stderr, "Error decoding I-node access time: %s\n", strerror(errno));
                exit(1);
            }
            strftime(aTime, 100, "%m/%d/%y %H:%M:%S", gmt);
            
            // Output all information
            fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", k+1, fType, inode.i_mode & 0x0FFF, inode.i_uid, inode.i_gid, inode.i_links_count, cTime, mTime, aTime, inode.i_size, inode.i_blocks);
            
            // Handle the next 15 addresses as needed. End of INODE part
            if (fType != 's' && ((fType == 'f') || (fType == 'd'))) {
                for (int j = 0; j < 15; j++)
                    fprintf(stdout, ",%d", inode.i_block[j]);
            }
            fprintf(stdout, "\n");
            
            // Directory Processing
            if (fType == 'd')
                handleDirectoryPrint(&inode, k+1);
            
            // Indirect Block Processing
            if (fType == 'f' || fType == 'd') {
                handleIndirectPrint(k+1, 12, 1, inode.i_block[12]);
                handleIndirectPrint(k+1, 268, 2, inode.i_block[13]);
                handleIndirectPrint(k+1, 65804, 3, inode.i_block[14]);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // Check number of arguments
    if (argc != 2)
    {
        fprintf(stderr, "Error: Incorrect number of arguments!\n");
        exit(1);
    }
    
    // Open file as read only. Check for errors
    img_fd = open(argv[1], O_RDONLY);
    if (img_fd < 0)
    {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        exit(1);
    }

    superblockSummary();
    groupSummary();
    free_block_entries();
    free_inode_entries();
    handleInodes();

    exit(0);
}

