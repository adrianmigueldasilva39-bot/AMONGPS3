// source/game.h
#pragma once
#include <vector>
#include <string>
using namespace std;

struct Vec2 { float x, y; };
struct Task { int id; float x; float y; bool done; string nome; int progresso; };
struct Body { float x; float y; int cor; bool reported; };
struct PlayerBean {
    float x, y;
    float vx, vy;
    int cor;
    int id;
    bool vivo;
    bool isImpostor;
    bool isPlayer;
    bool inVent;
    int killCooldown;
    int taskIndex;
    int tasksDone;
    float dir;
    bool moving;
};

class Game {
public:
    bool init();
    void run();
    void shutdown();
private:
    PlayerBean player;
    vector<PlayerBean> bots;
    vector<Task> tasks;
    vector<Body> bodies;
    int state;
    int timer;
    int voteTimer;
    int impostorId;
    int reportedBody;
    int votedOut;
    int tasksTotal;
    int tasksDoneGlobal;
    bool emergencyCalled;
    bool running;
    int frame;
    int padOldCross;
    int padOldTriangle;
    void init_game();
    void update_game();
    void update_player(int crossPressed, int triPressed, int lx, int ly);
    void update_bots();
    void update_impostor();
    void do_task(int idx);
    void kill_player(int botIdx);
    void report_body(int bodyIdx);
    void start_meeting(int bodyIdx);
    void vote_logic();
    void render_game();
    void draw_rect(int x, int y, int w, int h, int cor);
    void draw_circle(int x, int y, int r, int cor);
    void draw_bean(int x, int y, int cor, bool vivo, bool impostor);
    void draw_text(int x, int y, const char* texto);
    void draw_map();
    void draw_hud();
    void draw_meeting();
};