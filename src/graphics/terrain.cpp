#include "graphics/terrain.h"
#include <algorithm>
#include <cmath>

namespace mcgng {

// TerrainTileset implementation

TerrainTileset::~TerrainTileset() {
    auto& renderer = Renderer::instance();
    for (auto texture : m_tiles) {
        if (texture != INVALID_TEXTURE) {
            renderer.destroyTexture(texture);
        }
    }
}

bool TerrainTileset::load(const uint8_t* pixels, const uint8_t* palette,
                          int tileWidth, int tileHeight, int tileCount) {
    if (!pixels || !palette || tileWidth <= 0 || tileHeight <= 0 || tileCount <= 0) {
        return false;
    }

    // Clear existing tiles
    auto& renderer = Renderer::instance();
    for (auto texture : m_tiles) {
        if (texture != INVALID_TEXTURE) {
            renderer.destroyTexture(texture);
        }
    }
    m_tiles.clear();

    m_tileWidth = tileWidth;
    m_tileHeight = tileHeight;
    m_tiles.reserve(tileCount);

    // Create texture for each tile
    int tilePixelSize = tileWidth * tileHeight;

    for (int i = 0; i < tileCount; ++i) {
        const uint8_t* tilePixels = pixels + (i * tilePixelSize);
        TextureHandle texture = renderer.createTextureIndexed(tilePixels, palette,
                                                               tileWidth, tileHeight);
        m_tiles.push_back(texture);
    }

    return true;
}

TextureHandle TerrainTileset::getTileTexture(int index) const {
    if (index < 0 || index >= static_cast<int>(m_tiles.size())) {
        return INVALID_TEXTURE;
    }
    return m_tiles[index];
}

// TerrainMap implementation

bool TerrainMap::load(const TerrainTile* tiles, int width, int height) {
    if (!tiles || width <= 0 || height <= 0) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_tiles.assign(tiles, tiles + (width * height));
    return true;
}

bool TerrainMap::load(const uint16_t* tileIndices, const uint8_t* heights,
                      const uint8_t* flags, int width, int height) {
    if (!tileIndices || width <= 0 || height <= 0) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_tiles.resize(width * height);

    for (int i = 0; i < width * height; ++i) {
        m_tiles[i].tileIndex = tileIndices[i];
        m_tiles[i].height = heights ? heights[i] : 0;
        m_tiles[i].flags = flags ? flags[i] : 0;
    }

    return true;
}

void TerrainMap::setTileset(std::shared_ptr<TerrainTileset> tileset) {
    m_tileset = std::move(tileset);
}

const TerrainTile* TerrainMap::getTile(int x, int y) const {
    if (!isValidPosition(x, y)) {
        return nullptr;
    }
    return &m_tiles[y * m_width + x];
}

TerrainTile* TerrainMap::getTile(int x, int y) {
    if (!isValidPosition(x, y)) {
        return nullptr;
    }
    return &m_tiles[y * m_width + x];
}

void TerrainMap::worldToIso(int worldX, int worldY, int& isoX, int& isoY) const {
    // Convert cartesian to isometric
    // For a standard 2:1 isometric projection
    int halfTile = m_tileSize / 2;
    isoX = (worldX - worldY) * halfTile;
    isoY = (worldX + worldY) * halfTile / 2;
}

void TerrainMap::isoToWorld(int isoX, int isoY, int& worldX, int& worldY) const {
    // Convert isometric to cartesian
    int halfTile = m_tileSize / 2;
    if (halfTile == 0) halfTile = 1;  // Avoid division by zero

    worldX = (isoX / halfTile + isoY * 2 / halfTile) / 2;
    worldY = (isoY * 2 / halfTile - isoX / halfTile) / 2;
}

void TerrainMap::screenToTile(int screenX, int screenY, int cameraX, int cameraY,
                              int& outTileX, int& outTileY) const {
    // Convert screen position to world position
    int worldX = screenX + cameraX;
    int worldY = screenY + cameraY;

    // Convert to tile coordinates
    isoToWorld(worldX, worldY, outTileX, outTileY);
}

void TerrainMap::tileToScreen(int tileX, int tileY, int cameraX, int cameraY,
                              int& outScreenX, int& outScreenY) const {
    int isoX, isoY;
    worldToIso(tileX, tileY, isoX, isoY);

    outScreenX = isoX - cameraX;
    outScreenY = isoY - cameraY;
}

void TerrainMap::render(int cameraX, int cameraY, int viewWidth, int viewHeight) {
    if (m_tiles.empty() || !m_tileset) {
        return;
    }

    auto& renderer = Renderer::instance();

    // Calculate visible tile range
    // For isometric rendering, we need to account for the diamond shape
    int halfTile = m_tileSize / 2;
    int quarterTile = m_tileSize / 4;

    // Expand view bounds to account for partially visible tiles
    int margin = 2;

    // Convert camera bounds to tile coordinates
    int startTileX, startTileY, endTileX, endTileY;

    // Get corners of view in world space and convert to tile coords
    // This is a simplified version - full implementation would be more accurate
    screenToTile(0, 0, cameraX, cameraY, startTileX, startTileY);
    screenToTile(viewWidth, viewHeight, cameraX, cameraY, endTileX, endTileY);

    // Add margin
    startTileX = std::max(0, startTileX - margin);
    startTileY = std::max(0, startTileY - margin);
    endTileX = std::min(m_width - 1, endTileX + margin);
    endTileY = std::min(m_height - 1, endTileY + margin);

    // Render tiles in back-to-front order (painter's algorithm)
    // For isometric, we render diagonally
    for (int row = startTileY; row <= endTileY; ++row) {
        for (int col = startTileX; col <= endTileX; ++col) {
            const TerrainTile* tile = getTile(col, row);
            if (!tile) continue;

            TextureHandle texture = m_tileset->getTileTexture(tile->tileIndex);
            if (texture == INVALID_TEXTURE) continue;

            // Calculate screen position
            int screenX, screenY;
            tileToScreen(col, row, cameraX, cameraY, screenX, screenY);

            // Adjust for height
            screenY -= tile->height * quarterTile;

            // Draw the tile
            renderer.drawTexture(texture, screenX, screenY);
        }
    }
}

} // namespace mcgng
