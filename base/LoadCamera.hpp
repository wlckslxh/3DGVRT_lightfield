/*
* Abura Soba, 2025
* 
* Camera Loader class
*
*/

#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include "json.hpp"
#include <glm/gtx/matrix_decompose.hpp>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

#define RAD2DEGREE 57.2957795131

using namespace std;
using nlohmann::json;

enum DatasetType { none, nerf, collmap };

struct CameraFrame {
	string filePath;
	glm::mat4 transformMatrix;
	glm::quat rotation;
	glm::vec3 position;
};

struct NerfCameraData {
	float cameraAngleX;
	glm::mat4 projectionMatrix;
	vector<CameraFrame> frames;
	vector<glm::vec3> centers;
};

class CameraLoader {
private:
	void calcIntrinsics(float cameraAngleX, uint32_t width, uint32_t height, float nearPlane, float farPlane) {
		float fx, fy;
		float cx, cy;

		fx = fy = 0.5f * width / tan(0.5f * cameraAngleX);
		cx = width / 2.0f;
		cy = height / 2.0f;

		fovy = 2 * atan(height / (2 * fy));
		fovy *= RAD2DEGREE;
	}

	void c2wTow2cRT(CameraFrame& frame) {
		glm::mat3 R = glm::mat3(frame.transformMatrix);
		glm::vec3 T = glm::vec3(frame.transformMatrix[3]);
		glm::mat3 R_inv = glm::transpose(R);
		glm::vec3 T_inv = -R_inv * T;
		frame.transformMatrix = glm::mat4(1.0f);
		frame.transformMatrix = glm::mat4(R_inv);
		frame.transformMatrix[3] = glm::vec4(T_inv, 1.0f);

		frame.position = -T_inv;
		frame.rotation = glm::normalize(glm::quat_cast(R_inv));
		nerfCameras.centers.push_back(T_inv);

		frame.position = -glm::vec3(frame.transformMatrix[3]);  // translation
		
		glm::vec3 scale;
		scale.x = glm::length(glm::vec3(frame.transformMatrix[0]));
		scale.y = glm::length(glm::vec3(frame.transformMatrix[1]));
		scale.z = glm::length(glm::vec3(frame.transformMatrix[2]));

		// 회전 행렬 만들기 (scale 제거)
		glm::mat3 rotationMatrix;
		rotationMatrix[0] = glm::vec3(frame.transformMatrix[0]) / scale.x;
		rotationMatrix[1] = glm::vec3(frame.transformMatrix[1]) / scale.y;
		rotationMatrix[2] = glm::vec3(frame.transformMatrix[2]) / scale.z;

		// 회전 쿼터니언 추출
		frame.rotation = glm::quat_cast(rotationMatrix);
	}

public:
	NerfCameraData nerfCameras;
	float fovy;
	vector<string> camNames;

#ifdef __ANDROID__
	json loadJsonFromFile(const std::string& jsonPath) {
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, jsonPath.c_str(), AASSET_MODE_STREAMING);
		if (!asset) {
			throw std::runtime_error("Failed to open asset: " + jsonPath);
		}

		size_t size = AAsset_getLength(asset);
		std::vector<char> buffer(size);
		AAsset_read(asset, buffer.data(), size);
		AAsset_close(asset);

		std::string jsonString(buffer.begin(), buffer.end());
		std::stringstream ss(jsonString);

		json j;
		ss >> j;
		return j;
	}

#else
	json loadJsonFromFile(const std::string& jsonPath) {
		std::ifstream fin(jsonPath);
		if (!fin.is_open()) {
			throw std::runtime_error("Failed to open file : " + jsonPath);
		}
		json j;
		fin >> j;
		return j;
	}
#endif

	void loadNerfCameraData(const string& jsonPath, uint32_t width, uint32_t height, float znear, float zfar) {
		json j = loadJsonFromFile(jsonPath);

		nerfCameras.cameraAngleX = j["camera_angle_x"].get<float>();
		calcIntrinsics(nerfCameras.cameraAngleX, width, height, znear, zfar);
		int i = 0;
		
		for (const auto& frame : j["frames"]) {
			CameraFrame f;
			f.filePath = frame["file_path"].get<string>();
			glm::mat4 c2w;
			const auto& matrix = frame["transform_matrix"];
			for (int row = 0; row < 4; ++row) {
				for (int col = 0; col < 4; ++col) {
					f.transformMatrix[col][row] = frame["transform_matrix"][row][col].get<float>();
				}
			}
			c2wTow2cRT(f);
			nerfCameras.frames.push_back(f);

			camNames.push_back(to_string(i));
			i++;
		}
	}

	void PrintCameraData(const NerfCameraData& data) {
		std::cout << "camera_angle_x: " << data.cameraAngleX << "\n";
		for (const auto& frame : data.frames) {
			std::cout << "File: " << frame.filePath << "\nMatrix:\n";
			cout << "transform matrix : \n";
			for (int i = 0; i < 4; ++i)
				std::cout << frame.transformMatrix[0][i] << " " << frame.transformMatrix[1][i] << " " << frame.transformMatrix[2][i] << " " << frame.transformMatrix[3][i] << "\n";
			std::cout << "-----\n";
		}
	}
};