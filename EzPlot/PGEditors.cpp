#include "PGEditors.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxSpinOrderCtrlEditor, wxPGSpinCtrlEditor);

wxPGWindowList wxSpinOrderCtrlEditor::CreateControls(wxPropertyGrid* propGrid,
    wxPGProperty* property,
    const wxPoint& pos,
    const wxSize& sz) const
{
    // Fake int property so normal spin editor buttons are created for other properties:
    wxIntProperty* FakeIntP = new wxIntProperty("FakeIntP", wxPG_LABEL, 0);
    wxPGWindowList wndList = wxPGSpinCtrlEditor::CreateControls(propGrid, FakeIntP, pos, sz);

    // Change validator of property back to default text control validator after creating controls:
    propGrid->SetPropertyValidator(property, wxTextValidator(wxFILTER_NONE));

    return wndList;
}

bool wxSpinOrderCtrlEditor::OnEvent(wxPropertyGrid* propGrid,
    wxPGProperty* property,
    wxWindow* ctrl,
    wxEvent& event) const
{
    if (event.GetEventType() == wxEVT_SPIN_UP)
    {
        wxPropertyGridIterator it;
        for (it = propGrid->GetIterator(); !it.AtEnd(); it++)
        {
            wxPGProperty* Prop = *it;
            if (Prop == property) {
                it--;
                wxPGProperty* LastProp = *it;
                if (LastProp->GetParent() == Prop->GetParent()) {
                    wxVariant Val = Prop->GetValue();
                    wxVariant LastVal = LastProp->GetValue();
                    wxString Label = Prop->GetLabel();
                    wxString LastLabel = LastProp->GetLabel();
                    wxString Name = Prop->GetName();
                    wxString LastName = LastProp->GetName();
                    wxString TrashName = "This is just because equal names are not allowed";
                    Prop->SetLabel(LastLabel);
                    LastProp->SetLabel(Label);
                    Prop->SetValue(LastVal);
                    LastProp->SetValue(Val);
                    Prop->SetName(TrashName);
                    LastProp->SetName(Name);
                    Prop->SetName(LastName);
                }
                break;
            }
        }
    }
    else if (event.GetEventType() == wxEVT_SPIN_DOWN)
    {
        wxPropertyGridIterator it;
        for (it = propGrid->GetIterator(); !it.AtEnd(); it++)
        {
            wxPGProperty* Prop = *it;
            if (Prop == property) {
                it++;
                wxPGProperty* NextProp = *it;
                if (NextProp->GetParent() == Prop->GetParent()) {
                    wxVariant Val = Prop->GetValue();
                    wxVariant NextVal = NextProp->GetValue();
                    wxString Label = Prop->GetLabel();
                    wxString NextLabel = NextProp->GetLabel();
                    wxString Name = Prop->GetName();
                    wxString NextName = NextProp->GetName();
                    wxString TrashName = "This is just because equal names are not allowed";
                    Prop->SetLabel(NextLabel);
                    NextProp->SetLabel(Label);
                    Prop->SetValue(NextVal);
                    NextProp->SetValue(Val);
                    Prop->SetName(TrashName);
                    NextProp->SetName(Name);
                    Prop->SetName(NextName);
                }
                break;
            }
        }
    }
    return wxPGSpinCtrlEditor::OnEvent(propGrid, property, ctrl, event);
}