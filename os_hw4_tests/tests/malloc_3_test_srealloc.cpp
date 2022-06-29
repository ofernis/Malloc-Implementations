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

TEST_CASE("realloc", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    int *a = (int *)srealloc(nullptr, 10 * sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; i++)
    {
        a[i] = i;
    }

    verify_blocks(1, aligned_size(10 * sizeof(int)), 0, 0);
    verify_size(base);

    int *b = (int *)srealloc(a, aligned_size(100 * sizeof(int)));
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    for (int i = 0; i < 10; i++)
    {
        REQUIRE(b[i] == i);
    }

    verify_blocks(1, aligned_size(100 * sizeof(int)), 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, aligned_size(100 * sizeof(int)), 1, aligned_size(100 * sizeof(int)));
    verify_size(base);
}

TEST_CASE("realloc shrink", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    int *a = (int *)srealloc(nullptr, 30 * sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; i++)
    {
        a[i] = i;
    }

    verify_blocks(1, aligned_size(30 * sizeof(int)), 0, 0);
    verify_size(base);

    int *b = (int *)srealloc(a, 10 * sizeof(int));
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    for (int i = 0; i < 10; i++)
    {
        REQUIRE(b[i] == i);
    }

    verify_blocks(1, aligned_size(30 * sizeof(int)), 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, aligned_size(30 * sizeof(int)), 1, aligned_size(30 * sizeof(int)));
    verify_size(base);
}

TEST_CASE("srealloc Max size", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)srealloc(nullptr, MAX_ALLOCATION_SIZE);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *b = (char *)srealloc(a, MAX_ALLOCATION_SIZE + 1);
    REQUIRE(b == nullptr);
    after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *c = (char *)srealloc(nullptr, MAX_ALLOCATION_SIZE + 1);
    REQUIRE(c == nullptr);
    after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}
