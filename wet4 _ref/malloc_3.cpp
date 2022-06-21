#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define MAX_SIZE 100000000
#define HIST_MAX 128
#define MIN_GAP 128

enum merge {
    MergeLeft, MergeRight, MergeBoth
};

typedef struct MallocMetaData {
    size_t size;
    bool is_free;
    MallocMetaData *next;
    MallocMetaData *prev;
    MallocMetaData *next_hist;
    MallocMetaData *prev_hist;
} MMD;

class BlockList {
private:
    MMD *block_list;

    int mmd_blocks;
    int mmd_bytes;
public:
    MMD *hist[HIST_MAX];

    BlockList() : block_list(NULL), hist(), mmd_blocks(0), mmd_bytes(0) {
    };

    void insertBlock(MMD *block);

    void removeBlock(MMD *block);

    void addBlockToHist(MMD *block);

    void removeBlockFromHist(MMD *block);

    void mergeBlock(MMD *block, merge option);

    void splitBlock(MMD *block, size_t size);

    void freeBlock(void *ptr);

    void freeMapBlock(void *ptr);

    void *allocateBlock(size_t size);

    void *allocateMapBlock(size_t size);

    MMD *get_mmd(void *p);


    size_t numFreeBlocks();

    size_t numFreeBytes();

    size_t numTotalBlocks();

    size_t numTotalBytes();

    void add_bytes(size_t num) {
        mmd_bytes += num;
    }
};


MMD *BlockList::get_mmd(void *p) {
    return (MMD *) ((char *) p - sizeof(MMD));
}

void BlockList::removeBlockFromHist(MMD *block) {
    int hist_index = block->size / 1000;

    // block is head
    if (block->prev_hist == NULL) {
        hist[hist_index] = block->next_hist;
        block->next_hist = NULL;
        return;
    }
    // block is tail
    if (block->next_hist == NULL) {
        block->prev_hist->next_hist = NULL;
        block->prev_hist = NULL;
        return;
    }
    // block in the middle
    block->prev_hist->next_hist = block->next_hist;
    block->next_hist->prev_hist = block->prev_hist;
    block->prev_hist = NULL;
    block->next_hist = NULL;
}

void BlockList::addBlockToHist(MMD *block) {
    int index = block->size / 1000;

    MMD *temp = hist[index], *current_block = NULL;

    // List is empty
    if (temp == NULL) {
        hist[index] = block;
        return;
    }

    while (temp) {
        if (block->size < temp->size) {
            current_block = temp;
            break;
        }
        current_block = temp;
        temp = temp->next_hist;
    }

    // Got to the end of the list
    if (temp == NULL) {
        current_block->next_hist = block;
        block->prev_hist = current_block;
        return;
    }

    if (current_block->prev_hist == NULL) {
        // current_block is head
        hist[index] = block;
        block->next_hist = current_block;

    } else {
        // current_block is in the middle
        block->prev_hist = current_block->prev_hist;
        block->next_hist = current_block;
        block->prev_hist->next_hist = block;
        current_block->prev_hist = block;
    }
}

void *BlockList::allocateMapBlock(size_t size) {
    void *new_block = mmap(NULL, sizeof(MMD) + size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_block == MAP_FAILED)
        return NULL;

    MMD *new_mmd_block = (MMD *) new_block;
    new_mmd_block->size = size;
    new_mmd_block->is_free = false;
    mmd_blocks++;
    mmd_bytes += size;
    return new_block;
}

