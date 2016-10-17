#include "optimizer.h"

#include <iostream>
#include <cassert>

namespace optimizer
{
    namespace
    {
        constexpr bool insideGameZone(const geom::Point &p)
        {
            return p.x >= 0 && p.x <= game::ZONE.x &&
                p.y >= 0 && p.y <= game::ZONE.y;
        }

    }
    ostream &operator<<(ostream &stream, const optimizer::Criteria &c)
    {
        return stream<<"{shots="<<c.shotsFired
            <<",points="<<c.alivePoints
            <<",enemies="<<c.aliveEnemies
            <<",damage="<<c.totalDamage
            <<'}';
    }

    Optimizer::Optimizer(const CmdFuncCol &searchCmdProducers)
        :searchCmdProducers(searchCmdProducers),
        root(), nextRoot(), bestLeaf(), totalBestLeaf(),
        unfinishedBestLeaf(), nextLeafs(), unfinishedLeafs(),
        depth(0),
        seenStates()
    {}

    pair<game::Cmd, bool> Optimizer::optimize(const game::World &world,
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
            if(nextRoot->data.world.getWorld() != world)
            {
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
                if(totalBestLeaf &&
                    lessDropCriteria(makeCriteria(cur->data),
                        makeCriteria(totalBestLeaf->data)))
                {
                    continue;
                }
                const auto &cmd = cur->data.cmd;
                if(cmd.getType() == game::Cmd::TYPE_MOVE)
                {
                    if(!insideGameZone(cmd.getMovePoint()))
                        continue;
                }
                auto &worldEval = cur->data.world;
                const auto totalHealthBefore = worldEval.getTotalHealth();
                const auto validWorld = worldEval.eval(cur->data.cmd);
                ++worldEvals;
                int totalDamage = cur->data.state.totalDamage;
                size_t shotsFired = cur->data.state.shotsFired;
                if(cmd.getType() == game::Cmd::TYPE_SHOOT)
                {
                    totalDamage += totalHealthBefore -
                        worldEval.getTotalHealth();
                    shotsFired += 1;
                }
                const State nextState{shotsFired, totalDamage};
                cur->data.state = nextState;
                if(validWorld)
                {
                    if(!seenStates.insert(makeReducedState(worldEval, nextState)).second)
                        continue;
                    unfinishedBestLeaf = bestResultNode(
                        unfinishedBestLeaf.lock(), cur);
                    if(worldEval.getWorld().enemies.empty() ||
                        worldEval.getWorld().dataPoints.empty())
                    {
                        totalBestLeaf = bestResultNode(
                            totalBestLeaf, cur);
                        continue;
                    }
                    for(const auto &f : searchCmdProducers)
                    {
                        const auto cmds = f(worldEval);
                        for(const auto &c : cmds)
                        {
                            shared_ptr<Node> node(
                                new Node{
                                NodeData{
                                c,
                                worldEval,
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
            return make_pair(game::Cmd::makeMoveCmd(world.player.pos), false);
        }
    }

    pair<Criteria, bool> Optimizer::bestCriteria() const
    {
        auto bestLeafLock = bestLeaf.lock();
        if(bestLeafLock)
        {
            return make_pair(makeCriteria(bestLeafLock->data), true);
        }
        else
        {
            return make_pair(Criteria{}, false);
        }
    }

    void Optimizer::reset(const game::World &world)
    {
        const State nextState{0, 0};
        root.reset(new Node{
            NodeData{
            game::Cmd::makeMoveCmd(world.player.pos),
            game::WorldEval(world),
            nextState
            },
            NodePtrCol(),
            weak_ptr<Node>()});
        nextRoot = root;
        bestLeaf.reset();
        totalBestLeaf.reset();
        unfinishedBestLeaf.reset();
        nextLeafs.clear();
        depth = 0;
        seenStates.clear();
        unfinishedLeafs.clear();
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
                    weak_ptr<Node>(root)
                    });
                root->children.push_back(node);
                unfinishedLeafs.push_back(node);
            }
        }
    }

    ReducedState Optimizer::makeReducedState(
        const game::WorldEval &worldEval, const State &s)
    {
        const auto &w = worldEval.getWorld();
        FlagCol enemies(worldEval.getMaxEnemyId()+1, false);
        for(const auto &e : w.enemies)
        {
            const auto id = e.id;
            assert(static_cast<size_t>(id) < enemies.size());
            enemies[id] = true;
        }
        FlagCol points(worldEval.getMaxDataPointId()+1, false);
        for(const auto &p : w.dataPoints)
        {
            const auto id = p.id;
            assert(static_cast<size_t>(id) < points.size());
            points[id] = true;
        }
        const auto POS_REDUCER = game::ENEMY_STEP_DIST;
        return ReducedState{
            geom::Point{w.player.pos.x/POS_REDUCER,
                w.player.pos.y/POS_REDUCER},
                s.shotsFired,
                s.totalDamage,
                move(enemies),
                move(points)
        };
    }

    shared_ptr<Optimizer::Node> Optimizer::bestResultNode(shared_ptr<Node> left,
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
}
