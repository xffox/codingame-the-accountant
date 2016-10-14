#ifndef GAME_H
#define GAME_H

#include <vector>
#include <chrono>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "geom.h"

namespace game
{
    using namespace std;

    struct Enemy
    {
        int id;
        int life;
        geom::Point pos;
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
        geom::Point pos;
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
        geom::Point pos;
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

        const geom::Point &getMovePoint() const
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

        static Cmd makeMoveCmd(const geom::Point &p, const string &comment=string())
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
            geom::Point pos;
        };
        Type type;
        Data data;
        string comment;

    private:
        Cmd(Type type, const Data &data, const string &comment)
            :type(type), data(data), comment(comment){}
    };

    using PointCol = vector<geom::Point>;
    using EnemyCol = vector<Enemy>;
    using DataPointCol = vector<DataPoint>;

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
    const int PLAYER_STEP_DIST = 1000;
    const int DEATH_DIST = 2000;
    const int ENEMY_STEP_DIST = 500;
    constexpr geom::Point ZONE{16000, 9000};
    constexpr chrono::milliseconds TIME_LIMIT(100);

    class WorldEval
    {
    public:
        WorldEval(const World &world);

        bool eval(const Cmd &cmd);

        const World &getWorld() const
        {
            return world;
        }

        int getTotalHealth() const
        {
            return totalHealth;
        }

        pair<DataPoint, bool> getEnemyPoint(const int enemyId) const;

        size_t getMaxEnemyId() const
        {
            return maxEnemyId;
        }

        size_t getMaxDataPointId() const
        {
            return maxDataPointId;
        }

    private:
        using IdIdxCol = vector<int>;

        void indexEnemies()
        {
            enemiesById.clear();
            enemiesById.resize(getMaxEnemyId()+1, -1);
            for(size_t i = 0; i < world.enemies.size(); ++i)
            {
                enemiesById[world.enemies[i].id] = i;
            }
        }

        void indexDataPoints()
        {
            pointsById.clear();
            pointsById.resize(getMaxDataPointId()+1, -1);
            for(size_t i = 0; i < world.dataPoints.size(); ++i)
            {
                pointsById[world.dataPoints[i].id] = i;
            }
        }

        Enemy *findEnemy(int enemyId);
        const Enemy *findEnemy(int enemyId) const
        {
            return const_cast<WorldEval*>(this)->findEnemy(enemyId);
        }
        bool eraseEnemy(int enemyId);
        int enemyIdx(int enemyId) const;

        DataPoint *findDataPoint(int dataPointId);
        const DataPoint *findDataPoint(int dataPointId) const
        {
            return const_cast<WorldEval*>(this)->findDataPoint(dataPointId);
        }
        int dataPointIdx(int dataPointId) const;

        int findEnemyPoint(int enemyId) const;
        bool eraseEnemyPoint(int enemyId);
        void clearEnemyPoints();

        static int closestDataPoint(const geom::Point &pos, const World &w);

        World world;
        IdIdxCol enemiesById;
        IdIdxCol pointsById;
        IdIdxCol enemyPoints;
        int totalHealth;
        size_t maxEnemyId;
        size_t maxDataPointId;
    };
}

#endif
