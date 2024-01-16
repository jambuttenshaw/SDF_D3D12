

/*
 *
 *	This shader takes a 3D texture containing distance values and builds a buffer of AABB's around the iso-surface
 *
 */

// These must match those defined in C++
#define NUM_SHADER_THREADS 8


[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, NUM_SHADER_THREADS)]
void main(uint3 DTid : SV_DispatchThreadID)
{

}

