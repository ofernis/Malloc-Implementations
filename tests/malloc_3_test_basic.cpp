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

TEST_CASE("Sanity", "[malloc3]")
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
}

TEST_CASE("Check size", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(1);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(aligned_size(1) + _size_meta_data() == (size_t)after - (size_t)base);

    verify_blocks(1, 1, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    after = sbrk(0);
    REQUIRE(aligned_size(24) + _size_meta_data() * 2 == (size_t)after - (size_t)base);

    verify_blocks(2, 24, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(2, 24, 1, 8);
    verify_size(base);
    sfree(b);
    verify_blocks(1, 24 + _size_meta_data(), 1, 24 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("0 size", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(0);
    REQUIRE(a == nullptr);
    void *after = sbrk(0);
    REQUIRE(after == base);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("Max size", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MAX_ALLOCATION_SIZE);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *b = (char *)smalloc(MAX_ALLOCATION_SIZE + 1);
    REQUIRE(b == nullptr);
    verify_blocks(1, MAX_ALLOCATION_SIZE, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size_with_large_blocks(base, 0);
}

TEST_CASE("free", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);

    verify_blocks(3, 16 * 3, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(3, 16 * 3, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(2, 16 * 3 + _size_meta_data(), 1, 16 * 2 + _size_meta_data());
    verify_size(base);
    sfree(c);
    verify_blocks(1, 16 * 3 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    REQUIRE(a == new_a);
    char *new_b = (char *)smalloc(10);
    REQUIRE(b != new_b);
    char *new_c = (char *)smalloc(10);
    REQUIRE(c != new_c);

    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 0, 0);
    verify_size(base);

    sfree(new_a);
    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);
    sfree(new_b);
    verify_blocks(2, 16 * 5 + _size_meta_data() * 3, 1, 16 * 4 + _size_meta_data() * 3);
    verify_size(base);
    sfree(new_c);
    verify_blocks(1, 16 * 5 + _size_meta_data() * 4, 1, 16 * 5 + _size_meta_data() * 4);
    verify_size(base);
}

TEST_CASE("free 2", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);

    verify_blocks(3, 16 * 3, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(3, 16 * 3, 1, 16);
    verify_size(base);
    sfree(a);
    verify_blocks(2, 16 * 3 + _size_meta_data(), 1, 16 * 2 + _size_meta_data());
    verify_size(base);
    sfree(c);
    verify_blocks(1, 16 * 3 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    REQUIRE(a == new_a);
    char *new_b = (char *)smalloc(10);
    REQUIRE(b != new_b);
    char *new_c = (char *)smalloc(10);
    REQUIRE(c != new_c);

    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 0, 0);
    verify_size(base);

    sfree(new_a);
    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);
    sfree(new_b);
    verify_blocks(2, 16 * 5 + _size_meta_data() * 3, 1, 16 * 4 + _size_meta_data() * 3);
    verify_size(base);
    sfree(new_c);
    verify_blocks(1, 16 * 5 + _size_meta_data() * 4, 1, 16 * 5 + _size_meta_data() * 4);
    verify_size(base);
}

TEST_CASE("free 3", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);

    verify_blocks(3, 16 * 3, 0, 0);
    verify_size(base);

    sfree(c);
    verify_blocks(3, 16 * 3, 1, 16);
    verify_size(base);
    sfree(a);
    verify_blocks(3, 16 * 3, 2, 16 * 2);
    verify_size(base);
    sfree(b);
    verify_blocks(1, 16 * 3 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    REQUIRE(a == new_a);
    char *new_b = (char *)smalloc(10);
    REQUIRE(b != new_b);
    char *new_c = (char *)smalloc(10);
    REQUIRE(c != new_c);

    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 0, 0);
    verify_size(base);

    sfree(new_a);
    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);
    sfree(new_b);
    verify_blocks(2, 16 * 5 + _size_meta_data() * 3, 1, 16 * 4 + _size_meta_data() * 3);
    verify_size(base);
    sfree(new_c);
    verify_blocks(1, 16 * 5 + _size_meta_data() * 4, 1, 16 * 5 + _size_meta_data() * 4);
    verify_size(base);
}

TEST_CASE("free holes 1", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    char *d = (char *)smalloc(10);
    REQUIRE(d != nullptr);
    char *e = (char *)smalloc(10);
    REQUIRE(e != nullptr);

    verify_blocks(5, 16 * 5, 0, 0);
    verify_size(base);

    sfree(a);
    verify_blocks(5, 16 * 5, 1, 16);
    verify_size(base);
    sfree(c);
    verify_blocks(5, 16 * 5, 2, 16 * 2);
    verify_size(base);
    sfree(e);
    verify_blocks(5, 16 * 5, 3, 16 * 3);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    REQUIRE(a == new_a);
    char *new_c = (char *)smalloc(10);
    REQUIRE(c == new_c);
    char *new_e = (char *)smalloc(10);
    REQUIRE(e == new_e);

    verify_blocks(5, 16 * 5, 0, 0);
    verify_size(base);

    sfree(new_a);
    verify_blocks(5, 16 * 5, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(4, 16 * 5 + _size_meta_data() * 1, 1, 16 * 2 + _size_meta_data());
    verify_size(base);
    sfree(new_c);
    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);
    sfree(d);
    verify_blocks(2, 16 * 5 + _size_meta_data() * 3, 1, 16 * 4 + _size_meta_data() * 3);
    verify_size(base);
    sfree(new_e);
    verify_blocks(1, 16 * 5 + _size_meta_data() * 4, 1, 16 * 5 + _size_meta_data() * 4);
    verify_size(base);
}

TEST_CASE("free holes 2", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    char *d = (char *)smalloc(10);
    REQUIRE(d != nullptr);
    char *e = (char *)smalloc(10);
    REQUIRE(e != nullptr);

    verify_blocks(5, 16 * 5, 0, 0);
    verify_size(base);

    sfree(c);
    verify_blocks(5, 16 * 5, 1, 16);
    verify_size(base);
    sfree(a);
    verify_blocks(5, 16 * 5, 2, 16 * 2);
    verify_size(base);
    sfree(e);
    verify_blocks(5, 16 * 5, 3, 16 * 3);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    REQUIRE(a == new_a);
    char *new_c = (char *)smalloc(10);
    REQUIRE(c == new_c);
    char *new_e = (char *)smalloc(10);
    REQUIRE(e == new_e);

    verify_blocks(5, 16 * 5, 0, 0);
    verify_size(base);

    sfree(new_a);
    verify_blocks(5, 16 * 5, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(4, 16 * 5 + _size_meta_data() * 1, 1, 16 * 2 + _size_meta_data());
    verify_size(base);
    sfree(new_c);
    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);
    sfree(d);
    verify_blocks(2, 16 * 5 + _size_meta_data() * 3, 1, 16 * 4 + _size_meta_data() * 3);
    verify_size(base);
    sfree(new_e);
    verify_blocks(1, 16 * 5 + _size_meta_data() * 4, 1, 16 * 5 + _size_meta_data() * 4);
    verify_size(base);
}

TEST_CASE("free holes 3", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    char *c = (char *)smalloc(10);
    REQUIRE(c != nullptr);
    char *d = (char *)smalloc(10);
    REQUIRE(d != nullptr);
    char *e = (char *)smalloc(10);
    REQUIRE(e != nullptr);

    verify_blocks(5, 16 * 5, 0, 0);
    verify_size(base);

    sfree(e);
    verify_blocks(5, 16 * 5, 1, 16);
    verify_size(base);
    sfree(a);
    verify_blocks(5, 16 * 5, 2, 16 * 2);
    verify_size(base);
    sfree(c);
    verify_blocks(5, 16 * 5, 3, 16 * 3);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    REQUIRE(a == new_a);
    char *new_c = (char *)smalloc(10);
    REQUIRE(c == new_c);
    char *new_e = (char *)smalloc(10);
    REQUIRE(e == new_e);

    verify_blocks(5, 16 * 5, 0, 0);
    verify_size(base);

    sfree(new_a);
    verify_blocks(5, 16 * 5, 1, 16);
    verify_size(base);
    sfree(b);
    verify_blocks(4, 16 * 5 + _size_meta_data() * 1, 1, 16 * 2 + _size_meta_data());
    verify_size(base);
    sfree(new_c);
    verify_blocks(3, 16 * 5 + _size_meta_data() * 2, 1, 16 * 3 + _size_meta_data() * 2);
    verify_size(base);
    sfree(d);
    verify_blocks(2, 16 * 5 + _size_meta_data() * 3, 1, 16 * 4 + _size_meta_data() * 3);
    verify_size(base);
    sfree(new_e);
    verify_blocks(1, 16 * 5 + _size_meta_data() * 4, 1, 16 * 5 + _size_meta_data() * 4);
    verify_size(base);
}

TEST_CASE("Wilderness available", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    char *wilderness = (char *)smalloc(16);
    REQUIRE(wilderness != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    sfree(wilderness);
    verify_blocks(1, 16, 1, 16);
    verify_size(base);

    char *bigger1 = (char *)smalloc(32);
    REQUIRE(bigger1 != nullptr);
    REQUIRE(bigger1 == wilderness);
    verify_blocks(1, 32, 0, 0);
    verify_size(base);

    sfree(bigger1);
    verify_blocks(1, 32, 1, 32);
    verify_size(base);

    char *bigger2 = (char *)smalloc(104);
    REQUIRE(bigger2 != nullptr);
    REQUIRE(bigger2 == wilderness);
    verify_blocks(1, 104, 0, 0);
    verify_size(base);

    sfree(bigger2);
    verify_blocks(1, 104, 1, 104);
    verify_size(base);
}

TEST_CASE("Wilderness available pad", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    char *pad = (char *)smalloc(16);
    REQUIRE(pad != nullptr);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);

    char *wilderness = (char *)smalloc(16);
    REQUIRE(wilderness != nullptr);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);

    sfree(wilderness);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);

    char *bigger1 = (char *)smalloc(32);
    REQUIRE(bigger1 != nullptr);
    REQUIRE(bigger1 == wilderness);
    verify_blocks(2, 48, 0, 0);
    verify_size(base);

    sfree(bigger1);
    verify_blocks(2, 48, 1, 32);
    verify_size(base);

    char *bigger2 = (char *)smalloc(104);
    REQUIRE(bigger2 != nullptr);
    REQUIRE(bigger2 == wilderness);
    verify_blocks(2, 120, 0, 0);
    verify_size(base);

    sfree(bigger2);
    verify_blocks(2, 120, 1, 104);
    verify_size(base);

    sfree(pad);
    verify_blocks(1, 120 + _size_meta_data(), 1, 120 + _size_meta_data());
    verify_size(base);
}

