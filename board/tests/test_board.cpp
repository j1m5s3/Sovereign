// The weekly challenge board: accept, reject (wrong week or a tampered save), ranking, and TCP.
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/challenge.h"
#include "sovereign/serialize.h"
#include "sovereign_board/board.h"
#include "sovereign_board/tcp.h"

using namespace sov;
using namespace sov::board;
using sovtest::rules;

namespace {
std::string freshDir(const char* tag) {
    static int n = 0;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return std::string("board_test/") + tag + "_" + std::to_string(ms) + "_" + std::to_string(++n);
}

struct Played {
    std::vector<uint8_t> at6;
    std::vector<uint8_t> at12;
};

const Played& week7() {
    static Played p = [] {
        Played x;
        const Challenge ch = weeklyChallenge(rules(), 7);
        std::string err;
        auto g = Game::create(rules(), ch.setup, &err);
        if (!g) {
            std::printf("cannot create challenge: %s\n", err.c_str());
            std::abort();
        }
        while (g->state().turn < 6 && !g->gameOver()) ai::playTurn(*g);
        x.at6 = saveGame(*g);
        while (g->state().turn < 12 && !g->gameOver()) ai::playTurn(*g);
        x.at12 = saveGame(*g);
        return x;
    }();
    return p;
}

struct Pump {
    std::atomic<bool> run{true};
    Server* server = nullptr;
    std::thread t;
    explicit Pump(Server& s) : server(&s) {
        t = std::thread([this] {
            while (run) {
                server->poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    ~Pump() {
        run = false;
        t.join();
    }
};
}  // namespace

TEST(the_board_accepts_a_valid_challenge_save) {
    Board board(rules(), freshDir("accept"));
    const SubmitReply r = board.submit(7, "Alice", week7().at12);
    CHECK(r.accepted);
    CHECK_EQ(r.error, std::string());
    CHECK(r.result.valid);
    CHECK_EQ(r.rank, 1);
    REQUIRE(board.ranking(7).size() == 1u);
    CHECK_EQ(board.ranking(7)[0].name, std::string("Alice"));
}

TEST(the_board_rejects_a_wrong_week_save) {
    Board board(rules(), freshDir("wrong"));
    const SubmitReply r = board.submit(8, "Alice", week7().at12);
    CHECK(!r.accepted);
    CHECK_EQ(r.error, std::string("not this week's challenge"));
    CHECK(board.ranking(8).empty());
}

TEST(the_board_rejects_a_tampered_save) {
    Board board(rules(), freshDir("tamper"));
    std::vector<uint8_t> bad = week7().at12;
    REQUIRE(!bad.empty());
    bad.back() ^= 0xFFu;
    const SubmitReply r = board.submit(7, "Alice", bad);
    CHECK(!r.accepted);
    CHECK(!r.error.empty());
    CHECK(board.ranking(7).empty());
}

TEST(the_board_ranks_by_goal_then_turns_then_score) {
    const std::string dir = freshDir("rank");
    Board board(rules(), dir);
    std::ofstream out(dir + "/week-9.txt", std::ios::binary);
    out << "sovereign-board 1\nweek 9\n"
        << "Win 1 80 100\n"
        << "Fast 1 40 50\n"
        << "Slow 0 10 999\n"
        << "Mid 1 80 200\n";
    out.close();
    const std::vector<Entry> rows = board.ranking(9);
    REQUIRE(rows.size() == 4u);
    CHECK_EQ(rows[0].name, std::string("Fast"));
    CHECK_EQ(rows[1].name, std::string("Mid"));
    CHECK_EQ(rows[2].name, std::string("Win"));
    CHECK_EQ(rows[3].name, std::string("Slow"));

    const SubmitReply a = board.submit(7, "Alice", week7().at12);
    const SubmitReply b = board.submit(7, "Bob", week7().at6);
    CHECK(a.accepted && b.accepted);
    const std::vector<Entry> live = board.ranking(7);
    REQUIRE(live.size() == 2u);
    CHECK_EQ(live[0].name, std::string("Bob"));  // fewer turns, same missed goal
    CHECK_EQ(live[1].name, std::string("Alice"));
    CHECK_EQ(a.rank, 1);  // Alice was alone when she submitted
    CHECK_EQ(b.rank, 1);
}

TEST(the_board_keeps_the_ranking_on_disk) {
    const std::string dir = freshDir("disk");
    {
        Board board(rules(), dir);
        CHECK(board.submit(7, "Alice", week7().at12).accepted);
        CHECK(board.submit(7, "Alice", week7().at6).accepted);  // better: fewer turns
    }
    Board again(rules(), dir);
    const std::vector<Entry> rows = again.ranking(7);
    REQUIRE(rows.size() == 1u);
    CHECK_EQ(rows[0].name, std::string("Alice"));
    CHECK_EQ(rows[0].turn, checkChallenge(rules(), weeklyChallenge(rules(), 7), week7().at6).turn);
}

TEST(tcp_submit_reaches_the_board) {
    const std::string dir = freshDir("tcp");
    Board board(rules(), dir);
    TcpListener listener(0, "127.0.0.1");
    REQUIRE(listener.ok());
    Server server(board, listener);
    Pump pump(server);
    const SubmitReply ok = submitSave("127.0.0.1", listener.port(), 7, "Ann", week7().at12);
    CHECK(ok.accepted);
    CHECK_EQ(ok.rank, 1);
    const SubmitReply wrong = submitSave("127.0.0.1", listener.port(), 8, "Ann", week7().at12);
    CHECK(!wrong.accepted);
    CHECK_EQ(wrong.error, std::string("not this week's challenge"));
    std::vector<uint8_t> bad = week7().at12;
    bad.back() ^= 0xFFu;
    const SubmitReply tampered = submitSave("127.0.0.1", listener.port(), 7, "Eve", bad);
    CHECK(!tampered.accepted);
    const std::vector<Entry> rows = fetchRanking("127.0.0.1", listener.port(), 7);
    REQUIRE(rows.size() == 1u);
    CHECK_EQ(rows[0].name, std::string("Ann"));
}
