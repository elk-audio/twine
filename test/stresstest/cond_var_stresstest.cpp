#include <random>
#include <array>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <thread>
#include <atomic>

#ifdef TWINE_WINDOWS_THREADING
#include "getopt_win.h"
#else
#include <getopt.h>
#include <sys/mman.h>
#endif
#ifdef TWINE_BUILD_WITH_XENOMAI
#include "elk-warning-suppressor/warning_suppressor.hpp"

ELK_PUSH_WARNING

ELK_DISABLE_UNUSED_PARAMETER
#include <cobalt/pthread.h>
#include <xenomai/init.h>
ELK_POP_WARNING
#elif TWINE_BUILD_WITH_EVL
    #include <evl/evl.h>
    #include <evl/thread.h>
    #include <evl/proxy.h>
#endif

#include "twine/twine.h"
#include "twine_internal.h"

/*
 * Tool for stress testing Condition Variable implementations.
 *
 * The test spawns 1 worker thread per condition variable and notifies
 * these at random intervals while counting the number of wake ups.
 * Ideally the number of wake ups should be equal or very close to
 * the number of notifications sent.
 */

constexpr int DEFAULT_INSTANCES = 4;
constexpr int DEFAULT_ITERATIONS = 10000;
constexpr auto INTERVAL = std::chrono::microseconds (2500);

constexpr int NOTIFICATION_INTENSITY_MIN = 20;
constexpr int NOTIFICATION_INTENSITY_MAX = 1;
constexpr int PRINT_INTERVAL = 17;

struct ProcessData
{
    twine::RtConditionVariable* cond_var;
    uint64_t*                   counter;
    std::atomic_bool*           run;
};

void worker_function(ProcessData data)
{
    while(*data.run)
    {
        if (data.cond_var->wait())
        {
            *data.counter += 1;
        }
    }
}

#ifdef TWINE_BUILD_WITH_XENOMAI
void xenomai_thread_init()
{
    // For some obscure reasons, xenomai_init() crashes
    // if argv is allocated here on the stack, so we malloc it
    // beforehand.
    int argc = 1;
    char** argv = (char**) malloc(2 * sizeof(char*));
    argv[0] = (char*) malloc(32 * sizeof(char));
    argv[1] = nullptr;
    strcpy(argv[0], "stress_test");
    optind = 1;
    xenomai_init(&argc, (char* const**) &argv);
    free(argv[0]);
    free(argv);
    mlockall(MCL_CURRENT|MCL_FUTURE);
    twine::init_xenomai();
}
#elif TWINE_BUILD_WITH_EVL
void xenomai_thread_init()
{
    evl_init();
    twine::init_xenomai();
}
#else
void xenomai_thread_init()
{
    std::cout << "Test not built with xenomai support!" << std::endl;
}
#endif


std::tuple<int, int, bool, bool> parse_opts(int argc, char** argv)
{
    int instances = DEFAULT_INSTANCES;
    int iters = DEFAULT_ITERATIONS;
    bool xenomai = false;
    bool print_timings = false;
    signed char c;

    while ((c = getopt(argc, argv, "c:i:xt")) != -1)
    {
        switch (c)
        {
            case 'c':
                instances = atoi(optarg);
                break;
            case 'i':
                iters = atoi(optarg);
                break;
            case 't':
                print_timings = true;
                break;
            case 'x':
                if (!xenomai)
                {
                    xenomai_thread_init();
                    xenomai = true;
                }
                break;
            case '?':
                std::cout << "Options are: -c[n of condition variable instances], -i[n of iterations], -x - use xenomai threads, -t - print timings for each iteration" << std::endl;
                abort();
            default:
                abort();
        }
    }
    return std::make_tuple(instances, iters, xenomai, print_timings);
}

void print_iterations(int64_t iter, bool xenomai)
{
    if ((iter + 1) % PRINT_INTERVAL == 0)
    {
        if (xenomai)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
            __cobalt_printf("\rIterations: %i", iter);
#elif TWINE_BUILD_WITH_EVL
            evl_printf("\rIterations: %i", iter);
#endif
        }
        else
        {
            std::cout << "\rIterations: " << iter;
            std::cout.flush();
        }
    }
}

