//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// The main microphone format, and the geometry that comes with it.
//
//   1. shows only the rows the selected format needs: the distance between
//      the outer microphones for a spaced pair, a tree and the techniques
//      that are defined with one, the centre microphone for a tree only,
//   2. shows that distance read only when the technique fixes it (ORTF, NOS,
//      DIN), and editable when the user chooses it (AB, Decca Tree).
//
// Visibility is never remembered, it is read back from the view before it is
// changed, so a wrong assumption cannot get stuck the way it did when this
// first was written.
//
// The rules live in formatRules.h and are checked by a headless test.
//
// Registered as "FormatOptionsView" and used from editor.uidesc.
//
// The row layout is fixed: caption label, bound field, and - in the spacing
// row - a second label that shows the technique's own distance. uicheck.py
// verifies that order, so a change to editor.uidesc that breaks this file is
// caught before it reaches a screen.
//------------------------------------------------------------------------

#include "formatRules.h"
#include "parameters.h"

#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/uidescription/iviewcreator.h"
#include "vstgui/uidescription/uiviewfactory.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace MyCompanyName {
namespace {

using namespace VSTGUI;

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

/** The last text label directly inside a view, which is where the spacing row
	shows the distance a technique is defined with. */
CTextLabel* findLastLabel (CView* view)
{
	auto* container = dynamic_cast<CViewContainer*> (view);
	if (!container)
		return nullptr;

	CTextLabel* found = nullptr;
	for (uint32_t i = 0; i < container->getNbViews (); ++i)
	{
		if (auto* label = dynamic_cast<CTextLabel*> (container->getView (i)))
			found = label;
	}
	return found;
}

//------------------------------------------------------------------------
/** Records the decision, so that a problem can be diagnosed from the outside.
	Temporary: remove together with its call site once the UI is settled. */
void writeDiagnostics (int format, bool spacing, bool centre, bool locked, bool foundSpacing, bool foundCentre)
{
		// no file logging in a released build
	}

//------------------------------------------------------------------------
class FormatOptionsView : public CViewContainer
{
public:
	explicit FormatOptionsView (const CRect& size) : CViewContainer (size) {}

	~FormatOptionsView () override { setWantsIdle (false); }

	// A container is painted through drawRect. It is only used here to get an
	// attached view, which is what the idle registration needs: the state is
	// applied from the idle callback, never from inside a paint.
	void drawRect (CDrawContext* context, const CRect& updateRect) override
	{
		if (!idleRequested && isAttached ())
		{
			idleRequested = true;
			setWantsIdle (true);
		}

		CViewContainer::drawRect (context, updateRect);
	}

	void onIdle () override
	{
		if (update ())
			refresh ();
	}

	CLASS_METHODS (FormatOptionsView, CViewContainer)

private:
	/** A row that the selected format may or may not need. */
	struct Row
	{
		CView* root = nullptr;
		CControl* edit = nullptr;       ///< the bound field
		CTextLabel* standard = nullptr; ///< the technique's own distance
	};

	/** Applies the rules. Returns true when it changed something, which means
	    the caller has to ask for a repaint. Must not be called from a paint:
	    a repaint requested from inside one is dropped by the frame, and the
	    change would only appear the next time the editor is redrawn - which is
	    exactly what went wrong before. */
	bool update ()
	{
		// Looked up again every time, never cached: reopening the window rebuilds
		// every control, and a cached pointer would go on answering with the value
		// it was created with.
		formatControl = findFormatControl ();

		if (!formatControl)
			return false;

		collect ();

		// a list parameter is stepped, so the control holds the index itself
		const int format = static_cast<int> (formatControl->getValue () + 0.5);
		const bool usesSpacing = Panorama::formatUsesSpacing (format);
		const bool usesCentre = Panorama::formatUsesCentreDistance (format);
		const bool locked = Panorama::formatLocksSpacing (format);
		const bool usesAngle = Panorama::formatAllowsHalfAngle (format);
		const bool usesPattern = !Panorama::formatPatternIsFixed (format);

		// Applied on every update, not only when the format changes: an early
		// update can run before the rows were added, and the state is read
		// back anyway, so repeating this costs nothing.
		bool changed = false;
		changed |= show (spacing.root, usesSpacing);
		changed |= show (centre.root, usesCentre);

		// A coincident pair has the angle it has, and a pair or a tree is done
		// with one pattern only, so neither of those has anything to offer.
		changed |= show (angle.root, usesAngle);
		changed |= show (pattern.root, usesPattern);

		// The entries a technique is not done with are greyed out, so a plain
		// pair cannot be given a cardioid and an XY cannot be given an omni.
		changed |= markPatternEntries (format);

		// the distance between the outer microphones: now always the user's,
		// since a format only fills the value in
		changed |= show (spacing.edit, true);
		if (spacing.standard)
			changed |= show (spacing.standard, false);

		if (format != lastFormat)
		{
			lastFormat = format;
			writeDiagnostics (format, usesSpacing, usesCentre, locked, spacing.root != nullptr,
			                  centre.root != nullptr);
		}

		return changed;
	}

