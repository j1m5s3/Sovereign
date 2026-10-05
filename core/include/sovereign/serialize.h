// Versioned save format (engine doc, Core foundations: Saves). Little-endian,
// explicit field widths, no padding: identical bytes on every platform.
#pragma once

#include "sovereign/api.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sovereign/game.h"
#include "sovereign/state.h"

namespace sov {

constexpr uint32_t kSaveVersion = 35;  // 2: cities (MVP-2), commands carry arg2; 3: research (MVP-3); 4: improvements; 5: combat; 6: city combat, barbarians; 7: districts; 8: victory; 9: leader gear and escorts; 10: succession, captivity, regicide; 11: assassins, leader promotions; 12: loyalty, Free Cities; 13: stances, reputation; 14: live battle contract; 15: live city assaults; 16: naval play (overland move orders); 17: great people and Great Works; 18: religion; 19: trade routes and roads; 20: world wonders; 21: city-states and envoys; 22: eras, ages and tourism; 23: diplomacy (commands carry data and text); 24: conversation summaries; 25: governors; 26: spies; 27: grievances, favor, World Congress; 28: climate and disasters; 29: difficulty; 30: player profiles; 31: live-battle habits; 32: setup profiles; 33: city projects; 34: the Science victory; 35: kills this era

class SOV_API ByteWriter {
public:
    void u8(uint8_t v) { buf_.push_back(v); }
    void u16(uint16_t v) { for (int i = 0; i < 2; ++i) u8(static_cast<uint8_t>(v >> (8 * i))); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) u8(static_cast<uint8_t>(v >> (8 * i))); }
    void u64(uint64_t v) { for (int i = 0; i < 8; ++i) u8(static_cast<uint8_t>(v >> (8 * i))); }
    void i8(int8_t v) { u8(static_cast<uint8_t>(v)); }
    void i16(int16_t v) { u16(static_cast<uint16_t>(v)); }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void i64(int64_t v) { u64(static_cast<uint64_t>(v)); }
    void boolean(bool v) { u8(v ? 1 : 0); }
    void str(const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        buf_.insert(buf_.end(), s.begin(), s.end());
    }
    void bytes(const std::vector<uint8_t>& b) {
        u32(static_cast<uint32_t>(b.size()));
        buf_.insert(buf_.end(), b.begin(), b.end());
    }
    const std::vector<uint8_t>& data() const { return buf_; }
    std::vector<uint8_t> take() { return std::move(buf_); }

private:
    std::vector<uint8_t> buf_;
};

class SOV_API ByteReader {
public:
    explicit ByteReader(const std::vector<uint8_t>& b) : b_(b) {}
    bool ok() const { return ok_; }
    bool atEnd() const { return pos_ == b_.size(); }
    uint8_t u8() {
        if (pos_ >= b_.size()) { ok_ = false; return 0; }
        return b_[pos_++];
    }
    uint16_t u16() { uint16_t v = 0; for (int i = 0; i < 2; ++i) v |= static_cast<uint16_t>(u8()) << (8 * i); return v; }
    uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(u8()) << (8 * i); return v; }
    uint64_t u64() { uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(u8()) << (8 * i); return v; }
    int8_t i8() { return static_cast<int8_t>(u8()); }
    int16_t i16() { return static_cast<int16_t>(u16()); }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    int64_t i64() { return static_cast<int64_t>(u64()); }
    bool boolean() { return u8() != 0; }
    std::string str() {
        uint32_t n = u32();
        if (!ok_ || n > b_.size() - pos_) { ok_ = false; return {}; }
        std::string s(b_.begin() + static_cast<long>(pos_), b_.begin() + static_cast<long>(pos_ + n));
        pos_ += n;
        return s;
    }
    std::vector<uint8_t> bytes() {
        uint32_t n = u32();
        if (!ok_ || n > b_.size() - pos_) { ok_ = false; return {}; }
        std::vector<uint8_t> v(b_.begin() + static_cast<long>(pos_), b_.begin() + static_cast<long>(pos_ + n));
        pos_ += n;
        return v;
    }
    // Marks the stream bad when a count is implausible for the bytes left.
    bool checkCount(uint32_t n, size_t minBytesEach) {
        if (n > (b_.size() - pos_) / std::max<size_t>(1, minBytesEach)) ok_ = false;
        return ok_;
    }

private:
    const std::vector<uint8_t>& b_;
    size_t pos_ = 0;
    bool ok_ = true;
};

SOV_API std::vector<uint8_t> serializeState(const GameState& s);
SOV_API bool deserializeState(ByteReader& r, GameState& s);

// Full save: header, rules checksum, state and command log.
// One command in the save format's encoding (the network carries commands the same way).
SOV_API void encodeCommand(ByteWriter& w, const Command& c);
SOV_API Command decodeCommand(ByteReader& r);

SOV_API std::vector<uint8_t> saveGame(const Game& game);
// A play profile as a small text file, carried between games (leader doc §10, player modelling):
// "sovereign-profile 1" then "key value" lines; unknown keys are skipped.
SOV_API std::string profileToText(const PlayerProfile& profile);
SOV_API bool profileFromText(const std::string& text, PlayerProfile& out);

SOV_API std::unique_ptr<Game> loadGame(const Rules& rules, const std::vector<uint8_t>& bytes, std::string* error);

}  // namespace sov