void *BlockList::allocateBlock(size_t size) {
    int hist_index = size / 1000;
    MMD *block_found = NULL;

    // Find block if available
    for (int i = hist_index; i <= HIST_MAX; i++) {
        MMD *block = hist[i];
        while (block) {
            if (block->is_free && size <= block->size) {
                block_found = block;
                break;
            }
            block = block->next;
        }
        if (block_found)
            break;
    }

    if (block_found) {
        removeBlockFromHist(block_found);
        if (size + MIN_GAP + sizeof(MMD) <= block_found->size) {
            splitBlock(block_found, size);
            block_found->size = size;
        }
        block_found->is_free = false;
        return block_found;

    } else {
        // allocate more space from heap
        MMD *temp = block_list;
        size_t alloc_size = size + sizeof(MMD);

        // Find last block
        while (temp && temp->next) {
            temp = temp->next;
        }
        //check for Wilderness
        if (temp && temp->is_free) {
            alloc_size -= (temp->size + sizeof(MMD));
            removeBlockFromHist(temp);
            if (sbrk(alloc_size) == (void *) -1) {
                return NULL;
            }
            temp->size = size;
            temp->is_free = false;
            return temp;
        }
        void *program_break = sbrk(alloc_size);
        if (program_break == (void *) -1)
            return NULL;

        MMD *new_block = (MMD *) program_break;
        new_block->size = size;
        new_block->is_free = false;
        new_block->next = NULL;
        new_block->prev = NULL;
        new_block->next_hist = NULL;
        new_block->prev_hist = NULL;

        insertBlock(new_block);

        return program_break;
    }
}

void BlockList::splitBlock(MMD *block, size_t size) {
    MMD *new_block = (MMD *) ((char *) block + size + sizeof(MMD));
    new_block->size = block->size - size - sizeof(MMD);
    new_block->is_free = true;

    // insert to block_list
    new_block->next = block->next;
    new_block->prev = block;
    block->next = new_block;

    if (new_block->next){
        new_block->next->prev = new_block;
        if(new_block->next->is_free){
            mergeBlock(new_block, MergeRight);
        }
    }



    // insert to histogram
    addBlockToHist(new_block);
}

void BlockList::mergeBlock(MMD *block, merge option) {
    if (option == MergeLeft) {
        MMD *prev_block = block->prev;
        removeBlockFromHist(block);
        removeBlockFromHist(prev_block);

        prev_block->size = prev_block->size + block->size + sizeof(MMD);
        removeBlock(block);
        addBlockToHist(prev_block);
    } else if (option == MergeRight) {
        MMD *next_block = block->next;
        removeBlockFromHist(block);
        removeBlockFromHist(next_block);

        block->size = block->size + next_block->size + sizeof(MMD);
        removeBlock(next_block);

        addBlockToHist(block);
    } else {
        MMD *prev_block = block->prev;
        MMD *next_block = block->next;

        removeBlock(block);
        removeBlock(next_block);

        removeBlockFromHist(block);
        removeBlockFromHist(prev_block);
        removeBlockFromHist(next_block);

        prev_block->size = prev_block->size + block->size + next_block->size + 2 * sizeof(MMD);
        addBlockToHist(prev_block);
    }
}

void BlockList::insertBlock(MMD *block) {
    MMD *tail = block_list, *prev = NULL;
    while (tail) {
        prev = tail;
        tail = tail->next;
    }
    if (prev == NULL) {
        block_list = block;
    } else {
        prev->next = block;
        block->prev = prev;
    }

}

void BlockList::freeMapBlock(void *ptr) {
    MMD *block = get_mmd(ptr);
    mmd_bytes -= block->size;
    mmd_blocks--;
    munmap(block, sizeof(MMD) + block->size);

}

void BlockList::removeBlock(MMD *block) {

    // block is head
    if (block->prev == NULL) {
        block_list = block->next;
        block->next = NULL;
        return;
    }
    // block is tail
    if (block->next == NULL) {
        block->prev->next = NULL;
        block->prev = NULL;
        return;
    }
    // block in the middle
    block->prev->next = block->next;
    block->next->prev = block->prev;
    block->prev = NULL;
    block->next = NULL;
}

void BlockList::freeBlock(void *ptr) {
    MMD *block = get_mmd(ptr);
    block->is_free = true;
    bool next_block_free = block->next && block->next->is_free;
    bool prev_block_free = block->prev && block->prev->is_free;

    if (!next_block_free && !prev_block_free) {
        addBlockToHist(block);
        return;
    }


    merge option;
    if (next_block_free) {
        if (prev_block_free) {
            option = MergeBoth;
        } else {
            option = MergeRight;
        }
    } else {
        option = MergeLeft;
    }
    mergeBlock(block, option);
}

