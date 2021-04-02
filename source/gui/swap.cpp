#include "swap.hpp"
#include <assert.h>


void var_swapchain_t::init(uint32_t size) {
    allocs_[0] = lgdb_create_linear_allocator(size);
    allocs_[1] = lgdb_create_linear_allocator(size);
    current_alloc_ = 0;
    
    // Allocate just 8 bytes so that we don't start with an offset of 0
    lgdb_lnmalloc(&allocs_[0], 8);
    lgdb_lnmalloc(&allocs_[1], 8);
}


void var_swapchain_t::swap() {
    current_alloc_ = !current_alloc_;
}


void *var_swapchain_t::get_address(copy_offset_t offset) {
    return (void *)((uint8_t *)allocs_[current_alloc_].start + offset);
}


void *var_swapchain_t::get_address_prev(copy_offset_t offset) {
    return (void *)((uint8_t *)allocs_[!current_alloc_].start + offset);
}


copy_offset_t var_swapchain_t::allocate(uint32_t byte_count) {
    void *new_addr_curr = lgdb_lnmalloc(&allocs_[ current_alloc_], byte_count);
    void *new_addr_prev = lgdb_lnmalloc(&allocs_[!current_alloc_], byte_count);

    copy_offset_t diff_curr = (uint8_t *)new_addr_curr - (uint8_t *)allocs_[ current_alloc_].start;
    copy_offset_t diff_prev = (uint8_t *)new_addr_prev - (uint8_t *)allocs_[!current_alloc_].start;

    assert(diff_curr == diff_prev);

    return diff_curr;
}
