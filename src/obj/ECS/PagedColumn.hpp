#ifndef PAGED_COLUMN_HPP
#define PAGED_COLUMN_HPP
#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <cassert>

class PagedColumn {
public:
    // 1024 entities per page guarantees perfect column alignment across the Table for SIMD
    static constexpr size_t ENTITIES_PER_PAGE = 1024; 
    
private:
    struct Page {
        // C++20: make_unique_for_overwrite avoids the $O(N)$ zero-initialization penalty
        std::unique_ptr<uint8_t[]> data;
        
        // Dynamically size the page based on the specific component's byte size
        Page(size_t bytes) : data(std::make_unique_for_overwrite<uint8_t[]>(bytes)) {}
    };

    std::vector<Page> pages;
    size_t element_size;
    size_t total_elements = 0;

public:
    PagedColumn(size_t elem_size) : element_size(elem_size) {
        assert(elem_size > 0 && "Component size must be greater than 0!");
    }

    // Allocate space for one more element and return its previous index
    size_t push_back() {
        if (total_elements == pages.size() * ENTITIES_PER_PAGE) {
            // Capacity reached (or initially empty), allocate a new perfectly-sized page
            pages.emplace_back(ENTITIES_PER_PAGE * element_size);
        }
        return total_elements++;
    }

    // Pop the last element (used during entity destruction and swap-and-pop)
    void pop_back() {
        if (total_elements > 0) {
            total_elements--;
            // Free the page entirely if it becomes empty to prevent memory bloating
            if ((total_elements == pages.size() * ENTITIES_PER_PAGE) && !pages.empty()) {
                pages.pop_back();
            }
        }
    }

    // Random Access: O(1) mathematical lookup to find the right page and offset
    void* get(size_t index) {
        assert(index < total_elements && "Out of bounds access!");
        size_t page_idx = index / ENTITIES_PER_PAGE;
        size_t element_idx = index % ENTITIES_PER_PAGE;
        return &pages[page_idx].data[element_idx * element_size];
    }

    // Sequential Access: Expose the raw contiguous block for SIMD systems
    void* get_page_data(size_t page_index) {
        assert(page_index < pages.size() && "Out of bounds page access!");
        return pages[page_index].data.get();
    }

    size_t size() const { return total_elements; }
    
    size_t capacity() const { return pages.size() * ENTITIES_PER_PAGE; }
};

#endif