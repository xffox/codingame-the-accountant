#include "logic.h"

#include <iostream>
#include <cassert>
#include <algorithm>

#include "optimizer.h"

namespace logic
{
    namespace
    {
        using VectCol = vector<geom::Vect>;
    }

    Logic::Logic()
        :optimizer(optimizer::CmdFuncCol{
            [](const game::WorldEval &worldEval) {
                const auto &world = worldEval.getWorld();
                CmdCol res;
                if(!world.enemies.empty())
                {
                const auto enemyPoints = calcEnemyClosestPoints(world);
                const auto closestEnemyIdx = selectClosestEnemy(world);
                assert(closestEnemyIdx < world.enemies.size());
                const auto pointEnemyIdx = selectPointEnemy(worldEval);
                assert(pointEnemyIdx < world.enemies.size());
                const auto &pointEnemy = world.enemies[pointEnemyIdx];
                const auto &closestEnemy = world.enemies[closestEnemyIdx];
                res.push_back(game::Cmd::makeShootCmd(pointEnemy.id,
                        "shooting point enemy"));
                double step = game::PLAYER_STEP_DIST;
                const auto closestEnemyDist = geom::dist(
                    world.player.pos, nextEnemyPosition(closestEnemy,
                        enemyPoints[closestEnemyIdx]));
                if(step + game::DEATH_DIST >= closestEnemyDist)
                step = closestEnemyDist - game::DEATH_DIST - 5.0;
                res.push_back(game::Cmd::makeMoveCmd(
                        geom::add(world.player.pos,
                            geom::mult(
                                geom::normDirection(world.player.pos,
                                    nextEnemyPosition(pointEnemy,
                                        enemyPoints[pointEnemyIdx])),
                                step)),
                        "moving to point enemy"));
                //                  res.push_back(Cmd::makeMoveCmd(enemyPoints[closestEnemyIdx],
                //                          "moving to closest enemy destination"));
                if(pointEnemyIdx != closestEnemyIdx)
                {
                    res.push_back(game::Cmd::makeShootCmd(closestEnemy.id,
                            "shooting closest enemy"));
                    res.push_back(game::Cmd::makeMoveCmd(
                            geom::add(world.player.pos,
                                geom::mult(
                                    geom::normDirection(world.player.pos,
                                        nextEnemyPosition(closestEnemy,
                                            enemyPoints[closestEnemyIdx])),
                                    step)),
                            "moving to closest enemy"));
                }
                }
                return res;
            },
                [](const game::WorldEval &worldEval) {
                    const auto moveCmd = game::Cmd::makeMoveCmd(selectPosition(worldEval.getWorld()),
                        "moving to enemies centroid");
                    return CmdCol{moveCmd};
                },
                [](const game::WorldEval &worldEval) {
                    const auto runPosRes = selectRunPosition(worldEval.getWorld());
                    if(runPosRes.second)
                    {
                        return CmdCol{game::Cmd::makeMoveCmd(runPosRes.first,
                            "running from enemies")};
                    }
                    return Logic::CmdCol();
                }
        }
    )
    {}

    game::Cmd Logic::step(const game::World &world)
    {
        cerr<<"trying optimized step"<<endl;
        const auto optRes = optimizer.optimize(world,
            chrono::duration_cast<chrono::milliseconds>(game::TIME_LIMIT*0.95));
        if(optRes.second)
        {
            return optRes.first;
        }
        cerr<<"no optimized solution"<<endl;
        return game::Cmd::makeMoveCmd(world.player.pos, "give up");
    }

    game::PointCol Logic::calcEnemyClosestPoints(const game::World &w)
    {
        const auto &enemies = w.enemies;
        const auto &points = w.dataPoints;
        game::PointCol res;
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

    IdxCol Logic::enemiesIndicesByDistance(const game::World &w, size_t count)
    {
        IdxCol res;
        // TODO: static initializer?
        for(size_t i = 0; i < w.enemies.size(); ++i)
            res.push_back(i);
        const auto middle = min(count, res.size());
        partial_sort(res.begin(), res.begin()+middle, res.end(),
            [&w](const size_t left, const size_t right) {
            const auto leftDist = geom::dist(w.player.pos, w.enemies[left].pos);
            const auto rightDist = geom::dist(w.player.pos, w.enemies[right].pos);
            return leftDist < rightDist;
            });
        res.resize(middle);
        return res;
    }

    size_t Logic::selectPointEnemy(const game::WorldEval &w)
    {
        const auto &enemies = w.getWorld().enemies;
        assert(!enemies.empty());
        double pointEnemyMinDist = numeric_limits<double>::max();
        int pointClosestEnemyIdx = 0;
        for(size_t i = 0; i < enemies.size(); ++i)
        {
            const auto &e = enemies[i];
            const auto ep = w.getEnemyPoint(e.id);
            if(ep.second)
            {
                const auto d = geom::dist(e.pos, ep.first.pos);
                if(d < pointEnemyMinDist)
                {
                    pointEnemyMinDist = d;
                    pointClosestEnemyIdx = i;
                }
            }
        }
        return pointClosestEnemyIdx;
    }

    size_t Logic::selectClosestEnemy(const game::World &w)
    {
        const auto &player = w.player;
        auto iter = min_element(w.enemies.begin(), w.enemies.end(),
            [&player](const game::EnemyCol::value_type &left, const game::EnemyCol::value_type &right) {
            return geom::dist(player.pos, left.pos) < geom::dist(player.pos, right.pos);
            });
        assert(iter != w.enemies.end());
        return iter - w.enemies.begin();
    }

    // TODO: don't move too close to enemies
    geom::Point Logic::selectPosition(const game::World &w)
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
            const geom::Point target{static_cast<int>(avgX),
                static_cast<int>(avgY)};
            return target;
        }
        else
        {
            return w.player.pos;
        }
    }

    geom::Point Logic::averageDataPointPosition(const game::World &w)
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
        return geom::Point{static_cast<int>(avgX), static_cast<int>(avgY)};
    }

    pair<geom::Point, bool> Logic::selectRunPosition(const game::World &w)
    {
        const auto &enemies = w.enemies;
        const auto &player = w.player;
        game::PointCol deathPoints;
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
                deathPoints.push_back(geom::Point{0, player.pos.y});
            if(game::ZONE.x - player.pos.x <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                deathPoints.push_back(geom::Point{game::ZONE.x, player.pos.y});
            if(player.pos.y <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                deathPoints.push_back(geom::Point{player.pos.x, 0});
            if(game::ZONE.y - player.pos.y <= game::DEATH_DIST+game::PLAYER_STEP_DIST)
                deathPoints.push_back(geom::Point{player.pos.x, game::ZONE.y});
            geom::Vect runDir{0.0, 0.0};
            VectCol deathVectors;
            for(const auto &p : deathPoints)
            {
                const auto v = geom::normDirection(p, player.pos);
                runDir += v;
                deathVectors.push_back(v);
            }
            // TODO: proper equality check
            // TODO: proper danger move check
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
        return make_pair(geom::Point{0, 0}, false);
    }

    geom::Point Logic::nextEnemyPosition(const game::Enemy &enemy, const geom::Point &point)
    {
        return geom::add(enemy.pos,
            geom::mult(geom::normDirection(enemy.pos,
                    point), game::ENEMY_STEP_DIST));
    }
}
