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
#include <set>
#include <tuple>

#include "geom.h"
#include "game.h"
#include "logic.h"

using namespace std;

/**
 * Shoot enemies before they collect all the incriminating data!
 * The closer you are to an enemy, the more damage you do but don't get too close or you'll get killed.
 **/
namespace
{
    using EnemyMap = unordered_map<int, game::Enemy>;
    using DataPointMap = unordered_map<int, game::DataPoint>;

    ostream &operator<<(ostream &stream, const game::Enemy &enemy)
    {
        return stream<<"{id="<<enemy.id<<",life="<<enemy.life<<",pos="
            <<enemy.pos<<'}';
    }

    ostream &operator<<(ostream &stream, const game::DataPoint &point)
    {
        return stream<<"{id="<<point.id<<",pos="<<point.pos<<'}';
    }

    void printWorldComparison(ostream &stream, const game::World &actual,
        const game::World &predicted)
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
    logic::Logic logic;
    while(1)
    {
        game::World world{game::Player{geom::Point{0,0}}, game::DataPointCol(), game::EnemyCol()};
        cin>>world.player.pos.x>>world.player.pos.y; cin.ignore();
        int dataCount = 0;
        cin >> dataCount; cin.ignore();
        world.dataPoints.reserve(dataCount);
        for (int i = 0; i < dataCount; i++)
        {
            game::DataPoint p{0, geom::Point{0,0}};
            cin>>p.id>>p.pos.x>>p.pos.y; cin.ignore();
            world.dataPoints.push_back(p);
        }
        int enemyCount = 0;
        cin>>enemyCount; cin.ignore();
        world.enemies.reserve(enemyCount);
        for (int i = 0; i < enemyCount; i++)
        {
            game::Enemy e{0, 0, geom::Point{0, 0}};
            cin>>e.id>>e.pos.x>>e.pos.y>>e.life; cin.ignore();
            world.enemies.push_back(e);
        }
        const auto cmd = logic.step(world);
        switch(cmd.getType())
        {
        case game::Cmd::TYPE_MOVE:
            {
                const auto &pos = cmd.getMovePoint();
                cout<<"MOVE "<<pos.x<<' '<<pos.y<<' '<<cmd.getComment()<<endl;
            }
            break;
        case game::Cmd::TYPE_SHOOT:
            cout<<"SHOOT "<<cmd.getShootId()<<' '<<cmd.getComment()<<endl;
            break;
        default:
            assert(false);
        }
    }
    return 0;
}
