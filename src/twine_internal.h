#ifndef TWINE_TWINE_INTERNAL_H
#define TWINE_TWINE_INTERNAL_H

namespace twine {
/**
 * @brief Signal to twine that Worker Pools should use the xenomai thread api and
 *        not the default std::thread implementation.
 *        Must be called before creating any Worker Pools. Not indented to be called
 *        from processors or plugins.
 */
void init_xenomai();

/**
 * @brief Used to signal that the current thread is a realtime thread
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

} // namespace twine
#endif //TWINE_TWINE_INTERNAL_H
