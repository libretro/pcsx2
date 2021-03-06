/////////////////////////////////////////////////////////////////////////////
// Name:        wx/event.h
// Purpose:     Event classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVENT_H_
#define _WX_EVENT_H_

#include "wx/defs.h"
#include "wx/cpp.h"
#include "wx/object.h"
#include "wx/clntdata.h"

#include "wx/dynarray.h"
#include "wx/thread.h"
#include "wx/tracker.h"
#include "wx/typeinfo.h"
#include "wx/any.h"

// Currently VC6 and VC7 are known to not be able to compile CallAfter() code,
// so disable it for them.
#if !defined(__VISUALC__) || wxCHECK_VISUALC_VERSION(8)
    #include "wx/meta/removeref.h"

    #define wxHAS_CALL_AFTER
#endif

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxList;
class WXDLLIMPEXP_FWD_BASE wxEvent;
class WXDLLIMPEXP_FWD_BASE wxEventFilter;

// We operate with pointer to members of wxEvtHandler (such functions are used
// as event handlers in the event tables or as arguments to Connect()) but by
// default MSVC uses a restricted (but more efficient) representation of
// pointers to members which can't deal with multiple base classes. To avoid
// mysterious (as the compiler is not good enough to detect this and give a
// sensible error message) errors in the user code as soon as it defines
// classes inheriting from both wxEvtHandler (possibly indirectly, e.g. via
// wxWindow) and something else (including our own wxTrackable but not limited
// to it), we use the special MSVC keyword telling the compiler to use a more
// general pointer to member representation for the classes inheriting from
// wxEvtHandler.
#ifdef __VISUALC__
    #define wxMSVC_FWD_MULTIPLE_BASES __multiple_inheritance
#else
    #define wxMSVC_FWD_MULTIPLE_BASES
#endif

class WXDLLIMPEXP_FWD_BASE wxMSVC_FWD_MULTIPLE_BASES wxEvtHandler;
class wxEventConnectionRef;

// ----------------------------------------------------------------------------
// Event types
// ----------------------------------------------------------------------------

typedef int wxEventType;

#define wxEVT_ANY           ((wxEventType)-1)

// this is used to make the event table entry type safe, so that for an event
// handler only a function with proper parameter list can be given. See also
// the wxEVENT_HANDLER_CAST-macro.
#define wxStaticCastEvent(type, val) static_cast<type>(val)

#define wxDECLARE_EVENT_TABLE_ENTRY(type, winid, idLast, fn, obj) \
    wxEventTableEntry(type, winid, idLast, wxNewEventTableFunctor(type, fn), obj)

#define wxDECLARE_EVENT_TABLE_TERMINATOR() \
    wxEventTableEntry(wxEVT_NULL, 0, 0, 0, 0)

// generate a new unique event type
extern WXDLLIMPEXP_BASE wxEventType wxNewEventType();

// define macros to create new event types:
    // the macros are the same ones as above but defined differently as we only
    // use the integer event type values to identify events in this case

    #define wxDEFINE_EVENT( name, type ) \
        const wxEventType name( wxNewEventType() )

    #define wxDECLARE_EXPORTED_EVENT( expdecl, name, type ) \
        extern const expdecl wxEventType name
    #define wxDECLARE_EVENT( name, type ) \
        wxDECLARE_EXPORTED_EVENT( wxEMPTY_PARAMETER_VALUE, name, type )

    #define wxDEFINE_EVENT_ALIAS( name, type, value ) \
        const wxEventType name = value
    #define wxDECLARE_EXPORTED_EVENT_ALIAS( expdecl, name, type ) \
        extern const expdecl wxEventType name

// Try to cast the given event handler to the correct handler type:

#define wxEVENT_HANDLER_CAST( functype, func ) \
    ( wxObjectEventFunction )( wxEventFunction )wxStaticCastEvent( functype, &func )


// These are needed for the functor definitions
typedef void (wxEvtHandler::*wxEventFunction)(wxEvent&);

// We had some trouble (specifically with eVC for ARM WinCE build) with using
// wxEventFunction in the past so we had introduced wxObjectEventFunction which
// used to be a typedef for a member of wxObject and not wxEvtHandler to work
// around this but as eVC is not really supported any longer we now only keep
// this for backwards compatibility and, despite its name, this is a typedef
// for wxEvtHandler member now -- but if we have the same problem with another
// compiler we can restore its old definition for it.
typedef wxEventFunction wxObjectEventFunction;

// The event functor which is stored in the static and dynamic event tables:
class WXDLLIMPEXP_BASE wxEventFunctor
{
public:
    virtual ~wxEventFunctor();

    // Invoke the actual event handler:
    virtual void operator()(wxEvtHandler *, wxEvent&) = 0;

    // this function tests whether this functor is matched, for the purpose of
    // finding it in an event table in Unbind(), by the given functor:
    virtual bool IsMatching(const wxEventFunctor& functor) const = 0;

    // If the functor holds an wxEvtHandler, then get access to it and track
    // its lifetime with wxEventConnectionRef:
    virtual wxEvtHandler *GetEvtHandler() const
        { return NULL; }

    // This is only used to maintain backward compatibility in
    // wxAppConsoleBase::CallEventHandler and ensures that an overwritten
    // wxAppConsoleBase::HandleEvent is still called for functors which hold an
    // wxEventFunction:
    virtual wxEventFunction GetEvtMethod() const
        { return NULL; }

private:
    WX_DECLARE_ABSTRACT_TYPEINFO(wxEventFunctor)
};

// A plain method functor for the untyped legacy event types:
class WXDLLIMPEXP_BASE wxObjectEventFunctor : public wxEventFunctor
{
public:
    wxObjectEventFunctor(wxObjectEventFunction method, wxEvtHandler *handler)
        : m_handler( handler ), m_method( method )
        { }

    virtual void operator()(wxEvtHandler *handler, wxEvent& event);

    virtual bool IsMatching(const wxEventFunctor& functor) const
    {
        if ( wxTypeId(functor) == wxTypeId(*this) )
        {
            const wxObjectEventFunctor &other =
                static_cast< const wxObjectEventFunctor & >( functor );

            // FIXME-VC6: amazing but true: replacing "m_method == 0" here
            // with "!m_method" makes VC6 crash with an ICE in DLL build (only!)
            // Also notice that using "NULL" instead of "0" results in warnings
            // about "using NULL in arithmetics" from arm-linux-androideabi-g++
            // 4.4.3 used for wxAndroid build.

            return ( m_method == other.m_method || other.m_method == 0 ) &&
                   ( m_handler == other.m_handler || other.m_handler == NULL );
        }
        else
            return false;
    }

    virtual wxEvtHandler *GetEvtHandler() const
        { return m_handler; }

    virtual wxEventFunction GetEvtMethod() const
        { return m_method; }

private:
    wxEvtHandler *m_handler;
    wxEventFunction m_method;

    // Provide a dummy default ctor for type info purposes
    wxObjectEventFunctor() { }

    WX_DECLARE_TYPEINFO_INLINE(wxObjectEventFunctor)
};

// Create a functor for the legacy events: used by Connect()
inline wxObjectEventFunctor *
wxNewEventFunctor(const wxEventType& WXUNUSED(evtType),
                  wxObjectEventFunction method,
                  wxEvtHandler *handler)
{
    return new wxObjectEventFunctor(method, handler);
}

// This version is used by wxDECLARE_EVENT_TABLE_ENTRY()
inline wxObjectEventFunctor *
wxNewEventTableFunctor(const wxEventType& WXUNUSED(evtType),
                       wxObjectEventFunction method)
{
    return new wxObjectEventFunctor(method, NULL);
}

inline wxObjectEventFunctor
wxMakeEventFunctor(const wxEventType& WXUNUSED(evtType),
                        wxObjectEventFunction method,
                        wxEvtHandler *handler)
{
    return wxObjectEventFunctor(method, handler);
}

// many, but not all, standard event types

    // some generic events
extern WXDLLIMPEXP_BASE const wxEventType wxEVT_NULL;
extern WXDLLIMPEXP_BASE const wxEventType wxEVT_FIRST;
extern WXDLLIMPEXP_BASE const wxEventType wxEVT_USER_FIRST;

    // Need events declared to do this
class WXDLLIMPEXP_FWD_BASE wxIdleEvent;
class WXDLLIMPEXP_FWD_BASE wxThreadEvent;
class WXDLLIMPEXP_FWD_BASE wxAsyncMethodCallEvent;
class WXDLLIMPEXP_FWD_CORE wxCommandEvent;
class WXDLLIMPEXP_FWD_CORE wxMouseEvent;
class WXDLLIMPEXP_FWD_CORE wxFocusEvent;
class WXDLLIMPEXP_FWD_CORE wxChildFocusEvent;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;
class WXDLLIMPEXP_FWD_CORE wxNavigationKeyEvent;
class WXDLLIMPEXP_FWD_CORE wxSetCursorEvent;
class WXDLLIMPEXP_FWD_CORE wxScrollEvent;
class WXDLLIMPEXP_FWD_CORE wxSpinEvent;
class WXDLLIMPEXP_FWD_CORE wxScrollWinEvent;
class WXDLLIMPEXP_FWD_CORE wxSizeEvent;
class WXDLLIMPEXP_FWD_CORE wxMoveEvent;
class WXDLLIMPEXP_FWD_CORE wxCloseEvent;
class WXDLLIMPEXP_FWD_CORE wxActivateEvent;
class WXDLLIMPEXP_FWD_CORE wxWindowCreateEvent;
class WXDLLIMPEXP_FWD_CORE wxWindowDestroyEvent;
class WXDLLIMPEXP_FWD_CORE wxShowEvent;
class WXDLLIMPEXP_FWD_CORE wxIconizeEvent;
class WXDLLIMPEXP_FWD_CORE wxMaximizeEvent;
class WXDLLIMPEXP_FWD_CORE wxMouseCaptureChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxMouseCaptureLostEvent;
class WXDLLIMPEXP_FWD_CORE wxPaintEvent;
class WXDLLIMPEXP_FWD_CORE wxEraseEvent;
class WXDLLIMPEXP_FWD_CORE wxNcPaintEvent;
class WXDLLIMPEXP_FWD_CORE wxMenuEvent;
class WXDLLIMPEXP_FWD_CORE wxContextMenuEvent;
class WXDLLIMPEXP_FWD_CORE wxSysColourChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxDisplayChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxQueryNewPaletteEvent;
class WXDLLIMPEXP_FWD_CORE wxPaletteChangedEvent;
class WXDLLIMPEXP_FWD_CORE wxJoystickEvent;
class WXDLLIMPEXP_FWD_CORE wxDropFilesEvent;
class WXDLLIMPEXP_FWD_CORE wxInitDialogEvent;
class WXDLLIMPEXP_FWD_CORE wxUpdateUIEvent;
class WXDLLIMPEXP_FWD_CORE wxClipboardTextEvent;
class WXDLLIMPEXP_FWD_CORE wxHelpEvent;


    // Command events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_BUTTON, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHECKBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHOICE, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LISTBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LISTBOX_DCLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHECKLISTBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SLIDER, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RADIOBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RADIOBUTTON, wxCommandEvent);

