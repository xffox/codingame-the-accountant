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
//      cerr<<"player: pos="<<world.player.pos<<endl;
//      for(const auto &e : world.enemies)
//          cerr<<"enemy: id="<<e.id<<" life="<<e.life<<" pos="<<e.pos<<endl;
//      for(const auto &p : world.dataPoints)
//          cerr<<"point: id="<<p.id<<" pos="<<p.pos<<endl;
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
