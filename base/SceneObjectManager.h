#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <map>
#include <string>

namespace vkglTF
{
	class Model;
	class Mesh;
}

//typedef vkglTF::Model GameObject; //

/**
 * Contain Model or Mesh.
 * reference only Model or Mesh
 */

/**
 * Reference Model or Mesh
 */
class SceneObject
{
public:
	SceneObject() = delete;
	virtual ~SceneObject() = default;
	SceneObject(vkglTF::Model* pInModel, vkglTF::Mesh* pInMesh):pModel(pInModel), pMesh(pInMesh){}

	vkglTF::Model* pModel = nullptr;
	vkglTF::Mesh* pMesh = nullptr;
	std::string name;
	uint64_t blasDeviceAddress = 0;

	glm::mat4* pTransformMat = nullptr;
};

class DynamicObject : public SceneObject
{
public:
	enum TYPE
	{
		NONE =			0,
		SCALE =			1 << 0,
		ROTATE =		1 << 1,
		TRANSLATE =		1 << 2,
		SINE_MOVE =		1 << 3,
		FOLLOW_CAM =	1 << 4,
	};
	static DynamicObject::TYPE eSponza_Sphere2;
	static DynamicObject::TYPE eSponza_Statue;
	static DynamicObject::TYPE eInstancingCube;


	DynamicObject() = delete;
	~DynamicObject() override = default;
	DynamicObject(vkglTF::Model* pInModel, vkglTF::Mesh* pInMesh, DynamicObject::TYPE eInType = TYPE::NONE);

	glm::mat4* pCamTransform = nullptr;
	DynamicObject::TYPE eType = TYPE::NONE;

	float accMoveDistanceY = 0.f, moveDistanceMax = 1.1f; // Distance
	float accScaling = 1.f, scalingMax = 2.f;
	float animationSpeed = 0.5f, rotSpeed = 10.f, moveSpeed = 1.4f, scalingSpeed = 1.f; // Speed

	glm::vec3 position;

	virtual void Update(float deltaTime);
};


class InstancingObject : public DynamicObject
{
public:
	InstancingObject() = delete;
	~InstancingObject() override = default;
	InstancingObject(vkglTF::Model* pInModel, vkglTF::Mesh* pInMesh, DynamicObject::TYPE eInType = TYPE::NONE, uint32_t inNumInstancing = 1);

	/**
	 * @return numInstancing
	 */
	uint32_t Reset_InstancingData(int32_t newNum);
	void resetInstancingPositions(const std::vector<glm::vec3>& positions);

	uint32_t prevNumInstancing = 0; // update after blas Instancing in CreateTLAS
	uint32_t numInstancing = 1;
	uint32_t numInstancingMax = 1;
	uint32_t blasCustomIndex = 0;
	float accTime = 0.f; // Accumulted Delta Time
	std::vector<glm::mat4> instancingTransforms; // final transform
	std::vector<glm::vec3> instancingRotationAxis;

	std::vector<glm::vec3> instancingPositions;
	std::vector<glm::mat4> instancingRotations;

	void Update(float deltaTime) override;
};


class SceneObjectManager
{
public:
	SceneObjectManager() = default;
	~SceneObjectManager();


	std::map<std::string, SceneObject*> sceneObjects;
	std::map<std::string, DynamicObject*> dynamicObjects;
	std::map<std::string, InstancingObject*> instancingObjects;

	uint32_t totalInstancingCount = 0;

	void EmplaceObject(const std::string& name, SceneObject* pInstance, bool isDynamic = false, bool instancing = false);
	SceneObject* FindAndGet_SceneObj(const std::string& name);
	InstancingObject* FindAndGet_InstancingObj(const std::string& name);
	const std::map<std::string, SceneObject*>& Get_SceneObjMapRef() { return sceneObjects; }
	void updateObjects(float deltaTime);
};