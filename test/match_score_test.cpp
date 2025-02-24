#include <chrono>
#include <gtest/gtest.h>
#include <match_score.h>

TEST(MatchTest, TokenOffsetsExceedWindowSize) {
    std::vector<std::vector<uint16_t>> token_positions = {
        std::vector<uint16_t>({1}), std::vector<uint16_t>({1}), std::vector<uint16_t>({1}), std::vector<uint16_t>({1}),
        std::vector<uint16_t>({1}), std::vector<uint16_t>({1}), std::vector<uint16_t>({1}), std::vector<uint16_t>({1}),
        std::vector<uint16_t>({1}), std::vector<uint16_t>({1}), std::vector<uint16_t>({1}), std::vector<uint16_t>({1})
    };

    const Match & this_match = Match(100, token_positions);

    ASSERT_EQ(WINDOW_SIZE, (size_t)this_match.words_present);
}

TEST(MatchTest, MatchScoreV2) {
    std::vector<std::vector<uint16_t>> token_offsets;
    token_offsets.push_back({25});
    token_offsets.push_back({26});
    token_offsets.push_back({11, 18, 24, 60});
    token_offsets.push_back({14, 27, 63});

    auto match = Match(100, token_offsets, true);
    ASSERT_EQ(4, match.words_present);
    ASSERT_EQ(3, match.distance);

    std::vector<uint16_t> expected_offsets = {25, 26, 24, 27};
    for(size_t i=0; i<token_offsets.size(); i++) {
        ASSERT_EQ(expected_offsets[i], match.offsets[i].offset);
    }

    // without populate window
    match = Match(100, token_offsets, false);
    ASSERT_EQ(4, match.words_present);
    ASSERT_EQ(3, match.distance);
    ASSERT_EQ(0, match.offsets.size());

    token_offsets.clear();
    token_offsets.push_back({38, 50, 170, 187, 195, 222});
    token_offsets.push_back({39, 140, 171, 189, 223});
    token_offsets.push_back({169, 180});

    match = Match(100, token_offsets, true);
    ASSERT_EQ(3, match.words_present);
    ASSERT_EQ(2, match.distance);

    expected_offsets = {170, 171, 169};
    for(size_t i=0; i<token_offsets.size(); i++) {
        ASSERT_EQ(expected_offsets[i], match.offsets[i].offset);
    }

    token_offsets.clear();
    token_offsets.push_back({38, 50, 187, 195, 201});
    token_offsets.push_back({120, 167, 171, 223});
    token_offsets.push_back({240, 250});

    match = Match(100, token_offsets, true);
    ASSERT_EQ(1, match.words_present);
    ASSERT_EQ(0, match.distance);

    expected_offsets = {38, MAX_DISPLACEMENT, MAX_DISPLACEMENT};
    for(size_t i=0; i<token_offsets.size(); i++) {
        ASSERT_EQ(expected_offsets[i], match.offsets[i].offset);
    }

    // without populate window
    match = Match(100, token_offsets, false);
    ASSERT_EQ(1, match.words_present);
    ASSERT_EQ(0, match.distance);
    ASSERT_EQ(0, match.offsets.size());


    /*size_t total_distance = 0, words_present = 0, offset_sum = 0;
    auto begin = std::chrono::high_resolution_clock::now();

    for(size_t i = 0; i < 1; i++) {
        auto match = Match(100, token_offsets, true);
        total_distance += match.distance;
        words_present += match.words_present;
        offset_sum += match.offsets.size();
    }

    uint64_t timeNanos = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - begin).count();
    LOG(INFO) << "Time taken: " << timeNanos;
    LOG(INFO) << total_distance << ", " << words_present << ", " << offset_sum;*/
}