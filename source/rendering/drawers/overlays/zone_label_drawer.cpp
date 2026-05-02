//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "rendering/drawers/overlays/zone_label_drawer.h"
#include "editor/editor.h"
#include "map/map.h"
#include "map/position.h"
#include "map/tile.h"

#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"

#include <nanovg.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <queue>
#include <unordered_set>
#include <vector>

namespace {

	bool tileHasZoneId(Tile* tile, uint16_t zoneId) {
		if (!tile || !(tile->getMapFlags() & TILESTATE_ZONE_BRUSH)) {
			return false;
		}
		for (uint16_t id : tile->getZoneIds()) {
			if (id == zoneId) {
				return true;
			}
		}
		return false;
	}

	uint64_t packXY(int x, int y) {
		return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
	}

} // namespace

void ZoneLabelDrawer::draw(NVGcontext* vg, const RenderView& view, Editor& editor, const DrawingOptions& options, const ViewBounds& bounds) {
	if (!vg || !options.show_zone_areas || options.ingame) {
		return;
	}

	const int z = view.camera_pos.z;
	float zoom = view.zoom;
	float tileSizeScreen = 32.0f / zoom;
	float fontSize = 11.0f;

	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

	std::unordered_set<uint16_t> zoneIdsPresent;
	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
			Tile* tile = editor.map.getTile(Position(x, y, z));
			if (!tile || !(tile->getMapFlags() & TILESTATE_ZONE_BRUSH)) {
				continue;
			}
			for (uint16_t id : tile->getZoneIds()) {
				zoneIdsPresent.insert(id);
			}
		}
	}

	for (uint16_t zid : zoneIdsPresent) {
		std::unordered_set<uint64_t> unvisited;
		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				Tile* tile = editor.map.getTile(Position(x, y, z));
				if (tileHasZoneId(tile, zid)) {
					unvisited.insert(packXY(x, y));
				}
			}
		}

		while (!unvisited.empty()) {
			const uint64_t seed = *unvisited.begin();
			const int sx = static_cast<int>(static_cast<uint32_t>(seed >> 32));
			const int sy = static_cast<int>(static_cast<uint32_t>(seed & 0xFFFFFFFFu));

			// Flood-fill the *entire* 4-connected component on this floor (not clipped to the
			// view). If we only clustered visible tiles, the centroid jumped whenever the user panned.
			std::vector<std::pair<int, int>> component;
			std::queue<std::pair<int, int>> q;
			std::unordered_set<uint64_t> bfs_visited;
			q.push({ sx, sy });
			bfs_visited.insert(packXY(sx, sy));

			static constexpr int kDx[4] = { 0, 0, 1, -1 };
			static constexpr int kDy[4] = { 1, -1, 0, 0 };

			while (!q.empty()) {
				const auto [cx, cy] = q.front();
				q.pop();
				component.push_back({ cx, cy });

				for (int i = 0; i < 4; ++i) {
					const int nx = cx + kDx[i];
					const int ny = cy + kDy[i];
					Position np(nx, ny, z);
					if (!np.isValid()) {
						continue;
					}
					const uint64_t nk = packXY(nx, ny);
					if (bfs_visited.count(nk) != 0) {
						continue;
					}
					Tile* nt = editor.map.getTile(np);
					if (!tileHasZoneId(nt, zid)) {
						continue;
					}
					bfs_visited.insert(nk);
					q.push({ nx, ny });
				}
			}

			for (const auto& p : component) {
				unvisited.erase(packXY(p.first, p.second));
			}

			if (component.empty()) {
				continue;
			}

			// Anchor to an actual zoned tile. The bbox center (min+max)/2 can lie outside the
			// component on L-shaped or disjoint-looking shapes, so labels appeared over non-zone tiles.
			double sumx = 0.0;
			double sumy = 0.0;
			for (const auto& p : component) {
				sumx += static_cast<double>(p.first) + 0.5;
				sumy += static_cast<double>(p.second) + 0.5;
			}
			const double n = static_cast<double>(component.size());
			const double centx = sumx / n;
			const double centy = sumy / n;

			int anchor_x = component[0].first;
			int anchor_y = component[0].second;
			double best_d2 = 1.0e100;
			for (const auto& p : component) {
				const double tx = static_cast<double>(p.first) + 0.5;
				const double ty = static_cast<double>(p.second) + 0.5;
				const double dx = tx - centx;
				const double dy = ty - centy;
				const double d2 = dx * dx + dy * dy;
				if (d2 < best_d2 - 1e-12) {
					best_d2 = d2;
					anchor_x = p.first;
					anchor_y = p.second;
				} else if (std::abs(d2 - best_d2) <= 1e-12 && (p.first < anchor_x || (p.first == anchor_x && p.second < anchor_y))) {
					// Stable tie-break (symmetric blobs).
					anchor_x = p.first;
					anchor_y = p.second;
				}
			}

			int unscaled_x = 0;
			int unscaled_y = 0;
			view.getScreenPosition(anchor_x, anchor_y, z, unscaled_x, unscaled_y);

			float screen_x = static_cast<float>(unscaled_x) / zoom;
			float screen_y = static_cast<float>(unscaled_y) / zoom;
			float labelX = screen_x + tileSizeScreen / 2.0f;
			float labelY = screen_y + tileSizeScreen / 2.0f;

			std::string text;
			Tile* anchorTile = editor.map.getTile(Position(anchor_x, anchor_y, z));
			if (anchorTile && anchorTile->getZoneIds().size() > 1) {
				std::vector<uint16_t> ids = anchorTile->getZoneIds();
				std::sort(ids.begin(), ids.end());
				ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
				text = "zone id: ";
				for (size_t i = 0; i < ids.size(); ++i) {
					if (i > 0) {
						text += '/';
					}
					text += std::format("{}", ids[i]);
				}
			} else {
				text = std::format("zone id: {}", zid);
			}

			float textBounds[4];
			nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, textBounds);
			float textWidth = textBounds[2] - textBounds[0];
			float textHeight = textBounds[3] - textBounds[1];

			const float paddingX = 5.0f;
			const float paddingY = 3.0f;
			const float radius = 4.0f;
			const float tailW = 10.0f;
			const float tailH = 6.0f;

			const float bubbleLeft = labelX - textWidth / 2.0f - paddingX;
			const float bubbleTop = labelY - textHeight / 2.0f - paddingY;
			const float bubbleW = textWidth + paddingX * 2.0f;
			const float bubbleH = textHeight + paddingY * 2.0f;
			const float bubbleBottom = bubbleTop + bubbleH;

			nvgBeginPath(vg);
			nvgRoundedRect(vg, bubbleLeft, bubbleTop, bubbleW, bubbleH, radius);
			nvgFillColor(vg, nvgRGBA(255, 255, 255, 230));
			nvgFill(vg);

			nvgBeginPath(vg);
			const float tailCx = labelX;
			nvgMoveTo(vg, tailCx - tailW / 2.0f, bubbleBottom);
			nvgLineTo(vg, tailCx + tailW / 2.0f, bubbleBottom);
			nvgLineTo(vg, tailCx, bubbleBottom + tailH);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(255, 255, 255, 230));
			nvgFill(vg);

			nvgFillColor(vg, nvgRGBA(20, 20, 20, 255));
			nvgText(vg, labelX, labelY, text.c_str(), nullptr);
		}
	}
}