// wxEVT_SCROLLBAR is deprecated, use wxEVT_SCROLL... events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLBAR, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_VLBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMBOBOX, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TOOL_RCLICKED, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TOOL_DROPDOWN, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TOOL_ENTER, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMBOBOX_DROPDOWN, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMBOBOX_CLOSEUP, wxCommandEvent);

    // Thread and asynchronous method call events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_THREAD, wxThreadEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_ASYNC_METHOD_CALL, wxAsyncMethodCallEvent);

    // Mouse event types
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEFT_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEFT_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MIDDLE_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MIDDLE_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RIGHT_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RIGHT_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOTION, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ENTER_WINDOW, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEAVE_WINDOW, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_LEFT_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MIDDLE_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_RIGHT_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SET_FOCUS, wxFocusEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_KILL_FOCUS, wxFocusEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHILD_FOCUS, wxChildFocusEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOUSEWHEEL, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX1_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX1_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX1_DCLICK, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX2_DOWN, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX2_UP, wxMouseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AUX2_DCLICK, wxMouseEvent);

    // Character input event type
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHAR, wxKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CHAR_HOOK, wxKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_NAVIGATION_KEY, wxNavigationKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_KEY_DOWN, wxKeyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_KEY_UP, wxKeyEvent);
// This is a private event used by wxMSW code only and subject to change or
// disappear in the future. Don't use.
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_AFTER_CHAR, wxKeyEvent);

    // Set cursor event
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SET_CURSOR, wxSetCursorEvent);

    // wxScrollBar and wxSlider event identifiers
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_TOP, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_BOTTOM, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_LINEUP, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_LINEDOWN, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_PAGEUP, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_PAGEDOWN, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_THUMBTRACK, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_THUMBRELEASE, wxScrollEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLL_CHANGED, wxScrollEvent);

    // Scroll events from wxWindow
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_TOP, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_BOTTOM, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_LINEUP, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_LINEDOWN, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_PAGEUP, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_PAGEDOWN, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_THUMBTRACK, wxScrollWinEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SCROLLWIN_THUMBRELEASE, wxScrollWinEvent);

    // System events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SIZE, wxSizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVE, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CLOSE_WINDOW, wxCloseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_END_SESSION, wxCloseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_QUERY_END_SESSION, wxCloseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ACTIVATE_APP, wxActivateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ACTIVATE, wxActivateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CREATE, wxWindowCreateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DESTROY, wxWindowDestroyEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SHOW, wxShowEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ICONIZE, wxIconizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MAXIMIZE, wxMaximizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOUSE_CAPTURE_CHANGED, wxMouseCaptureChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_PAINT, wxPaintEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_ERASE_BACKGROUND, wxEraseEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_NC_PAINT, wxNcPaintEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU_OPEN, wxMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU_CLOSE, wxMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MENU_HIGHLIGHT, wxMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_CONTEXT_MENU, wxContextMenuEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SYS_COLOUR_CHANGED, wxSysColourChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DISPLAY_CHANGED, wxDisplayChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_QUERY_NEW_PALETTE, wxQueryNewPaletteEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_PALETTE_CHANGED, wxPaletteChangedEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_BUTTON_DOWN, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_BUTTON_UP, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_MOVE, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_JOY_ZMOVE, wxJoystickEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DROP_FILES, wxDropFilesEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_INIT_DIALOG, wxInitDialogEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_IDLE, wxIdleEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_UPDATE_UI, wxUpdateUIEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SIZING, wxSizeEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVING, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVE_START, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_MOVE_END, wxMoveEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_HIBERNATE, wxActivateEvent);

    // Clipboard events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_COPY, wxClipboardTextEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_CUT, wxClipboardTextEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_PASTE, wxClipboardTextEvent);

    // Generic command events
    // Note: a click is a higher-level event than button down/up
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_LEFT_CLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_LEFT_DCLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_RIGHT_CLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_RIGHT_DCLICK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_SET_FOCUS, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_KILL_FOCUS, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COMMAND_ENTER, wxCommandEvent);

    // Help events
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_HELP, wxHelpEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_DETAILED_HELP, wxHelpEvent);

// these 2 events are the same
#define wxEVT_TOOL wxEVT_MENU

// ----------------------------------------------------------------------------
// Compatibility
// ----------------------------------------------------------------------------

// this event is also used by wxComboBox and wxSpinCtrl which don't include
// wx/textctrl.h in all ports [yet], so declare it here as well
//
// still, any new code using it should include wx/textctrl.h explicitly
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT, wxCommandEvent);


// ----------------------------------------------------------------------------
// wxEvent(-derived) classes
// ----------------------------------------------------------------------------

// the predefined constants for the number of times we propagate event
// upwards window child-parent chain
enum wxEventPropagation
{
    // don't propagate it at all
    wxEVENT_PROPAGATE_NONE = 0,

    // propagate it until it is processed
    wxEVENT_PROPAGATE_MAX = INT_MAX
};

// The different categories for a wxEvent; see wxEvent::GetEventCategory.
// NOTE: they are used as OR-combinable flags by wxEventLoopBase::YieldFor
enum wxEventCategory
{
    // this is the category for those events which are generated to update
    // the appearance of the GUI but which (usually) do not comport data
    // processing, i.e. which do not provide input or output data
    // (e.g. size events, scroll events, etc).
    // They are events NOT directly generated by the user's input devices.
    wxEVT_CATEGORY_UI = 1,

    // this category groups those events which are generated directly from the
    // user through input devices like mouse and keyboard and usually result in
    // data to be processed from the application.
    // (e.g. mouse clicks, key presses, etc)
    wxEVT_CATEGORY_USER_INPUT = 2,

    // this category is for wxSocketEvent
    wxEVT_CATEGORY_SOCKET = 4,

    // this category is for wxTimerEvent
    wxEVT_CATEGORY_TIMER = 8,

    // this category is for any event used to send notifications from the
    // secondary threads to the main one or in general for notifications among
    // different threads (which may or may not be user-generated)
    wxEVT_CATEGORY_THREAD = 16,


    // implementation only

    // used in the implementations of wxEventLoopBase::YieldFor
    wxEVT_CATEGORY_UNKNOWN = 32,

    // a special category used as an argument to wxEventLoopBase::YieldFor to indicate that
    // Yield() should leave all wxEvents on the queue while emptying the native event queue
    // (native events will be processed but the wxEvents they generate will be queued)
    wxEVT_CATEGORY_CLIPBOARD = 64,


    // shortcut masks

    // this category groups those events which are emitted in response to
    // events of the native toolkit and which typically are not-"delayable".
    wxEVT_CATEGORY_NATIVE_EVENTS = wxEVT_CATEGORY_UI|wxEVT_CATEGORY_USER_INPUT,

    // used in wxEventLoopBase::YieldFor to specify all event categories should be processed:
    wxEVT_CATEGORY_ALL =
        wxEVT_CATEGORY_UI|wxEVT_CATEGORY_USER_INPUT|wxEVT_CATEGORY_SOCKET| \
        wxEVT_CATEGORY_TIMER|wxEVT_CATEGORY_THREAD|wxEVT_CATEGORY_UNKNOWN| \
        wxEVT_CATEGORY_CLIPBOARD
};

/*
 * wxWidgets events, covering all interesting things that might happen
 * (button clicking, resizing, setting text in widgets, etc.).
 *
 * For each completely new event type, derive a new event class.
 * An event CLASS represents a C++ class defining a range of similar event TYPES;
 * examples are canvas events, panel item command events.
 * An event TYPE is a unique identifier for a particular system event,
 * such as a button press or a listbox deselection.
 *
 */

class WXDLLIMPEXP_BASE wxEvent : public wxObject
{
public:
    wxEvent(int winid = 0, wxEventType commandType = wxEVT_NULL );

    void SetEventType(wxEventType typ) { m_eventType = typ; }
    wxEventType GetEventType() const { return m_eventType; }

    wxObject *GetEventObject() const { return m_eventObject; }
    void SetEventObject(wxObject *obj) { m_eventObject = obj; }

    long GetTimestamp() const { return m_timeStamp; }
    void SetTimestamp(long ts = 0) { m_timeStamp = ts; }

    int GetId() const { return m_id; }
    void SetId(int Id) { m_id = Id; }

    // Returns the user data optionally associated with the event handler when
    // using Connect() or Bind().
    wxObject *GetEventUserData() const { return m_callbackUserData; }

    // Can instruct event processor that we wish to ignore this event
    // (treat as if the event table entry had not been found): this must be done
    // to allow the event processing by the base classes (calling event.Skip()
    // is the analog of calling the base class version of a virtual function)
    void Skip(bool skip = true) { m_skipped = skip; }
    bool GetSkipped() const { return m_skipped; }

    // This function is used to create a copy of the event polymorphically and
    // all derived classes must implement it because otherwise wxPostEvent()
    // for them wouldn't work (it needs to do a copy of the event)
    virtual wxEvent *Clone() const = 0;

    // this function is used to selectively process events in wxEventLoopBase::YieldFor
    // NOTE: by default it returns wxEVT_CATEGORY_UI just because the major
    //       part of wxWidgets events belong to that category.
    virtual wxEventCategory GetEventCategory() const
        { return wxEVT_CATEGORY_UI; }

    // Implementation only: this test is explicitly anti OO and this function
    // exists only for optimization purposes.
    bool IsCommandEvent() const { return m_isCommandEvent; }

    // Determine if this event should be propagating to the parent window.
    bool ShouldPropagate() const
        { return m_propagationLevel != wxEVENT_PROPAGATE_NONE; }

    // Stop an event from propagating to its parent window, returns the old
    // propagation level value
    int StopPropagation()
    {
        int propagationLevel = m_propagationLevel;
        m_propagationLevel = wxEVENT_PROPAGATE_NONE;
        return propagationLevel;
    }

    // Resume the event propagation by restoring the propagation level
    // (returned by StopPropagation())
    void ResumePropagation(int propagationLevel)
    {
        m_propagationLevel = propagationLevel;
    }

    // This method is for internal use only and allows to get the object that
    // is propagating this event upwards the window hierarchy, if any.
    wxEvtHandler* GetPropagatedFrom() const { return m_propagatedFrom; }

    // This is for internal use only and is only called by
    // wxEvtHandler::ProcessEvent() to check whether it's the first time this
    // event is being processed
    bool WasProcessed()
    {
        if ( m_wasProcessed )
            return true;

        m_wasProcessed = true;

        return false;
    }

    // This is for internal use only and is used for setting, testing and
    // resetting of m_willBeProcessedAgain flag.
    void SetWillBeProcessedAgain()
    {
        m_willBeProcessedAgain = true;
    }

    bool WillBeProcessedAgain()
    {
        if ( m_willBeProcessedAgain )
        {
            m_willBeProcessedAgain = false;
            return true;
        }

        return false;
    }

    // This is also used only internally by ProcessEvent() to check if it
    // should process the event normally or only restrict the search for the
    // event handler to this object itself.
    bool ShouldProcessOnlyIn(wxEvtHandler *h) const
    {
        return h == m_handlerToProcessOnlyIn;
    }

    // Called to indicate that the result of ShouldProcessOnlyIn() wasn't taken
    // into account. The existence of this function may seem counterintuitive
    // but unfortunately it's needed by wxScrollHelperEvtHandler, see comments
    // there. Don't even think of using this in your own code, this is a gross
    // hack and is only needed because of wx complicated history and should
    // never be used anywhere else.
    void DidntHonourProcessOnlyIn()
    {
        m_handlerToProcessOnlyIn = NULL;
    }

protected:
    wxObject*         m_eventObject;
    wxEventType       m_eventType;
    long              m_timeStamp;
    int               m_id;

public:
    // m_callbackUserData is for internal usage only
    wxObject*         m_callbackUserData;

private:
    // If this handler
    wxEvtHandler *m_handlerToProcessOnlyIn;

protected:
    // the propagation level: while it is positive, we propagate the event to
    // the parent window (if any)
    int               m_propagationLevel;

    // The object that the event is being propagated from, initially NULL and
    // only set by wxPropagateOnce.
    wxEvtHandler*     m_propagatedFrom;

