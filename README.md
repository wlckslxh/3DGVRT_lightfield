# Mobile Ray Tracing Project 

## Preview
![image](./results/images/Screenshot_20241105-183715.png)
by Graphics Lab, Sogang University
<br/><br/>

## Assets
```
\\\163.239.24.71\GraphicsLab\Project\2024_Samsung_Project\SamsungVulkanRT_assets
```
<br/>

## Build
```
1. Clone this repository.
2. Copy the 'assets' folder to your cloned root.
3. Make build system using cmake.
4. Build using Visual Studio IDE.
```
<br/>

## Run

Once built, projects can be run from the bin directory. The list of available command line options can be brought up with `--help`:
```
 -v, --validation: Enable validation layers
 -br, --benchruntime: Set duration time for benchmark mode in seconds
 -vs, --vsync: Enable V-Sync
 -w, --width: Set window width
 -f, --fullscreen: Start in fullscreen mode
 --help: Show help
 -h, --height: Set window height
 -bt, --benchframetimes: Save frame times to benchmark results file
 -s, --shaders: Select shader type to use (glsl or hlsl)
 -b, --benchmark: Run project in benchmark mode
 -g, --gpu: Select GPU to run on
 -bf, --benchfilename: Set file name for benchmark results
 -gl, --listgpus: Display a list of available Vulkan devices
 -bw, --benchwarmup: Set warmup time for benchmark mode in seconds
```

Note that some projects require specific device features, and if you are on a multi-gpu system you might need to use the `-gl` and `-g` to select a gpu that supports them.
<br/><br/>

## List of Projects

### [Vulkan Full Ray tracing](projects/VulkanFullRT/)
- 1 ray tracing pipeline
### [Vulkan Full Ray Query](projects/VulkanFullRayQuery/)
- 1 graphics pipeline
### [Vulkan Hybrid](projects/VulkanHybrid/)
- 1 graphics pipeline + 1 ray tracing pipeline
    - [Pass 0] Geometry pass (graphics pipeline)
    - [Pass 1] Ligting pass (ray tracing pipeline)
### [Vulkan Deferred Ray Query](projects/VulkanDeferredRayQuery/)
- 2 graphics pipelines
    - [Pass 0] Geometry pass (graphics pipeline)
    - [Pass 1] Ligting pass (graphics pipeline)
### [Vulkan Hybrid with Shadow mapping](projects/VulkanHybridShadowmap)
- 2 graphics pipelines + 1 ray tracing pipeline
    - [Pass 0] Shadow map generation pass (graphics pipeline)
    - [Pass 1] Geometry pass (graphics pipeline)
    - [Pass 2] Ligting pass (ray tracing pipeline)
### [Vulkan Hybrid with Shadow mapping Ray Query](projects/VulkanHybridShadowmapRayQuery)
- 3 graphics pipelines
    - [Pass 0] Shadow map generation pass (graphics pipeline)
    - [Pass 1] Geometry pass (graphics pipeline)
    - [Pass 2] Ligting pass (graphics pipeline)
---
### Other Developing or unused projects
#### [Vulkan Light Optimization](projects/VulkanLightOptimization/)
- 2 graphics pipelines + 1 ray tracing pipeline
    - [Pass 0] Geometry pass (graphics pipeline)
    - [Pass 1] Accumulation pass (graphics pipeline) x (# of lights)
    - [Pass 2] Ligting pass (ray tracing pipeline)
#### [Vulkan Light Optimization Ray Query](projects/VulkanLightOptimizationRayQuery/)
- 3 graphics pipelines
    - [Pass 0] Geometry pass (graphics pipeline)
    - [Pass 1] Accumulation pass (graphics pipeline) x (# of lights)
    - [Pass 2] Ligting pass (graphics pipeline)
#### [Vulkan Stencil](projects/VulkanStencil/)
- to be continued :-/
#### [Vulkan Deferred](projects/VulkanDeferred/)
- 2 graphics pipelines
    - [Pass 0] Geometry pass (graphics pipeline)
    - [Pass 1] Ligting pass (graphics pipeline)
    - No ray tracing effect
<br/>

## Experiments
- FullRT &ensp; vs &ensp; FullRayQuery
- Hybrid &ensp; vs &ensp; DeferredRayQuery
- HybridShadowmap &ensp; vs &ensp; HybridShadowmapRayQuery
- (LO &ensp; vs &ensp; LORQ) 
---
- limit GPU clock to 768MHz(S25)
- set MULTIQUEUE to 0 and DIRECTRENDER to 1 in Define.h
- 권한문제 해결
    ```
    adb root
    adb remount
    adb shell setenforce 0
    ```
---
- SPLIT BLAS Experiments
    - set SPLIT_BLAS to 1 in both Define.h and define.glsl
    - if the projcet to be executed uses mrt pass, set HYBRID to 1 in Define.h
    - need to decide flag values in Define.h
        - NUMBER_OF_CELLS_PER_LONGEST_AXIS
        - ONE_VERTEX_BUFFER (0 or 1)
        - ADAPTIVE_SPLIT (0 or 1)
- DYNAMIC SCENE Experiments
    - set DYNAMIC_SCENE to 1 in both Define.h and define.glsl
    - asset path : model named as "models/xxx_dynamic_multi_blas"
    - need to decide flag values in Define.h
        - FIXED_INSTANCING (0 or 1) : whether to fix the position and number of instancing BLAS

## Shaders

Vulkan consumes shaders in an intermediate representation called SPIR-V. This makes it possible to use different shader languages by compiling them to that bytecode format. The primary shader language used here is [GLSL](shaders/glsl).
<br/><br/>

## Camera Interface

1. Move

    |Key|Action|
    |:---:|:------:|
    |W|forward|
    |S|backward|
    |A|left|
    |D|right|
    |E|up|
    |Q|down|
    |Wheel|forward, backward|
    |Mouse Middle button|move along x, y axes|
    |R|reset to original position and orientation|

2. Rotation

    |Key|Action|
    |:---:|:------:|
    |Left arrow key|yaw -|
    |Right arrow key|yaw +|
    |Down arrow key|pitch -|
    |Up arrow key|pitch +|
    |Mouse Left button|pitch + yaw|
    |Mouse Right button|roll(갸우뚱)|


3. Velocity control

    |Key|Action|
    |:---:|:----:|
    |-|Slow down of W, D, S, D, E, Q key|
    |+|Speed up of W, D, S, D, E, Q key|
    |{|Slow down of Mouse buttons|
    |}|Speed up of Mouse buttons|


4. Camera mode
<br/>F2: SG_camera 설정이 안되어있을 경우, runtime에서 변경가능

    |Mode|
    |----|
    |SG_camera|
    |first person|
    |lookat|
<br/>

## Androids

1. Vulkan spec 확인(S24에서 작동 안 함)

   - adb shell
   - cmd gpu vkjson
   
2. 프로젝트 추가

   - 수정할 파일들
     - build.gradle
     - cmakelists.txt
     - androidmanifest.xml
     - vulkanactivity.java

3. 안드로이드 스튜디오 프로파일

   - AndroidManifest.xml의 ```</activity>``` 아래 코드 추가
      ```<profileable android:shell="true"/>```

4. release mode

   - 서버의 Project\2024_Samsung_Project\03.데이터\Android_keystore에서 readme.txt 참조

## References

We refer to the repository shown below.

https://github.com/SaschaWillems/Vulkan.git
