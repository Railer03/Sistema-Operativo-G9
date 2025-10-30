// Single-file multiplayer Pong (server + client) in C++ using SDL2 and POSIX sockets.
// Build with the accompanying Makefile. Run as server: ./pong server
// Run as client: ./pong client <server_ip>
// Requirements implemented:
// - Multiplayer over TCP sockets (real-time messages)
// - Teams with min/max players enforced (configurable)
// - Start condition: at least 2 teams with at least minPlayersPerTeam each
// - End condition: when a team's score >= scoreToWin; points awarded equal to number of players in scoring team
// - Clear textual messages sent to clients for state updates and events
// Single-file requirement satisfied.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <ifaddrs.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <signal.h>
#include <errno.h>

#include <cstdlib>
#include <ctime>

// Attempt to read a simple .env file (KEY=VAL lines). Returns true if any values were loaded into the map.
static bool load_env_file_into_map(const std::string &path, std::map<std::string,std::string> &out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        // strip comments and trim
        auto posc = line.find('#');
        if (posc != std::string::npos) line = line.substr(0, posc);
        // trim
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        std::string s = line.substr(start, end - start + 1);
        if (s.empty()) continue;
        auto eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = s.substr(0, eq);
        std::string val = s.substr(eq+1);
        // trim key and val
        auto trim = [](std::string &x){ size_t a = x.find_first_not_of(" \t\r\n"); if (a==std::string::npos) { x.clear(); return; } size_t b = x.find_last_not_of(" \t\r\n"); x = x.substr(a, b-a+1); };
        trim(key); trim(val);
        // remove optional quotes
        if (val.size() >= 2 && ((val.front()=='"' && val.back()=='"') || (val.front()=='\'' && val.back()=='\''))) {
            val = val.substr(1, val.size()-2);
        }
        if (!key.empty()) out[key] = val;
    }
    return true;
}

// --- simple 7-segment style digit renderer (no SDL_ttf required) ---
// draws a digit in a box of given width/height using filled rectangles for segments
void draw_digit(SDL_Renderer *ren, int x, int y, int w, int h, int digit) {
    // segment thickness relative
    int t = std::max(2, w/8);
    int segW = w - 2*t; // horizontal segment width
    int segH = h/2 - 2*t; // vertical segment height approx
    // coordinates for segments (A:top, B:top-right, C:bottom-right, D:bottom, E:bottom-left, F:top-left, G:middle)
    SDL_Rect A{ x + t, y, segW, t };
    SDL_Rect D{ x + t, y + h - t, segW, t };
    SDL_Rect G{ x + t, y + h/2 - t/2, segW, t };
    SDL_Rect F{ x, y + t, t, h/2 - t - t/2 };
    SDL_Rect B{ x + w - t, y + t, t, h/2 - t - t/2 };
    SDL_Rect E{ x, y + h/2 + t/2, t, h/2 - t - t/2 };
    SDL_Rect C{ x + w - t, y + h/2 + t/2, t, h/2 - t - t/2 };
    // segments on for digits 0-9
    static const bool segs[10][7] = {
        {true,true,true,true,true,true,false}, //0
        {false,true,true,false,false,false,false}, //1
        {true,true,false,true,true,false,true}, //2
        {true,true,true,true,false,false,true}, //3
        {false,true,true,false,false,true,true}, //4
        {true,false,true,true,false,true,true}, //5
        {true,false,true,true,true,true,true}, //6
        {true,true,true,false,false,false,false}, //7
        {true,true,true,true,true,true,true}, //8
        {true,true,true,true,false,true,true} //9
    };
    const bool *on = segs[digit%10];
    if (on[0]) SDL_RenderFillRect(ren, &A);
    if (on[1]) SDL_RenderFillRect(ren, &B);
    if (on[2]) SDL_RenderFillRect(ren, &C);
    if (on[3]) SDL_RenderFillRect(ren, &D);
    if (on[4]) SDL_RenderFillRect(ren, &E);
    if (on[5]) SDL_RenderFillRect(ren, &F);
    if (on[6]) SDL_RenderFillRect(ren, &G);
}

// draw a non-negative integer (may be >9) centered inside a rectangle
void draw_number(SDL_Renderer *ren, int x, int y, int w, int h, int value) {
    if (value < 0) value = 0;
    std::string s = std::to_string(value);
    int digits = (int)s.size();
    int digitW = std::max(12, w / (digits==0?1:digits) - 4);
    int digitH = h - 8;
    int totalW = digits * digitW + (digits-1) * 4;
    int startX = x + (w - totalW)/2;
    for (int i=0;i<digits;i++) {
        int d = s[i]-'0';
        draw_digit(ren, startX + i*(digitW+4), y + 4, digitW, digitH, d);
    }
}


using namespace std::chrono_literals;

// Configuration
const int SERVER_PORT = 9009;
const int TICK_MS = 16; // ~60 FPS server tick
const int WIN_W = 800;
const int WIN_H = 600;

// Game rules (defaults; may be overridden by server mode)
const int maxTeams = 2;
const int minTeams = 2;
const int scoreToWin = 10;

// maximum slots we support per team (compile-time upper bound)
const int MAX_PLAYERS_PER_TEAM = 4;

// runtime-configurable per server mode
int runtimeMinPlayersPerTeam = 2;
int runtimeMaxPlayersPerTeam = 4;
int runtimePaddleSpeed = 300; // pixels per second base
int runtimePaddleW = 20;
int runtimePaddleH = 100;

// Paddle/layout constants (use on both server and client)
const int PADDLE_W = 20;
const int PADDLE_H = 100;
const int PADDLE_MARGIN = 50; // distance from screen edge

struct ClientInfo {
    int sock;
    int id;
    int team; // 0..maxTeams-1
    int slot; // slot index within team (0..runtimeMaxPlayersPerTeam-1)
    int inputDir; // -1 up, 1 down, 0 none
    std::string name;
};

// Server-side state
struct GameState {
    float ballX = WIN_W/2.0f;
    float ballY = WIN_H/2.0f;
    float ballVX = 0.0f; // pixels per second (initialized later from GAME_SPEED)
    float ballVY = 0.0f;
    float paddleY[maxTeams][MAX_PLAYERS_PER_TEAM];
    int paddleH = PADDLE_H;
    int paddleW = PADDLE_W;
    int paddleX[maxTeams];
    int scores[maxTeams];
    bool running = false; // whether game started
} state;

// Game tuning from environment
float initialBallSpeed = 250.0f; // default initial horizontal speed (pixels/sec)
float initialVYRatio = 0.48f;    // ratio of vertical speed to horizontal speed for initial VY
float accelerationFactor = 1.0f; // multiplier applied every accelIntervalSec
std::chrono::steady_clock::time_point lastAccelerationTime;
const std::chrono::seconds accelIntervalSec{10};

