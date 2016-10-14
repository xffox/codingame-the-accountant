#include <cstddef>
#include <iostream>

#include "game.h"
#include "logic.h"

namespace
{
    game::DataPointCol makeDataPoints(std::size_t sz)
    {
        game::DataPointCol res;
        res.reserve(sz);
        for(size_t i = 0; i < sz; ++i)
        {
            res.push_back(game::DataPoint{static_cast<int>(i), geom::Point{
                static_cast<int>(i*10+10),
                static_cast<int>(i*10+10)}});
        }
        return res;
    }

    game::EnemyCol makeEnemies(std::size_t sz)
    {
        game::EnemyCol res;
        res.reserve(sz);
        for(size_t i = 0; i < sz; ++i)
        {
            res.push_back(game::Enemy{static_cast<int>(i),
                100,
                geom::Point{
                    static_cast<int>(i*10+5000),
                    static_cast<int>(i*10+5000)}});
        }
        return res;
    }

    void run()
    {
        const std::size_t TRIALS = 100;
        for(std::size_t i = 1; i <= TRIALS; ++i)
        {
            std::cerr<<"running trial: "<<i<<std::endl;
            game::WorldEval w(game::World{
                game::Player{geom::Point{100, 100}},
                makeDataPoints(20),
                makeEnemies(30)
            });
            logic::Logic logic;

            std::size_t depth = 1;
            for(;; ++depth)
            {
                const auto cmd = logic.step(w.getWorld());
                std::cerr<<"command: "<<cmd.getComment()<<std::endl;
                const auto r = w.eval(cmd);
                if(!r)
                {
                    std::cerr<<"player killed"<<std::endl;
                    break;
                }
                if(w.getWorld().enemies.empty())
                {
                    std::cerr<<"enemies destroyed"<<std::endl;
                    break;
                }
                if(w.getWorld().dataPoints.empty())
                {
                    std::cerr<<"data points destroyed"<<std::endl;
                    break;
                }
            }
            std::cerr<<"trial result: depth="<<depth<<std::endl;
        }
    }
}

int main()
{
    run();
}
