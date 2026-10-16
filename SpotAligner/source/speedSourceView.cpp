//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// Shows only the parameters that belong to the selected source of the speed
// of sound; the other group is hidden.
//
// VSTGUI cannot show a group of controls depending on another control's
// value, so this container does it itself, the same way FormatOptionsView
// does for the main microphone format.
//
// Three properties of VSTGUI shaped this:
//
//  * No IControlListener. A CControl has exactly one listener and the VST3
//    editor needs it for the parameter binding, so the mode is only read,
//    never listened to. The parameter binding stays untouched.
//
//  * Hiding is enough. CViewContainer skips an invisible child both when
//    drawing and when hit testing, so a hidden group cannot be seen, clicked
//    or tabbed into - one mechanism instead of three.
//
//  * Hiding is not enough on its own. A CTextEdit hands editing to a native
//    platform control, which keeps the keyboard focus until the focus view
//    changes (CFrame::setFocusView -> CTextEdit::looseFocus destroys that
//    control). A group that goes away therefore also gives up the focus.
//
// The state is checked on every paint and on a 30 Hz idle callback, because
// when the editor opens the mode control receives its value after the first
// paint and no further repaint is guaranteed. Visibility is never remembered,
// it is read back from the view, and a change asks for one repaint - without
// that the new state would only appear the next time something else redraws
// the editor.
//
// The view is registered as "SpeedSourceView" and used from editor.uidesc.
//------------------------------------------------------------------------

#include "parameters.h"

#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/uidescription/iviewcreator.h"
#include "vstgui/uidescription/uiviewfactory.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace MyCompanyName {
namespace {

using namespace VSTGUI;

/** The two groups are told apart by the parameters they contain. */
constexpr int32_t kGroupManual = 0; ///< speed of sound entered by hand
constexpr int32_t kGroupAir = 1;    ///< speed of sound from the air conditions
constexpr int32_t kGroupCount = 2;

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
/** The group a view belongs to, or -1 when it belongs to none. */
int32_t groupOf (CView* view)
{
	if (!view)
		return -1;

	if (auto* control = dynamic_cast<CControl*> (view))
	{
		switch (control->getTag ())
		{
			case kParamSpeedOfSound: return kGroupManual;
			case kParamTemperature:
			case kParamHumidity:
			case kParamPressure: return kGroupAir;
			default: break;
		}
	}

	if (auto* container = dynamic_cast<CViewContainer*> (view))
	{
		for (uint32_t i = 0; i < container->getNbViews (); ++i)
		{
			const int32_t group = groupOf (container->getView (i));
			if (group >= 0)
				return group;
		}
	}
	return -1;
}

//------------------------------------------------------------------------
/** Records the decision, so that a problem can be diagnosed from the outside.
	Temporary: remove together with its call site once the UI is settled. */
void writeDiagnostics (int32_t mode, int32_t group, bool visible, bool focusDropped)
{
	const char* temp = std::getenv ("TEMP");
	if (!temp)
		return;

	std::string path (temp);
	path += "\\spotaligner_ui.log";

	if (FILE* file = std::fopen (path.c_str (), "a"))
	{
		std::fprintf (file, "mode=%d group=%d visible=%d focus=%d\n", static_cast<int> (mode),
		              static_cast<int> (group), visible ? 1 : 0, focusDropped ? 1 : 0);
		std::fclose (file);
	}
}

//------------------------------------------------------------------------
class SpeedSourceView : public CViewContainer
{
public:
	explicit SpeedSourceView (const CRect& size) : CViewContainer (size) {}

	~SpeedSourceView () override { setWantsIdle (false); }

