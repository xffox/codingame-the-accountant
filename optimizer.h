#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <vector>
#include <functional>
#include <chrono>
#include <memory>
#include <set>
#include <list>
#include <ostream>

#include "game.h"

namespace optimizer
{
    using namespace std;

    using Clock = chrono::steady_clock;

    using CmdCol = vector<game::Cmd>;
    using CmdFuncCol = vector<function<CmdCol(const game::WorldEval&)>>;

    struct Criteria
    {
        size_t shotsFired;
        size_t alivePoints;
        size_t aliveEnemies;
        int totalDamage;
    };
    inline bool operator<(const Criteria &left, const Criteria &right)
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
    ostream &operator<<(ostream &stream, const optimizer::Criteria &c);

    using FlagCol = vector<bool>;
    struct ReducedState
    {
        geom::Point playerPos;
        size_t shotsFired;
        int totalDamage;
        FlagCol enemies;
        FlagCol points;
    };
    inline bool operator<(const ReducedState &left, const ReducedState &right)
    {
        return
            (left.playerPos < right.playerPos ||
             (left.playerPos == right.playerPos &&
              (left.shotsFired < right.shotsFired ||
               (left.shotsFired == right.shotsFired &&
                (left.totalDamage < right.totalDamage ||
                 (left.totalDamage == right.totalDamage &&
                  (left.enemies < right.enemies ||
                   (left.enemies == right.enemies &&
                    (left.points < right.points)))))))));
    }
    using ReducedStateSet = set<ReducedState>;

    class Optimizer
    {
    public:
        Optimizer(const CmdFuncCol &searchCmdProducers);
        Optimizer(const Optimizer&) = delete;
        Optimizer &operator=(const Optimizer&) = delete;

        pair<game::Cmd, bool> optimize(const game::World &world,
            chrono::milliseconds timeLimit);

        pair<Criteria, bool> bestCriteria() const;

    private:
        struct State
        {
            size_t shotsFired;
            int totalDamage;
        };
        struct NodeData
        {
            game::Cmd cmd;
            game::WorldEval world;
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
                d.world.getWorld().dataPoints.size(),
                d.world.getWorld().enemies.size(),
                d.state.totalDamage
            };
        }

        static bool lessDropCriteria(const Criteria &left, const Criteria &right)
        {
            return (left.alivePoints < right.alivePoints);
        }

        static ReducedState makeReducedState(const game::WorldEval &w, const State &s);
        void reset(const game::World &world);
        static shared_ptr<Node> bestResultNode(shared_ptr<Node> left,
            shared_ptr<Node> right);

        CmdFuncCol searchCmdProducers;
        shared_ptr<Node> root;
        shared_ptr<Node> nextRoot;
        weak_ptr<Node> bestLeaf;
        shared_ptr<Node> totalBestLeaf;
        weak_ptr<Node> unfinishedBestLeaf;
        NodeWeakPtrList nextLeafs;
        NodeWeakPtrList unfinishedLeafs;
        size_t depth;
        ReducedStateSet seenStates;
    };
}

#endif
