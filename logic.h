#ifndef LOGIC_H
#define LOGIC_H

#include <vector>
#include <functional>

#include "game.h"
#include "geom.h"
#include "optimizer.h"

namespace logic
{
    using namespace std;

    using IdxCol = vector<size_t>;

    class Logic
    {
    public:
        Logic();

        game::Cmd step(const game::World &world);

    private:
        using CmdCol = vector<game::Cmd>;

        static game::PointCol calcEnemyClosestPoints(const game::World &w);
        static IdxCol enemiesIndicesByDistance(const game::World &w, size_t count);
        static size_t selectPointEnemy(const game::WorldEval &w);
        static size_t selectClosestEnemy(const game::World &w);
        // TODO: don't move too close to enemies
        static geom::Point selectPosition(const game::World &w);
        static geom::Point averageDataPointPosition(const game::World &w);
        static pair<geom::Point, bool> selectRunPosition(const game::World &w);
        static geom::Point nextEnemyPosition(const game::Enemy &enemy, const geom::Point &point);

        optimizer::Optimizer optimizer;
    };
}

#endif
