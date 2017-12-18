#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <numeric>
#include <transform_iterator.h>

#include <triangulate.h>

static const float EPSILON = 0.0000000001f;

float Area(const std::vector<glm::vec2> &contour) {
  
	float A = 0.0f;
	for (size_t p = contour.size() - 1, q = 0; q < contour.size(); p = q++) {
		A += contour[p].x * contour[q].y - contour[q].x * contour[p].y;
	}

	return A * 0.5f;
}

bool InsideTriangle(glm::vec2 A, glm::vec2 B, glm::vec2 C, glm::vec2 P) {
	auto a = C - B;
	auto b = A - C;
	auto c = B - A;
	auto ap = P - A;
	auto bp = P - B;
	auto cp = P - C;

	float aCROSSbp = a.x * bp.y - a.y * bp.x;
	float cCROSSap = c.x * ap.y - c.y * ap.x;
	float bCROSScp = b.x * cp.y - b.y * cp.x;

	return aCROSSbp >= 0.0f
		&& bCROSScp >= 0.0f
		&& cCROSSap >= 0.0f;
};

bool Snip(const std::vector<glm::vec2> &contour, size_t u, size_t v, size_t w, const std::vector<int> &V) {
	const auto &A(contour[V[u]]);
	const auto &B(contour[V[v]]);
	const auto &C(contour[V[w]]);

	if (EPSILON > (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x))
		return false;

	for (size_t p = 0; p < V.size(); p++) {
		if (p != u && p != v && p != w
			&& InsideTriangle(A, B, C, contour[V[p]]))
			return false;
	}

	return true;
}

std::vector<uint32_t> polygon_triangulate(const std::vector<glm::vec2> &contour) {
	/* allocate and initialize list of Vertices in polygon */
	if (contour.size() < 3)
		throw std::invalid_argument("less than 3 vertices");

	std::vector<uint32_t> result;
	std::vector<int> V(contour.size());

	/* we want a counter-clockwise polygon in V */

	if (0.0f < Area(contour)) {
		std::iota(std::begin(V), std::end(V), 0);
	} else {
		for (int v = 0; v < contour.size(); v++)
			V[v] = V.size() - 1 - v;
	}

	/*  remove nv-2 Vertices, creating 1 triangle every time */
	int count = 2 * V.size();   /* error detection */

	for (auto v = V.size() - 1; V.size() > 2; ) {
		/* if we loop, it is probably a non-simple polygon */
		if (0 >= count--) {
			//** Triangulate: ERROR - probable bad polygon!
			throw std::invalid_argument("bad polygon");
		}

		/* three consecutive vertices in current polygon, <u,v,w> */
		const auto u = v % V.size();
		v = (u + 1) % V.size();
		const auto w = (v + 1) % V.size();

		if (Snip(contour, u, v, w, V)) {
			/* output Triangle */
			result.push_back(V[u]);
			result.push_back(V[v]);
			result.push_back(V[w]);

			/* remove v from remaining polygon */
			V.erase(std::begin(V) + v);

			/* resest error detection counter */
			count = 2 * V.size();
		}
	}

	return std::move(result);
}

// TODO(gardell): Remove this, only for test
std::vector<glm::vec2> Process(const std::vector<glm::vec2> &contour) {
	const auto indices(polygon_triangulate(contour));
	std::vector<glm::vec2> vertices;
	vertices.reserve(indices.size());
	std::transform(std::cbegin(indices), std::cend(indices), std::back_inserter(vertices), [&](const auto &index) { return contour[index]; });
	return std::move(vertices);
}
