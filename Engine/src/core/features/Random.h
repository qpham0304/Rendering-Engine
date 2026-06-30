#include <random>

class Random {
public:
    Random() = delete;

    static int GenInt(int min, int max);
    static float GenFloat(float min, float max);
    static double GenDouble(double min, double max);
    static uint64_t GenUUID();

private:
    static std::random_device rd;
    static std::mt19937 gen;
};