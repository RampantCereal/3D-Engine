#pragma once

#include "framework.h"
#include "texture.h"

extern Vector3 cubemapFaceNormals[6][3]; //(x,y,z)

struct SphericalHarmonics {
	Vector3 coeffs[9];
};

//struct to store probes
struct sProbe {
	Vector3 pos; //where is located
	Vector3 local; //its ijk pos in the matrix
	int index; //its index in the linear array
	SphericalHarmonics sh; //coeffs
};

//struct to store reflection probes info
struct sReflectionProbe {
	Vector3 pos;
	Texture* cubemap = NULL;
};




SphericalHarmonics computeSH( FloatImage images[], bool degamma = false);
