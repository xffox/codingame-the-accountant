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
#include <stdexcept>
#include <memory>

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
    inline bool operator==(const Player &left, const Player &right)
    {
        return left.pos == right.pos;
    }
    inline bool operator!=(const Player &left, const Player &right)
    {
        return !(left == right);
    }

    class Cmd
    {
    public:
        enum Type
        {
            TYPE_MOVE,
            TYPE_SHOOT
        };

        Type getType() const
        {
            return type;
        }

        const Point &getMovePoint() const
        {
            if(type != TYPE_MOVE)
                throw logic_error("accessing position in non-move command");
            return data.pos;
        }

        int getShootId() const
        {
            if(type != TYPE_SHOOT)
                throw logic_error("accessing id in non-shoot command");
            return data.id;
        }

        const string &getComment() const
        {
            return comment;
        }

        static Cmd makeMoveCmd(const Point &p, const string &comment=string())
        {
            Data d;
            d.pos = p;
            return Cmd(TYPE_MOVE, d, comment);
        }
        static Cmd makeShootCmd(int id, const string &comment=string())
        {
            Data d;
            d.id = id;
            return Cmd(TYPE_SHOOT, d, comment);
        }

    private:
        union Data
        {
            int id;
            Point pos;
        };
        Type type;
        Data data;
        string comment;

    private:
        Cmd(Type type, const Data &data, const string &comment)
            :type(type), data(data), comment(comment){}
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
    inline bool operator==(const World &left, const World &right)
    {
        return left.player == right.player &&
            left.dataPoints == right.dataPoints &&
            left.enemies == right.enemies;
    }
    inline bool operator!=(const World &left, const World &right)
    {
        return !(left == right);
    }

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
    using EnemyMap = unordered_map<int, Enemy>;
    using DataPointMap = unordered_map<int, DataPoint>;

    struct AuxEnemy
    {
        int closestPointId;
        Vect velocity;
    };
    using AuxEnemyCol = vector<AuxEnemy>;

    struct AuxWorld
    {
        AuxEnemyCol auxEnemies;
    };

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
        return
            (left.alivePoints < right.alivePoints ||
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
    static bool lessDropCriteria(const Criteria &left, const Criteria &right)
    {
        return (left.alivePoints < right.alivePoints);
    }

    ostream &operator<<(ostream &stream, const Point &p)
    {
        return stream<<'('<<p.x<<','<<p.y<<')';
    }

    ostream &operator<<(ostream &stream, const Enemy &enemy)
    {
        return stream<<"{id="<<enemy.id<<",life="<<enemy.life<<",pos="
            <<enemy.pos<<'}';
    }

    ostream &operator<<(ostream &stream, const DataPoint &point)
    {
        return stream<<"{id="<<point.id<<",pos="<<point.pos<<'}';
    }

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

    pair<World, bool> eval(const World &world, const Cmd &cmd)
    {
        World nextWorld(world);
        using IdSet = unordered_set<int>;
        using PointEnemiesMap = unordered_map<int, IdSet>;
        using EnemyMap = unordered_map<int, EnemyCol::iterator>;
        PointEnemiesMap pointEnemies;
        EnemyMap nextEnemiesById;
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
                nextEnemiesById.insert(make_pair(nextWorld.enemies[i].id,
                        nextWorld.enemies.begin()+i));
            }
        }
        if(cmd.getType() == Cmd::TYPE_MOVE)
        {
            if(geom::dist(nextWorld.player.pos, cmd.getMovePoint()) >
                game::PLAYER_STEP_DIST)
            {
                const auto direction = geom::normDirection(
                    nextWorld.player.pos, cmd.getMovePoint());
                const auto directedStep = geom::mult(direction, game::PLAYER_STEP_DIST);
                nextWorld.player.pos = geom::add(nextWorld.player.pos,
                    directedStep);
            }
            else
            {
                nextWorld.player.pos = cmd.getMovePoint();
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
            if(geom::dist(nextWorld.player.pos, e.pos) <= game::DEATH_DIST)
            {
                return make_pair(nextWorld, false);
            }
        }
        int deadEnemyId = -1;
        if(cmd.getType() == Cmd::TYPE_SHOOT)
        {
            assert(!nextWorld.enemies.empty());
            auto iter = nextEnemiesById.find(cmd.getShootId());
            assert(iter != nextEnemiesById.end());
            assert(iter->second != nextWorld.enemies.end());
            const int damage = round(125000.0/
                pow(geom::dist(nextWorld.player.pos, iter->second->pos), 1.2));
            if(iter->second->life > damage)
            {
                iter->second->life -= damage;
            }
            else
            {
                deadEnemyId = iter->second->id;
                nextWorld.enemies.erase(iter->second);
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

    class Optimizer
    {
    private:
        static const size_t SURVIVAL_COUNT;

    public:
        using CmdCol = vector<Cmd>;
        using CmdFuncCol = vector<function<CmdCol(const World&)>>;

    public:
        Optimizer(CmdFuncCol &&searchCmdProducers)
            :searchCmdProducers(move(searchCmdProducers)),
            root(), nextRoot(), bestLeaf(), totalBestLeaf(),
            unfinishedBestLeaf(), nextLeafs(), unfinishedLeafs(),
            depth(0)
        {
        }

        pair<Cmd, bool> optimize(const World &world,
            chrono::milliseconds timeLimit)
        {
            const auto beginTime = Clock::now();
            bool timeout = false;
            if(!root || !nextRoot)
            {
                reset(world);
            }
            else
            {
                if(nextRoot->data.world != world)
                {
                    printWorldComparison(cerr, world, nextRoot->data.world);
                    cerr<<"predicted world mismatch, reseting optimization tree"<<endl;
                    reset(world);
                }
                else
                {
                    // reset dropped paths
                    root = nextRoot;
                }
            }
            size_t worldEvals = 0;
            if(unfinishedLeafs.empty())
                cerr<<"search tree is fully built"<<endl;
            while(!unfinishedLeafs.empty())
            {
                while(!unfinishedLeafs.empty())
                {
                    if(Clock::now() - beginTime >= timeLimit)
                    {
                        timeout = true;
                        break;
                    }
                    auto cur = unfinishedLeafs.front().lock();
                    unfinishedLeafs.pop_front();
                    if(!cur)
                        continue;
                    const auto &cmd = cur->data.cmd;
//                  if(cmd.getType() == Cmd::TYPE_MOVE)
//                  {
//                      if(!insideGameZone(cmd.getMovePoint()))
//                          continue;
//                  }
                    const auto w = eval(cur->data.world, cur->data.cmd);
                    ++worldEvals;
                    int totalDamage = cur->data.state.totalDamage;
                    size_t shotsFired = cur->data.state.shotsFired;
                    if(cmd.getType() == Cmd::TYPE_SHOOT)
                    {
                        totalDamage += enemiesLifes(cur->data.world) -
                            enemiesLifes(w.first);
                        shotsFired += 1;
                    }
                    const State nextState{shotsFired, totalDamage};
                    cur->data.world = w.first;
                    cur->data.state = nextState;
                    // TODO: maybe timeout here
                    if(w.second)
                    {
                        unfinishedBestLeaf = bestResultNode(
                            unfinishedBestLeaf.lock(), cur);
                        if(w.first.enemies.empty() || w.first.dataPoints.empty())
                        {
                            totalBestLeaf = bestResultNode(
                                totalBestLeaf, cur);
                            continue;
                        }
                        if(totalBestLeaf &&
                            lessDropCriteria(makeCriteria(cur->data),
                                makeCriteria(totalBestLeaf->data)))
                        {
                            continue;
                        }
                        for(const auto &f : searchCmdProducers)
                        {
                            const auto cmds = f(w.first);
                            for(const auto &c : cmds)
                            {
                                shared_ptr<Node> node(
                                    new Node{
                                    NodeData{
                                        c,
                                        w.first,
                                        nextState
                                    },
                                    NodePtrCol(),
                                    weak_ptr<Node>(cur)});
                                cur->children.push_back(node);
                                nextLeafs.push_back(node);
                            }
                        }
                        if(timeout)
                            break;
                    }
                }
                if(!timeout)
                {
                    ++depth;
                    assert(unfinishedLeafs.empty());
                    unfinishedLeafs = move(nextLeafs);
                    bestLeaf = bestResultNode(
                        totalBestLeaf, unfinishedBestLeaf.lock());
                    unfinishedBestLeaf.reset();
                    if(unfinishedLeafs.empty())
                    {
                        cerr<<"full optimization tree is built"<<endl;
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            bestLeaf = bestResultNode(
                bestLeaf.lock(), totalBestLeaf);
            auto cur = bestLeaf.lock();
            shared_ptr<Node> parent;
            if(cur)
                parent = cur->parent.lock();
            if(cur && parent)
            {
                const auto criteria = makeCriteria(cur->data);
                while(true)
                {
                    auto grandParent = parent->parent.lock();
                    if(!grandParent)
                        break;
                    cur = parent;
                    parent = grandParent;
                }
                parent.reset();
                assert(cur);
                nextRoot = cur;
                assert(depth > 0);
                cerr<<"optimizer stats: depth="<<depth<<" time="
                    <<chrono::duration_cast<chrono::milliseconds>(Clock::now()-beginTime).count()
                    <<" evals="<<worldEvals
                    <<endl;
                cerr<<"optimized result: "<<criteria<<endl;
                --depth;
                return make_pair(cur->data.cmd, true);
            }
            else
            {
                nextRoot.reset();
                cerr<<"optimizer stats: depth="<<depth<<" time="
                    <<chrono::duration_cast<chrono::milliseconds>(Clock::now()-beginTime).count()
                    <<" evals="<<worldEvals
                    <<endl;
                cerr<<"no optimized result"<<endl;
                return make_pair(Cmd::makeMoveCmd(world.player.pos), false);
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

    private:
        struct State
        {
            size_t shotsFired;
            int totalDamage;
        };
        struct NodeData
        {
            Cmd cmd;
            World world;
            State state;
        };
        struct Node;
        using NodePtrCol = vector<shared_ptr<Node>>;
        using NodeWeakPtrList = list<weak_ptr<Node>>;
        struct Node
        {
            NodeData data;
            NodePtrCol children;
            weak_ptr<Node> parent;
        };

        static Criteria makeCriteria(const NodeData &d)
        {
            return Criteria{
                d.state.shotsFired,
                d.world.dataPoints.size(),
                d.world.enemies.size(),
                d.state.totalDamage
            };
        }

        void reset(const World &world)
        {
            root.reset(new Node{
                NodeData{
                    Cmd::makeMoveCmd(world.player.pos),
                    world,
                    State{0, 0}
                },
                NodePtrCol(),
                weak_ptr<Node>()});
            nextRoot = root;
            bestLeaf.reset();
            totalBestLeaf.reset();
            unfinishedBestLeaf.reset();
            nextLeafs.clear();
            depth = 0;
            unfinishedLeafs.clear();
            const State nextState{0, 0};
            for(const auto &f : searchCmdProducers)
            {
                const auto cmds = f(world);
                for(const auto &c : cmds)
                {
                    shared_ptr<Node> node(
                        new Node{
                        NodeData{
                        c,
                        world,
                        nextState
                        },
                        NodePtrCol(),
                        weak_ptr<Node>(root)});
                    root->children.push_back(node);
                    unfinishedLeafs.push_back(node);
                }
            }
        }

        static shared_ptr<Node> bestResultNode(shared_ptr<Node> left,
            shared_ptr<Node> right)
        {
            if(left && right)
            {
                const auto leftCriteria = makeCriteria(left->data);
                const auto rightCriteria = makeCriteria(right->data);
                if(leftCriteria < rightCriteria)
                    return right;
                else
                    return left;
            }
            else
            {
                if(left)
                    return left;
                else
                    return right;
            }
        }

        CmdFuncCol searchCmdProducers;
        shared_ptr<Node> root;
        shared_ptr<Node> nextRoot;
        weak_ptr<Node> bestLeaf;
        shared_ptr<Node> totalBestLeaf;
        weak_ptr<Node> unfinishedBestLeaf;
        NodeWeakPtrList nextLeafs;
        NodeWeakPtrList unfinishedLeafs;
        size_t depth;
    };
    const size_t Optimizer::SURVIVAL_COUNT = 100;

    class Logic
    {
    public:
        Cmd step(const World &world)
        {
            cerr<<"trying optimized step"<<endl;
            const auto optRes = optimizer.optimize(world,
                chrono::duration_cast<chrono::milliseconds>(game::TIME_LIMIT*0.9));
            if(optRes.second)
            {
                return optRes.first;
            }
            cerr<<"no optimized solution"<<endl;
            return Cmd::makeMoveCmd(world.player.pos);
        }

    private:
        using CmdCol = vector<Cmd>;
        using CmdFuncCol = vector<function<CmdCol(const World&)>>;

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

        static PointCol calcEnemyClosestPoints(const World &w)
        {
            const auto &enemies = w.enemies;
            const auto &points = w.dataPoints;
            PointCol res;
            for(size_t i = 0; i < enemies.size(); ++i)
            {
                const auto &e = enemies[i];
                size_t minIdx = 0;
                double minDist = numeric_limits<double>::max();
                for(size_t j = 0; j < points.size(); ++j)
                {
                    const auto &p = points[j];
                    const auto pointDist = geom::dist(p.pos, e.pos);
                    if(pointDist < minDist)
                    {
                        minIdx = j;
                        minDist = pointDist;
                    }
                }
                res.push_back(points[minIdx].pos);
            }
            return res;
        }

        static size_t selectPointEnemy(const World &w,
            const PointCol &enemyPoints)
        {
            const auto &enemies = w.enemies;
            assert(!enemies.empty());
            assert(enemies.size() == enemyPoints.size());
            double pointEnemyMinDist = numeric_limits<double>::max();
            int pointClosestEnemyIdx = 0;
            for(size_t i = 0; i < enemies.size(); ++i)
            {
                const auto &e = enemies[i];
                const auto d = geom::dist(e.pos, enemyPoints[i]);
                if(d < pointEnemyMinDist)
                {
                    pointEnemyMinDist = d;
                    pointClosestEnemyIdx = i;
                }
            }
            return pointClosestEnemyIdx;
        }

        static size_t selectClosestEnemy(const World &w)
        {
            const auto &player = w.player;
            auto iter = min_element(w.enemies.begin(), w.enemies.end(),
                [&player](const EnemyCol::value_type &left, const EnemyCol::value_type &right) {
                    return geom::dist(player.pos, left.pos) < geom::dist(player.pos, right.pos);
                });
            assert(iter != w.enemies.end());
            return iter - w.enemies.begin();
        }

        // TODO: don't move too close to enemies
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

        static Point averageDataPointPosition(const World &w)
        {
            const auto &points = w.dataPoints;
            assert(!points.empty());
            long long int avgX = 0;
            long long int avgY = 0;
            for(const auto &p : points)
            {
                avgX += p.pos.x;
                avgY += p.pos.y;
            }
            avgX /= points.size();
            avgY /= points.size();
            return Point{static_cast<int>(avgX), static_cast<int>(avgY)};
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

        static Point nextEnemyPosition(const Enemy &enemy, const Point &point)
        {
            return geom::add(enemy.pos,
                geom::mult(geom::normDirection(enemy.pos,
                        point), game::ENEMY_STEP_DIST));
        }

        Optimizer optimizer = Optimizer(CmdFuncCol{
            [](const World &world) {
            const auto moveCmd = Cmd::makeMoveCmd(selectPosition(world),
                "moving to enemies centroid");
            return CmdCol{moveCmd};
            },
            [](const World &world) {
                CmdCol res;
                if(!world.enemies.empty())
                {
                    const auto enemyPoints = calcEnemyClosestPoints(world);
                    const auto pointEnemyIdx = selectPointEnemy(world,
                        enemyPoints);
                    assert(pointEnemyIdx < world.enemies.size());
                    const auto &pointEnemy = world.enemies[pointEnemyIdx];
                    const auto closestEnemyIdx = selectClosestEnemy(world);
                    assert(closestEnemyIdx < world.enemies.size());
                    const auto &closestEnemy = world.enemies[closestEnemyIdx];
                    res.push_back(Cmd::makeShootCmd(pointEnemy.id,
                            "shooting point enemy"));
                    res.push_back(Cmd::makeMoveCmd(nextEnemyPosition(pointEnemy,
                                enemyPoints[pointEnemyIdx]),
                            "moving to point enemy"));
                    if(pointEnemy.id != closestEnemy.id)
                    {
                        res.push_back(Cmd::makeShootCmd(closestEnemy.id,
                                "shooting closest enemy"));
                        res.push_back(Cmd::makeMoveCmd(
                                nextEnemyPosition(closestEnemy,
                                    enemyPoints[closestEnemyIdx]),
                                "moving to closest enemy"));
                    }
                }
                return res;
            },
            [](const World &world) {
                const auto runPosRes = selectRunPosition(world);
                if(runPosRes.second)
                {
                    return CmdCol{Cmd::makeMoveCmd(runPosRes.first,
                        "running from enemies")};
                }
                return Logic::CmdCol();
            }
        });
    };
}

int main()
{
    Logic logic;
//  World predictedWorld{Player{Point{0,0}}, DataPointCol(), EnemyCol()};
    while(1)
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
        switch(cmd.getType())
        {
        case Cmd::TYPE_MOVE:
            {
                const auto &pos = cmd.getMovePoint();
                cout<<"MOVE "<<pos.x<<' '<<pos.y<<' '<<cmd.getComment()<<endl;
            }
            break;
        case Cmd::TYPE_SHOOT:
            cout<<"SHOOT "<<cmd.getShootId()<<' '<<cmd.getComment()<<endl;
            break;
        default:
            assert(false);
        }
//      predictedWorld = eval(world, cmd).first;
    }
    return 0;
}