    bool              m_skipped;
    bool              m_isCommandEvent;

    // initially false but becomes true as soon as WasProcessed() is called for
    // the first time, as this is done only by ProcessEvent() it explains the
    // variable name: it becomes true after ProcessEvent() was called at least
    // once for this event
    bool m_wasProcessed;

    // This one is initially false too, but can be set to true to indicate that
    // the event will be passed to another handler if it's not processed in
    // this one.
    bool m_willBeProcessedAgain;

protected:
    wxEvent(const wxEvent&);            // for implementing Clone()
    wxEvent& operator=(const wxEvent&); // for derived classes operator=()

private:
    // It needs to access our m_propagationLevel and m_propagatedFrom fields.
    friend class WXDLLIMPEXP_FWD_BASE wxPropagateOnce;

    // and this one needs to access our m_handlerToProcessOnlyIn
    friend class WXDLLIMPEXP_FWD_BASE wxEventProcessInHandlerOnly;


    DECLARE_ABSTRACT_CLASS(wxEvent)
};

/*
 * Helper class to temporarily change an event not to propagate.
 */
class WXDLLIMPEXP_BASE wxPropagationDisabler
{
public:
    wxPropagationDisabler(wxEvent& event) : m_event(event)
    {
        m_propagationLevelOld = m_event.StopPropagation();
    }

    ~wxPropagationDisabler()
    {
        m_event.ResumePropagation(m_propagationLevelOld);
    }

private:
    wxEvent& m_event;
    int m_propagationLevelOld;

    wxDECLARE_NO_COPY_CLASS(wxPropagationDisabler);
};

/*
 * Helper used to indicate that an event is propagated upwards the window
 * hierarchy by the given window.
 */
class WXDLLIMPEXP_BASE wxPropagateOnce
{
public:
    // The handler argument should normally be non-NULL to allow the parent
    // event handler to know that it's being used to process an event coming
    // from the child, it's only NULL by default for backwards compatibility.
    wxPropagateOnce(wxEvent& event, wxEvtHandler* handler = NULL)
        : m_event(event),
          m_propagatedFromOld(event.m_propagatedFrom)
    {
        wxASSERT_MSG( m_event.m_propagationLevel > 0,
                        wxT("shouldn't be used unless ShouldPropagate()!") );

        m_event.m_propagationLevel--;
        m_event.m_propagatedFrom = handler;
    }

    ~wxPropagateOnce()
    {
        m_event.m_propagatedFrom = m_propagatedFromOld;
        m_event.m_propagationLevel++;
    }

private:
    wxEvent& m_event;
    wxEvtHandler* const m_propagatedFromOld;

    wxDECLARE_NO_COPY_CLASS(wxPropagateOnce);
};

// A helper object used to temporarily make wxEvent::ShouldProcessOnlyIn()
// return true for the handler passed to its ctor.
class wxEventProcessInHandlerOnly
{
public:
    wxEventProcessInHandlerOnly(wxEvent& event, wxEvtHandler *handler)
        : m_event(event),
          m_handlerToProcessOnlyInOld(event.m_handlerToProcessOnlyIn)
    {
        m_event.m_handlerToProcessOnlyIn = handler;
    }

    ~wxEventProcessInHandlerOnly()
    {
        m_event.m_handlerToProcessOnlyIn = m_handlerToProcessOnlyInOld;
    }

private:
    wxEvent& m_event;
    wxEvtHandler * const m_handlerToProcessOnlyInOld;

    wxDECLARE_NO_COPY_CLASS(wxEventProcessInHandlerOnly);
};


class WXDLLIMPEXP_BASE wxEventBasicPayloadMixin
{
public:
    wxEventBasicPayloadMixin()
        : m_commandInt(0),
          m_extraLong(0)
    {
    }

    void SetString(const wxString& s) { m_cmdString = s; }
    const wxString& GetString() const { return m_cmdString; }

    void SetInt(int i) { m_commandInt = i; }
    int GetInt() const { return m_commandInt; }

    void SetExtraLong(long extraLong) { m_extraLong = extraLong; }
    long GetExtraLong() const { return m_extraLong; }

protected:
    // Note: these variables have "cmd" or "command" in their name for backward compatibility:
    //       they used to be part of wxCommandEvent, not this mixin.
    wxString          m_cmdString;     // String event argument
    int               m_commandInt;
    long              m_extraLong;     // Additional information (e.g. select/deselect)

    wxDECLARE_NO_ASSIGN_CLASS(wxEventBasicPayloadMixin);
};

class WXDLLIMPEXP_BASE wxEventAnyPayloadMixin : public wxEventBasicPayloadMixin
{
public:
    wxEventAnyPayloadMixin() : wxEventBasicPayloadMixin() {}

    wxDECLARE_NO_ASSIGN_CLASS(wxEventBasicPayloadMixin);
};


// Idle event
/*
 wxEVT_IDLE
 */

// Whether to always send idle events to windows, or
// to only send update events to those with the
// wxWS_EX_PROCESS_IDLE style.

enum wxIdleMode
{
        // Send idle events to all windows
    wxIDLE_PROCESS_ALL,

        // Send idle events to windows that have
        // the wxWS_EX_PROCESS_IDLE flag specified
    wxIDLE_PROCESS_SPECIFIED
};

class WXDLLIMPEXP_BASE wxIdleEvent : public wxEvent
{
public:
    wxIdleEvent()
        : wxEvent(0, wxEVT_IDLE),
          m_requestMore(false)
        { }
    wxIdleEvent(const wxIdleEvent& event)
        : wxEvent(event),
          m_requestMore(event.m_requestMore)
    { }

    void RequestMore(bool needMore = true) { m_requestMore = needMore; }
    bool MoreRequested() const { return m_requestMore; }

    virtual wxEvent *Clone() const { return new wxIdleEvent(*this); }

    // Specify how wxWidgets will send idle events: to
    // all windows, or only to those which specify that they
    // will process the events.
    static void SetMode(wxIdleMode mode) { sm_idleMode = mode; }

    // Returns the idle event mode
    static wxIdleMode GetMode() { return sm_idleMode; }

protected:
    bool m_requestMore;
    static wxIdleMode sm_idleMode;

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxIdleEvent)
};


// Thread event

class WXDLLIMPEXP_BASE wxThreadEvent : public wxEvent,
                                       public wxEventAnyPayloadMixin
{
public:
    wxThreadEvent(wxEventType eventType = wxEVT_THREAD, int id = wxID_ANY)
        : wxEvent(id, eventType)
        { }

    wxThreadEvent(const wxThreadEvent& event)
        : wxEvent(event),
          wxEventAnyPayloadMixin(event)
    {
        // make sure our string member (which uses COW, aka refcounting) is not
        // shared by other wxString instances:
        SetString(GetString().Clone());
    }

    virtual wxEvent *Clone() const
    {
        return new wxThreadEvent(*this);
    }

    // this is important to avoid that calling wxEventLoopBase::YieldFor thread events
    // gets processed when this is unwanted:
    virtual wxEventCategory GetEventCategory() const
        { return wxEVT_CATEGORY_THREAD; }

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxThreadEvent)
};


// Asynchronous method call events: these event are processed by wxEvtHandler
// itself and result in a call to its Execute() method which simply calls the
// specified method. The difference with a simple method call is that this is
// done asynchronously, i.e. at some later time, instead of immediately when
// the event object is constructed.

#ifdef wxHAS_CALL_AFTER

// This is a base class used to process all method calls.
class wxAsyncMethodCallEvent : public wxEvent
{
public:
    wxAsyncMethodCallEvent(wxObject* object)
        : wxEvent(wxID_ANY, wxEVT_ASYNC_METHOD_CALL)
    {
        SetEventObject(object);
    }

    wxAsyncMethodCallEvent(const wxAsyncMethodCallEvent& other)
        : wxEvent(other)
    {
    }

    virtual void Execute() = 0;
};

// This is a version for calling methods without parameters.
template <typename T>
class wxAsyncMethodCallEvent0 : public wxAsyncMethodCallEvent
{
public:
    typedef T ObjectType;
    typedef void (ObjectType::*MethodType)();

    wxAsyncMethodCallEvent0(ObjectType* object,
                            MethodType method)
        : wxAsyncMethodCallEvent(object),
          m_object(object),
          m_method(method)
    {
    }

    wxAsyncMethodCallEvent0(const wxAsyncMethodCallEvent0& other)
        : wxAsyncMethodCallEvent(other),
          m_object(other.m_object),
          m_method(other.m_method)
    {
    }

    virtual wxEvent *Clone() const
    {
        return new wxAsyncMethodCallEvent0(*this);
    }

    virtual void Execute()
    {
        (m_object->*m_method)();
    }

private:
    ObjectType* const m_object;
    const MethodType m_method;
};

// This is a version for calling methods with a single parameter.
template <typename T, typename T1>
class wxAsyncMethodCallEvent1 : public wxAsyncMethodCallEvent
{
public:
    typedef T ObjectType;
    typedef void (ObjectType::*MethodType)(T1 x1);
    typedef typename wxRemoveRef<T1>::type ParamType1;

    wxAsyncMethodCallEvent1(ObjectType* object,
                            MethodType method,
                            const ParamType1& x1)
        : wxAsyncMethodCallEvent(object),
          m_object(object),
          m_method(method),
          m_param1(x1)
    {
    }

    wxAsyncMethodCallEvent1(const wxAsyncMethodCallEvent1& other)
        : wxAsyncMethodCallEvent(other),
          m_object(other.m_object),
          m_method(other.m_method),
          m_param1(other.m_param1)
    {
    }

    virtual wxEvent *Clone() const
    {
        return new wxAsyncMethodCallEvent1(*this);
    }

    virtual void Execute()
    {
        (m_object->*m_method)(m_param1);
    }

private:
    ObjectType* const m_object;
    const MethodType m_method;
    const ParamType1 m_param1;
};

// This is a version for calling methods with two parameters.
template <typename T, typename T1, typename T2>
class wxAsyncMethodCallEvent2 : public wxAsyncMethodCallEvent
{
public:
    typedef T ObjectType;
    typedef void (ObjectType::*MethodType)(T1 x1, T2 x2);
    typedef typename wxRemoveRef<T1>::type ParamType1;
    typedef typename wxRemoveRef<T2>::type ParamType2;

    wxAsyncMethodCallEvent2(ObjectType* object,
                            MethodType method,
                            const ParamType1& x1,
                            const ParamType2& x2)
        : wxAsyncMethodCallEvent(object),
          m_object(object),
          m_method(method),
          m_param1(x1),
          m_param2(x2)
    {
    }

    wxAsyncMethodCallEvent2(const wxAsyncMethodCallEvent2& other)
        : wxAsyncMethodCallEvent(other),
          m_object(other.m_object),
          m_method(other.m_method),
          m_param1(other.m_param1),
          m_param2(other.m_param2)
    {
    }

    virtual wxEvent *Clone() const
    {
        return new wxAsyncMethodCallEvent2(*this);
    }

    virtual void Execute()
    {
        (m_object->*m_method)(m_param1, m_param2);
    }

private:
    ObjectType* const m_object;
    const MethodType m_method;
    const ParamType1 m_param1;
    const ParamType2 m_param2;
};

// This is a version for calling any functors
template <typename T>
class wxAsyncMethodCallEventFunctor : public wxAsyncMethodCallEvent
{
public:
    typedef T FunctorType;

    wxAsyncMethodCallEventFunctor(wxObject *object, const FunctorType& fn)
        : wxAsyncMethodCallEvent(object),
          m_fn(fn)
    {
    }

