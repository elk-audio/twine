#include <random>
#include <array>
#include <iostream>
#include <string>
#include <cstring>
#include <thread>

#include <getopt.h>
#include <sys/mman.h>

#ifdef TWINE_BUILD_WITH_XENOMAI
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cobalt/pthread.h>
#include <xenomai/init.h>
#pragma GCC diagnostic pop
#endif

#include "twine/twine.h"
#include "twine_internal.h"

/*
 * Tool for stress testing WorkerPool implementations with
 * variable number of cores and worker threads.
 */

constexpr int DEFAULT_CORES = 4;
constexpr int DEFAULT_WORKERS = 10;
constexpr int MAX_LOAD = 300;
constexpr int DEFAULT_ITERATIONS = 10000;

/* iir parameters: */
constexpr float CUTOFF = 0.2f;
constexpr float Q = 0.5f;
constexpr float w0 = 2.0f * M_PI * CUTOFF;
constexpr float w0_cos = std::cos(w0);
constexpr float w0_sin = std::sin(w0);
constexpr float alpha = w0_sin / Q;
constexpr float norm = 1.0f / (1.0f + alpha);

constexpr float co_a1 = -2.0f * w0_cos * norm;
constexpr float co_a2 = (1 - alpha) * norm;
constexpr float co_b0 = (1.0f - w0_cos) / 2.0f * norm;;
constexpr float co_b1 = (1 - w0_cos) * norm;
constexpr float co_b2 = co_b0;

using AudioBuffer = std::array<float, 128>;
using FilterRegister = std::array<float, 2>;
using TimeStamp = std::chrono::nanoseconds;


/* Biquad implementation to keep the cpu busy */
template <size_t length>
void process_filter(std::array<float, length>& buffer, FilterRegister& mem)
{
    for (unsigned int i = 0; i < length; ++i)
    {
        float w = buffer[i] - co_a1 * mem[0] - co_a2 * mem[1];
        buffer[i] = co_b1 * mem[0] + co_b2 * mem[1] + co_b0 * w;
        mem[1] = mem[0];
        mem[0] = w;
    }
}

struct TimeStats
{
    TimeStamp min_time{std::numeric_limits<int64_t>::max()};
    TimeStamp max_time{0};
    TimeStamp mean_time{0};
};

void update_stats(TimeStats& data, TimeStamp new_time)
{
    data.min_time = std::min(data.min_time, new_time);
    data.max_time = std::max(data.max_time, new_time);
    data.mean_time = (data.mean_time * 99 + new_time) / 100;
}

struct ProcessData
{
    AudioBuffer buffer;
    FilterRegister mem;
    TimeStamp start_time{0};
    TimeStamp end_time{0};

    TimeStats total;
    TimeStats start;

    int count{0};
    int id{0};
};

void worker_function(void* data)
{
    auto process_data = reinterpret_cast<ProcessData*>(data);
    auto start_time = twine::current_rt_time();

    int iters = MAX_LOAD;
    for (int i = 0; i < iters; ++i)
    {
        process_filter(process_data->buffer, process_data->mem);
    }

    process_data->start_time = start_time;
    process_data->end_time = twine::current_rt_time();

    process_data->count++;
}

