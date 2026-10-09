// The board: checkChallenge, then a ranked list per week on disk.
#include "sovereign_board/board.h"

#include <algorithm>
#include <string>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <utility>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "sovereign/challenge.h"

namespace sov::board {
namespace {

bool makeDir(const std::string& path) {
    if (path.empty() || path == "." || (path.size() == 2 && path[1] == ':')) return true;
#ifdef _WIN32
    const int r = _mkdir(path.c_str());
#else
    const int r = mkdir(path.c_str(), 0755);
#endif
    return r == 0 || errno == EEXIST;
}

void makeDirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        if (c == '/' || c == '\\') makeDir(cur);
        cur += c;
    }
    makeDir(cur);
}

std::string weekPath(const std::string& dir, int32_t week) {
    return dir + "/week-" + std::to_string(week) + ".txt";
}

bool validName(const std::string& n) {
    if (n.empty() || n.size() > kMaxName || n == "week") return false;
    for (unsigned char c : n) {
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

std::vector<Entry> loadWeek(const std::string& path, int32_t week) {
    std::ifstream in(path);
    if (!in) return {};
    std::string line;
    if (!std::getline(in, line)) return {};
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "sovereign-board 1") return {};
    std::vector<Entry> out;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key == "week") {
            int32_t w = 0;
            ss >> w;
            if (w != week) return {};
            continue;
        }
        Entry e;
        e.name = key;
        int goal = 0;
        if (!(ss >> goal >> e.turn >> e.score) || !validName(e.name)) continue;
        e.goalMet = goal != 0;
        out.push_back(std::move(e));
    }
    std::sort(out.begin(), out.end(), better);
    return out;
}

bool saveWeek(const std::string& path, int32_t week, const std::vector<Entry>& rows) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "sovereign-board 1\nweek " << week << "\n";
    for (const Entry& e : rows) {
        out << e.name << ' ' << (e.goalMet ? 1 : 0) << ' ' << e.turn << ' ' << e.score << '\n';
    }
    return static_cast<bool>(out);
}

int32_t rankOf(const std::vector<Entry>& rows, const std::string& name) {
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].name == name) return static_cast<int32_t>(i) + 1;
    }
    return 0;
}

}  // namespace

Board::Board(const Rules& rules, std::string dataDir) : rules_(rules), dataDir_(std::move(dataDir)) {
    makeDirs(dataDir_);
}

SubmitReply Board::submit(int32_t week, std::string name, const std::vector<uint8_t>& save) {
    SubmitReply r;
    if (week < 0) {
        r.error = "not a week";
        return r;
    }
    if (!validName(name)) {
        r.error = "not a valid name";
        return r;
    }
    r.result = checkChallenge(rules_, weeklyChallenge(rules_, week), save);
    if (!r.result.valid) {
        r.error = r.result.error.empty() ? "not a valid challenge save" : r.result.error;
        return r;
    }
    std::vector<Entry> rows = loadWeek(weekPath(dataDir_, week), week);
    Entry e;
    e.name = name;
    e.goalMet = r.result.goalMet;
    e.turn = r.result.turn;
    e.score = r.result.score;
    bool found = false;
    for (Entry& old : rows) {
        if (old.name != name) continue;
        found = true;
        if (better(e, old)) old = e;
        break;
    }
    if (!found) rows.push_back(std::move(e));
    std::sort(rows.begin(), rows.end(), better);
    if (!saveWeek(weekPath(dataDir_, week), week, rows)) {
        r.error = "cannot write the board";
        return r;
    }
    r.accepted = true;
    r.rank = rankOf(rows, name);
    return r;
}

std::vector<Entry> Board::ranking(int32_t week) const { return loadWeek(weekPath(dataDir_, week), week); }

}  // namespace sov::board