size_t BlockList::numFreeBlocks() {
    MMD *temp = block_list;
    size_t count = 0;
    while (temp) {
        if (temp->is_free) count++;
        temp = temp->next;

    }
    return count;
}

size_t BlockList::numFreeBytes() {
    MMD *temp = block_list;
    size_t free_bytes = 0;
    while (temp) {
        if (temp->is_free)
            free_bytes += temp->size;
        temp = temp->next;
    }
    return free_bytes;
}

size_t BlockList::numTotalBlocks() {
    MMD *temp = block_list;
    size_t count = 0;
    while (temp) {
        count++;
        temp = temp->next;
    }
    return count + mmd_blocks;
}

size_t BlockList::numTotalBytes() {
    MMD *temp = block_list;
    size_t free_bytes = 0;
    while (temp) {
        free_bytes += temp->size;
        temp = temp->next;
    }
    return free_bytes + mmd_bytes;
}


// ******** Main Functions **********

BlockList bl = BlockList();

void *smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE)
        return NULL;
    void *allocated;
    if (size > 128e3) {
        allocated = bl.allocateMapBlock(size);
    } else {
        allocated = bl.allocateBlock(size);
    }
    if (allocated == NULL)
        return NULL;
    return (char *) allocated + sizeof(MMD);
}

void *scalloc(size_t num, size_t size) {
    void *p = smalloc(num * size);
    if (p == NULL)
        return NULL;
    memset(p, 0, num * size);
    return p;
}

void sfree(void *p) {
    if (p == NULL)
        return;
    MMD *block = bl.get_mmd(p);
    if (block->size > 128e3) {
        bl.freeMapBlock(p);
    } else {
        bl.freeBlock(p);
    }
}

