#pragma once
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/propgrid/editors.h>
#include <iostream>
#include <stdlib.h>

class wxSpinOrderCtrlEditor : public wxPGSpinCtrlEditor
{
    wxDECLARE_DYNAMIC_CLASS(wxSpinOrderCtrlEditor);

public:
    wxSpinOrderCtrlEditor() {}
    virtual ~wxSpinOrderCtrlEditor() {}

    virtual wxString GetName() const { return "SpinOrderCtrlEditor"; }

    virtual wxPGWindowList CreateControls(wxPropertyGrid* propGrid,
        wxPGProperty* property,
        const wxPoint& pos,
        const wxSize& sz) const;

    virtual bool OnEvent(wxPropertyGrid* propGrid,
        wxPGProperty* property,
        wxWindow* ctrl,
        wxEvent& event) const;
};