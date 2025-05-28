/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 *
 * Camera(Quaternion)
 *
 */

#define USE_CORRECT_VULKAN_PERSPECTIVE_IHM

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "LoadCamera.hpp"
#include "Define.h"

class QuaternionCamera {
public:
    glm::vec3 position;
    glm::quat rotation;
    glm::mat4 perspective;
    float fov;
    float znear;
    float zfar;
    float deltaTime;

    bool updated;

    CameraLoader cameraLoader;
    DatasetType dataType;
    glm::mat4 viewMatrix;

    QuaternionCamera()
        : position(0.0f, 0.0f, 0.0f), rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), perspective(glm::mat4(0.0f)) {
    }

    void setDeltaTime(float deltaTime) {
        this->deltaTime = deltaTime;
    }

    glm::mat4 getViewMatrix() const {
        // 쿼터니언을 행렬로 변환하고, 역행렬 적용 (View matrix)
        glm::mat4 rotMat = glm::mat4_cast(rotation);
        glm::mat4 transMat = glm::translate(glm::mat4(1.0f), -position);
        //return viewMatrix;
        return rotMat * transMat;
    }

    glm::mat4 getProjectionMatrix() const {
        return perspective;
    }

    void setPerspective(float fov, float aspect, float znear, float zfar)
    {
        glm::mat4 currentMatrix = perspective;
#ifdef USE_CORRECT_VULKAN_PERSPECTIVE_IHM
        this->fov = fov;
        this->znear = znear;
        this->zfar = zfar;
        perspective = glm::perspective_Vulkan_no_depth_reverse(glm::radians(fov), aspect, znear, zfar);

        if (perspective != currentMatrix) {
            updated = true;
        }
#else
        glm::mat4 currentMatrix = matrices.perspective;
        this->fov = fov;
        this->znear = znear;
        this->zfar = zfar;
        matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
        if (flipY) {
            matrices.perspective[1][1] *= -1.0f;
        }
        if (matrices.view != currentMatrix) {
            updated = true;
        }
#endif
    };

    void setTranslation(glm::vec3 position) {
        this->position = position;
    }

    void setRotation(glm::vec3 radians) {
        glm::quat qPitch = glm::angleAxis(radians.x, glm::vec3(1, 0, 0));
        glm::quat qYaw = glm::angleAxis(radians.y, glm::vec3(0, 1, 0));
        glm::quat qRoll = glm::angleAxis(radians.z, glm::vec3(0, 0, 1));

        rotation = glm::normalize(qYaw * qPitch * qRoll);
    }

    void rotate(const glm::vec3& localAxis, float angleRadians) {
        angleRadians *= deltaTime;
        glm::vec3 worldAxis = rotation * localAxis;
        glm::quat q = glm::angleAxis(angleRadians, glm::normalize(worldAxis));
        rotation = glm::normalize(q * rotation);
    }

    void move(const glm::vec3& delta) {
        // 로컬 방향으로 이동하려면 쿼터니언 회전을 적용
        position += rotation * delta * deltaTime;
    }

    glm::vec3 getForward() const {
        return rotation * glm::vec3(0, 0, -1);
    }

    glm::vec3 getRight() const {
        return rotation * glm::vec3(1, 0, 0);
    }

    glm::vec3 getUp() const {
        return rotation * glm::vec3(0, 1, 0);
    }

    /* load nerf camera */
    const vector<string> getCamNames() {
        return cameraLoader.camNames;
    }

    void setNerfCamera(uint32_t idx) {
        CameraFrame* frame = &cameraLoader.nerfCameras.frames[idx];
        this->rotation = frame->rotation;
        this->position = frame->position;
        //viewMatrix = frame->transformMatrix;
        //perspective = cameraLoader.nerfCameras.projectionMatrix;
    }

    void setDatasetCamera(DatasetType type, uint32_t idx, float aspect) {
        if (type == nerf) {
            //setPerspective(cameraLoader.fovy, aspect, znear, zfar);
            setPerspective(FOV_Y, aspect, znear, zfar);
            setNerfCamera(idx);
        }
    }

    void loadDatasetCamera(DatasetType type, string path, uint32_t width, uint32_t height) {
        if (type == nerf) {
            cameraLoader.loadNerfCameraData(path, width, height, znear, zfar);
            //cameraLoader.PrintCameraData(cameraLoader.nerfCameras);
        }
        dataType = type;
    }

    void flipX() {
        rotation.x *= -1;
    }
    void flipY() {
        rotation.y *= -1;
    }
    void flipZ() {
        rotation.z *= -1;
    }
    void flipW() {
        rotation.w *= -1;
    }
};