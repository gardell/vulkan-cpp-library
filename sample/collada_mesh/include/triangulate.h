// TODO(gardell): Might want to implement our own more efficient algorithm with Delaunay triangulation etc.

// COTD Entry submitted by John W. Ratcliff [jratcliff@verant.com]

// ** THIS IS A CODE SNIPPET WHICH WILL EFFICIEINTLY TRIANGULATE ANY
// ** POLYGON/CONTOUR (without holes) AS A STATIC CLASS.  THIS SNIPPET
// ** IS COMPRISED OF 3 FILES, TRIANGULATE.H, THE HEADER FILE FOR THE
// ** TRIANGULATE BASE CLASS, TRIANGULATE.CPP, THE IMPLEMENTATION OF
// ** THE TRIANGULATE BASE CLASS, AND TEST.CPP, A SMALL TEST PROGRAM
// ** DEMONSTRATING THE USAGE OF THE TRIANGULATOR.  THE TRIANGULATE
// ** BASE CLASS ALSO PROVIDES TWO USEFUL HELPER METHODS, ONE WHICH
// ** COMPUTES THE AREA OF A POLYGON, AND ANOTHER WHICH DOES AN EFFICENT
// ** POINT IN A TRIANGLE TEST.
// ** SUBMITTED BY JOHN W. RATCLIFF (jratcliff@verant.com) July 22, 2000

#ifndef TRIANGULATE_H

#define TRIANGULATE_H

/*****************************************************************/
/** Static class to triangulate any contour/polygon efficiently **/
/** You should replace Vector2d with whatever your own Vector   **/
/** class might be.  Does not support polygons with holes.      **/
/** Uses STL vectors to represent a dynamic array of vertices.  **/
/** This code snippet was submitted to FlipCode.com by          **/
/** John W. Ratcliff (jratcliff@verant.com) on July 22, 2000    **/
/** I did not write the original code/algorithm for this        **/
/** this triangulator, in fact, I can't even remember where I   **/
/** found it in the first place.  However, I did rework it into **/
/** the following black-box static class so you can make easy   **/
/** use of it in your own code.  Simply replace Vector2d with   **/
/** whatever your own Vector implementation might be.           **/
/*****************************************************************/

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

// throws exceptions
std::vector<glm::vec2> Process(const std::vector<glm::vec2> &contour);

template<typename It>
glm::vec3 polygon_normal(It begin, It end) {
	glm::vec3 normal(0, 0, 0);

	for (auto it(begin); it != end; ++it) {
		const auto &current(*it), &next(*(++It(it) != end ? ++It(it) : begin));
		normal += (current - next).yzx * (current + next).zxy;
	}

	return normal;
}

// TODO(gardell): Remove
// throws exceptions
std::vector<glm::vec3> Process(const std::vector<glm::vec3> &contour);

// throws exceptions
std::vector<uint32_t> polygon_triangulate(const std::vector<glm::vec2> &contour);

// throws exception
template<typename It>
std::vector<uint32_t> polygon_triangulate(It begin, It end) {
	const auto rotation(glm::rotation(
		glm::normalize(polygon_normal(begin, end)),
		glm::vec3(0, 0, 1)));

	std::vector<glm::vec2> flat;
	flat.reserve(std::distance(begin, end));
	std::transform(
		std::forward<It>(begin),
		std::forward<It>(end),
		std::back_inserter(flat),
		[&](const auto &vertex) {
			return glm::vec2(glm::rotate(rotation, vertex).xy);
		});

	return polygon_triangulate(flat);
}

#endif
