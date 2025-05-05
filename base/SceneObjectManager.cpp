#include "SceneObjectManager.h"

#include "VulkanglTFModel.h"
#include "SimpleUtils.h"

#include <random>

DynamicObject::TYPE DynamicObject::eSponza_Sphere2 = DynamicObject::TYPE::TRANSLATE;
DynamicObject::TYPE DynamicObject::eSponza_Statue = DynamicObject::TYPE(TYPE::TRANSLATE);
DynamicObject::TYPE DynamicObject::eInstancingCube = DynamicObject::TYPE::SINE_MOVE;

DynamicObject::DynamicObject(vkglTF::Model* pInModel, vkglTF::Mesh* pInMesh, DynamicObject::TYPE eInType) : SceneObject(pInModel, pInMesh) , eType(eInType)
{
	pModel = (pInModel);
	pMesh = (pInMesh);
	// Reference Model
	if (pModel)
	{
		pTransformMat = &(pModel->linearNodes[0]->matrix);
		name = pModel->path;
	}
	else // Reference Mesh
	{
		pTransformMat = &(pMesh->uniformBlock.matrix);
		name = pMesh->name;
	}
}

void DynamicObject::Update(float deltaTime)
{
	// Rigid Transformation
	glm::mat4 scaleMat;
	glm::mat4 rotMat;
	glm::mat4 translateMat = glm::translate(glm::mat4(), position);
	glm::mat4 newTransformMat;

	if (eType & TYPE::SCALE)
	{
		float curScaling = scalingSpeed * deltaTime;
		accScaling += curScaling;
		if (accScaling > scalingMax)
		{
			accScaling = scalingMax;
			scalingSpeed *= -1.f;
		}
		else if (accScaling < -scalingMax)
		{
			accScaling = -scalingMax;
			scalingSpeed *= -1.f;
		}
		scaleMat = SimpleUtils::ScaleMatAccumulated(*pTransformMat, glm::vec3(accScaling, accScaling, accScaling));
	}
	if (eType & TYPE::ROTATE)
	{
		float rotatingRad = glm::radians(rotSpeed * deltaTime);
	
		glm::mat4 rotX = glm::rotate(glm::mat4(), rotatingRad, SimpleUtils::Right(*pTransformMat));
		glm::mat4 rotY = glm::rotate(glm::mat4(), rotatingRad, SimpleUtils::Up(*pTransformMat));
		glm::mat4 rotZ = glm::rotate(glm::mat4(), rotatingRad, SimpleUtils::Look(*pTransformMat));

		rotMat = rotZ * rotY * rotX;
		rotMat = SimpleUtils::RotationMatAccumulated(*pTransformMat, rotMat);
	}
	if (eType & TYPE::TRANSLATE)
	{
		float curMoveDistance = moveSpeed * deltaTime;
		accMoveDistanceY += curMoveDistance;
		if (accMoveDistanceY > moveDistanceMax)
		{
			accMoveDistanceY = moveDistanceMax;
			moveSpeed *= -1.f;
		}
		else if (accMoveDistanceY < -moveDistanceMax)
		{
			accMoveDistanceY = -moveDistanceMax;
			moveSpeed *= -1.f;
		}
		translateMat = SimpleUtils::TranslationMatAccumulated(*pTransformMat, glm::vec3(0.f, curMoveDistance, 0.f));
	}
	newTransformMat = translateMat * rotMat * scaleMat;

	*pTransformMat = newTransformMat;
}

InstancingObject::InstancingObject(vkglTF::Model* pInModel, vkglTF::Mesh* pInMesh, DynamicObject::TYPE eInType, uint32_t inNumInstancing) 
	: DynamicObject(pInModel,pInMesh, eInType), numInstancing(inNumInstancing)
{
	prevNumInstancing = numInstancing;
	numInstancingMax = numInstancing;

	instancingTransforms.resize(numInstancing);
	instancingRotationAxis.resize(numInstancing);
	instancingPositions.resize(numInstancing);
	instancingRotations.resize(numInstancing);

	std::random_device              rd;         // Will be used to obtain a seed for the random number engine
	std::mt19937                    gen(rd());  // Standard mersenne_twister_engine seeded with rd()
	std::normal_distribution<float> dis(0.f, 0.5f);

	// Randomly Init Data
	for (uint32_t i = 0; i < numInstancing; ++i)
	{
		// init pos
		instancingPositions[i] = glm::vec3(dis(gen), dis(gen) + 0.4f, dis(gen));
		instancingTransforms[i][3] = glm::vec4(instancingPositions[i], 1);

		// init rotation axis
		instancingRotationAxis[i] = glm::normalize(glm::vec3(dis(gen), dis(gen), dis(gen)));
	}
}

