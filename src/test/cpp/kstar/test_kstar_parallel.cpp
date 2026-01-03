#include <future>
#include <vector>

#include <gtest/gtest.h>

#include "sequence.hpp"
#include "thread_pool.hpp"

using namespace osprey;

TEST(KStarParallel_SYNTHESIZED, SequenceEqualityAndFields) {
    Sequence seq;
    seq.residue_assignments = {0, 1, 2, 3};
    seq.string_representation = "0 1 2 3";

    ASSERT_EQ(seq.residue_assignments.size(), 4u);
    EXPECT_EQ(seq.residue_assignments[0], 0);
    EXPECT_EQ(seq.residue_assignments[1], 1);

    Sequence seq2 = seq;
    EXPECT_TRUE(seq == seq2);
}

TEST(KStarParallel_SYNTHESIZED, ThreadPoolEnqueueAndResults) {
    ThreadPool pool(4);

    std::vector<std::future<int>> futures;
    futures.reserve(10);
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([i]() { return i * 2; }));
    }

    for (size_t i = 0; i < futures.size(); ++i) {
        EXPECT_EQ(futures[i].get(), static_cast<int>(i) * 2);
    }
}

