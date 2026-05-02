#include "app/main.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/core/drawing_options.h"
#include "app/definitions.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

	void HsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
		if (h < 0.f) {
			h += 360.f;
		}
		if (h >= 360.f) {
			h = std::fmod(h, 360.f);
		}
		const float c = v * s;
		const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
		const float m = v - c;
		float rp = 0.f, gp = 0.f, bp = 0.f;
		if (h < 60.f) {
			rp = c;
			gp = x;
		} else if (h < 120.f) {
			rp = x;
			gp = c;
		} else if (h < 180.f) {
			gp = c;
			bp = x;
		} else if (h < 240.f) {
			gp = x;
			bp = c;
		} else if (h < 300.f) {
			rp = x;
			bp = c;
		} else {
			rp = c;
			gp = x;
		}
		r = static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround((rp + m) * 255.f)), 0, 255));
		g = static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround((gp + m) * 255.f)), 0, 255));
		b = static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround((bp + m) * 255.f)), 0, 255));
	}

	// Golden-angle hue so nearby ids differ clearly; strong S/V for editor visibility.
	void ZoneIdToRgb(uint16_t id, uint8_t& r, uint8_t& g, uint8_t& b) {
		const float h = std::fmod(static_cast<float>(id) * 137.508f, 360.f);
		const float s = 0.80f;
		const float v = 0.58f;
		HsvToRgb(h, s, v, r, g, b);
	}

} // namespace

void TileColorCalculator::BlendGameplayZoneTint(uint8_t& r, uint8_t& g, uint8_t& b, const std::vector<uint16_t>& zoneIds) {
	if (zoneIds.empty()) {
		return;
	}

	std::vector<uint16_t> ids = zoneIds;
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

	const size_t n = ids.size();
	uint8_t tr = 255;
	uint8_t tg = 255;
	uint8_t tb = 255;

	if (n >= 3) {
		// Many overlaps → bright neutral (readable “collision” read)
		tr = 252;
		tg = 250;
		tb = 255;
	} else if (n == 2) {
		uint8_t r1 = 0;
		uint8_t g1 = 0;
		uint8_t b1 = 0;
		uint8_t r2 = 0;
		uint8_t g2 = 0;
		uint8_t b2 = 0;
		ZoneIdToRgb(ids[0], r1, g1, b1);
		ZoneIdToRgb(ids[1], r2, g2, b2);
		tr = static_cast<uint8_t>((static_cast<uint16_t>(r1) + static_cast<uint16_t>(r2)) / 2u);
		tg = static_cast<uint8_t>((static_cast<uint16_t>(g1) + static_cast<uint16_t>(g2)) / 2u);
		tb = static_cast<uint8_t>((static_cast<uint16_t>(b1) + static_cast<uint16_t>(b2)) / 2u);
	} else {
		ZoneIdToRgb(ids[0], tr, tg, tb);
	}

	// Blend toward tint so tile art stays visible (semi-transparent overlay effect).
	constexpr int kBlend = 118;
	r = static_cast<uint8_t>((static_cast<int>(r) * (255 - kBlend) + static_cast<int>(tr) * kBlend) / 255);
	g = static_cast<uint8_t>((static_cast<int>(g) * (255 - kBlend) + static_cast<int>(tg) * kBlend) / 255);
	b = static_cast<uint8_t>((static_cast<int>(b) * (255 - kBlend) + static_cast<int>(tb) * kBlend) / 255);
}

