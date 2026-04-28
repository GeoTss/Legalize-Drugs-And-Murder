#ifndef COMMAND_BUFFER_HPP
#define COMMAND_BUFFER_HPP
#pragma once

#include <vector>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "ECS/Manager.hpp"
#include "ECS/Entity.hpp"

class CommandBuffer {
private:
    
    using ExecuteFn = void(*)(Manager&, EntityId, std::byte*);
    using DestroyFn = void(*)(std::byte*);

    struct CommandHeader {
        EntityId entity;
        ExecuteFn execute;
        DestroyFn destroy;
        size_t next_cmd_offset; 
    };

    std::vector<std::byte> buffer;

    static constexpr size_t align_forward(size_t ptr, size_t alignment) noexcept {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }

public:
    CommandBuffer() {
        buffer.reserve(8192);
    }

    ~CommandBuffer() {
        clear();
    }

    template <typename T>
    void addComponent(EntityId entity, const T& component = T{}) {
        size_t current_offset = buffer.size();
        
        size_t payload_offset = align_forward(current_offset + sizeof(CommandHeader), alignof(T));
        size_t next_cmd_offset = payload_offset + sizeof(T);

        buffer.resize(next_cmd_offset);

        std::byte* payload_ptr = buffer.data() + payload_offset;
        std::construct_at(reinterpret_cast<T*>(payload_ptr), component);

        CommandHeader header;
        header.entity = entity;
        header.next_cmd_offset = next_cmd_offset;
        
        header.execute = [](Manager& m, EntityId e, std::byte* p) {
            T* comp_ptr = reinterpret_cast<T*>(p);
            m.addComponent<T>(e, comp_ptr);
            std::destroy_at(comp_ptr);
        };
        
        header.destroy = [](std::byte* p) {
            std::destroy_at(reinterpret_cast<T*>(p));
        };

        std::memcpy(buffer.data() + current_offset, &header, sizeof(CommandHeader));
    }

    template <typename T>
    void removeComponent(EntityId entity) {
        size_t current_offset = buffer.size();
        size_t next_cmd_offset = current_offset + sizeof(CommandHeader);
        buffer.resize(next_cmd_offset);

        CommandHeader header;
        header.entity = entity;
        header.next_cmd_offset = next_cmd_offset;
        header.destroy = nullptr;
        
        header.execute = [](Manager& m, EntityId e, std::byte*) {
            m.removeComponent<T>(e);
        };

        std::memcpy(buffer.data() + current_offset, &header, sizeof(CommandHeader));
    }

    void destroyEntity(EntityId entity) {
        size_t current_offset = buffer.size();
        size_t next_cmd_offset = current_offset + sizeof(CommandHeader);
        buffer.resize(next_cmd_offset);

        CommandHeader header;
        header.entity = entity;
        header.next_cmd_offset = next_cmd_offset;
        header.destroy = nullptr;
        
        header.execute = [](Manager& m, EntityId e, std::byte*) {
            m.destroyEntity(e);
        };

        std::memcpy(buffer.data() + current_offset, &header, sizeof(CommandHeader));
    }

    void flush(Manager& manager) {
        size_t offset = 0;
        while (offset < buffer.size()) {
            CommandHeader* header = reinterpret_cast<CommandHeader*>(buffer.data() + offset);
            
            if (header->execute) {
                size_t payload_offset = align_forward(offset + sizeof(CommandHeader), alignof(std::max_align_t));
                std::byte* payload_ptr = buffer.data() + payload_offset;
                
                header->execute(manager, header->entity, payload_ptr);
            }
            offset = header->next_cmd_offset;
        }
        buffer.clear();
    }

    void clear() {
        size_t offset = 0;
        while (offset < buffer.size()) {
            CommandHeader* header = reinterpret_cast<CommandHeader*>(buffer.data() + offset);
            if (header->destroy) {
                size_t payload_offset = align_forward(offset + sizeof(CommandHeader), alignof(std::max_align_t));
                header->destroy(buffer.data() + payload_offset);
            }
            offset = header->next_cmd_offset;
        }
        buffer.clear();
    }
};

#endif