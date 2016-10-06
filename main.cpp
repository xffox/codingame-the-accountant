#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cassert>
#include <unordered_map>
#include <functional>
#include <utility>
#include <cstddef>
#include <unordered_set>
#include <list>
#include <numeric>
#include <chrono>

using namespace std;

/**
 * Shoot enemies before they collect all the incriminating data!
 * The closer you are to an enemy, the more damage you do but don't get too close or you'll get killed.
 **/
namespace
{
    struct Point
    {
        int x;
        int y;
    };
    inline bool operator==(const Point &left, const Point &right)
    {
        return left.x == right.x && left.y == right.y;
    }
    inline bool operator!=(const Point &left, const Point &right)
    {
        return !(left == right);
    }

    struct Enemy
    {
        int id;
        int life;
        Point pos;
    };
    inline bool operator==(const Enemy &left, const Enemy &right)
    {
        return left.id == right.id && left.life == right.life &&
            left.pos == right.pos;
    }
    inline bool operator!=(const Enemy &left, const Enemy &right)
    {
        return !(left == right);
    }

    struct DataPoint
    {
        int id;
        Point pos;
    };
    inline bool operator==(const DataPoint &left, const DataPoint &right)
    {
        return left.id == right.id && left.pos == right.pos;
    }
    inline bool operator!=(const DataPoint &left, const DataPoint &right)
    {
        return !(left == right);
    }

    struct Player
    {
        Point pos;
    };

    struct Cmd
    {
        enum Type
        {
            TYPE_MOVE,
            TYPE_SHOOT
        };
        union Data
        {
            int id;
            Point pos;
        };
        Type type;
        Data data;
    };

    using PointCol = vector<Point>;
    using EnemyCol = vector<Enemy>;
    using DataPointCol = vector<DataPoint>;
    using Clock = chrono::steady_clock;

    struct World
    {
        Player player;
        DataPointCol dataPoints;
        EnemyCol enemies;
    };

    struct Vect
    {
        double x;
        double y;

        Vect &operator+=(const Vect &that)
        {
            x += that.x;
            y += that.y;
            return *this;
        }
    };
    Vect operator+(const Vect &left, const Vect &right)
    {
        Vect res(left);
        // TODO: will it be moved?
        return res += right;
    }
    using VectCol = vector<Vect>;

    namespace geom
    {
        double dist(const Point &left, const Point &right)
        {
            const double dx = left.x - right.x;
            const double dy = left.y - right.y;
            return sqrt(dx*dx + dy*dy);
        }

        Vect norm(const Vect &v)
        {
            const auto d = sqrt(v.x*v.x + v.y*v.y);
            if(d != 0.0)
            {
                return Vect{v.x/d, v.y/d};
            }
            return v;
        }

        Vect orthogonal(const Vect &v)
        {
            return Vect{v.y, -v.x};
        }

        Vect normDirection(const Point &begin, const Point &end)
        {
            const double dx = end.x - begin.x;
            const double dy = end.y - begin.y;
            return norm(Vect{dx, dy});
        }

        Vect mult(const Vect &v, double m)
        {
            return Vect{v.x*m, v.y*m};
        }

        Point add(const Point &p, const Vect &v)
        {
            return Point{
                static_cast<int>(p.x + v.x),
                static_cast<int>(p.y + v.y)
            };
        }
    }

    namespace game
    {
        const int PLAYER_STEP_DIST = 1000;
        const int DEATH_DIST = 2000;
        const int ENEMY_STEP_DIST = 500;
        constexpr Point ZONE{16000, 9000};
        constexpr chrono::milliseconds TIME_LIMIT(100);
    }

