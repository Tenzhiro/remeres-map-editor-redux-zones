//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "brushes/flag/flag_brush.h"

#include "map/map.h"
#include "map/tile.h"
#include "game/sprites.h"

#include <array>
#include <string_view>
#include <algorithm>

FlagBrush::FlagBrush(uint32_t flag) :
	flag(flag),
	zoneId(0) {
	//
}

std::string FlagBrush::getName() const {
	static constexpr std::array<std::pair<uint32_t, std::string_view>, 5> flagNames = { { { TILESTATE_PROTECTIONZONE, "PZ brush (0x01)" },
																						  { TILESTATE_NOPVP, "No combat zone brush (0x04)" },
																						  { TILESTATE_NOLOGOUT, "No logout zone brush (0x08)" },
																						  { TILESTATE_PVPZONE, "PVP Zone brush (0x10)" },
																						  { TILESTATE_ZONE_BRUSH, "Zone brush (0x40)" } } };

	auto it = std::ranges::find_if(flagNames, [this](const auto& p) { return p.first == flag; });
	if (it != flagNames.end()) {
		return std::string(it->second);
	}
	return "Unknown flag brush";
}

int FlagBrush::getLookID() const {
	static constexpr std::array<std::pair<uint32_t, int>, 5> flagSprites = { { { TILESTATE_PROTECTIONZONE, EDITOR_SPRITE_PZ_TOOL },
																			   { TILESTATE_NOPVP, EDITOR_SPRITE_NOPVP_TOOL },
																			   { TILESTATE_NOLOGOUT, EDITOR_SPRITE_NOLOG_TOOL },
																			   { TILESTATE_PVPZONE, EDITOR_SPRITE_PVPZ_TOOL },
																			   { TILESTATE_ZONE_BRUSH, EDITOR_SPRITE_ZONE_TOOL } } };

	auto it = std::ranges::find_if(flagSprites, [this](const auto& p) { return p.first == flag; });
	if (it != flagSprites.end()) {
		return it->second;
	}
	return 0;
}

bool FlagBrush::canDraw(BaseMap* map, const Position& position) const {
	if (Tile* tile = map->getTile(position)) {
		return tile->hasGround();
	}
	return false;
}

void FlagBrush::undraw(BaseMap* /*map*/, Tile* tile) {
	if (flag & TILESTATE_ZONE_BRUSH) {
		if (zoneId == 0) {
			tile->unsetMapFlags(flag);
			tile->clearZoneId();
		} else {
			tile->removeZoneId(zoneId);
			if (tile->getZoneIds().empty()) {
				tile->unsetMapFlags(flag);
			}
		}
	} else {
		tile->unsetMapFlags(flag);
	}
}

void FlagBrush::draw(BaseMap* /*map*/, Tile* tile, void* /*parameter*/) {
	if (!tile->hasGround()) {
		return;
	}
	if (flag & TILESTATE_ZONE_BRUSH) {
		if (zoneId == 0) {
			// Use Ctrl + zone brush to remove IDs; do not clear on normal click (avoids accidents).
			return;
		}
		tile->setMapFlags(flag);
		tile->addZoneId(zoneId);
	} else {
		tile->setMapFlags(flag);
	}
}
