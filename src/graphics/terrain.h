#ifndef MCGNG_TERRAIN_H
#define MCGNG_TERRAIN_H

#include "graphics/renderer.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace mcgng {

/**
 * Terrain tile data.
 */
struct TerrainTile {
    uint16_t tileIndex = 0;     // Index into tileset
    uint8_t height = 0;         // Elevation level
    uint8_t flags = 0;          // Passability, cover, etc.

    // Flag bits
    static constexpr uint8_t FLAG_IMPASSABLE = 0x01;
    static constexpr uint8_t FLAG_WATER = 0x02;
    static constexpr uint8_t FLAG_FOREST = 0x04;
    static constexpr uint8_t FLAG_ROAD = 0x08;
    static constexpr uint8_t FLAG_BUILDING = 0x10;

    bool isPassable() const { return !(flags & FLAG_IMPASSABLE); }
    bool isWater() const { return flags & FLAG_WATER; }
    bool isForest() const { return flags & FLAG_FOREST; }
    bool isRoad() const { return flags & FLAG_ROAD; }
    bool isBuilding() const { return flags & FLAG_BUILDING; }
};

/**
 * Terrain tileset - collection of tile images.
 */
class TerrainTileset {
public:
    TerrainTileset() = default;
    ~TerrainTileset();

    /**
     * Load tileset from extracted tile data.
     * @param pixels Raw pixel data for all tiles
     * @param palette 256-color palette
     * @param tileWidth Width of each tile
     * @param tileHeight Height of each tile
     * @param tileCount Number of tiles
     * @return true on success
     */
    bool load(const uint8_t* pixels, const uint8_t* palette,
              int tileWidth, int tileHeight, int tileCount);

    /**
     * Get the texture for a tile.
     */
    TextureHandle getTileTexture(int index) const;

    /**
     * Get tile dimensions.
     */
    int getTileWidth() const { return m_tileWidth; }
    int getTileHeight() const { return m_tileHeight; }
    int getTileCount() const { return static_cast<int>(m_tiles.size()); }

private:
    std::vector<TextureHandle> m_tiles;
    int m_tileWidth = 0;
    int m_tileHeight = 0;
};

/**
 * Isometric terrain map renderer.
 *
 * MCG uses isometric projection with:
 * - 45-degree diamond tiles
 * - Multiple height levels
 * - Original resolutions: 45px or 90px tiles
 */
class TerrainMap {
public:
    TerrainMap() = default;
    ~TerrainMap() = default;

    /**
     * Load terrain map data.
     * @param tiles Tile data array
     * @param width Map width in tiles
     * @param height Map height in tiles
     * @return true on success
     */
    bool load(const TerrainTile* tiles, int width, int height);

    /**
     * Load from raw height and tile index arrays.
     */
    bool load(const uint16_t* tileIndices, const uint8_t* heights,
              const uint8_t* flags, int width, int height);

    /**
     * Set the tileset to use for rendering.
     */
    void setTileset(std::shared_ptr<TerrainTileset> tileset);

    /**
     * Render the visible portion of the terrain.
     * @param cameraX Camera X position (world units)
     * @param cameraY Camera Y position (world units)
     * @param viewWidth Viewport width in pixels
     * @param viewHeight Viewport height in pixels
     */
    void render(int cameraX, int cameraY, int viewWidth, int viewHeight);

    /**
     * Convert screen coordinates to tile coordinates.
     * @param screenX Screen X position
     * @param screenY Screen Y position
     * @param cameraX Camera X position
     * @param cameraY Camera Y position
     * @param outTileX Output tile X
     * @param outTileY Output tile Y
     */
    void screenToTile(int screenX, int screenY, int cameraX, int cameraY,
                      int& outTileX, int& outTileY) const;

    /**
     * Convert tile coordinates to screen coordinates.
     */
    void tileToScreen(int tileX, int tileY, int cameraX, int cameraY,
                      int& outScreenX, int& outScreenY) const;

    /**
     * Get tile at position.
     */
    const TerrainTile* getTile(int x, int y) const;
    TerrainTile* getTile(int x, int y);

    /**
     * Get map dimensions.
     */
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    /**
     * Check if a position is within map bounds.
     */
    bool isValidPosition(int x, int y) const {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }

    /**
     * Set tile size (45 or 90 pixels for MCG).
     */
    void setTileSize(int size) { m_tileSize = size; }
    int getTileSize() const { return m_tileSize; }

private:
    std::vector<TerrainTile> m_tiles;
    std::shared_ptr<TerrainTileset> m_tileset;
    int m_width = 0;
    int m_height = 0;
    int m_tileSize = 45;  // Default to 45-pixel tiles

    // Isometric projection helpers
    void worldToIso(int worldX, int worldY, int& isoX, int& isoY) const;
    void isoToWorld(int isoX, int isoY, int& worldX, int& worldY) const;
};

} // namespace mcgng

#endif // MCGNG_TERRAIN_H
