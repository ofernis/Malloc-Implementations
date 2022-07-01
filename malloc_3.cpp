#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#define MAX_VAL (100000000)
#define MAP_SIZE (128 * 1024)

typedef struct MallocMetaData {
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
    MallocMetaData* next_by_size;
    MallocMetaData* prev_by_size;
} *MetaData;

class BlocksLinkedList {
private:
    MetaData list;
    MetaData list_by_size;

public:
    size_t num_of_map;
    size_t bytes_of_map;
    BlocksLinkedList() : list(NULL),list_by_size(NULL),
    num_of_map(0),bytes_of_map(0) {};
    void* allocateBlock(size_t size);
    void insertNewBlock(MetaData new_block);
    void freeBlock(void* block);
    //void addToList(MetaData block);
    //void removeFromList(MetaData block);
    void split(MetaData block,size_t size);
    void insertToListSize(MetaData block);
    void removeFromListAddress(MetaData block);
    void removeFromListSize(MetaData block);
    int alignTo8(size_t size);
    // get methods - useful for required stats methods
    MetaData get_metadata(void *block);
    size_t getNumOfTotalBlocks();
    size_t getNumOfTotalBytes();
    size_t getNumOfFreeBlocks();
    size_t getNumOfFreeBytes();
    //size_t _size_meta_data();
};

////////////////////////////////////
// Class methods implementations //
//////////////////////////////////
MetaData BlocksLinkedList::get_metadata(void *block) {
    return (MetaData) ((size_t) block - sizeof(MallocMetaData));
}
void* BlocksLinkedList::allocateBlock(size_t size) {
    size_t allocation_size = size + sizeof(MallocMetaData);
    MetaData iterator = this->list_by_size,wilderness=NULL;
    while(iterator)
    {
        if (iterator->size >= size && iterator->is_free)
        {
            iterator->is_free = false;
            removeFromListSize(iterator);
            return iterator;

            ////split?
        }
        wilderness=iterator;
        iterator = iterator->next;
    }
    if(wilderness&&wilderness->is_free)
    {
        void* prog_break = sbrk(alignTo8(allocation_size-wilderness->size));
        if (prog_break == (void*) -1) {
            return NULL;
        }
        removeFromListSize(wilderness);
        wilderness->size=alignTo8(allocation_size);
        wilderness->is_free= false;
        return prog_break;
    }
    void* prog_break = sbrk(alignTo8(allocation_size));
    if (prog_break == (void*) -1) {
        return NULL;
    }
    MetaData new_alloc_block = (MetaData) prog_break;
    new_alloc_block->size = alignTo8(size);
    new_alloc_block->is_free = false;
    new_alloc_block->next = NULL;
    new_alloc_block->prev = NULL;
    new_alloc_block->next_by_size = NULL;
    new_alloc_block->prev_by_size = NULL;
    insertNewBlock(new_alloc_block);
    return prog_break;
}

void BlocksLinkedList::insertNewBlock(MetaData new_block) {

    //to list by address to the end
    if(this->list==NULL)
    {
        this->list = new_block;
        return;
    }
    MetaData last = this->list;
    MetaData prev1 = last;

    while(last) {
        prev1 = last;
        last = last->next;
    }
    prev1->next = new_block;
    new_block->prev = prev1;

}
void BlocksLinkedList::insertToListSize(MetaData block) {
    MetaData iterator = this->list_by_size,prev=NULL;
    while (iterator) {
        if (iterator->size >= block->size && iterator > block) {
            if (iterator->prev_by_size == NULL) {
                block->next_by_size= this->list_by_size;
                block->next_by_size->prev_by_size=block;
                this->list_by_size =block;
            } else if (iterator == NULL) {
                prev->next_by_size= block;
                block->prev_by_size=prev;
                block->next_by_size=NULL;
            }
            else
            {
                block->next_by_size= iterator;
                block->next_by_size->prev_by_size=block;
                block->prev_by_size=prev;
                if(prev!=NULL)
                {
                    block->prev_by_size->next_by_size=block;
                }
            }
            return;
        }
        prev=iterator;
        iterator = iterator->next_by_size;
    }
    if(prev==NULL)
    {
        this->list_by_size=block;
        block->next_by_size=NULL;
        block->prev_by_size=NULL;
        return;
    }
    prev->next_by_size=block;
    block->prev_by_size=prev;
    block->next_by_size=iterator;
    if(iterator!=NULL)
    {
        iterator->prev_by_size=block;
    }
}

void BlocksLinkedList::removeFromListAddress(MetaData block)
{
    if (block->prev== NULL)//block is first
    {
        this->list = block->next;
        block->next=NULL;
        if(this->list)
        {
            this->list->prev=NULL;
        }
        return;
    }
        // block is last
    else if (block->next == NULL) {
        block->prev->next = NULL;
        block->prev = NULL;
        return;
    }
    else {
        block->prev->next = block->next;
        block->next->prev = block->prev;
        block->prev = NULL;
        block->next = NULL;
    }

}

