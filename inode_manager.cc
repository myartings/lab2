#include "inode_manager.h"
#include "ctime" // My change
#include "string" //My chagne
// disk layer -----------------------------------------

disk::disk()
{
    bzero(blocks, sizeof(blocks));
}

    void
disk::read_block(blockid_t id, char *buf)
{
    if (id < 0 || id >= BLOCK_NUM || buf == NULL)
	return;

    memcpy(buf, blocks[id], BLOCK_SIZE);
}

    void
disk::write_block(blockid_t id, const char *buf)
{
    if (id < 0 || id >= BLOCK_NUM || buf == NULL)
	return;

    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
    blockid_t
block_manager::alloc_block()
{
    /*
     * your lab1 code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */

    //My change

    char buf[BLOCK_SIZE];
    blockid_t start_id = IBLOCK(INODE_NUM,sb.nblocks) + 1;
    for (blockid_t id = start_id; id < BLOCK_NUM; id++)
    {
	read_block(BBLOCK(id), buf);
	char temp;
	temp = *(buf + (id % BPB)/8) & (1 <<(7 -( (id % BPB) % 8)));

	if(temp == 0) 
	{
	    *(buf + (id % BPB)/8) = *(buf + (id % BPB)/8) ^ (1 <<(7- ((id % BPB)%8)));
	    write_block(BBLOCK(id), buf);

	    return id;
	}

    }

    //end change

    return 0;
}

    void
block_manager::free_block(uint32_t id)
{
    /* 
     * your lab1 code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */

    //My change
    
    char buf[BLOCK_SIZE];
    read_block(BBLOCK(id), buf);

    *(buf + (id % BPB) / 8)  = *(buf + (id % BPB) / 8) ^ (1 <<(7 -( (id % BPB)%8)));
    write_block(BBLOCK(id), buf);

    //end change

    return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

}

    void
block_manager::read_block(uint32_t id, char *buf)
{
    d->read_block(id, buf);
}

    void
block_manager::write_block(uint32_t id, const char *buf)
{
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
	printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
	exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
    uint32_t
inode_manager::alloc_inode(uint32_t type)
{
    /* 
     * your lab1 code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().
     */

    //My change
    struct inode * ino;
    int32_t i;
    for( i = 1; i <= INODE_NUM; i++ )
    {
	ino = get_inode(i);
	if(ino == NULL) break;
    }

    ino = (struct inode*)malloc(sizeof(struct inode));
    ino->type = type;
    ino->size = 0;
    time_t now_time;
    now_time = time(NULL);
    ino->atime = (unsigned int)now_time;
    ino->ctime = (unsigned int)now_time;
    ino->mtime = (unsigned int)now_time;
    put_inode(i, ino);

    free(ino);

    return i;
    //end change 

    return 1;
}

    void
inode_manager::free_inode(uint32_t inum)
{
    /* 
     * your lab1 code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */

    //My change
    struct inode * ino = get_inode(inum);

    if(ino !=  NULL)
    {
	int block_cnt = ino->size / BLOCK_SIZE;
	if(ino->size % BLOCK_SIZE) block_cnt++;

	for(int i = 0; i< NDIRECT; i++)
	    ino->blocks[i] = 0;
	ino->type = 0;
	ino->size = 0;

	put_inode(inum,ino);
    }

    free(ino);

    //end change
    return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
    struct inode* 
inode_manager::get_inode(uint32_t inum)
{
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if (inum < 0 || inum >= INODE_NUM) {
	printf("\tim: inum out of range\n");
	return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode*)buf + inum%IPB;

    if (ino_disk->type == 0) {
	printf("\tim: inode not exist\n");
	return NULL;
    }

    ino = (struct inode*)malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

    void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
	return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
    void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
    /*
     * your lab1 code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_Out
     */

    //My change
    struct inode * ino = get_inode(inum);
    *size = ino->size ;
    char last_block[BLOCK_SIZE];
    *buf_out = (char *)malloc(*size);
    //compute the number of using blocks
    int block_cnt = ino->size / BLOCK_SIZE;
    if(ino->size % BLOCK_SIZE) block_cnt += 1;
    if(block_cnt > NDIRECT)
    {

	for(int i = 0; i < NDIRECT; i++)
	{

	    bm->read_block(ino->blocks[i], *buf_out + i * BLOCK_SIZE) ;

	}


	char buf_temp[BLOCK_SIZE];
	bm->read_block(ino->blocks[NDIRECT], buf_temp);
	blockid_t * bid;

	for(int i = 0; i< block_cnt - NDIRECT; i++)
	{

	    bid = (blockid_t *)buf_temp + i;
	    /*if last block*/
	    if(i == block_cnt - NDIRECT - 1)
	    {
		bm->read_block(*bid, last_block);
		memcpy(*buf_out + (block_cnt - 1) * BLOCK_SIZE, last_block, *size - (block_cnt - 1) * BLOCK_SIZE);
	    }
	    else
		bm->read_block(*bid, *buf_out + (NDIRECT + i) * BLOCK_SIZE);
	}

    }

    else 
    {
	for(int i = 0; i < block_cnt; i++)
	{
	    /*if last block*/
	    if( i == block_cnt - 1)
	    {
		bm->read_block(ino->blocks[block_cnt - 1], last_block);
		memcpy(*buf_out + (block_cnt - 1) * BLOCK_SIZE, last_block, *size - (block_cnt - 1) * BLOCK_SIZE);
	    }
	    else
		bm->read_block(ino->blocks[i], *buf_out + i * BLOCK_SIZE);
	}
    }

   /* std::string s = *buf_out;
 for(int i = 0; i< *size/4096; i++)
	{
	    std::string stemp = s.substr(i*4096,100);
    printf("ino is %d, size is %d, ino reading content is %s \n", ino, size,  stemp.c_str());
	stemp = s.substr((i+1)*4096- 100, 100);
    printf("ino is %d, size is %d, ino reading content is %s \n", ino, size,  stemp.c_str());
	printf("________________________________________________________\n");

	}*/
    time_t now_time;
    now_time = time(NULL);
    ino->atime = (unsigned int)now_time;
    put_inode(inum, ino);
    free(ino);
    
    //end change

    return;
}

/* alloc/free blocks if needed */
    void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
    /*
     * your lab1 code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf 
     * is larger or smaller than the size of original inode
     */

    //My change
    if(size < 0) return ;
    if(size > (int) (MAXFILE * BLOCK_SIZE)) 
	size = MAXFILE * BLOCK_SIZE;
    struct inode * ino = get_inode(inum);

    int block_cnt = size/BLOCK_SIZE;
    if( size % BLOCK_SIZE) block_cnt += 1;
    int block_cnt_old = ino->size / BLOCK_SIZE;
    if( ino->size % BLOCK_SIZE) block_cnt_old += 1;

    char buf_temp[BLOCK_SIZE];
    blockid_t * bid;

    char last_block[BLOCK_SIZE];
    memset(last_block,0,BLOCK_SIZE);

    /* cases which needs to free blocks */
    if(block_cnt <= block_cnt_old)
    {
	if(block_cnt_old <= NDIRECT)  /* b < b0 < N */
	{
	    for(int i = 0; i < block_cnt; i++ )
	    {
		/*if last block*/
		if(i == block_cnt - 1)
		{
		    memcpy(last_block, buf + (block_cnt - 1) * BLOCK_SIZE, size - (block_cnt -1) * BLOCK_SIZE);
		    bm->write_block(ino->blocks[i], last_block);
		}

		else
		    bm->write_block(ino->blocks[i] , buf + BLOCK_SIZE * i);
	    }
	    for(int i = 0; i < block_cnt_old - block_cnt; i++)
	    {

		ino->blocks[block_cnt_old - i] = 0;
		bm->free_block(ino->blocks[block_cnt_old - i ]);

	    }
	}//end if 

	else  
	{

	    bm->read_block(ino->blocks[NDIRECT], buf_temp); //get indirect block data

	    if(block_cnt <= NDIRECT)  /* b < N < b0 */
	    {
		for(int i = 0; i < block_cnt; i++)
		{
		    /*if last block*/
		    if(i == block_cnt - 1)
		    {
			memcpy(last_block, buf + (block_cnt - 1) * BLOCK_SIZE, size - (block_cnt -1) * BLOCK_SIZE);
			bm->write_block(ino->blocks[i], last_block);
		    }
		    else
			bm->write_block(ino->blocks[i] , buf + BLOCK_SIZE * i);

		}
		for(int i = 0; i < block_cnt_old - block_cnt; i++)
		{   
		    ino->blocks[block_cnt_old - i] = 0;
		    bm->free_block(ino->blocks[block_cnt_old - i]);
		}

		for(int i = 0; i< block_cnt_old - NDIRECT ; i++)
		{   

		    bid = (blockid_t *)buf_temp + i;
		    bm->free_block(*bid);

		}
		/*free indirect block*/

		bm->free_block(ino->blocks[NDIRECT]);
		ino->blocks[NDIRECT] = 0;

	    }// end if

	    else   /* N < b < b0 */
	    {

		for(int i = 0; i < NDIRECT; i++)
		    bm->write_block(ino->blocks[i], buf + BLOCK_SIZE * i);

		for(int i = 0; i < block_cnt - NDIRECT; i++)
		{
		    bid = (blockid_t *)buf_temp + i;
		    /*if last block*/
		    if(i == block_cnt - NDIRECT)
		    {
			memcpy(last_block, buf + (block_cnt - 1) * BLOCK_SIZE, size - (block_cnt -1) * BLOCK_SIZE);
			bm->write_block(*bid, last_block);
		    }
		    else
			bm->write_block(*bid , buf + (NDIRECT + i) *BLOCK_SIZE );
		}

		for(int i = block_cnt; i < block_cnt_old - block_cnt; i++)
		{
		    bid = (blockid_t *)buf_temp + i;
		    bm->free_block(*bid);
		}

	    }//end else



	} // end  else

    }
    else  /*when block_cnt > block_cnt_old, need to allocate new blocks*/
    {	
	if( block_cnt < NDIRECT) /* b0 < b < N */
	{
	    for(int i = 0; i < block_cnt_old; i++)
	    {
		bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
	    }

	    for(int i = 0; i < block_cnt - block_cnt_old; i++)
	    {
		blockid_t id = bm->alloc_block();
		/*if last block*/
		if(i == block_cnt - block_cnt_old - 1)
		{
		    memcpy(last_block, buf + (block_cnt - 1) * BLOCK_SIZE, size - (block_cnt -1) * BLOCK_SIZE);
		    bm->write_block(id, last_block);
		}
		else
		    bm->write_block(id, buf + (i + block_cnt_old) * BLOCK_SIZE);

		/* store the block id in inode*/

		ino->blocks[block_cnt_old + i] = id;

	    }
	}

	else 
	{
	    bm->read_block(ino->blocks[NDIRECT], buf_temp);
	    if( block_cnt_old > NDIRECT)  /* N < b0 < b */
	    {
		for(int i = 0; i < NDIRECT; i++)
		    bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);

		for(int i = 0; i < block_cnt_old - NDIRECT; i++)
		{
		    bid = (blockid_t *)buf_temp + i;
		    bm->write_block(*bid, buf + (NDIRECT + i) * BLOCK_SIZE);

		}


		for(int i = 0; i < block_cnt - block_cnt_old; i++)
		{
		    blockid_t id = bm->alloc_block();
		    /*if last block*/
		    if(i == block_cnt - block_cnt_old - 1)
		    {
			memcpy(last_block, buf + (block_cnt - 1) * BLOCK_SIZE, size - (block_cnt -1) * BLOCK_SIZE);
			bm->write_block(id, last_block);
		    }
		    else
			bm->write_block(id, buf + (block_cnt_old + i) * BLOCK_SIZE);

		    /* store the block id in indirect block*/
		    memcpy(buf_temp + (block_cnt_old - NDIRECT + i)*sizeof(blockid_t), &id, sizeof(blockid_t));
		    bm->write_block(ino->blocks[NDIRECT], buf_temp);
		}
	    }

	    else   /* b0 < N < b */
	    {
		/*allocate a block for indirect block*/
		blockid_t indirect_id = bm->alloc_block();
		ino->blocks[NDIRECT] = indirect_id;
		blockid_t * temp_id;

		for(int i = 0; i < block_cnt_old; i++)
		    bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);

		for(int i = 0; i < NDIRECT - block_cnt_old; i++)
		{
		    blockid_t id = bm->alloc_block();
		    bm->write_block(id, buf + (block_cnt_old + i) * BLOCK_SIZE);

		    /* store the block id in inode*/
		    ino->blocks[block_cnt_old + i] = id;
		}

		for(int i = 0; i < block_cnt - NDIRECT; i++)
		{
		    blockid_t id = bm->alloc_block();
		    /*if last block*/
		    if(i == block_cnt - NDIRECT - 1)
		    {
			memcpy(last_block, buf + (block_cnt - 1) * BLOCK_SIZE, size - (block_cnt -1) * BLOCK_SIZE);
			bm->write_block(id, last_block);
		    }
		    else
			bm->write_block(id, buf + (NDIRECT + i) * BLOCK_SIZE);

		    /* store the block id in indirect block*/
		    temp_id = (blockid_t *)buf_temp + i;
		    *temp_id = id;

		    bm->write_block(ino->blocks[NDIRECT], buf_temp);

		}

	    }

	}

    }

   /* for(int i = 0; i< temp_str.size()/4096; i++)
	{
    string stemp = temp_str.substr(i*4096,100);
    printf("ino is %d, size is %d, off is %d, read content is %s \n", ino, size, off, stemp.c_str());
	stemp = temp_str.substr((i+1)*4096- 100, 100);
    printf("ino is %d, size is %d, off is %d, read content is %s \n", ino, size, off, stemp.c_str());
	printf("/////////////////////////////////////////////////////\n");

	}*/
    ino->size = size;
    time_t now_time;
    now_time = time(NULL);
    ino->mtime = (unsigned int)now_time;
    ino->ctime = (unsigned int)now_time;
    put_inode(inum, ino);
    free(ino);
    //end change

    return;
}

    void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
    /*
     * your lab1 code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */

    //My change
    struct inode * ino = get_inode(inum);
    if(ino == NULL) return;
    a.type = ino->type;
    a.size = ino->size;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;

    free(ino);
    //end change

    return;
}

    void
inode_manager::remove_file(uint32_t inum)
{
    /*
     * your lab1 code goes here
     * note: you need to consider about both the data block and inode of the file
     */
    //My change
    struct inode * ino = get_inode(inum);

    if( ino != NULL)
    {
	char buf_temp[BLOCK_SIZE];
	blockid_t * id;
	int block_cnt = ino->size / BLOCK_SIZE;
	if(ino->size % BLOCK_SIZE) block_cnt++;

	/* free date block */
	if(block_cnt <= NDIRECT)
	    for(int i = 0; i < block_cnt; i++)
	    {
		bm->free_block(ino->blocks[i]);
	    }

	else
	{
	    bm->read_block(ino->blocks[NDIRECT],buf_temp);

	    for(int i = 0; i < NDIRECT; i++)
		bm->free_block(ino->blocks[i]);

	    for(int i = 0; i < block_cnt - NDIRECT; i++)
	    {
		id = (blockid_t *)buf_temp + i;
		bm->free_block(*id);
	    }
	    bm->free_block(ino->blocks[NDIRECT]);
	}

	/*free inode*/
	free_inode(inum);
    }
    //end change

    return;
}