    wxAsyncMethodCallEventFunctor(const wxAsyncMethodCallEventFunctor& other)
        : wxAsyncMethodCallEvent(other),
          m_fn(other.m_fn)
    {
    }

    virtual wxEvent *Clone() const
    {
        return new wxAsyncMethodCallEventFunctor(*this);
    }

    virtual void Execute()
    {
        m_fn();
    }

private:
    FunctorType m_fn;
};

#endif // wxHAS_CALL_AFTER

// ============================================================================
// event handler and related classes
// ============================================================================


// struct containing the members common to static and dynamic event tables
// entries
struct WXDLLIMPEXP_BASE wxEventTableEntryBase
{
    wxEventTableEntryBase(int winid, int idLast,
                          wxEventFunctor* fn, wxObject *data)
        : m_id(winid),
          m_lastId(idLast),
          m_fn(fn),
          m_callbackUserData(data)
    {
        wxASSERT_MSG( idLast == wxID_ANY || winid <= idLast,
                      "invalid IDs range: lower bound > upper bound" );
    }

    wxEventTableEntryBase( const wxEventTableEntryBase &entry )
        : m_id( entry.m_id ),
          m_lastId( entry.m_lastId ),
          m_fn( entry.m_fn ),
          m_callbackUserData( entry.m_callbackUserData )
    {
        // This is a 'hack' to ensure that only one instance tries to delete
        // the functor pointer. It is safe as long as the only place where the
        // copy constructor is being called is when the static event tables are
        // being initialized (a temporary instance is created and then this
        // constructor is called).

        const_cast<wxEventTableEntryBase&>( entry ).m_fn = NULL;
    }

    ~wxEventTableEntryBase()
    {
        delete m_fn;
    }

    // the range of ids for this entry: if m_lastId == wxID_ANY, the range
    // consists only of m_id, otherwise it is m_id..m_lastId inclusive
    int m_id,
        m_lastId;

    // function/method/functor to call
    wxEventFunctor* m_fn;

    // arbitrary user data associated with the callback
    wxObject* m_callbackUserData;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxEventTableEntryBase);
};

// an entry from a static event table
struct WXDLLIMPEXP_BASE wxEventTableEntry : public wxEventTableEntryBase
{
    wxEventTableEntry(const int& evType, int winid, int idLast,
                      wxEventFunctor* fn, wxObject *data)
        : wxEventTableEntryBase(winid, idLast, fn, data),
        m_eventType(evType)
    { }

    // the reference to event type: this allows us to not care about the
    // (undefined) order in which the event table entries and the event types
    // are initialized: initially the value of this reference might be
    // invalid, but by the time it is used for the first time, all global
    // objects will have been initialized (including the event type constants)
    // and so it will have the correct value when it is needed
    const int& m_eventType;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxEventTableEntry);
};

// an entry used in dynamic event table managed by wxEvtHandler::Connect()
struct WXDLLIMPEXP_BASE wxDynamicEventTableEntry : public wxEventTableEntryBase
{
    wxDynamicEventTableEntry(int evType, int winid, int idLast,
                             wxEventFunctor* fn, wxObject *data)
        : wxEventTableEntryBase(winid, idLast, fn, data),
          m_eventType(evType)
    { }

    // not a reference here as we can't keep a reference to a temporary int
    // created to wrap the constant value typically passed to Connect() - nor
    // do we need it
    int m_eventType;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxDynamicEventTableEntry);
};

// ----------------------------------------------------------------------------
// wxEventTable: an array of event entries terminated with {0, 0, 0, 0, 0}
// ----------------------------------------------------------------------------

struct WXDLLIMPEXP_BASE wxEventTable
{
    const wxEventTable *baseTable;    // base event table (next in chain)
    const wxEventTableEntry *entries; // bottom of entry array
};

// ----------------------------------------------------------------------------
// wxEventHashTable: a helper of wxEvtHandler to speed up wxEventTable lookups.
// ----------------------------------------------------------------------------

WX_DEFINE_ARRAY_PTR(const wxEventTableEntry*, wxEventTableEntryPointerArray);

class WXDLLIMPEXP_BASE wxEventHashTable
{
private:
    // Internal data structs
    struct EventTypeTable
    {
        wxEventType                   eventType;
        wxEventTableEntryPointerArray eventEntryTable;
    };
    typedef EventTypeTable* EventTypeTablePointer;

public:
    // Constructor, needs the event table it needs to hash later on.
    // Note: hashing of the event table is not done in the constructor as it
    //       can be that the event table is not yet full initialize, the hash
    //       will gets initialized when handling the first event look-up request.
    wxEventHashTable(const wxEventTable &table);
    // Destructor.
    ~wxEventHashTable();

    // Handle the given event, in other words search the event table hash
    // and call self->ProcessEvent() if a match was found.
    bool HandleEvent(wxEvent& event, wxEvtHandler *self);

    // Clear table
    void Clear();

protected:
    // Init the hash table with the entries of the static event table.
    void InitHashTable();
    // Helper function of InitHashTable() to insert 1 entry into the hash table.
    void AddEntry(const wxEventTableEntry &entry);
    // Allocate and init with null pointers the base hash table.
    void AllocEventTypeTable(size_t size);
    // Grow the hash table in size and transfer all items currently
    // in the table to the correct location in the new table.
    void GrowEventTypeTable();

protected:
    const wxEventTable    &m_table;
    bool                   m_rebuildHash;

    size_t                 m_size;
    EventTypeTablePointer *m_eventTypeTable;

    static wxEventHashTable* sm_first;
    wxEventHashTable* m_previous;
    wxEventHashTable* m_next;

    wxDECLARE_NO_COPY_CLASS(wxEventHashTable);
};

