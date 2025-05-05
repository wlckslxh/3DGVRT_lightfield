#pragma once
#include "VulkanRTBase.h"
// wyj added
namespace SimpleUtils
{
	VkTransformMatrixKHR glmToVkMatrix(const glm::mat4& glmMat4);
	glm::mat4 VkMatrixToGlm(const VkTransformMatrixKHR& vkMat);
	static VkTransformMatrixKHR IdentityVkTransformMatrix = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0 };

	glm::vec3 Scale(const glm::mat4& transform);
	glm::vec3 Right(const glm::mat4& transform);
	glm::vec3 Up(const glm::mat4& transform);
	glm::vec3 Look(const glm::mat4& transform);
	glm::vec3 Position(const glm::mat4& transform);

	/**
	 * @return Accumulated Scale from Transform
	 */
	glm::mat4 ScaleMatAccumulated(const glm::mat4& transform, glm::vec3 addingScale);
	/**
	 * @return Accumulated Rotation from Transform
	 */
	glm::mat4 RotationMatAccumulated(const glm::mat4& transform, float angle, glm::vec3 axis);
	glm::mat4 RotationMatAccumulated(const glm::mat4& transform, const glm::mat4& rotate);
	/**
	 * @return Accumulated Translation from Transform
	 */
	glm::mat4 TranslationMatAccumulated(const glm::mat4& transform, glm::vec3 addingPos);

	// Get Matrix from Current Transform
	glm::mat4 GetRotation(const glm::mat4& transform);
	glm::mat4 GetTranslation(const glm::mat4& transform);


	void PrintCamPos(const glm::mat4& view);
};

