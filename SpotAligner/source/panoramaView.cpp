//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// The panorama display: a semicircle with a draggable point.
//
// The point and the spot microphone angle are two views of the same value:
//
//   * the angle parameter moves the point (initial state, automation, or the
//     angle field next to it),
//   * dragging the point writes the angle parameter, exactly the way a user
//     edit does: beginEdit, setValueNormalized, valueChanged, endEdit. That
//     is what tells the host about the edit, so automation is recorded the
//     same way as when the field is typed into.
//
// The display covers the front half of the turn, -90 degrees hard left to
// +90 degrees hard right; the parameter's full -180 .. +180 is mapped onto it
// and clamped, so an object placed behind the array stays at the edge.
//
// The distance the point has from the centre is not connected to anything
// yet.
//
// Every coordinate conversion lives in panoramaGeometry.h, which has no
// VSTGUI dependency and is checked by a headless test. State is applied from
// the idle callback, never from inside a paint, because a repaint requested
// during a paint is dropped by the frame.
//
// Registered as "PanoramaView" and used from editor.uidesc.
//------------------------------------------------------------------------

#include "panoramaGeometry.h"
#include "parameters.h"

#include "vstgui/lib/cbuttonstate.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/uidescription/iviewcreator.h"
#include "vstgui/uidescription/uiviewfactory.h"

#include <cmath>

