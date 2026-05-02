#ifndef RME_RENDERING_TILE_COLOR_CALCULATOR_H_
#define RME_RENDERING_TILE_COLOR_CALCULATOR_H_

#include <cstdint>
#include <vector>

class Tile;
struct DrawingOptions;

class TileColorCalculator {
public:
	static void Calculate(const Tile* tile, const DrawingOptions& options, uint32_t current_house_id, int spawn_count, uint8_t& r, uint8_t& g, uint8_t& b);
	static void GetHouseColor(uint32_t house_id, uint8_t& r, uint8_t& g, uint8_t& b);
	static void GetMinimapColor(const Tile* tile, uint8_t& r, uint8_t& g, uint8_t& b);

	/** Blends r,g,b (tile color multipliers) toward distinct per-zone-id hues; 2 ids → mix; 3+ distinct ids → light neutral. */
	static void BlendGameplayZoneTint(uint8_t& r, uint8_t& g, uint8_t& b, const std::vector<uint16_t>& zoneIds);
};

#endif
