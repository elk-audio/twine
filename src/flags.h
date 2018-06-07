#ifndef TWINE_FLAGS_H
#define TWINE_FLAGS_H

namespace twine {

/**
 * @brief Used to signal that the current thread is
 */
class ThreadRtFlag
{
public:
    ThreadRtFlag()
    {
        _instance_counter += 1;
    }
    ~ThreadRtFlag()
    {
        _instance_counter -= 1;
    }

    static bool is_realtime()
    {
        return _instance_counter > 0;
    }

private:
    static thread_local int _instance_counter;
};

class XenomaiRtFlag
{
public:
    void set(bool enabled)
    {
        _enabled = enabled;
    }
    bool is_set()
    {
        return _enabled;
    }
private:
    static bool _enabled;
};
} // namespace yarn

#endif //TWINE_FLAGS_H