TEST_CASE("Large allocation", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MMAP_THRESHOLD);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *b = (char *)smalloc(MMAP_THRESHOLD - 8);
    REQUIRE(b != nullptr);
    verify_blocks(2, 2 * MMAP_THRESHOLD - 8, 0, 0);
    verify_size_with_large_blocks(base, MMAP_THRESHOLD - 8 + _size_meta_data());

    sfree(a);
    verify_blocks(1, MMAP_THRESHOLD - 8, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, MMAP_THRESHOLD - 8, 1, MMAP_THRESHOLD - 8);
    verify_size(base);
}

TEST_CASE("Large unaligned allocation", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MMAP_THRESHOLD + 1);
    REQUIRE(a != nullptr);
    void *after = sbrk(0);
    REQUIRE(0 == (size_t)after - (size_t)base);
    verify_blocks(1, MMAP_THRESHOLD + 8, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("Alignment", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    REQUIRE((size_t)a % 8 == 0);
    verify_blocks(1, 16, 0, 0);
    verify_size(base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE((size_t)b % 8 == 0);
    verify_blocks(2, 32, 0, 0);
    verify_size(base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    sfree(a);
    verify_blocks(2, 32, 1, 16);
    verify_size(base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    sfree(b);
    verify_blocks(1, 32 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);
}

TEST_CASE("Alignment unaligned base", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    // Should be on all linux systems
    REQUIRE(((size_t)base) % 8 == 0);
    sbrk(3);
    base = sbrk(0);
    void *aligned_base = (void *)((size_t)sbrk(0) + 5);
    REQUIRE(((size_t)base) % 8 != 0);

    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    char *a = (char *)smalloc(10);
    REQUIRE(a != nullptr);
    REQUIRE((size_t)a % 8 == 0);
    verify_blocks(1, 16, 0, 0);
    verify_size(aligned_base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    char *b = (char *)smalloc(10);
    REQUIRE(b != nullptr);
    REQUIRE((size_t)b % 8 == 0);
    verify_blocks(2, 32, 0, 0);
    verify_size(aligned_base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    sfree(a);
    verify_blocks(2, 32, 1, 16);
    verify_size(aligned_base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    sfree(b);
    verify_blocks(1, 32 + _size_meta_data(), 1, 32 + _size_meta_data());
    verify_size(aligned_base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);
}

TEST_CASE("Alignment MMAP", "[malloc3]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);

    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    char *a = (char *)smalloc(MMAP_THRESHOLD);
    REQUIRE(a != nullptr);
    REQUIRE((size_t)a % 8 == 0);
    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
    REQUIRE(_size_meta_data() % 8 == 0);
    REQUIRE(_num_allocated_bytes() % 8 == 0);
    REQUIRE(_num_free_bytes() % 8 == 0);
}
