#include "City.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

// ============================================================================
// Anonymous namespace - Constants and helpers
// ============================================================================
namespace
{
    // ========================================================================
    // Constants (tunable parameters)
    // ------------------------------------------------------------------------
    
    // Geometry precision
    constexpr float LEN_EPSILON = 1e-4f;           // Tolerance for zero-length vectors
    
    // Gap fill mesh generation
    constexpr float GAP_COVER_MARGIN = 4.5f;       // How far to expand the cover mesh (meters)
    constexpr float TOP_DROP = 0.02f;              // Small downward offset to avoid z-fighting
    constexpr float BACKING_DROP = 2.0f;           // How far down the backing extends (meters)
    constexpr float VERTEX_RESERVE_FACTOR = 21.0f; // Approximate vertices per triangle (3 faces × 7 vertices)
    
    // Colors (RGB)
    const glm::vec3 BACKING_COLOR(0.46f, 0.46f, 0.46f);  // Gray for bottom faces
    const glm::vec3 SKIRT_COLOR(0.44f, 0.44f, 0.44f);    // Slightly darker gray for sides
    
    // ------------------------------------------------------------------------
    // Geometry Helpers
    // ------------------------------------------------------------------------
    
    /**
     * Expands a vertex outward from a center point in the XZ plane.
     * @param vertex The original vertex position
     * @param center The center point to expand away from
     * @param margin The distance to expand (must be > 0)
     * @return Expanded vertex position
     */
    glm::vec3 ExpandVertexXZ(const glm::vec3& vertex, const glm::vec3& center, float margin)
    {
        // Safety: margin must be positive
        if (margin <= 0.0f)
        {
            return vertex;
        }
        
        glm::vec2 dir(vertex.x - center.x, vertex.z - center.z);
        float len = glm::length(dir);
        
        if (len > LEN_EPSILON)
        {
            dir /= len;
            return glm::vec3(vertex.x + dir.x * margin, vertex.y, vertex.z + dir.y * margin);
        }
        
        return vertex;
    }
    
    // ------------------------------------------------------------------------
    // Mesh Building Helpers
    // ------------------------------------------------------------------------
    
    /**
     * Adds a single vertex to the mesh with default UVs.
     */
    void AddVertex(std::vector<Vertex>& vertices, 
                   const glm::vec3& position, 
                   const glm::vec3& normal, 
                   const glm::vec3& color)
    {
        Vertex vertex;
        vertex.position = position;
        vertex.normal = normal;
        vertex.color = color;
        vertex.texUV = glm::vec2(0.0f);  // No texture coordinates needed for gap fill
        vertices.push_back(vertex);
    }
    
    /**
     * Adds a triangle to the mesh with a flat normal pointing up.
     */
    void AddTriangle(std::vector<Vertex>& vertices, 
                     std::vector<GLuint>& indices,
                     const glm::vec3& a, 
                     const glm::vec3& b, 
                     const glm::vec3& c,
                     const glm::vec3& color)
    {
        // Flat normal pointing upward (for gap fill meshes)
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        
        const GLuint start = static_cast<GLuint>(vertices.size());
        
        AddVertex(vertices, a, normal, color);
        AddVertex(vertices, b, normal, color);
        AddVertex(vertices, c, normal, color);
        
        indices.push_back(start);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
    }
    
    /**
     * Adds a quad (two triangles) to the mesh.
     * Order: a → b → c → d (clockwise or counter-clockwise)
     */
    void AddQuad(std::vector<Vertex>& vertices, 
                 std::vector<GLuint>& indices,
                 const glm::vec3& a, 
                 const glm::vec3& b, 
                 const glm::vec3& c, 
                 const glm::vec3& d,
                 const glm::vec3& color)
    {
        AddTriangle(vertices, indices, a, b, c, color);
        AddTriangle(vertices, indices, c, d, a, color);
    }
    
    // ------------------------------------------------------------------------
    // Texture Helpers
    // ------------------------------------------------------------------------
    