uint32_t InstancingObject::Reset_InstancingData(int32_t newNum)
{
	numInstancing = newNum;

	// Only resize if bigger. Preserve already Initialized data
	if (numInstancingMax < newNum)
	{
		std::random_device              rd;         // Will be used to obtain a seed for the random number engine
		std::mt19937                    gen(rd());  // Standard mersenne_twister_engine seeded with rd()
		std::normal_distribution<float> dis(0.f, 0.5f);

		instancingTransforms.resize(newNum);
		instancingRotationAxis.resize(newNum);
		instancingPositions.resize(newNum);
		instancingRotations.resize(newNum);

		for (int32_t i = numInstancingMax; i < newNum; ++i)
		{
			// init pos
			instancingPositions[i] = glm::vec3(dis(gen), dis(gen) + 0.4f, dis(gen));
			instancingTransforms[i][3] = glm::vec4(instancingPositions[i], 1);

			// init rotation axis
			instancingRotationAxis[i] = glm::normalize(glm::vec3(dis(gen), dis(gen), dis(gen)));
		}
		numInstancingMax = newNum;
	}

	return numInstancing;
}

void InstancingObject::resetInstancingPositions(const std::vector<glm::vec3>& positions)
{
	// May Not Go Here
	if (positions.empty())
	{
		std::random_device              rd;         // Will be used to obtain a seed for the random number engine
		std::mt19937                    gen(rd());  // Standard mersenne_twister_engine seeded with rd()
		std::normal_distribution<float> dis(0.f, 0.5f);

		// Randomly Init Data
		for (uint32_t i = 0; i < numInstancing; ++i)
		{
			// init pos
			instancingPositions[i] = glm::vec3(dis(gen), dis(gen) + 0.4f, dis(gen));
			instancingTransforms[i][3] = glm::vec4(instancingPositions[i], 1);
		}
	}
	else
	{
		uint32_t posIndex = 0;
		// for default 10 positions
		for (posIndex = 0; posIndex < positions.size(); ++posIndex)
		{
			instancingPositions[posIndex] = positions[posIndex];
			instancingTransforms[posIndex][3] = glm::vec4(instancingPositions[posIndex], 1);
		}
		for (; posIndex < numInstancing; ++posIndex)
		{
			uint32_t originalIndex = posIndex % 10;
			instancingPositions[posIndex] = instancingPositions[originalIndex];
			instancingPositions[posIndex].y += (float)(posIndex / 10) * 0.3f;
			instancingTransforms[posIndex][3] = glm::vec4(instancingPositions[originalIndex], 1);
		}
	}
}

void InstancingObject::Update(float deltaTime)
{
	accTime += deltaTime;
	// Update Original
	DynamicObject::Update(deltaTime);

	// Update Instancing Objs
	for (uint32_t instanceIndex = 0; instanceIndex < numInstancing; ++instanceIndex)
	{
		glm::mat4& refRot = instancingRotations[instanceIndex];

		glm::mat4 scaleMat;

		// Rotate
		float rotatingRad = glm::radians(rotSpeed * deltaTime);
		refRot = glm::rotate(refRot, rotatingRad, instancingRotationAxis[instanceIndex]);

		// Translate
		float addingPosY = moveDistanceMax * glm::sin(accTime + instanceIndex);
		//glm::mat4 translateMat = SimpleUtils::GetTranslation(instancingTransforms[instanceIndex]);
		glm::mat4 translateMat = glm::translate(glm::mat4(), instancingPositions[instanceIndex]);
		translateMat[3].y += addingPosY;

		instancingTransforms[instanceIndex] = translateMat * refRot * scaleMat;
	}	
}

SceneObjectManager::~SceneObjectManager()
{
	for (auto& pair : sceneObjects)
	{
		SceneObject*& refpSceneObj = pair.second;
		delete refpSceneObj;
		refpSceneObj = nullptr;
	}
}

void SceneObjectManager::EmplaceObject(const std::string& name, SceneObject* pInstance, bool isDynamic, bool instancing)
{
	sceneObjects.emplace(name, pInstance);

	if (isDynamic)
		dynamicObjects.emplace(name, dynamic_cast<DynamicObject*>(pInstance));
	if (instancing)
	{
		instancingObjects.emplace(name, dynamic_cast<InstancingObject*>(pInstance));
		totalInstancingCount += dynamic_cast<InstancingObject*>(pInstance)->numInstancing;
	}
}


SceneObject* SceneObjectManager::FindAndGet_SceneObj(const std::string& name)
{
	auto iterator = sceneObjects.find(name);

	if (iterator == sceneObjects.end())
		return nullptr;

	return iterator->second;
}

InstancingObject* SceneObjectManager::FindAndGet_InstancingObj(const std::string& name)
{
	auto iterator = instancingObjects.find(name);

	if (iterator == instancingObjects.end())
		return nullptr;

	return iterator->second;
}



void SceneObjectManager::updateObjects(float deltaTime)
{
	for (auto& pair : dynamicObjects)
	{
		auto& pDynamicObject = pair.second;

		if (pDynamicObject == nullptr)
			continue;

		pDynamicObject->Update(deltaTime);
	}
}