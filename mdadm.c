#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"

int is_mounted = 0;
int is_written = 0;

uint32_t pack_bytes(uint32_t BlockID, uint32_t DiskID, uint32_t Command)
{
  uint32_t operation = 0x0 | BlockID | DiskID << 8 | Command << 12;
  return operation;
}

int mdadm_mount(void) {
	if (is_mounted == 0)
  {
    is_mounted = 1;
    uint32_t jbodMount = pack_bytes(0, 0, JBOD_MOUNT);
    jbod_operation(jbodMount, NULL);
    return 1;
  }
  return -1;
}

int mdadm_unmount(void) {
	if (is_mounted == 0)
  {
    return -1;
  }
  if (is_mounted == 1)
  {
    is_mounted = 0;
    uint32_t jbodUnmount = pack_bytes(0, 0, JBOD_UNMOUNT);
    jbod_operation(jbodUnmount, NULL);
    return 1;
  }
  return -1;
}

int mdadm_write_permission(void){
	if (is_written == 0)
  {
    is_written = 1;
    uint32_t jbodWrite = pack_bytes(0, 0, JBOD_WRITE_PERMISSION);
    jbod_operation(jbodWrite, NULL);
    return 0;
  }

  return -1;
}


int mdadm_revoke_write_permission(void){
	if (is_written == 1)
  {
    is_written = 0;
    uint32_t jbodRevokeWrite = pack_bytes(0, 0, JBOD_WRITE_PERMISSION);
    jbod_operation(jbodRevokeWrite, NULL);
    return 0;
  }

  return -1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
	if (len == 0 && buf == NULL) 
    return 0;
  if (len >1024)
    return -1;
  if ((addr + len) >= JBOD_NUM_DISKS * JBOD_DISK_SIZE)
    return -1;
  if (is_mounted == 0)
    return -1;
  if (len != 0 && buf == NULL)
    return -1;

  
  uint8_t tempbuffer[JBOD_BLOCK_SIZE]; // stores the of jbod read
  uint32_t bytes_remaining = len; // bytes remaining to read, which is inialized as the read length parameter
  int bytes_to_read = 0;               // bytes I need to read
  int bytes_read = 0;                  // total bytes read
                                       // bytes to read in the block from the offest postion to end postion, with the last possible end position being the end of the block


  // the loop runs while the total bytes read is less than read length parameter
  while (bytes_read < len)
  {

    int current_disk = (addr) / JBOD_DISK_SIZE;        // Indicates the current disk position
    int current_block = (addr) / JBOD_BLOCK_SIZE;      // Indicates the current block position
    int offset = (addr) % JBOD_BLOCK_SIZE;             // The diffrence between the top of a block and the location of the start address
    int bytes_remaining_in_block = JBOD_BLOCK_SIZE - offset; // The diffrence between offest position and the end of the block

    // Gets the current disk
    jbod_operation(pack_bytes(0, current_disk, JBOD_SEEK_TO_DISK), NULL);
    // Gets the current block
    jbod_operation(pack_bytes(current_block, 0, JBOD_SEEK_TO_BLOCK), NULL);
    // Reads the current block and increments to the next block
    if (cache_enabled() == false || cache_lookup(current_disk,current_block,buf) == -1){
      jbod_operation(pack_bytes(0, 0, JBOD_READ_BLOCK), tempbuffer);
      
      if (cache_enabled() == true){
        cache_insert(current_disk,current_block,tempbuffer);
      }

    }

    // if read length is less than the bytes left in a block from the offset position
    // copy the bytes into the read buffer, with read length as bytes to read
    if (bytes_remaining < bytes_remaining_in_block)
    {
      bytes_to_read = bytes_remaining;
      memcpy(buf + bytes_read, tempbuffer, bytes_to_read);
    }
    // if read length is greater than the bytes left in a block from the offset position
    // copy the bytes remaining in the block
    else if (bytes_remaining >= bytes_remaining_in_block)
    {
      bytes_to_read = bytes_remaining_in_block;
      memcpy(buf + bytes_read, tempbuffer, bytes_to_read);
    }
    if ((addr % JBOD_DISK_SIZE) == JBOD_DISK_SIZE - 1 ){
      current_disk++; 
      jbod_operation(pack_bytes(0, current_disk, JBOD_SEEK_TO_DISK), NULL);
    }


    // Update the start address, bytes remaining, bytes read, and bytes to read because its going intpo a new block
    addr += bytes_to_read;
    bytes_remaining = bytes_remaining - bytes_to_read;
    bytes_read += bytes_to_read;
    bytes_to_read = 0;
  }

  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
	if (len == 0 && buf == NULL)
    return 0;
  if (len > 1024)
    return -1;
  if ((addr + len) > JBOD_NUM_DISKS * JBOD_DISK_SIZE)
    return -1;
  if (is_mounted == 0)
    return -1;
  if (is_written == 0)
    return -1;
  if (len != 0 && buf == NULL)
    return -1;

  uint8_t tempbuffer[JBOD_BLOCK_SIZE]; // stores the of jbod read
  uint32_t current_addr = addr; // stores the current address
  int bytes_to_write = 0; // bytes left to write in the block
  int bytes_written = 0; // bytes written so far 

  while (bytes_written < len)
  {
    // printf("cur %d",current_addr);
    // modify current address
    int current_disk = current_addr /(256*256); // Indicates the current disk position
    int current_block = ((current_addr % (256*256))/256); // Indicates the current block position
    int offset = (current_addr) % JBOD_BLOCK_SIZE; // The diffrence between the top of a block and the location of the start address

    // printf(" dd%d %d %d\n", current_addr, current_disk, current_block);

    jbod_operation(pack_bytes(0, current_disk, JBOD_SEEK_TO_DISK), NULL); // Gets the current disk
    jbod_operation(pack_bytes(current_block, 0, JBOD_SEEK_TO_BLOCK), NULL); // Gets the current block
    jbod_operation(pack_bytes(0, 0, JBOD_READ_BLOCK), tempbuffer); // read the tempbuffer

    int max_write = JBOD_BLOCK_SIZE - offset;  // bytes left in the block
    int bytes_remaining = len - bytes_written; // total bytes left to write 

    // if bytes left in the block from the current address is less than total bytes to write
    if (max_write < bytes_remaining)
    {
      bytes_to_write = max_write;
    }
    else{
      bytes_to_write = bytes_remaining; 
    }
    if ((addr % JBOD_DISK_SIZE) == JBOD_DISK_SIZE - 1 ){
      current_disk++; 
      jbod_operation(pack_bytes(0, current_disk, JBOD_SEEK_TO_DISK), NULL);
    }
    // printf("%d\n", bytes_to_write);
    // Copies the contents of write buf to temp buf
    memcpy(tempbuffer + offset, buf + bytes_written, bytes_to_write);

    // Seeks disks and block then writes 
    if (jbod_operation(pack_bytes(0, current_disk, JBOD_SEEK_TO_DISK), NULL)==-1) ;
    if (jbod_operation(pack_bytes(current_block, 0, JBOD_SEEK_TO_BLOCK), NULL)==-1) ;
    // if (jbod_operation(pack_bytes(0, 0, JBOD_WRITE_BLOCK), tempbuffer)==-1) ;

    if (cache_enabled() == false || cache_lookup(current_disk,current_block,tempbuffer) == -1){
      if (jbod_operation(pack_bytes(0, 0, JBOD_WRITE_BLOCK), tempbuffer)==-1) ;
      
      if (cache_lookup(current_disk,current_block,tempbuffer) == -1){
        cache_insert(current_disk,current_block,tempbuffer);
      }
      else if (cache_lookup(current_disk,current_block,tempbuffer) == 1){
        cache_update(current_disk,current_block,tempbuffer);
      }
    }

//
    // updates the bytes written and current address 
    bytes_written += bytes_to_write;
    current_addr += bytes_to_write;
  }

  return len;
}
