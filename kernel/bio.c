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
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf bucket[NBUFBUCKET];
  struct spinlock bucketlock[NBUFBUCKET];
} bcache;

extern uint ticks;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < NBUFBUCKET; i++){
    bcache.bucket[i].next = 0;
    char name[10] = {0};
    snprintf(name, 10, "bcache-%d", i);
    initlock(&bcache.bucketlock[i], name);
  }

  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.bucket[0].next;
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].next = b;
    b->ticks = ticks;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int get_bucketno = blockno % NBUFBUCKET;
  struct buf *prev;
  struct buf *node;
  int evict_bucketno;
  uint min_ticks;

  acquire(&bcache.bucketlock[get_bucketno]);

  // Is the block already cached?
  for(b = bcache.bucket[get_bucketno].next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->ticks = ticks;
      release(&bcache.bucketlock[get_bucketno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  release(&bcache.bucketlock[get_bucketno]);

  // Anything possible.

  acquire(&bcache.lock);

  // Double check.
  acquire(&bcache.bucketlock[get_bucketno]);
  for(b = bcache.bucket[get_bucketno].next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      b->ticks = ticks;
      release(&bcache.bucketlock[get_bucketno]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucketlock[get_bucketno]);

  // Evict block.
  node = 0;
  prev = 0;
  min_ticks = ticks;
  evict_bucketno = -1;
  for(int j = 0; j < NBUFBUCKET; j++){
    acquire(&bcache.bucketlock[j]);
    for(b = &bcache.bucket[j]; b->next; b = b->next){
      if(b->next->refcnt == 0 && b->next->ticks <= min_ticks){
        min_ticks = b->next->ticks;
        prev = b;
        node = b->next;
        if(evict_bucketno != j && evict_bucketno != -1)
          release(&bcache.bucketlock[evict_bucketno]);
        evict_bucketno = j;
      }
    }
    if(evict_bucketno != j)
      release(&bcache.bucketlock[j]);
  }
  if(evict_bucketno != -1) {
    if(get_bucketno != evict_bucketno){
      prev->next = node->next;
      release(&bcache.bucketlock[evict_bucketno]);
      acquire(&bcache.bucketlock[get_bucketno]);
      node->next = bcache.bucket[get_bucketno].next;
      bcache.bucket[get_bucketno].next = node;
    }
    node->dev = dev;
    node->blockno = blockno;
    node->valid = 0;
    node->refcnt = 1;
    node->ticks = ticks;
    release(&bcache.bucketlock[get_bucketno]);
    release(&bcache.lock);
    acquiresleep(&node->lock);
    return node;
  }

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

  // acquire(&bcache.lock);
  acquire(&bcache.bucketlock[b->blockno % NBUFBUCKET]);
  b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

  release(&bcache.bucketlock[b->blockno % NBUFBUCKET]);
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  // acquire(&bcache.lock);
  acquire(&bcache.bucketlock[b->blockno % NBUFBUCKET]);
  b->refcnt++;
  b->ticks = ticks;
  release(&bcache.bucketlock[b->blockno % NBUFBUCKET]);
  // release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  // acquire(&bcache.lock);
  acquire(&bcache.bucketlock[b->blockno % NBUFBUCKET]);
  b->refcnt--;
  release(&bcache.bucketlock[b->blockno % NBUFBUCKET]);
  // release(&bcache.lock);
}