// Helper to reset ball to center with given horizontal direction (>0: to right, <0: to left)
void reset_ball(int direction) {
    if (direction == 0) direction = (rand() % 2) ? 1 : -1;
    state.ballX = WIN_W / 2.0f;
    state.ballY = WIN_H / 2.0f;
    float mag = initialBallSpeed;
    state.ballVX = (direction > 0) ? mag : -mag;
    // give VY a small randomized sign and magnitude relative to mag
    float vy = mag * initialVYRatio;
    if ((rand() % 2) == 0) vy = -vy;
    state.ballVY = vy;
    lastAccelerationTime = std::chrono::steady_clock::now();
}

std::mutex clients_m;
std::map<int, ClientInfo> clients; // key: sock
std::atomic<int> nextClientId{1};
std::mutex state_m;
std::atomic<bool> server_running{true};

// Helper: set non-blocking
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Send a text message, newline-terminated
bool send_msg(int sock, const std::string &s) {
    std::string t = s + "\n";
    const char *buf = t.c_str();
    size_t total = 0;
    size_t len = t.size();
    while (total < len) {
        ssize_t n = send(sock, buf + total, len - total, MSG_NOSIGNAL);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket buffer full; treat as non-fatal (message may be dropped)
                return true;
            }
            // fatal error (broken pipe, connection reset, etc.)
            return false;
        }
        // unexpected case
        return false;
    }
    return true;
}

// Broadcast message to all clients
void broadcast(const std::string &msg) {
    std::vector<int> to_drop;
    {
        std::lock_guard<std::mutex> lk(clients_m);
        for (auto &p : clients) {
            if (!send_msg(p.first, msg)) {
                to_drop.push_back(p.first);
            }
        }
        // remove failed sockets
        for (int s : to_drop) {
            if (clients.count(s)) {
                close(s);
                clients.erase(s);
            }
        }
    }
}

// Assign new client to a team with least players (up to max)
// Find a team and slot for a new client. Returns pair(team,slot) or (-1,-1) if none available.
std::pair<int,int> find_team_and_slot() {
    std::vector<int> counts(maxTeams,0);
    std::vector<std::array<bool,MAX_PLAYERS_PER_TEAM>> used(maxTeams);
    for (int t=0;t<maxTeams;t++) for (int s=0;s<MAX_PLAYERS_PER_TEAM;s++) used[t][s]=false;
    std::lock_guard<std::mutex> lk(clients_m);
    for (auto &kv : clients) {
        int t = kv.second.team;
        int s = kv.second.slot;
        if (t>=0 && t<maxTeams && s>=0 && s<MAX_PLAYERS_PER_TEAM) {
            used[t][s] = true;
            counts[t]++;
        }
    }
    // pick the team with the least players but with available slot and respecting runtimeMaxPlayersPerTeam
    int best = -1;
    for (int t=0;t<maxTeams;t++) {
        if (counts[t] < runtimeMaxPlayersPerTeam) {
            if (best == -1 || counts[t] < counts[best]) best = t;
        }
    }
    if (best == -1) return {-1,-1};
    // find first free slot in team 'best'
    for (int s=0;s<runtimeMaxPlayersPerTeam && s<MAX_PLAYERS_PER_TEAM;s++) {
        if (!used[best][s]) return {best,s};
    }
    return {-1,-1};
}

// Check start condition
bool start_condition_met() {
    std::vector<int> counts(maxTeams,0);
    std::lock_guard<std::mutex> lk(clients_m);
    for (auto &kv : clients) counts[kv.second.team]++;
    int teams_ok = 0;
    for (int t=0;t<maxTeams;t++) if (counts[t] >= runtimeMinPlayersPerTeam) teams_ok++;
    return teams_ok >= minTeams;
}