    struct Criteria
    {
        size_t shotsFired;
        size_t alivePoints;
        size_t aliveEnemies;
        int totalDamage;
    };
    static bool operator<(const Criteria &left, const Criteria &right)
    {
        return (left.alivePoints < right.alivePoints ||
            (left.alivePoints == right.alivePoints &&
             (left.aliveEnemies > right.aliveEnemies ||
              (left.aliveEnemies == right.aliveEnemies &&
               (left.shotsFired > right.shotsFired ||
                (left.shotsFired == right.shotsFired &&
                 left.totalDamage < right.totalDamage))))));
    }
    ostream &operator<<(ostream &stream, const Criteria &c)
    {
        return stream<<"{shots="<<c.shotsFired
            <<",points="<<c.alivePoints
            <<",enemies="<<c.aliveEnemies
            <<",damage="<<c.totalDamage
            <<'}';
    }

    ostream &operator<<(ostream &stream, const Point &p)
    {
        return stream<<'('<<p.x<<','<<p.y<<')';
    }

    ostream &operator<<(ostream &stream, const Enemy &enemy)
    {
        return stream<<"{id="<<enemy.id<<",life="<<enemy.life<<",pos="
            <<enemy.pos<<endl;
    }

    ostream &operator<<(ostream &stream, const DataPoint &point)
    {
        return stream<<"{id="<<point.id<<",pos="<<point.pos<<endl;
    }

    pair<World, bool> eval(const World &world, const Cmd &cmd)
    {
        World nextWorld(world);
        using IdSet = unordered_set<int>;
        using PointEnemiesMap = unordered_map<int, IdSet>;
        PointEnemiesMap pointEnemies;
        if(!world.dataPoints.empty())
        {
            for(size_t i = 0; i < world.enemies.size(); ++i)
            {
                const auto &e = world.enemies[i];
                auto closestPointIter = min_element(
                    world.dataPoints.begin(), world.dataPoints.end(),
                    [&e](const DataPointCol::value_type &left,
                        const DataPointCol::value_type &right) {
                    const auto leftDist = geom::dist(left.pos, e.pos);
                    const auto rightDist = geom::dist(right.pos, e.pos);
                    return leftDist < rightDist ||
                    (leftDist == rightDist && left.id < right.id);
                    });
                assert(closestPointIter != world.dataPoints.end());
                if(geom::dist(e.pos, closestPointIter->pos) <= game::ENEMY_STEP_DIST)
                {
                    nextWorld.enemies[i].pos = closestPointIter->pos;
                    pointEnemies[closestPointIter->id].insert(e.id);
                }
                else
                {
                    const auto direction = geom::normDirection(
                        e.pos, closestPointIter->pos);
                    nextWorld.enemies[i].pos = geom::add(nextWorld.enemies[i].pos,
                        geom::mult(direction, game::ENEMY_STEP_DIST));
                }
            }
        }
        if(cmd.type == Cmd::TYPE_MOVE)
        {
            if(geom::dist(nextWorld.player.pos, cmd.data.pos) >
                game::PLAYER_STEP_DIST)
            {
                const auto direction = geom::normDirection(
                    nextWorld.player.pos, cmd.data.pos);
                const auto directedStep = geom::mult(direction, game::PLAYER_STEP_DIST);
                nextWorld.player.pos = geom::add(nextWorld.player.pos,
                    directedStep);
            }
            else
            {
                nextWorld.player.pos = cmd.data.pos;
            }
            if(nextWorld.player.pos.x < 0 ||
                nextWorld.player.pos.x > game::ZONE.x)
            {
                nextWorld.player.pos.x =
                    min(max(0, nextWorld.player.pos.x), game::ZONE.x);
            }
            if(nextWorld.player.pos.y < 0 ||
                nextWorld.player.pos.y > game::ZONE.y)
            {
                nextWorld.player.pos.y =
                    min(max(0, nextWorld.player.pos.y), game::ZONE.y);
            }
        }
        for(const auto &e : nextWorld.enemies)
        {
            // TODO: floating equality
            if(geom::dist(nextWorld.player.pos, e.pos) < game::DEATH_DIST)
            {
                return make_pair(nextWorld, false);
            }
        }
        int deadEnemyId = -1;
        if(cmd.type == Cmd::TYPE_SHOOT)
        {
            // TODO: optimize
            auto iter = find_if(nextWorld.enemies.begin(),
                nextWorld.enemies.end(),
                [&cmd](const EnemyCol::value_type &v) {
                return v.id == cmd.data.id;
                });
            assert(iter != nextWorld.enemies.end());
            const int damage = round(125000.0/
                pow(geom::dist(nextWorld.player.pos, iter->pos), 1.2));
            if(iter->life > damage)
            {
                iter->life -= damage;
            }
            else
            {
                deadEnemyId = iter->id;
                nextWorld.enemies.erase(iter);
            }
        }
        nextWorld.dataPoints.clear();
        for(const auto &p : world.dataPoints)
        {
            auto iter = pointEnemies.find(p.id);
            if(iter != pointEnemies.end() && deadEnemyId != -1)
                iter->second.erase(deadEnemyId);
            if(iter == pointEnemies.end() || iter->second.empty())
            {
                nextWorld.dataPoints.push_back(p);
            }
        }
        return make_pair(move(nextWorld), true);
    }