// ----------------------------------------------------------------------------
// wxEvtHandler: the base class for all objects handling wxWidgets events
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxEvtHandler : public wxObject
                                    , public wxTrackable
{
public:
    wxEvtHandler();
    virtual ~wxEvtHandler();


    // Event handler chain
    // -------------------

    wxEvtHandler *GetNextHandler() const { return m_nextHandler; }
    wxEvtHandler *GetPreviousHandler() const { return m_previousHandler; }
    virtual void SetNextHandler(wxEvtHandler *handler) { m_nextHandler = handler; }
    virtual void SetPreviousHandler(wxEvtHandler *handler) { m_previousHandler = handler; }

    void SetEvtHandlerEnabled(bool enabled) { m_enabled = enabled; }
    bool GetEvtHandlerEnabled() const { return m_enabled; }

    void Unlink();
    bool IsUnlinked() const;


    // Global event filters
    // --------------------

    // Add an event filter whose FilterEvent() method will be called for each
    // and every event processed by wxWidgets. The filters are called in LIFO
    // order and wxApp is registered as an event filter by default. The pointer
    // must remain valid until it's removed with RemoveFilter() and is not
    // deleted by wxEvtHandler.
    static void AddFilter(wxEventFilter* filter);

    // Remove a filter previously installed with AddFilter().
    static void RemoveFilter(wxEventFilter* filter);


    // Event queuing and processing
    // ----------------------------

    // Process an event right now: this can only be called from the main
    // thread, use QueueEvent() for scheduling the events for
    // processing from other threads.
    virtual bool ProcessEvent(wxEvent& event);

    // Process an event by calling ProcessEvent and handling any exceptions
    // thrown by event handlers. It's mostly useful when processing wx events
    // when called from C code (e.g. in GTK+ callback) when the exception
    // wouldn't correctly propagate to wxEventLoop.
    bool SafelyProcessEvent(wxEvent& event);
        // NOTE: uses ProcessEvent()

    // This method tries to process the event in this event handler, including
    // any preprocessing done by TryBefore() and all the handlers chained to
    // it, but excluding the post-processing done in TryAfter().
    //
    // It is meant to be called from ProcessEvent() only and is not virtual,
    // additional event handlers can be hooked into the normal event processing
    // logic using TryBefore() and TryAfter() hooks.
    //
    // You can also call it yourself to forward an event to another handler but
    // without propagating it upwards if it's unhandled (this is usually
    // unwanted when forwarding as the original handler would already do it if
    // needed normally).
    bool ProcessEventLocally(wxEvent& event);

    // Schedule the given event to be processed later. It takes ownership of
    // the event pointer, i.e. it will be deleted later. This is safe to call
    // from multiple threads although you still need to ensure that wxString
    // fields of the event object are deep copies and not use the same string
    // buffer as other wxString objects in this thread.
    virtual void QueueEvent(wxEvent *event);

    // Add an event to be processed later: notice that this function is not
    // safe to call from threads other than main, use QueueEvent()
    virtual void AddPendingEvent(const wxEvent& event)
    {
        // notice that the thread-safety problem comes from the fact that
        // Clone() doesn't make deep copies of wxString fields of wxEvent
        // object and so the same wxString could be used from both threads when
        // the event object is destroyed in this one -- QueueEvent() avoids
        // this problem as the event pointer is not used any more in this
        // thread at all after it is called.
        QueueEvent(event.Clone());
    }

    void ProcessPendingEvents();
        // NOTE: uses ProcessEvent()

    void DeletePendingEvents();

#if wxUSE_THREADS
    bool ProcessThreadEvent(const wxEvent& event);
        // NOTE: uses AddPendingEvent(); call only from secondary threads
#endif

#ifdef wxHAS_CALL_AFTER
    // Asynchronous method calls: these methods schedule the given method
    // pointer for a later call (during the next idle event loop iteration).
    //
    // Notice that the method is called on this object itself, so the object
    // CallAfter() is called on must have the correct dynamic type.
    //
    // These method can be used from another thread.

    template <typename T>
    void CallAfter(void (T::*method)())
    {
        QueueEvent(
            new wxAsyncMethodCallEvent0<T>(static_cast<T*>(this), method)
        );
    }

    // Notice that we use P1 and not T1 for the parameter to allow passing
    // parameters that are convertible to the type taken by the method
    // instead of being exactly the same, to be closer to the usual method call
    // semantics.
    template <typename T, typename T1, typename P1>
    void CallAfter(void (T::*method)(T1 x1), P1 x1)
    {
        QueueEvent(
            new wxAsyncMethodCallEvent1<T, T1>(
                static_cast<T*>(this), method, x1)
        );
    }

    template <typename T, typename T1, typename T2, typename P1, typename P2>
    void CallAfter(void (T::*method)(T1 x1, T2 x2), P1 x1, P2 x2)
    {
        QueueEvent(
            new wxAsyncMethodCallEvent2<T, T1, T2>(
                static_cast<T*>(this), method, x1, x2)
        );
    }

    template <typename T>
    void CallAfter(const T& fn)
    {
        QueueEvent(new wxAsyncMethodCallEventFunctor<T>(this, fn));
    }
#endif // wxHAS_CALL_AFTER


    // Connecting and disconnecting
    // ----------------------------

    // These functions are used for old, untyped, event handlers and don't
    // check that the type of the function passed to them actually matches the
    // type of the event. They also only allow connecting events to methods of
    // wxEvtHandler-derived classes.
    //
    // The template Connect() methods below are safer and allow connecting
    // events to arbitrary functions or functors -- but require compiler
    // support for templates.

    // Dynamic association of a member function handler with the event handler,
    // winid and event type
    void Connect(int winid,
                 int lastId,
                 wxEventType eventType,
                 wxObjectEventFunction func,
                 wxObject *userData = NULL,
                 wxEvtHandler *eventSink = NULL)
    {
        DoBind(winid, lastId, eventType,
                  wxNewEventFunctor(eventType, func, eventSink),
                  userData);
    }

    // Convenience function: take just one id
    void Connect(int winid,
                 wxEventType eventType,
                 wxObjectEventFunction func,
                 wxObject *userData = NULL,
                 wxEvtHandler *eventSink = NULL)
        { Connect(winid, wxID_ANY, eventType, func, userData, eventSink); }

    // Even more convenient: without id (same as using id of wxID_ANY)
    void Connect(wxEventType eventType,
                 wxObjectEventFunction func,
                 wxObject *userData = NULL,
                 wxEvtHandler *eventSink = NULL)
        { Connect(wxID_ANY, wxID_ANY, eventType, func, userData, eventSink); }

    bool Disconnect(int winid,
                    int lastId,
                    wxEventType eventType,
                    wxObjectEventFunction func = NULL,
                    wxObject *userData = NULL,
                    wxEvtHandler *eventSink = NULL)
    {
        return DoUnbind(winid, lastId, eventType,
                            wxMakeEventFunctor(eventType, func, eventSink),
                            userData );
    }

    bool Disconnect(int winid = wxID_ANY,
                    wxEventType eventType = wxEVT_NULL,
                    wxObjectEventFunction func = NULL,
                    wxObject *userData = NULL,
                    wxEvtHandler *eventSink = NULL)
        { return Disconnect(winid, wxID_ANY, eventType, func, userData, eventSink); }

    bool Disconnect(wxEventType eventType,
                    wxObjectEventFunction func,
                    wxObject *userData = NULL,
                    wxEvtHandler *eventSink = NULL)
        { return Disconnect(wxID_ANY, eventType, func, userData, eventSink); }

    wxList* GetDynamicEventTable() const { return m_dynamicEvents ; }

    // User data can be associated with each wxEvtHandler
    void SetClientObject( wxClientData *data ) { DoSetClientObject(data); }
    wxClientData *GetClientObject() const { return DoGetClientObject(); }

    void SetClientData( void *data ) { DoSetClientData(data); }
    void *GetClientData() const { return DoGetClientData(); }


    // implementation from now on
    // --------------------------

    // check if the given event table entry matches this event by id (the check
    // for the event type should be done by caller) and call the handler if it
    // does
    //
    // return true if the event was processed, false otherwise (no match or the
    // handler decided to skip the event)
    static bool ProcessEventIfMatchesId(const wxEventTableEntryBase& tableEntry,
                                        wxEvtHandler *handler,
                                        wxEvent& event);

    virtual bool SearchEventTable(wxEventTable& table, wxEvent& event);
    bool SearchDynamicEventTable( wxEvent& event );

    // Avoid problems at exit by cleaning up static hash table gracefully
    void ClearEventHashTable() { GetEventHashTable().Clear(); }
    void OnSinkDestroyed( wxEvtHandler *sink );


private:
    void DoBind(int winid,
                   int lastId,
                   wxEventType eventType,
                   wxEventFunctor *func,
                   wxObject* userData = NULL);

    bool DoUnbind(int winid,
                      int lastId,
                      wxEventType eventType,
                      const wxEventFunctor& func,
                      wxObject *userData = NULL);

    static const wxEventTableEntry sm_eventTableEntries[];

protected:
    // hooks for wxWindow used by ProcessEvent()
    // -----------------------------------------

    // this one is called before trying our own event table to allow plugging
    // in the event handlers overriding the default logic, this is used by e.g.
    // validators.
    virtual bool TryBefore(wxEvent& event);

    // This one is not a hook but just a helper which looks up the handler in
    // this object itself.
    //
    // It is called from ProcessEventLocally() and normally shouldn't be called
    // directly as doing it would ignore any chained event handlers
    bool TryHereOnly(wxEvent& event);

    // Another helper which simply calls pre-processing hook and then tries to
    // handle the event at this handler level.
    bool TryBeforeAndHere(wxEvent& event)
    {
        return TryBefore(event) || TryHereOnly(event);
    }

    // this one is called after failing to find the event handle in our own
    // table to give a chance to the other windows to process it
    //
    // base class implementation passes the event to wxTheApp
    virtual bool TryAfter(wxEvent& event);

#if WXWIN_COMPATIBILITY_2_8
    // deprecated method: override TryBefore() instead of this one
    wxDEPRECATED_BUT_USED_INTERNALLY_INLINE(
        virtual bool TryValidator(wxEvent& WXUNUSED(event)), return false; )

    wxDEPRECATED_BUT_USED_INTERNALLY_INLINE(
        virtual bool TryParent(wxEvent& event), return DoTryApp(event); )
#endif // WXWIN_COMPATIBILITY_2_8


    static const wxEventTable sm_eventTable;
    virtual const wxEventTable *GetEventTable() const;

    static wxEventHashTable   sm_eventHashTable;
    virtual wxEventHashTable& GetEventHashTable() const;

    wxEvtHandler*       m_nextHandler;
    wxEvtHandler*       m_previousHandler;
    wxList*             m_dynamicEvents;
    wxList*             m_pendingEvents;

#if wxUSE_THREADS
    // critical section protecting m_pendingEvents
    wxCriticalSection m_pendingEventsLock;
#endif // wxUSE_THREADS

    // Is event handler enabled?
    bool                m_enabled;


    // The user data: either an object which will be deleted by the container
    // when it's deleted or some raw pointer which we do nothing with - only
    // one type of data can be used with the given window (i.e. you cannot set
    // the void data and then associate the container with wxClientData or vice
    // versa)
    union
    {
        wxClientData *m_clientObject;
        void         *m_clientData;
    };

    // what kind of data do we have?
    wxClientDataType m_clientDataType;

    // client data accessors
    virtual void DoSetClientObject( wxClientData *data );
    virtual wxClientData *DoGetClientObject() const;

    virtual void DoSetClientData( void *data );
    virtual void *DoGetClientData() const;

    // Search tracker objects for event connection with this sink
    wxEventConnectionRef *FindRefInTrackerList(wxEvtHandler *handler);

private:
    // pass the event to wxTheApp instance, called from TryAfter()
    bool DoTryApp(wxEvent& event);

    // try to process events in all handlers chained to this one
    bool DoTryChain(wxEvent& event);

    // Head of the event filter linked list.
    static wxEventFilter* ms_filterList;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxEvtHandler)
};

WX_DEFINE_ARRAY_WITH_DECL_PTR(wxEvtHandler *, wxEvtHandlerArray, class WXDLLIMPEXP_BASE);


// Define an inline method of wxObjectEventFunctor which couldn't be defined
// before wxEvtHandler declaration: at least Sun CC refuses to compile function
// calls through pointer to member for forward-declared classes (see #12452).
inline void wxObjectEventFunctor::operator()(wxEvtHandler *handler, wxEvent& event)
{
    wxEvtHandler * const realHandler = m_handler ? m_handler : handler;

    (realHandler->*m_method)(event);
}

// ----------------------------------------------------------------------------
// wxEventConnectionRef represents all connections between two event handlers
// and enables automatic disconnect when an event handler sink goes out of
// scope. Each connection/disconnect increases/decreases ref count, and
// when it reaches zero the node goes out of scope.
// ----------------------------------------------------------------------------

class wxEventConnectionRef : public wxTrackerNode
{
public:
    wxEventConnectionRef() : m_src(NULL), m_sink(NULL), m_refCount(0) { }
    wxEventConnectionRef(wxEvtHandler *src, wxEvtHandler *sink)
        : m_src(src), m_sink(sink), m_refCount(1)
    {
        m_sink->AddNode(this);
    }

    // The sink is being destroyed
    virtual void OnObjectDestroy( )
    {
        if ( m_src )
            m_src->OnSinkDestroyed( m_sink );
        delete this;
    }

    virtual wxEventConnectionRef *ToEventConnection() { return this; }

    void IncRef() { m_refCount++; }
    void DecRef()
    {
        if ( !--m_refCount )
        {
            // The sink holds the only external pointer to this object
            if ( m_sink )
                m_sink->RemoveNode(this);
            delete this;
        }
    }

private:
    wxEvtHandler *m_src,
                 *m_sink;
    int m_refCount;

    friend class wxEvtHandler;

    wxDECLARE_NO_ASSIGN_CLASS(wxEventConnectionRef);
};

// Post a message to the given event handler which will be processed during the
// next event loop iteration.
//
// Notice that this one is not thread-safe, use wxQueueEvent()
inline void wxPostEvent(wxEvtHandler *dest, const wxEvent& event)
{
    wxCHECK_RET( dest, "need an object to post event to" );

    dest->AddPendingEvent(event);
}

// Wrapper around wxEvtHandler::QueueEvent(): adds an event for later
// processing, unlike wxPostEvent it is safe to use from different thread even
// for events with wxString members
inline void wxQueueEvent(wxEvtHandler *dest, wxEvent *event)
{
    wxCHECK_RET( dest, "need an object to queue event for" );

    dest->QueueEvent(event);
}

typedef void (wxEvtHandler::*wxEventFunction)(wxEvent&);
typedef void (wxEvtHandler::*wxIdleEventFunction)(wxIdleEvent&);
typedef void (wxEvtHandler::*wxThreadEventFunction)(wxThreadEvent&);

#define wxEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxEventFunction, func)
#define wxIdleEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxIdleEventFunction, func)
#define wxThreadEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxThreadEventFunction, func)

// N.B. In GNU-WIN32, you *have* to take the address of a member function
// (use &) or the compiler crashes...

#define wxDECLARE_EVENT_TABLE()                                         \
    private:                                                            \
        static const wxEventTableEntry sm_eventTableEntries[];          \
    protected:                                                          \
        static const wxEventTable        sm_eventTable;                 \
        virtual const wxEventTable*      GetEventTable() const;         \
        static wxEventHashTable          sm_eventHashTable;             \
        virtual wxEventHashTable&        GetEventHashTable() const

// N.B.: when building DLL with Borland C++ 5.5 compiler, you must initialize
//       sm_eventTable before using it in GetEventTable() or the compiler gives
//       E2233 (see http://groups.google.com/groups?selm=397dcc8a%241_2%40dnews)

