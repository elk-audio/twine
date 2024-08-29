#include <iostream>
#include <cstring>
#include <csignal>
#include <cstdlib>
#include <chrono>
#include <exception>

#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include <evl/evl.h>
#include <evl/syscall.h>

#define TWINE_EXPOSE_INTERNALS 1
#include "twine/twine.h"

constexpr int DEFAULT_NUM_ITERATIONS = 1000;
static unsigned triggers = 0;
static unsigned wakeups = 0;
static int stop_flag = 0;
pthread_t main_rt_task;
pthread_t main_nrt_task;
std::unique_ptr<twine::RtConditionVariable> _input_trigger_cond_var;
std::unique_ptr<twine::RtConditionVariable> _output_trigger_cond_var;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_flag = 1;
}

static void* main_rt_thread(void* data)
{
    int efd = evl_attach_self("/condvar_rt_duplex:%d", getpid());
    if (efd < 0)
    {
        throw std::runtime_error(strerror(errno));
    }

    while (!stop_flag)
    {
        _input_trigger_cond_var->wait();
        _output_trigger_cond_var->notify();
        if (evl_is_inband())
        {
            std::cout << "Rt thread in NRT context ! Exiting thread...\n";
            return nullptr;
        }
    }

    return nullptr;
}

static void* worker_func_non_rt(void* data)
{
    while (!stop_flag)
    {
        _output_trigger_cond_var->wait();
        wakeups++;
    }
    return nullptr;
}

int init_nrt_thread()
{
    struct sched_param rt_params = {.sched_priority = 90};
    pthread_attr_t task_attributes;
    pthread_attr_init(&task_attributes);
    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);

    auto res = pthread_create(&main_nrt_task, &task_attributes,
                                worker_func_non_rt, nullptr);
    return res;
}

int init_rt_thread()
{
    struct sched_param rt_params = {.sched_priority = 90};
    pthread_attr_t task_attributes;
    pthread_attr_init(&task_attributes);
    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);

    auto res = pthread_create(&main_rt_task, &task_attributes,
                                main_rt_thread, nullptr);
    return res;
}

void xenomai_thread_init()
{
    evl_init();
    twine::init_xenomai();
}

void print_usage(char *argv[])
{
    std::cout << "\n\nTwine RT condition variable full duplex test (RT <-> NRT).\n" <<
            "Tests wakeup of condition variables from one context (RT/NRT) to another.\n";
    std::cout << "Usage: \n\n";
    std::cout << argv[0] << " [-n]\n\n";
    std::cout << "Options:\n";
    std::cout << "    -h               : Help for usage options.\n";
    std::cout << "    -n <num iterations> : Specify the number of iterations. \n"
           "                              Default is " << DEFAULT_NUM_ITERATIONS << "\n";
    std::cout << "    - stop the program with SIGINT\n";
}

int main(int argc, char **argv)
{
    int res = 0;
    int option = 0;
    if (argc < 2)
    {
        std::cout << "\n\nRunning default Iterations size of " << DEFAULT_NUM_ITERATIONS << "\n";
        std::cout << "For help use "<< argv[0] <<  "[-h]\n";
    }
    int num_iterations = DEFAULT_NUM_ITERATIONS;

    while ((option = getopt(argc, argv,"hn:")) != -1)
    {
        switch (option)
        {
        case 'h' :
            print_usage(argv);
            exit(-1);
            break;

        case 'n' :
            num_iterations = atoi(optarg);
            break;

        default:
            print_usage(argv);
            exit(-1);
            break;
        }
    }

    signal(SIGINT, sigint_handler);
    xenomai_thread_init();
    _input_trigger_cond_var =
            twine::RtConditionVariable::create_rt_condition_variable();
    _output_trigger_cond_var =
            twine::RtConditionVariable::create_rt_condition_variable();
    init_nrt_thread();
    init_rt_thread();
    int i = 0;
    while (i < num_iterations)
    {
        usleep(10000);
        triggers++;
        _input_trigger_cond_var->notify();
        i++;
    }
    stop_flag = 1;
    usleep(10000);
    std::cout << "\n Results:\n\tTriggers = " << triggers << " Wakeups: " << wakeups << "\n";
    return 0;
}
