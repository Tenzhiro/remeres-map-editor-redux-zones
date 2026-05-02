//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_DRAWERS_OVERLAYS_ZONE_LABEL_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_ZONE_LABEL_DRAWER_H_

#include "rendering/core/render_view.h"

struct NVGcontext;
class Editor;

#include "rendering/core/drawing_options.h"

class ZoneLabelDrawer {
public:
	ZoneLabelDrawer() = default;
	~ZoneLabelDrawer() = default;

	void draw(NVGcontext* vg, const RenderView& view, Editor& editor, const DrawingOptions& options, const ViewBounds& bounds);
};

#endif
