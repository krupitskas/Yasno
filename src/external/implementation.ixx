module;

#pragma warning(push)
#pragma warning(disable : 4018) // '>=': signed/unsigned mismatch
#pragma warning(disable : 4267) // 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4267) // 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4244) // 'argument': conversion from 'int' to 'short', possible loss of data

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#pragma warning(pop)

export module external.implementaion;