    class Logic
    {
    public:
        Cmd step(const World &world)
        {
            cerr<<"trying optimized step"<<endl;
            const auto optRes = optimize(world);
            if(optRes.second)
            {
                return optRes.first;
            }
            cerr<<"no optimized solution"<<endl;
            const auto runRes = selectRunPosition(world);
            if(runRes.second)
            {
                const auto &target = runRes.first;
                cerr<<"running from enemies: ("<<target.x<<','<<target.y<<')'<<endl;
                Cmd::Data data;
                data.pos = target;
                const Cmd cmd{Cmd::TYPE_MOVE, data};
                return cmd;
            }
            cerr<<"no solution, staying"<<endl;
            Cmd::Data data;
            data.pos = world.player.pos;
            const Cmd cmd{Cmd::TYPE_MOVE, data};
            return cmd;
        }

    private:
        struct PosId
        {
            int id;
            Point pos;
            Point prev;
        };
        using PosIdCol = vector<PosId>;

        static pair<Cmd, bool> optimize(const World &world)
        {
            struct State
            {
                World world;
                size_t shotsFired;
                int totalDamage;
            };
            struct Candidate
            {
                Cmd initialCommand;
                Cmd currentCommand;
                State state;
            };
            using CandidateList = list<Candidate>;
            CandidateList candidates;
            {
                Cmd::Data moveCmdData;
                moveCmdData.pos = selectPosition(world);
                if(insideGameZone(moveCmdData.pos))
                {
                    Cmd moveCmd{Cmd::TYPE_MOVE, moveCmdData};
                    candidates.push_back(
                        Candidate{moveCmd, moveCmd,
                        State{world, 0, 0}});
                }
            }
            if(!world.enemies.empty())
            {
                const auto enemyId = selectEnemy(world);
                const auto closestEnemyId = selectClosestEnemy(world);
                Cmd::Data shootCmdData;
                shootCmdData.id = enemyId;
                {
                    const Cmd shootCmd{Cmd::TYPE_SHOOT, shootCmdData};
                    candidates.push_back(
                        Candidate{shootCmd, shootCmd,
                        State{world, 0, 0}});
                }
                if(enemyId != closestEnemyId)
                {
                    shootCmdData.id = closestEnemyId;
                    const Cmd shootCmd{Cmd::TYPE_SHOOT, shootCmdData};
                    candidates.push_back(
                        Candidate{shootCmd, shootCmd,
                        State{world, 0, 0}});
                }
            }
            {
                const auto runRes = selectRunPosition(world);
                if(runRes.second && insideGameZone(runRes.first))
                {
                    Cmd::Data runCmdData;
                    runCmdData.pos = runRes.first;
                    Cmd runCmd{Cmd::TYPE_MOVE, runCmdData};
                    candidates.push_back(Candidate{
                        runCmd, runCmd, State{world, 0, 0}});
                }
            }
            Cmd::Data d;
            d.pos = world.player.pos;
            auto bestResult = make_pair(Cmd{Cmd::TYPE_MOVE, d},
                Criteria{numeric_limits<size_t>::max(), 0,
                numeric_limits<size_t>::max(), 0});
            bool foundSol = false;
            const auto timeLimit = game::TIME_LIMIT*0.8;
            const auto beginTime = Clock::now();
            size_t depth = 1;
            for(;; ++depth)
            {
                CandidateList nextCandidates;
                bool curFoundSol = false;
                auto curBestResult = make_pair(Cmd{Cmd::TYPE_MOVE, d},
                    Criteria{numeric_limits<size_t>::max(), 0,
                    numeric_limits<size_t>::max(), 0});
                bool timeout = false;
                if(candidates.empty())
                    break;
                for(const auto &cur : candidates)
                {
                    if(Clock::now() - beginTime >= timeLimit)
                    {
                        timeout = true;
                        break;
                    }
                    const auto &cmd = cur.currentCommand;
                    if(cmd.type == Cmd::TYPE_MOVE)
                    {
                        if(!insideGameZone(cmd.data.pos))
                            continue;
                    }
                    const auto w = eval(cur.state.world, cur.currentCommand);
                    if(Clock::now() - beginTime >= timeLimit)
                    {
                        timeout = true;
                        break;
                    }
                    if(w.second)
                    {
                        int totalDamage = cur.state.totalDamage;
                        size_t shotsFired = cur.state.shotsFired;
                        if(cmd.type == Cmd::TYPE_SHOOT)
                        {
                            totalDamage += enemiesLifes(cur.state.world) -
                                enemiesLifes(w.first);
                            shotsFired += 1;
                        }
                        const State nextState{w.first, shotsFired, totalDamage};
                        Criteria currentCriteria{shotsFired,
                            w.first.dataPoints.size(), w.first.enemies.size(),
                            totalDamage};
                        if(curBestResult.second < currentCriteria)
                        {
                            curBestResult = make_pair(cur.initialCommand, currentCriteria);
                            curFoundSol = true;
                        }
                        {
                            Cmd::Data moveCmdData;
                            moveCmdData.pos = selectPosition(w.first);
                            Cmd moveCmd{Cmd::TYPE_MOVE, moveCmdData};
                            nextCandidates.push_back(Candidate{
                                cur.initialCommand, moveCmd, nextState});
                        }
                        if(!w.first.enemies.empty())
                        {
                            const auto enemyId = selectEnemy(w.first);
                            const auto closestEnemyId =
                                selectClosestEnemy(w.first);
                            Cmd::Data shootCmdData;
                            shootCmdData.id = enemyId;
                            nextCandidates.push_back(Candidate{
                                cur.initialCommand, Cmd{Cmd::TYPE_SHOOT, shootCmdData},
                                nextState});
                            if(enemyId != closestEnemyId)
                            {
                                shootCmdData.id = closestEnemyId;
                                nextCandidates.push_back(Candidate{
                                    cur.initialCommand, Cmd{Cmd::TYPE_SHOOT, shootCmdData},
                                    nextState});
                            }
                        }
                        {
                            Cmd::Data runCmdData;
                            const auto runPosRes = selectRunPosition(w.first);
                            if(runPosRes.second)
                            {
                                runCmdData.pos = runPosRes.first;
                                Cmd runCmd{Cmd::TYPE_MOVE, runCmdData};
                                nextCandidates.push_back(Candidate{
                                    cur.initialCommand, runCmd, nextState});
                            }
                        }
                    }
                }
                if(!timeout)
                {
                    candidates = move(nextCandidates);
                    bestResult = move(curBestResult);
                    foundSol = curFoundSol;
                }
                else
                {
                    break;
                }
            }
            cerr<<"simulation: depth="<<depth<<" time="
                <<chrono::duration_cast<chrono::milliseconds>(Clock::now()-beginTime).count()<<endl;
            if(foundSol)
            {
                cerr<<"optimized solution end result: "<<bestResult.second<<endl;
                return make_pair(bestResult.first, true);
            }
            else
            {
                cerr<<"no optimized result"<<endl;
                return make_pair(bestResult.first, false);
            }
        }

