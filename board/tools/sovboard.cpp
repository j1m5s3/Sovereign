// sovboard: run the weekly challenge board, submit a save, or print a week's ranking.
//
//   sovboard serve --rules data/rules [--port 7788] [--data board-data]
//   sovboard submit --save FILE --week N --name Alice [--host 127.0.0.1] [--port 7788]
//   sovboard list --week N [--host 127.0.0.1] [--port 7788]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "sovereign/rules.h"
#include "sovereign_board/board.h"
#include "sovereign_board/tcp.h"

using namespace sov;
using namespace sov::board;

namespace {
void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const auto n = in.tellg();
    if (n < 0) return {};
    std::vector<uint8_t> b(static_cast<size_t>(n));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(b.data()), n);
    return b;
}

void printRanking(int32_t week, const std::vector<Entry>& rows) {
    std::printf("week %d\n", week);
    if (rows.empty()) {
        std::printf("(empty)\n");
        return;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        const Entry& e = rows[i];
        std::printf("%zu. %s  %s  turn %d  score %d\n", i + 1, e.name.c_str(), e.goalMet ? "goal met" : "goal missed", e.turn,
                    e.score);
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) {
        std::fprintf(stderr, "usage: sovboard serve|submit|list [options]\n");
        return 2;
    }
    const std::string mode = argv[1];
    std::string rulesDir = "data/rules", host = "127.0.0.1", dataDir = "board-data", savePath, name = "Player";
    int port = 7788, week = -1;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string a = argv[i], v = argv[i + 1];
        if (a == "--rules") rulesDir = v;
        else if (a == "--host") host = v;
        else if (a == "--data") dataDir = v;
        else if (a == "--save") savePath = v;
        else if (a == "--name") name = v;
        else if (a == "--port") port = std::atoi(v.c_str());
        else if (a == "--week") week = std::atoi(v.c_str());
        else {
            std::fprintf(stderr, "unknown option %s\n", a.c_str());
            return 2;
        }
    }
    if (mode == "serve") {
        Rules rules;
        std::string err;
        if (!rules.load({rulesDir}, &err)) {
            std::fprintf(stderr, "cannot load rules: %s\n", err.c_str());
            return 1;
        }
        TcpListener listener(port);
        if (!listener.ok()) {
            std::fprintf(stderr, "cannot listen on %d\n", port);
            return 1;
        }
        Board board(rules, dataDir);
        Server server(board, listener);
        std::printf("challenge board listening on %d\ndata: %s\n", listener.port(), dataDir.c_str());
        for (;;) {
            server.poll();
            sleepMs(5);
        }
    }
    if (mode == "submit") {
        if (savePath.empty() || week < 0) {
            std::fprintf(stderr, "submit needs --save FILE --week N --name NAME\n");
            return 2;
        }
        const std::vector<uint8_t> save = readFile(savePath);
        if (save.empty()) {
            std::fprintf(stderr, "cannot read %s\n", savePath.c_str());
            return 1;
        }
        const SubmitReply r = submitSave(host, port, week, name, save);
        if (!r.accepted) {
            std::fprintf(stderr, "rejected: %s\n", r.error.c_str());
            return 1;
        }
        std::printf("accepted  rank %d  %s  turn %d  score %d\n", r.rank, r.result.goalMet ? "goal met" : "goal missed",
                    r.result.turn, r.result.score);
        return 0;
    }
    if (mode == "list") {
        if (week < 0) {
            std::fprintf(stderr, "list needs --week N\n");
            return 2;
        }
        auto link = tcpConnect(host, port, 10);
        if (!link) {
            std::fprintf(stderr, "cannot connect to %s:%d\n", host.c_str(), port);
            return 1;
        }
        Client c(std::move(link), 10);
        const std::vector<Entry> rows = c.ranking(week);
        if (rows.empty() && !c.connected()) {
            std::fprintf(stderr, "no reply\n");
            return 1;
        }
        printRanking(week, rows);
        return 0;
    }
    std::fprintf(stderr, "usage: sovboard serve|submit|list [options]\n");
    return 2;
}
