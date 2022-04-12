#pragma once

#include <railguard/utils/vector.h>

#include <glm/vec3.hpp>

namespace rg
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
    };

    struct Triangle
    {
        uint32_t index[3] = {0, 0, 0};

        inline Triangle(uint32_t i0, uint32_t i1, uint32_t i2) : index {i0, i1, i2}
        {
        }
    };

    /** Set of vertices and indices constituting a shape, e.g a cube. */
    class MeshPart
    {
      private:
        Vector<Vertex>   m_vertices {10};
        Vector<Triangle> m_triangles {10};

      public:
        MeshPart(Vector<Vertex> &&vertices, Vector<Triangle> &&triangles);

        [[nodiscard]] inline const Vector<Vertex> &vertices() const
        {
            return m_vertices;
        }
        [[nodiscard]] inline const Vector<Triangle> &triangles() const
        {
            return m_triangles;
        }

        [[nodiscard]] static constexpr size_t vertex_byte_size()
        {
            return sizeof(Vertex);
        }
        [[nodiscard]] static constexpr size_t triangle_byte_size()
        {
            return sizeof(Triangle);
        }
        [[nodiscard]] static constexpr size_t index_byte_size()
        {
            return sizeof(uint32_t);
        }

        [[nodiscard]] inline size_t vertex_count() const
        {
            return m_vertices.size();
        }
        [[nodiscard]] inline size_t triangle_count() const
        {
            return m_triangles.size();
        }
    };
} // namespace rg