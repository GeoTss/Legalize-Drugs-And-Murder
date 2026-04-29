#ifndef PAGED_COLUMN_HPP
#define PAGED_COLUMN_HPP
#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <cassert>

class PagedColumn {
public:
    static constexpr size_t ENTITIES_PER_PAGE = 1024; 
    
private:
    struct Page {
        std::unique_ptr<uint8_t[]> data;
        
        Page(size_t bytes) : data(std::make_unique_for_overwrite<uint8_t[]>(bytes)) {}
    };

    std::vector<Page> pages;
    size_t element_size;
    size_t total_elements = 0;

public:
    PagedColumn(size_t elem_size) : element_size(elem_size) {
        assert(elem_size > 0 && "Component size must be greater than 0!");
    }

    size_t push_back() {
        if (total_elements == pages.size() * ENTITIES_PER_PAGE) {
            pages.emplace_back(ENTITIES_PER_PAGE * element_size);
        }
        return total_elements++;
    }

    void pop_back() {
        if (total_elements > 0) {
            total_elements--;
            if ((total_elements == pages.size() * ENTITIES_PER_PAGE) && !pages.empty()) {
                pages.pop_back();
            }
        }
    }

    void* get(size_t index) {
        assert(index < total_elements && "Out of bounds access!");
        size_t page_idx = index / ENTITIES_PER_PAGE;
        size_t element_idx = index % ENTITIES_PER_PAGE;
        return &pages[page_idx].data[element_idx * element_size];
    }

    void* get_page_data(size_t page_index) {
        assert(page_index < pages.size() && "Out of bounds page access!");
        return pages[page_index].data.get();
    }

    size_t size() const { return total_elements; }
    
    size_t capacity() const { return pages.size() * ENTITIES_PER_PAGE; }
};

#endif