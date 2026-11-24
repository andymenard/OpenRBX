#ifndef APPDRAW_ADORNG3D_H
#define APPDRAW_ADORNG3D_H

#include "rbxgraphics/Adorn.h"

#include <GLG3D/RenderDevice.h>
#include <GLG3D/TextureManager.h>

namespace RBX {

class AdornG3D : public Adorn
{
private:
	G3D::RenderDevice* renderDevice;
	G3D::TextureManager textureManager;
	// Render-device pixels per GUI unit. The 3D scene renders at the device's full resolution while
	// the 2D GUI and input stay in the widget units the client lays them out in.
	float uiScale = 1.0f;

public:
	AdornG3D(G3D::RenderDevice* renderDevice) : renderDevice(renderDevice) {}

	G3D::RenderDevice* getRenderDevice() { return renderDevice; }
	void setUiScale(float scale) { uiScale = scale; }

	G3D::ReferenceCountedPointer<TextureProxyBase> createTextureProxy(const std::string& filename) override;

	G3D::Rect2D getViewport() const override;
	int getWidth() const override;
	int getHeight() const override;

	void setTexture(int id, G3D::ReferenceCountedPointer<TextureProxyBase> _texture) override;
	G3D::Rect2D getTextureSize(G3D::ReferenceCountedPointer<TextureProxyBase> _texture) const override;

	void line2d(const G3D::Vector2& p0, const G3D::Vector2& p1, const G3D::Color4& color) const override;
	void outlineRect2d(const G3D::Rect2D& rect, float thick, const G3D::Color4& color) const override;
	void rect2d(const G3D::Rect2D& rect, const G3D::Color4& color) const override;

	G3D::Vector2 get2DStringBounds(const std::string& s, double size, Spacing spacing) const override;
	G3D::Vector2 drawFont2D(
		const std::string& s,
		const G3D::Vector2& pos2d,
		double size,
		const G3D::Color4& color,
		const G3D::Color4& outline,
		XAlign xalign,
		YAlign yalign,
		Spacing spacing
	) const override;

	void setObjectToWorldMatrix(const G3D::CoordinateFrame& c) override;
	void setColor(const G3D::Color4& color) override;

	void box(const G3D::AABox& box, const G3D::Color4& solidColor, const G3D::Color4& wireColor) override;
	void sphere(const G3D::Sphere& sphere, const G3D::Color4& solidColor, const G3D::Color4& wireColor) override;
	void explosion(const G3D::Sphere& sph, const G3D::Color4& solidColor, const G3D::Color4& wireColor) override;

	void cylinderAlongX(
		float radius, float length, const G3D::Color4& solidColor, const G3D::Color4& wireColor
	) override;

	void ray(const G3D::Ray& ray, const G3D::Color4& color, float scale) override;
	void lineSegment(const G3D::LineSegment& lineSegment, const G3D::Color4& color, float scale) override;
	void axes(const G3D::Color4& xColor, const G3D::Color4& yColor, const G3D::Color4& zColor, float scale) override;

	void quad(
		const G3D::Vector3& v0,
		const G3D::Vector3& v1,
		const G3D::Vector3& v2,
		const G3D::Vector3& v3,
		const G3D::Color4& color
	) override;
};

} // namespace RBX

#endif // APPDRAW_ADORNG3D_H
