#ifndef TWINE_TWINE_H_
#define TWINE_TWINE_H_

namespace twine {

    bool current_thread_is_realtime();

    void init_xenomai();

}// namespace twine

#endif // TWINE_TWINE_H_