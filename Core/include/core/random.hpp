#include <stdlib.h> // for rand() and srand()
#include <time.h>   // for time()

// Seed the random generator (call once at program start)
inline void seedRandom() { srand((unsigned int)time(NULL)); }

// Returns a random integer between min and max inclusive
inline int range(int min, int max) { return min + rand() % (max - min + 1); }

// Returns a random float between min and max inclusive
inline float range(float min, float max) {
  float scale = (float)rand() / (float)RAND_MAX;
  return min + scale * (max - min);
}

// Returns a random double between min and max inclusive
inline double range(double min, double max) {
  double scale = (double)rand() / (double)RAND_MAX;
  return min + scale * (max - min);
}

// Returns a random number between 0 and 1 inclusive
inline double random01() { return (double)rand() / (double)RAND_MAX; }
