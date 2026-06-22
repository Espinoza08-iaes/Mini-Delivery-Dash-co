#include "Pathfinder.h"
#include <iostream>
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <cmath>
#include <cstring>

Pathfinder::Pathfinder() : mNavMesh(nullptr), mNavQuery(nullptr) {}

Pathfinder::~Pathfinder() {
    Cleanup();
}

void Pathfinder::Cleanup() {
    if (mNavMesh) dtFreeNavMesh(mNavMesh);
    if (mNavQuery) dtFreeNavMeshQuery(mNavQuery);
    mNavMesh = nullptr;
    mNavQuery = nullptr;
}

void Pathfinder::Initialize(const std::vector<game::WorldTriangle>& roadTriangles, const glm::vec3& minBounds, const glm::vec3& maxBounds) {
    Cleanup();
    if (roadTriangles.empty()) return;

    std::cout << "[PATHFINDER] Generating Recast NavMesh..." << std::endl;

    // Use ALL road triangles - rcErodeWalkableArea handles edge avoidance
    std::vector<float> verts;
    std::vector<int> tris;
    verts.reserve(roadTriangles.size() * 9);
    tris.reserve(roadTriangles.size() * 3);

    for (size_t i = 0; i < roadTriangles.size(); ++i) {
        if (!roadTriangles[i].isStrictlyStreet) continue;
        
        int vBase = (int)(verts.size() / 3);
        verts.push_back(roadTriangles[i].a.x); verts.push_back(roadTriangles[i].a.y); verts.push_back(roadTriangles[i].a.z);
        verts.push_back(roadTriangles[i].b.x); verts.push_back(roadTriangles[i].b.y); verts.push_back(roadTriangles[i].b.z);
        verts.push_back(roadTriangles[i].c.x); verts.push_back(roadTriangles[i].c.y); verts.push_back(roadTriangles[i].c.z);
        tris.push_back(vBase);
        tris.push_back(vBase + 1);
        tris.push_back(vBase + 2);
    }

    int ntris = (int)(tris.size() / 3);
    int nverts = (int)(verts.size() / 3);
    if (ntris == 0) {
        std::cerr << "[PATHFINDER] No road triangles found!" << std::endl;
        return;
    }
    std::cout << "[PATHFINDER] Road triangles: " << ntris << ", vertices: " << nverts << std::endl;

    // --- Recast Configuration ---
    // Tuned for city streets with cars (~2m wide vehicles)
    rcConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cs = 0.3f;            // Cell size (smaller = more precise NavMesh)
    cfg.ch = 0.2f;            // Cell height
    cfg.walkableSlopeAngle = 35.0f;  // Reject steep slopes
    cfg.walkableHeight = (int)ceilf(2.0f / cfg.ch);   // Agent height
    cfg.walkableClimb = (int)floorf(0.4f / cfg.ch);    // Max step height (curbs ~0.4m)
    cfg.walkableRadius = (int)ceilf(1.5f / cfg.cs);    // Agent radius (half car width)
    cfg.maxEdgeLen = (int)(12.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = (int)rcSqr(4.0f);      // Smaller minimum region
    cfg.mergeRegionArea = (int)rcSqr(10.0f);    // Merge small regions
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = cfg.cs * 6.0f;
    cfg.detailSampleMaxError = 1.0f;

    // Bounds
    cfg.bmin[0] = minBounds.x - 5.0f;
    cfg.bmin[1] = minBounds.y - 5.0f;
    cfg.bmin[2] = minBounds.z - 5.0f;
    cfg.bmax[0] = maxBounds.x + 5.0f;
    cfg.bmax[1] = maxBounds.y + 5.0f;
    cfg.bmax[2] = maxBounds.z + 5.0f;

    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
    std::cout << "[PATHFINDER] Grid: " << cfg.width << "x" << cfg.height << std::endl;

    rcContext ctx(false);

    // Step 1: Heightfield
    rcHeightfield* solid = rcAllocHeightfield();
    if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
        std::cerr << "[PATHFINDER] Could not create heightfield." << std::endl;
        return;
    }

    // Step 2: Rasterize
    unsigned char* triAreas = new unsigned char[ntris];
    memset(triAreas, 0, ntris);
    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts.data(), nverts, tris.data(), ntris, triAreas);
    if (!rcRasterizeTriangles(&ctx, verts.data(), nverts, tris.data(), triAreas, ntris, *solid, cfg.walkableClimb)) {
        std::cerr << "[PATHFINDER] Could not rasterize triangles." << std::endl;
        delete[] triAreas;
        return;
    }
    delete[] triAreas;

    // Step 3: Filter
    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    // Step 4: Compact heightfield
    rcCompactHeightfield* chf = rcAllocCompactHeightfield();
    if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf)) {
        rcFreeHeightField(solid);
        return;
    }
    rcFreeHeightField(solid);

    // Step 5: Erode walkable area by agent radius (keeps NPCs away from walls/curbs)
    if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) return;

    // Step 6: Build regions
    if (!rcBuildDistanceField(&ctx, *chf)) return;
    if (!rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) return;

    // Step 7: Contours
    rcContourSet* cset = rcAllocContourSet();
    if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset)) return;

    // Step 8: Poly mesh
    rcPolyMesh* pmesh = rcAllocPolyMesh();
    if (!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh)) return;

    // Step 9: Detail mesh
    rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
    if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh)) return;

    rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);

    std::cout << "[PATHFINDER] NavMesh polygons: " << pmesh->npolys << ", verts: " << pmesh->nverts << std::endl;

    // Step 10: Create Detour NavMesh
    if (pmesh->npolys > 0 && cfg.maxVertsPerPoly <= DT_VERTS_PER_POLYGON) {
        unsigned char* navData = 0;
        int navDataSize = 0;

        // Set all poly flags to 1 (walkable)
        for (int i = 0; i < pmesh->npolys; ++i) {
            pmesh->flags[i] = 1;
        }

        dtNavMeshCreateParams params;
        memset(&params, 0, sizeof(params));
        params.verts = pmesh->verts;
        params.vertCount = pmesh->nverts;
        params.polys = pmesh->polys;
        params.polyAreas = pmesh->areas;
        params.polyFlags = pmesh->flags;
        params.polyCount = pmesh->npolys;
        params.nvp = pmesh->nvp;
        params.detailMeshes = dmesh->meshes;
        params.detailVerts = dmesh->verts;
        params.detailVertsCount = dmesh->nverts;
        params.detailTris = dmesh->tris;
        params.detailTriCount = dmesh->ntris;
        params.walkableHeight = cfg.walkableHeight * cfg.ch;
        params.walkableRadius = cfg.walkableRadius * cfg.cs;
        params.walkableClimb = cfg.walkableClimb * cfg.ch;
        rcVcopy(params.bmin, pmesh->bmin);
        rcVcopy(params.bmax, pmesh->bmax);
        params.cs = cfg.cs;
        params.ch = cfg.ch;
        params.buildBvTree = true;

        if (dtCreateNavMeshData(&params, &navData, &navDataSize)) {
            mNavMesh = dtAllocNavMesh();
            dtStatus status = mNavMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
            if (dtStatusFailed(status)) {
                dtFree(navData);
                std::cerr << "[PATHFINDER] Detour init failed!" << std::endl;
            } else {
                mNavQuery = dtAllocNavMeshQuery();
                mNavQuery->init(mNavMesh, 4096);
                std::cout << "[PATHFINDER] Recast NavMesh successfully generated!" << std::endl;
            }
        }
    }

    rcFreePolyMesh(pmesh);
    rcFreePolyMeshDetail(dmesh);
}