void BlocksLinkedList::removeFromListSize(MetaData block)
{
    if (block->prev_by_size == NULL)//block is smallest
    {
        this->list_by_size = block->next_by_size;
        block->next_by_size=NULL;
        if(this->list_by_size)
        {
            this->list_by_size->prev_by_size=NULL;
        }
        return;
    }
        // block is largest
    else if (block->next_by_size == NULL) {
        block->prev_by_size->next_by_size = NULL;
        block->prev_by_size = NULL;
        return;
    }
    else {
        block->prev_by_size->next_by_size = block->next_by_size;
        block->next_by_size->prev_by_size = block->prev_by_size;
        block->prev_by_size = NULL;
        block->next_by_size = NULL;
    }

}
int BlocksLinkedList::alignTo8(size_t size) {
    if(size%8==0) {
        return size;
    }
    return size+8-(size%8);
}

void BlocksLinkedList::freeBlock(void* ptr) {
    MetaData block=get_metadata(ptr);
    block->is_free = true;

    if(block->next!=NULL && block->next->is_free&&
       block->prev!=NULL && block->prev->is_free)
    {
        MetaData prev_block=block->prev;
        removeFromListSize(block->prev);
        removeFromListSize(block->next);
        block->prev->size = alignTo8(block->prev->size + block->size + 2*sizeof(MallocMetaData)+ block->next->size);
        removeFromListAddress(block->next);
        removeFromListAddress(block);
        insertToListSize(prev_block);
    }
    else if(block->prev!=NULL && block->prev->is_free)//need to merge block with block before
    {
        MetaData prev_block=block->prev;
        removeFromListSize(block);
        removeFromListSize(block->prev);

        block->prev->size = alignTo8(block->prev->size + block->size + sizeof(MallocMetaData));
        removeFromListAddress(block);
        insertToListSize(prev_block);
    }
    else if(block->next!=NULL && block->next->is_free)//need to merge block with block before
    {
        removeFromListSize(block);
        removeFromListSize(block->next);

        block->size = alignTo8(block->size + block->next->size + sizeof(MallocMetaData));
        removeFromListAddress(block->next);
        insertToListSize(block);
    }
    else
    {
        insertToListSize(block);
    }
}
void BlocksLinkedList::split(MetaData block,size_t size)
{
    if(block->size-size-sizeof(MallocMetaData)>128)
    {
        //need to split blocks challenge 1
        MetaData new_alloc=(MetaData) ((char *) block + size +sizeof(MallocMetaData));
        new_alloc->is_free=true;
        new_alloc->size=block->size-size-sizeof(MallocMetaData);
        if(block->next&&block->next->is_free)
        {
            new_alloc->size+=block->next->size+sizeof(MallocMetaData);
            removeFromListAddress(block->next);
            removeFromListSize(block->next);
        }
        new_alloc->size= alignTo8(new_alloc->size);
        insertToListSize(new_alloc);
        new_alloc->next=block->next;

        new_alloc->prev=block;
        if(block->next!=NULL)
        {
            block->next->prev = new_alloc;
        }
        block->next=new_alloc;
    }
}
size_t BlocksLinkedList::getNumOfTotalBlocks() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        if (iterator->size <= MAP_SIZE) {
            counter++;
            iterator = iterator->next;
        }
    }
    counter+=this->num_of_map;
    return counter;
}

size_t BlocksLinkedList::getNumOfTotalBytes() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        if(iterator->size<=MAP_SIZE) {
            counter += iterator->size;
            iterator = iterator->next;
        }
    }
    counter+=this->bytes_of_map;
    return counter;
}

size_t BlocksLinkedList::getNumOfFreeBlocks() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        if(iterator->is_free&&iterator->size<=MAP_SIZE) {
            counter++;
        }
        iterator = iterator->next;
    }
    return counter;
}

size_t BlocksLinkedList::getNumOfFreeBytes() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        if (iterator->is_free&&iterator->size<=MAP_SIZE) {
            counter += iterator->size;
        }
        iterator = iterator->next;
    }
    return counter;
}
///////////////////////////////////
// Basic malloc implementations //
/////////////////////////////////

BlocksLinkedList blocks_list =  BlocksLinkedList(); // init our global list

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_VAL) {
        return NULL;
    }
    if (size > MAP_SIZE) {
        void *block = mmap(NULL, sizeof(MallocMetaData) + size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (block == MAP_FAILED) {
            return NULL;
        }
        MetaData my_block=(MetaData)block;
        my_block->is_free = false;
        my_block->size = size;
        blocks_list.bytes_of_map+=size;
        blocks_list.num_of_map++;
        return (char*)block+sizeof(MallocMetaData);
    }

    void* prog_break = blocks_list.allocateBlock(size);
    if (prog_break == (void*) -1) {
        return NULL;
    }
    return (char*) prog_break + sizeof(MallocMetaData); // return the address of prog_break with an offset of the meta-data struct size
}

