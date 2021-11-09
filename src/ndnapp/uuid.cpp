// TODO: add copyright

#include "uuid.hpp"

namespace uuid 
{

    auto randomlySeededMersenneTwister() {
        // Magic number 624: The number of unsigned ints the MT uses as state
        std::vector<unsigned int> random_data(624);
        std::random_device source;
        std::generate(begin(random_data), end(random_data), [&]() {return source(); });
        std::seed_seq seeds(begin(random_data), end(random_data));
        std::mt19937 seededEngine(seeds);
        return seededEngine;
    }

    static std::mt19937                    gen(randomlySeededMersenneTwister());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::string generate_uuid_v4() {
        std::stringstream ss;
        int i;
        ss << std::hex;
        for (i = 0; i < 8; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 4; i++) {
            ss << dis(gen);
        }
        ss << "-4";
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        ss << dis2(gen);
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 12; i++) {
            ss << dis(gen);
        };
        return ss.str();
    }

}