std::vector<glm::vec3> Pathfinder::FindPath(const glm::vec3& start, const glm::vec3& end) const {
    if (!mNavMesh || !mNavQuery) return {};

    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    float extents[3] = {15.0f, 15.0f, 15.0f};
    
    dtPolyRef startRef, endRef;
    float startPt[3], endPt[3];
    
    float s[3] = {start.x, start.y, start.z};
    float e[3] = {end.x, end.y, end.z};

    mNavQuery->findNearestPoly(s, extents, &filter, &startRef, startPt);
    mNavQuery->findNearestPoly(e, extents, &filter, &endRef, endPt);

    if (!startRef || !endRef) return {};

    dtPolyRef path[256];
    int pathCount;
    mNavQuery->findPath(startRef, endRef, startPt, endPt, &filter, path, &pathCount, 256);

    if (pathCount == 0) return {};

    float straightPath[256 * 3];
    unsigned char straightPathFlags[256];
    dtPolyRef straightPathPolys[256];
    int straightPathCount;

    mNavQuery->findStraightPath(startPt, endPt, path, pathCount,
                                straightPath, straightPathFlags,
                                straightPathPolys, &straightPathCount, 256);

    std::vector<glm::vec3> result;
    for (int i = 0; i < straightPathCount; ++i) {
        result.push_back(glm::vec3(straightPath[i*3], straightPath[i*3+1], straightPath[i*3+2]));
    }
    return result;
}

