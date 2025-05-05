file(GLOB BASE_SRC "../../../base/*.cpp" "../../../external/imgui/*.cpp")
add_library(libbase SHARED ${BASE_SRC})

set(KTX_DIR ${EXTERNAL_DIR}/ktx)
set(DFDUTILS_DIR ${KTX_DIR}/external/dfdutils)
set(BASISU_DIR ${KTX_DIR}/external/basisu)
file(GLOB DFDUTILS_SOURCES "${DFDUTILS_DIR}/*.c")
file(GLOB BASISU_SOURCES "${BASISU_DIR}/zstd/*.c" "${BASISU_DIR}/encoder/*.cpp" "${BASISU_DIR}/transcoder/*.cpp")
set(KTX_SOURCES
	${KTX_DIR}/lib/texture.c
	${KTX_DIR}/lib/hashlist.c
	${KTX_DIR}/lib/checkheader.c
	${KTX_DIR}/lib/swap.c
	${KTX_DIR}/lib/memstream.c
	${KTX_DIR}/lib/filestream.c
	${KTX_DIR}/lib/texture1.c
	${KTX_DIR}/lib/texture2.c
	${KTX_DIR}/lib/vkformat_typesize.c
	${KTX_DIR}/lib/vkformat_check.c
	${KTX_DIR}/lib/miniz_wrapper.cpp
	${KTX_DIR}/lib/basis_transcode.cpp
	${DFDUTILS_SOURCES}
	${BASISU_SOURCES}
)
set(KTX_INCLUDE
	${KTX_DIR}/include
	${KTX_DIR}/lib
	${KTX_DIR}/other_include
	${KTX_DIR}/external
	${DFDUTILS_DIR}
	${DFDUTILS_DIR}/vulkan
	${DFDUTILS_DIR}/KHR
	${BASISU_DIR}/encoder
	${BASISU_DIR}/transcoder
	${BASISU_DIR}/zstd
)

add_library(libktx ${KTX_SOURCES})
target_include_directories(libktx PUBLIC ${KTX_INCLUDE})
set_property(TARGET libktx PROPERTY FOLDER "external")

include_directories(${BASE_DIR})
include_directories(${EXTERNAL_DIR})
include_directories(${EXTERNAL_DIR}/glm)
include_directories(${EXTERNAL_DIR}/gli)
include_directories(${EXTERNAL_DIR}/imgui)
include_directories(${EXTERNAL_DIR}/tinygltf)
include_directories(${EXTERNAL_DIR}/basisu/zstd)
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${EXTERNAL_DIR}/ktx/lib)
include_directories(${EXTERNAL_DIR}/ktx/include)
include_directories(${EXTERNAL_DIR}/ktx/include/KHR)
include_directories(${EXTERNAL_DIR}/ktx/other_include)

target_link_libraries(
	libbase
	android
	log
	z
	libktx
)