#define wxBEGIN_EVENT_TABLE(theClass, baseClass) \
    const wxEventTable theClass::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass::sm_eventTableEntries[0] }; \
    const wxEventTable *theClass::GetEventTable() const \
        { return &theClass::sm_eventTable; } \
    wxEventHashTable theClass::sm_eventHashTable(theClass::sm_eventTable); \
    wxEventHashTable &theClass::GetEventHashTable() const \
        { return theClass::sm_eventHashTable; } \
    const wxEventTableEntry theClass::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE1(theClass, baseClass, T1) \
    template<typename T1> \
    const wxEventTable theClass<T1>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1>::sm_eventTableEntries[0] }; \
    template<typename T1> \
    const wxEventTable *theClass<T1>::GetEventTable() const \
        { return &theClass<T1>::sm_eventTable; } \
    template<typename T1> \
    wxEventHashTable theClass<T1>::sm_eventHashTable(theClass<T1>::sm_eventTable); \
    template<typename T1> \
    wxEventHashTable &theClass<T1>::GetEventHashTable() const \
        { return theClass<T1>::sm_eventHashTable; } \
    template<typename T1> \
    const wxEventTableEntry theClass<T1>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE2(theClass, baseClass, T1, T2) \
    template<typename T1, typename T2> \
    const wxEventTable theClass<T1, T2>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2> \
    const wxEventTable *theClass<T1, T2>::GetEventTable() const \
        { return &theClass<T1, T2>::sm_eventTable; } \
    template<typename T1, typename T2> \
    wxEventHashTable theClass<T1, T2>::sm_eventHashTable(theClass<T1, T2>::sm_eventTable); \
    template<typename T1, typename T2> \
    wxEventHashTable &theClass<T1, T2>::GetEventHashTable() const \
        { return theClass<T1, T2>::sm_eventHashTable; } \
    template<typename T1, typename T2> \
    const wxEventTableEntry theClass<T1, T2>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE3(theClass, baseClass, T1, T2, T3) \
    template<typename T1, typename T2, typename T3> \
    const wxEventTable theClass<T1, T2, T3>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3> \
    const wxEventTable *theClass<T1, T2, T3>::GetEventTable() const \
        { return &theClass<T1, T2, T3>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3> \
    wxEventHashTable theClass<T1, T2, T3>::sm_eventHashTable(theClass<T1, T2, T3>::sm_eventTable); \
    template<typename T1, typename T2, typename T3> \
    wxEventHashTable &theClass<T1, T2, T3>::GetEventHashTable() const \
        { return theClass<T1, T2, T3>::sm_eventHashTable; } \
    template<typename T1, typename T2, typename T3> \
    const wxEventTableEntry theClass<T1, T2, T3>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE4(theClass, baseClass, T1, T2, T3, T4) \
    template<typename T1, typename T2, typename T3, typename T4> \
    const wxEventTable theClass<T1, T2, T3, T4>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4> \
    const wxEventTable *theClass<T1, T2, T3, T4>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4> \
    wxEventHashTable theClass<T1, T2, T3, T4>::sm_eventHashTable(theClass<T1, T2, T3, T4>::sm_eventTable); \
    template<typename T1, typename T2, typename T3, typename T4> \
    wxEventHashTable &theClass<T1, T2, T3, T4>::GetEventHashTable() const \
        { return theClass<T1, T2, T3, T4>::sm_eventHashTable; } \
    template<typename T1, typename T2, typename T3, typename T4> \
    const wxEventTableEntry theClass<T1, T2, T3, T4>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE5(theClass, baseClass, T1, T2, T3, T4, T5) \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    const wxEventTable theClass<T1, T2, T3, T4, T5>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4, T5>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    const wxEventTable *theClass<T1, T2, T3, T4, T5>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4, T5>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    wxEventHashTable theClass<T1, T2, T3, T4, T5>::sm_eventHashTable(theClass<T1, T2, T3, T4, T5>::sm_eventTable); \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    wxEventHashTable &theClass<T1, T2, T3, T4, T5>::GetEventHashTable() const \
        { return theClass<T1, T2, T3, T4, T5>::sm_eventHashTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    const wxEventTableEntry theClass<T1, T2, T3, T4, T5>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE7(theClass, baseClass, T1, T2, T3, T4, T5, T6, T7) \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    const wxEventTable theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    const wxEventTable *theClass<T1, T2, T3, T4, T5, T6, T7>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    wxEventHashTable theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventHashTable(theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTable); \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    wxEventHashTable &theClass<T1, T2, T3, T4, T5, T6, T7>::GetEventHashTable() const \
        { return theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventHashTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
    const wxEventTableEntry theClass<T1, T2, T3, T4, T5, T6, T7>::sm_eventTableEntries[] = { \

#define wxBEGIN_EVENT_TABLE_TEMPLATE8(theClass, baseClass, T1, T2, T3, T4, T5, T6, T7, T8) \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    const wxEventTable theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTableEntries[0] }; \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    const wxEventTable *theClass<T1, T2, T3, T4, T5, T6, T7, T8>::GetEventTable() const \
        { return &theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    wxEventHashTable theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventHashTable(theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTable); \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    wxEventHashTable &theClass<T1, T2, T3, T4, T5, T6, T7, T8>::GetEventHashTable() const \
        { return theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventHashTable; } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
    const wxEventTableEntry theClass<T1, T2, T3, T4, T5, T6, T7, T8>::sm_eventTableEntries[] = { \

#define wxEND_EVENT_TABLE() \
    wxDECLARE_EVENT_TABLE_TERMINATOR() };

/*
 * Event table macros
 */

// helpers for writing shorter code below: declare an event macro taking 2, 1
// or none ids (the missing ids default to wxID_ANY)
//
// macro arguments:
//  - evt one of wxEVT_XXX constants
//  - id1, id2 ids of the first/last id
//  - fn the function (should be cast to the right type)
#define wx__DECLARE_EVT2(evt, id1, id2, fn) \
    wxDECLARE_EVENT_TABLE_ENTRY(evt, id1, id2, fn, NULL),
#define wx__DECLARE_EVT1(evt, id, fn) \
    wx__DECLARE_EVT2(evt, id, wxID_ANY, fn)
#define wx__DECLARE_EVT0(evt, fn) \
    wx__DECLARE_EVT1(evt, wxID_ANY, fn)


// Generic events
#define EVT_CUSTOM(event, winid, func) \
    wx__DECLARE_EVT1(event, winid, wxEventHandler(func))
#define EVT_CUSTOM_RANGE(event, id1, id2, func) \
    wx__DECLARE_EVT2(event, id1, id2, wxEventHandler(func))

// EVT_COMMAND
#define EVT_COMMAND(winid, event, func) \
    wx__DECLARE_EVT1(event, winid, wxCommandEventHandler(func))

#define EVT_COMMAND_RANGE(id1, id2, event, func) \
    wx__DECLARE_EVT2(event, id1, id2, wxCommandEventHandler(func))

#define EVT_NOTIFY(event, winid, func) \
    wx__DECLARE_EVT1(event, winid, wxNotifyEventHandler(func))

#define EVT_NOTIFY_RANGE(event, id1, id2, func) \
    wx__DECLARE_EVT2(event, id1, id2, wxNotifyEventHandler(func))

// Miscellaneous
#define EVT_SIZE(func)  wx__DECLARE_EVT0(wxEVT_SIZE, wxSizeEventHandler(func))
#define EVT_SIZING(func)  wx__DECLARE_EVT0(wxEVT_SIZING, wxSizeEventHandler(func))
#define EVT_MOVE(func)  wx__DECLARE_EVT0(wxEVT_MOVE, wxMoveEventHandler(func))
#define EVT_MOVING(func)  wx__DECLARE_EVT0(wxEVT_MOVING, wxMoveEventHandler(func))
#define EVT_MOVE_START(func)  wx__DECLARE_EVT0(wxEVT_MOVE_START, wxMoveEventHandler(func))
#define EVT_MOVE_END(func)  wx__DECLARE_EVT0(wxEVT_MOVE_END, wxMoveEventHandler(func))
#define EVT_CLOSE(func)  wx__DECLARE_EVT0(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(func))
#define EVT_END_SESSION(func)  wx__DECLARE_EVT0(wxEVT_END_SESSION, wxCloseEventHandler(func))
#define EVT_QUERY_END_SESSION(func)  wx__DECLARE_EVT0(wxEVT_QUERY_END_SESSION, wxCloseEventHandler(func))
#define EVT_PAINT(func)  wx__DECLARE_EVT0(wxEVT_PAINT, wxPaintEventHandler(func))
#define EVT_NC_PAINT(func)  wx__DECLARE_EVT0(wxEVT_NC_PAINT, wxNcPaintEventHandler(func))
#define EVT_ERASE_BACKGROUND(func)  wx__DECLARE_EVT0(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(func))
#define EVT_CHAR(func)  wx__DECLARE_EVT0(wxEVT_CHAR, wxCharEventHandler(func))
#define EVT_KEY_DOWN(func)  wx__DECLARE_EVT0(wxEVT_KEY_DOWN, wxKeyEventHandler(func))
#define EVT_KEY_UP(func)  wx__DECLARE_EVT0(wxEVT_KEY_UP, wxKeyEventHandler(func))
#define EVT_CHAR_HOOK(func)  wx__DECLARE_EVT0(wxEVT_CHAR_HOOK, wxCharEventHandler(func))
#define EVT_MENU_OPEN(func)  wx__DECLARE_EVT0(wxEVT_MENU_OPEN, wxMenuEventHandler(func))
#define EVT_MENU_CLOSE(func)  wx__DECLARE_EVT0(wxEVT_MENU_CLOSE, wxMenuEventHandler(func))
#define EVT_MENU_HIGHLIGHT(winid, func)  wx__DECLARE_EVT1(wxEVT_MENU_HIGHLIGHT, winid, wxMenuEventHandler(func))
#define EVT_MENU_HIGHLIGHT_ALL(func)  wx__DECLARE_EVT0(wxEVT_MENU_HIGHLIGHT, wxMenuEventHandler(func))
#define EVT_SET_FOCUS(func)  wx__DECLARE_EVT0(wxEVT_SET_FOCUS, wxFocusEventHandler(func))
#define EVT_KILL_FOCUS(func)  wx__DECLARE_EVT0(wxEVT_KILL_FOCUS, wxFocusEventHandler(func))
#define EVT_CHILD_FOCUS(func)  wx__DECLARE_EVT0(wxEVT_CHILD_FOCUS, wxChildFocusEventHandler(func))
#define EVT_ACTIVATE(func)  wx__DECLARE_EVT0(wxEVT_ACTIVATE, wxActivateEventHandler(func))
#define EVT_ACTIVATE_APP(func)  wx__DECLARE_EVT0(wxEVT_ACTIVATE_APP, wxActivateEventHandler(func))
#define EVT_HIBERNATE(func)  wx__DECLARE_EVT0(wxEVT_HIBERNATE, wxActivateEventHandler(func))
#define EVT_END_SESSION(func)  wx__DECLARE_EVT0(wxEVT_END_SESSION, wxCloseEventHandler(func))
#define EVT_QUERY_END_SESSION(func)  wx__DECLARE_EVT0(wxEVT_QUERY_END_SESSION, wxCloseEventHandler(func))
#define EVT_DROP_FILES(func)  wx__DECLARE_EVT0(wxEVT_DROP_FILES, wxDropFilesEventHandler(func))
#define EVT_INIT_DIALOG(func)  wx__DECLARE_EVT0(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(func))
#define EVT_SYS_COLOUR_CHANGED(func) wx__DECLARE_EVT0(wxEVT_SYS_COLOUR_CHANGED, wxSysColourChangedEventHandler(func))
#define EVT_DISPLAY_CHANGED(func)  wx__DECLARE_EVT0(wxEVT_DISPLAY_CHANGED, wxDisplayChangedEventHandler(func))
#define EVT_SHOW(func) wx__DECLARE_EVT0(wxEVT_SHOW, wxShowEventHandler(func))
#define EVT_MAXIMIZE(func) wx__DECLARE_EVT0(wxEVT_MAXIMIZE, wxMaximizeEventHandler(func))
#define EVT_ICONIZE(func) wx__DECLARE_EVT0(wxEVT_ICONIZE, wxIconizeEventHandler(func))
#define EVT_NAVIGATION_KEY(func) wx__DECLARE_EVT0(wxEVT_NAVIGATION_KEY, wxNavigationKeyEventHandler(func))
#define EVT_PALETTE_CHANGED(func) wx__DECLARE_EVT0(wxEVT_PALETTE_CHANGED, wxPaletteChangedEventHandler(func))
#define EVT_QUERY_NEW_PALETTE(func) wx__DECLARE_EVT0(wxEVT_QUERY_NEW_PALETTE, wxQueryNewPaletteEventHandler(func))
#define EVT_WINDOW_CREATE(func) wx__DECLARE_EVT0(wxEVT_CREATE, wxWindowCreateEventHandler(func))
#define EVT_WINDOW_DESTROY(func) wx__DECLARE_EVT0(wxEVT_DESTROY, wxWindowDestroyEventHandler(func))
#define EVT_SET_CURSOR(func) wx__DECLARE_EVT0(wxEVT_SET_CURSOR, wxSetCursorEventHandler(func))
#define EVT_MOUSE_CAPTURE_CHANGED(func) wx__DECLARE_EVT0(wxEVT_MOUSE_CAPTURE_CHANGED, wxMouseCaptureChangedEventHandler(func))
#define EVT_MOUSE_CAPTURE_LOST(func) wx__DECLARE_EVT0(wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEventHandler(func))

// Mouse events
#define EVT_LEFT_DOWN(func) wx__DECLARE_EVT0(wxEVT_LEFT_DOWN, wxMouseEventHandler(func))
#define EVT_LEFT_UP(func) wx__DECLARE_EVT0(wxEVT_LEFT_UP, wxMouseEventHandler(func))
#define EVT_MIDDLE_DOWN(func) wx__DECLARE_EVT0(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(func))
#define EVT_MIDDLE_UP(func) wx__DECLARE_EVT0(wxEVT_MIDDLE_UP, wxMouseEventHandler(func))
#define EVT_RIGHT_DOWN(func) wx__DECLARE_EVT0(wxEVT_RIGHT_DOWN, wxMouseEventHandler(func))
#define EVT_RIGHT_UP(func) wx__DECLARE_EVT0(wxEVT_RIGHT_UP, wxMouseEventHandler(func))
#define EVT_MOTION(func) wx__DECLARE_EVT0(wxEVT_MOTION, wxMouseEventHandler(func))
#define EVT_LEFT_DCLICK(func) wx__DECLARE_EVT0(wxEVT_LEFT_DCLICK, wxMouseEventHandler(func))
#define EVT_MIDDLE_DCLICK(func) wx__DECLARE_EVT0(wxEVT_MIDDLE_DCLICK, wxMouseEventHandler(func))
#define EVT_RIGHT_DCLICK(func) wx__DECLARE_EVT0(wxEVT_RIGHT_DCLICK, wxMouseEventHandler(func))
#define EVT_LEAVE_WINDOW(func) wx__DECLARE_EVT0(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(func))
#define EVT_ENTER_WINDOW(func) wx__DECLARE_EVT0(wxEVT_ENTER_WINDOW, wxMouseEventHandler(func))
#define EVT_MOUSEWHEEL(func) wx__DECLARE_EVT0(wxEVT_MOUSEWHEEL, wxMouseEventHandler(func))
#define EVT_MOUSE_AUX1_DOWN(func) wx__DECLARE_EVT0(wxEVT_AUX1_DOWN, wxMouseEventHandler(func))
#define EVT_MOUSE_AUX1_UP(func) wx__DECLARE_EVT0(wxEVT_AUX1_UP, wxMouseEventHandler(func))
#define EVT_MOUSE_AUX1_DCLICK(func) wx__DECLARE_EVT0(wxEVT_AUX1_DCLICK, wxMouseEventHandler(func))
#define EVT_MOUSE_AUX2_DOWN(func) wx__DECLARE_EVT0(wxEVT_AUX2_DOWN, wxMouseEventHandler(func))
#define EVT_MOUSE_AUX2_UP(func) wx__DECLARE_EVT0(wxEVT_AUX2_UP, wxMouseEventHandler(func))
#define EVT_MOUSE_AUX2_DCLICK(func) wx__DECLARE_EVT0(wxEVT_AUX2_DCLICK, wxMouseEventHandler(func))

// All mouse events
#define EVT_MOUSE_EVENTS(func) \
    EVT_LEFT_DOWN(func) \
    EVT_LEFT_UP(func) \
    EVT_LEFT_DCLICK(func) \
    EVT_MIDDLE_DOWN(func) \
    EVT_MIDDLE_UP(func) \
    EVT_MIDDLE_DCLICK(func) \
    EVT_RIGHT_DOWN(func) \
    EVT_RIGHT_UP(func) \
    EVT_RIGHT_DCLICK(func) \
    EVT_MOUSE_AUX1_DOWN(func) \
    EVT_MOUSE_AUX1_UP(func) \
    EVT_MOUSE_AUX1_DCLICK(func) \
    EVT_MOUSE_AUX2_DOWN(func) \
    EVT_MOUSE_AUX2_UP(func) \
    EVT_MOUSE_AUX2_DCLICK(func) \
    EVT_MOTION(func) \
    EVT_LEAVE_WINDOW(func) \
    EVT_ENTER_WINDOW(func) \
    EVT_MOUSEWHEEL(func)

// Scrolling from wxWindow (sent to wxScrolledWindow)
#define EVT_SCROLLWIN_TOP(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_TOP, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_BOTTOM(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_BOTTOM, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_LINEUP(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_LINEUP, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_LINEDOWN(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_LINEDOWN, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_PAGEUP(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_PAGEUP, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_PAGEDOWN(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_PAGEDOWN, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_THUMBTRACK(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_THUMBTRACK, wxScrollWinEventHandler(func))
#define EVT_SCROLLWIN_THUMBRELEASE(func) wx__DECLARE_EVT0(wxEVT_SCROLLWIN_THUMBRELEASE, wxScrollWinEventHandler(func))

#define EVT_SCROLLWIN(func) \
    EVT_SCROLLWIN_TOP(func) \
    EVT_SCROLLWIN_BOTTOM(func) \
    EVT_SCROLLWIN_LINEUP(func) \
    EVT_SCROLLWIN_LINEDOWN(func) \
    EVT_SCROLLWIN_PAGEUP(func) \
    EVT_SCROLLWIN_PAGEDOWN(func) \
    EVT_SCROLLWIN_THUMBTRACK(func) \
    EVT_SCROLLWIN_THUMBRELEASE(func)

// Scrolling from wxSlider and wxScrollBar
#define EVT_SCROLL_TOP(func) wx__DECLARE_EVT0(wxEVT_SCROLL_TOP, wxScrollEventHandler(func))
#define EVT_SCROLL_BOTTOM(func) wx__DECLARE_EVT0(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(func))
#define EVT_SCROLL_LINEUP(func) wx__DECLARE_EVT0(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(func))
#define EVT_SCROLL_LINEDOWN(func) wx__DECLARE_EVT0(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(func))
#define EVT_SCROLL_PAGEUP(func) wx__DECLARE_EVT0(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(func))
#define EVT_SCROLL_PAGEDOWN(func) wx__DECLARE_EVT0(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(func))
#define EVT_SCROLL_THUMBTRACK(func) wx__DECLARE_EVT0(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(func))
#define EVT_SCROLL_THUMBRELEASE(func) wx__DECLARE_EVT0(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(func))
#define EVT_SCROLL_CHANGED(func) wx__DECLARE_EVT0(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(func))

#define EVT_SCROLL(func) \
    EVT_SCROLL_TOP(func) \
    EVT_SCROLL_BOTTOM(func) \
    EVT_SCROLL_LINEUP(func) \
    EVT_SCROLL_LINEDOWN(func) \
    EVT_SCROLL_PAGEUP(func) \
    EVT_SCROLL_PAGEDOWN(func) \
    EVT_SCROLL_THUMBTRACK(func) \
    EVT_SCROLL_THUMBRELEASE(func) \
    EVT_SCROLL_CHANGED(func)

// Scrolling from wxSlider and wxScrollBar, with an id
#define EVT_COMMAND_SCROLL_TOP(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_TOP, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_BOTTOM(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_BOTTOM, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_LINEUP(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_LINEUP, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_LINEDOWN(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_LINEDOWN, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_PAGEUP(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_PAGEUP, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_PAGEDOWN(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_PAGEDOWN, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_THUMBTRACK(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_THUMBTRACK, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_THUMBRELEASE(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_THUMBRELEASE, winid, wxScrollEventHandler(func))
#define EVT_COMMAND_SCROLL_CHANGED(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLL_CHANGED, winid, wxScrollEventHandler(func))

#define EVT_COMMAND_SCROLL(winid, func) \
    EVT_COMMAND_SCROLL_TOP(winid, func) \
    EVT_COMMAND_SCROLL_BOTTOM(winid, func) \
    EVT_COMMAND_SCROLL_LINEUP(winid, func) \
    EVT_COMMAND_SCROLL_LINEDOWN(winid, func) \
    EVT_COMMAND_SCROLL_PAGEUP(winid, func) \
    EVT_COMMAND_SCROLL_PAGEDOWN(winid, func) \
    EVT_COMMAND_SCROLL_THUMBTRACK(winid, func) \
    EVT_COMMAND_SCROLL_THUMBRELEASE(winid, func) \
    EVT_COMMAND_SCROLL_CHANGED(winid, func)

#if WXWIN_COMPATIBILITY_2_6
    // compatibility macros for the old name, deprecated in 2.8
    #define wxEVT_SCROLL_ENDSCROLL wxEVT_SCROLL_CHANGED
    #define EVT_COMMAND_SCROLL_ENDSCROLL EVT_COMMAND_SCROLL_CHANGED
    #define EVT_SCROLL_ENDSCROLL EVT_SCROLL_CHANGED
#endif // WXWIN_COMPATIBILITY_2_6

// Convenience macros for commonly-used commands
#define EVT_CHECKBOX(winid, func) wx__DECLARE_EVT1(wxEVT_CHECKBOX, winid, wxCommandEventHandler(func))
#define EVT_CHOICE(winid, func) wx__DECLARE_EVT1(wxEVT_CHOICE, winid, wxCommandEventHandler(func))
#define EVT_LISTBOX(winid, func) wx__DECLARE_EVT1(wxEVT_LISTBOX, winid, wxCommandEventHandler(func))
#define EVT_LISTBOX_DCLICK(winid, func) wx__DECLARE_EVT1(wxEVT_LISTBOX_DCLICK, winid, wxCommandEventHandler(func))
#define EVT_MENU(winid, func) wx__DECLARE_EVT1(wxEVT_MENU, winid, wxCommandEventHandler(func))
#define EVT_MENU_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_MENU, id1, id2, wxCommandEventHandler(func))
#if defined(__SMARTPHONE__)
#  define EVT_BUTTON(winid, func) EVT_MENU(winid, func)
#else
#  define EVT_BUTTON(winid, func) wx__DECLARE_EVT1(wxEVT_BUTTON, winid, wxCommandEventHandler(func))
#endif
#define EVT_SLIDER(winid, func) wx__DECLARE_EVT1(wxEVT_SLIDER, winid, wxCommandEventHandler(func))
#define EVT_RADIOBOX(winid, func) wx__DECLARE_EVT1(wxEVT_RADIOBOX, winid, wxCommandEventHandler(func))
#define EVT_RADIOBUTTON(winid, func) wx__DECLARE_EVT1(wxEVT_RADIOBUTTON, winid, wxCommandEventHandler(func))
// EVT_SCROLLBAR is now obsolete since we use EVT_COMMAND_SCROLL... events
#define EVT_SCROLLBAR(winid, func) wx__DECLARE_EVT1(wxEVT_SCROLLBAR, winid, wxCommandEventHandler(func))
#define EVT_VLBOX(winid, func) wx__DECLARE_EVT1(wxEVT_VLBOX, winid, wxCommandEventHandler(func))
#define EVT_COMBOBOX(winid, func) wx__DECLARE_EVT1(wxEVT_COMBOBOX, winid, wxCommandEventHandler(func))
#define EVT_TOOL(winid, func) wx__DECLARE_EVT1(wxEVT_TOOL, winid, wxCommandEventHandler(func))
#define EVT_TOOL_DROPDOWN(winid, func) wx__DECLARE_EVT1(wxEVT_TOOL_DROPDOWN, winid, wxCommandEventHandler(func))
#define EVT_TOOL_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_TOOL, id1, id2, wxCommandEventHandler(func))
#define EVT_TOOL_RCLICKED(winid, func) wx__DECLARE_EVT1(wxEVT_TOOL_RCLICKED, winid, wxCommandEventHandler(func))
#define EVT_TOOL_RCLICKED_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_TOOL_RCLICKED, id1, id2, wxCommandEventHandler(func))
#define EVT_TOOL_ENTER(winid, func) wx__DECLARE_EVT1(wxEVT_TOOL_ENTER, winid, wxCommandEventHandler(func))
#define EVT_CHECKLISTBOX(winid, func) wx__DECLARE_EVT1(wxEVT_CHECKLISTBOX, winid, wxCommandEventHandler(func))
#define EVT_COMBOBOX_DROPDOWN(winid, func) wx__DECLARE_EVT1(wxEVT_COMBOBOX_DROPDOWN, winid, wxCommandEventHandler(func))
#define EVT_COMBOBOX_CLOSEUP(winid, func) wx__DECLARE_EVT1(wxEVT_COMBOBOX_CLOSEUP, winid, wxCommandEventHandler(func))

// Generic command events
#define EVT_COMMAND_LEFT_CLICK(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_LEFT_CLICK, winid, wxCommandEventHandler(func))
#define EVT_COMMAND_LEFT_DCLICK(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_LEFT_DCLICK, winid, wxCommandEventHandler(func))
#define EVT_COMMAND_RIGHT_CLICK(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_RIGHT_CLICK, winid, wxCommandEventHandler(func))
#define EVT_COMMAND_RIGHT_DCLICK(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_RIGHT_DCLICK, winid, wxCommandEventHandler(func))
#define EVT_COMMAND_SET_FOCUS(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_SET_FOCUS, winid, wxCommandEventHandler(func))
#define EVT_COMMAND_KILL_FOCUS(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_KILL_FOCUS, winid, wxCommandEventHandler(func))
#define EVT_COMMAND_ENTER(winid, func) wx__DECLARE_EVT1(wxEVT_COMMAND_ENTER, winid, wxCommandEventHandler(func))

// Joystick events

#define EVT_JOY_BUTTON_DOWN(func) wx__DECLARE_EVT0(wxEVT_JOY_BUTTON_DOWN, wxJoystickEventHandler(func))
#define EVT_JOY_BUTTON_UP(func) wx__DECLARE_EVT0(wxEVT_JOY_BUTTON_UP, wxJoystickEventHandler(func))
#define EVT_JOY_MOVE(func) wx__DECLARE_EVT0(wxEVT_JOY_MOVE, wxJoystickEventHandler(func))
#define EVT_JOY_ZMOVE(func) wx__DECLARE_EVT0(wxEVT_JOY_ZMOVE, wxJoystickEventHandler(func))

// All joystick events
#define EVT_JOYSTICK_EVENTS(func) \
    EVT_JOY_BUTTON_DOWN(func) \
    EVT_JOY_BUTTON_UP(func) \
    EVT_JOY_MOVE(func) \
    EVT_JOY_ZMOVE(func)

// Idle event
#define EVT_IDLE(func) wx__DECLARE_EVT0(wxEVT_IDLE, wxIdleEventHandler(func))

// Update UI event
#define EVT_UPDATE_UI(winid, func) wx__DECLARE_EVT1(wxEVT_UPDATE_UI, winid, wxUpdateUIEventHandler(func))
#define EVT_UPDATE_UI_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_UPDATE_UI, id1, id2, wxUpdateUIEventHandler(func))

// Help events
#define EVT_HELP(winid, func) wx__DECLARE_EVT1(wxEVT_HELP, winid, wxHelpEventHandler(func))
#define EVT_HELP_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_HELP, id1, id2, wxHelpEventHandler(func))
#define EVT_DETAILED_HELP(winid, func) wx__DECLARE_EVT1(wxEVT_DETAILED_HELP, winid, wxHelpEventHandler(func))
#define EVT_DETAILED_HELP_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_DETAILED_HELP, id1, id2, wxHelpEventHandler(func))

// Context Menu Events
#define EVT_CONTEXT_MENU(func) wx__DECLARE_EVT0(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(func))
#define EVT_COMMAND_CONTEXT_MENU(winid, func) wx__DECLARE_EVT1(wxEVT_CONTEXT_MENU, winid, wxContextMenuEventHandler(func))

// Clipboard text Events
#define EVT_TEXT_CUT(winid, func) wx__DECLARE_EVT1(wxEVT_TEXT_CUT, winid, wxClipboardTextEventHandler(func))
#define EVT_TEXT_COPY(winid, func) wx__DECLARE_EVT1(wxEVT_TEXT_COPY, winid, wxClipboardTextEventHandler(func))
#define EVT_TEXT_PASTE(winid, func) wx__DECLARE_EVT1(wxEVT_TEXT_PASTE, winid, wxClipboardTextEventHandler(func))

// Thread events
#define EVT_THREAD(id, func)  wx__DECLARE_EVT1(wxEVT_THREAD, id, wxThreadEventHandler(func))

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

// This is an ugly hack to allow the use of Bind() instead of Connect() inside
// the library code if the library was built with support for it, here is how
// it is used:
//
// class SomeEventHandlingClass : wxBIND_OR_CONNECT_HACK_BASE_CLASS
//                                public SomeBaseClass
// {
// public:
//     SomeEventHandlingClass(wxWindow *win)
//     {
//         // connect to the event for the given window
//         wxBIND_OR_CONNECT_HACK(win, wxEVT_SOMETHING, wxSomeEventHandler,
//                                SomeEventHandlingClass::OnSomeEvent, this);
//     }
//
// private:
//     void OnSomeEvent(wxSomeEvent&) { ... }
// };
//
// This is *not* meant to be used by library users, it is only defined here
// (and not in a private header) because the base class must be visible from
// other public headers, please do NOT use this in your code, it will be
// removed from future wx versions without warning.
    #define wxBIND_OR_CONNECT_HACK_BASE_CLASS public wxEvtHandler,
    #define wxBIND_OR_CONNECT_HACK_ONLY_BASE_CLASS : public wxEvtHandler
    #define wxBIND_OR_CONNECT_HACK(win, evt, handler, func, obj) \
        win->Connect(evt, handler(func), NULL, obj)

// ----------------------------------------------------------------------------
// Compatibility macro aliases
// ----------------------------------------------------------------------------

// deprecated variants _not_ requiring a semicolon after them and without wx prefix
// (note that also some wx-prefixed macro do _not_ require a semicolon because
//  it's not always possible to force the compire to require it)

#define DECLARE_EVENT_TABLE_ENTRY(type, winid, idLast, fn, obj) \
    wxDECLARE_EVENT_TABLE_ENTRY(type, winid, idLast, fn, obj)
#define DECLARE_EVENT_TABLE_TERMINATOR()               wxDECLARE_EVENT_TABLE_TERMINATOR()
#define DECLARE_EVENT_TABLE()                          wxDECLARE_EVENT_TABLE();
#define BEGIN_EVENT_TABLE(a,b)                         wxBEGIN_EVENT_TABLE(a,b)
#define BEGIN_EVENT_TABLE_TEMPLATE1(a,b,c)             wxBEGIN_EVENT_TABLE_TEMPLATE1(a,b,c)
#define BEGIN_EVENT_TABLE_TEMPLATE2(a,b,c,d)           wxBEGIN_EVENT_TABLE_TEMPLATE2(a,b,c,d)
#define BEGIN_EVENT_TABLE_TEMPLATE3(a,b,c,d,e)         wxBEGIN_EVENT_TABLE_TEMPLATE3(a,b,c,d,e)
#define BEGIN_EVENT_TABLE_TEMPLATE4(a,b,c,d,e,f)       wxBEGIN_EVENT_TABLE_TEMPLATE4(a,b,c,d,e,f)
#define BEGIN_EVENT_TABLE_TEMPLATE5(a,b,c,d,e,f,g)     wxBEGIN_EVENT_TABLE_TEMPLATE5(a,b,c,d,e,f,g)
#define BEGIN_EVENT_TABLE_TEMPLATE6(a,b,c,d,e,f,g,h)   wxBEGIN_EVENT_TABLE_TEMPLATE6(a,b,c,d,e,f,g,h)
#define END_EVENT_TABLE()                              wxEND_EVENT_TABLE()

// other obsolete event declaration/definition macros; we don't need them any longer
// but we keep them for compatibility as it doesn't cost us anything anyhow
#define BEGIN_DECLARE_EVENT_TYPES()
#define END_DECLARE_EVENT_TYPES()
#define DECLARE_EXPORTED_EVENT_TYPE(expdecl, name, value) \
    extern expdecl const wxEventType name;
#define DECLARE_EVENT_TYPE(name, value) \
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_CORE, name, value)
#define DECLARE_LOCAL_EVENT_TYPE(name, value) \
    DECLARE_EXPORTED_EVENT_TYPE(wxEMPTY_PARAMETER_VALUE, name, value)
#define DEFINE_EVENT_TYPE(name) const wxEventType name = wxNewEventType();
#define DEFINE_LOCAL_EVENT_TYPE(name) DEFINE_EVENT_TYPE(name)

// alias for backward compatibility with 2.9.0:
#define wxEVT_COMMAND_THREAD                  wxEVT_THREAD
// other old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_BUTTON_CLICKED          wxEVT_BUTTON
#define wxEVT_COMMAND_CHECKBOX_CLICKED        wxEVT_CHECKBOX
#define wxEVT_COMMAND_CHOICE_SELECTED         wxEVT_CHOICE
#define wxEVT_COMMAND_LISTBOX_SELECTED        wxEVT_LISTBOX
#define wxEVT_COMMAND_LISTBOX_DOUBLECLICKED   wxEVT_LISTBOX_DCLICK
#define wxEVT_COMMAND_CHECKLISTBOX_TOGGLED    wxEVT_CHECKLISTBOX
#define wxEVT_COMMAND_MENU_SELECTED           wxEVT_MENU
#define wxEVT_COMMAND_TOOL_CLICKED            wxEVT_TOOL
#define wxEVT_COMMAND_SLIDER_UPDATED          wxEVT_SLIDER
#define wxEVT_COMMAND_RADIOBOX_SELECTED       wxEVT_RADIOBOX
#define wxEVT_COMMAND_RADIOBUTTON_SELECTED    wxEVT_RADIOBUTTON
#define wxEVT_COMMAND_SCROLLBAR_UPDATED       wxEVT_SCROLLBAR
#define wxEVT_COMMAND_VLBOX_SELECTED          wxEVT_VLBOX
#define wxEVT_COMMAND_COMBOBOX_SELECTED       wxEVT_COMBOBOX
#define wxEVT_COMMAND_TOOL_RCLICKED           wxEVT_TOOL_RCLICKED
#define wxEVT_COMMAND_TOOL_DROPDOWN_CLICKED   wxEVT_TOOL_DROPDOWN
#define wxEVT_COMMAND_TOOL_ENTER              wxEVT_TOOL_ENTER
#define wxEVT_COMMAND_COMBOBOX_DROPDOWN       wxEVT_COMBOBOX_DROPDOWN
#define wxEVT_COMMAND_COMBOBOX_CLOSEUP        wxEVT_COMBOBOX_CLOSEUP
#define wxEVT_COMMAND_TEXT_COPY               wxEVT_TEXT_COPY
#define wxEVT_COMMAND_TEXT_CUT                wxEVT_TEXT_CUT
#define wxEVT_COMMAND_TEXT_PASTE              wxEVT_TEXT_PASTE
#define wxEVT_COMMAND_TEXT_UPDATED            wxEVT_TEXT

#endif // _WX_EVENT_H_
