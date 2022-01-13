// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  //struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;

#define TBL_SIZE 7
struct entry {
  struct buf head;
  struct spinlock lock;
};

struct entry tbl[TBL_SIZE];

void insert_buf(struct buf* head, struct buf* b) {
  b->next = head->next, b->prev = head;
  head->next->prev = b, head->next = b;
}

void countBuf(int pp) {
  acquire(&tbl[pp].lock);
  int i = 0;
  for (struct buf* ptr = (tbl[pp].head).next; ptr != &tbl[pp].head;ptr=ptr->next,i++);
  release(&tbl[pp].lock);
  printf("count :: but:%d buf:%d\n", pp, i);
}

void
binit(void)
{
  struct buf *b;

  //initlock(&bcache.lock, "bcache");
  // Create linked list of buffers
  for (int i = 0;i<TBL_SIZE;i++)
    tbl[i].head.prev = &tbl[i].head, tbl[i].head.next = &tbl[i].head, initlock(&tbl[i].lock, "bcache");
  
  int i = 0, cnt = 0;
  int step = NBUF/TBL_SIZE;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    if (cnt++ <= step) {
      b->tbl_index = i;
      insert_buf(&tbl[i].head, b);
      initsleeplock(&b->lock, "buffer");
      if (cnt == step)
        cnt=0, i = (i+1)%TBL_SIZE;
    }
  }
  for (int i = 0;i<TBL_SIZE;i++)
    countBuf(i);
}

int tbl_index(uint dev, uint blockno) {
  return (dev+blockno*100) % TBL_SIZE;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  //acquire(&bcache.lock);

  // Is the block already cached?
  int i = tbl_index(dev, blockno);
  acquire(&tbl[i].lock);
  // check cache
  for(b = tbl[i].head.next; b != &tbl[i].head; b = b->next)
    if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        release(&tbl[i].lock);
        acquiresleep(&b->lock);
        return b;
      }
  
  // steal free buf
  for (int j=i, cnt=0;cnt < TBL_SIZE;j=((j+1)%TBL_SIZE),cnt++) {
    if (i != j)
      acquire(&tbl[j].lock);
    for(b = tbl[j].head.next; b != &tbl[j].head; b = b->next){
      if(b->refcnt == 0){
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->tbl_index = i;
        struct buf *prev = b->prev, *next = b->next;
        if (prev) {
          prev->next = next;
        } else {
          tbl[j].head.next = next;
        }

        if (next) {
          next->prev = prev;
        }
        insert_buf(&tbl[i].head, b);
        if (i != j)
          release(&tbl[j].lock);
        release(&tbl[i].lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    if (i != j)
      release(&tbl[j].lock);
  }
  release(&tbl[i].lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&tbl[b->tbl_index].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = tbl[b->tbl_index].head.next;
    b->prev = &tbl[b->tbl_index].head;
    tbl[b->tbl_index].head.next->prev = b;
    tbl[b->tbl_index].head.next = b;
  }
  
  release(&tbl[b->tbl_index].lock);
}

void
bpin(struct buf *b) {
  acquire(&tbl[b->tbl_index].lock);
  b->refcnt++;
  release(&tbl[b->tbl_index].lock);
}

void
bunpin(struct buf *b) {
  acquire(&tbl[b->tbl_index].lock);
  b->refcnt--;
  release(&tbl[b->tbl_index].lock);
}


