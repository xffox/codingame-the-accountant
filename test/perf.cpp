#include <chrono>
#include <iostream>

#include "optimizer.h"
#include "geom.h"
#include "game.h"
#include "logic.h"

namespace
{
    void perf()
    {
        const game::World world{
            game::Player{
                geom::Point{
                    8000, 8999
                }
            },
            game::DataPointCol{
                game::DataPoint{
                    0,
                    geom::Point{
                        200, 1000
                    }
                },
                game::DataPoint{
                    1,
                    geom::Point{
                        1099, 300
                    }
                }
            },
            game::EnemyCol{
                game::Enemy{
                    0, 3,
                    geom::Point{
                        15999, 0
                    }
                },
                game::Enemy{
                    1, 2,
                    geom::Point{
                        15111, 4536
                    }
                },
                game::Enemy{
                    2, 20,
                    geom::Point{
                        11111, 5536
                    }
                },
                game::Enemy{
                    3, 2,
                    geom::Point{
                        15999,7600
                    }
                },
                game::Enemy{
                    4, 8,
                    geom::Point{
                        11999,6000
                    }
                },
                game::Enemy{
                    5, 13,
                    geom::Point{
                        13999,8404
                    }
                },
                game::Enemy{
                    6, 50,
                    geom::Point{
                        7200,0
                    }
                }
            }
        };
        optimizer::Optimizer optimizer(logic::Logic::searchFuncs);
        const auto r = optimizer.optimize(world, std::chrono::milliseconds(1000000));
        if(r.second)
        {
            std::cerr<<"solution found"<<std::endl;
            const auto totalRes = optimizer.bestCriteria();
            if(totalRes.second)
            {
                std::cerr<<"total best solution: "<<totalRes.first<<std::endl;
            }
            else
            {
                std::cerr<<"no total solution found"<<std::endl;
            }
        }
        else
        {
            std::cerr<<"no solution found"<<std::endl;
        }
    }
}

int main()
{
    perf();
}