// Server loop handling connections and game simulation
void run_server() {
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt=1; setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return; }
    if (listen(listen_sock, 10) < 0) { perror("listen"); return; }
    set_nonblocking(listen_sock);

    std::cout << "Server listening on port " << SERVER_PORT << "\n";
    // enumerate IPv4 interfaces and print addresses so user knows which IP to use for clients
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        std::cout << "Available IPv4 addresses:" << std::endl;
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                char addrbuf[INET_ADDRSTRLEN];
                struct sockaddr_in *sin = (struct sockaddr_in*)ifa->ifa_addr;
                if (inet_ntop(AF_INET, &sin->sin_addr, addrbuf, sizeof(addrbuf))) {
                    std::cout << "  " << ifa->ifa_name << ": " << addrbuf << std::endl;
                }
            }
        }
        freeifaddrs(ifaddr);
        std::cout << "Use one of the above IPs with: ./pong client <IP>\n";
    } else {
        std::cout << "(could not enumerate local interfaces)" << std::endl;
    }

    // initialize paddles
    for (int t=0;t<maxTeams;t++) {
        // use runtime-configured paddle size
        state.paddleW = runtimePaddleW;
        state.paddleH = runtimePaddleH;
        state.paddleX[t] = (t==0) ? PADDLE_MARGIN : (WIN_W - PADDLE_MARGIN - state.paddleW);
        // initialize per-slot paddle positions: evenly spaced around center
        float spacing = (float)(state.paddleH + 10);
        int slots = runtimeMaxPlayersPerTeam;
        for (int s=0;s<MAX_PLAYERS_PER_TEAM;s++) {
            if (s < slots) {
                float start = WIN_H/2.0f - spacing * (slots-1) / 2.0f;
                state.paddleY[t][s] = start + s * spacing;
            } else {
                state.paddleY[t][s] = WIN_H/2.0f - state.paddleH/2.0f; // default
            }
        }
        state.scores[t]=0;
    }

    // initialize random seed and ball
    srand((unsigned)time(NULL));
    reset_ball((rand()%2)?1:-1);
    auto lastTick = std::chrono::steady_clock::now();
    while (server_running.load()) {
        // Accept new clients
        sockaddr_in cli_addr; socklen_t cli_len = sizeof(cli_addr);
        int c = accept(listen_sock, (sockaddr*)&cli_addr, &cli_len);
        if (c > 0) {
            set_nonblocking(c);
            // attempt to read an optional PREFTEAM <n> sent by the client immediately after connect
            int preferredTeam = -1;
            {
                // use select to wait a short time for initial preference without busy-looping
                fd_set rfds; FD_ZERO(&rfds); FD_SET(c, &rfds);
                struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
                int rv = select(c+1, &rfds, NULL, NULL, &tv);
                if (rv > 0 && FD_ISSET(c, &rfds)) {
                    char prebuf[128]; ssize_t rr = recv(c, prebuf, sizeof(prebuf)-1, 0);
                    if (rr > 0) {
                        prebuf[rr]=0;
                        std::string prestr(prebuf);
                        std::istringstream pis(prestr);
                        std::string line;
                        while (std::getline(pis, line)) {
                            if (line.rfind("PREFTEAM",0)==0) {
                                std::istringstream ls(line);
                                std::string tag; int t; ls>>tag>>t; preferredTeam = t; break;
                            }
                        }
                    }
                }
            }

            // choose the team/slot, honoring preferredTeam if possible
            int team = -1, slot = -1;
            {
                // compute used slots and counts similar to find_team_and_slot
                std::vector<int> counts(maxTeams,0);
                std::vector<std::array<bool,MAX_PLAYERS_PER_TEAM>> used(maxTeams);
                for (int t=0;t<maxTeams;t++) for (int s=0;s<MAX_PLAYERS_PER_TEAM;s++) used[t][s]=false;
                {
                    std::lock_guard<std::mutex> lk(clients_m);
                    for (auto &kv : clients) {
                        int t = kv.second.team;
                        int s = kv.second.slot;
                        if (t>=0 && t<maxTeams && s>=0 && s<MAX_PLAYERS_PER_TEAM) {
                            used[t][s] = true;
                            counts[t]++;
                        }
                    }
                }
                if (preferredTeam >= 0 && preferredTeam < maxTeams) {
                    if (counts[preferredTeam] < runtimeMaxPlayersPerTeam) {
                        team = preferredTeam;
                        // find free slot in that team
                        for (int s=0;s<runtimeMaxPlayersPerTeam && s<MAX_PLAYERS_PER_TEAM;s++) {
                            if (!used[team][s]) { slot = s; break; }
                        }
                    } else {
                        // preferred team is full: reject this connection so the client knows the choice failed
                        send_msg(c, "INFO Team full");
                        close(c);
                        continue; // skip adding this client
                    }
                }
                // fallback: pick least-populated team with available slot
                if (team == -1) {
                    int best = -1;
                    for (int t=0;t<maxTeams;t++) {
                        if (counts[t] < runtimeMaxPlayersPerTeam) {
                            if (best == -1 || counts[t] < counts[best]) best = t;
                        }
                    }
                    if (best != -1) {
                        team = best;
                        for (int s=0;s<runtimeMaxPlayersPerTeam && s<MAX_PLAYERS_PER_TEAM;s++) if (!used[team][s]) { slot = s; break; }
                    }
                }
            }
            if (team < 0) {
                send_msg(c, "INFO Server full or team limits reached");
                close(c);
            } else {
                ClientInfo ci;
                ci.sock = c;
                ci.id = nextClientId++;
                ci.team = team;
                ci.slot = slot;
                ci.inputDir = 0;
                ci.name = "Player" + std::to_string(ci.id);
                {
                    std::lock_guard<std::mutex> lk(clients_m);
                    clients[c] = ci;
                }
                std::stringstream ss; ss<<"INFO Connected as "<<ci.name<<" on team "<<ci.team<<" slot="<<ci.slot;
                send_msg(c, ss.str());
                // tell the client explicitly which team was assigned
                std::stringstream as; as<<"ASSIGNED "<<ci.team;
                send_msg(c, as.str());
                broadcast(std::string("INFO Player joined: ") + ci.name + " team=" + std::to_string(ci.team));
                std::cout << "Client "<<ci.id<<" connected on team "<<team<<" slot "<<slot<<"\n";
            }
        }

        // Read input from clients (collect pending broadcasts and removals while holding the lock,
        // then perform broadcasts and removals after releasing the lock to avoid calling broadcast() while holding clients_m)
    std::vector<int> to_remove;
    std::vector<std::string> pending_msgs;
        {
            std::lock_guard<std::mutex> lk(clients_m);
            for (auto &kv : clients) {
                int sock = kv.first;
                char buf[256];
                ssize_t r = recv(sock, buf, sizeof(buf)-1, 0);
                if (r > 0) {
                    buf[r]=0;
                    std::string in(buf);
                    // parse lines
                    std::istringstream iss(in);
                    std::string line;
                    while (std::getline(iss, line)) {
                        // ignore any shutdown command coming from clients; clients should only disconnect themselves
                        if (line.rfind("INPUT",0) == 0) {
                            int dir = 0;
                            if (line.find("UP")!=std::string::npos) dir=-1;
                            if (line.find("DOWN")!=std::string::npos) dir=1;
                            kv.second.inputDir = dir;
                        } else if (line.rfind("NAME",0)==0) {
                            kv.second.name = line.substr(5);
                        } else if (line.rfind("CHAT",0)==0) {
                            pending_msgs.push_back(std::string("CHAT ") + kv.second.name + ": " + line.substr(5));
                        }
                    }
                } else if (r==0) {
                    // client closed connection; record for removal and queue a leave message
                    std::string name = kv.second.name;
                    to_remove.push_back(sock);
                    pending_msgs.push_back(std::string("INFO Player left: ") + name);
                } else {
                    // r < 0; non-blocking maybe no data
                }
            }
        }
        // perform pending broadcasts outside lock
        for (auto &m : pending_msgs) broadcast(m);
        // now remove closed sockets (acquire lock to modify clients map)
        if (!to_remove.empty()) {
            std::lock_guard<std::mutex> lk(clients_m);
            for (int s : to_remove) {
                if (clients.count(s)) {
                    close(s);
                    clients.erase(s);
                }
            }
        }

        // NOTE: clients disconnect normally (recv==0) and server stays running. No client can trigger global shutdown here.

        // Update game if running or if start condition transitions
        if (!state.running && start_condition_met()) {
            state.running = true;
            broadcast("STATE START");
        }

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> dt = now - lastTick;
        if (dt >= std::chrono::milliseconds(TICK_MS)) {
            float secs = dt.count();
            lastTick = now;

            // Apply inputs per connected client: each player controls their slot's paddle
            {
                std::lock_guard<std::mutex> lk(clients_m);
                for (auto &kv : clients) {
                    int t = kv.second.team;
                    int s = kv.second.slot;
                    int dir = kv.second.inputDir;
                    if (t>=0 && t<maxTeams && s>=0 && s<runtimeMaxPlayersPerTeam && s<MAX_PLAYERS_PER_TEAM) {
                        state.paddleY[t][s] += dir * (float)runtimePaddleSpeed * secs;
                        float maxY = (float)(WIN_H - state.paddleH);
                        if (state.paddleY[t][s] < 0.0f) state.paddleY[t][s] = 0.0f;
                        if (state.paddleY[t][s] > maxY) state.paddleY[t][s] = maxY;
                    }
                }
            }

            // Move ball if running
            if (state.running) {
                state.ballX += state.ballVX * secs;
                state.ballY += state.ballVY * secs;
                // collisions with top/bottom
                if (state.ballY < 0) { state.ballY=0; state.ballVY = -state.ballVY; }
                if (state.ballY > WIN_H) { state.ballY = WIN_H; state.ballVY = -state.ballVY; }
                // paddle collisions
                for (int t=0;t<maxTeams;t++) {
                    int px = state.paddleX[t];
                    for (int s=0;s<runtimeMaxPlayersPerTeam && s<MAX_PLAYERS_PER_TEAM;s++) {
                        int py = (int)state.paddleY[t][s];
                        if (state.ballX >= px && state.ballX <= px + state.paddleW) {
                            if (state.ballY >= py && state.ballY <= py + state.paddleH) {
                                state.ballVX = -state.ballVX; // simple reflect
                                // tweak velocity based on where it hit
                                float rel = ((state.ballY - py) / state.paddleH) - 0.5f;
                                state.ballVY += rel * 200.0f;
                            }
                        }
                    }
                }
                // scoring: ball beyond left or right
                if (state.ballX < 0) {
                    // right team scores
                    int scoringTeam = 1;
                    int playersOnTeam = 0;
                    { std::lock_guard<std::mutex> lk(clients_m); for (auto &kv: clients) if (kv.second.team==scoringTeam) playersOnTeam++; }
                    // In 2v2 mode points are 1 per goal; otherwise award at least 1 (or number of players)
                    int pointsAward = (runtimeMaxPlayersPerTeam >= 2) ? 1 : std::max(1, playersOnTeam);
                    state.scores[scoringTeam] += pointsAward;
                    std::stringstream ss; ss<<"SCORE Team "<<scoringTeam<<" scored "<<pointsAward<<" pts. Tot="<<state.scores[scoringTeam];
                    broadcast(ss.str());
                    // reset ball to the right
                    reset_ball(1);
                } else if (state.ballX > WIN_W) {
                    int scoringTeam = 0;
                    int playersOnTeam = 0;
                    { std::lock_guard<std::mutex> lk(clients_m); for (auto &kv: clients) if (kv.second.team==scoringTeam) playersOnTeam++; }
                    int pointsAward = (runtimeMaxPlayersPerTeam >= 2) ? 1 : std::max(1, playersOnTeam);
                    state.scores[scoringTeam] += pointsAward;
                    std::stringstream ss; ss<<"SCORE Team "<<scoringTeam<<" scored "<<pointsAward<<" pts. Tot="<<state.scores[scoringTeam];
                    broadcast(ss.str());
                    // reset ball to the left
                    reset_ball(-1);
                }
                // check end condition
                for (int t=0;t<maxTeams;t++) {
                    if (state.scores[t] >= scoreToWin) {
                        // announce winner (Red for team 0, Blue for team 1)
                        std::string winner = (t==0) ? "Red" : "Blue";
                        broadcast(std::string("STATE END ") + winner + " Win!");
                        state.running = false;
                        // send countdown TIMER messages for 5 seconds before next match
                        int waitSec = 5;
                        for (int s = waitSec; s >= 0; --s) {
                                if (s == 0) {
                                // At zero: set red team to start at -1 (user request) and others to 0, and reset ball
                                for (int k=0;k<maxTeams;k++) state.scores[k] = (k==0) ? -1 : 0;
                                reset_ball((rand()%2)?1:-1);
                                std::stringstream ts; ts<<"TIMER "<<s;
                                broadcast(ts.str());
                                // broadcast explicit STATE snapshot with zeroed scores and centered ball
                                std::vector<int> counts(maxTeams,0);
                                {
                                    std::lock_guard<std::mutex> lk(clients_m);
                                    for (auto &kv : clients) if (kv.second.team>=0 && kv.second.team<maxTeams) counts[kv.second.team]++;
                                }
                                std::stringstream ss2;
                                ss2<<"STATE ";
                                ss2<<"BALL "<<state.ballX<<" "<<state.ballY<<" ";
                                for (int t=0;t<maxTeams;t++) {
                                    ss2<<"PC"<<t<<":"<<counts[t]<<" ";
                                    for (int si=0;si<runtimeMaxPlayersPerTeam && si<MAX_PLAYERS_PER_TEAM;si++) {
                                        ss2<<"P"<<t<<"_"<<si<<":"<<state.paddleY[t][si]<<" ";
                                    }
                                }
                                for (int t=0;t<maxTeams;t++) ss2<<"S"<<t<<":"<<state.scores[t]<<" ";
                                ss2<<"PW:"<<state.paddleW<<" PH:"<<state.paddleH;
                                broadcast(ss2.str());
                            } else {
                                std::stringstream ts; ts<<"TIMER "<<s;
                                broadcast(ts.str());
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                        // start next match automatically
                        state.running = true;
                        broadcast(std::string("STATE START"));
                        break;
                    }
                }

                // Apply periodic acceleration every accelIntervalSec seconds (multiply magnitudes)
                if (accelerationFactor != 1.0f) {
                    auto nowAcc = std::chrono::steady_clock::now();
                    if (nowAcc - lastAccelerationTime >= accelIntervalSec) {
                        // multiply magnitudes, preserve signs
                        float signX = (state.ballVX >= 0.0f) ? 1.0f : -1.0f;
                        float signY = (state.ballVY >= 0.0f) ? 1.0f : -1.0f;
                        state.ballVX = signX * std::abs(state.ballVX) * accelerationFactor;
                        state.ballVY = signY * std::abs(state.ballVY) * accelerationFactor;
                        lastAccelerationTime = nowAcc;
                        broadcast(std::string("INFO Ball speed increased"));
                    }
                }
            }

            // Broadcast periodic state (include per-slot paddle positions and paddle width/height so clients render correctly)
            {
                // count players per team
                std::vector<int> counts(maxTeams,0);
                {
                    std::lock_guard<std::mutex> lk(clients_m);
                    for (auto &kv : clients) if (kv.second.team>=0 && kv.second.team<maxTeams) counts[kv.second.team]++;
                }
                std::stringstream ss;
                ss<<"STATE ";
                ss<<"BALL "<<state.ballX<<" "<<state.ballY<<" ";
                for (int t=0;t<maxTeams;t++) {
                    ss<<"PC"<<t<<":"<<counts[t]<<" ";
                    for (int s=0;s<runtimeMaxPlayersPerTeam && s<MAX_PLAYERS_PER_TEAM;s++) {
                        ss<<"P"<<t<<"_"<<s<<":"<<state.paddleY[t][s]<<" ";
                    }
                }
                for (int t=0;t<maxTeams;t++) ss<<"S"<<t<<":"<<state.scores[t]<<" ";
                ss<<"PW:"<<state.paddleW<<" PH:"<<state.paddleH;
                broadcast(ss.str());
            }
        }

        std::this_thread::sleep_for(4ms);
    }
    // cleanup on server shutdown: close all client sockets and the listening socket
    {
        std::lock_guard<std::mutex> lk(clients_m);
        for (auto &p : clients) {
            close(p.first);
        }
        clients.clear();
    }
    close(listen_sock);
    std::cout << "Server shutdown complete.\n";
}

// Client: connects to server, runs SDL2 GUI and sends inputs, receives updates
struct RemoteState {
    float ballX = WIN_W/2.0f, ballY = WIN_H/2.0f;
    float paddleY[maxTeams][MAX_PLAYERS_PER_TEAM];
    int scores[maxTeams];
    int playerCount[maxTeams];
    int maxSlotsPerTeam[maxTeams];
    int rPaddleW = PADDLE_W;
    int rPaddleH = PADDLE_H;
    int myTeam = -1; // which team this client was assigned to (0=red,1=blue)
    std::mutex m;
    std::string lastMessage;
    std::chrono::steady_clock::time_point messageTime;
    int nextMatchSeconds = 0;
    SDL_Color winColor{255,255,255,255};
    bool running=false;
} rstate;

// initialize client-side default paddle positions so UI is stable before server updates
struct _RStateInit { _RStateInit() {
    for (int t=0;t<maxTeams;t++) {
        rstate.scores[t]=0;
        rstate.playerCount[t]=0;
        rstate.maxSlotsPerTeam[t]=1; // default to at least 1 slot until server tells us otherwise
        for (int s=0;s<MAX_PLAYERS_PER_TEAM;s++) rstate.paddleY[t][s]= WIN_H/2.0f - PADDLE_H/2.0f;
    }
    rstate.rPaddleW = PADDLE_W; rstate.rPaddleH = PADDLE_H;
} } _rinit;

void client_recv_loop(int sock) {
    char buf[512];
    std::string partial;
    while (true) {
        ssize_t r = recv(sock, buf, sizeof(buf)-1, 0);
        if (r > 0) {
            buf[r]=0; partial += buf;
            std::istringstream iss(partial);
            std::string line;
            std::string rest;
            bool last_incomplete=false;
            while (std::getline(iss, line)) {
                if (!iss.eof()) {
                    // complete line
                    if (line.rfind("STATE",0)==0) {
                        // handle explicit start/end messages first
                        if (line.rfind("STATE START",0)==0) {
                            rstate.running = true;
                            // clear any pending next-match timer when a new match starts
                            rstate.nextMatchSeconds = 0;
                        } else if (line.rfind("STATE END",0)==0) {
                            std::lock_guard<std::mutex> lk(rstate.m);
                            if (line.size() > 6) rstate.lastMessage = line.substr(6); else rstate.lastMessage = "";
                            rstate.messageTime = std::chrono::steady_clock::now();
                            // set win color based on winner mentioned in message
                            if (line.find("Red") != std::string::npos) rstate.winColor = SDL_Color{200,50,50,255};
                            else if (line.find("Blue") != std::string::npos) rstate.winColor = SDL_Color{50,50,200,255};
                            else rstate.winColor = SDL_Color{255,255,255,255};
                            rstate.running = false;
                        }
                        // parse tokens for STATE updates
                        std::istringstream ls(line);
                        std::string tag; ls>>tag;
                        std::string token;
                        while (ls>>token) {
                            if (token=="BALL") { ls>>rstate.ballX>>rstate.ballY; }
                            else if (token.rfind("PC",0)==0) {
                                size_t colon = token.find(':');
                                if (colon!=std::string::npos && token.size()>=3) {
                                    int team = token[2]-'0';
                                    int cnt = std::stoi(token.substr(colon+1));
                                    if (team>=0 && team<maxTeams) rstate.playerCount[team] = cnt;
                                }
                            }
                            else if (token.size()>0 && token[0]=='P' && token.find('_')!=std::string::npos) {
                                size_t colon = token.find(':');
                                if (colon==std::string::npos) continue;
                                std::string left = token.substr(0, colon);
                                std::string valstr = token.substr(colon+1);
                                size_t us = left.find('_');
                                if (us==std::string::npos) continue;
                                int team = std::stoi(left.substr(1, us-1));
                                int slot = std::stoi(left.substr(us+1));
                                if (team>=0 && team<maxTeams && slot>=0 && slot<MAX_PLAYERS_PER_TEAM) {
                                        rstate.paddleY[team][slot] = std::stof(valstr);
                                        // remember how many slots the server reports so we render ghost paddles for empty slots
                                        if (rstate.maxSlotsPerTeam[team] < slot+1) rstate.maxSlotsPerTeam[team] = slot+1;
                                }
                            }
                            else if (token.size()>0 && token[0]=='S' && token.find(':')!=std::string::npos) {
                                int team = token[1]-'0';
                                size_t colon = token.find(':');
                                if (team>=0 && team<maxTeams) rstate.scores[team] = std::stoi(token.substr(colon+1));
                            }
                            else if (token.rfind("PW:",0)==0) { rstate.rPaddleW = std::stoi(token.substr(3)); }
                            else if (token.rfind("PH:",0)==0) { rstate.rPaddleH = std::stoi(token.substr(3)); }
                        }
                    } else if (line.rfind("ASSIGNED",0)==0) {
                        // server explicitly assigned our team
                        std::istringstream ls(line);
                        std::string tag; int teamnum=-1; ls>>tag>>teamnum;
                        std::lock_guard<std::mutex> lk(rstate.m); rstate.myTeam = teamnum;
                    } else if (line.rfind("INFO",0)==0 || line.rfind("CHAT",0)==0 || line.rfind("SCORE",0)==0) {
                        // parse connected message to extract assigned team reliably
                        if (line.rfind("INFO Connected as",0)==0) {
                            // example: "INFO Connected as Player1 on team 0 slot=0"
                            size_t pos = line.find("team");
                            int teamnum = -1;
                            if (pos != std::string::npos) {
                                // find first digit sequence after 'team'
                                size_t i = pos + 4; // after 'team'
                                while (i < line.size() && !isdigit((unsigned char)line[i])) i++;
                                size_t j = i;
                                while (j < line.size() && isdigit((unsigned char)line[j])) j++;
                                if (j > i) {
                                    teamnum = std::stoi(line.substr(i, j-i));
                                }
                            }
                            std::lock_guard<std::mutex> lk(rstate.m);
                            if (teamnum >= 0) rstate.myTeam = teamnum;
                            rstate.lastMessage = line;
                        } else {
                            std::lock_guard<std::mutex> lk(rstate.m); rstate.lastMessage = line;
                        }
                    } else if (line.rfind("TIMER",0)==0) {
                        // server countdown message: 'TIMER <seconds>'
                        std::istringstream ts(line);
                        std::string tag; int secs=0; ts>>tag>>secs;
                        rstate.nextMatchSeconds = secs;
                    } else if (line.rfind("STATE START",0)==0) {
                        rstate.running = true;
                    } else if (line.rfind("STATE END",0)==0) {
                        std::lock_guard<std::mutex> lk(rstate.m); rstate.lastMessage = line;
                        rstate.messageTime = std::chrono::steady_clock::now();
                        rstate.running = false;
                    }
                } else {
                    // incomplete line; save
                    last_incomplete = true;
                    rest = line;
                }
            }
            partial = last_incomplete ? rest : std::string();
        } else if (r==0) {
            std::lock_guard<std::mutex> lk(rstate.m); rstate.lastMessage = "INFO Disconnected from server";
            close(sock); break;
        } else {
            // nothing
            std::this_thread::sleep_for(5ms);
        }
    }
}

// allow optional preferred team (-1 = none)
int run_client(const char *server_ip, int preferredTeam = -1) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) { std::cerr<<"Invalid server IP\n"; return 1; }
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }

    // Note: client no longer sends PREFTEAM; teams are assigned by the server after connect.

    // make socket non-blocking for the recv thread and main loop
    set_nonblocking(sock);

    std::thread recv_t(client_recv_loop, sock);
    recv_t.detach();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr<<"SDL Init failed: "<<SDL_GetError()<<"\n"; return 1; }
    if (TTF_Init() != 0) { std::cerr<<"TTF_Init failed: "<<TTF_GetError()<<"\n"; /* continue, fallback to drawn digits */ }
    SDL_Window *win = SDL_CreateWindow("Multiplayer Pong", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // try to load a font; common font path fallback
    TTF_Font *font = nullptr;
    const char *fontCandidates[] = {"DejaVuSans.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/freefont/FreeSans.ttf"};
    for (auto p : fontCandidates) {
        font = TTF_OpenFont(p, 28);
        if (font) break;
    }
    if (!font) {
        std::cerr<<"Warning: could not load TTF font. Scores will be drawn as shapes fallback. Install a TTF (e.g. libsdl2-ttf-dev) to enable text.\n";
    }

    bool quit=false;
    Uint32 last = SDL_GetTicks();
    // persistent key state so holding a key produces continuous input (no reliance on key repeat)
    bool keyUpPressed = false;
    bool keyDownPressed = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type==SDL_QUIT) {
                // user pressed the window close button: disconnect from server and exit client
                // send a notification so server knows the player left, but DO NOT request server shutdown
                send_msg(sock, "DISCONNECT");
                quit = true;
                break;
            }
            if (e.type==SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_UP) keyUpPressed = true;
                if (e.key.keysym.sym == SDLK_DOWN) keyDownPressed = true;
                if (e.key.keysym.sym == SDLK_ESCAPE) quit=true;
            }
            if (e.type==SDL_KEYUP) {
                if (e.key.keysym.sym == SDLK_UP) keyUpPressed = false;
                if (e.key.keysym.sym == SDLK_DOWN) keyDownPressed = false;
            }
        }
        // derive direction from persistent key state
        int dir = 0;
        if (keyUpPressed && !keyDownPressed) dir = -1;
        else if (keyDownPressed && !keyUpPressed) dir = 1;
        // send input every frame to server
        if (dir < 0) send_msg(sock, "INPUT UP");
        else if (dir > 0) send_msg(sock, "INPUT DOWN");
        else send_msg(sock, "INPUT NONE");

        // responsive render: scale positions/sizes according to current window size
        int curW=WIN_W, curH=WIN_H;
        SDL_GetWindowSize(win, &curW, &curH);
        float sx = curW / (float)WIN_W;
        float sy = curH / (float)WIN_H;
        float scale = std::min(sx, sy); // uniform scale for sizes

    // render
        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); SDL_RenderClear(ren);
        // ball (scale position and size)
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        float bx = rstate.ballX * sx;
        float by = rstate.ballY * sy;
        int ballSize = std::max(4, (int)std::round(12 * scale));
        SDL_Rect ballr{ (int)bx - ballSize/2, (int)by - ballSize/2, ballSize, ballSize };
        SDL_RenderFillRect(ren,&ballr);
    // paddles: draw team 0 (red) on the left, team 1 (blue) on the right (positions scaled)
        int paddleXLeft = (int)std::round(PADDLE_MARGIN * sx);
        int paddleXRight = curW - (int)std::round(PADDLE_MARGIN * sx) - (int)std::round(rstate.rPaddleW * scale);
        // draw left team paddles (red)
        SDL_SetRenderDrawColor(ren, 200,50,50,255);
        for (int s=0;s<rstate.maxSlotsPerTeam[0] && s<MAX_PLAYERS_PER_TEAM;s++) {
            int py = (int)std::round(rstate.paddleY[0][s] * sy);
            int pw = (int)std::round(rstate.rPaddleW * scale);
            int ph = (int)std::round(rstate.rPaddleH * scale);
            SDL_Rect p{paddleXLeft, py, pw, ph}; SDL_RenderFillRect(ren,&p);
        }
        // draw right team paddles (blue)
        SDL_SetRenderDrawColor(ren, 50,50,200,255);
        for (int s=0;s<rstate.maxSlotsPerTeam[1] && s<MAX_PLAYERS_PER_TEAM;s++) {
            int py = (int)std::round(rstate.paddleY[1][s] * sy);
            int pw = (int)std::round(rstate.rPaddleW * scale);
            int ph = (int)std::round(rstate.rPaddleH * scale);
            SDL_Rect p{paddleXRight, py, pw, ph}; SDL_RenderFillRect(ren,&p);
        }

        // scores (render simple grey boxes at top and markers inside them for each point), positions scaled
        SDL_SetRenderDrawColor(ren, 80,80,80,255);
        int boxW = (int)std::round(80 * scale);
        int boxH = (int)std::round(40 * scale);
        SDL_Rect s0{ (int)std::round(curW/2.0f - 100*scale), (int)std::round(20*scale), boxW, boxH }; SDL_RenderFillRect(ren,&s0);
        SDL_Rect s1{ (int)std::round(curW/2.0f + 20*scale), (int)std::round(20*scale), boxW, boxH }; SDL_RenderFillRect(ren,&s1);
        // draw numeric scores inside the top boxes (text if TTF available, else drawn digits fallback)
        if (font) {
            // render text textures for both scores in white; scale destination rect if needed
            SDL_Color white{255,255,255,255};
            std::string t0 = std::to_string(rstate.scores[0]);
            std::string t1 = std::to_string(rstate.scores[1]);
            SDL_Surface *surf0 = TTF_RenderUTF8_Blended(font, t0.c_str(), white);
            SDL_Surface *surf1 = TTF_RenderUTF8_Blended(font, t1.c_str(), white);
            if (surf0) {
                SDL_Texture *tex0 = SDL_CreateTextureFromSurface(ren, surf0);
                int tw=0,th=0; SDL_QueryTexture(tex0, NULL, NULL, &tw, &th);
                // if texture larger than box, scale it down
                float scaleTex = std::min(1.0f, (float)boxW / (float)tw);
                SDL_Rect dst0{ s0.x + (s0.w - (int)(tw*scaleTex))/2, s0.y + (s0.h - (int)(th*scaleTex))/2, (int)(tw*scaleTex), (int)(th*scaleTex) };
                SDL_RenderCopy(ren, tex0, NULL, &dst0);
                SDL_DestroyTexture(tex0);
                SDL_FreeSurface(surf0);
            }
            if (surf1) {
                SDL_Texture *tex1 = SDL_CreateTextureFromSurface(ren, surf1);
                int tw=0,th=0; SDL_QueryTexture(tex1, NULL, NULL, &tw, &th);
                float scaleTex = std::min(1.0f, (float)boxW / (float)tw);
                SDL_Rect dst1{ s1.x + (s1.w - (int)(tw*scaleTex))/2, s1.y + (s1.h - (int)(th*scaleTex))/2, (int)(tw*scaleTex), (int)(th*scaleTex) };
                SDL_RenderCopy(ren, tex1, NULL, &dst1);
                SDL_DestroyTexture(tex1);
                SDL_FreeSurface(surf1);
            }
        } else {
            SDL_SetRenderDrawColor(ren, 255,255,255,255);
            draw_number(ren, s0.x, s0.y, s0.w, s0.h, rstate.scores[0]);
            draw_number(ren, s1.x, s1.y, s1.w, s1.h, rstate.scores[1]);
        }

        // render lastMessage (e.g. "Red Win!" or INFO messages) centered for a short time
        int localNextMatch = 0;
        SDL_Color localWinColor{255,255,255,255};
        std::string localMessage;
        std::chrono::steady_clock::time_point localMessageTime;
        {
            std::lock_guard<std::mutex> lk(rstate.m);
            localNextMatch = rstate.nextMatchSeconds;
            localWinColor = rstate.winColor;
            localMessage = rstate.lastMessage;
            localMessageTime = rstate.messageTime;
        }
        if (!localMessage.empty()) {
            auto age = std::chrono::steady_clock::now() - localMessageTime;
            if (age < std::chrono::milliseconds(2000)) {
                if (font) {
                    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, localMessage.c_str(), localWinColor);
                    if (surf) {
                        SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
                        int tw=0, th=0; SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
                        SDL_Rect dst{ (WIN_W - tw)/2, (WIN_H - th)/2, tw, th };
                        SDL_RenderCopy(ren, tex, NULL, &dst);
                        SDL_DestroyTexture(tex);
                        SDL_FreeSurface(surf);
                    }
                } else {
                    // fallback print
                    static int lastPrinted = 0;
                    int nowms = SDL_GetTicks();
                    if (nowms - lastPrinted > 500) { std::cout<<localMessage<<"\n"; lastPrinted = nowms; }
                }
            } else {
                // clear old message
                std::lock_guard<std::mutex> lk(rstate.m);
                rstate.lastMessage.clear();
            }
        }

        // show which team this client controls (if known)
        int myTeamLocal = -1;
        {
            std::lock_guard<std::mutex> lk(rstate.m);
            myTeamLocal = rstate.myTeam;
        }
        if (myTeamLocal >= 0) {
            std::string you = (myTeamLocal==0) ? "Eres: Rojo" : "Eres: Azul";
            if (font) {
                SDL_Color col = (myTeamLocal==0) ? SDL_Color{200,50,50,255} : SDL_Color{50,50,200,255};
                SDL_Surface *surf = TTF_RenderUTF8_Blended(font, you.c_str(), col);
                if (surf) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
                    int tw=0, th=0; SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
                    SDL_Rect dst{20, 20, tw, th}; SDL_RenderCopy(ren, tex, NULL, &dst);
                    SDL_DestroyTexture(tex); SDL_FreeSurface(surf);
                }
            } else {
                // fallback console hint
                static int printed = 0; if (!printed) { std::cout<<you<<"\n"; printed=1; }
            }
        }

        // render bottom countdown for next match if present
        if (localNextMatch > 0) {
            std::string timerText = std::string("Next match in: ") + std::to_string(localNextMatch);
            if (font) {
                SDL_Color col{220,220,220,255};
                SDL_Surface *surf = TTF_RenderUTF8_Blended(font, timerText.c_str(), col);
                if (surf) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
                    int tw=0, th=0; SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
                    SDL_Rect dst{ (WIN_W - tw)/2, WIN_H - th - 20, tw, th };
                    SDL_RenderCopy(ren, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(surf);
                }
            } else {
                // fallback: draw the number only
                draw_number(ren, WIN_W/2 - 40, WIN_H - 60, 80, 40, localNextMatch);
            }
        }

        SDL_RenderPresent(ren);

        Uint32 cur = SDL_GetTicks();
        Uint32 elapsed = cur - last;
        if (elapsed < (Uint32)TICK_MS) SDL_Delay(TICK_MS - elapsed);
        last = SDL_GetTicks();
    }

    if (font) { TTF_CloseFont(font); }
    TTF_Quit();
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    close(sock);
    return 0;
}

