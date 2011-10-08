#ifndef PERLINNOISE_H
#define PERLINNOISE_H

#include <stdio.h>
extern "C"{
#include <clib/rseq.h>
#include <clib/random/mt19937ar.h>
#include <clib/timemeas.h>
}
#include <clib/random/tinymt32.h>
#include <cpplib/RandomSequence.h>

namespace PerlinNoise{

/// \brief Callback object that receives result of perlin_noise
///
/// We do this because not all application needs temporary memry to store the result.
class PerlinNoiseCallback{
public:
	virtual void operator()(float, int ix, int iy) = 0;
};

/// \brief Callback object that will assign noise values to float 2-d array field.
template<int CELLSIZE>
class FieldAssign : public PerlinNoiseCallback{
	typedef float (&fieldType)[CELLSIZE][CELLSIZE];
	fieldType field;
public:
	FieldAssign(fieldType field) : field(field){}
	void operator()(float f, int ix, int iy){
		field[ix][iy] = f;
	}
};

/// \brief Parameters given to perlin_noise.
struct PerlinNoiseParams{
	long seed;
	int xofs, yofs;
	double persistence;

	PerlinNoiseParams(
		long seed = 0,
		double persistence = 0.5,
		int xofs = 0,
		int yofs = 0)
		: seed(seed),
		persistence(persistence),
		xofs(xofs),
		yofs(xofs)
	{
	}
};

template<int CELLSIZE>
inline void perlin_noise(long seed, PerlinNoiseCallback &callback, int xofs = 0, int yofs = 0){
	perlin_noise<CELLSIZE>(PerlinNoiseParams(seed, 0.5, xofs, yofs), callback);
}

/// \brief Generate Perlin Noise and return it through callback.
/// \param param Parameters to generate the noise.
/// \param callback Callback object to receive the result.
template<int CELLSIZE>
void perlin_noise(const PerlinNoiseParams &param, PerlinNoiseCallback &callback){
	double persistence = param.persistence;
	int work[CELLSIZE][CELLSIZE];
	int work2[CELLSIZE][CELLSIZE] = {0};
	int maxwork2 = 0;
	int octave;
	int xi, yi;

	// Temporarily save noise patturn for use as the source signal of fractal noise.
	timemeas_t tm;
	TimeMeasStart(&tm);
//	init_genrand(param.seed);
//	TINYMT32_T tmt;
//	tinymt32_init(&tmt, param.seed);
	for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++){
//		unsigned long seeds[3] = {param.seed, (xi + param.xofs), (yi + param.yofs)};
//		init_by_array(seeds, 3);
//		tinymt32_init(&tmt, param.seed ^ (xi + param.xofs) + (yi + param.yofs) * 1947);
		RandomSequence rs(param.seed ^ (xi + param.xofs), (yi + param.yofs));
//		unsigned long x = 123456789 * xi + 315892 * param.seed;
//		unsigned long y = 362436069 * yi;
//		unsigned long z = 521288629;
//		unsigned long w = 88675123 ^ param.seed;
/*		for(int j = 0; j < 5; j++){
			unsigned long long r = 4294966893ull * (uint64_t(x) + y);
			x = uint32_t(r & 0xFFFFFFFF);
			y = r >> 32;
		}*/
/*		for(int j = 0; j < 3; j++){
			unsigned long t = x ^ (x << 11);
			x = y;
			y = w;
			w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
		}*/
//		srand(x + y * 2456 + w * 521);
		int base;
//		base = genrand_int32() % 256;
//		base = tinymt32_generate_uint32(&tmt) % 256;
		base = rs.next() % 256;
//		base = (x + y + z + w) % 256;
//		rand();
//		base = rand() % 256;
//		base = x % 256;
		work[xi][yi] = base;
	}
	char buf[128];
	sprintf(buf, "%g\n", TimeMeasLap(&tm));
	OutputDebugStringA(buf);

	double factor = 1.0;
	double sumfactor = 0.0;

	// Accumulate signal over octaves to produce Perlin noise.
	for(octave = 0; (1 << octave) < CELLSIZE; octave += 1){
		int cell = 1 << octave;
		if(octave == 0){
			for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++)
				work2[xi][yi] = work[xi][yi];
		}
		else for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++){
			int xj, yj;
			double sum = 0;
			for(xj = 0; xj <= 1; xj++) for(yj = 0; yj <= 1; yj++){
				sum += (double)work[xi / cell + xj][yi / cell + yj]
				* (xj ? xi % cell : (cell - xi % cell - 1)) / (double)cell
				* (yj ? yi % cell : (cell - yi % cell - 1)) / (double)cell;
			}
			work2[xi][yi] += (int)(sum * factor);
			if(maxwork2 < work2[xi][yi])
				maxwork2 = work2[xi][yi];
		}
		sumfactor += factor;
		factor /= param.persistence;
	}

	// Return result
	for(int xi = 0; xi < CELLSIZE; xi++) for(int yi = 0; yi < CELLSIZE; yi++){
		callback((float)work2[xi][yi] / maxwork2, xi, yi);
	}
}

}

#endif
