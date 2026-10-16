//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// Shows a warning when the track the plug-in sits on cannot carry the stereo
// image, and stays out of the way otherwise.
//
// The processor finds this out from the output bus the host activates, because
// that is the only thing a plug-in can know about the track it is on. It
// reports the answer through a read only parameter, the host forwards that to
// the editor, and this view reads it: the same path a user edit takes, so no
// private channel between the audio and the user interface is needed.
//
// The control that carries the answer is only there to be read; this view
// hides it so it never shows up as a stray value.
//
// State is applied from the idle callback, never from inside a paint, and a
// change asks for a repaint - a repaint requested during a paint is dropped by
// the frame, which is what made the earlier switchers look dead.
//
// Registered as "TrackStatusView" and used from editor.uidesc.
//------------------------------------------------------------------------

#include "parameters.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/uidescription/iviewcreator.h"
#include "vstgui/uidescription/uiviewfactory.h"

#include <cstdio>
#include <string>
#include <vector>

namespace MyCompanyName {
namespace {

using namespace VSTGUI;

/** The warning panel behind the message. */
const CColor kWarningBackground (0x3A, 0x2F, 0x14, 0xFF);
const CColor kWarningFrame (0xE8, 0xC0, 0x5A, 0xFF);

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
class TrackStatusView : public CViewContainer
{
public:
	explicit TrackStatusView (const CRect& size) : CViewContainer (size) {}

	~TrackStatusView () override { setWantsIdle (false); }

	void drawRect (CDrawContext* context, const CRect& updateRect) override
	{
		if (!idleRequested && isAttached ())
		{
			idleRequested = true;
			setWantsIdle (true);
		}

		if (warning)
		{
			const CRect size = getViewSize ();
			context->setFillColor (kWarningBackground);
			context->drawRect (size, kDrawFilled);
			context->setFrameColor (kWarningFrame);
			context->setLineWidth (1.0);
			context->drawRect (size, kDrawStroked);
		}

		CViewContainer::drawRect (context, updateRect);
	}

	void onIdle () override
	{
		if (update ())
			refresh ();
	}

	CLASS_METHODS (TrackStatusView, CViewContainer)

private:
	/** Applies the state. Returns true when something changed. */
	bool update ()
	{
		// Looked up again every time, never cached: reopening the window rebuilds
		// every control, and a cached pointer would keep answering with the value
		// it was created with.
		statusLabel = findStatusLabel ();

		collectLabels ();

		if (!statusLabel)
			return false;

		// The number is read out of the text the controller formatted for this
		// label. A label never receives a numeric value from the editor (see
		// updateControlValue in vst3editor.cpp: for a label only setText is
		// called), so reading the value of the control would report zero for
		// ever and warn about a track that carries stereo perfectly well. The
		// text, on the other hand, is kept up to date by the host itself - it
		// is the same path the speed and delay readouts use.
		const int channels = firstNumberIn (statusLabel->getText ().data ());
		const bool wanted = channels < static_cast<int> (kOutputChannelsForStereo);

		bool changed = false;

		if (wanted != warning)
		{
			warning = wanted;
			changed = true;
		}

		for (auto* label : warningLabels)
		{
			if (label->isVisible () != warning)
			{
				label->setVisible (warning);
				changed = true;
			}
		}

		// The line that carries the report is a label of its own in the info
		// view and stays visible there; this view only reads it to know whether
		// there is anything to warn about.

		return changed;
	}

	/** Finds the label that carries the answer. Its text is kept up to date by
	    the editor from the parameter, which is the whole point of this way of
	    reading it; nothing is written into it here.

	    It is looked up in this view first, then in the whole editor, so that
	    moving it in editor.uidesc cannot silently disconnect this view, and it
	    has to be a CTextLabel: only a label is fed the text of the parameter,
	    which is what this view reads. uicheck.py checks that. */
	CTextLabel* findStatusLabel ()
	{
		const int32_t tag = static_cast<int32_t> (kParamTrackChannels);

		CTextLabel* label = dynamic_cast<CTextLabel*> (findControl (this, tag));

		if (!label)
		{
			if (auto* frame = getFrame ())
				label = dynamic_cast<CTextLabel*> (findControl (frame, tag));
		}

		return label;
	}

	/** Collects the warning labels of this view: every label without the
	    control tag. Retried until they are there, because the first update can
	    run before the children have been added. */
	void collectLabels ()
	{
		// Rebuilt every time, for the same reason as the status label above.
		warningLabels.clear ();
		labels.clear ();

		for (uint32_t i = 0; i < getNbViews (); ++i)
		{
			auto* label = dynamic_cast<CTextLabel*> (getView (i));
			if (!label)
				continue;

			if (label->getTag () == static_cast<int32_t> (kParamTrackChannels))
				continue;

			labels.push_back (label);
		}

		warningLabels = labels;
	}

	/** Asks for a repaint, ours and the whole editor. Called outside the paint
	    cycle only, because the frame drops a repaint requested during one. */
	void refresh ()
	{
		invalid ();
		if (auto* frame = getFrame ())
			frame->invalid ();
	}

	CTextLabel* statusLabel = nullptr;
	std::vector<CTextLabel*> labels;
	std::vector<CTextLabel*> warningLabels;
	bool warning = false;
	bool idleRequested = false;
};

//------------------------------------------------------------------------
class TrackStatusViewCreator : public ViewCreatorAdapter
{
public:
	TrackStatusViewCreator () { UIViewFactory::registerViewCreator (*this); }

	IdStringPtr getViewName () const override { return "TrackStatusView"; }
	IdStringPtr getBaseViewName () const override { return "CViewContainer"; }
	UTF8StringPtr getDisplayName () const override { return "Track Status"; }

	CView* create (const UIAttributes& attributes, const IUIDescription* description) const override
	{
		return new TrackStatusView (CRect (0, 0, 100, 100));
	}
};

/** Registers the view when the plug-in module is loaded. */
const TrackStatusViewCreator gTrackStatusViewCreator;

} // namespace
} // namespace MyCompanyName
