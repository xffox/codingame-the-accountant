#include "game.h"

#include <cstddef>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <utility>
#include <cassert>

using namespace std;

namespace game
{
    namespace
    {
        struct ExtractId
        {
            template<class T>
            int operator()(const T &left, const T &right) const
            {
                return left.id < right.id;
            }
        };
    }

    ostream &operator<<(ostream &stream, const game::Enemy &enemy)
    {
        return stream<<"{id="<<enemy.id<<",life="<<enemy.life<<",pos="
            <<enemy.pos<<'}';
    }

    ostream &operator<<(ostream &stream, const DataPoint &point)
    {
        return stream<<"{id="<<point.id<<",pos="<<point.pos<<'}';
    }

    WorldEval::WorldEval(const World &world)
        :world(world), enemiesById(), pointsById(), enemyPoints(),
        totalHealth(0),
        maxEnemyId(!world.enemies.empty()
            ?max_element(world.enemies.begin(), world.enemies.end(), ExtractId())->id
            :0),
        maxDataPointId(!world.dataPoints.empty()
            ?max_element(world.dataPoints.begin(), world.dataPoints.end(), ExtractId())->id
            :0)
    {
        indexDataPoints();
        indexEnemies();
        enemyPoints.resize(getMaxEnemyId()+1, -1);
        for(const auto &e : this->world.enemies)
        {
            enemyPoints[e.id] = closestDataPoint(e.pos, this->world);
            totalHealth += e.life;
        }
    }

    int WorldEval::closestDataPoint(const geom::Point &pos, const World &w)
    {
        auto closestPointIter = min_element(
            w.dataPoints.begin(), w.dataPoints.end(),
            [&pos](const DataPointCol::value_type &left,
                const DataPointCol::value_type &right) {
            const auto leftDist = geom::dist(left.pos, pos);
            const auto rightDist = geom::dist(right.pos, pos);
            return leftDist < rightDist ||
            (leftDist == rightDist && left.id < right.id);
            });
        assert(closestPointIter != w.dataPoints.end());
        return closestPointIter->id;
    }

    bool WorldEval::eval(const Cmd &cmd)
    {
        using IdSet = unordered_set<int>;
        using PointEnemiesMap = unordered_map<int, IdSet>;
        PointEnemiesMap pointEnemies;
        if(!world.dataPoints.empty())
        {
            for(size_t i = 0; i < world.enemies.size(); ++i)
            {
                const auto &e = world.enemies[i];
                const auto enemyPointId = findEnemyPoint(e.id);
                assert(enemyPointId >= 0);
                const auto *closestPointPtr = findDataPoint(enemyPointId);
                assert(closestPointPtr != nullptr);
                const auto &closestPoint = *closestPointPtr;
                if(static_cast<int>(geom::dist(closestPoint.pos, e.pos)) <= game::ENEMY_STEP_DIST)
                {
                    world.enemies[i].pos = closestPoint.pos;
                    pointEnemies[closestPoint.id].insert(e.id);
                }
                else
                {
                    const auto direction = geom::normDirection(
                        e.pos, closestPoint.pos);
                    world.enemies[i].pos = geom::add(world.enemies[i].pos,
                        geom::mult(direction, game::ENEMY_STEP_DIST));
                }
            }
        }
        if(cmd.getType() == Cmd::TYPE_MOVE)
        {
            if(geom::dist(world.player.pos, cmd.getMovePoint()) >
                game::PLAYER_STEP_DIST)
            {
                const auto direction = geom::normDirection(
                    world.player.pos, cmd.getMovePoint());
                const auto directedStep = geom::mult(direction, game::PLAYER_STEP_DIST);
                world.player.pos = geom::add(world.player.pos,
                    directedStep);
            }
            else
            {
                world.player.pos = cmd.getMovePoint();
            }
            if(world.player.pos.x < 0 ||
                world.player.pos.x > game::ZONE.x)
            {
                world.player.pos.x =
                    min(max(0, world.player.pos.x), game::ZONE.x);
            }
            if(world.player.pos.y < 0 ||
                world.player.pos.y > game::ZONE.y)
            {
                world.player.pos.y =
                    min(max(0, world.player.pos.y), game::ZONE.y);
            }
        }
        for(const auto &e : world.enemies)
        {
            // TODO: floating equality
            if(geom::dist(world.player.pos, e.pos) <= game::DEATH_DIST)
            {
                return false;
            }
        }
        int deadEnemyId = -1;
        if(cmd.getType() == Cmd::TYPE_SHOOT)
        {
            assert(!world.enemies.empty());
            auto *enemyPtr = findEnemy(cmd.getShootId());
            assert(enemyPtr != nullptr);
            auto &enemy = *enemyPtr;
            const int damage = calcDamage(world.player.pos, enemy.pos);
            if(enemy.life > damage)
            {
                enemy.life -= damage;
                assert(totalHealth >= damage);
                totalHealth -= damage;
            }
            else
            {
                deadEnemyId = enemy.id;
                const auto removed = eraseEnemyPoint(deadEnemyId);
                assert(removed);
                assert(totalHealth >= enemy.life);
                totalHealth -= enemy.life;
                const auto r = eraseEnemy(deadEnemyId);
                assert(r);
            }
        }
        DataPointCol nextPoints;
        bool pointsChanged = false;
        for(const auto &p : world.dataPoints)
        {
            auto iter = pointEnemies.find(p.id);
            if(iter != pointEnemies.end() && deadEnemyId != -1)
            {
                iter->second.erase(deadEnemyId);
            }
            if(iter == pointEnemies.end() || iter->second.empty())
            {
                nextPoints.push_back(p);
            }
            else
            {
                pointsChanged = true;
            }
        }
        if(pointsChanged)
        {
            world.dataPoints = move(nextPoints);
            indexDataPoints();
            if(!world.dataPoints.empty())
            {
                for(auto &e : world.enemies)
                {
                    const auto enemyPointId = findEnemyPoint(e.id);
                    assert(enemyPointId >= 0);
                    if(findDataPoint(enemyPointId) == nullptr)
                        enemyPoints[e.id] = closestDataPoint(e.pos, world);
                }
            }
            else
            {
                clearEnemyPoints();
            }
        }
        return true;
    }