// Simple SDL menu: Hostear / Conectar. Hosted game starts server in-thread and then joins as chosen color.
int run_menu() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr<<"SDL Init failed: "<<SDL_GetError()<<"\n"; return 1; }
    if (TTF_Init() != 0) { std::cerr<<"TTF_Init failed: "<<TTF_GetError()<<"\n"; }
    SDL_Window *win = SDL_CreateWindow("Pong - Menu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font *font = nullptr;
    const char *fontCandidates[] = {"DejaVuSans.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/freefont/FreeSans.ttf"};
    for (auto p : fontCandidates) { font = TTF_OpenFont(p, 20); if (font) break; }

    enum MenuState { MAIN, HOST_SETUP, CONNECT_INPUT } state = MAIN;
    bool quit = false;
    int selectedPlayerCount = 2; // 2 or 4

    std::string ipInput = "127.0.0.1";

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { quit = true; break; }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x, my = e.button.y;
                if (state == MAIN) {
                    // two big buttons
                    SDL_Rect hostb{120,150,160,60}; SDL_Rect connb{360,150,160,60};
                    if (mx>=hostb.x && mx<=hostb.x+hostb.w && my>=hostb.y && my<=hostb.y+hostb.h) { state = HOST_SETUP; }
                    if (mx>=connb.x && mx<=connb.x+connb.w && my>=connb.y && my<=connb.y+connb.h) { state = CONNECT_INPUT; }
                } else if (state == HOST_SETUP) {
                    // player count buttons
                    SDL_Rect b2{120,120,120,50}; SDL_Rect b4{260,120,120,50};
                    if (mx>=b2.x && mx<=b2.x+b2.w && my>=b2.y && my<=b2.y+b2.h) { selectedPlayerCount = 2; }
                    if (mx>=b4.x && mx<=b4.x+b4.w && my>=b4.y && my<=b4.y+b4.h) { selectedPlayerCount = 4; }
                    // Start button
                    SDL_Rect startb{240,320,160,60};
                    if (mx>=startb.x && mx<=startb.x+startb.w && my>=startb.y && my<=startb.y+startb.h) {
                        // set runtime options based on selectedPlayerCount
                        if (selectedPlayerCount == 2) {
                            runtimeMinPlayersPerTeam = 1; runtimeMaxPlayersPerTeam = 1; runtimePaddleSpeed = 300*2; runtimePaddleH = PADDLE_H;
                        } else {
                            runtimeMinPlayersPerTeam = 2; runtimeMaxPlayersPerTeam = 2; runtimePaddleSpeed = 300*2; runtimePaddleH = std::max(4, PADDLE_H/2);
                        }
                        // spawn server thread
                        std::thread server_thread([](){ run_server(); });
                        server_thread.detach();
                        // small delay to let server start listening
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));

                        // cleanup menu SDL so run_client can init its own SDL
                        if (font) { TTF_CloseFont(font); font = nullptr; }
                        TTF_Quit(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();

                        // join local server (no explicit preferred team)
                        return run_client("127.0.0.1");
                    }
                } else if (state == CONNECT_INPUT) {
                    // Connect button
                    SDL_Rect connectb{240,300,160,60};
                    if (mx>=connectb.x && mx<=connectb.x+connectb.w && my>=connectb.y && my<=connectb.y+connectb.h) {
                        // cleanup menu SDL
                        if (font) { TTF_CloseFont(font); font = nullptr; }
                        TTF_Quit(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
                        return run_client(ipInput.c_str());
                    }
                }
            }
            if (state == CONNECT_INPUT && e.type == SDL_TEXTINPUT) {
                ipInput += e.text.text;
            }
            if (state == CONNECT_INPUT && e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && !ipInput.empty()) ipInput.pop_back();
                if (e.key.keysym.sym == SDLK_RETURN) { if (font) { TTF_CloseFont(font); font=nullptr; } TTF_Quit(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return run_client(ipInput.c_str()); }
            }
        }

        // render UI
        SDL_SetRenderDrawColor(ren, 20,20,20,255); SDL_RenderClear(ren);
        if (state == MAIN) {
            // Title
            if (font) {
                SDL_Color white{230,230,230,255};
                SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Menu Principal", white);
                if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{(640-tw)/2,40,tw,th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); }
            }
            // Hostear button
            SDL_Rect hostb{120,150,160,60}; SDL_SetRenderDrawColor(ren, 80,150,80,255); SDL_RenderFillRect(ren,&hostb);
            SDL_SetRenderDrawColor(ren, 200,200,200,255);
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Hostear", SDL_Color{255,255,255,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{hostb.x + (hostb.w-tw)/2, hostb.y + (hostb.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            // Conectar button
            SDL_Rect connb{360,150,160,60}; SDL_SetRenderDrawColor(ren, 80,120,200,255); SDL_RenderFillRect(ren,&connb);
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Conectar", SDL_Color{255,255,255,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{connb.x + (connb.w-tw)/2, connb.y + (connb.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
        } else if (state == HOST_SETUP) {
            // Player count selection
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Selecciona cantidad de jugadores:", SDL_Color{220,220,220,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{120,60,tw,th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            SDL_Rect b2{120,120,120,50}; SDL_Rect b4{260,120,120,50}; SDL_SetRenderDrawColor(ren, selectedPlayerCount==2?SDL_Color{100,200,100,255}.r:100, selectedPlayerCount==2?SDL_Color{100,200,100,255}.g:100,100,255); SDL_RenderFillRect(ren,&b2); SDL_SetRenderDrawColor(ren, selectedPlayerCount==4?SDL_Color{100,200,100,255}.r:100, selectedPlayerCount==4?SDL_Color{100,200,100,255}.g:100,100,255); SDL_RenderFillRect(ren,&b4);
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "2 (1vs1)", SDL_Color{255,255,255,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{b2.x + (b2.w-tw)/2, b2.y + (b2.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "4 (2vs2)", SDL_Color{255,255,255,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{b4.x + (b4.w-tw)/2, b4.y + (b4.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            // label cantidad maxima/minima de equipo: 2
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "cantidad maxima/minima de equipo: 2", SDL_Color{200,200,200,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{120,190,tw,th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            // Teams will be assigned automatically by the server; no color selection here.
            // Start button
            SDL_Rect startb{240,320,160,60}; SDL_SetRenderDrawColor(ren, 100,220,170,255); SDL_RenderFillRect(ren,&startb);
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Iniciar y Hostear", SDL_Color{0,0,0,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{startb.x + (startb.w-tw)/2, startb.y + (startb.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
        } else if (state == CONNECT_INPUT) {
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Ingresa IP del servidor:", SDL_Color{220,220,220,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{120,120,tw,th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            // draw input box
            SDL_Rect box{120,160,400,50}; SDL_SetRenderDrawColor(ren, 40,40,40,255); SDL_RenderFillRect(ren,&box);
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, ipInput.c_str(), SDL_Color{255,255,255,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{box.x+8, box.y + (box.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
            SDL_Rect connectb{240,300,160,60}; SDL_SetRenderDrawColor(ren, 80,160,220,255); SDL_RenderFillRect(ren,&connectb);
            if (font) { SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Conectar", SDL_Color{255,255,255,255}); if (s) { SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s); int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th); SDL_Rect dst{connectb.x + (connectb.w-tw)/2, connectb.y + (connectb.h-th)/2, tw, th}; SDL_RenderCopy(ren,t,NULL,&dst); SDL_DestroyTexture(t); SDL_FreeSurface(s); } }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (font) TTF_CloseFont(font);
    TTF_Quit(); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
int main(int argc, char **argv) {
    // avoid process termination when writing to a closed socket
    signal(SIGPIPE, SIG_IGN);
    // read optional game tuning from environment
    const char *gs = getenv("GAME_SPEED");
    if (gs) {
        try { initialBallSpeed = std::stof(std::string(gs)); } catch(...) { /* ignore parse errors */ }
    }
    const char *ga = getenv("GAME_ACCELERATION");
    if (ga) {
        try { accelerationFactor = std::stof(std::string(ga)); } catch(...) { /* ignore parse errors */ }
    }
    if (argc < 2) {
        // no args: show graphical menu
        return run_menu();
    }
    std::string mode = argv[1];
    if (mode == "server") {
        // optional server mode: './pong server 1' for 1v1, './pong server 2' for 2v2
        int serverMode = 1;
        if (argc >= 3) serverMode = atoi(argv[2]);
        if (serverMode == 1) {
            // 1v1: needs 2 players (1 per team). Paddle speed = double.
            runtimeMinPlayersPerTeam = 1;
            runtimeMaxPlayersPerTeam = 1;
            runtimePaddleSpeed = 300 * 2; // double speed
            runtimePaddleW = PADDLE_W;
            runtimePaddleH = PADDLE_H;
        } else if (serverMode == 2) {
            // 2v2: needs 4 players (2 per team). Paddle speed = double, size = half.
            runtimeMinPlayersPerTeam = 2;
            runtimeMaxPlayersPerTeam = 2;
            runtimePaddleSpeed = 300 * 2; // double speed
            runtimePaddleW = PADDLE_W;
            runtimePaddleH = std::max(4, PADDLE_H / 2); // avoid zero
        } else {
            std::cout<<"Unknown server mode "<<serverMode<<"; defaulting to mode 1\n";
            runtimeMinPlayersPerTeam = 1;
            runtimeMaxPlayersPerTeam = 1;
            runtimePaddleSpeed = 300 * 2;
            runtimePaddleW = PADDLE_W;
            runtimePaddleH = PADDLE_H;
        }
        std::cout<<"Server mode "<<serverMode<<" running: minPerTeam="<<runtimeMinPlayersPerTeam<<" maxPerTeam="<<runtimeMaxPlayersPerTeam<<" paddleSpeed="<<runtimePaddleSpeed<<" paddleH="<<runtimePaddleH<<"\n";
        run_server();
    } else if (mode == "client") {
        const char *ip = "127.0.0.1";
        if (argc >= 3) ip = argv[2];
        return run_client(ip);
    } else {
        std::cout<<"Unknown mode: "<<mode<<"\n";
    }
    return 0;
}
