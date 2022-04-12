#include "railguard/core/mesh.h"

namespace rg
{

    MeshPart::MeshPart(Vector<Vertex> &&vertices, Vector<Triangle> &&triangles)
        : m_vertices(std::move(vertices)),
          m_triangles(std::move(triangles))
    {
    }
} // namespace rg