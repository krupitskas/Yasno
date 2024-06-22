uint JenkinsHash(uint x)
{
	x += x << 10;
	x ^= x >> 6;
	x += x << 3;
	x ^= x >> 11;
	x += x << 15;
	return x;
}

uint InitRNG(uint2 pixel , uint2 resolution , uint frame) 
{
	const uint rng_state = dot(pixel , uint2(1, resolution.x)) ^ JenkinsHash(frame);
	return JenkinsHash(rng_state);
}

float UintToFloat(uint x) 
{
	return asfloat(0x3f800000 | (x >> 9)) - 1.f;
}

uint Xorshift(inout uint rng_state)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

float Rand(inout uint rng_state) 
{
	return UintToFloat(Xorshift(rng_state));
}

// input : pixel screen space coordinate, frame number, sample number
// return : four random numbers
uint4 Pcg4d(uint4 v)
{
	v = v * 1664525u + 1013904223u;
	
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	
	v = v ^ (v >> 16u);
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	
	return v;
}