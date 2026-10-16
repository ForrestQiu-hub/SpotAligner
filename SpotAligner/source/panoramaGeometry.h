//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// Geometry of the panorama display: a semicircle with the flat side at the
// bottom, and a point that is dragged inside it.
//
// Kept free of any VSTGUI type on purpose, so that it can be checked by a
// headless test instead of by looking at the screen.
//
// Angle convention: 0 degrees is straight ahead (up), negative is to the
// left, positive to the right. The semicircle covers -90 .. +90 degrees.
//------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cmath>

namespace MyCompanyName {
namespace Panorama {

constexpr double kPi = 3.14159265358979323846;

//------------------------------------------------------------------------
/** The semicircle: flat side at the bottom, dome on top. */
struct Dome
{
	double centerX = 0.0;
	double centerY = 0.0;
	double radius = 1.0;
};

//------------------------------------------------------------------------
/** Fits the dome into a view rectangle, leaving @a margin free on every side
	so that the point and the outline are not clipped. */
inline Dome fitDome (double left, double top, double width, double height, double margin = 4.0)
{
	Dome dome;
	const double usableWidth = width - 2.0 * margin;
	const double usableHeight = height - 2.0 * margin;

	dome.radius = std::max (1.0, std::min (usableWidth / 2.0, usableHeight));
	dome.centerX = left + width / 2.0;
	dome.centerY = top + height - margin;
	return dome;
}

//------------------------------------------------------------------------
/** The screen position of an angle/distance pair.
	@param azimuthDegrees  0 straight ahead, negative left, positive right
	@param radiusFraction  0 at the centre, 1 on the dome */
inline void toPoint (const Dome& dome, double azimuthDegrees, double radiusFraction, double& x, double& y)
{
	const double radians = azimuthDegrees * kPi / 180.0;
	const double fraction = std::max (0.0, std::min (1.0, radiusFraction));
	const double r = dome.radius * fraction;

	x = dome.centerX + std::sin (radians) * r;
	y = dome.centerY - std::cos (radians) * r;
}

//------------------------------------------------------------------------
/** The inverse of toPoint. */
inline void toPolar (const Dome& dome, double x, double y, double& azimuthDegrees, double& radiusFraction)
{
	const double dx = x - dome.centerX;
	const double dy = dome.centerY - y; // up is positive

	const double r = std::sqrt (dx * dx + dy * dy);
	radiusFraction = dome.radius > 0.0 ? r / dome.radius : 0.0;
	azimuthDegrees = std::atan2 (dx, dy) * 180.0 / kPi;
}

//------------------------------------------------------------------------
/** Keeps a point on or above the flat side and inside the radius. */
inline void clampToDome (const Dome& dome, double& x, double& y)
{
	double dx = x - dome.centerX;
	double dy = dome.centerY - y;

	if (dy < 0.0)
		dy = 0.0; // never below the flat side

	const double r = std::sqrt (dx * dx + dy * dy);
	if (r > dome.radius && r > 0.0)
	{
		const double scale = dome.radius / r;
		dx *= scale;
		dy *= scale;
	}

	x = dome.centerX + dx;
	y = dome.centerY - dy;
}

//------------------------------------------------------------------------
inline double clamp01 (double value)
{
	return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

//------------------------------------------------------------------------
/** Maps a distance onto the radius of the display.

	The first @a nearMeters metres get @a nearShare of the radius, the rest
	takes what is left. Objects usually stand within a few metres of the array,
	so the near field is stretched and the far field compressed: with a share
	of 0.4 the first two of eighteen metres take 40 % of the radius instead of
	the 11 % a linear mapping would give them.

	@param meters     distance to place
	@param maxMeters  distance that sits on the dome
	@param nearMeters distance that gets the stretched part (0 disables it)
	@param nearShare  share of the radius that part gets (0 .. 1) */
inline double distanceToRadius (double meters, double maxMeters, double nearMeters, double nearShare)
{
	if (meters <= 0.0 || maxMeters <= 0.0)
		return 0.0;
	if (meters >= maxMeters)
		return 1.0;
	if (nearMeters <= 0.0 || nearMeters >= maxMeters)
		return meters / maxMeters;

	nearShare = clamp01 (nearShare);
	if (meters <= nearMeters)
		return nearShare * meters / nearMeters;

	return nearShare + (1.0 - nearShare) * (meters - nearMeters) / (maxMeters - nearMeters);
}

/** The inverse of distanceToRadius. */
inline double radiusToDistance (double fraction, double maxMeters, double nearMeters, double nearShare)
{
	fraction = clamp01 (fraction);

	if (maxMeters <= 0.0)
		return 0.0;
	if (nearMeters <= 0.0 || nearMeters >= maxMeters)
		return fraction * maxMeters;

	nearShare = clamp01 (nearShare);
	if (nearShare <= 0.0 || nearShare >= 1.0)
		return fraction * maxMeters;

	if (fraction <= nearShare)
		return nearMeters * fraction / nearShare;

	return nearMeters + (maxMeters - nearMeters) * (fraction - nearShare) / (1.0 - nearShare);
}

} // namespace Panorama
} // namespace MyCompanyName