	/** Asks for a repaint, ours and the whole editor. Called outside the paint
	    cycle only; ordinary invalid() is not enough because the frame drops it
	    when it arrives during a paint. */
	void refresh ()
	{
		invalid ();
		if (auto* frame = getFrame ())
			frame->invalid ();
	}

	/** Finds the control that selects the main microphone format.

	    It is looked up in this view first, then in the whole editor, so that
	    moving the control in editor.uidesc cannot silently disable this view. */
	CControl* findFormatControl ()
	{
		const int32_t tag = static_cast<int32_t> (kParamMainFormat);

		if (auto* control = findControl (this, tag))
			return control;

		if (auto* frame = getFrame ())
			return findControl (frame, tag);

		return nullptr;
	}

	/** Finds the rows. Retried on every update until both are there: the first
	    update can run while editor.uidesc has only added some of the children,
	    and giving up at that point would leave the rows untouched forever. */
	void collect ()
	{
		// Rebuilt every time, for the same reason as the format control above.
		spacing = Row ();
		centre = Row ();
		angle = Row ();
		pattern = Row ();
		patternControl = nullptr;

		for (uint32_t i = 0; i < getNbViews (); ++i)
		{
			auto* child = getView (i);

			if (!spacing.root)
			{
				if (auto* edit = findControl (child, static_cast<int32_t> (kParamAbSpacing)))
				{
					spacing.root = child;
					spacing.edit = edit;
					spacing.standard = findLastLabel (child);
				}
			}

			if (!centre.root)
			{
				if (auto* edit = findControl (child, static_cast<int32_t> (kParamCentreDistance)))
				{
					centre.root = child;
					centre.edit = edit;
				}
			}

			// the two rows that describe the array itself instead of its name
			if (!angle.root)
			{
				if (findControl (child, static_cast<int32_t> (kParamMainHalfAngle)))
					angle.root = child;
			}

			if (!pattern.root)
			{
				if (findControl (child, static_cast<int32_t> (kParamMainPattern)))
					pattern.root = child;
			}
		}
	}

	/** Greys out the pattern entries the selected technique is not done with.
	    Returns true when it changed something. */
	bool markPatternEntries (int format)
	{
		patternControl = findControl (pattern.root, static_cast<int32_t> (kParamMainPattern));

		auto* menu = dynamic_cast<COptionMenu*> (patternControl);
		if (!menu)
			return false;

		bool changed = false;
		const int entries = menu->getNbEntries ();
		for (int32_t i = 0; i < entries && i < Panorama::kPatternCount; ++i)
		{
			auto* entry = menu->getEntry (i);
			if (!entry)
				continue;

			const bool wanted = Panorama::formatAllowsPattern (format, i);
			if (entry->isEnabled () != wanted)
			{
				entry->setEnabled (wanted);
				changed = true;
			}
		}
		return changed;
	}

	/** Reads the state back before changing it, so that a stale idea of what
	    is on screen cannot survive. Returns true when it changed something. */
	static bool show (CView* view, bool visible)
	{
		if (!view || view->isVisible () == visible)
			return false;

		view->setVisible (visible);
		return true;
	}

	/** Returns true when the text changed. */
	bool setStandardText (int format)
	{
		char text[48];
		std::snprintf (text, sizeof (text), "%.0f cm (%s)", Panorama::formatStandardSpacingCm (format),
		               Panorama::formatName (format));

		if (standardText == text)
			return false;

		standardText = text;
		spacing.standard->setText (text);
		return true;
	}

	CControl* formatControl = nullptr;
	Row spacing;
	Row centre;
	Row angle;   ///< how far the array microphones are aimed outward
	Row pattern; ///< what the array microphones are made of
	CControl* patternControl = nullptr;
	std::string standardText;
	int lastFormat = -1;
	bool idleRequested = false;
};

//------------------------------------------------------------------------
class FormatOptionsViewCreator : public ViewCreatorAdapter
{
public:
	FormatOptionsViewCreator () { UIViewFactory::registerViewCreator (*this); }

	IdStringPtr getViewName () const override { return "FormatOptionsView"; }
	IdStringPtr getBaseViewName () const override { return "CViewContainer"; }
	UTF8StringPtr getDisplayName () const override { return "Format Options"; }

	CView* create (const UIAttributes& attributes, const IUIDescription* description) const override
	{
		return new FormatOptionsView (CRect (0, 0, 100, 100));
	}
};

/** Registers the view when the plug-in module is loaded. */
const FormatOptionsViewCreator gFormatOptionsViewCreator;

} // namespace
} // namespace MyCompanyName
