#ifndef DEFERRED_COMMAND_BUFFER_HPP
#define DEFERRED_COMMAND_BUFFER_HPP
#pragma once

#include <vector>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <cstring>
#include <algorithm>

#include "Manager.hpp"
#include "Entity.hpp"

constexpr uint64_t fnv1a_64_cmd(const char *str, size_t len) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(str[i]);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

template <typename T> constexpr uint64_t getComponentHash() {
#if defined(__clang__) || defined(__GNUC__)
    return fnv1a_64_cmd(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 1);
#elif defined(_MSC_VER)
    return fnv1a_64_cmd(__FUNCSIG__, sizeof(__FUNCSIG__) - 1);
#endif
}

class DeferredCommandBuffer {
private:
    Manager& manager;

    using ExecuteFn = void(*)(Manager&, EntityId, std::byte*);
    using DestroyFn = void(*)(std::byte*);

    enum class CmdType : uint8_t { ADD, REMOVE, DESTROY };

    struct CommandHeader {
        EntityId entity;
        uint64_t componentHash;
        CmdType type;
        
        ExecuteFn execute;
        DestroyFn destroy;
        
        size_t next_cmd_offset; 
        size_t payload_offset; 
    };

    std::vector<std::byte> buffer;

    static constexpr size_t align_forward(size_t ptr, size_t alignment) noexcept {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }

public:
    DeferredCommandBuffer(Manager& m) : manager(m) {
        buffer.reserve(8192);
    }

    ~DeferredCommandBuffer() {
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
        header.componentHash = getComponentHash<T>();
        header.type = CmdType::ADD;
        header.next_cmd_offset = next_cmd_offset;
        header.payload_offset = payload_offset;
        
        header.execute = [](Manager& m, EntityId e, std::byte* p) {
            T* comp_ptr = reinterpret_cast<T*>(p);
            m.addComponent<T>(e, comp_ptr);
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
        header.componentHash = getComponentHash<T>();
        header.type = CmdType::REMOVE;
        header.next_cmd_offset = next_cmd_offset;
        header.payload_offset = 0;
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
        header.componentHash = 0;
        header.type = CmdType::DESTROY;
        header.next_cmd_offset = next_cmd_offset;
        header.payload_offset = 0; 
        header.destroy = nullptr;
        
        header.execute = [](Manager& m, EntityId e, std::byte*) {
            m.destroyEntity(e);
        };

        std::memcpy(buffer.data() + current_offset, &header, sizeof(CommandHeader));
    }

    void execute() {
        if (buffer.empty()) return;

        std::vector<CommandHeader*> cmds;
        size_t offset = 0;
        while (offset < buffer.size()) {
            CommandHeader* header = reinterpret_cast<CommandHeader*>(buffer.data() + offset);
            cmds.push_back(header);
            offset = header->next_cmd_offset;
        }

        std::stable_sort(cmds.begin(), cmds.end(), [](const CommandHeader* a, const CommandHeader* b) {
            return a->entity < b->entity;
        });

        size_t i = 0;
        while (i < cmds.size()) {
            EntityId current_entity = cmds[i]->entity;
            size_t j = i;
            
            bool is_destroyed = false;
            
            while (j < cmds.size() && cmds[j]->entity == current_entity) {
                if (cmds[j]->type == CmdType::DESTROY) is_destroyed = true;
                j++;
            }

            if (is_destroyed) {
                manager.destroyEntity(current_entity);
            } else {
                
                std::vector<CommandHeader*> final_cmds;
                
                for (size_t k = i; k < j; ++k) {
                    CommandHeader* cmd = cmds[k];
                    
                    bool overwritten = false;
                    for (auto& f_cmd : final_cmds) {
                        if (f_cmd->componentHash == cmd->componentHash) {
                            f_cmd = cmd;
                            overwritten = true;
                            break;
                        }
                    }
                    if (!overwritten) {
                        final_cmds.push_back(cmd);
                    }
                }

                for (CommandHeader* cmd : final_cmds) {
                    std::byte* payload = cmd->payload_offset ? buffer.data() + cmd->payload_offset : nullptr;
                    cmd->execute(manager, cmd->entity, payload);
                }
            }

            i = j;
        }

        clear();
    }

    void clear() {
        size_t offset = 0;
        while (offset < buffer.size()) {
            CommandHeader* header = reinterpret_cast<CommandHeader*>(buffer.data() + offset);
            if (header->destroy && header->payload_offset) {
                std::byte* payload_ptr = buffer.data() + header->payload_offset;
                header->destroy(payload_ptr);
            }
            offset = header->next_cmd_offset;
        }
        buffer.clear();
    }
};

#endif