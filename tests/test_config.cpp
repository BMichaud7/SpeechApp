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
#include "Config.hpp"
#include <fstream>
#include <cstdio>

using namespace speech;

static std::string write_tmp(const std::string& content) {
    char path[] = "/tmp/speech_test_XXXXXX.xml";
    int fd = mkstemps(path, 4);
    if (fd < 0) throw std::runtime_error("mkstemps failed");
    write(fd, content.data(), content.size());
    close(fd);
    return path;
}

TEST(Config, Defaults) {
    AppConfig cfg;
    EXPECT_EQ(cfg.amqp.url,           "amqp://localhost:5672");
    EXPECT_EQ(cfg.amqp.demod_topic,   "rf.demod");
    EXPECT_FLOAT_EQ(cfg.vad.energy_threshold_db, -35.0f);
    EXPECT_EQ(cfg.vad.min_speech_ms,  250);
    EXPECT_EQ(cfg.transcriber.model_path, "/etc/sdr-speech/models/ggml-base.en.bin");
    EXPECT_EQ(cfg.transcriber.n_threads,  4);
    EXPECT_TRUE(cfg.transcript.enabled);
    EXPECT_EQ(cfg.transcript.rotate_days, 30);
}

TEST(Config, FullParse) {
    auto path = write_tmp(R"xml(<?xml version="1.0"?>
<speech_app>
  <amqp>
    <url>amqp://broker:5672</url>
    <username>alice</username>
    <password>secret</password>
    <demod_topic>rf.demod.test</demod_topic>
    <speech_topic>rf.speech.test</speech_topic>
  </amqp>
  <vad>
    <energy_threshold_db>-30</energy_threshold_db>
    <min_speech_ms>500</min_speech_ms>
  </vad>
  <transcriber>
    <model_path>/models/ggml-small.bin</model_path>
    <language>en</language>
    <n_threads>8</n_threads>
    <translate>true</translate>
  </transcriber>
  <transcript>
    <enabled>false</enabled>
    <dir>/tmp/logs</dir>
    <db_path>/tmp/logs/t.db</db_path>
    <rotate_days>7</rotate_days>
  </transcript>
</speech_app>)xml");

    auto cfg = AppConfig::from_xml(path);
    std::remove(path.c_str());

    EXPECT_EQ(cfg.amqp.url,         "amqp://broker:5672");
    EXPECT_EQ(cfg.amqp.username,    "alice");
    EXPECT_EQ(cfg.amqp.demod_topic, "rf.demod.test");
    EXPECT_FLOAT_EQ(cfg.vad.energy_threshold_db, -30.0f);
    EXPECT_EQ(cfg.vad.min_speech_ms, 500);
    EXPECT_EQ(cfg.transcriber.model_path, "/models/ggml-small.bin");
    EXPECT_EQ(cfg.transcriber.language,   "en");
    EXPECT_EQ(cfg.transcriber.n_threads,  8);
    EXPECT_TRUE(cfg.transcriber.translate);
    EXPECT_FALSE(cfg.transcript.enabled);
    EXPECT_EQ(cfg.transcript.dir,         "/tmp/logs");
    EXPECT_EQ(cfg.transcript.rotate_days, 7);
}

TEST(Config, MissingFileThrows) {
    EXPECT_THROW(AppConfig::from_xml("/nonexistent.xml"), std::runtime_error);
}

TEST(Config, PartialConfigKeepsDefaults) {
    auto path = write_tmp("<speech_app><amqp><url>amqp://x:1</url></amqp></speech_app>");
    auto cfg  = AppConfig::from_xml(path);
    std::remove(path.c_str());
    EXPECT_EQ(cfg.amqp.url, "amqp://x:1");
    EXPECT_FLOAT_EQ(cfg.vad.energy_threshold_db, -35.0f);  // default preserved
    EXPECT_EQ(cfg.transcriber.n_threads, 4);
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
