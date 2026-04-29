#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>

#include "obj/ECS/Manager.hpp"
#include "obj/ECS/View.hpp"
#include "obj/CommandBuffer.hpp"
#include "obj/ECS/Timer/Timer.hpp"

#define TEST_CLEAN_ENV(testFunc)                                                                   \
    {                                                                                              \
        Manager manager;                                                                           \
        testFunc(manager);                                                                         \
    }

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

struct Health {
    float current = 100.0f;
};
struct Poison {
    float dps = 5.0f;
};
struct DeadTag {};

struct CompA {
    int val = 1;
};
struct CompB {
    int val = 2;
};
struct CompC {
    int val = 3;
};
struct CompD {
    int val = 4;
};
struct CompE {
    int val = 5;
};
struct CompF {
    int val = 6;
};

// ==========================================
// TEST CASES
// ==========================================

void testEntityLifecycle(Manager &m) {
    std::cout << "[TEST] Entity Lifecycle... " << std::flush;

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

void testArchetypeTransitions(Manager &m) {
    std::cout << "[TEST] Archetype Transitions & Component Retrieval..." << std::flush;

    EntityId e = m.addEntity();

    // 1. Add Single Component
    Position p{10.0f, 20.0f};
    m.addComponent<Position>(e, &p);
    assert(m.has_component<Position>(e));
    assert(!m.has_component<Velocity>(e));

    Position *retrievedPos = m.getComponent<Position>(e);
    assert(retrievedPos != nullptr);
    assert(retrievedPos->x == 10.0f && retrievedPos->y == 20.0f);

    // 2. Add Multiple Components (Triggering an Archetype Edge move)
    Velocity v{1.5f, 2.5f};
    m.addComponents<Velocity, TagComponent>(e, &v, nullptr);

    assert(m.has_component<Velocity>(e));
    assert(m.has_component<TagComponent>(e));
    assert(m.has_component<Position>(e)); // Should have moved over safely

    // Values should survive the archetype memory transition
    Velocity *retrievedVel = m.getComponent<Velocity>(e);
    retrievedPos = m.getComponent<Position>(e);
    assert(retrievedVel->dx == 1.5f);
    assert(retrievedPos->x == 10.0f);

    std::cout << "PASSED\n";
}

void testSwapAndPop(Manager &m) {
    std::cout << "[TEST] Swap and Pop (Component Removal)... " << std::flush;

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
    Position *retrievedE3 = m.getComponent<Position>(e3);
    assert(retrievedE3->x == 3.0f && retrievedE3->y == 3.0f);

    // Verify untouched entity
    Position *retrievedE1 = m.getComponent<Position>(e1);
    assert(retrievedE1->x == 1.0f && retrievedE1->y == 1.0f);

    std::cout << "PASSED\n";
}

void testPagedColumnBoundariesAndSystems(Manager &m) {
    std::cout << "[TEST] Paged Memory Boundaries & System Execution... " << std::flush;

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
    m.runSystem<Position, Velocity>([](EntityId e, Position &pos, Velocity &vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    // Verify the math applied correctly to a random subset (First, Middle, Last)
    Position *pFirst = m.getComponent<Position>(tracking[0]);
    Position *pMid = m.getComponent<Position>(tracking[ENTITY_COUNT / 2]);
    Position *pLast = m.getComponent<Position>(tracking[ENTITY_COUNT - 1]);

    assert(pFirst->x == 1.0f && pFirst->y == 2.0f);
    assert(pMid->x == 1.0f && pMid->y == 2.0f);
    assert(pLast->x == 1.0f && pLast->y == 2.0f);

    std::cout << "PASSED\n";
}

void testEntitylessSystem(Manager &m) {
    std::cout << "[TEST] Entity-less System Execution... " << std::flush;

    EntityId e1 = m.addEntity();
    EntityId e2 = m.addEntity();
    EntityId e3 = m.addEntity(); // This entity will act as our control/filter test

    Position p1{0.0f, 0.0f};
    Velocity v1{1.0f, 1.5f};
    Position p2{10.0f, 10.0f};
    Velocity v2{-2.0f, -2.0f};
    Position p3{50.0f, 50.0f}; // No velocity!

    m.addComponents<Position, Velocity>(e1, &p1, &v1);
    m.addComponents<Position, Velocity>(e2, &p2, &v2);

    // e3 only gets a Position. It should be completely ignored by the system below.
    m.addComponent<Position>(e3, &p3);

    // Run the system without requesting the EntityId in the lambda signature
    m.runSystem<Position, Velocity>([](Position &pos, Velocity &vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    // Verify e1 and e2 were updated
    Position *res1 = m.getComponent<Position>(e1);
    assert(res1->x == 1.0f && res1->y == 1.5f);

    Position *res2 = m.getComponent<Position>(e2);
    assert(res2->x == 8.0f && res2->y == 8.0f);

    // Verify e3 was completely bypassed by the Archetype filter
    Position *res3 = m.getComponent<Position>(e3);
    assert(res3->x == 50.0f && res3->y == 50.0f);

    std::cout << "PASSED\n";
}

void testDeferredCommandBuffer(Manager &m) {
    std::cout << "[TEST] Deferred Command Buffer... ";

    CommandBuffer cmd;

    EntityId e1 = m.addEntity();
    EntityId e2 = m.addEntity();

    m.addComponent<Position>(e1);
    m.addComponent<Position>(e2);
    m.addComponent<Velocity>(e1);

    // Run a system that queues structural changes WHILE iterating
    m.runSystem<Position>([&cmd](EntityId e, Position &pos) {
        // Safe: Intending to add a tag
        cmd.addComponent<TagComponent>(e);

        // Safe: Intending to remove a component
        cmd.removeComponent<Position>(e);
    });

    // Run a system that targets a specific entity for destruction
    m.runSystem<Velocity>([&cmd](EntityId e, Velocity &vel) {
        // Safe: Intending to destroy the whole entity
        cmd.destroyEntity(e);
    });

    // Iterators are now finished. We can safely flush!
    cmd.flush(m);

    // Verify the deferred changes were applied correctly
    assert(!m.isEntityValid(e1)); // e1 had Velocity, so it should be destroyed entirely
    assert(m.isEntityValid(e2));  // e2 didn't have Velocity, so it survives

    assert(!m.has_component<Position>(e2));    // Position was deferred-removed
    assert(m.has_component<TagComponent>(e2)); // TagComponent was deferred-added

    std::cout << "PASSED\n";
}

void testComponentInitialization(Manager &m) {
    std::cout << "[TEST] Component Initialization (Initial Values)... ";

    // 1. Single component WITH an explicit initial value
    EntityId e1 = m.addEntity();
    Position p1{123.4f, 567.8f};
    m.addComponent<Position>(e1, &p1);

    Position *res1 = m.getComponent<Position>(e1);
    assert(res1 != nullptr);
    assert(res1->x == 123.4f && res1->y == 567.8f);

    // 2. Single component WITHOUT an initial value (should default construct)
    EntityId e2 = m.addEntity();
    m.addComponent<Position>(e2); // initialValue defaults to nullptr

    Position *res2 = m.getComponent<Position>(e2);
    assert(res2 != nullptr);
    assert(res2->x == 0.0f && res2->y == 0.0f); // Verifies std::construct_at() zero-initialized it

    // 3. Multiple components WITH explicit initial values
    EntityId e3 = m.addEntity();
    Position p3{11.1f, 22.2f};
    Velocity v3{33.3f, 44.4f};
    m.addComponents<Position, Velocity>(e3, &p3, &v3);

    Position *res3p = m.getComponent<Position>(e3);
    Velocity *res3v = m.getComponent<Velocity>(e3);
    assert(res3p != nullptr && res3p->x == 11.1f && res3p->y == 22.2f);
    assert(res3v != nullptr && res3v->dx == 33.3f && res3v->dy == 44.4f);

    // 4. Multiple components with MIXED initial values and an empty Tag!
    // This specifically tests the `if constexpr (!std::is_empty_v<Ts>)` fix we applied.
    EntityId e4 = m.addEntity();
    Position p4{99.9f, 88.8f};

    // Passing &p4 for Position, nullptr for Velocity (forces default init), and nullptr for
    // TagComponent
    m.addComponents<Position, Velocity, TagComponent>(e4, &p4, nullptr, nullptr);

    Position *res4p = m.getComponent<Position>(e4);
    Velocity *res4v = m.getComponent<Velocity>(e4);

    assert(res4p != nullptr && res4p->x == 99.9f && res4p->y == 88.8f);
    assert(res4v != nullptr && res4v->dx == 0.0f && res4v->dy == 0.0f);
    assert(m.has_component<TagComponent>(e4)); // Tag should exist without corrupting memory

    std::cout << "PASSED\n";
}

void testSystemArchetypeFragmentation(Manager &m) {
    std::cout << "[TEST] Archetype Fragmentation Iteration... ";

    // We are going to spawn entities across 5 COMPLETELY DIFFERENT Archetypes (Tables).
    // They all share Position and Velocity, but have different tags.
    // A robust system iterator must seamlessly jump between all 5 tables without missing entities.

    EntityId e1 = m.addEntity();
    m.addComponents<Position, Velocity>(e1, new Position{0, 0}, new Velocity{1, 1});
    EntityId e2 = m.addEntity();
    m.addComponents<Position, Velocity, TagComponent>(
        e2, new Position{0, 0}, new Velocity{1, 1}, nullptr);
    EntityId e3 = m.addEntity();
    m.addComponents<Position, Velocity, DeadTag>(
        e3, new Position{0, 0}, new Velocity{1, 1}, nullptr);
    EntityId e4 = m.addEntity();
    m.addComponents<Position, Velocity, TagComponent, DeadTag>(
        e4, new Position{0, 0}, new Velocity{1, 1}, nullptr, nullptr);

    // e5 has Position, but NO velocity. It should be skipped!
    EntityId e5 = m.addEntity();
    m.addComponent<Position>(e5, new Position{0, 0});

    int processedCount = 0;
    m.runSystem<Position, Velocity>([&processedCount](EntityId e, Position &p, Velocity &v) {
        p.x += v.dx;
        processedCount++;
    });

    // Exactly 4 entities should have been processed across 4 different tables.
    assert(processedCount == 4);

    // Verify the math applied correctly across the fragmented memory
    assert(m.getComponent<Position>(e1)->x == 1.0f);
    assert(m.getComponent<Position>(e4)->x == 1.0f);

    // Verify e5 was ignored by the query
    assert(m.getComponent<Position>(e5)->x == 0.0f);

    std::cout << "PASSED\n";
}

void testSequentialSystemsPipeline(Manager &m) {
    std::cout << "[TEST] Sequential Systems & Command Buffer Pipeline... ";

    // Simulates a real game loop where System A's math triggers System B's logic
    EntityId player = m.addEntity();
    Position p{0.0f, 90.0f}; // Just below the poison zone (100.0f)
    Velocity v{0.0f, 15.0f}; // Will push player into the poison zone
    Health h{10.0f};
    Poison poison{15.0f}; // Deadly enough to kill in one tick

    m.addComponents<Position, Velocity, Health, Poison>(player, &p, &v, &h, &poison);

    CommandBuffer cmd;

    // SYSTEM 1: Movement
    m.runSystem<Position, Velocity>([](Position &pos, Velocity &vel) {
        pos.y += vel.dy;
    });

    // SYSTEM 2: Environmental Hazard (Only applies if Y > 100)
    m.runSystem<Position, Health, Poison>(
        [&cmd](EntityId e, Position &pos, Health &hp, Poison &dmg) {
            if (pos.y > 100.0f) {
                hp.current -= dmg.dps;
                if (hp.current <= 0) {
                    cmd.addComponent<DeadTag>(e); // Queue structural change
                }
            }
        });

    cmd.flush(m); // Apply the DeadTag

    // Validate the pipeline execution
    assert(m.getComponent<Position>(player)->y == 105.0f);    // Movement worked
    assert(m.getComponent<Health>(player)->current == -5.0f); // Damage worked
    assert(m.has_component<DeadTag>(player)); // Feedback loop into structural change worked

    std::cout << "PASSED\n";
}

void testMegaQueryTupleExpansion(Manager &m) {
    std::cout << "[TEST] Mega Query (6+ Components) Tuple Expansion... ";

    // This tests if `Table::column_mapping` and the variadic parameter pack unpacking
    // inside `runSystem` accidentally misaligns pointers when dealing with massive queries.

    EntityId boss = m.addEntity();
    CompA a;
    CompB b;
    CompC c;
    CompD d;
    CompE e_comp;
    CompF f;

    m.addComponents<CompA, CompB, CompC, CompD, CompE, CompF>(boss, &a, &b, &c, &d, &e_comp, &f);

    // Run a system that requires ALL 6 components, and mutates them
    m.runSystem<CompA, CompB, CompC, CompD, CompE, CompF>(
        [](CompA &ca, CompB &cb, CompC &cc, CompD &cd, CompE &ce, CompF &cf) {
            ca.val *= 2; // 2
            cb.val *= 2; // 4
            cc.val *= 2; // 6
            cd.val *= 2; // 8
            ce.val *= 2; // 10
            cf.val *= 2; // 12
        });

    // If tuple expansion or memory offsets are broken, these fetches will return garbage
    assert(m.getComponent<CompA>(boss)->val == 2);
    assert(m.getComponent<CompC>(boss)->val == 6);
    assert(m.getComponent<CompF>(boss)->val == 12);

    std::cout << "PASSED\n";
}

void testEmptyQuerySafety(Manager &m) {
    std::cout << "[TEST] Empty Query Safety (No Matches)... ";

    // Spawn an entity with Position, but run a system that wants Velocity.
    // The engine should cleanly skip all tables and not throw a segfault or out-of-bounds array
    // error.
    EntityId e = m.addEntity();
    Position p{10.0f, 10.0f};
    m.addComponent<Position>(e, &p);

    bool systemRan = false;
    m.runSystem<Velocity>([&systemRan](Velocity &v) {
        systemRan = true; // This should NEVER execute
    });

    assert(!systemRan && "System executed on unmatched archetype!");

    std::cout << "PASSED\n";
}

void testViewIteratorRobustness(Manager& m) {
    std::cout << "[TEST] View Iterator Robustness... ";

    // Table 1 (Matches) - 2 entities
    EntityId e1 = m.addEntity(); m.addComponents<Position, Velocity>(e1);
    EntityId e2 = m.addEntity(); m.addComponents<Position, Velocity>(e2);
    
    // Table 2 (Doesn't match) - 1 entity
    EntityId e3 = m.addEntity(); m.addComponents<Position, TagComponent>(e3);
    
    // Table 3 (Matches) - 1 entity
    EntityId e4 = m.addEntity(); m.addComponents<Position, Velocity, DeadTag>(e4);

    // Destroy e1 so Table 1 has a "hole" that got filled, testing if the iterator still reads exactly the right count
    m.destroyEntity(e1);

    int count = 0;
    auto myView = m.view<Position, Velocity>();
    
    for (EntityId e : myView) {
        count++;
        // Ensure e3 never sneaks in
        assert(e != e3 && "Iterator yielded an entity from the wrong archetype!");
    }

    // e2 and e4 should be the only ones processed
    assert(count == 2 && "Iterator failed to yield the exact number of entities!");

    std::cout << "PASSED\n";
}

void testComponentIdempotency(Manager& m) {
    std::cout << "[TEST] Component Idempotency (Double Add/Remove)... ";

    EntityId e = m.addEntity();
    Position p{10.0f, 20.0f};
    
    // 1. Double Add
    m.addComponent<Position>(e, &p);
    m.addComponent<Position>(e, new Position{99.0f, 99.0f}); // Should safely ignore or overwrite, NOT change archetype
    
    // Ensure memory wasn't corrupted and the archetype didn't shift invalidly
    assert(m.getComponent<Position>(e)->x == 10.0f || m.getComponent<Position>(e)->x == 99.0f);

    // 2. Double Remove
    m.removeComponent<Position>(e);
    m.removeComponent<Position>(e); // Should safely do nothing

    assert(m.getComponent<Position>(e) == nullptr);

    // 3. Remove from empty entity
    EntityId emptyEnt = m.addEntity();
    m.removeComponent<Velocity>(emptyEnt); // Should safely return

    std::cout << "PASSED\n";
}

void testEntityGenerationLimit(Manager& m) {
    std::cout << "[TEST] Entity Generation Exhaustion (ABA Problem)... ";

    EntityId firstId = m.addEntity();
    uint32_t index = getEntityIndex(firstId);

    // Kill and revive this exact index 4095 times
    for (int i = 0; i < 4095; ++i) {
        m.destroyEntity(firstId);
        firstId = m.addEntity();
        
        // Assert we are actually recycling the exact same slot
        assert(getEntityIndex(firstId) == index);
    }

    assert(getEntityGeneration(firstId) == 4095);

    // Destroy it one last time. It should hit the cap of 4095 and REFUSE to go in the freeEnttIds list.
    m.destroyEntity(firstId);

    // The next created entity should get a brand new index, NOT the exhausted one!
    EntityId newSlot = m.addEntity();
    assert(getEntityIndex(newSlot) != index && "Engine recycled an exhausted generation ID!");

    std::cout << "PASSED\n";
}

void testReverseSwapAndPopStress(Manager& m) {
    std::cout << "[TEST] Reverse Swap-and-Pop Stress... ";

    std::vector<EntityId> tracking;
    for (int i = 0; i < 1000; ++i) {
        EntityId e = m.addEntity();
        m.addComponent<Position>(e, new Position{(float)i, (float)i});
        tracking.push_back(e);
    }

    // Destroy every EVEN indexed entity starting from 0.
    // This forces maximum memory fragmentation and rapid swap-and-pops.
    for (int i = 0; i < 1000; i += 2) {
        m.destroyEntity(tracking[i]);
    }

    // Now verify the ODD indexed entities survived and have exactly the right data
    for (int i = 1; i < 1000; i += 2) {
        Position* p = m.getComponent<Position>(tracking[i]);
        assert(p != nullptr);
        assert(p->x == (float)i); // If swap-and-pop overwrote the wrong row, this fails!
    }

    std::cout << "PASSED\n";
}

void testParallelExecutionCorrectness(Manager& m) {
    std::cout << "[TEST] Parallel Execution Correctness... ";

    // Spawn 10,000 entities across a few different archetypes
    Position p{0.0f, 0.0f};
    Velocity v{1.0f, 2.0f};

    std::vector<EntityId> tracking;
    for (int i = 0; i < 10000; ++i) {
        EntityId e = m.addEntity();
        if (i % 2 == 0) {
            m.addComponents<Position, Velocity>(e, &p, &v);
        } else {
            m.addComponents<Position, Velocity, TagComponent>(e, &p, &v, nullptr);
        }
        tracking.push_back(e);
    }

    // Run the system in PARALLEL
    m.runSystem<Position, Velocity>(execution::par, [](EntityId e, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    // Verify the math applied perfectly to every single entity
    for (EntityId e : tracking) {
        Position* res = m.getComponent<Position>(e);
        assert(res != nullptr);
        assert(res->x == 1.0f && "Parallel chunking missed an entity's X value!");
        assert(res->y == 2.0f && "Parallel chunking missed an entity's Y value!");
    }

    std::cout << "PASSED\n";
}

void benchmarkSystemExecution(Manager& m) {
    std::cout << "\n========================================\n";
    std::cout << "   BENCHMARK: SEQUENTIAL VS PARALLEL    \n";
    std::cout << "========================================\n";

    // 1. Spawn 1 MILLION entities. 
    // This forces the PagedColumn to allocate nearly 1,000 pages.
    std::cout << "Spawning 1,000,000 entities... ";
    Position p{0.0f, 0.0f};
    Velocity v{1.0f, 1.0f};
    
    for (int i = 0; i < 1'000'000; ++i) {
        EntityId e = m.addEntity();
        m.addComponents<Position, Velocity>(e, &p, &v);
    }
    std::cout << "Done.\n\n";

    // 2. A heavily unoptimized payload to simulate complex game logic (like AI or Physics)
    auto heavyPayload = [](EntityId e, Position& pos, Velocity& vel) {
        // Run an expensive loop per entity to make the CPU sweat
        for (int i = 0; i < 50; ++i) { 
            pos.x += std::sin(pos.x * vel.dx) * 0.001f;
            pos.y += std::cos(pos.y * vel.dy) * 0.001f;
        }
    };

    // 3. Sequential Benchmark
    {
        std::cout << "[Running Sequential System (1 Core)]\n";
        __TIME_IT__
        m.runSystem<Position, Velocity>(heavyPayload); // Defaults to Exec::Seq
    }

    // 4. Parallel Benchmark
    {
        std::cout << "[Running Parallel System (All Cores)]\n";
        __TIME_IT__
        m.runSystem<Position, Velocity>(execution::par, heavyPayload); // Tag dispatched to Exec::Par
    }
    
    std::cout << "========================================\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "    ECS ARCHITECTURE TEST SUITE         \n";
    std::cout << "========================================\n";

    TEST_CLEAN_ENV(testEntityLifecycle);
    TEST_CLEAN_ENV(testComponentInitialization);
    TEST_CLEAN_ENV(testArchetypeTransitions);
    TEST_CLEAN_ENV(testSwapAndPop);
    TEST_CLEAN_ENV(testPagedColumnBoundariesAndSystems);
    TEST_CLEAN_ENV(testEntitylessSystem);
    TEST_CLEAN_ENV(testDeferredCommandBuffer);

    TEST_CLEAN_ENV(testSystemArchetypeFragmentation);
    TEST_CLEAN_ENV(testSequentialSystemsPipeline);
    TEST_CLEAN_ENV(testMegaQueryTupleExpansion);
    TEST_CLEAN_ENV(testEmptyQuerySafety);

    TEST_CLEAN_ENV(testViewIteratorRobustness);
    TEST_CLEAN_ENV(testComponentIdempotency)
    TEST_CLEAN_ENV(testEntityGenerationLimit)
    TEST_CLEAN_ENV(testReverseSwapAndPopStress)

    TEST_CLEAN_ENV(testParallelExecutionCorrectness);

    TEST_CLEAN_ENV(benchmarkSystemExecution);

    std::cout << "========================================\n";
    std::cout << " ALL TESTS COMPLETED SUCCESSFULLY!      \n";
    std::cout << " Memory Layout & Lifecycles Verified.   \n";
    std::cout << "========================================\n";

    return 0;
}