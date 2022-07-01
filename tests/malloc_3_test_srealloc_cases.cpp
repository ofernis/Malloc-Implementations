#include "my_stdlib.h"
#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#define MAX_ALLOCATION_SIZE (1e8)
#define MMAP_THRESHOLD (128 * 1024)
#define MIN_SPLIT_SIZE (128)

static inline size_t aligned_size(size_t size)
{
    return (size % 8) ? (size & (size_t)(-8)) + 8 : size;
}

#define verify_blocks(allocated_blocks, allocated_bytes, free_blocks, free_bytes)                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        REQUIRE(_num_allocated_blocks() == allocated_blocks);                                                          \
        REQUIRE(_num_allocated_bytes() == aligned_size(allocated_bytes));                                              \
        REQUIRE(_num_free_blocks() == free_blocks);                                                                    \
        REQUIRE(_num_free_bytes() == aligned_size(free_bytes));                                                        \
        REQUIRE(_num_meta_data_bytes() == aligned_size(_size_meta_data() * allocated_blocks));                         \
    } while (0)

#define verify_size(base)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        void *after = sbrk(0);                                                                                         \
        REQUIRE(_num_allocated_bytes() + aligned_size(_size_meta_data() * _num_allocated_blocks()) ==                  \
                (size_t)after - (size_t)base);                                                                         \
    } while (0)

#define verify_size_with_large_blocks(base, diff)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        void *after = sbrk(0);                                                                                         \
        REQUIRE(diff == (size_t)after - (size_t)base);                                                                 \
    } while (0)

template <typename T>
void populate_array(T *array, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        array[i] = (T)i;
    }
}

template <typename T>
void validate_array(T *array, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        REQUIRE((array[i] == (T)i));
    }
}

