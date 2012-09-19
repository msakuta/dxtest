#ifndef PERLINNOISE3D_H
#define PERLINNOISE3D_H
/** \file
 * \brief Defines Perlin Noise in 3D generator classes and functions.
 */

#include <stdio.h>
extern "C"{
#include <clib/rseq.h>
//#include <clib/random/mt19937ar.h>
#include <clib/timemeas.h>
}
//#include <clib/random/tinymt32.h>
#include <cpplib/RandomSequence.h>
#include "SignModulo.h"
#include "perlinNoise.h"


namespace PerlinNoise{


/// \brief Callback object that receives result of perlin_noise
///
/// We do this because not all application needs temporary memry to store the result.
class PerlinNoiseCallback3D{
public:
	virtual void operator()(float, int ix, int iy, int iz) = 0;
};

/// \brief Callback object that will assign noise values to float 2-d array field.
template<int CELLSIZE>
class FieldAssign3D : public PerlinNoiseCallback3D{
	typedef float (&fieldType)[CELLSIZE][CELLSIZE][CELLSIZE];
	fieldType field;
public:
	FieldAssign3D(fieldType field) : field(field){}
	void operator()(float f, int ix, int iy, int iz){
		field[ix][iy][iz] = f;
	}
};

/// \brief Parameters given to perlin_noise.
struct PerlinNoiseParams3D : PerlinNoiseParams{
	int zofs;

	PerlinNoiseParams3D(
		long seed = 0,
		double persistence = 0.5,
		long octaves = 4,
		int xofs = 0,
		int yofs = 0,
		int zofs = 0)
		: PerlinNoiseParams(seed, persistence, octaves, xofs, yofs), zofs(zofs)
	{
	}
};

template<int CELLSIZE>
inline void perlin_noise_3D(long seed, PerlinNoiseCallback &callback, int xofs = 0, int yofs = 0, int zofs = 0){
	perlin_noise_3D<CELLSIZE>(PerlinNoiseParams3D(seed, 0.5, xofs, yofs, zofs), callback);
}

/// \brief Generate Perlin Noise and return it through callback.
/// \param param Parameters to generate the noise.
/// \param callback Callback object to receive the result.
template<int CELLSIZE>
void perlin_noise_3D(const PerlinNoiseParams3D &param, PerlinNoiseCallback3D &callback){
	static const int baseMax = 255;

	struct Random{
		RandomSequence rs;
		Random(long seed, long x, long y, long z) : rs(seed ^ x ^ (z << 16), y ^ ((unsigned long)z >> 16)){
		}
		unsigned long next(){
			unsigned long u = rs.next();
			return (u >> 8) & 0xf0 + (u & 0xf);
		}
	};

	double persistence = param.persistence;
	int work2[CELLSIZE][CELLSIZE][CELLSIZE] = {0};
	int octave;
	int xi, yi, zi;

	double factor = 1.0;
	double sumfactor = 0.0;

	// Accumulate signal over octaves to produce Perlin noise.
	for(octave = 0; octave < param.octaves; octave += 1){
		int cell = 1 << octave;
		if(octave == 0){
			for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++) for(zi = 0; zi < CELLSIZE; zi++)
				work2[xi][yi][zi] = Random(param.seed, (xi + param.xofs), (yi + param.yofs), zi + param.zofs).next();
		}
		else for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++) for(zi = 0; zi < CELLSIZE; zi++){
			int xj, yj, zj;
			double sum = 0;
			int xsm = SignModulo(xi + param.xofs, cell);
			int ysm = SignModulo(yi + param.yofs, cell);
			int zsm = SignModulo(zi + param.zofs, cell);
			int xsd = SignDiv(xi + param.xofs, cell);
			int ysd = SignDiv(yi + param.yofs, cell);
			int zsd = SignDiv(zi + param.zofs, cell);
			for(xj = 0; xj <= 1; xj++) for(yj = 0; yj <= 1; yj++) for(zj = 0; zj <= 1; zj++){
				sum += (double)(Random(param.seed, (xsd + xj), (ysd + yj), zsd + zj).next())
				* (xj ? xsm : (cell - xsm - 1)) / (double)cell
				* (yj ? ysm : (cell - ysm - 1)) / (double)cell
				* (zj ? zsm : (cell - zsm - 1)) / (double)cell;
			}
			work2[xi][yi][zi] += (int)(sum * factor);
		}
		sumfactor += factor;
		factor /= param.persistence;
	}

	// Return result
	for(int xi = 0; xi < CELLSIZE; xi++) for(int yi = 0; yi < CELLSIZE; yi++) for(int zi = 0; zi < CELLSIZE; zi++){
		callback(float((double)work2[xi][yi][zi] / baseMax / sumfactor), xi, yi, zi);
	}
}

}

#endif
