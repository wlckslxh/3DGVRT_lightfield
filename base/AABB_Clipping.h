#pragma once

#include "VulkanglTFModel.h"

namespace AABB_Triangle_Clipping {
	enum _axis { _X = 0, _Y, _Z };
	enum _side { _MIN = 0, _MAX };

	struct _AABB {
		float xmin, ymin, zmin;
		float xmax, ymax, zmax;
	};

	inline bool _inside(float coord, float boundary, _side side) {
		bool result;
		if (side == _MIN)
			result = coord >= boundary;// -SCENE_EPSILON;
		else
			result = coord <= boundary;// +SCENE_EPSILON;
		return result;
	}

//	inline auto intersect_aabb_triangle(_AABB const& aabb, vkglTF::Vertex** triangle) -> bool
//	{		
//		// Convert AABB to center-extents form
//		glm::vec3 e = glm::vec3((aabb.xmax - aabb.xmin) * 0.5f, (aabb.ymax - aabb.ymin) * 0.5f, (aabb.zmax - aabb.zmin) * 0.5f);
//		glm::vec3 c = glm::vec3(aabb.xmin + e.x, aabb.ymin + e.y, aabb.zmin + e.z);
//
//		glm::vec3 v0 = triangle[0]->pos - c;
//		glm::vec3 v2 = triangle[1]->pos - c;
//		glm::vec3 v1 = triangle[2]->pos - c;
//
//		// Compute the edge vectors of the triangle  (ABC)
//		// That is, get the lines between the points as vectors
//		glm::vec3 f0 = v1 - v0; // B - A
//		glm::vec3 f1 = v2 - v1; // C - B
//		glm::vec3 f2 = v0 - v2; // A - C
//
//		// Compute the face normals of the AABB, because the AABB
//		// is at center, and of course axis aligned, we know that 
//		// it's normals are the X, Y and Z axis.
//		glm::vec3 u0 = glm::vec3(1.0f, 0.0f, 0.0f);
//		glm::vec3 u1 = glm::vec3(0.0f, 1.0f, 0.0f);
//		glm::vec3 u2 = glm::vec3(0.0f, 0.0f, 1.0f);
//
//		// There are a total of 13 axis to test!
//
//		// We first test against 9 axis, these axis are given by
//		// cross product combinations of the edges of the triangle
//		// and the edges of the AABB. You need to get an axis testing
//		// each of the 3 sides of the AABB against each of the 3 sides
//		// of the triangle. The result is 9 axis of seperation
//		// https://awwapp.com/b/umzoc8tiv/
//
//		// Compute the 9 axis
//		glm::vec3 axis_u0_f0 = glm::cross(u0, f0);
//		glm::vec3 axis_u0_f1 = glm::cross(u0, f1);
//		glm::vec3 axis_u0_f2 = glm::cross(u0, f2);
//
//		glm::vec3 axis_u1_f0 = glm::cross(u1, f0);
//		glm::vec3 axis_u1_f1 = glm::cross(u1, f1);
//		glm::vec3 axis_u1_f2 = glm::cross(u2, f2);
//
//		glm::vec3 axis_u2_f0 = glm::cross(u2, f0);
//		glm::vec3 axis_u2_f1 = glm::cross(u2, f1);
//		glm::vec3 axis_u2_f2 = glm::cross(u2, f2);
//
//		// Testing axis: axis_u0_f0
//		// Project all 3 vertices of the triangle onto the Seperating axis
//		float p0 = glm::dot(v0, axis_u0_f0);
//		float p1 = glm::dot(v1, axis_u0_f0);
//		float p2 = glm::dot(v2, axis_u0_f0);
//		// Project the AABB onto the seperating axis
//		// We don't care about the end points of the prjection
//		// just the length of the half-size of the AABB
//		// That is, we're only casting the extents onto the 
//		// seperating axis, not the AABB center. We don't
//		// need to cast the center, because we know that the
//		// aabb is at origin compared to the triangle!
//		float r = e.X * Math.Abs(glm::vec3.Dot(u0, axis_u0_f0)) +
//			e.Y * Math.Abs(glm::vec3.Dot(u1, axis_u0_f0)) +
//			e.Z * Math.Abs(glm::vec3.Dot(u2, axis_u0_f0));
//		// Now do the actual test, basically see if either of
//		// the most extreme of the triangle points intersects r
//		// You might need to write Min & Max functions that take 3 arguments
//		if (Max(-Max(p0, p1, p2), Min(p0, p1, p2)) > r) {
//			// This means BOTH of the points of the projected triangle
//			// are outside the projected half-length of the AABB
//			// Therefore the axis is seperating and we can exit
//			return false;
//		}
//
//		// Repeat this test for the other 8 seperating axis
//		// You may wish to make some kind of a helper function to keep
//		// things readable
//	TODO: 8 more SAT tests
//
//		// Next, we have 3 face normals from the AABB
//		// for these tests we are conceptually checking if the bounding box
//		// of the triangle intersects the bounding box of the AABB
//		// that is to say, the seperating axis for all tests are axis aligned:
//		// axis1: (1, 0, 0), axis2: (0, 1, 0), axis3 (0, 0, 1)
//	TODO: 3 SAT tests
//		// Do the SAT given the 3 primary axis of the AABB
//		// You already have vectors for this: u0, u1 & u2
//
//		// Finally, we have one last axis to test, the face normal of the triangle
//		// We can get the normal of the triangle by crossing the first two line segments
//		glm::vec3 triangleNormal = glm::vec3.Cross(f0, f1);
//TODO: 1 SAT test
//
//// Passed testing for all 13 seperating axis that exist!
//return true;
//	}