TEST_CASE("srealloc case a", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    REQUIRE(a != nullptr);

    verify_blocks(1, 32, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(1, 32, 1, 32);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    verify_blocks(1, 32, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    char *c = (char *)srealloc(b, 32);
    REQUIRE(c != nullptr);
    REQUIRE(c == b);
    verify_blocks(1, 32, 0, 0);
    verify_size(base);
    validate_array(c, 32);

    char *d = (char *)srealloc(c, 32);
    REQUIRE(c != nullptr);
    REQUIRE(c == d);
    verify_blocks(1, 32, 0, 0);
    verify_size(base);
    validate_array(c, 32);

    sfree(c);
    verify_blocks(1, 32, 1, 32);
    verify_size(base);
}

TEST_CASE("srealloc case a split", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32 + MIN_SPLIT_SIZE + _size_meta_data());
    REQUIRE(a != nullptr);

    verify_blocks(1, 32 + MIN_SPLIT_SIZE + _size_meta_data(), 0, 0);
    verify_size(base);
    populate_array(a, 32 + MIN_SPLIT_SIZE + _size_meta_data());

    char *b = (char *)srealloc(a, 32);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    verify_blocks(2, 32 + MIN_SPLIT_SIZE, 1, MIN_SPLIT_SIZE);
    verify_size(base);
    validate_array(b, 32);

    sfree(b);
    verify_blocks(1, 32 + MIN_SPLIT_SIZE + _size_meta_data(), 1, 32 + MIN_SPLIT_SIZE + _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case a mmap", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MMAP_THRESHOLD);
    REQUIRE(a != nullptr);

    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    populate_array(a, MMAP_THRESHOLD);

    char *b = (char *)srealloc(a, MMAP_THRESHOLD);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_array(b, MMAP_THRESHOLD);

    char *c = (char *)srealloc(b, 32);
    REQUIRE(c != nullptr);
    REQUIRE(c != b);
    verify_blocks(1, 32, 0, 0);
    verify_size(base);
    validate_array(c, 32);

    sfree(c);
    verify_blocks(1, 32, 1, 32);
    verify_size(base);
}

TEST_CASE("srealloc case b", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    verify_blocks(3, 32 * 3, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    sfree(c);
    verify_blocks(3, 32 * 3, 2, 32 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 1, 32);
    verify_size(base);
    validate_array(new_b, 32);

    char *new_b2 = (char *)srealloc(new_b, 64 + _size_meta_data());
    REQUIRE(new_b2 != nullptr);
    REQUIRE(new_b2 == new_b);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 1, 32);
    verify_size(base);
    validate_array(new_b2, 32);

    sfree(new_b2);
    verify_blocks(1, 32 * 3 + 2 * _size_meta_data(), 1, 32 * 3 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case b metadata", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    verify_blocks(3, 32 * 3, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    sfree(c);
    verify_blocks(3, 32 * 3, 2, 32 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64 + _size_meta_data());
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 1, 32);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(1, 32 * 3 + 2 * _size_meta_data(), 1, 32 * 3 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case b split", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MIN_SPLIT_SIZE + 32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    verify_blocks(3, MIN_SPLIT_SIZE + 32 * 3, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    sfree(c);
    verify_blocks(3, MIN_SPLIT_SIZE + 32 * 3, 2, MIN_SPLIT_SIZE + 32 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(2, MIN_SPLIT_SIZE + 32 * 3 + _size_meta_data(), 1, MIN_SPLIT_SIZE + 32 + _size_meta_data());
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(1, MIN_SPLIT_SIZE + 32 * 3 + 2 * _size_meta_data(), 1,
                  MIN_SPLIT_SIZE + 32 * 3 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case b wilderness", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    verify_blocks(2, 32 * 2, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    verify_blocks(2, 32 * 2, 1, 32);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 32 * 3 + _size_meta_data());
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(1, 32 * 3 + _size_meta_data(), 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(1, 32 * 3 + _size_meta_data(), 1, 32 * 3 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case b wilderness 2", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(64);
    char *b = (char *)smalloc(64);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    verify_blocks(2, 64 * 2, 0, 0);
    verify_size(base);
    populate_array(b, 64);

    sfree(a);
    verify_blocks(2, 64 * 2, 1, 64);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64 * 3);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(1, 64 * 3, 0, 0);
    verify_size(base);
    validate_array(new_b, 64);

    sfree(new_b);
    verify_blocks(1, 64 * 3, 1, 64 * 3);
    verify_size(base);
}

TEST_CASE("srealloc case c", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(64);
    REQUIRE(a != nullptr);

    verify_blocks(1, 64, 0, 0);
    verify_size(base);
    populate_array(a, 64);

    char *new_a = (char *)srealloc(a, 64 * 3);
    REQUIRE(new_a != nullptr);
    REQUIRE(new_a == a);
    verify_blocks(1, 64 * 3, 0, 0);
    verify_size(base);
    validate_array(new_a, 64);

    sfree(new_a);
    verify_blocks(1, 64 * 3, 1, 64 * 3);
    verify_size(base);
}

TEST_CASE("srealloc case c 2", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(64);
    char *b = (char *)smalloc(64);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    verify_blocks(2, 64 * 2, 0, 0);
    verify_size(base);
    populate_array(b, 64);

    char *new_b = (char *)srealloc(b, 64 * 3);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == b);
    verify_blocks(2, 64 * 4, 0, 0);
    verify_size(base);
    validate_array(new_b, 64);

    sfree(a);
    verify_blocks(2, 64 * 4, 1, 64);
    verify_size(base);

    sfree(new_b);
    verify_blocks(1, 64 * 4 + _size_meta_data(), 1, 64 * 4 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case d", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    verify_blocks(3, 32 * 3, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(c);
    verify_blocks(3, 32 * 3, 1, 32);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == b);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    char *new_b2 = (char *)srealloc(new_b, 64 + _size_meta_data());
    REQUIRE(new_b2 != nullptr);
    REQUIRE(new_b2 == new_b);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 0, 0);
    verify_size(base);
    validate_array(new_b2, 32);

    sfree(new_b2);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 1, 32 * 2 + _size_meta_data());
    verify_size(base);

    sfree(a);
    verify_blocks(1, 32 * 3 + 2 * _size_meta_data(), 1, 32 * 3 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case d metadata", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    verify_blocks(3, 32 * 3, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(c);
    verify_blocks(3, 32 * 3, 1, 32);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64 + _size_meta_data());
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == b);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(2, 32 * 3 + _size_meta_data(), 1, 32 * 2 + _size_meta_data());
    verify_size(base);

    sfree(a);
    verify_blocks(1, 32 * 3 + 2 * _size_meta_data(), 1, 32 * 3 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case d split", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(MIN_SPLIT_SIZE + 32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    verify_blocks(3, MIN_SPLIT_SIZE + 32 * 3, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(c);
    verify_blocks(3, MIN_SPLIT_SIZE + 32 * 3, 1, MIN_SPLIT_SIZE + 32);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 64);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == b);
    verify_blocks(3, MIN_SPLIT_SIZE + 32 * 3, 1, MIN_SPLIT_SIZE);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(2, MIN_SPLIT_SIZE + 32 * 3 + _size_meta_data(), 1, MIN_SPLIT_SIZE + 32 * 2 + _size_meta_data());
    verify_size(base);

    sfree(a);
    verify_blocks(1, MIN_SPLIT_SIZE + 32 * 3 + 2 * _size_meta_data(), 1,
                  MIN_SPLIT_SIZE + 32 * 3 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case e", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    char *pad2 = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);
    REQUIRE(pad2 != nullptr);

    size_t pad_size = 64;

    verify_blocks(5, 32 * 3 + pad_size, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    sfree(c);
    verify_blocks(5, 32 * 3 + pad_size, 2, 32 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 32 * 3 + _size_meta_data() * 2);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(3, 32 * 3 + 2 * _size_meta_data() + pad_size, 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(3, 32 * 3 + 2 * _size_meta_data() + pad_size, 1, 32 * 3 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    sfree(pad2);
    verify_blocks(1, 32 * 3 + 4 * _size_meta_data() + pad_size, 1, 32 * 3 + 4 * _size_meta_data() + pad_size);
    verify_size(base);
}

TEST_CASE("srealloc case e no pad", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    size_t pad_size = 32;

    verify_blocks(4, 32 * 3 + pad_size, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    sfree(c);
    verify_blocks(4, 32 * 3 + pad_size, 2, 32 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 32 * 3 + _size_meta_data() * 2);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(2, 32 * 3 + 2 * _size_meta_data() + pad_size, 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(2, 32 * 3 + 2 * _size_meta_data() + pad_size, 1, 32 * 3 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(1, 32 * 3 + 3 * _size_meta_data() + pad_size, 1, 32 * 3 + 3 * _size_meta_data() + pad_size);
    verify_size(base);
}

TEST_CASE("srealloc case e split", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(304);
    char *b = (char *)smalloc(104);
    char *c = (char *)smalloc(304);
    char *pad2 = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);
    REQUIRE(pad2 != nullptr);

    size_t pad_size = 64;
    size_t blocks_size = 304 * 2 + 104;

    verify_blocks(5, blocks_size + pad_size, 0, 0);
    verify_size(base);
    populate_array(b, 104);

    sfree(a);
    sfree(c);
    verify_blocks(5, blocks_size + pad_size, 2, 304 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 512);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(4, blocks_size + _size_meta_data() + pad_size, 1, blocks_size - 512 + _size_meta_data());
    verify_size(base);
    validate_array(new_b, 104);

    sfree(new_b);
    verify_blocks(3, blocks_size + 2 * _size_meta_data() + pad_size, 1, blocks_size + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    sfree(pad2);
    verify_blocks(1, blocks_size + 4 * _size_meta_data() + pad_size, 1, blocks_size + 4 * _size_meta_data() + pad_size);
    verify_size(base);
}

TEST_CASE("srealloc case fi", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    size_t pad_size = 32;

    verify_blocks(4, 32 * 3 + pad_size, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(a);
    sfree(c);
    verify_blocks(4, 32 * 3 + pad_size, 2, 32 * 2);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 32 * 4 + _size_meta_data() * 2);
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == a);
    verify_blocks(2, 32 * 4 + 2 * _size_meta_data() + pad_size, 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(2, 32 * 4 + 2 * _size_meta_data() + pad_size, 1, 32 * 4 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(1, 32 * 4 + 3 * _size_meta_data() + pad_size, 1, 32 * 4 + 3 * _size_meta_data() + pad_size);
    verify_size(base);
}

TEST_CASE("srealloc case fii", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    size_t pad_size = 32;

    verify_blocks(4, 32 * 3 + pad_size, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(c);
    verify_blocks(4, 32 * 3 + pad_size, 1, 32);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 32 * 3 + _size_meta_data());
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == b);
    verify_blocks(3, 32 * 4 + _size_meta_data() + pad_size, 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(3, 32 * 4 + _size_meta_data() + pad_size, 1, 32 * 3 + _size_meta_data());
    verify_size(base);

    sfree(a);
    verify_blocks(2, 32 * 4 + 2 * _size_meta_data() + pad_size, 1, 32 * 4 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(1, 32 * 4 + 3 * _size_meta_data() + pad_size, 1, 32 * 4 + 3 * _size_meta_data() + pad_size);
    verify_size(base);
}

TEST_CASE("srealloc case g", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *pad2 = (char *)smalloc(32);
    char *b = (char *)smalloc(160);
    char *pad3 = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(pad2 != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(pad3 != nullptr);

    size_t pad_size = 32 * 3;
    size_t blocks_size = 32 + 160;

    verify_blocks(5, blocks_size + pad_size, 0, 0);
    verify_size(base);
    populate_array(a, 32);

    sfree(b);
    verify_blocks(5, blocks_size + pad_size, 1, 160);
    verify_size(base);

    char *new_a = (char *)srealloc(a, 160);
    REQUIRE(new_a != nullptr);
    REQUIRE(new_a == b);
    verify_blocks(5, blocks_size + pad_size, 1, 32);
    verify_size(base);
    validate_array(new_a, 32);

    sfree(new_a);
    verify_blocks(5, blocks_size + pad_size, 2, blocks_size);
    verify_size(base);

    sfree(pad1);
    sfree(pad2);
    sfree(pad3);
    verify_blocks(1, blocks_size + pad_size + 4 * _size_meta_data(), 1, blocks_size + pad_size + 4 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("srealloc case h", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *pad2 = (char *)smalloc(32);
    char *b = (char *)smalloc(160);
    char *pad3 = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(pad2 != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(pad3 != nullptr);

    size_t pad_size = 32 * 3;
    size_t blocks_size = 32 + 160;

    verify_blocks(5, blocks_size + pad_size, 0, 0);
    verify_size(base);
    populate_array(a, 32);

    sfree(b);
    verify_blocks(5, blocks_size + pad_size, 1, 160);
    verify_size(base);

    char *new_a = (char *)srealloc(a, 320);
    REQUIRE(new_a != nullptr);
    REQUIRE(new_a != a);
    REQUIRE(new_a != b);
    blocks_size += 320;
    verify_blocks(6, blocks_size + pad_size, 2, 32 + 160);
    verify_size(base);
    validate_array(new_a, 32);

    sfree(new_a);
    verify_blocks(6, blocks_size + pad_size, 3, blocks_size);
    verify_size(base);

    sfree(pad1);
    sfree(pad2);
    sfree(pad3);
    verify_blocks(1, blocks_size + pad_size + 5 * _size_meta_data(), 1, blocks_size + pad_size + 5 * _size_meta_data());
    verify_size(base);
}
