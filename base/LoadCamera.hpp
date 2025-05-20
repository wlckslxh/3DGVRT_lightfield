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

#define RAD2DEGREE 57.2957795131

using namespace std;
using json = nlohmann::json;

struct CameraFrame {
	string filePath;
	glm::mat4 transformMatrix;
	glm::mat3 rotation;
	glm::mat3 position;
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
		nerfCameras.centers.push_back(T_inv);
	}

public:
	NerfCameraData nerfCameras;
	float fovy;

	void loadNerfCameraData(const string& jsonPath, uint32_t width, uint32_t height, float znear, float zfar) {
		ifstream fin(jsonPath);
		if (!fin.is_open()) {
			throw runtime_error("Failed to open file : " + jsonPath);
		}

		json j;
		fin >> j;

		nerfCameras.cameraAngleX = j["camera_angle_x"].get<float>();
		calcIntrinsics(nerfCameras.cameraAngleX, width, height, znear, zfar);
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