    /**
     * Creates a 1x1 pixel texture for solid color rendering.
     * Uses static storage to avoid recreating identical textures.
     */
    const Texture& GetWhiteDiffuseTexture()
    {
        static const unsigned char whitePixel[] = {255, 255, 255, 255};
        static Texture whiteTexture(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
        return whiteTexture;
    }
    
    const Texture& GetBlackSpecularTexture()
    {
        static const unsigned char blackPixel[] = {0, 0, 0, 255};
        static Texture blackTexture(blackPixel, 1, 1, GL_RGBA, "specular", 1);
        return blackTexture;
    }
}

// ============================================================================
// City Class Implementation
// ============================================================================

City::City(const std::string& modelPath, float scale, float yOffset, float xOffset, float zOffset, bool autoAlign)
    : mModel(modelPath.c_str())
    , mScale(scale)
    , mYOffset(yOffset)
    , mXOffset(xOffset)
    , mZOffset(zOffset)
{
    if (autoAlign)
    {
        AlignModelToGround();
    }
    
    mPhysics.Initialize(mModel, GetMatrix());
    BuildVisualGapFillMesh();
}

// ----------------------------------------------------------------------------
// Private Helpers
// ----------------------------------------------------------------------------

void City::AlignModelToGround()
{
    float minY = 1e9f;
    const auto& meshes = mModel.GetMeshes();
    const auto& matrices = mModel.GetMatricesMeshes();
    
    for (size_t i = 0; i < meshes.size(); ++i)
    {
        const glm::mat4 meshMatrix = matrices[i];
        for (const auto& vertex : meshes[i].vertices)
        {
            const glm::vec3 worldPos = glm::vec3(meshMatrix * glm::vec4(vertex.position, 1.0f));
            if (worldPos.y < minY)
            {
                minY = worldPos.y;
            }
        }
    }
    
    if (minY == 1e9f)
    {
        minY = 0.0f;  // Fallback if no vertices found
    }
    
    mYOffset = -minY * mScale + 0.01f;  // 0.01f to prevent z-fighting with ground
}

void City::BuildVisualGapFillMesh()
{
    const std::vector<game::WorldTriangle>& roadTriangles = mPhysics.GetRoadTriangles();
    
    if (roadTriangles.empty())
    {
        mVisualGapFillMesh.reset();
        return;
    }
    
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    // Pre-allocate memory for performance
    vertices.reserve(static_cast<size_t>(roadTriangles.size() * VERTEX_RESERVE_FACTOR));
    indices.reserve(vertices.capacity() * 3);
    
    for (const game::WorldTriangle& tri : roadTriangles)
    {
        AddGapFillForTriangle(tri, vertices, indices);
    }
    
    // Build mesh with shared textures
    std::vector<Texture> textures;
    textures.push_back(GetWhiteDiffuseTexture());
    textures.push_back(GetBlackSpecularTexture());
    
    mVisualGapFillMesh.reset(new Mesh(vertices, indices, textures));
}

void City::AddGapFillForTriangle(const game::WorldTriangle& tri,
                                  std::vector<Vertex>& vertices,
                                  std::vector<GLuint>& indices) const
{
    const glm::vec3 center = (tri.a + tri.b + tri.c) / 3.0f;
    
    // Top face (slightly lowered to avoid z-fighting)
    const glm::vec3 aTop = tri.a - glm::vec3(0.0f, TOP_DROP, 0.0f);
    const glm::vec3 bTop = tri.b - glm::vec3(0.0f, TOP_DROP, 0.0f);
    const glm::vec3 cTop = tri.c - glm::vec3(0.0f, TOP_DROP, 0.0f);
    
    // Bottom/backing face (expanded outward and lowered)
    const glm::vec3 aBack = ExpandVertexXZ(tri.a, center, GAP_COVER_MARGIN) - glm::vec3(0.0f, BACKING_DROP, 0.0f);
    const glm::vec3 bBack = ExpandVertexXZ(tri.b, center, GAP_COVER_MARGIN) - glm::vec3(0.0f, BACKING_DROP, 0.0f);
    const glm::vec3 cBack = ExpandVertexXZ(tri.c, center, GAP_COVER_MARGIN) - glm::vec3(0.0f, BACKING_DROP, 0.0f);
    
    // Bottom triangle
    AddTriangle(vertices, indices, aBack, bBack, cBack, BACKING_COLOR);
    
    // Side walls (quads)
    AddQuad(vertices, indices, aTop, bTop, bBack, aBack, SKIRT_COLOR);
    AddQuad(vertices, indices, bTop, cTop, cBack, bBack, SKIRT_COLOR);
    AddQuad(vertices, indices, cTop, aTop, aBack, cBack, SKIRT_COLOR);
}

// ----------------------------------------------------------------------------
// Public Interface
// ----------------------------------------------------------------------------

glm::mat4 City::GetMatrix() const
{
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(mXOffset, mYOffset, mZOffset));
    transform = glm::scale(transform, glm::vec3(mScale));
    return transform;
}

void City::Draw(Shader& shader, Camera& camera)
{
    // Draw the main city model
    mModel.Draw(shader, camera, GetMatrix());
    
    // Draw visual gap fill mesh if it exists
    if (mVisualGapFillMesh)
    {
        shader.Activate();
        glUniform1i(glGetUniformLocation(shader.ID, "uUseAlpha"), 0);
        glUniform1i(glGetUniformLocation(shader.ID, "uIsEmissive"), 0);
        mVisualGapFillMesh->Draw(shader, camera, glm::mat4(1.0f));
    }
}

float City::GetHeightAt(float x, float z, float currentY, bool* outFound, float snapDownMax, float snapUpMax) const
{
    return mPhysics.GetHeightAt(x, z, currentY, outFound, snapDownMax, snapUpMax);
}

bool City::GetGroundSample(const glm::vec3& worldPos, float currentY, game::GroundSample& outSample, float snapDownMax, float snapUpMax) const
{
    return mPhysics.GetGroundSample(worldPos, currentY, outSample, snapDownMax, snapUpMax);
}

bool City::CheckCollision(const glm::vec3& pos, float radius) const
{
    return mPhysics.CheckCollision(pos, radius);
}

glm::vec3 City::GetBestRoadSpawn(const glm::vec3& preferred, float maxDistance) const
{
    return mPhysics.GetBestRoadSpawn(preferred, maxDistance);
}

glm::vec3 City::GetWorldMinBounds() const
{
    return mPhysics.GetWorldMinBounds();
}

glm::vec3 City::GetWorldMaxBounds() const
{
    return mPhysics.GetWorldMaxBounds();
}