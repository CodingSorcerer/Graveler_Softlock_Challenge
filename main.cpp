#include <functional>
#include <iostream>
#include <random>
#include <thread>
#include <unordered_set>

class ThreadPool
{
    int maxThreads;
    std::vector<std::function<void(std::uniform_int_distribution<>&, std::mt19937&)>> waitQueue;
    std::thread** threads;
    std::mutex queue;
    bool shutdown;

    std::function<void(std::uniform_int_distribution<>&, std::mt19937&)> getJob()
    {
        std::function output = [](std::uniform_int_distribution<>&, std::mt19937&){};
        std::lock_guard lock(queue);

        if (!waitQueue.empty())
        {
            output = waitQueue.back();
            waitQueue.pop_back();
        }

        return output;
    }

    void threadMainLoop(std::uniform_int_distribution<>& paraChance, std::mt19937& generator)
    {
        while(!shutdown)
        {
            std::function<void(std::uniform_int_distribution<>&, std::mt19937&)> job = getJob();
            job(paraChance, generator);
        }
    }

public:
    ThreadPool(const int maximumThreads)
    {
        shutdown = false;
        maxThreads = maximumThreads;
        threads = new std::thread*[maximumThreads];
        for (int x=0; x<maximumThreads; x++) threads[x] = new std::thread([this]()
        {
            std::uniform_int_distribution paraChance(1,4);
            std::mt19937 randomNumberGenerator(std::time(nullptr));

            threadMainLoop(paraChance, randomNumberGenerator);
        });
    }

    void addToQueue(const std::function<void(std::uniform_int_distribution<>&, std::mt19937&)>& job)
    {
        std::lock_guard lock(queue);
        waitQueue.emplace_back(job);
    }

    void join()
    {
        bool cont = true;
        while(cont)
        {
            std::lock_guard lock(queue);
            cont = !waitQueue.empty();
        }
    }

    ~ThreadPool()
    {
        shutdown = true;
        for (int x=0; x<maxThreads; x++)
        {
            threads[x]->join();
            delete threads[x];
        }
        delete[] threads;
    }
};

int main()
{
    volatile int maxParalisysTurns = 0;
    volatile double numJobsRun = 0;
    std::mutex turnCheckMutex, jobsRunMutex;
    ThreadPool pool(100);

    std::thread tracker([&numJobsRun, &jobsRunMutex]()
    {
        int percentFinished = 0;
        for (int x=0; x<100; x++) std::cout << "🮐" << std::flush;

        do
        {
            std::lock_guard lock(jobsRunMutex);
            const int prevPercent = percentFinished;
            percentFinished = (numJobsRun/1000000000)*100;

            if (percentFinished > prevPercent)
            {
                std::cout << "\r" << std::flush;

                for (int x=0; x<percentFinished; x++) std::cout << "🮋" << std::flush;
                for (int x=percentFinished; x<100; x++) std::cout << "🮐" << std::flush;
            }
        }
        while(percentFinished < 100);
    });

    for (int x=0; x<1000000000; x++)
    {
        pool.addToQueue([&maxParalisysTurns, &turnCheckMutex, &numJobsRun, &jobsRunMutex](std::uniform_int_distribution<>& paraChance, std::mt19937& generator)
        {
            int safeTurns = 55;
            int totalTurns = 0;

            while (safeTurns > 0)
            {
                const int randRoll = paraChance(generator);
                safeTurns = (safeTurns - 1) + (randRoll >> 2);
                totalTurns++;
            }

            std::lock_guard lock(turnCheckMutex);
            std::lock_guard lock1(jobsRunMutex);
            totalTurns -= 55;
            if (totalTurns > maxParalisysTurns) maxParalisysTurns = totalTurns;
            numJobsRun++;
        });
    }

    pool.join();
    tracker.join();

    std::cout << std::endl << "Highest Paralisys Turns: " << maxParalisysTurns << std::endl;
    return 0;
}