void* run_stress_test(void* data)
{
    auto [cond_vars, frequencies, counts, iters, xenomai, print_timings] =
               *(reinterpret_cast<std::tuple<std::vector<std::unique_ptr<twine::RtConditionVariable>>*,
                                  std::vector<int>*,
                                  std::vector<uint64_t>*,
                                  int,
                                  bool,
                                  bool>*>(data));

#ifdef TWINE_BUILD_WITH_EVL
    if (xenomai)
    {
        evl_attach_self("/condvar_stress_test_main");
    }
#endif
    for (int64_t iter = 0; iter < iters; ++iter)
    {
        for (int i = 0; i < static_cast<int>(cond_vars->size()); ++i)
        {
            if (iter % frequencies->at(i) == 0)
            {
                counts->at(i)++;
                cond_vars->at(i)->notify();
            }
        }
        if (print_timings)
        {
            print_iterations(iter, xenomai);
        }

#ifdef TWINE_BUILD_WITH_XENOMAI
        if (xenomai)
        {
            /* Since xenomai runs with realtime priority, we can't max out cpu usage as
            * that would starve the linux kernel, so we leave a time slice for it here */
            timespec t;
            t.tv_sec = 0;
            t.tv_nsec = INTERVAL.count();
            __cobalt_nanosleep(&t, nullptr);
        }
        else
#elif TWINE_BUILD_WITH_EVL
        if (xenomai)
        {
            evl_usleep(INTERVAL.count() / 1000);
        }
        else
#endif
        {
            std::this_thread::sleep_for(INTERVAL);
        }
    }
    return nullptr;
}

void run_stress_test_in_xenomai_thread([[maybe_unused]] void* data)
{
#ifdef TWINE_BUILD_WITH_XENOMAI
    /* Threadpool must be controlled from another xenomai thread */
    struct sched_param rt_params = { .sched_priority = 80 };
    pthread_attr_t task_attributes;
    __cobalt_pthread_attr_init(&task_attributes);

    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);
    pthread_t thread;

    auto res = __cobalt_pthread_create(&thread, &task_attributes, &run_stress_test, data);
    if (res != 0)
    {
        std::cout << "Failed to start xenomai thread: " << strerror(res) <<std::endl;
    }
    /* Wait for the xenomai thread to finish */
    __cobalt_pthread_join(thread, nullptr);
#elif TWINE_BUILD_WITH_EVL
    struct sched_param rt_params = { .sched_priority = 80 };
    pthread_attr_t task_attributes;
    pthread_attr_init(&task_attributes);

    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);
    pthread_t thread;

    auto res = pthread_create(&thread, &task_attributes, &run_stress_test, data);
    if (res != 0)
    {
        std::cout << "Failed to start EVL thread: " << strerror(res) <<std::endl;
    }

    pthread_join(thread, nullptr);
#endif
}

void print_results(const std::vector<uint64_t>& rt_counts, const std::vector<uint64_t>& non_rt_counts)
{
    std::cout << std::endl;
    for (int i = 0; i < static_cast<int>(rt_counts.size()); ++i)
    {
        std::cout << "Condition variable: " << i << "\t notifications: "<< rt_counts[i] << " \t wake ups: "
            << non_rt_counts[i] << " \t missed wakeups: "
            << static_cast<int64_t>(rt_counts[i]) -  static_cast<int64_t>(non_rt_counts[i]) << std::endl;
    }
}


int main(int argc, char **argv)
{
    auto [instances, iters, xenomai, timings] = parse_opts(argc, argv);

    std::vector<std::thread> non_rt_threads;
    std::vector<uint64_t> rt_counts(instances, 0);
    std::vector<uint64_t> non_rt_counts(instances, 0);
    std::vector<int> frequencies;
    std::vector<std::unique_ptr<twine::RtConditionVariable>> cond_vars;
    std::atomic_bool run = true;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(NOTIFICATION_INTENSITY_MAX, NOTIFICATION_INTENSITY_MIN);

    for (int i = 0; i < instances; ++i)
    {
        cond_vars.emplace_back(twine::RtConditionVariable::create_rt_condition_variable());

        ProcessData d;
        d.cond_var = cond_vars.back().get();
        d.counter = &non_rt_counts[i];
        d.run = &run;
        auto thread = std::thread(worker_function, d);
        non_rt_threads.push_back(std::move(thread));

        /* Frequency should be interpreted as "notify that variable every nth interrupt" */
        frequencies.push_back(dist(gen));
    }
    auto test_data = std::make_tuple(&cond_vars, &frequencies, &rt_counts, iters, xenomai, timings);
    if (xenomai)
    {
        run_stress_test_in_xenomai_thread(&test_data);
    }
    else
    {
        run_stress_test(&test_data);
    }
    run.store(false);


    for (int i = 0; i < instances ; ++i)
    {
        cond_vars[i]->notify();
        non_rt_threads[i].join();
    }
    print_results(rt_counts, non_rt_counts);
    return 0;
}
