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

} // namespace twine
#endif //TWINE_TWINE_INTERNAL_H
