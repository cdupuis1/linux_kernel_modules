Sample Block Driver
===================

This driver tries to be as simple a block driver as possible.  We use a block
of virtually contiguous memory to simulate an actual block device.  The pointer
to this is stored in our global driver structure, blk_example->store.  We read
and write to memory at the appropriate offset based on where the request points
to. 

Module parameters
-----------------

num_sectors - Size in 512 byte sectors for our virtual store

Initialization
--------------

To register a block device the following steps:

1. Call register_blkdev() to get a block major number
2. Set up blk_mq_tag_set struct which includes our function pointers for both
queuing a request or completing one.
3. Call blk_mq_alloc_tag_set() which allocates the tags for each request queue
4. Call blk_mq_alloc_disk() to allocate the request queue and gendisk object
5. Set the QUEUE_FLAG_NOMERGES so that each request is a page.  This is not
performant but simplfies the logic of handling a request later as each request
will not be more than a page.
6. Call set_capacity() to let the block layer know our disk size
7. Call add_disk() to make our disk usable to the rest of the system

Handling a request
------------------

To queue a request to our driver, the block layer calls our blk_ex_queue_rq()
callback.  Here we blk_mq_start_request() to let the block layer know that we
have started to process the request.  We then save a reference to our
private command struct which is allocated along with the block layer request.
We then queue a work item to the system workqueue with blk_example_complete()
as the callback function.

In blk_example_complete() we first determine where in our fake disk we're going
to read/write from using blk_rq_pos() which gives us the sector we're working
on.  We then multiply this by 512 to get the start of the byte position in our
fake disk.  We take our store_mutex so we have exclusive access to our virtual
store and then for each bio_vec we get the virtual address of where we're
reading/writing.  We then adjust the virtual address of the offset in the
bio_vec where we'll read/write from our virtual store.  Then we do a memcpy
who's direction depends on the data direction in the request.  We then adjust
the address into our page store where we'll read/write as we process the next
bio_vec.

The final step is to call blk_mq_complete_rq() to let the block layer know
that we're done processing this request.  We theb release our store_mutex so
another request can access the virtual store ending our processing of the
block layer request.