	// A container is painted through drawRect. It is only used here to get an
	// attached view, which is what the idle registration needs: the state is
	// applied from the idle callback, never from inside a paint.
	void drawRect (CDrawContext* context, const CRect& updateRect) override
	{
		if (!idleRequested && isAttached ())
		{
			// registers with the frame's idle dispatch, which reaches this
			// view however deep it is nested
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

	CLASS_METHODS (SpeedSourceView, CViewContainer)

private:
	/** One of the two groups. */
	struct Group
	{
		std::vector<CView*> roots;        ///< the top level views to show or hide
		std::vector<CControl*> controls;  ///< everything in it that can be edited
	};

	/** Applies the rules. Returns true when it changed something, which means
	    the caller has to ask for a repaint. Must not be called from a paint:
	    a repaint requested from inside one is dropped by the frame. */
	bool update ()
	{
		// Looked up again every time, never cached: closing the window and
		// opening it again rebuilds every control, and a cached pointer keeps
		// answering with the value it was created with.
		modeControl = findModeControl ();

		if (!modeControl)
			return false;

		// the speed mode parameter: 0 = manual, 1 = from the air conditions
		const int32_t mode = modeControl->getValueNormalized () < 0.5f ? kGroupManual : kGroupAir;

		bool anyChange = false;

		for (int32_t index = 0; index < kGroupCount; ++index)
		{
			Group& group = groups[index];

			collect (index, group);

			if (group.roots.empty ())
				continue;

			const bool visible = (mode == index);

			bool changed = false;
			for (auto* root : group.roots)
			{
				if (root->isVisible () != visible)
				{
					root->setVisible (visible);
					changed = true;
				}
			}

			// a group that comes back has to be editable again
			for (auto* control : group.controls)
				control->setMouseEnabled (visible);

			// a group that goes away must not keep the keyboard focus
			const bool focusDropped = visible ? false : dropFocus (group);

			if (changed)
			{
				anyChange = true;
				writeDiagnostics (mode, index, visible, focusDropped);
			}
		}

		return anyChange;
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

	/** Finds the control that selects the source of the speed of sound.

	    It is looked up in this view first, then in the whole editor: the row
	    that carries it sits outside this container, and a layout change must
	    not be able to silently disable this view - which is exactly what
	    happened when the row was moved out of it. */
	CControl* findModeControl ()
	{
		const int32_t tag = static_cast<int32_t> (kParamSpeedMode);

		if (auto* control = findControl (this, tag))
			return control;

		if (auto* frame = getFrame ())
			return findControl (frame, tag);

		return nullptr;
	}

	/** Finds the views and controls of one group. Retried until they are
	    there: the first update can run while editor.uidesc has only added some
	    of the children, and giving up then would leave the group alone. */
	void collect (int32_t index, Group& group)
	{
		// Rebuilt every time, for the same reason as the mode control above.
		group.roots.clear ();
		group.controls.clear ();

		for (uint32_t i = 0; i < getNbViews (); ++i)
		{
			auto* child = getView (i);
			if (groupOf (child) != index)
				continue;

			group.roots.push_back (child);
			collectControls (child, group.controls);
		}
	}

	static void collectControls (CView* view, std::vector<CControl*>& out)
	{
		if (!view)
			return;

		if (auto* control = dynamic_cast<CControl*> (view))
			out.push_back (control);

		if (auto* container = dynamic_cast<CViewContainer*> (view))
		{
			for (uint32_t i = 0; i < container->getNbViews (); ++i)
				collectControls (container->getView (i), out);
		}
	}

	/** Takes the keyboard focus away from a group that is being hidden: a
	    CTextEdit that keeps the focus stays editable even while invisible,
	    because it hands editing to a native control. */
	bool dropFocus (const Group& group)
	{
		auto* frame = getFrame ();
		if (!frame)
			return false;

		auto* focused = dynamic_cast<CControl*> (frame->getFocusView ());
		if (!focused)
			return false;

		if (std::find (group.controls.begin (), group.controls.end (), focused) == group.controls.end ())
			return false;

		frame->setFocusView (nullptr);
		return true;
	}

	CControl* modeControl = nullptr;
	Group groups[kGroupCount];
	bool idleRequested = false;
};

//------------------------------------------------------------------------
class SpeedSourceViewCreator : public ViewCreatorAdapter
{
public:
	SpeedSourceViewCreator () { UIViewFactory::registerViewCreator (*this); }

	IdStringPtr getViewName () const override { return "SpeedSourceView"; }
	IdStringPtr getBaseViewName () const override { return "CViewContainer"; }
	UTF8StringPtr getDisplayName () const override { return "Speed Source Group"; }

	CView* create (const UIAttributes& attributes, const IUIDescription* description) const override
	{
		return new SpeedSourceView (CRect (0, 0, 100, 100));
	}
};

/** Registers the view when the plug-in module is loaded. */
const SpeedSourceViewCreator gSpeedSourceViewCreator;

} // namespace
} // namespace MyCompanyName