std::string to_error_string(twine::WorkerPoolStatus status)
{
    switch (status)
    {
        case twine::WorkerPoolStatus::PERMISSION_DENIED:
            return "Permission denied";

        case twine::WorkerPoolStatus::LIMIT_EXCEEDED:
            return "Thread count limit exceeded";

        default:
            return "Error";
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
#endif
#ifndef TWINE_BUILD_WITH_XENOMAI
void xenomai_thread_init()
{
    std::cout << "Test not built with xenomai support!" << std::endl;
}
#endif


std::tuple<int, int, int, bool, bool> parse_opts(int argc, char** argv)
{
    int workers = DEFAULT_WORKERS;
    int cores = DEFAULT_CORES;
    int iters = DEFAULT_ITERATIONS;
    bool xenomai = false;
    bool print_timings = false;
    signed char c;

    while ((c = getopt(argc, argv, "w:c:i:xt")) != -1)
    {
        switch (c)
        {
            case 'w':
                workers = atoi(optarg);
                break;
            case 'c':
                cores = atoi(optarg);
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
                std::cout << "Options are: -w[n of worker threads], -c[n of cores], -i[n of iterations], -x - use xenomai threads, -t - print timings for each iteration" << std::endl;
                abort();

            default:
                abort();
        }
    }
    return std::make_tuple(workers, cores, iters, xenomai,
                           print_timings);
}


void update_timings(std::vector<ProcessData>* data, int iter, bool xenomai, bool print, TimeStamp start_time, TimeStamp end_time)
{
    static float min_time{10000000};
    static float max_time{0};
    static float mean_time{0};

    float current_total = (end_time - start_time).count() / 1000.0f;
    max_time = std::max(max_time, current_total);
    min_time = std::min(min_time, current_total);
    mean_time = (4.0f * mean_time + current_total) / 5;

    if (print)
    {
        if (xenomai)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
            __cobalt_printf("Iteration %i: total time: %.1f us, avg: %.1f us, min: %.1f us, max: %.1f us\n", iter, current_total, mean_time, min_time, max_time);
#endif
        } else
        {
            std::cout << "Iteration: " << iter << ", total time: " << current_total << " us, avg: " << mean_time
                      << " us, min: " << min_time << " us, max: " << max_time << " us\n";
        }
    }

    int id = 0;
    for(auto& w : *data)
    {
        auto process_time = std::chrono::duration_cast<std::chrono::microseconds>(w.end_time - w.start_time);
        auto offset_time = std::chrono::duration_cast<std::chrono::microseconds>(w.start_time - start_time);
        if (print)
        {
            if (xenomai)
            {
#ifdef TWINE_BUILD_WITH_XENOMAI
                __cobalt_printf("Worker %i: start offset: %i us, total: %i us\n", id, offset_time.count(), process_time.count());
#endif
            } else
            {
                std::cout << "Worker " << id << ": start offset: " << offset_time.count() << " us, total: "
                          << process_time.count() << " us, " << std::endl;
            }
        }
        update_stats(w.total, process_time);
        update_stats(w.start, offset_time);
        id++;
    }

}

void print_iterations(int iter, bool xenomai)
{
    if ((iter + 1) % 10 == 0)
    {
        if (xenomai)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
            __cobalt_printf("\rIterations: %i", iter);
#endif
        }
        else
        {
            std::cout << "\rIterations: " << iter;
            std::cout.flush();
        }
    }
}

void print_final_stats(const std::vector<ProcessData>& data)
{
    for(unsigned int i = 0; i < data.size(); ++i)
    {
        const auto& w = data[i];
        std::cout << "Worker " << i << ": Total time: avg: " << w.total.mean_time.count() / 1000.0  <<
                                            " us, min: " << w.total.min_time.count() / 1000.0 <<
                                            " us, max: " << w.total.max_time.count() / 1000.0 <<
                                            " us;\t Thread start time: avg: " << w.start.mean_time.count() / 1000.0 <<
                                            " us, min: " << w.start.min_time.count() / 1000.0 <<
                                            " us, max: " << w.start.max_time.count() / 1000.0 << "us. " << std::endl;
    }
}

void* run_stress_test(void* data)
{
    twine::set_flush_denormals_to_zero();
    auto [pool, process_data, iters, xenomai, print_timings] = *(reinterpret_cast<std::tuple<twine::WorkerPool*, std::vector<ProcessData>*, int, bool, bool>*>(data));
    for (int i = 0; i < iters; ++i)
    {
        auto start_time = twine::current_rt_time();

        // Run all workers
        pool->wakeup_and_wait();

        auto end_time = twine::current_rt_time();
        if ((i + 1) % 10 == 0)
        {
            if (!print_timings)
            {
                print_iterations(i, xenomai);
            }
            if (xenomai)
            {
#ifdef TWINE_BUILD_WITH_XENOMAI
                /* Since xenomai runs with realtime priority, we can't max out cpu usage as
                 * that would starve the linux kernel, so we leave a time slice for it here */
                timespec t;
                t.tv_sec = 0;
                t.tv_nsec = 5000000;
                __cobalt_nanosleep(&t, nullptr);
#endif
            }
        }
        update_timings(process_data, i, xenomai, print_timings, start_time, end_time);
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
#endif
}

int main(int argc, char **argv)
{
    auto [workers, cores, iters, xenomai, timings] = parse_opts(argc, argv);

    std::vector<ProcessData> data;
    data.reserve(workers);
    auto worker_pool = twine::WorkerPool::create_worker_pool(cores);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1, 1);

    for (int i = 0; i < workers; ++i)
    {
        ProcessData d;
        d.mem = {0,0};
        d.id = i;
        for (auto& b : d.buffer)
        {
            b = dist(gen);
        }

        data.push_back(d);
        auto res = worker_pool->add_worker(worker_function, &data[i]);
        if (res != twine::WorkerPoolStatus::OK)
        {
            std::cout << "Failed to start workers: " << to_error_string(res) << std::endl;
            return -1;
        }
    }
    auto test_data = std::make_tuple(worker_pool.get(), &data, iters, xenomai, timings);

    if (xenomai)
    {
        run_stress_test_in_xenomai_thread(&test_data);

    }
    else
    {
        run_stress_test(&test_data);
    }

    std::cout << "\n" << iters << " iterations" << std::endl;
    print_final_stats(data);

    return 0;
}