void* scalloc(size_t num, size_t size) {
    void* ptr = smalloc(num * size);
    if (ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, num * size);
    return ptr;
}

void sfree(void* p) {
    if (p == NULL) {
        return;
    }
    MetaData data=blocks_list.get_metadata(p);

    if(data->size>MAP_SIZE)
    {
        blocks_list.num_of_map--;
        blocks_list.bytes_of_map-=data->size;
        munmap(data, sizeof(MallocMetaData) + data->size);
    }
    else
    {
        blocks_list.freeBlock(p);
    }
}

void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_VAL) {
        return NULL;
    }
    if (oldp == NULL) {
        return smalloc(size);
    }
    MetaData oldb = blocks_list.get_metadata(oldp);//check if need to use memset
    size_t size_old = oldb->size;
    if (size <= size_old) { //case A use same block
        oldb->size=size;
        blocks_list.split(oldb,size);
        return oldp;
    }
    unsigned int possible_size=size_old;
    if(oldb->prev&&oldb->prev->is_free)
    {
        possible_size+=oldb->prev->size+sizeof(MallocMetaData);
    }
    if(possible_size>=size)
    {//case B try adjacent prev block
        blocks_list.removeFromListSize(oldb);
        blocks_list.removeFromListSize(oldb->prev);

        oldb->prev->size = blocks_list.alignTo8(oldb->prev->size + oldb->size + sizeof(MallocMetaData));
        blocks_list.removeFromListAddress(oldb);
        oldb->prev->is_free= false;
        blocks_list.split(oldb->prev,size);
        return oldb->prev;
    }
    else
    {
        if(oldb->next==NULL)//wilderness case B2 + C
        {
            void* prog_break = sbrk(blocks_list.alignTo8(size-possible_size-sizeof(MallocMetaData)));
            if (prog_break == (void*) -1) {
                return NULL;
            }
            blocks_list.removeFromListSize(oldb);
            oldb->size=blocks_list.alignTo8(size);
            oldb->is_free= false;
            return oldb;
        }
        else
        {//not wilderness case
            possible_size=size_old;
            if(oldb->next->is_free)
            {
                possible_size+=oldb->next->size+sizeof(MallocMetaData);
            }
            if(possible_size>=size)
            {//case D merge higher address
                blocks_list.removeFromListSize(oldb);
                blocks_list.removeFromListSize(oldb->next);
                oldb->size = blocks_list.alignTo8(oldb->next->size + oldb->size + sizeof(MallocMetaData));
                blocks_list.removeFromListAddress(oldb->next);
                blocks_list.split(oldb,size);
                return oldb;
            }
        }
    }//case A to D failed

    possible_size=size_old;
    if(oldb->prev&&oldb->prev->is_free)
    {
        possible_size+=oldb->prev->size+sizeof(MallocMetaData);
    }
    if(oldb->next&&oldb->next->is_free)
    {
        possible_size+=oldb->next->size+sizeof(MallocMetaData);
    }
    if(possible_size>=size)
    {//case E try all three blocks
        blocks_list.removeFromListSize(oldb);
        blocks_list.removeFromListSize(oldb->prev);
        blocks_list.removeFromListSize(oldb->next);
        oldb->prev->size = blocks_list.alignTo8(size);
        blocks_list.removeFromListAddress(oldb);
        blocks_list.removeFromListAddress(oldb->next);
        return oldb->prev;
    }
    else
    {
        if(oldb->next->next==NULL)//wilderness case F1 F2
        {
            void* prog_break = sbrk(blocks_list.alignTo8(size-possible_size-sizeof(MallocMetaData)));
            if (prog_break == (void*) -1) {
                return NULL;
            }
            blocks_list.removeFromListSize(oldb);
            blocks_list.removeFromListSize(oldb->next);
            oldb->size=blocks_list.alignTo8(size);
            oldb->is_free= false;
            blocks_list.removeFromListAddress(oldb->next);
            if(oldb->prev&&oldb->prev->is_free)
            {
                oldb->prev->size=oldb->size;//case F1
                oldb->prev->is_free= false;
                blocks_list.removeFromListSize(oldb->prev);
                blocks_list.removeFromListAddress(oldb);
            }

            return oldb;
        }
        else
        {
            sfree(oldp);//case G + H
            smalloc(size);
        }
    }





    void* newp = smalloc(size);
    if (newp == NULL) {
        return NULL;
    }
    memcpy(newp, oldp, oldb->size);
    sfree(oldp);
    return newp;
}


size_t _num_free_blocks() {
    return blocks_list.getNumOfFreeBlocks();
}

size_t _num_free_bytes() {
    return blocks_list.getNumOfFreeBytes();
}

size_t _num_allocated_blocks() {
    return blocks_list.getNumOfTotalBlocks();
}

size_t _num_allocated_bytes() {
    return blocks_list.getNumOfTotalBytes(); // maybe should be (Total - Free) ?
}

size_t _num_meta_data_bytes() {
    return sizeof(MallocMetaData) * blocks_list.getNumOfTotalBlocks();
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}