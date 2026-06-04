/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#include <gtest/gtest.h>
#include "TranscriptStore.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace speech;
namespace fs = std::filesystem;

class TranscriptStoreTest : public ::testing::Test {
protected:
    std::string tmp_dir;
    std::string db_path;

    void SetUp() override {
        char tmpl[] = "/tmp/speech_store_XXXXXX";
        tmp_dir = mkdtemp(tmpl);
        db_path = tmp_dir + "/t.db";
    }
    void TearDown() override {
        fs::remove_all(tmp_dir);
    }

    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void save_entry(TranscriptStore& s, const std::string& text,
                    double freq = 146.52e6, int64_t ts = 0) {
        s.save(ts > 0 ? ts : now_ms(), freq, "FM_NB",
               "en", 0.95f, -20.0f, 4.0f, text);
    }
};

TEST_F(TranscriptStoreTest, DbFileCreated) {
    TranscriptStore s(tmp_dir, db_path);
    EXPECT_TRUE(fs::exists(db_path));
}

TEST_F(TranscriptStoreTest, SingleSaveAppearsInTail) {
    TranscriptStore s(tmp_dir, db_path);
    save_entry(s, "Alpha Bravo Charlie");
    auto rows = s.tail(10);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].text, "Alpha Bravo Charlie");
    EXPECT_EQ(rows[0].freq_mhz, "146.520");
    EXPECT_EQ(rows[0].modulation, "FM_NB");
}

TEST_F(TranscriptStoreTest, TailOrderedNewestFirst) {
    TranscriptStore s(tmp_dir, db_path);
    for (int i = 0; i < 5; ++i)
        save_entry(s, "msg " + std::to_string(i), 146.52e6, 1000LL * (i+1));
    auto rows = s.tail(10);
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0].text, "msg 4");   // newest first
    EXPECT_EQ(rows[4].text, "msg 0");
}

TEST_F(TranscriptStoreTest, TailLimitRespected) {
    TranscriptStore s(tmp_dir, db_path);
    for (int i = 0; i < 20; ++i)
        save_entry(s, "entry " + std::to_string(i));
    EXPECT_EQ(s.tail(5).size(), 5u);
}

TEST_F(TranscriptStoreTest, EmptyTextNotSaved) {
    TranscriptStore s(tmp_dir, db_path);
    s.save(now_ms(), 146.52e6, "FM_NB", "en", 0.9f, -20.0f, 4.0f, "");
    EXPECT_EQ(s.tail(10).size(), 0u);
}

TEST_F(TranscriptStoreTest, TextLogFileCreated) {
    TranscriptStore s(tmp_dir, db_path);
    save_entry(s, "Broadcast test");
    bool found = false;
    for (auto& e : fs::directory_iterator(tmp_dir))
        if (e.path().filename().string().starts_with("transcript_"))
            found = true;
    EXPECT_TRUE(found);
}

TEST_F(TranscriptStoreTest, TextLogContainsEntry) {
    TranscriptStore s(tmp_dir, db_path);
    save_entry(s, "Coast guard, over.", 162.025e6);
    std::string content;
    for (auto& e : fs::directory_iterator(tmp_dir)) {
        if (!e.path().string().ends_with(".txt")) continue;
        std::ifstream f(e.path());
        content = std::string(std::istreambuf_iterator<char>(f), {});
    }
    EXPECT_NE(content.find("162.025"), std::string::npos);
    EXPECT_NE(content.find("Coast guard"), std::string::npos);
}

TEST_F(TranscriptStoreTest, MultipleEntriesAppend) {
    TranscriptStore s(tmp_dir, db_path);
    save_entry(s, "first message");
    save_entry(s, "second message");
    std::string content;
    for (auto& e : fs::directory_iterator(tmp_dir)) {
        if (!e.path().string().ends_with(".txt")) continue;
        std::ifstream f(e.path());
        content = std::string(std::istreambuf_iterator<char>(f), {});
    }
    EXPECT_NE(content.find("first message"),  std::string::npos);
    EXPECT_NE(content.find("second message"), std::string::npos);
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
