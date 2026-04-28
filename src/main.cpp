#include <iostream>
#include <cassert>
#include <vector>

#include "obj/ECS/Manager.hpp"
#include "obj/CommandBuffer.hpp"

// ==========================================
// DUMMY COMPONENTS FOR TESTING
// ==========================================
struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct TagComponent {}; // Tests the zero-byte component edge case

// ==========================================
// TEST CASES
// ==========================================

void testEntityLifecycle(Manager& m) {
    std::cout << "[TEST] Entity Lifecycle... ";
    
    EntityId e1 = m.addEntity();
    assert(m.isEntityValid(e1));
    
    m.destroyEntity(e1);
    assert(!m.isEntityValid(e1)); // e1 is now a dangling ID

    EntityId e2 = m.addEntity();
    assert(m.isEntityValid(e2));
    
    // The index should be reused (index 0), but the generation must be bumped
    assert(e1 != e2); 
    assert(getEntityIndex(e1) == getEntityIndex(e2));
    assert(getEntityGeneration(e1) < getEntityGeneration(e2));
    
    std::cout << "PASSED\n";
}

void testArchetypeTransitions(Manager& m) {
    std::cout << "[TEST] Archetype Transitions & Component Retrieval... ";
    
    EntityId e = m.addEntity();
    
    // 1. Add Single Component
    Position p{10.0f, 20.0f};
    m.addComponent<Position>(e, &p);
    
    assert(m.has_component<Position>(e));
    assert(!m.has_component<Velocity>(e));
    
    Position* retrievedPos = m.getComponent<Position>(e);
    assert(retrievedPos != nullptr);
    assert(retrievedPos->x == 10.0f && retrievedPos->y == 20.0f);

    // 2. Add Multiple Components (Triggering an Archetype Edge move)
    Velocity v{1.5f, 2.5f};
    m.addComponents<Velocity, TagComponent>(e, &v, nullptr);
    
    assert(m.has_component<Velocity>(e));
    assert(m.has_component<TagComponent>(e));
    assert(m.has_component<Position>(e)); // Should have moved over safely

    // Values should survive the archetype memory transition
    Velocity* retrievedVel = m.getComponent<Velocity>(e);
    retrievedPos = m.getComponent<Position>(e);
    assert(retrievedVel->dx == 1.5f);
    assert(retrievedPos->x == 10.0f);

    std::cout << "PASSED\n";
}

void testSwapAndPop(Manager& m) {
    std::cout << "[TEST] Swap and Pop (Component Removal)... ";
    
    EntityId e1 = m.addEntity();
    EntityId e2 = m.addEntity();
    EntityId e3 = m.addEntity();

    Position p1{1.0f, 1.0f};
    Position p2{2.0f, 2.0f};
    Position p3{3.0f, 3.0f};

    // All entities land in the same Archetype Table
    m.addComponent<Position>(e1, &p1);
    m.addComponent<Position>(e2, &p2);
    m.addComponent<Position>(e3, &p3);

    // Remove the middle element (e2) to trigger Swap-and-Pop logic.
    // e3's data should be moved into e2's old slot, and the table should shrink.
    m.removeComponent<Position>(e2);

    assert(m.has_component<Position>(e1));
    assert(!m.has_component<Position>(e2));
    assert(m.has_component<Position>(e3));

    // Verify memory integrity of the moved entity
    Position* retrievedE3 = m.getComponent<Position>(e3);
    assert(retrievedE3->x == 3.0f && retrievedE3->y == 3.0f);
    
    // Verify untouched entity
    Position* retrievedE1 = m.getComponent<Position>(e1);
    assert(retrievedE1->x == 1.0f && retrievedE1->y == 1.0f);

    std::cout << "PASSED\n";
}

