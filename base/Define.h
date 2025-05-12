/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 *
 * Define.h
 *
 */

#pragma once

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#define START_FRAME 300
#define MEASURE_FRAME 500
#else
#define START_FRAME 2000
#define MEASURE_FRAME 2000
#endif

#define STOP_LIGHT 0

#define ASSET 0
#define MULTIQUEUE 0	// 0 is Default
#define DIRECTRENDER 1	// 1 is Default
#define TIMER_CORRECTION 1
#define TEXTURE_COMPRESSION 1

#define USE_ANIMATION 0 // 0 is Default

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