	void _clip_polygon_against_plane(std::vector<glm::vec3>& pg_from, std::vector<glm::vec3>& pg_to, float boundary, _axis axis, _side side) {
		bool inside_prev, inside_cur; // are the prev and cur vertices inside the clipping plane, respectively?
		const int pg_from_size = pg_from.size();
		
		pg_to.clear();
		if (pg_from_size == 0) return;
		// for the first vkglTF::Vertex
		inside_prev = _inside(pg_from[0][axis], boundary, side);
		//if (inside_prev)
		//	pg_to.push_back(pg_from[0]);

		for (int index_prev = 0; index_prev < pg_from_size; index_prev++) {
			const int index_cur = (index_prev + 1) % pg_from_size;
			const bool inside_cur = _inside(pg_from[index_cur][axis], boundary, side);

			if (inside_prev) {
				if (inside_cur) { // prev in & cur in
					pg_to.push_back(pg_from[index_cur]);
				}
				else { // prev in & cur out
					float t = fabsf((pg_from[index_prev][axis] - boundary) / (pg_from[index_prev][axis] - pg_from[index_cur][axis]));

					glm::vec3 vertex;
					vertex = t * pg_from[index_cur] + (1.0f - t) * pg_from[index_prev];

					pg_to.push_back(vertex);
				}
			}
			else {
				if (inside_cur) { // prev out & cur in
					float t = fabsf((pg_from[index_prev][axis] - boundary) / (pg_from[index_prev][axis] - pg_from[index_cur][axis]));

					glm::vec3 vertex;
					vertex = t * pg_from[index_cur] + (1.0f - t) * pg_from[index_prev];

					pg_to.push_back(vertex);
					
					pg_to.push_back(pg_from[index_cur]);
				}
				else { // prev out & cur out
					// do nothing
				}
			}

			inside_prev = inside_cur;
		}
	}

	void _clip_triangle_against_AABB(glm::vec3* triangle, _AABB& AABB, std::vector<glm::vec3>& polygon) {
		// clip triangle against AABB and return the resulting polygon.
		// To call this function
		// 1. set up triangle[0], triangle[1], triangle[2] so that their coord stores p[0], p[1], and p[2], and
		//    their st stores (1.0, 0.0), (0.0, 1.0), and (0.0, 0.0), respectively.
		// 2. set up AABB's coordinates.
		//  Then, calling this function returns a convex polygon.
		//  From this (non-null) polygon, generate a list of triangles using delauneay trianglulation (for a convex polygon).

		std::vector<glm::vec3> pg[2]; // flip its function during iterations
		int pg_from = 0;

		// initially copy the input triangle to pg_from
		pg[pg_from].clear();
		for (int i = 0; i < 3; i++)
			pg[pg_from].push_back(triangle[i]);

		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.xmin, _X, _MIN);
		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.xmax, _X, _MAX);

		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.ymin, _Y, _MIN);
		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.ymax, _Y, _MAX);

		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.zmin, _Z, _MIN);
		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.zmax, _Z, _MAX);

		// do this more efficiently ???
		polygon.clear();
		for (int i = 0; i < pg[1 - pg_from].size(); i++)
			polygon.push_back(pg[1 - pg_from][i]);
	}

	void _clip_triangle_against_AABB_np(glm::vec3 triangle[3], _AABB& AABB, std::vector<glm::vec3>& polygon) {
		// clip triangle against AABB and return the resulting polygon.
		// To call this function
		// 1. set up triangle[0], triangle[1], triangle[2] so that their coord stores p[0], p[1], and p[2], and
		//    their st stores (1.0, 0.0), (0.0, 1.0), and (0.0, 0.0), respectively.
		// 2. set up AABB's coordinates.
		//  Then, calling this function returns a convex polygon.
		//  From this (non-null) polygon, generate a list of triangles using delauneay trianglulation (for a convex polygon).

		std::vector<glm::vec3> pg[2]; // flip its function during iterations
		int pg_from = 0;

		// initially copy the input triangle to pg_from
		pg[pg_from].clear();
		for (int i = 0; i < 3; i++)
			pg[pg_from].push_back(triangle[i]);

		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.xmin, _X, _MIN);
		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.xmax, _X, _MAX);

		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.ymin, _Y, _MIN);
		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.ymax, _Y, _MAX);

		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.zmin, _Z, _MIN);
		pg_from = 1 - pg_from;
		_clip_polygon_against_plane(pg[pg_from], pg[1 - pg_from], AABB.zmax, _Z, _MAX);

		// do this more efficiently ???
		polygon.clear();
		for (int i = 0; i < pg[1 - pg_from].size(); i++)
			polygon.push_back(pg[1 - pg_from][i]);
	}
}