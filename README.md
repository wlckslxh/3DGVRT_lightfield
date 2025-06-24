# 3DGVRT (3D Gaussian Vulkan Ray Tracing)

## 👀 Preview
![image](./results/images/Screenshot_20241105-183715.png)


## 🚀 Assets
It's still being prepared. 😘 <br/>

## 🎠 Build
```
1. Clone this repository or download zip file.
2. Make build system using cmake and select Visual Studio for IDE.
3. Set as startup project 'VulkanFullRT'
4. Build and Run!
```

## 📷 Camera Interface

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

## 🌷 Experiments
- 3DGS &ensp; vs &ensp; 3DGVRT
- 3DGRT &ensp; vs &ensp; 3DGVRT
- Mobile execution
---
- 권한문제 해결 😨
    ```
    adb root
    adb remount
    adb shell setenforce 0
    ```
- Vulkan version 😎
    ```
    SDK version 1.4.313.0
    Vulkan driver version 576.28.0.0
    ➡️ 1.4.303.0
    ```

## 🏭 List of Projects

### [Vulkan Full Ray tracing](projects/VulkanFullRT/)
1. Gaussian Enclosing pass (전처리로 1번 수행🔂)
    - Vulkan compute pipeline
2. Particle Rendering pass (매 frame마다 수행🔁)
    - Vulkan ray tracing pipeline or Vulkan compute pipeline (chosen by macro RAY_QUERY)

### [Vulkan Hybrid](projects/VulkanHybrid/)
- 1 graphics pipeline + 1 ray tracing pipeline
    - [Pass 0] Geometry pass (graphics pipeline)
    - [Pass 1] Ligting pass (ray tracing pipeline)

## 🌠 Shaders

Vulkan consumes shaders in an intermediate representation called SPIR-V. This makes it possible to use different shader languages by compiling them to that bytecode format. The primary shader language used here is [GLSL](shaders/glsl).
<br/><br/>

## 🚦 Run Options

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

Note that some projects require specific device features, and if you are on a multi-gpu system you might need to use the `-gl` and `-g` to select a gpu that supports them. <br/>

## 💕 Acknowledgements
Sehee Cho, Graphics lab, Sogang university <br/>
Taekgeun Yoo, Graphics lab, Sogang university <br/>
Junsoo Kim, Graphics lab, Sogang university <br/>

## 🙏 References
We refer to those repositories shown below.

https://github.com/SaschaWillems/Vulkan.git <br/>
https://github.com/nv-tlabs/3dgrut <br/>