/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 *
 * Camera(Euler Angle)
 *
 */
#pragma once

#define USE_CORRECT_VULKAN_PERSPECTIVE_IHM

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Define.h"


#include "LoadCamera.hpp"

class Camera
{
private:
	float fov;
	float znear, zfar;

	CameraLoader cameraLoader;

	void updateViewMatrix()
	{
		glm::mat4 currentMatrix = matrices.view;

#if defined(Y_IS_UP)
		glm::vec3 uVec = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 vVec = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 nVec = glm::vec3(0.0f, 0.0f, 1.0f);
#elif defined(N_IS_UP)
		glm::vec3 uVec = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 vVec = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 nVec = glm::vec3(0.0f, -1.0f, 0.0f);
#endif

		if(glm::radians(rotation.y) != 0)
		rotateAxis(glm::radians(rotation.y), &vVec, &nVec, &uVec);
		if (glm::radians(rotation.x) != 0)
		rotateAxis(glm::radians(rotation.x), &uVec, &vVec, &nVec);
		if (glm::radians(rotation.z) != 0)
		rotateAxis(glm::radians(rotation.z), &nVec, &uVec, &vVec);

		glm::mat4 rot(glm::vec4(uVec, 0.0f), glm::vec4(vVec, 0.0f), glm::vec4(nVec, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		matrices.view = glm::translate(glm::transpose(rot), glm::vec3(-position));

		if (matrices.view != currentMatrix) {
			updated = true;
		}
	};

public:
	enum CameraType { lookat, firstperson, SG_camera, cameraSize};
	CameraType type = CameraType::lookat;
	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();
	glm::vec4 viewPos = glm::vec4();
	DatasetType dataType;

	float rotationSpeed = 1.0f;
	float movementSpeed = 15.0f;

	bool updated = true;
	bool flipY = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
		bool forward = false;
		bool backward = false;
	} keys;

	const vector<string> getCamNames() {
		return cameraLoader.camNames;
	}

	void setNearFar(float _znear, float _zfar) {
		znear = _znear;
		zfar = _zfar;
	}

	void setNerfCamera(uint32_t idx) {
		CameraFrame* frame = &cameraLoader.nerfCameras.frames[idx];
		matrices.view = frame->transformMatrix;
		//matrices.perspective = cameraLoader.nerfCameras.projectionMatrix;
	}

	void setDatasetCamera(DatasetType type, uint32_t idx, float aspect) {
		if (type == nerf) {
			setPerspective(cameraLoader.fovy, aspect, znear, zfar);
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

	void rotateAxis(float rad_angle, glm::vec3* u, glm::vec3* v, glm::vec3* n) {  // rotate u, v->n
		float sina, cosa;
		glm::vec3 tempAxis;

		sina = sin(rad_angle);
		cosa = cos(rad_angle);

		// calculate v vector
		tempAxis = *v * cosa + glm::cross(*u, *v) * sina;
		*v = tempAxis;
		*v = glm::normalize(*v);

		// calculate u vector
		tempAxis = glm::cross(*u, *v);

		*n = tempAxis;
		*n = glm::normalize(*n);
	}

	bool moving()
	{
		return keys.left || keys.right || keys.up || keys.down|| keys.forward || keys.backward;
	}

	float getNearClip() { 
		return znear;
	}

	float getFarClip() {
		return zfar;
	}

	float getFov() {
		return fov;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
#ifdef USE_CORRECT_VULKAN_PERSPECTIVE_IHM
		glm::mat4 currentMatrix = matrices.perspective;
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective_Vulkan_no_depth_reverse(glm::radians(fov), aspect, znear, zfar);
		
		if (matrices.view != currentMatrix) {
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

	void updateAspectRatio(float aspect)
	{
#ifdef USE_CORRECT_VULKAN_PERSPECTIVE_IHM
		glm::mat4 currentMatrix = matrices.perspective;
		matrices.perspective = glm::perspective_Vulkan_no_depth_reverse(glm::radians(fov), aspect, znear, zfar);

		if (matrices.view != currentMatrix) {
			updated = true;
		}
#else
		glm::mat4 currentMatrix = matrices.perspective;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		if (flipY) {
			matrices.perspective[1][1] *= -1.0f;
		}
		if (matrices.view != currentMatrix) {
			updated = true;
		}
#endif
	}

	void setPosition(glm::vec3 position)
	{
		this->position = position;
		updateViewMatrix();
	}

	void setRotation(glm::vec3 rotation)
	{
		this->rotation = rotation;
		updateViewMatrix();
	}

	void rotate(glm::vec3 delta)
	{
		this->rotation += delta;
		updateViewMatrix();
	}

	void setTranslation(glm::vec3 translation)
	{
		this->position = translation;
		updateViewMatrix();
	};

	void translate(glm::vec3 delta)
	{
		this->position += delta;
		updateViewMatrix();
	}

	void setRotationSpeed(float rotationSpeed)
	{
		this->rotationSpeed = rotationSpeed;
	}

	void setMovementSpeed(float movementSpeed)
	{
		this->movementSpeed = movementSpeed;
	}

	void update(float deltaTime)
	{
		updated = false;
		if (type == CameraType::firstperson || type == CameraType::SG_camera)
		{
			if (moving())
			{
				float moveSpeed = deltaTime * movementSpeed;

#if defined(Y_IS_UP)
				glm::vec3 uVec = glm::vec3(1.0f, 0.0f, 0.0f);
				glm::vec3 vVec = glm::vec3(0.0f, 1.0f, 0.0f);
				glm::vec3 nVec = glm::vec3(0.0f, 0.0f, 1.0f);
#elif defined(N_IS_UP)
				glm::vec3 uVec = glm::vec3(1.0f, 0.0f, 0.0f);
				glm::vec3 vVec = glm::vec3(0.0f, 0.0f, 1.0f);
				glm::vec3 nVec = glm::vec3(0.0f, -1.0f, 0.0f);
#endif

				if (glm::radians(rotation.y) != 0)
					rotateAxis(glm::radians(rotation.y), &vVec, &nVec, &uVec);
				if (glm::radians(rotation.x) != 0)
					rotateAxis(glm::radians(rotation.x), &uVec, &vVec, &nVec);
				if (glm::radians(rotation.z) != 0)
					rotateAxis(glm::radians(rotation.z), &nVec, &uVec, &vVec);

				if (type == CameraType::firstperson || type == CameraType::SG_camera) {
					if (keys.forward) {
						position -= nVec * moveSpeed;
					}
					if (keys.backward) {
						position += nVec * moveSpeed;
					}
					if (keys.left)
					{
						position -= uVec * moveSpeed;
					}
					if (keys.right)
					{
						position += uVec * moveSpeed;
					}
					if (keys.up)
					{
						position += vVec * moveSpeed;
					}
					if (keys.down) {
						position -= vVec * moveSpeed;
					}
				}
			}
		}
#if !LOAD_NERF_CAMERA
		updateViewMatrix();
#endif
	};

	// Update camera passing separate axis data (gamepad)
	// Returns true if view or position has been changed
	bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
	{
		bool retVal = false;

		if (type == CameraType::firstperson)
		{
			// Use the common console thumbstick layout		
			// Left = view, right = move

			const float deadZone = 0.0015f;
			const float range = 1.0f - deadZone;

			glm::vec3 camFront;
			camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			camFront.y = sin(glm::radians(rotation.x));
			camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
			camFront = glm::normalize(camFront);

			float moveSpeed = deltaTime * movementSpeed * 2.0f;
			float rotSpeed = deltaTime * rotationSpeed * 50.0f;
			 
			// Move
			if (fabsf(axisLeft.y) > deadZone)
			{
				float pos = (fabsf(axisLeft.y) - deadZone) / range;
				position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}
			if (fabsf(axisLeft.x) > deadZone)
			{
				float pos = (fabsf(axisLeft.x) - deadZone) / range;
				position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}

			// Rotate
			if (fabsf(axisRight.x) > deadZone)
			{
				float pos = (fabsf(axisRight.x) - deadZone) / range;
				rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
			if (fabsf(axisRight.y) > deadZone)
			{
				float pos = (fabsf(axisRight.y) - deadZone) / range;
				rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
		}
		else
		{
			// todo: move code from example base class for look-at
		}

		if (retVal)
		{
			updateViewMatrix();
		}

		return retVal;
	}
};