namespace MyCompanyName {
namespace {

using namespace VSTGUI;

// Mirrors the colours of editor.uidesc.
const CColor kBackground (0x15, 0x18, 0x1C, 0xFF);
const CColor kGuide (0x2E, 0x32, 0x38, 0xFF);
const CColor kDome (0x4A, 0x50, 0x58, 0xFF);
const CColor kNearFieldArc (0x7A, 0x84, 0x90, 0xFF);
const CColor kDot (0xE8, 0xC0, 0x5A, 0xFF);
const CColor kDotEdge (0x23, 0x26, 0x2B, 0xFF);

/** How fine the dome outline is drawn. */
constexpr int32_t kDomeSegments = 60;
constexpr double kDotRadius = 4.5;

/** The display covers the front half of the turn. */
constexpr double kHalfAngleDegrees = 90.0;

/** Smallest visible distance from the centre. */
constexpr double kMinRadiusFraction = 0.08;

/** Below this the value counts as unchanged, so a drag does not spam the
	parameter with writes that make no difference. */
constexpr double kEpsilon = 1.0e-6;

//------------------------------------------------------------------------
/** Recursively finds the control with the given tag. */
CControl* findControl (CView* view, int32_t tag)
{
	if (!view)
		return nullptr;

	if (auto* control = dynamic_cast<CControl*> (view))
	{
		if (control->getTag () == tag)
			return control;
	}

	if (auto* container = dynamic_cast<CViewContainer*> (view))
	{
		for (uint32_t i = 0; i < container->getNbViews (); ++i)
		{
			if (auto* found = findControl (container->getView (i), tag))
				return found;
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------
class PanoramaView : public CView
{
public:
	explicit PanoramaView (const CRect& size) : CView (size) {}

	~PanoramaView () override { setWantsIdle (false); }

	void draw (CDrawContext* context) override
	{
		if (!idleRequested && isAttached ())
		{
			idleRequested = true;
			setWantsIdle (true);
		}

		// Read the parameters once more right here, before anything is drawn. The
		// idle callback does this too, but on the first paint after a window is
		// reopened it may not have run yet, and then the point would be drawn
		// where the view was constructed - the centre - until something asked
		// for another repaint. Doing it here cannot fail: no repaint is asked
		// for during a paint, the value is simply already right.
		followParameters ();

		drawDisplay (context);
	}

	void onIdle () override
	{
		if (followParameters ())
			refresh ();
	}

	/** Asks for a repaint, ours and the whole editor.

	    Calling invalid() on itself is not enough: the frame draws its children
	    into a cached composition of its own, and a view that only marks itself
	    dirty can go on showing what it drew last. The other three views of this
	    editor invalidate the frame as well, and they repaint correctly; this one
	    did not, which is why the point never appeared to move when a distance or
	    an angle was typed, and why it sat where the very first paint had put it
	    after the window was reopened. */
	void refresh ()
	{
		invalid ();

		if (auto* frame = getFrame ())
			frame->invalid ();
	}

	CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override
	{
		if (!(buttons & kLButton))
			return kMouseEventNotHandled;

		dragging = true;
		dragTo (where);
		return kMouseEventHandled;
	}

	CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override
	{
		if (!dragging)
			return kMouseEventNotHandled;

		dragTo (where);
		return kMouseEventHandled;
	}

	CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override
	{
		if (!dragging)
			return kMouseEventNotHandled;

		dragTo (where);
		dragging = false;
		return kMouseEventHandled;
	}

	CMouseEventResult onMouseCancel () override
	{
		dragging = false;
		return kMouseEventHandled;
	}

	CLASS_METHODS (PanoramaView, CView)

private:
	Panorama::Dome currentDome () const
	{
		const CRect size = getViewSize ();
		return Panorama::fitDome (size.left, size.top, size.getWidth (), size.getHeight ());
	}

	//--------------------------------------------------------------------
	void drawDisplay (CDrawContext* context)
	{
		const CRect size = getViewSize ();
		const Panorama::Dome dome = currentDome ();

		context->setFillColor (kBackground);
		context->drawRect (size, kDrawFilled);

		// frame, so that the display reads as its own area
		CRect frame (size);
		frame.inset (1.0, 1.0);
		context->setFrameColor (kDome);
		context->setLineWidth (1.0);
		context->drawRect (frame, kDrawStroked);

		// guides from the centre
		context->setFrameColor (kGuide);
		for (double azimuth = -60.0; azimuth <= 60.0; azimuth += 30.0)
		{
			double x = 0.0;
			double y = 0.0;
			Panorama::toPoint (dome, azimuth, 1.0, x, y);
			context->drawLine (CPoint (dome.centerX, dome.centerY), CPoint (x, y));
		}

		// the dome, as a polyline
		context->setFrameColor (kDome);
		context->setLineWidth (1.5);
		drawArc (context, dome, 1.0);

		// where the stretched near field ends, so that the scale is readable
		context->setFrameColor (kNearFieldArc);
		context->setLineWidth (1.0);
		drawArc (context, dome, nearFieldFraction ());

		// the flat side
		context->drawLine (CPoint (dome.centerX - dome.radius, dome.centerY),
		                   CPoint (dome.centerX + dome.radius, dome.centerY));

		// A spoke through the centre at the angle of the point, and a ring at
		// the distance it stands for. Without them the point alone says very
		// little: the near field stretch puts the distances one actually works
		// at within a few pixels of the centre, so a typed angle or distance
		// moved it by almost nothing and looked as if it had not been applied
		// at all. The mappings were always there; it was the reading that was
		// impossible.
		{
			double spokeX = 0.0;
			double spokeY = 0.0;
			Panorama::toPoint (dome, azimuthDegrees, 1.0, spokeX, spokeY);

			context->setFrameColor (CColor (0xE8, 0xC0, 0x5A, 0x50));
			context->setLineWidth (3.0);
			context->drawLine (CPoint (dome.centerX, dome.centerY), CPoint (spokeX, spokeY));

			if (radiusFraction > 0.12)
			{
				context->setFrameColor (CColor (0xE8, 0xC0, 0x5A, 0x30));
				context->setLineWidth (1.0);
				drawArc (context, dome, radiusFraction);
			}
		}

		// the point
		double x = 0.0;
		double y = 0.0;
		Panorama::toPoint (dome, azimuthDegrees, radiusFraction, x, y);

		context->setFillColor (kDotEdge);
		context->drawEllipse (
		    CRect (x - kDotRadius - 1.0, y - kDotRadius - 1.0, x + kDotRadius + 1.0, y + kDotRadius + 1.0),
		    kDrawFilled);

		context->setFillColor (kDot);
		context->drawEllipse (CRect (x - kDotRadius, y - kDotRadius, x + kDotRadius, y + kDotRadius), kDrawFilled);
	}

	/** One arc of the dome, at the given share of the radius. */
	static void drawArc (CDrawContext* context, const Panorama::Dome& dome, double fraction)
	{
		double previousX = 0.0;
		double previousY = 0.0;

		for (int32_t i = 0; i <= kDomeSegments; ++i)
		{
			const double azimuth = -kHalfAngleDegrees + 2.0 * kHalfAngleDegrees * i / kDomeSegments;
			double x = 0.0;
			double y = 0.0;
			Panorama::toPoint (dome, azimuth, fraction, x, y);

			if (i > 0)
				context->drawLine (CPoint (previousX, previousY), CPoint (x, y));

			previousX = x;
			previousY = y;
		}
	}

	/** Where the stretched near field ends, as a share of the radius. */
	static double nearFieldFraction ()
	{
		return Panorama::distanceToRadius (kNearFieldMeters, kMaxDistanceMeters, kNearFieldMeters,
		                                   kNearFieldRadiusShare);
	}

	//--------------------------------------------------------------------
	/** Finds the control that carries the spot microphone angle.

	    It is looked up in this view first, then in the whole editor, so that
	    moving it in editor.uidesc cannot silently disconnect this display. */
	CControl* findAngleControl ()
	{
		return findTaggedControl (static_cast<int32_t> (kParamSpotAngle));
	}

	/** The control that carries the distance, which is what the distance of
	    the point from the centre stands for. */
	CControl* findDistanceControl ()
	{
		return findTaggedControl (static_cast<int32_t> (kParamDistance));
	}

	CControl* findTaggedControl (int32_t tag)
	{
		if (auto* control = findControl (this, tag))
			return control;

		if (auto* frame = getFrame ())
			return findControl (frame, tag);

		return nullptr;
	}

	/** Moves the point to where the parameters say it belongs. Returns true
	    when it moved. */
	bool followParameters ()
	{
		// Looked up again on every idle, never cached. Closing the window and
		// opening it again builds the editor a second time, and the controls of
		// the first one are gone: a cached pointer keeps answering with the
		// value it was created with - zero - and the point fell back to the
		// centre of the dome while the parameters were perfectly fine.
		angleControl = findAngleControl ();
		distanceControl = findDistanceControl ();

		const double degrees = readoutNumber (static_cast<int32_t> (kParamSpotAzimuth), azimuthDegrees);
		const double meters = readoutNumber (static_cast<int32_t> (kParamSpotDistance),
		                                     Panorama::radiusToDistance (radiusFraction, kMaxDistanceMeters,
		                                                                 kNearFieldMeters, kNearFieldRadiusShare));

		// while the point is being dragged it is the one writing the
		// parameters, so it must not be pulled back by them in between
		if (dragging)
			return false;

		bool moved = false;

		const double clamped = clampAngle (degrees);
		if (std::fabs (clamped - azimuthDegrees) >= kEpsilon)
		{
			azimuthDegrees = clamped;
			moved = true;
		}

		double fraction = distanceToRadius (meters);
		if (fraction < kMinRadiusFraction)
			fraction = kMinRadiusFraction;

		if (std::fabs (fraction - radiusFraction) >= kEpsilon)
		{
			radiusFraction = fraction;
			moved = true;
		}

		if (moved)
			writeFollowDiagnostics (true, azimuthDegrees, radiusFraction);

		return moved;
	}

	/** Moves the point to the mouse and writes the parameters it stands for. */
	void dragTo (const CPoint& where)
	{
		const Panorama::Dome dome = currentDome ();
		const CPoint domePoint = resolveMouseSpace (where);

		double x = domePoint.x;
		double y = domePoint.y;
		Panorama::clampToDome (dome, x, y);
		Panorama::toPolar (dome, x, y, azimuthDegrees, radiusFraction);

		// keep the point visible when it sits on the centre
		if (radiusFraction < kMinRadiusFraction)
			radiusFraction = kMinRadiusFraction;

		azimuthDegrees = clampAngle (azimuthDegrees);

		writeAngleParameter ();
		writeDistanceParameter ();
		writeDragDiagnostics (where, domePoint);

		refresh ();
	}

	/** Reads a number out of the text of a read only label. Falls back to the
	    given value when the label is not there yet or carries no number. */
	double readoutNumber (int32_t tag, double fallback)
	{
		auto* label = dynamic_cast<CTextLabel*> (findTaggedControl (tag));
		if (!label)
			return fallback;

		const char* text = label->getText ();
		if (!text)
			return fallback;

		char* end = nullptr;
		const double value = std::strtod (text, &end);
		return end == text ? fallback : value;
	}

	/** Temporary: what the reverse mapping found, so that a silent failure can
	    be traced from the outside. Remove with the rest of the diagnostics. */
	void writeFollowDiagnostics (bool found, double degrees, double fraction) const
	{
		// no file logging in a released build
	}

	/** Brings the mouse position into the space the dome is laid out in.

	    VSTGUI hands a view its mouse events in the view's own coordinates
	    (CFrame converts them with frameToLocal), while the drawing context of
	    draw() is the parent's, and the dome is laid out for the drawing. The
	    two spaces differ by the view's position inside its parent - but rather
	    than trust that, both readings are tried and the one that lands on the
	    mouse is taken: a reading from the wrong space always has to be clamped
	    into the dome, or ends up far away from the mouse. */
	CPoint resolveMouseSpace (const CPoint& where) const
	{
		const Panorama::Dome dome = currentDome ();

		const CPoint direct = where;
		const CPoint shifted = toDomeSpace (where);

		const double directDistance = distanceToDome (dome, direct);
		const double shiftedDistance = distanceToDome (dome, shifted);

		// without an offset first, so that the two readings that both land
		// inside the dome do not fight over rounding
		return shiftedDistance < directDistance ? shifted : direct;
	}

	/** How far a candidate is from the dome: 0 when it is inside, and the
	    distance to the nearest point of the dome when it is not. */
	static double distanceToDome (const Panorama::Dome& dome, const CPoint& candidate)
	{
		double x = candidate.x;
		double y = candidate.y;
		Panorama::clampToDome (dome, x, y);

		const double dx = x - candidate.x;
		const double dy = y - candidate.y;
		return std::sqrt (dx * dx + dy * dy);
	}

	/** From the view's own coordinates into the ones the dome uses. */
	CPoint toDomeSpace (const CPoint& where) const
	{
		const CRect size = getViewSize ();
		return CPoint (where.x + size.left, where.y + size.top);
	}

	/** Temporary: records where the mouse arrived, which space was taken for it
	    and what the controls read back, so that the display can be diagnosed
	    from the outside. Remove with its call sites once it is settled. */
	void writeDragDiagnostics (const CPoint& where, const CPoint& used) const
	{
		// no file logging in a released build
	}

	/** Writes a parameter value the way a user edit does: the host learns about
	    the edit through beginEdit and endEdit, and about the value through
	    valueChanged, which is why the value has to be set in between. */
	static void writeParameter (CControl* control, double normalized)
	{
		if (!control || normalized < 0.0 || normalized > 1.0)
			return;

		if (std::fabs (normalized - control->getValueNormalized ()) < kEpsilon)
			return;

		control->beginEdit ();
		control->setValueNormalized (static_cast<float> (normalized));
		control->valueChanged ();
		control->endEdit ();
	}

	void writeAngleParameter ()
	{
		const double span = kMaxSpotAngleDegrees - kMinSpotAngleDegrees;
		if (span > 0.0)
			writeParameter (angleControl, (azimuthDegrees - kMinSpotAngleDegrees) / span);
	}

	/** The distance of the point from the centre is the distance parameter.
	    It stays the horizontal distance; the heights are separate controls.
	    The display stretches the near field, so the radius is not proportional
	    to the distance. */
	void writeDistanceParameter ()
	{
		const double span = kMaxDistanceMeters;
		if (span > 0.0)
			writeParameter (distanceControl, radiusToDistance (radiusFraction) / span);
	}

	/** Radius of the point, 0 at the centre and 1 on the dome. */
	static double distanceToRadius (double meters)
	{
		return Panorama::distanceToRadius (meters, kMaxDistanceMeters, kNearFieldMeters, kNearFieldRadiusShare);
	}

	/** The distance a radius stands for. */
	static double radiusToDistance (double fraction)
	{
		return Panorama::radiusToDistance (fraction, kMaxDistanceMeters, kNearFieldMeters, kNearFieldRadiusShare);
	}

	static double clampAngle (double degrees)
	{
		if (degrees < -kHalfAngleDegrees)
			return -kHalfAngleDegrees;
		if (degrees > kHalfAngleDegrees)
			return kHalfAngleDegrees;
		return degrees;
	}

	/** Position of the point: the angle and the distance from the centre, the
	    latter as a fraction of the longest distance. */
	double azimuthDegrees = 0.0;
	double radiusFraction = 0.6;

	CControl* angleControl = nullptr;
	CControl* distanceControl = nullptr;
	bool dragging = false;
	bool idleRequested = false;
};

//------------------------------------------------------------------------
class PanoramaViewCreator : public ViewCreatorAdapter
{
public:
	PanoramaViewCreator () { UIViewFactory::registerViewCreator (*this); }

	IdStringPtr getViewName () const override { return "PanoramaView"; }
	IdStringPtr getBaseViewName () const override { return "CView"; }
	UTF8StringPtr getDisplayName () const override { return "Panorama"; }

	CView* create (const UIAttributes& attributes, const IUIDescription* description) const override
	{
		return new PanoramaView (CRect (0, 0, 100, 100));
	}
};

/** Registers the view when the plug-in module is loaded. */
const PanoramaViewCreator gPanoramaViewCreator;

} // namespace
} // namespace MyCompanyName
