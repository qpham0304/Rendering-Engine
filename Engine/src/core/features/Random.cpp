#include "Random.h"

std::random_device Random::rd;
std::mt19937 Random::gen(Random::rd());

int Random::GenInt(int min, int max) {
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}

float Random::GenFloat(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

double Random::GenDouble(double min, double max) {
    std::uniform_real_distribution<double> dis(min, max);
    return dis(gen);
}

uint64_t Random::GenUUID() {
    std::uniform_int_distribution<uint64_t> dist;
    return dist(gen);
}
