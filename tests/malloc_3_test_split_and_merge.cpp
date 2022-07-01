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

TEST_CASE("Split block", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(16 + MIN_SPLIT_SIZE * 2 + _size_meta_data());
    REQUIRE(a != nullptr);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE * 2 + _size_meta_data(), 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE * 2 + _size_meta_data(), 1, 16 + MIN_SPLIT_SIZE * 2 + _size_meta_data());
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    verify_blocks(2, 16 + MIN_SPLIT_SIZE * 2, 1, MIN_SPLIT_SIZE * 2);
    verify_size(base);

    sfree(b);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE * 2 + _size_meta_data(), 1, 16 + MIN_SPLIT_SIZE * 2 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Split block under threshold", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(16 + MIN_SPLIT_SIZE + _size_meta_data() - 8);
    REQUIRE(a != nullptr);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data() - 8, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data() - 8, 1, 16 + MIN_SPLIT_SIZE + _size_meta_data() - 8);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data() - 8, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data() - 8, 1, 16 + MIN_SPLIT_SIZE + _size_meta_data() - 8);
    verify_size(base);
}

TEST_CASE("Split block threshold", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(16 + MIN_SPLIT_SIZE + _size_meta_data());
    REQUIRE(a != nullptr);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data(), 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data(), 1, 16 + MIN_SPLIT_SIZE + _size_meta_data());
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    verify_blocks(2, 16 + MIN_SPLIT_SIZE, 1, MIN_SPLIT_SIZE);
    verify_size(base);

    sfree(b);
    verify_blocks(1, 16 + MIN_SPLIT_SIZE + _size_meta_data(), 1, 16 + MIN_SPLIT_SIZE + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Merge with prev block", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(16);
    REQUIRE(a != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);

    sfree(b);
    verify_blocks(1, 32 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Merge with prev block pad", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    char *pad1 = (char *)smalloc(16);
    REQUIRE(pad1 != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *a = (char *)smalloc(16);
    REQUIRE(a != nullptr);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
    verify_blocks(3, 48, 0, 0);
    verify_size(base);

    char *pad2 = (char *)smalloc(16);
    REQUIRE(pad2 != nullptr);
    verify_blocks(4, 64, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(4, 64, 1, 16);
    verify_size(base);

    sfree(b);
    verify_blocks(3, 64 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(2, 64 + 2 * _size_meta_data(), 1, 48 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad2);
    verify_blocks(1, 64 + 3 * _size_meta_data(), 1, 64 + 3 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("Merge with next block", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(16);
    REQUIRE(a != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);

    sfree(a);
    verify_blocks(1, 32 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Merge with next block pad", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    char *pad1 = (char *)smalloc(16);
    REQUIRE(pad1 != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *a = (char *)smalloc(16);
    REQUIRE(a != nullptr);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
    verify_blocks(3, 48, 0, 0);
    verify_size(base);

    char *pad2 = (char *)smalloc(16);
    REQUIRE(pad2 != nullptr);
    verify_blocks(4, 64, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(4, 64, 1, 16);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 64 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(2, 64 + 2 * _size_meta_data(), 1, 48 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad2);
    verify_blocks(1, 64 + 3 * _size_meta_data(), 1, 64 + 3 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("Merge with both blocks", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(16);
    REQUIRE(a != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *c = (char *)smalloc(16);
    REQUIRE(c != nullptr);
    REQUIRE(c != a);
    REQUIRE(c != b);
    verify_blocks(3, 48, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 48, 1, 16);
    verify_size(base);

    sfree(c);
    verify_blocks(3, 48, 2, 32);
    verify_size(base);

    sfree(b);
    verify_blocks(1, 48 + 2 * _size_meta_data(), 1, 48 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("Merge with both blocks pad", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    char *pad1 = (char *)smalloc(16);
    REQUIRE(pad1 != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *a = (char *)smalloc(16);
    REQUIRE(a != nullptr);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
    verify_blocks(3, 48, 0, 0);
    verify_size(base);

    char *c = (char *)smalloc(16);
    REQUIRE(c != nullptr);
    REQUIRE(c != a);
    REQUIRE(c != b);
    verify_blocks(4, 64, 0, 0);
    verify_size(base);

    char *pad2 = (char *)smalloc(16);
    REQUIRE(pad2 != nullptr);
    verify_blocks(5, 80, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(5, 80, 1, 16);
    verify_size(base);

    sfree(c);
    verify_blocks(5, 80, 2, 32);
    verify_size(base);

    sfree(b);
    verify_blocks(3, 80 + 2 * _size_meta_data(), 1, 48 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(2, 80 + 3 * _size_meta_data(), 1, 64 + 3 * _size_meta_data());
    verify_size(base);

    sfree(pad2);
    verify_blocks(1, 80 + 4 * _size_meta_data(), 1, 80 + 4 * _size_meta_data());
    verify_size(base);
}