    pair<DataPoint, bool> WorldEval::getEnemyPoint(const int enemyId) const
    {
        const DataPoint defaultPoint{0, geom::Point{0, 0}};
        const auto enemyPointId = findEnemyPoint(enemyId);
        if(enemyPointId < 0)
            return make_pair(defaultPoint, false);
        auto *pointPtr = findDataPoint(enemyPointId);
        assert(pointPtr != nullptr);
        return make_pair(*pointPtr, true);
    }

    Enemy *WorldEval::findEnemy(int enemyId)
    {
        const auto idx = enemyIdx(enemyId);
        if(idx >= 0)
            return &world.enemies[idx];
        else
            return nullptr;
    }

    bool WorldEval::eraseEnemy(int enemyId)
    {
        const auto idx = enemyIdx(enemyId);
        if(idx >= 0)
        {
            world.enemies.erase(world.enemies.begin() + idx);
            indexEnemies();
            return true;
        }
        return false;
    }

    int WorldEval::enemyIdx(int enemyId) const
    {
        assert(static_cast<size_t>(enemyId) < enemiesById.size());
        return enemiesById[enemyId];
    }

    DataPoint *WorldEval::findDataPoint(int dataPointId)
    {
        const auto idx = dataPointIdx(dataPointId);
        if(idx >= 0)
            return &world.dataPoints[idx];
        else
            return nullptr;
    }

    int WorldEval::dataPointIdx(int dataPointId) const
    {
        assert(static_cast<size_t>(dataPointId) < pointsById.size());
        return pointsById[dataPointId];
    }

    int WorldEval::findEnemyPoint(int enemyId) const
    {
        assert(static_cast<size_t>(enemyId) < enemyPoints.size());
        return enemyPoints[enemyId];
    }

    bool WorldEval::eraseEnemyPoint(int enemyId)
    {
        assert(static_cast<size_t>(enemyId) < enemyPoints.size());
        const auto idx = enemyPoints[enemyId];
        if(idx >= 0)
        {
            enemyPoints[enemyId] = -1;
            return true;
        }
        else
        {
            return false;
        }
    }

    void WorldEval::clearEnemyPoints()
    {
        enemyPoints.clear();
        enemyPoints.resize(getMaxEnemyId()+1, -1);
    }

    int WorldEval::calcDamage(const geom::Point &player, const geom::Point &enemy)
    {
        return round(125000.0/
            pow(geom::dist(player, enemy), 1.2));
    }
}