void testPagedColumnBoundariesAndSystems(Manager& m) {
    std::cout << "[TEST] Paged Memory Boundaries & System Execution... ";
    
    // Create enough entities to force PagedColumn to allocate new pages.
    // Even if PAGE_SIZE is large, creating 5000 entities guarantees robust iteration testing.
    const int ENTITY_COUNT = 5000;
    std::vector<EntityId> tracking;
    tracking.reserve(ENTITY_COUNT);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = m.addEntity();
        Position p{0.0f, 0.0f};
        Velocity v{1.0f, 2.0f}; // Move 1 unit right, 2 units down per step
        
        m.addComponents<Position, Velocity>(e, &p, &v);
        tracking.push_back(e);
    }

    // Run a system to iterate over ALL 5000 entities. 
    // This tests our variadic tuple expansion and PagedColumn::get(physRow) lookups.
    m.runSystem<Position, Velocity>([](EntityId e, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    // Verify the math applied correctly to a random subset (First, Middle, Last)
    Position* pFirst = m.getComponent<Position>(tracking[0]);
    Position* pMid   = m.getComponent<Position>(tracking[ENTITY_COUNT / 2]);
    Position* pLast  = m.getComponent<Position>(tracking[ENTITY_COUNT - 1]);

    assert(pFirst->x == 1.0f && pFirst->y == 2.0f);
    assert(pMid->x == 1.0f   && pMid->y == 2.0f);
    assert(pLast->x == 1.0f  && pLast->y == 2.0f);

    std::cout << "PASSED\n";
}

void testEntitylessSystem(Manager& m) {
    std::cout << "[TEST] Entity-less System Execution... ";
    
    EntityId e1 = m.addEntity();
    EntityId e2 = m.addEntity();
    EntityId e3 = m.addEntity(); // This entity will act as our control/filter test

    Position p1{0.0f, 0.0f}; Velocity v1{1.0f, 1.5f};
    Position p2{10.0f, 10.0f}; Velocity v2{-2.0f, -2.0f};
    Position p3{50.0f, 50.0f}; // No velocity!

    m.addComponents<Position, Velocity>(e1, &p1, &v1);
    m.addComponents<Position, Velocity>(e2, &p2, &v2);
    
    // e3 only gets a Position. It should be completely ignored by the system below.
    m.addComponent<Position>(e3, &p3);

    // Run the system without requesting the EntityId in the lambda signature
    m.runSystem<Position, Velocity>([](Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    // Verify e1 and e2 were updated
    Position* res1 = m.getComponent<Position>(e1);
    assert(res1->x == 1.0f && res1->y == 1.5f);

    Position* res2 = m.getComponent<Position>(e2);
    assert(res2->x == 8.0f && res2->y == 8.0f);

    // Verify e3 was completely bypassed by the Archetype filter
    Position* res3 = m.getComponent<Position>(e3);
    assert(res3->x == 50.0f && res3->y == 50.0f);

    std::cout << "PASSED\n";
}

void testDeferredCommandBuffer(Manager& m) {
    std::cout << "[TEST] Deferred Command Buffer... ";
    
    CommandBuffer cmd;
    
    EntityId e1 = m.addEntity();
    EntityId e2 = m.addEntity();

    m.addComponent<Position>(e1);
    m.addComponent<Position>(e2);
    m.addComponent<Velocity>(e1);

    // Run a system that queues structural changes WHILE iterating
    m.runSystem<Position>([&cmd](EntityId e, Position& pos) {
        // Safe: Intending to add a tag
        cmd.addComponent<TagComponent>(e); 
        
        // Safe: Intending to remove a component
        cmd.removeComponent<Position>(e);  
    });

    // Run a system that targets a specific entity for destruction
    m.runSystem<Velocity>([&cmd](EntityId e, Velocity& vel) {
        // Safe: Intending to destroy the whole entity
        cmd.destroyEntity(e); 
    });

    // Iterators are now finished. We can safely flush!
    cmd.flush(m);

    // Verify the deferred changes were applied correctly
    assert(!m.isEntityValid(e1)); // e1 had Velocity, so it should be destroyed entirely
    assert(m.isEntityValid(e2));  // e2 didn't have Velocity, so it survives
    
    assert(!m.has_component<Position>(e2));  // Position was deferred-removed
    assert(m.has_component<TagComponent>(e2)); // TagComponent was deferred-added

    std::cout << "PASSED\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "    ECS ARCHITECTURE TEST SUITE         \n";
    std::cout << "========================================\n";

    Manager manager;

    testEntityLifecycle(manager);
    testArchetypeTransitions(manager);
    testSwapAndPop(manager);
    testPagedColumnBoundariesAndSystems(manager);
    testEntitylessSystem(manager);
    testDeferredCommandBuffer(manager);

    std::cout << "========================================\n";
    std::cout << " ALL TESTS COMPLETED SUCCESSFULLY!      \n";
    std::cout << " Memory Layout & Lifecycles Verified.   \n";
    std::cout << "========================================\n";

    return 0;
}