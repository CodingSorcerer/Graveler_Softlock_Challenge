#include <functional>
#include <iostream>
#include <random>
#include <thread>

/*
 * Classs: Threadpool
 * Threadpool is designed to keep threads alive that will share the work of the program. These threads area always running
 * and simply checck the queue for work to do. This saves time spinning up threads on the fly for each battle.
 */
class ThreadPool
{
    int maxThreads; //This is the maximum nuber of threads allowed to run at the same time. Computers may throw errors if this is set too high.
    std::vector<std::function<void(std::uniform_int_distribution<>&, std::mt19937&)>> waitQueue; //all battles get queued here
    std::thread** threads; //this keeps track of each created thread. Think of it as a list of pointers.
    std::mutex queue; //this is the mutex, locking this prevents multiple threads from accessing the queue at the same time.
    bool shutdown; //setting this to true, tells the threadpool that it's time to stop all threads.

    /*
     * This private method gets the next job from the queue. It will lock the queue mutex, then check to see if the
     * wait queue has a job. If there is a job, that job is passed out and removed from the queue. If the queue is
     * empty, it returns a function that does nothing.
     */
    std::function<void(std::uniform_int_distribution<>&, std::mt19937&)> getJob()
    {
        std::function output = [](std::uniform_int_distribution<>&, std::mt19937&){};
        std::lock_guard lock(queue); //this lock ends when the variable goes out of scope

        if (!waitQueue.empty())
        {
            output = waitQueue.back();
            waitQueue.pop_back();
        }

        return output;
    }

    /*
     * this is the main processing loop for the threads, basically it just checks the thread queue, then runs them.
     * Dont worry about the parameters, those will be discussed later on in the file.
     */
    void threadMainLoop(std::uniform_int_distribution<>& paraChance, std::mt19937& generator)
    {
        while(!shutdown)
        {
            std::function<void(std::uniform_int_distribution<>&, std::mt19937&)> job = getJob();
            job(paraChance, generator);
        }
    }

public:
    /*
     * This is the constuctor for the thread pool. It only takes in the max number of threads. It then creates the
     * threads and the job queue.
     */
    ThreadPool(const int maximumThreads)
    {
        shutdown = false; //set this to false, we just started up!
        maxThreads = maximumThreads; //make sure we remember how many threads we made (important later for freeing memory)
        threads = new std::thread*[maximumThreads]; //Create an array of pointers
        //the line below creats a number of threads specified by max threads and passes them a lambda function
        for (int x=0; x<maximumThreads; x++) threads[x] = new std::thread([this]()
        {
            /*
             * this is the body of the lambda function
             * note that each thread has it's own random number generator. they could all share one to save ram... however
             * this would create a bottleneck as rng generators are not thread safe and would have to be locked behind
             * a mutex.
             */
            std::uniform_int_distribution paraChance(1,4); //this creates a list of int 1 through 4. This is our dice.
            std::mt19937 randomNumberGenerator(std::time(nullptr)); //this is a random number genrator seeded by the system clock.

            threadMainLoop(paraChance, randomNumberGenerator);//use the thread's rng in the main loop
        });
    }

    /*
     * this allows the caller to add a job to the queue. that job can be any function that takess in a dice distrobution
     * and a random number generator.
     */
    void addToQueue(const std::function<void(std::uniform_int_distribution<>&, std::mt19937&)>& job)
    {
        std::lock_guard lock(queue);
        waitQueue.emplace_back(job);
    }

    /*
     * This function will wait for all jobs in the queue to finish before returning. This allows the caller to wait for
     * the jobs to be done before ending the program.
     */
    void join()
    {
        bool cont = true;
        while(cont)
        {
            std::lock_guard lock(queue);
            cont = !waitQueue.empty();
        }
    }

    /*
     * This is the deconstructor. It will allow us to clean up the mess we made in RAM memory.
     */
    ~ThreadPool()
    {
        shutdown = true; //tell the threads to shut down.
        for (int x=0; x<maxThreads; x++) //this is why we kept track of the max number of threads
        {
            threads[x]->join(); //wait for the thread to complete
            delete threads[x]; //delete the thread
        }
        delete[] threads; //delete the list of threads
        //we are now free! All memory cleaned up!
    }
};

/*
 * Now i'm sure you're tired of hearing about threads. this is the main function where the acutal pokemon battles are
 * simulated.
 */
int main()
{
    //note the volitile qualifier lets the compiler know that these variables are accessed by many different threads
    volatile int maxParalisysTurns = 0; //this will keep track of the number of turns gravler was paralyzed
    volatile double numJobsRun = 0; //this is used to keep track of the number of jobs completed, so we can dipslay progress
    std::mutex turnCheckMutex, jobsRunMutex; //these are the mutexes that guard the above two integers
    ThreadPool pool = 100; //create a pool of 100 threads! Could probably be set higher but this is what I tested with.

    /*
     * This is the tracker thread. It's only job is to print a loading bar on the screen while the program runs.
     * This lets you know about how close the program is to being done. Helps to know that it's actually running.
     */
    std::thread tracker([&numJobsRun, &jobsRunMutex]()
    {
        int percentFinished = 0; //start at 0%
        for (int x=0; x<100; x++) std::cout << "░" << std::flush; //print out an unfilled bar with 100 tiles

        do
        {
            std::lock_guard lock(jobsRunMutex);
            const int prevPercent = percentFinished; //record the previous percent
            percentFinished = (numJobsRun/1000000000)*100; //check what percent completion we're currently at

            if (percentFinished > prevPercent) //if the completed percentage went up, fill the bar accordingly
            {
                std::cout << "\r" << std::flush;

                for (int x=0; x<percentFinished; x++) std::cout << "█" << std::flush;
                for (int x=percentFinished; x<100; x++) std::cout << "░" << std::flush;
            }
        }
        while(percentFinished < 100); //if we reach 100% and print a full bar, we can quit
    });

    /*
     * Now that we have our loading bar created, we can start the threads for the pokemon battles! Here we go, 1 billion
     * battles!
     */
    for (int x=0; x<1000000000; x++)
    {
        //add all 1 billion battles to the queue
        pool.addToQueue([&maxParalisysTurns, &turnCheckMutex, &numJobsRun, &jobsRunMutex](std::uniform_int_distribution<>& paraChance, std::mt19937& generator)
        {
            int safeTurns = 55; //gravler has 55 status turns before it must self destruct
            int totalTurns = 0; //how many turns the battle lasted in total

            while (safeTurns > 0) //if safe turns are depleated we self destruct!!! That ends the simulation
            {
                const int randRoll = paraChance(generator); //roll for paralisys
                safeTurns = (safeTurns - 1) + (randRoll >> 2); //bit shifting by 2 is the same as deviding by 4.
                                                               //this means that we only decrement safe turns if
                                                               //a 4 is not rolled. 4 represents paralasys triggering.
                totalTurns++; //increment the number of turns taken
            }

            std::lock_guard lock(turnCheckMutex);
            std::lock_guard lock1(jobsRunMutex);
            totalTurns -= 55; //subtract our 55 garunteed turns from the value to get just the paralisys turns
            if (totalTurns > maxParalisysTurns) maxParalisysTurns = totalTurns; //if we got more paralisys than before, record it!
            numJobsRun++; //update our progress!
        });
    }

    pool.join(); //wait for all threads to finish
    tracker.join(); //wait for tracker to finish

    std::cout << std::endl << "Highest Paralisys Turns: " << maxParalisysTurns << std::endl; //print our results
    return 0;
}
