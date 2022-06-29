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

TEST_CASE("Reuse", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);

    verify_blocks(1, 10, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(1, 10, 1, 10);
    verify_size(base);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);

    verify_blocks(1, 10, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, 10, 1, 10);
    verify_size(base);
}

TEST_CASE("Reuse two blocks", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);

    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);

    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    REQUIRE(c == a);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);
    sfree(c);
    verify_blocks(1, 32 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Reuse two blocks reverse", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);

    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);

    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    REQUIRE(c == b);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(c);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);
    sfree(a);
    verify_blocks(1, 32 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Reuse two blocks both", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);

    verify_blocks(1, 10, 0, 0);
    verify_size(base);

    char *padding = (char *)smalloc(10);
    REQUIRE(padding != nullptr);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);

    verify_blocks(3, 48, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 48, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(3, 48, 2, 32);
    verify_size(base);

    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    REQUIRE(c == a);

    verify_blocks(3, 48, 1, 16);
    verify_size(base);

    sfree(c);
    verify_blocks(3, 48, 2, 32);
    verify_size(base);

    sfree(padding);
    verify_blocks(1, 48 + 2 * _size_meta_data(), 1, 48 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("Reuse two blocks sizes small", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);

    verify_blocks(1, 10, 0, 0);
    verify_size(base);

    char *padding = (char *)smalloc(10);
    REQUIRE(padding != nullptr);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(100);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);

    verify_blocks(3, 136, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 136, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(3, 136, 2, 120);
    verify_size(base);

    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    REQUIRE(c == a);

    verify_blocks(3, 136, 1, 104);
    verify_size(base);

    sfree(c);
    verify_blocks(3, 136, 2, 120);
    verify_size(base);

    sfree(padding);
    verify_blocks(1, 136 + 2 * _size_meta_data(), 1, 136 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("Reuse two blocks sizes small reversed", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(100);
    REQUIRE(a != nullptr);

    verify_blocks(1, 104, 0, 0);
    verify_size(base);

    char *padding = (char *)smalloc(10);
    REQUIRE(padding != nullptr);

    verify_blocks(2, 120, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);

    verify_blocks(3, 136, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 136, 1, 104);
    verify_size(base);
    sfree(b);
    verify_blocks(3, 136, 2, 120);
    verify_size(base);

    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    REQUIRE(c == b);

    verify_blocks(3, 136, 1, 104);
    verify_size(base);

    sfree(c);
    verify_blocks(3, 136, 2, 120);
    verify_size(base);

    sfree(padding);
    verify_blocks(1, 136 + 2 * _size_meta_data(), 1, 136 + 2 * _size_meta_data());
    verify_size(base);
}

TEST_CASE("Reuse two blocks sizes large", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);

    verify_blocks(1, 10, 0, 0);
    verify_size(base);

    char *padding = (char *)smalloc(10);
    REQUIRE(padding != nullptr);

    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(100);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);

    verify_blocks(3, 136, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 136, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(3, 136, 2, 120);
    verify_size(base);

    char *c = (char *)smalloc(100);
    REQUIRE(c != nullptr);
    REQUIRE(c == b);

    verify_blocks(3, 136, 1, 16);
    verify_size(base);

    sfree(c);
    verify_blocks(3, 136, 2, 120);
    verify_size(base);

    sfree(padding);
    verify_blocks(1, 136 + 2 * _size_meta_data(), 1, 136 + 2 * _size_meta_data());
    verify_size(base);
}
