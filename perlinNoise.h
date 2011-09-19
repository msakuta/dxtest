#ifndef PERLINNOISE_H
#define PERLINNOISE_H
extern "C"{
#include <clib/rseq.h>
}

namespace PerlinNoise{

class PerlinNoiseCallback{
public:
	virtual void operator()(float, int ix, int iy) = 0;
};

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

template<int CELLSIZE>
void perlin_noise(long seed, PerlinNoiseCallback &callback, int xofs = 0, int yofs = 0){
	byte work[CELLSIZE][CELLSIZE];
	float work2[CELLSIZE][CELLSIZE] = {0};
	float maxwork2 = 0;
	int octave;
	for(octave = 0; (1 << octave) < CELLSIZE; octave += 1){
		int cell = 1 << octave;
		int xi, yi, zi;
		int k;
		for(xi = 0; xi < CELLSIZE / cell; xi++) for(yi = 0; yi < CELLSIZE / cell; yi++){
			struct random_sequence rs;
			initfull_rseq(&rs, seed ^ (xi + xofs), (yi + yofs));
			int base;
			base = rseq(&rs) % 256;
			if(octave == 0)
				callback(base, xi, yi);
			else
				work[xi][yi] = /*rseq(&rs) % 32 +*/ base;
		}
		if(octave == 0){
			for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++)
				work2[xi][yi] = work[xi][yi];
		}
		else for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++){
			int xj, yj, zj;
			int sum[4] = {0};
			for(k = 0; k < 1; k++){
				for(xj = 0; xj <= 1; xj++) for(yj = 0; yj <= 1; yj++){
					sum[k] += (double)work[xi / cell + xj][yi / cell + yj]
					* (xj ? xi % cell : (cell - xi % cell - 1)) / (double)cell
					* (yj ? yi % cell : (cell - yi % cell - 1)) / (double)cell;
				}
				work2[xi][yi] = work2[xi][yi] + sum[k];
				if(maxwork2 < work2[xi][yi])
					maxwork2 = work2[xi][yi];
			}
		}
	}
	for(int xi = 0; xi < CELLSIZE; xi++) for(int yi = 0; yi < CELLSIZE; yi++){
		callback(work2[xi][yi] / maxwork2, xi, yi);
	}
}

}

#endif