static float MyFrand() { return (float)rand() / (float)RAND_MAX; }

glm::vec3 Pathfinder::GetRandomNavPoint() const {
    if (!mNavMesh || !mNavQuery) return glm::vec3(0.0f);
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);
    
    dtPolyRef randomRef;
    float randomPt[3];
    
    if (dtStatusSucceed(mNavQuery->findRandomPoint(&filter, MyFrand, &randomRef, randomPt))) {
        return glm::vec3(randomPt[0], randomPt[1], randomPt[2]);
    }
    
    return glm::vec3(0.0f);
}

glm::vec3 Pathfinder::FindNearestPointOnNavMesh(const glm::vec3& pos) const {
    if (!mNavMesh || !mNavQuery) return pos;
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);
    
    float extents[3] = {30.0f, 30.0f, 30.0f};
    float p[3] = {pos.x, pos.y, pos.z};
    
    dtPolyRef nearestRef;
    float nearestPt[3];
    
    dtStatus status = mNavQuery->findNearestPoly(p, extents, &filter, &nearestRef, nearestPt);
    if (dtStatusSucceed(status) && nearestRef) {
        return glm::vec3(nearestPt[0], nearestPt[1], nearestPt[2]);
    }
    
    return pos;
}

bool Pathfinder::IsOnNavMesh(const glm::vec3& pos, float tolerance) const {
    if (!mNavMesh || !mNavQuery) return false;
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);
    
    float extents[3] = {tolerance, tolerance, tolerance};
    float p[3] = {pos.x, pos.y, pos.z};
    
    dtPolyRef nearestRef;
    float nearestPt[3];
    
    dtStatus status = mNavQuery->findNearestPoly(p, extents, &filter, &nearestRef, nearestPt);
    if (dtStatusFailed(status) || !nearestRef) return false;
    
    float dx = pos.x - nearestPt[0];
    float dz = pos.z - nearestPt[2];
    float distSq = dx * dx + dz * dz;
    return distSq < (tolerance * tolerance);
}

void Pathfinder::MarkAreaAsObstacle(const glm::vec3& pos, float radius) {
    if (!mNavMesh || !mNavQuery) return;
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);
    
    float extents[3] = {radius, radius, radius};
    float p[3] = {pos.x, pos.y, pos.z};
    
    dtPolyRef polys[256];
    int polyCount = 0;
    
    // Find all polygons around the stuck position
    dtStatus status = mNavQuery->queryPolygons(p, extents, &filter, polys, &polyCount, 256);
    if (dtStatusSucceed(status)) {
        for (int i = 0; i < polyCount; ++i) {
            // Unset the walkable flag (1) for this polygon, effectively making it a dead zone
            unsigned short flags = 0;
            mNavMesh->getPolyFlags(polys[i], &flags);
            flags &= ~1; // Remove walkable flag
            mNavMesh->setPolyFlags(polys[i], flags);
        }
        std::cout << "[PATHFINDER] Marked " << polyCount << " polygons as obstacles at (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }
}
