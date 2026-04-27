#ifndef PAGED_COLUMN_HPP
#define PAGED_COLUMN_HPP
#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <cassert>

class PagedColumn {
    // 16KB per page fits nicely into L1/L2 caches and OS page boundaries
    static constexpr size_t PAGE_SIZE = 16384; 
    
    struct Page {
        // C++20: make_unique_for_overwrite avoids the zero-initialization penalty
        std::unique_ptr<std::byte[]> data;
        
        Page() : data(std::make_unique_for_overwrite<std::byte[]>(PAGE_SIZE)) {}
    };

    std::vector<Page> pages;
    size_t element_size;
    size_t elements_per_page;
    size_t total_elements = 0;

public:
    PagedColumn(size_t elem_size) : element_size(elem_size) {
        assert(elem_size > 0 && elem_size <= PAGE_SIZE && "Component size too large for page!");
        elements_per_page = PAGE_SIZE / element_size;
    }

    // Allocate space for one more element and return its index
    size_t push_back() {
        if (total_elements % elements_per_page == 0) {
            // Capacity reached (or initially empty), allocate a new page
            pages.emplace_back();
        }
        return total_elements++;
    }

    // Pop the last element (used when removing entities)
    void pop_back() {
        if (total_elements > 0) {
            total_elements--;
            // Optional: Free the page if it becomes completely empty to save memory
            if (total_elements % elements_per_page == 0 && !pages.empty()) {
                pages.pop_back();
            }
        }
    }

    // O(1) mathematical lookup to find the right page and offset
    void* get(size_t index) {
        assert(index < total_elements && "Out of bounds access!");
        size_t page_idx = index / elements_per_page;
        size_t element_idx = index % elements_per_page;
        return &pages[page_idx].data[element_idx * element_size];
    }

    size_t size() const { return total_elements; }
};

#endif