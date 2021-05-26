#ifndef ROBAKI_RANDOMGENERATOR_H
#define ROBAKI_RANDOMGENERATOR_H

#include <cstdint>

namespace Worms {
    class RandomGenerator {
    private:
        uint32_t next_val;
    public:
        explicit RandomGenerator(uint32_t seed) : next_val{seed} {}

        uint32_t operator()() {
            uint32_t const ret = next_val;
            next_val = (static_cast<uint64_t>(next_val) * 279410273ULL) % 4294967291;
            return ret;
        }
    };
}

#endif //ROBAKI_RANDOMGENERATOR_H
