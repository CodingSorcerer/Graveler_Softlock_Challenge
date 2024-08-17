#include <functional>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

volatile int maxParalisysTurns = 0;
std::mutex turnCheckMutex, rollMutex;
std::uniform_int_distribution paraChance(1,4);
std::mt19937 randomNumberGenerator(std::time(nullptr));

inline int rollAd4()
{
    std::lock_guard lock(rollMutex);
    return paraChance(randomNumberGenerator);
}

inline int getTurnsInBattle()
{
    int safeTurns = 55;
    int totalTurns = 0;

    while (safeTurns > 0)
    {
        const int randRoll = rollAd4();
        safeTurns = (safeTurns - 1) + (randRoll >> 2);
        totalTurns++;
    }

    return totalTurns;
}

static void flushThreads(std::vector<std::thread*>& runningThreads)
{
    for (std::thread* thread : runningThreads)
    {
        thread->join();
        delete thread;
    }
    runningThreads.clear();
}

int main()
{
    std::vector<std::thread*> runningThreads;

    for (int x=0; x<1000000000; x++)
    {
        runningThreads.push_back(new std::thread([]()
            {
                int totalParalisysTurns = getTurnsInBattle() - 55;
                std::lock_guard lock(turnCheckMutex);
                if (totalParalisysTurns > maxParalisysTurns) maxParalisysTurns = totalParalisysTurns;
            }
        ));

        if (runningThreads.size() > 100)
        {
            flushThreads(runningThreads);
        }
    }

    flushThreads(runningThreads);

    std::cout << std::endl << "Highest Paralisys Turns: " << maxParalisysTurns << std::endl;
    return 0;
}
