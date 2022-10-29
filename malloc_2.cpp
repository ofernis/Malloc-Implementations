#include <unistd.h>
#include <string.h>

#define MAX_VAL 100000000

typedef struct MallocMetaData {
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
} *MetaData;

class BlocksLinkedList {
    private:
        MetaData list;
    public:
        BlocksLinkedList() : list(NULL) {};
        void* allocateBlock(size_t size);
        void insertNewBlock(MetaData new_block);
        void freeBlock(void* block);
        // get methods - useful for required stats methods
        MetaData getMetaData(void* block);
        size_t getNumOfTotalBlocks();
        size_t getNumOfTotalBytes();
        size_t getNumOfFreeBlocks();
        size_t getNumOfFreeBytes();
};

////////////////////////////////////
// Class methods implementations //
//////////////////////////////////

void* BlocksLinkedList::allocateBlock(size_t size) {
    size_t allocation_size = size + sizeof(MallocMetaData);
    MetaData iterator = this->list;
    while(iterator) {
        if (iterator->size >= size && iterator->is_free) {
            iterator->is_free = false;
            return iterator;
        }
        iterator = iterator->next;
    }
    void* prog_break = sbrk(allocation_size);
    if (prog_break == (void*) -1) {
        return NULL;
    }
    MetaData new_alloc_block = (MetaData) prog_break;
    new_alloc_block->size = size;
    new_alloc_block->is_free = false;
    new_alloc_block->next = NULL;
    new_alloc_block->prev = NULL;
    insertNewBlock(new_alloc_block); 
    return prog_break;
}

void BlocksLinkedList::insertNewBlock(MetaData new_block)
{
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

void BlocksLinkedList::freeBlock(void* block) {
    getMetaData(block)->is_free = true;
}

size_t BlocksLinkedList::getNumOfTotalBlocks() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        counter++;
        iterator = iterator->next;
    }
    return counter;
}

size_t BlocksLinkedList::getNumOfTotalBytes() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        counter += iterator->size;
        iterator = iterator->next;
    }
    return counter;
}

size_t BlocksLinkedList::getNumOfFreeBlocks() {
    MetaData iterator = this->list;
    size_t counter = 0;
    while (iterator) {
        if(iterator->is_free) {
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
        if (iterator->is_free) {
            counter += iterator->size;
        }
        iterator = iterator->next;
    }
    return counter;
}

///////////////////////////////////
// Basic malloc implementations //
/////////////////////////////////
MetaData BlocksLinkedList::getMetaData(void *block) {
    return MetaData ((char *) block - sizeof(MallocMetaData));
}
BlocksLinkedList blocks_list =  BlocksLinkedList(); // init our global list

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_VAL) {
        return NULL;
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
    blocks_list.freeBlock(p);
}

void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_VAL) {
        return NULL;
    }
    if (oldp == NULL) {
        return smalloc(size);
    }
    MetaData oldb = blocks_list.getMetaData(oldp);
    size_t size_old = oldb->size;
    if (size <= size_old) {
        return oldp;
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