        static int enemiesLifes(const World &w)
        {
            return accumulate(w.enemies.begin(), w.enemies.end(), 0,
                [](int r, const EnemyCol::value_type &v) {
                    return r + v.life;
                });
        }

        static bool insideGameZone(const Point &p)
        {
            return p.x >= 0 && p.x <= game::ZONE.x &&
                p.y >= 0 && p.y <= game::ZONE.y;
        }

        static int selectEnemy(const World &w)
        {
            const auto &enemies = w.enemies;
            const auto &points = w.dataPoints;
            assert(!enemies.empty());
            double pointEnemyMinDist = numeric_limits<double>::max();
            int pointClosestEnemy = enemies.front().id;
            for(const auto &e : enemies)
            {
                for(const auto &p : points)
                {
                    const auto pointDist = geom::dist(p.pos, e.pos);
                    if(pointDist < pointEnemyMinDist)
                    {
                        pointEnemyMinDist = pointDist;
                        pointClosestEnemy = e.id;
                    }
                }
            }
            return pointClosestEnemy;
        }

        static int selectClosestEnemy(const World &w)
        {
            const auto &enemies = w.enemies;
            assert(!enemies.empty());
            double enemyMinDist = numeric_limits<double>::max();
            int closestEnemy = enemies.front().id;
            for(const auto &e : enemies)
            {
                const auto d = geom::dist(w.player.pos, e.pos);
                if(d < enemyMinDist)
                {
                    enemyMinDist = d;
                    closestEnemy = e.id;
                }
            }
            return closestEnemy;
        }

