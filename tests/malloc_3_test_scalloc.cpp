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

TEST_CASE("scalloc", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)scalloc(10, 1);
    REQUIRE(a != nullptr);
    for (size_t i = 0; i < 10; i++)
    {
        REQUIRE(a[i] == 0);
    }

    verify_blocks(1, 10, 0, 0);
    verify_size(base);

    int *b = (int *)scalloc(100, sizeof(int));
    REQUIRE(b != nullptr);
    for (size_t i = 0; i < 10; i++)
    {
        REQUIRE(b[i] == 0);
    }

    verify_blocks(2, sizeof(int) * 100 + 10, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(2, sizeof(int) * 100 + 10, 1, 10);
    verify_size(base);
    sfree(b);
    verify_blocks(1, sizeof(int) * 100 + 10 + _size_meta_data(), 1, sizeof(int) * 100 + 10 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("scalloc taint", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    int *a = (int *)scalloc(100, sizeof(int));
    REQUIRE(a != nullptr);
    for (size_t i = 0; i < 100; i++)
    {
        REQUIRE(a[i] == 0);
    }

    verify_blocks(1, sizeof(int) * 100, 0, 0);
    verify_size(base);

    for (size_t i = 0; i < 100; i++)
    {
        a[i] = i;
    }

    sfree(a);
    verify_blocks(1, sizeof(int) * 100, 1, sizeof(int) * 100);
    verify_size(base);

    int *b = (int *)scalloc(100, sizeof(int));
    REQUIRE(b != nullptr);
    REQUIRE(a == b);
    for (size_t i = 0; i < 100; i++)
    {
        REQUIRE(b[i] == 0);
    }

    verify_blocks(1, sizeof(int) * 100, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, sizeof(int) * 100, 1, sizeof(int) * 100);
    verify_size(base);
}

TEST_CASE("scalloc 0 size", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)scalloc(0, 10);
    REQUIRE(a == nullptr);
    void *after = sbrk(0);
    REQUIRE(after == base);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
    a = (char *)scalloc(10, 0);
    REQUIRE(a == nullptr);
    after = sbrk(0);
    REQUIRE(after == base);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("scalloc Max size num", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)scalloc(MAX_ALLOCATION_SIZE, 1);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *b = (char *)scalloc(MAX_ALLOCATION_SIZE + 1, 1);
    REQUIRE(b == nullptr);
    after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size_with_large_blocks(base, 0);
}

TEST_CASE("scalloc Max size size", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)scalloc(1, MAX_ALLOCATION_SIZE);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *b = (char *)scalloc(1, MAX_ALLOCATION_SIZE + 1);
    REQUIRE(b == nullptr);
    after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size_with_large_blocks(base, 0);
}

TEST_CASE("scalloc Max size both", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)scalloc(MAX_ALLOCATION_SIZE / 8, 8);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *b = (char *)scalloc(MAX_ALLOCATION_SIZE / 8, 9);
    REQUIRE(b == nullptr);
    after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);
    b = (char *)scalloc(9, MAX_ALLOCATION_SIZE / 8);
    REQUIRE(b == nullptr);
    after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size_with_large_blocks(base, 0);
}
