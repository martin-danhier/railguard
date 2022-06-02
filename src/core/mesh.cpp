#include "railguard/core/mesh.h"

#include <railguard/core/renderer/renderer.h>

#include <iostream>
#include <tiny_obj_loader.h>

namespace rg
{

    MeshPart::MeshPart(Vector<Vertex> &&vertices, Vector<Triangle> &&triangles)
        : m_vertices(std::move(vertices)),
          m_triangles(std::move(triangles))
    {
    }

    MeshPartId MeshPart::load_from_obj(const char *filename, Renderer &renderer, bool duplicate_vertices)
    {
        // Attrib will contain the vertex arrays
        tinyobj::attrib_t attrib;
        // Shapes contain the infos for each separate object in the file
        std::vector<tinyobj::shape_t> shapes;
        // Material contains the infos about the material of each shape
        std::vector<tinyobj::material_t> materials;

        // Store errors and warnings
        std::string warn;
        std::string err;

        // Get the directory of the file, so that we can look for the material relatively to the model
        std::string dir = filename;
        dir             = dir.substr(0, dir.find_last_of('/') + 1);

        // Load the OBJ file
        tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, dir.c_str());

        // Print warnings and errors
        if (!warn.empty())
        {
            std::cout << "[Mesh Loader Warning] " << warn;
        }
        // If we have any error, print it to the console, and break the mesh loading.
        // This happens if the file can't be found or is malformed
        if (!err.empty())
        {
            std::cerr << "[Mesh Loader Error] " << err;
            return NULL_ID;
        }

        // Hardcode the loading of triangles
        constexpr int32_t vertices_per_face = 3;

        Vector<Vertex>   vertices(attrib.vertices.size() / 3);
        Vector<Triangle> triangles(shapes.size() * vertices_per_face);

        // Add all vertices
        if (!duplicate_vertices)
        {
            for (auto i = 0; i < attrib.vertices.size(); i += vertices_per_face)
            {
                vertices.push_back(Vertex {
                    .position = {attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2]},
                });
            }

            // For now, we merge all the shapes into one mesh part
            // When the tree structure is implemented, we will have to split the mesh into mesh parts
            // TODO
            for (auto &shape : shapes)
            {
                // We have an indexed model, but not as much as the Wavefront format
                // The wavefront format supports having 3 indices per vertex to also avoid duplicating normals and texture
                // coords
                // Here, we only have one index per vertex
                // So we will need to store the normals and texture coords with the vertex itself

                // Note that for now, this doesn't support having several values for the same vertex in different faces
                // If the values change, later faces will overwrite the first
                // Maybe later, store normals and texture coords separately and find a way to index them
                // Alternatively, duplicate a vertex if the combination with normal and tex coords change.

                size_t index_offset = 0;

                // For each face
                for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
                {
                    Triangle triangle {0, 0, 0};

                    // For each vertex of that face
                    for (size_t v = 0; v < vertices_per_face; v++)
                    {
                        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                        // Store triangle indices for index buffer
                        triangle.index[v] = idx.vertex_index;

                        // Store the normal in the associated vertex
                        vertices[idx.vertex_index].normal = glm::vec3 {
                            attrib.normals[3 * idx.normal_index + 0],
                            attrib.normals[3 * idx.normal_index + 1],
                            attrib.normals[3 * idx.normal_index + 2],
                        };

                        // Store the texture coords as well
                        vertices[idx.vertex_index].tex_coord = glm::vec2 {
                            attrib.texcoords[2 * idx.texcoord_index + 0],
                            1 - attrib.texcoords[2 * idx.texcoord_index + 1],
                        };
                    }

                    triangles.push_back(triangle);

                    index_offset += vertices_per_face;
                }
            }
        }
        else
        {
            for (auto &shape : shapes)
            {
                size_t index_offset = 0;

                // For each face
                for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
                {
                    Triangle triangle {0, 0, 0};

                    // For each vertex of that face
                    for (size_t v = 0; v < vertices_per_face; v++)
                    {
                        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                        // Store triangle indices for index buffer
                        triangle.index[v] = vertices.size();

                        vertices.push_back(Vertex {
                            .position =
                                glm::vec3 {
                                    attrib.vertices[3 * idx.vertex_index + 0],
                                    attrib.vertices[3 * idx.vertex_index + 1],
                                    attrib.vertices[3 * idx.vertex_index + 2],
                                },
                            .normal =
                                glm::vec3 {
                                    attrib.normals[3 * idx.normal_index + 0],
                                    attrib.normals[3 * idx.normal_index + 1],
                                    attrib.normals[3 * idx.normal_index + 2],
                                },
                            .tex_coord =
                                glm::vec2 {
                                    attrib.texcoords[2 * idx.texcoord_index + 0],
                                    1 - attrib.texcoords[2 * idx.texcoord_index + 1],
                                },
                        });
                    }

                    triangles.push_back(triangle);
                    index_offset += vertices_per_face;
                }
            }
        }

        // Now, we have our mesh part
        // TODO and soon we will have to store it in the tree structure
        // For now, just add that mesh and return its id
        return renderer.save_mesh_part(MeshPart(std::move(vertices), std::move(triangles)));
    }
} // namespace rg