        static Point selectPosition(const World &w)
        {
            const auto &enemies = w.enemies;
            if(!enemies.empty())
            {
                long long int avgX = 0;
                long long int avgY = 0;
                for(const auto &p : enemies)
                {
                    avgX += p.pos.x;
                    avgY += p.pos.y;
                }
                avgX /= enemies.size();
                avgY /= enemies.size();
                const Point target{static_cast<int>(avgX),
                    static_cast<int>(avgY)};
                return target;
            }
            else
            {
                return w.player.pos;
            }
        }

        static pair<Point, bool> selectRunPosition(const World &w)
        {
            const auto &enemies = w.enemies;
            const auto &player = w.player;
            PointCol deathPoints;
            for(const auto &p : enemies)
            {
                // TODO: proper equality comparison for floating point
                if(geom::dist(p.pos, player.pos) < game::DEATH_DIST+game::DEATH_DIST)
                {
                    deathPoints.push_back(p.pos);
                }
            }
            if(!deathPoints.empty())
            {
                if(player.pos.x <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                    deathPoints.push_back(Point{0, player.pos.y});
                if(game::ZONE.x - player.pos.x <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                    deathPoints.push_back(Point{game::ZONE.x, player.pos.y});
                if(player.pos.y <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                    deathPoints.push_back(Point{player.pos.x, 0});
                if(game::ZONE.y - player.pos.y <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                    deathPoints.push_back(Point{player.pos.x, game::ZONE.y});
                Vect runDir{0.0, 0.0};
                VectCol deathVectors;
                for(const auto &p : deathPoints)
                {
                    const auto v = geom::normDirection(p, player.pos);
                    runDir += v;
                    deathVectors.push_back(v);
                }
                // TODO: proper equality check
                if(abs(runDir.x) <= 0.01 && abs(runDir.y) <= 0.01)
                {
                    double maxValue = 0.0;
                    size_t maxI = 0;
                    size_t maxJ = 1;
                    for(size_t i = 0; i < deathVectors.size(); ++i)
                    {
                        for(size_t j = i+1; j < deathVectors.size(); ++j)
                        {
                            const auto &v1 = deathVectors[i];
                            const auto &v2 = deathVectors[j];
                            // normalized vectors
                            const double cosAngle = v1.x*v2.x + v1.y+v2.y;
                            const double sinHalfAngle = sqrt((1-cosAngle)/2.0);
                            if(sinHalfAngle > maxValue)
                            {
                                maxValue = sinHalfAngle;
                                maxI = i;
                                maxJ = j;
                            }
                        }
                    }
                    const auto dir = deathVectors[maxI] + deathVectors[maxJ];
                    if(abs(dir.x) <= 0.01 && abs(dir.y) <= 0.01)
                    {
                        runDir = geom::orthogonal(deathVectors.front());
                    }
                    else
                    {
                        runDir = geom::norm(dir);
                    }
                }
                else
                {
                    runDir = geom::norm(runDir);
                }
                const auto target = geom::add(player.pos, geom::mult(runDir,
                        game::PLAYER_STEP_DIST));
                return make_pair(target, true);
            }
            return make_pair(Point{0, 0}, false);
        }
    };

    void printWorldComparison(ostream &stream, const World &actual,
        const World &predicted)
    {
        if(predicted.player.pos != actual.player.pos)
        {
            stream<<"player prediction error: pred="
                <<predicted.player.pos
                <<" actual="<<actual.player.pos<<endl;
        }
        if(predicted.enemies.size() != actual.enemies.size())
        {
            stream<<"enemies prediction size mismatch: pred="
                <<predicted.enemies.size()
                <<" actual="<<actual.enemies.size()<<endl;
        }
        else
        {
            for(size_t i = 0; i < actual.enemies.size(); ++i)
            {
                if(actual.enemies[i] != predicted.enemies[i])
                {
                    stream<<"enemies prediction error: pred="<<predicted.enemies[i]
                        <<" actual="<<actual.enemies[i]<<endl;
                }
            }
        }
        if(predicted.dataPoints.size() != actual.dataPoints.size())
        {
            stream<<"points prediction size mismatch: pred="
                <<predicted.dataPoints.size()
                <<" actual="<<actual.dataPoints.size()<<endl;
        }
        else
        {
            for(size_t i = 0; i < actual.dataPoints.size(); ++i)
            {
                if(actual.dataPoints[i] != predicted.dataPoints[i])
                {
                    stream<<"points prediction error: pred="<<predicted.dataPoints[i]
                        <<" actual="<<actual.dataPoints[i]<<endl;
                }
            }
        }
    }
}

int main()
{
    Logic logic;
//  World predictedWorld{Player{Point{0,0}}, DataPointCol(), EnemyCol()};
    while (1)
    {
        World world{Player{Point{0,0}}, DataPointCol(), EnemyCol()};
        cin>>world.player.pos.x>>world.player.pos.y; cin.ignore();
        int dataCount = 0;
        cin >> dataCount; cin.ignore();
        world.dataPoints.reserve(dataCount);
        for (int i = 0; i < dataCount; i++)
        {
            DataPoint p{0, Point{0,0}};
            cin>>p.id>>p.pos.x>>p.pos.y; cin.ignore();
            world.dataPoints.push_back(p);
        }
        int enemyCount = 0;
        cin>>enemyCount; cin.ignore();
        world.enemies.reserve(enemyCount);
        for (int i = 0; i < enemyCount; i++)
        {
            Enemy e{0, 0, Point{0, 0}};
            cin>>e.id>>e.pos.x>>e.pos.y>>e.life; cin.ignore();
            world.enemies.push_back(e);
        }
//      printWorldComparison(cerr, world, predictedWorld);
        const auto cmd = logic.step(world);
        switch(cmd.type)
        {
        case Cmd::TYPE_MOVE:
            cout<<"MOVE "<<cmd.data.pos.x<<' '<<cmd.data.pos.y<<endl;
            break;
        case Cmd::TYPE_SHOOT:
            cout<<"SHOOT "<<cmd.data.id<<endl;
            break;
        default:
            assert(false);
        }
//      predictedWorld = eval(world, cmd).first;
    }
    return 0;
}
