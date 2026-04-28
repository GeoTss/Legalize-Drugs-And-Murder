#pragma once

template<auto Id>
struct counter{
    using tag = counter;

    struct generator{
        friend consteval auto is_defined(tag) { return true; }
    };
    friend consteval auto is_defined(tag);

    template<typename Tag = tag, auto = is_defined(Tag{})>
    static consteval auto exists(auto) { return true; }

    static consteval auto exists(...) { return generator(), false; }
};

template<typename T, auto Id = int()>
consteval inline auto unique_id(){
    if constexpr (!counter<Id>::exists(Id)) return Id;
    else return unique_id<T, Id+1>();
}