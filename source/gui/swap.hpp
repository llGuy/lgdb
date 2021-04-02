#pragma once


#include <stdint.h>

extern "C" {
#include <lgdb_alloc.h>
}


// May need to make this uint64_t although I doubt it for now
using copy_offset_t = uint32_t;


class var_swapchain_t {
public:
    void init(uint32_t size);
    void swap();

    void *get_address(copy_offset_t offset);
    void *get_address_prev(copy_offset_t offset);

    // Allocates in both
    copy_offset_t allocate(uint32_t byte_count);
private: 
    uint32_t current_alloc_;
    lgdb_linear_allocator_t allocs_[2];
};