void *srealloc(void *oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE)
        return NULL;

    if (oldp == NULL)
        return smalloc(size);
    if (size > 128e3) {
        MMD *old_mmd = bl.get_mmd(oldp);
        void *newp = smalloc(size);
        if (size <= old_mmd->size) {
            memcpy(newp, oldp, size);
        } else {
            memcpy(newp, oldp, old_mmd->size);
        }
        sfree(oldp);
        return newp;
    }
    MMD *old_block = bl.get_mmd(oldp);
    size_t old_size = old_block->size;
    if (size <= old_size) {
        if (old_size - (size + sizeof(MMD)) > MIN_GAP) {
            bl.splitBlock(old_block, size);
            old_block->size = size;
        }
        return oldp;
    }


    bool next_block_free = old_block->next && old_block->next->is_free;
    bool prev_block_free = old_block->prev && old_block->prev->is_free;

    if (prev_block_free && size <= old_block->size + old_block->prev->size + sizeof(MMD)) {
        MMD *prev_block = old_block->prev;
        if (old_block->size + prev_block->size - size >= MIN_GAP) {
            bl.removeBlockFromHist(prev_block);
            old_block->size = old_block->size + prev_block->size - size;
            prev_block->size = size;
            prev_block->is_free = false;
            void *newp = (char *) prev_block + sizeof(MMD);
            memcpy(newp, oldp, old_block->size);

            MMD *new_block = (MMD *) ((char *) old_block + size);
            new_block->size = old_block->size;
            new_block->is_free = true;
            new_block->prev = prev_block;
            new_block->next = old_block->next;
            prev_block->next = new_block;
            if (new_block->next){
                new_block->next->prev = new_block;
                if(new_block->next->is_free){
                    bl.mergeBlock(new_block, MergeRight);
                }
            }

            bl.addBlockToHist(new_block);

            return newp;
        } else {
            bl.removeBlockFromHist(prev_block);
            bl.removeBlock(old_block);
            prev_block->size = old_block->size + prev_block->size + sizeof(MMD);
            prev_block->is_free = false;

            void *newp = (char *) prev_block + sizeof(MMD);
            memcpy(newp, oldp, old_size);
            return newp;
        }
    }

    if (next_block_free && size <= old_block->size + old_block->next->size + sizeof(MMD)) {
        MMD *next_block = old_block->next;
        if (old_block->size + next_block->size - size >= MIN_GAP) {
            bl.removeBlockFromHist(next_block);
            MMD *new_block = (MMD *) ((char *) next_block + (size - old_block->size));
            new_block->size = next_block->size - (size - old_block->size);
            new_block->is_free = true;
            old_block->size = size;

            new_block->prev = next_block->prev;
            new_block->next = next_block->next;
            old_block->next = new_block;
            if (new_block->next){
                new_block->next->prev = new_block;
                if(new_block->next->is_free){
                    bl.mergeBlock(new_block, MergeRight);
                }
            }

            bl.addBlockToHist(new_block);
            void *newp = (char *) old_block + sizeof(MMD);
            return newp;
        } else {
            bl.removeBlockFromHist(next_block);
            bl.removeBlock(next_block);
            old_block->size = old_block->size + next_block->size + sizeof(MMD);
            old_block->is_free = false;

            void *newp = (char *) old_block + sizeof(MMD);
            return newp;
        }
    }

    if (prev_block_free && next_block_free &&
        size <= old_block->size + old_block->prev->size + old_block->next->size + 2 * sizeof(MMD)) {

        MMD *prev_block = old_block->prev;
        bl.removeBlockFromHist(prev_block);
        MMD *next_block = old_block->next;
        bl.removeBlockFromHist(next_block);

        if (old_block->size + prev_block->size + next_block->size + sizeof(MMD) - size >= MIN_GAP) {
            bl.removeBlock(old_block);
            size_t total_size = old_block->size + prev_block->size + next_block->size + sizeof(MMD);
            prev_block->size = size;

            MMD *new_block = (MMD *) ((char *) prev_block + size + sizeof(MMD));
            new_block->size = total_size - (size - old_block->size);

            new_block->prev = prev_block;
            new_block->next = next_block->next;
            prev_block->next = new_block;
            if (new_block->next){
                new_block->next->prev = new_block;
                if(new_block->next->is_free){
                    bl.mergeBlock(new_block, MergeRight);
                }
            }

            bl.addBlockToHist(new_block);
            void *newp = (char *) prev_block + sizeof(MMD);
            return newp;
        } else {
            bl.removeBlock(old_block);
            bl.removeBlock(next_block);

            prev_block->size = prev_block->size + old_block->size + prev_block->size + 2 * sizeof(MMD);
            prev_block->is_free = false;

            void *newp = (char *) prev_block + sizeof(MMD);
            memcpy(newp, oldp, old_size);
            return newp;
        }
    }


    if (!old_block->next) {
        if (old_block->prev && old_block->prev->is_free) {
            MMD *prev_block = old_block->prev;
            bl.removeBlockFromHist(prev_block);
            prev_block->is_free = false;
            prev_block->size = prev_block->size + old_block->size + sizeof(MMD);
            memcpy((char *) prev_block + sizeof(MMD), oldp, old_block->size);
            bl.removeBlock(old_block);
            old_block = prev_block;
        }
        sbrk(size - old_block->size);
        old_block->size = size;
        return (char*)old_block + sizeof(MMD);
    }

    // Find block if available

    void *newp = smalloc(size);
    if (newp == NULL)
        return NULL;
    memcpy(newp, oldp, old_size);
    sfree(oldp);
    return newp;
}


size_t _num_free_blocks() {
    return bl.numFreeBlocks();
}

size_t _num_free_bytes() {
    return bl.numFreeBytes();
}

size_t _num_allocated_blocks() {
    return bl.numTotalBlocks();
}

size_t _num_allocated_bytes() {
    return bl.numTotalBytes();
}

size_t _num_meta_data_bytes() {
    return bl.numTotalBlocks() * sizeof(MMD);
}

size_t _size_meta_data() {
    return sizeof(MMD);
}