void TileColorCalculator::Calculate(const Tile* tile, const DrawingOptions& options, uint32_t current_house_id, int spawn_count, uint8_t& r, uint8_t& g, uint8_t& b) {
	bool showspecial = options.show_only_colors || options.show_special_tiles;

	if (options.show_blocking && tile->isBlocking() && tile->size() > 0) {
		// g * 2/3 approx g * 171 / 256
		g = (g * 171) >> 8;
		b = (b * 171) >> 8;
	}

	int item_count = tile->items.size();
	if (options.highlight_items && item_count > 0 && !tile->items.back()->isBorder()) {
		// Fixed point factors (x/256)
		// 0.75 -> 192, 0.6 -> 154, 0.48 -> 123, 0.40 -> 102, 0.33 -> 84
		static constexpr std::array<int, 5> factor = { 192, 154, 123, 102, 84 };
		int idx = std::clamp(item_count, 1, 5) - 1;
		g = (g * factor[idx]) >> 8;
		r = (r * factor[idx]) >> 8;
	}

	if (options.show_spawns && spawn_count > 0) {
		// Precomputed 0.7^n * 256 for n=1..9
		static constexpr std::array<int, 9> spawn_factor = { 179, 125, 88, 61, 43, 30, 21, 15, 10 };
		int f = spawn_factor[std::clamp(spawn_count, 1, 9) - 1];
		g = (g * f) >> 8;
		b = (b * f) >> 8;
	}

	if (options.show_houses && tile->isHouseTile()) {
		uint32_t house_id = tile->getHouseID();

		// Get unique house color
		uint8_t hr = 255, hg = 255, hb = 255;
		GetHouseColor(house_id, hr, hg, hb);

		// Apply the house unique color tint to the tile
		r = static_cast<uint8_t>((r * hr + r) >> 8);
		g = static_cast<uint8_t>((g * hg + g) >> 8);
		b = static_cast<uint8_t>((b * hb + b) >> 8);

		if (static_cast<int>(house_id) == current_house_id) {
			// Pulse Effect on top of the unique color
			// We want to make it pulse brighter/intense
			// options.highlight_pulse [0.0, 1.0]

			// Simple intensity boost
			// When pulse is high, we brighten the color towards white
			if (options.highlight_pulse > 0.0f) {
				float boost = options.highlight_pulse * 0.6f; // Max 60% boost towards white

				r = static_cast<uint8_t>(std::min(255, static_cast<int>(r + (255 - r) * boost)));
				g = static_cast<uint8_t>(std::min(255, static_cast<int>(g + (255 - g) * boost)));
				b = static_cast<uint8_t>(std::min(255, static_cast<int>(b + (255 - b) * boost)));
			}
		}
	} else if (showspecial && tile->isPZ()) {
		r >>= 1;
		b >>= 1;
	}

	if (showspecial && tile->getMapFlags() & TILESTATE_PVPZONE) {
		g = r >> 2;
		b = (b * 171) >> 8;
	}

	if (showspecial && tile->getMapFlags() & TILESTATE_NOLOGOUT) {
		b >>= 1;
	}

	if (showspecial && tile->getMapFlags() & TILESTATE_NOPVP) {
		g >>= 1;
	}

	if (options.show_zone_areas && (tile->getMapFlags() & TILESTATE_ZONE_BRUSH) && !tile->getZoneIds().empty()) {
		BlendGameplayZoneTint(r, g, b, tile->getZoneIds());
	}
}

void TileColorCalculator::GetHouseColor(uint32_t house_id, uint8_t& r, uint8_t& g, uint8_t& b) {
	static thread_local uint32_t cached_house_id = 0xFFFFFFFF;
	static thread_local uint8_t cached_r = 0, cached_g = 0, cached_b = 0;

	if (cached_house_id == house_id) {
		r = cached_r;
		g = cached_g;
		b = cached_b;
		return;
	}

	// Use a simple seeded random to get consistent colors
	// Simple hash
	// (Cache removed as calculation is faster than hash map lookup)
	uint32_t hash = house_id;
	hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
	hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
	hash = (hash >> 16) ^ hash;

	// Generate color components
	r = (hash & 0xFF);
	g = ((hash >> 8) & 0xFF);
	b = ((hash >> 16) & 0xFF);

	// Ensure colors aren't too dark (keep at least one channnel reasonably high)
	if (r < 50 && g < 50 && b < 50) {
		r += 100;
		g += 100;
		b += 100;
	}

	cached_house_id = house_id;
	cached_r = r;
	cached_g = g;
	cached_b = b;
}

void TileColorCalculator::GetMinimapColor(const Tile* tile, uint8_t& r, uint8_t& g, uint8_t& b) {
	// Optimization: Use lookup table to avoid division/modulo operations per tile
	static constexpr auto table = []() {
		struct {
			uint8_t r[256], g[256], b[256];
		} t {};
		for (int i = 0; i < 256; ++i) {
			t.r[i] = static_cast<uint8_t>(i / 36 % 6 * 51);
			t.g[i] = static_cast<uint8_t>(i / 6 % 6 * 51);
			t.b[i] = static_cast<uint8_t>(i % 6 * 51);
		}
		return t;
	}();

	uint8_t color = tile->getMiniMapColor();
	r = table.r[color];
	g = table.g[color];
	b = table.b[color];
}
