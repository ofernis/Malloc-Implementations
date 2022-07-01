#include "my_stdlib.h"
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <unistd.h>

#define MAX_ALLOCATION_SIZE (1e8)
#define MMAP_THRESHOLD (128 * 1024)
#define DEFAULT_MMAP_THRESHOLD_MAX (4 * 1024 * 1024 * sizeof(long))
#define SMALLOC_HUGE_PAGE_THRESHOLD (1000 * 1000 * 4)
#define SCALLOC_HUGE_PAGE_THRESHOLD (1000 * 1000 * 2)

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

long long get_huge_pages_amount()
{
    std::ifstream meminfo("/proc/meminfo");
    REQUIRE(meminfo.is_open());
    std::string line;
    long long total = -1;
    long long free = -1;
    while (getline(meminfo, line))
    {
        if (line.find("HugePages_Total") != std::string::npos)
        {
            std::string sub = line.substr(line.find(":") + 1);
            total = std::atoll(sub.c_str());
        }
        if (line.find("HugePages_Free") != std::string::npos)
        {
            std::string sub = line.substr(line.find(":") + 1);
            free = std::atoll(sub.c_str());
        }
    }
    REQUIRE(total != -1);
    REQUIRE(free != -1);
    return total - free;
}

#define validate_huge_pages_amount(base, amount)                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        REQUIRE(base + amount == get_huge_pages_amount());                                                             \
    } while (0)

TEST_CASE("Huge pages smalloc", "[malloc4]")
{
    void *base = sbrk(0);
    long long huge_pages_base = get_huge_pages_amount();

    char *a = (char *)smalloc(SMALLOC_HUGE_PAGE_THRESHOLD);
    REQUIRE(a != nullptr);
    verify_blocks(1, SMALLOC_HUGE_PAGE_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 1);

    char *b = (char *)smalloc(SMALLOC_HUGE_PAGE_THRESHOLD - 8);
    REQUIRE(b != nullptr);
    verify_blocks(2, 2 * SMALLOC_HUGE_PAGE_THRESHOLD - 8, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 1);

    sfree(a);
    verify_blocks(1, SMALLOC_HUGE_PAGE_THRESHOLD - 8, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(b);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("Huge pages smalloc realloc", "[malloc4]")
{
    void *base = sbrk(0);
    long long huge_pages_base = get_huge_pages_amount();

    char *a = (char *)smalloc(MMAP_THRESHOLD);
    REQUIRE(a != nullptr);
    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 0);

    char *big_a = (char *)srealloc(a, SCALLOC_HUGE_PAGE_THRESHOLD);
    REQUIRE(big_a != nullptr);
    verify_blocks(1, SCALLOC_HUGE_PAGE_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);

    char *huge_a = (char *)srealloc(big_a, SMALLOC_HUGE_PAGE_THRESHOLD);
    REQUIRE(huge_a != nullptr);
    verify_blocks(1, SMALLOC_HUGE_PAGE_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 1);

    sfree(huge_a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("Huge pages scalloc", "[malloc4]")
{
    void *base = sbrk(0);
    long long huge_pages_base = get_huge_pages_amount();

    char *a = (char *)scalloc(1, SCALLOC_HUGE_PAGE_THRESHOLD);
    REQUIRE(a != nullptr);
    verify_blocks(1, SCALLOC_HUGE_PAGE_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 1);

    char *b = (char *)scalloc(1, SCALLOC_HUGE_PAGE_THRESHOLD - 8);
    REQUIRE(b != nullptr);
    verify_blocks(2, 2 * SCALLOC_HUGE_PAGE_THRESHOLD - 8, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 1);

    sfree(a);
    verify_blocks(1, SCALLOC_HUGE_PAGE_THRESHOLD - 8, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(b);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("Huge pages scalloc realloc", "[malloc4]")
{
    void *base = sbrk(0);
    long long huge_pages_base = get_huge_pages_amount();

    char *a = (char *)scalloc(MMAP_THRESHOLD, 1);
    REQUIRE(a != nullptr);
    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 0);

    char *huge_a = (char *)srealloc(a, SCALLOC_HUGE_PAGE_THRESHOLD);
    REQUIRE(huge_a != nullptr);
    verify_blocks(1, SCALLOC_HUGE_PAGE_THRESHOLD, 0, 0);
    verify_size_with_large_blocks(base, 0);
    validate_huge_pages_amount(huge_pages_base, 1);

    sfree(huge_a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);
}

TEST_CASE("Dynamic mmap", "[malloc4]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MMAP_THRESHOLD + 8);
    REQUIRE(a != nullptr);
    verify_blocks(1, MMAP_THRESHOLD + 8, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(MMAP_THRESHOLD);
    REQUIRE(b != nullptr);
    verify_blocks(1, MMAP_THRESHOLD, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, MMAP_THRESHOLD, 1, MMAP_THRESHOLD);
    verify_size(base);
}

TEST_CASE("Dynamic mmap 2", "[malloc4]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(MMAP_THRESHOLD + 8 * 4);
    REQUIRE(a != nullptr);
    verify_blocks(1, MMAP_THRESHOLD + 8 * 4, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(MMAP_THRESHOLD + 8 * 2);
    REQUIRE(b != nullptr);
    verify_blocks(1, MMAP_THRESHOLD + 8 * 2, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, MMAP_THRESHOLD + 8 * 2, 1, MMAP_THRESHOLD + 8 * 2);
    verify_size(base);
}

TEST_CASE("Dynamic mmap threshold max", "[malloc4]")
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *a = (char *)smalloc(DEFAULT_MMAP_THRESHOLD_MAX);
    REQUIRE(a != nullptr);
    verify_blocks(1, DEFAULT_MMAP_THRESHOLD_MAX, 0, 0);
    verify_size_with_large_blocks(base, 0);

    sfree(a);
    verify_blocks(0, 0, 0, 0);
    verify_size(base);

    char *b = (char *)smalloc(DEFAULT_MMAP_THRESHOLD_MAX - 8);
    REQUIRE(b != nullptr);
    verify_blocks(1, DEFAULT_MMAP_THRESHOLD_MAX - 8, 0, 0);
    verify_size(base);

    sfree(b);
    verify_blocks(1, DEFAULT_MMAP_THRESHOLD_MAX - 8, 1, DEFAULT_MMAP_THRESHOLD_MAX - 8);
    verify_size(base);
}
