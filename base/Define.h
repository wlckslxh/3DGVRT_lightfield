#pragma once

#define DRAW_WORLD_AXIS 0

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#define START_FRAME 300
#define MEASURE_FRAME 500
#else
#define START_FRAME 2000
#define MEASURE_FRAME 2000
#endif

// -------------------------- split blas -------------------------- //
#define SPLIT_BLAS 1		// This macro should be managed with define.glsl
#define NUMBER_OF_CELLS_PER_LONGEST_AXIS 10
#define SCENE_EPSILON 1e-4f
#define ONE_VERTEX_BUFFER 1
#define HYBRID 0

#define ADAPTIVE_SPLIT 0
#define MAX_VERTEX_CNT 10000
#define ADAPTIVE_SPLIT_CNT 8	// x,y,z - half,half,half (don't edit)
// ----------------------------------------------------------------- //

#define STOP_LIGHT 0

#define ASSET 0
#define ASYNC 1
#define MULTIQUEUE 0
#define DIRECTRENDER 1
#define TIMER_CORRECTION 1
#define TEXTURE_COMPRESSION 1

#define PRE_SHADOW_MAP 1 // This macro should be set 1(ON) when DYNAMIC_SCENE is 0(OFF).

#define ANY_HIT 0	// This macro should be managed with define.glsl

#define DYNAMIC_SCENE 0 // This macro should be managed with define.glsl
#define USE_ANIMATION 0 // 0 is Deafult

#if ASSET == 0
#define ASSET_PATH "models/suntemple_simple/SunTemple.gltf"

#elif ASSET == 1
#define ASSET_PATH "models/sponza_multi_blas_transparent/Sponza.gltf"

#elif ASSET == 2
#define ASSET_PATH "models/cornellBox/CornellBox.gltf"

#elif ASSET == 3
#define ASSET_PATH "models/checkerboard/CheckerBoard.gltf"

#elif ASSET == 4	
#define ASSET_PATH "models/bistroexterior/BistroExterior.gltf"

#elif ASSET == 5	
#define ASSET_PATH "models/sponza_dynamic_multi_blas/Sponza.gltf"
#endif

#if TEXTURE_COMPRESSION
#undef ASSET_PATH
#if ASSET == 0
#define ASSET_PATH "models/suntemple_etc1s/SunTemple.gltf"

#elif ASSET == 1
#define ASSET_PATH "models/sponza_etc1s/Sponza.gltf"

#elif ASSET == 4
#define ASSET_PATH "models/bistroexterior_etc1s/BistroExterior.gltf"
#endif
#endif

#define CUBEMAP_TEXTURE_PATH "cubeMapTextures/blueSky.ktx"
#define SPHERE_PATH "models/sphere/Sphere.gltf"

#if ASSET == 0
#define NUM_OF_LIGHTS_SUPPORTED 32
#define NUM_OF_DYNAMIC_LIGHTS 1	
#define NUM_OF_STATIC_LIGHTS 31
#define STATIC_LIGHT_OFFSET 1
#elif ASSET == 1
#define NUM_OF_LIGHTS_SUPPORTED 1
#define NUM_OF_DYNAMIC_LIGHTS 1		
#define NUM_OF_STATIC_LIGHTS 1		// for convenience
#define STATIC_LIGHT_OFFSET 1
#elif ASSET == 4
#define NUM_OF_LIGHTS_SUPPORTED 48
#define NUM_OF_DYNAMIC_LIGHTS 1	 // temporarily assigned value
#define NUM_OF_STATIC_LIGHTS 48	// temporarily assigned value
#define STATIC_LIGHT_OFFSET 0
#else
#define NUM_OF_LIGHTS_SUPPORTED 1
#define NUM_OF_DYNAMIC_LIGHTS 1		// for convenience
#define NUM_OF_STATIC_LIGHTS 1	
#define STATIC_LIGHT_OFFSET 0
#endif

#if DYNAMIC_SCENE
#undef ASSET_PATH // redefine
#if ASSET == 0
	#define ASSET_PATH "models/suntemple_dynamic_multi_blas/SunTemple.gltf"
#elif ASSET == 1
	#define ASSET_PATH "models/sponza_dynamic_multi_blas/Sponza.gltf"
#endif

// If FIXED_INSTANCING is 0, instancing cube is generated with fixed position, and number.
// If 0, instancing cube is generated in random position, and user can change number via imgui.
#define FIXED_INSTANCING 1 // 1 is Default
#if FIXED_INSTANCING
	constexpr unsigned int BLAS_INSTANCING_MAX = 10; // Instancing Object Count
#else
	constexpr unsigned int BLAS_INSTANCING_MAX = 1000; // Instancing Object Max
#endif
constexpr unsigned int PROTOTYPE_BLAS_COUNT = 2;
constexpr unsigned int BLAS_MAX_COUNT = (PROTOTYPE_BLAS_COUNT + BLAS_INSTANCING_MAX);
extern int32_t g_numInstancing;
#endif