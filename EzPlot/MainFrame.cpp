#include "MainFrame.h"
#include "GUIConsole.h"
#include "PyUtils.h"
#include "PGEditors.h"
#include <iostream>
#include <map>
#include <string>
#include <regex>
#include <fstream>
#include <wx/wx.h>
#include <wx/artprov.h>
#include <wx/menuitem.h>
#include <wx/filepicker.h>
#include <wx/dynarray.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/choicdlg.h>
#include <wx/valnum.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <winuser.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <windows.h>
#include <stdlib.h> 
#include <math.h>
#include <any>
#include <tuple>
#include <optional>
#include <algorithm>
#include <limits>
#include <filesystem>
#include <wx/activityindicator.h>
//#include <xlnt/xlnt.hpp>


BEGIN_EVENT_TABLE(MainFrame, wxFrame)
EVT_MENU(wxID_NEW, MainFrame::OnNew)
EVT_MENU(wxID_OPEN, MainFrame::OnOpen)
EVT_MENU(wxID_SAVE, MainFrame::OnSave)
EVT_MENU(wxID_EXIT, MainFrame::OnExit)
EVT_MENU(wxID_EDIT, MainFrame::OnEditCSVSettings)
EVT_MENU(wxID_ADD, MainFrame::OnEditFunctions)
EVT_PG_CHANGED(wxID_ANY, MainFrame::OnPropertyGridChanged)
EVT_PG_CHANGING(wxID_ANY, MainFrame::OnPropertyGridChanging)
END_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title) : wxFrame(NULL, -1, title, wxPoint(-1, -1)) {

	SetIcon(wxICON(MainIcon));

	char* appdata = getenv("LOCALAPPDATA");
	std::wstring PathToAppData = std::wstring(appdata, appdata + strlen(appdata)) + LR"(\Ezodox\EzPlot\)";
	if (not std::filesystem::exists(PathToAppData)) {
		std::filesystem::create_directories(PathToAppData);
	}

	// Add PyFitfunctions path to python path
	PyObject* sys_path = PySys_GetObject("path");
	PyList_Append(sys_path, PyUnicode_FromWideChar(PathToAppData.c_str(), wcslen(PathToAppData.c_str())));

	// Pre-import python modules:
	print_module = PyImport_ImportModule("python_modules.cprint");
	fp_module = PyImport_ImportModule("python_modules.file_picker");
	plot_module = PyImport_ImportModule("python_modules.plot");

	std::wstring PyFuncsPath = PathToAppData + L"PyFitfunctions.py";
	if (not std::filesystem::exists(PyFuncsPath)) {
		std::filesystem::copy_file(LR"(.\python_modules\PyFitfunctions_Default.py)", PyFuncsPath);
	}
	fitfunctions_module = PyImport_ImportModule("PyFitfunctions");

	CPlot = PyObject_GetAttrString(plot_module, "CPlot");
	GetColNames = PyObject_GetAttrString(fp_module, "GetColNames");
	catcher = PyObject_GetAttrString(print_module, "catchOutErr");
	ShowPlot = PyObject_GetAttrString(plot_module, "ShowPlot");

	// Read Lines from Fitfunctions file:
	std::vector<std::wstring> Lines;
	std::wstring line;
	std::wstring FuncsPath = PathToAppData + L"Fitfunctions.dat";
	if (not std::filesystem::exists(FuncsPath)) {
		FuncsPath = L"Fitfunctions_Default.dat";
	}
	std::wifstream Functionsfile(FuncsPath);
	if (Functionsfile.is_open())
	{
		while (std::getline(Functionsfile, line))
		{
			Lines.push_back(line);
		}
		Functionsfile.close();
	}

	// Store Fitfunctions data in the dictionaries:
	
	for (int i = 0; i < Lines.size(); i++) {
		PythonFuncs[Lines[i]] = Lines[i + 1];
		LatexFuncs[Lines[i]] = Lines[i + 2];
		std::size_t ParamsCount = GetPythonFuncParams(PythonFuncs[Lines[i]]).size();

		std::vector<std::wstring> pars;
		for (std::size_t p = 1; p <= ParamsCount; ++p) {
			pars.push_back(Lines[i + 2 + p]);
		}
		LatexParams[Lines[i]] = pars;
		i = i + 2 + ParamsCount;
	}

	// Read Lines from CSV Settings file:

	std::vector<std::string> CSVLines;
	std::string CSVline;
	std::ifstream CSVSettingsFile(PathToAppData + L"CSV Settings.dat");
	if (CSVSettingsFile.is_open())
	{
		while (std::getline(CSVSettingsFile, CSVline))
		{
			CSVLines.push_back(CSVline);
		}
		CSVSettingsFile.close();
	}

	// Store CSV Settings data in the dictionary:
	for (int i = 0; i < CSVLines.size(); i++) {
		CSVSettings[CSVLines[i]] = CSVLines[i + 1];
		i = i + 1;
	}

	if (CSVSettings.empty()) {
		CSVSettings["Seperator"] = ",";
		CSVSettings["Decimal"] = ".";
	}

	// creating MainPanel and Tabs:
	MainPanel = new wxPanel(this, wxID_ANY);
	Tabs = new wxNotebook(MainPanel, wxID_ANY);
	DataTab = new wxPanel(Tabs, wxID_ANY);
	DataPanel = new wxPanel(DataTab, wxID_ANY);
	SettingsTab = new wxPanel(Tabs, wxID_ANY);
	OutputTab = new wxPanel(Tabs, wxID_ANY);

	FilePicker = new wxFilePickerCtrl(DataTab, wxID_ANY, "Data table file", wxFileSelectorPromptStr,
		wxFileSelectorDefaultWildcardStr);
	FilePicker->SetMinSize(wxSize(900, -1));
	FilePicker->Bind(wxEVT_FILEPICKER_CHANGED, &MainFrame::OnFilePicked, this);

	/*
	LoadingIcon = new wxActivityIndicator(DataTab);
	LoadingIcon->SetMinSize(wxSize(50, 50));
	LoadingIcon->Start();
	*/

	CreateSettingsTab();
	CreateOutputTab();

	Tabs->AddPage(DataTab, L"Data");
	Tabs->AddPage(SettingsTab, L"Settings");
	Tabs->AddPage(OutputTab, L"Fit Parameters");

	/*
	LoadingSizer = new wxBoxSizer(wxHORIZONTAL);
	LoadingSizer->Add(425, 0);
	LoadingSizer->Add(LoadingIcon);
	*/

	// Set up the (vertical) sizer for the contents on DataTab:
	DataSizer = new wxBoxSizer(wxVERTICAL);
	DataSizer->Add(0, 40);
	DataSizer->Add(FilePicker, 0, wxALL, 10);
	DataSizer->Add(DataPanel, 0, wxALL, 10);
	//DataSizer->Add(LoadingSizer, 0, wxALL, 10);
	DataTab->SetSizer(DataSizer);

	// Set up the sizer for the Tabs on MainPanel:
	PanelSizer = new wxBoxSizer(wxHORIZONTAL);
	PanelSizer->Add(Tabs, 1, wxEXPAND);
	MainPanel->SetSizer(PanelSizer);

	// Set up the sizer for the contents on frame:
	FrameSizer = new wxBoxSizer(wxHORIZONTAL);
	FrameSizer->Add(MainPanel, 1, wxEXPAND);
	SetSizerAndFit(FrameSizer);

	wxMenuBar* MenuBar = new wxMenuBar();
	wxMenu* FileMenu = new wxMenu;
	wxMenu* EditMenu = new wxMenu;
	wxMenu* FitMenu = new wxMenu;

	// Append Items to FileMenu
	wxMenuItem* NewItem = new wxMenuItem(FileMenu, wxID_NEW);
	NewItem->SetBitmap(wxArtProvider::GetBitmap("wxART_NEW"));

	wxMenuItem* OpenItem = new wxMenuItem(FileMenu, wxID_OPEN);
	OpenItem->SetBitmap(wxArtProvider::GetBitmap("wxART_FILE_OPEN"));

	wxMenuItem* SaveItem = new wxMenuItem(FileMenu, wxID_SAVE);
	SaveItem->SetBitmap(wxArtProvider::GetBitmap("wxART_FILE_SAVE"));

	wxMenuItem* QuitItem = new wxMenuItem(FileMenu, wxID_EXIT);
	QuitItem->SetBitmap(wxArtProvider::GetBitmap("wxART_QUIT"));

	FileMenu->Append(NewItem);
	FileMenu->Append(OpenItem);
	FileMenu->Append(SaveItem);
	FileMenu->AppendSeparator();
	FileMenu->Append(QuitItem);

	// Append Items to EditMenu

	//wxMenuItem* UndoItem = new wxMenuItem(EditMenu, wxID_UNDO);
	//UndoItem->SetBitmap(wxArtProvider::GetBitmap("wxART_UNDO"));

	//wxMenuItem* RedoItem = new wxMenuItem(EditMenu, wxID_REDO);
	//RedoItem->SetBitmap(wxArtProvider::GetBitmap("wxART_REDO"));

	wxMenuItem* EditCSVSettings = new wxMenuItem(EditMenu, wxID_EDIT, wxT("&CSV Settings"));
	wxMenuItem* EditFunctionsItem = new wxMenuItem(EditMenu, wxID_ADD, wxT("&Fitfunctions"));

	//EditMenu->Append(UndoItem);
	//EditMenu->Append(RedoItem);
	EditMenu->Append(EditCSVSettings);
	EditMenu->Append(EditFunctionsItem);

	// Append MenuBar Items:
	MenuBar->Append(FileMenu, _("&File"));
	MenuBar->Append(EditMenu, _("&Edit"));
	SetMenuBar(MenuBar);

	// Windows closed:
	FuncsFrame = NULL;
	PlotFrame = NULL;
	FileName = "";

}

std::string ToRawString(const std::string& input)
{
	std::string output;
	output.reserve(input.size());
	for (const char c : input) {
		switch (c) {
		case '\'':  output += "\\'";        break;
		case '\"':  output += "\\\"";        break;
		case '\?':  output += "\\?";        break;
		case '\\':  output += "\\\\";       break;
		case '\a':  output += "\\a";        break;
		case '\b':  output += "\\b";        break;
		case '\f':  output += "\\f";        break;
		case '\n':  output += "\\n";        break;
		case '\r':  output += "\\r";        break;
		case '\t':  output += "\\t";        break;
		case '\v':  output += "\\v";        break;
		default:    output += c;            break;
		}
	}

	return output;
}

void MainFrame::CreateSettingsTab() {

	Colors.Add("black");
	Colors.Add("blue");
	Colors.Add("green");
	Colors.Add("red");
	Colors.Add("purple");
	Colors.Add("brown");
	Colors.Add("pink");
	Colors.Add("gray");
	Colors.Add("olive");
	Colors.Add("yellow");
	Colors.Add("cyan");
	Colors.Add("magenta");
	Colors.Add("darkblue");
	Colors.Add("yellowgreen");
	Colors.Add("darkcyan");

	CreateAdditionalValidators();
	
	PlotSettingsGrid = new wxPropertyGrid(SettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
		wxPG_SPLITTER_AUTO_CENTER | wxPG_DEFAULT_STYLE);
	PlotSettingsGrid->SetMinSize(wxSize(450, 500));
	PlotSettingsGrid->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
	PlotSettingsGrid->ClearActionTriggers(wxPG_ACTION_PREV_PROPERTY);
	PlotSettingsGrid->ClearActionTriggers(wxPG_ACTION_NEXT_PROPERTY);
	PlotSettingsGrid->DedicateKey(WXK_RETURN);
	PlotSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_RETURN);
	PlotSettingsGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY, WXK_RETURN);
	PlotSettingsGrid->DedicateKey(WXK_UP);
	PlotSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_UP);
	PlotSettingsGrid->AddActionTrigger(wxPG_ACTION_PREV_PROPERTY, WXK_UP);
	PlotSettingsGrid->DedicateKey(WXK_DOWN);
	PlotSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_DOWN);
	PlotSettingsGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY, WXK_DOWN);


	PlotSettingsGrid->Append(new wxPropertyCategory("General"));

	PlotWidth = PlotSettingsGrid->Append(new wxFloatProperty("Figure Width", wxPG_LABEL));
	PlotWidth->SetAttribute(L"Min", 0);
	PlotWidth->SetValidator(*eFloatValidator);
	PlotWidth->SetValueToUnspecified();
	//wxVariant Val = wxVariant(11.4);
	//PlotWidth->SetDefaultValue(Val);
	PlotWidth->SetHelpString("Width of the Figure.");
	PlotWidth->SetAttribute(L"Hint", 9.5);

	PlotHeight = PlotSettingsGrid->Append(new wxFloatProperty("Figure Height", wxPG_LABEL));
	PlotHeight->SetAttribute(L"Min", 0);
	PlotHeight->SetValidator(*eFloatValidator);
	PlotHeight->SetValueToUnspecified();
	//Val = wxVariant(4.8);
	//PlotHeight->SetDefaultValue(Val);
	PlotHeight->SetHelpString("Height of the Figure.");
	PlotHeight->SetAttribute(L"Hint", 4.0);

	PlotProjection = PlotSettingsGrid->Append(new wxBoolProperty("Projection", wxPG_LABEL));
	//Val = wxVariant(false);
	//PlotProjection->SetDefaultValue(Val);
	PlotProjection->SetValueToUnspecified();
	PlotProjection->SetHelpString("Create a polar plot?");
	PlotProjection->SetAttribute(L"Hint", "False");

	PlotSettingsGrid->Append(new wxPropertyCategory("Legend"));

	/*
	wxArrayString InitArr;
	InitArr.Add("");
	wxArrayInt InitArr2;
	InitArr2.Add(1);

	DataLegend = new wxFlagsProperty("Data in Legend", wxPG_LABEL, InitArr, InitArr2);
	PlotSettingsGrid->Append(DataLegend);
	*/

	PlotLegendRelX = PlotSettingsGrid->Append(new wxFloatProperty("Legend relative X-Position", 
		wxPG_LABEL));
	PlotLegendRelX->SetAttribute(L"Min", 0);
	PlotLegendRelX->SetAttribute(L"Max", 1);
	PlotLegendRelX->SetValidator(*eFloatValidator);
	PlotLegendRelX->SetValueToUnspecified();
	//Val = wxVariant(0.8);
	//PlotLegendRelX->SetDefaultValue(Val);
	PlotLegendRelX->SetHelpString("Relative X-Position of the legend. Starting from the left.");
	PlotLegendRelX->SetAttribute(L"Hint", 0.8);

	PlotLegendRelY = PlotSettingsGrid->Append(new wxFloatProperty("Legend relative Y-Position",
		wxPG_LABEL));
	PlotLegendRelY->SetAttribute(L"Min", 0);
	PlotLegendRelY->SetAttribute(L"Max", 1);
	PlotLegendRelY->SetValidator(*eFloatValidator);
	PlotLegendRelY->SetValueToUnspecified();
	//Val = wxVariant(0.8);
	//PlotLegendRelY->SetDefaultValue(Val);
	PlotLegendRelY->SetHelpString("Relative Y-Position of the legend. Starting from the top.");
	PlotLegendRelY->SetAttribute(L"Hint", 0.8);

	PlotLegendFontsize = PlotSettingsGrid->Append(new wxFloatProperty("Legend Fontsize", wxPG_LABEL));
	PlotLegendFontsize->SetAttribute(L"Min", 0);
	PlotLegendFontsize->SetValidator(*eFloatValidator);
	PlotLegendFontsize->SetValueToUnspecified();
	//Val = wxVariant(20);
	//PlotLegendFontsize->SetDefaultValue(Val);
	PlotLegendFontsize->SetHelpString("Fontsize of the legend.");
	PlotLegendFontsize->SetAttribute(L"Hint", 20);

	PlotLegendHandlelength = PlotSettingsGrid->Append(new wxFloatProperty("Legend Handlelength",
		wxPG_LABEL));
	PlotLegendHandlelength->SetAttribute(L"Min", 0);
	PlotLegendHandlelength->SetValidator(*eFloatValidator);
	PlotLegendHandlelength->SetValueToUnspecified();
	//Val = wxVariant(3);
	//PlotLegendHandlelength->SetDefaultValue(Val);
	PlotLegendHandlelength->SetHelpString("Length of the handle in the legend.");
	PlotLegendHandlelength->SetAttribute(L"Hint", 3);

	PlotLegendMarkerscale = PlotSettingsGrid->Append(new wxFloatProperty("Legend Markerscale",
		wxPG_LABEL));
	PlotLegendMarkerscale->SetAttribute(L"Min", 0);
	PlotLegendMarkerscale->SetValidator(*eFloatValidator);
	PlotLegendMarkerscale->SetValueToUnspecified();
	//Val = wxVariant(2.0);
	//PlotLegendMarkerscale->SetDefaultValue(Val);
	PlotLegendMarkerscale->SetHelpString("Scale of the marker in the legend.");
	PlotLegendMarkerscale->SetAttribute(L"Hint", 2.0);

	PrZoom = PlotSettingsGrid->Append(new wxPropertyCategory("Zoom"));

	ZoomCount = PlotSettingsGrid->Append(new wxIntProperty("Amount of Zooms", wxPG_LABEL));
	wxVariant Val = 0;
	ZoomCount->SetDefaultValue(Val);
	ZoomCount->SetAttribute(L"Min", 0);
	ZoomCount->SetEditor(wxPGEditor_SpinCtrl);
	ZoomCount->SetHelpString("Amount of Zooms over the plot.");
	//ZoomCount->SetAttribute(L"Hint", 0);

	ZoomXMin = PlotSettingsGrid->Append(new wxFloatProperty("Zoom X-Minimum", wxPG_LABEL));
	ZoomXMin->SetValidator(*eFloatValidator);
	ZoomXMin->SetValueToUnspecified();
	ZoomXMin->Hide(true);
	ZoomXMin->SetHelpString("Minimum of the x-Axis in the zoom.");

	ZoomXMax = PlotSettingsGrid->Append(new wxFloatProperty("Zoom X-Maximum", wxPG_LABEL));
	ZoomXMax->SetValidator(*eFloatValidator);
	ZoomXMax->SetValueToUnspecified();
	ZoomXMax->Hide(true);
	ZoomXMax->SetHelpString("Maximum of the x-Axis in the zoom.");

	ZoomYMin = PlotSettingsGrid->Append(new wxFloatProperty("Zoom Y-Minimum", wxPG_LABEL));
	ZoomYMin->SetValidator(*eFloatValidator);
	ZoomYMin->SetValueToUnspecified();
	ZoomYMin->Hide(true);
	ZoomYMin->SetHelpString("Minimum of the y-Axis in the zoom.");

	ZoomYMax = PlotSettingsGrid->Append(new wxFloatProperty("Zoom Y-Maximum", wxPG_LABEL));
	ZoomYMax->SetValidator(*eFloatValidator);
	ZoomYMax->SetValueToUnspecified();
	ZoomYMax->Hide(true);
	ZoomYMax->SetHelpString("Maximum of the y-Axis in the zoom.");

	ZoomXSci = PlotSettingsGrid->Append(new wxIntProperty("Zoom X-Axis Scientific Notation", wxPG_LABEL));
	ZoomXSci->SetEditor(wxPGEditor_SpinCtrl);
	ZoomXSci->SetValidator(*eIntValidator);
	ZoomXSci->SetValueToUnspecified();
	ZoomXSci->Hide(true);
	ZoomXSci->SetHelpString("Number of the power of ten to use for the x-Axis in the zoom.");

	ZoomYSci = PlotSettingsGrid->Append(new wxIntProperty("Zoom Y-Axis Scientific Notation", wxPG_LABEL));
	ZoomYSci->SetEditor(wxPGEditor_SpinCtrl);
	ZoomYSci->SetValidator(*eIntValidator);
	ZoomYSci->SetValueToUnspecified();
	ZoomYSci->Hide(true);
	ZoomYSci->SetHelpString("Number of the power of ten to use for the y-Axis in the zoom.");

	ZoomXPeriod = PlotSettingsGrid->Append(new wxFloatProperty("Zoom X Major Ticks Period", wxPG_LABEL));
	ZoomXPeriod->SetAttribute(L"Min", 0);
	ZoomXPeriod->SetValidator(*eFloatValidator);
	ZoomXPeriod->SetValueToUnspecified();
	ZoomXPeriod->Hide(true);
	ZoomXPeriod->SetHelpString("Distance (period) between the major x-ticks in the zoom.");

	ZoomYPeriod = PlotSettingsGrid->Append(new wxFloatProperty("Zoom Y Major Ticks Period", wxPG_LABEL));
	ZoomYPeriod->SetAttribute(L"Min", 0);
	ZoomYPeriod->SetValidator(*eFloatValidator);
	ZoomYPeriod->SetValueToUnspecified();
	ZoomYPeriod->Hide(true);
	ZoomYPeriod->SetHelpString("Distance (period) between the major y-ticks in the zoom.");

	ZoomHelpLines = PlotSettingsGrid->Append(new wxBoolProperty("Help lines for zooms?", wxPG_LABEL));
	ZoomHelpLines->SetValueToUnspecified();
	ZoomHelpLines->SetHelpString("Use help lines from the zoom area in the plot to the zoom subplots?");
	ZoomHelpLines->SetAttribute(L"Hint", "True");

	Markers = PlotSettingsGrid->Append(new wxPropertyCategory("Markers"));

	MarkerColor = PlotSettingsGrid->Append(new wxEnumProperty("Marker Color", wxPG_LABEL, Colors));
	MarkerColor->SetValueToUnspecified();
	MarkerColor->SetHelpString("Color of the markers");
	MarkerColor->SetAttribute(L"Hint", "black");

	MarkerSize = PlotSettingsGrid->Append(new wxFloatProperty("Marker Size", wxPG_LABEL));
	MarkerSize->SetAttribute(L"Min", 0);
	MarkerSize->SetValidator(*eFloatValidator);
	MarkerSize->SetValueToUnspecified();
	//Val = wxVariant(20);
	//MarkerSize->SetDefaultValue(Val);
	MarkerSize->SetHelpString("Size of the markers");
	MarkerSize->SetAttribute(L"Hint", 20);

	MarkerStyles.Add("circle");
	MarkerStyles.Add("point");
	MarkerStyles.Add("triangle_down");
	MarkerStyles.Add("triangle_up");
	MarkerStyles.Add("triangle_left");
	MarkerStyles.Add("triangle_right");
	MarkerStyles.Add("octagon");
	MarkerStyles.Add("square");
	MarkerStyles.Add("pentagon");
	MarkerStyles.Add("plus");
	MarkerStyles.Add("star");
	MarkerStyles.Add("hexagon");
	MarkerStyles.Add("X");
	MarkerStyles.Add("diamond");
	MarkerStyle = PlotSettingsGrid->Append(new wxEnumProperty("Marker Style", wxPG_LABEL, 
		MarkerStyles));
	MarkerStyle->SetValueToUnspecified();
	MarkerStyle->SetHelpString("Shape (style) of the markers");
	MarkerStyle->SetAttribute(L"Hint", "circle");

	MarkerOrder = PlotSettingsGrid->Append(new wxIntProperty("Marker Order", wxPG_LABEL));
	MarkerOrder->SetEditor(wxPGEditor_SpinCtrl);
	MarkerOrder->SetValidator(*eIntValidator);
	MarkerOrder->SetValueToUnspecified();
	//Val = wxVariant(2);
	//MarkerOrder->SetDefaultValue(Val);
	MarkerOrder->SetHelpString("Number of Order for the markers. Higher orders overlap smaller ones.");
	MarkerOrder->SetAttribute(L"Hint", 2);

	MarkerAlpha = PlotSettingsGrid->Append(new wxFloatProperty("Marker Alpha", wxPG_LABEL));
	MarkerAlpha->SetAttribute(L"Min", 0);
	MarkerAlpha->SetAttribute(L"Max", 1);
	MarkerAlpha->SetValidator(*eFloatValidator);
	MarkerAlpha->SetValueToUnspecified();
	//Val = wxVariant(1);
	//MarkerAlpha->SetDefaultValue(Val);
	MarkerAlpha->SetHelpString("Alpha (transparency) value of the markers");
	MarkerAlpha->SetAttribute(L"Hint", 1);

	MarkerConnect = PlotSettingsGrid->Append(new wxBoolProperty("Connect Markers?", wxPG_LABEL));
	MarkerConnect->SetValueToUnspecified();
	MarkerConnect->SetHelpString("Plot linear lines between every two markers?");
	MarkerConnect->SetAttribute(L"Hint", "False");

	ErrorWidth = PlotSettingsGrid->Append(new wxFloatProperty("Error Bars Width", wxPG_LABEL));
	ErrorWidth->SetAttribute(L"Min", 0);
	ErrorWidth->SetValidator(*eFloatValidator);
	ErrorWidth->SetValueToUnspecified();
	//Val = wxVariant(2);
	//ErrorWidth->SetDefaultValue(Val);
	ErrorWidth->SetHelpString("Width of the error bars.");
	ErrorWidth->SetAttribute(L"Hint", 2);

	ErrorCapsize = PlotSettingsGrid->Append(new wxFloatProperty("Error Capsize", wxPG_LABEL));
	ErrorCapsize->SetAttribute(L"Min", 0);
	ErrorCapsize->SetValidator(*eFloatValidator);
	ErrorCapsize->SetValueToUnspecified();
	//Val = wxVariant(3);
	//ErrorCapsize->SetDefaultValue(Val);
	ErrorCapsize->SetHelpString("Size of the cap of the error bars.");
	ErrorCapsize->SetAttribute(L"Hint", 3);

	PlotSettingsGrid->Append(new wxPropertyCategory("Axis"));

	PlotXLabel = PlotSettingsGrid->Append(new wxStringProperty("X-Label", wxPG_LABEL));
	PlotXLabel->SetHelpString("Label for the x-Axis. LaTeX-Math can be used.");
	PlotXLabel->SetAttribute(L"Hint", R"(Degrees / $^\circ$)");

	PlotYLabel = PlotSettingsGrid->Append(new wxStringProperty("Y-Label", wxPG_LABEL));
	PlotYLabel->SetHelpString("Label for the y-Axis. LaTeX-Math can be used.");
	PlotYLabel->SetAttribute(L"Hint", R"($\beta$ / km)");

	PlotLabelsize = PlotSettingsGrid->Append(new wxFloatProperty("Labelsize", wxPG_LABEL));
	PlotLabelsize->SetAttribute(L"Min", 0);
	PlotLabelsize->SetValidator(*eFloatValidator);
	PlotLabelsize->SetValueToUnspecified();
	//Val = wxVariant(30);
	//PlotLabelsize->SetDefaultValue(Val);
	PlotLabelsize->SetHelpString("Size of the axis-labels.");
	PlotLabelsize->SetAttribute(L"Hint", 30);

	PlotXMin = PlotSettingsGrid->Append(new wxFloatProperty("X-Limit Minimum", wxPG_LABEL));
	PlotXMin->SetValidator(*eFloatValidator);
	PlotXMin->SetValueToUnspecified();
	PlotXMin->SetHelpString("Minimum (left limit) of the x-axis.");

	PlotXMax = PlotSettingsGrid->Append(new wxFloatProperty("X-Limit Maximum", wxPG_LABEL));
	PlotXMax->SetValidator(*eFloatValidator);
	PlotXMax->SetValueToUnspecified();
	PlotXMax->SetHelpString("Maximum (right limit) of the x-axis.");

	PlotYMin = PlotSettingsGrid->Append(new wxFloatProperty("Y-Limit Minimum", wxPG_LABEL));
	PlotYMin->SetValidator(*eFloatValidator);
	PlotYMin->SetValueToUnspecified();
	PlotYMin->SetHelpString("Minimum (bottom limit) of the y-axis.");

	PlotYMax = PlotSettingsGrid->Append(new wxFloatProperty("Y-Limit Maximum", wxPG_LABEL));
	PlotYMax->SetValidator(*eFloatValidator);
	PlotYMax->SetValueToUnspecified();
	PlotYMax->SetHelpString("Maximum (top limit) of the y-axis.");

	PlotLogScaleX = PlotSettingsGrid->Append(new wxBoolProperty("X-Axis Logarithmic Scale?", wxPG_LABEL));
	PlotLogScaleX->SetValueToUnspecified();
	PlotLogScaleX->SetHelpString("Use logarithmic scale for the x-axis?");
	PlotLogScaleX->SetAttribute(L"Hint", "False");

	PlotLogScaleY = PlotSettingsGrid->Append(new wxBoolProperty("Y-Axis Logarithmic Scale?",
		wxPG_LABEL));
	PlotLogScaleY->SetValueToUnspecified();
	PlotLogScaleY->SetHelpString("Use logarithmic scale for the y-axis?");
	PlotLogScaleY->SetAttribute(L"Hint", "False");

	PlotLogBaseX = PlotSettingsGrid->Append(new wxFloatProperty("Base of X-Axis Logarithmic Scale",
		wxPG_LABEL));
	PlotLogBaseX->SetValidator(*eFloatValidator);
	PlotLogBaseX->SetValueToUnspecified();
	PlotLogBaseX->SetHelpString("Base of the logarithmic scale for the x-axis.");
	PlotLogBaseX->SetAttribute(L"Hint", "e");

	PlotLogBaseY = PlotSettingsGrid->Append(new wxFloatProperty("Base of Y-Axis Logarithmic Scale",
		wxPG_LABEL));
	PlotLogBaseY->SetValidator(*eFloatValidator);
	PlotLogBaseY->SetValueToUnspecified();
	PlotLogBaseY->SetHelpString("Base of the logarithmic scale for the y-axis.");
	PlotLogBaseY->SetAttribute(L"Hint", "e");

	PlotXSci = PlotSettingsGrid->Append(new wxIntProperty("X-Axis Scientific Notation", wxPG_LABEL));
	PlotXSci->SetEditor(wxPGEditor_SpinCtrl);
	PlotXSci->SetValidator(*eIntValidator);
	PlotXSci->SetValueToUnspecified();
	PlotXSci->SetHelpString("Number of the power of ten to use for the x-Axis.");

	PlotYSci = PlotSettingsGrid->Append(new wxIntProperty("Y-Axis Scientific Notation", wxPG_LABEL));
	PlotYSci->SetEditor(wxPGEditor_SpinCtrl);
	PlotYSci->SetValidator(*eIntValidator);
	PlotYSci->SetValueToUnspecified();
	PlotYSci->SetHelpString("Number of the power of ten to use for the y-Axis.");

	PlotTickLabelsize = PlotSettingsGrid->Append(new wxFloatProperty("Size of Tick Labels", wxPG_LABEL));
	PlotTickLabelsize->SetAttribute(L"Min", 0);
	PlotTickLabelsize->SetValidator(*eFloatValidator);
	PlotTickLabelsize->SetValueToUnspecified();
	//Val = wxVariant(20);
	//PlotTickLabelsize->SetDefaultValue(Val);
	PlotTickLabelsize->SetHelpString("Labelsize for the tick labels.");
	PlotTickLabelsize->SetAttribute(L"Hint", 20);

	wxArrayString Directions;
	Directions.Add("out");
	Directions.Add("in");
	PlotTickDirection = PlotSettingsGrid->Append(new wxEnumProperty("Tick Direction", wxPG_LABEL, Directions));
	PlotTickDirection->SetValueToUnspecified();
	PlotTickDirection->SetHelpString("Direction of the ticks.");
	PlotTickDirection->SetAttribute(L"Hint", "out");

	PlotMajorTickLength = PlotSettingsGrid->Append(new wxFloatProperty("Major Tick Length", wxPG_LABEL));
	PlotMajorTickLength->SetAttribute(L"Min", 0);
	PlotMajorTickLength->SetValidator(*eFloatValidator);
	PlotMajorTickLength->SetValueToUnspecified();
	//Val = wxVariant(10);
	//PlotMajorTickLength->SetDefaultValue(Val);
	PlotMajorTickLength->SetHelpString("Length of the major ticks.");
	PlotMajorTickLength->SetAttribute(L"Hint", 10);

	PlotMajorTickWidth = PlotSettingsGrid->Append(new wxFloatProperty("Major Tick Width", wxPG_LABEL));
	PlotMajorTickWidth->SetAttribute(L"Min", 0);
	PlotMajorTickWidth->SetValidator(*eFloatValidator);
	PlotMajorTickWidth->SetValueToUnspecified();
	//Val = wxVariant(2.5);
	//PlotMajorTickWidth->SetDefaultValue(Val);
	PlotMajorTickWidth->SetHelpString("Width of the major ticks.");
	PlotMajorTickWidth->SetAttribute(L"Hint", 2.5);

	PlotMajorTicksPeriodX = PlotSettingsGrid->Append(new wxFloatProperty("X Major Ticks Period", wxPG_LABEL));
	PlotMajorTicksPeriodX->SetAttribute(L"Min", 0);
	PlotMajorTicksPeriodX->SetValidator(*eFloatValidator);
	PlotMajorTicksPeriodX->SetValueToUnspecified();
	PlotMajorTicksPeriodX->SetHelpString("Distance (period) between the major x-ticks.");

	PlotMajorTicksPeriodY = PlotSettingsGrid->Append(new wxFloatProperty("Y Major Ticks Period", wxPG_LABEL));
	PlotMajorTicksPeriodY->SetAttribute(L"Min", 0);
	PlotMajorTicksPeriodY->SetValidator(*eFloatValidator);
	PlotMajorTicksPeriodY->SetValueToUnspecified();
	PlotMajorTicksPeriodY->SetHelpString("Distance (period) between the major y-ticks.");

	PlotMinorTicksX = PlotSettingsGrid->Append(new wxBoolProperty("X Minor Ticks", wxPG_LABEL));
	PlotMinorTicksX->SetValueToUnspecified();
	PlotMinorTicksX->SetHelpString("Use minor ticks on the x-axis?");
	PlotMinorTicksX->SetAttribute(L"Hint", "False");

	PlotMinorTicksY = PlotSettingsGrid->Append(new wxBoolProperty("Y Minor Ticks", wxPG_LABEL));
	PlotMinorTicksY->SetValueToUnspecified();
	PlotMinorTicksY->SetHelpString("Use minor ticks on the y-axis?");
	PlotMinorTicksY->SetAttribute(L"Hint", "False");

	PlotMinorTicksPeriodX = PlotSettingsGrid->Append(new wxFloatProperty("X Minor Ticks Period", wxPG_LABEL));
	PlotMinorTicksPeriodX->SetAttribute(L"Min", 0);
	PlotMinorTicksPeriodX->SetValidator(*eFloatValidator);
	PlotMinorTicksPeriodX->SetValueToUnspecified();
	PlotMajorTicksPeriodX->SetHelpString("Distance (period) between the minor x-ticks.");

	PlotMinorTicksPeriodY = PlotSettingsGrid->Append(new wxFloatProperty("Y Minor Ticks Period", wxPG_LABEL));
	PlotMinorTicksPeriodY->SetAttribute(L"Min", 0);
	PlotMinorTicksPeriodY->SetValidator(*eFloatValidator);
	PlotMinorTicksPeriodY->SetValueToUnspecified();
	PlotMinorTicksPeriodY->SetHelpString("Distance (period) between the minor y-ticks.");

	PlotMinorTickLength = PlotSettingsGrid->Append(new wxFloatProperty("Minor Tick Length", wxPG_LABEL));
	PlotMinorTickLength->SetAttribute(L"Min", 0);
	PlotMinorTickLength->SetValidator(*eFloatValidator);
	PlotMinorTickLength->SetValueToUnspecified();
	//Val = wxVariant(5);
	//PlotMinorTickLength->SetDefaultValue(Val);
	PlotMinorTickLength->SetHelpString("Length of the minor ticks.");
	PlotMinorTickLength->SetAttribute(L"Hint", 5);

	PlotMinorTickWidth = PlotSettingsGrid->Append(new wxFloatProperty("Minor Tick Width", wxPG_LABEL));
	PlotMinorTickWidth->SetAttribute(L"Min", 0);
	PlotMinorTickWidth->SetValidator(*eFloatValidator);
	PlotMinorTickWidth->SetValueToUnspecified();
	//Val = wxVariant(1.5);
	//PlotMinorTickWidth->SetDefaultValue(Val);
	PlotMinorTickWidth->SetHelpString("Width of the minor ticks.");
	PlotMinorTickWidth->SetAttribute(L"Hint", 1.5);

	// Checkboxes instead of true/false
	//PlotSettingsGrid->SetPropertyAttributeAll(wxPG_BOOL_USE_CHECKBOX, true);

	PlotSettingsGrid->SetPropertyAttributeAll(wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING, false);

	// Create FitSettingsGrid:
	FitSettingsGrid = new wxPropertyGrid(SettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxPG_SPLITTER_AUTO_CENTER | wxPG_DEFAULT_STYLE);
	FitSettingsGrid->SetMinSize(wxSize(450, 500));
	FitSettingsGrid->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
	FitSettingsGrid->ClearActionTriggers(wxPG_ACTION_PREV_PROPERTY);
	FitSettingsGrid->ClearActionTriggers(wxPG_ACTION_NEXT_PROPERTY);
	FitSettingsGrid->DedicateKey(WXK_RETURN);
	FitSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_RETURN);
	FitSettingsGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY, WXK_RETURN);
	FitSettingsGrid->DedicateKey(WXK_UP);
	FitSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_UP);
	FitSettingsGrid->AddActionTrigger(wxPG_ACTION_PREV_PROPERTY, WXK_UP);
	FitSettingsGrid->DedicateKey(WXK_DOWN);
	FitSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_DOWN);
	FitSettingsGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY, WXK_DOWN);

	FitCount = FitSettingsGrid->Append(new wxIntProperty("Amount of Fits", wxPG_LABEL));
	FitCount->SetAttribute(L"Min", 0);
	FitCount->SetEditor(wxPGEditor_SpinCtrl);
	Val = wxVariant(0);
	FitCount->SetDefaultValue(Val);
	FitCount->SetHelpString("How many fits to use on the data.");

	wxArrayString FitFunctions;

	for (auto element : PythonFuncs) {
		FitFunctions.Add(element.first);
	}

	FitFunctions.Add(L"[new]");

	FitFuncs = FitSettingsGrid->Append(new wxEnumProperty("Fitfunction", wxPG_LABEL, FitFunctions));
	FitFuncs->SetValueToUnspecified();
	FitFuncs->Hide(true);
	FitFuncs->SetHelpString("The function used for fitting.");

	FitSVs = FitSettingsGrid->Append(new wxStringProperty("Fit Starting Values", wxPG_LABEL));
	FitSVs->SetValueFromString("<composed>");
	FitSVs->Hide(true);
	FitSVs->SetHelpString("The start parameters for the values in the fitfunction.");

	FitData = FitSettingsGrid->Append(new wxEnumProperty("Fitted Data", wxPG_LABEL));
	FitData->SetValueToUnspecified();
	FitData->Hide(true);
	FitData->SetHelpString("Which data to fit.");

	FitXMin = FitSettingsGrid->Append(new wxFloatProperty("Fit X-Minimum", wxPG_LABEL));
	FitXMin->SetValidator(*eFloatValidator);
	FitXMin->SetValueToUnspecified();
	FitXMin->Hide(true);
	FitXMin->SetHelpString("Left x-limit where the fit starts.");

	FitXMax = FitSettingsGrid->Append(new wxFloatProperty("Fit X-Maximum", wxPG_LABEL));
	FitXMax->SetValidator(*eFloatValidator);
	FitXMax->SetValueToUnspecified();
	FitXMax->Hide(true);
	FitXMax->SetHelpString("Right x-limit where the fit ends.");

	FitColor = FitSettingsGrid->Append(new wxEnumProperty("Fit Color", wxPG_LABEL, Colors));
	FitColor->SetValueToUnspecified();
	FitColor->Hide(true);
	FitColor->SetHelpString("Color of the fit line.");
	FitColor->SetAttribute(L"Hint", "blue");

	FitName = FitSettingsGrid->Append(new wxStringProperty("Fit Name", wxPG_LABEL));
	FitName->SetValueToUnspecified();
	FitName->Hide(true);
	FitName->SetHelpString("Name of the fit for the output and LaTeX table.");

	FitExAreaMin = FitSettingsGrid->Append(new wxFloatProperty("Fit excluded area X-Minimum",
		wxPG_LABEL));
	FitExAreaMin->SetValidator(*eFloatValidator);
	FitExAreaMin->SetValueToUnspecified();
	FitExAreaMin->Hide(true);
	FitExAreaMin->SetHelpString("Start of an excluded x-area in the fit x-area");

	FitExAreaMax = FitSettingsGrid->Append(new wxFloatProperty("Fit excluded area X-Maximum",
		wxPG_LABEL));
	FitExAreaMax->SetValidator(*eFloatValidator);
	FitExAreaMax->SetValueToUnspecified();
	FitExAreaMax->Hide(true);
	FitExAreaMax->SetHelpString("End of an excluded x-area in the fit x-area");

	wxArrayString PlotAreas;
	PlotAreas.Add("Fit-Area");
	PlotAreas.Add("X-Interceptions");
	PlotAreas.Add("");
	FitPAreaMin = FitSettingsGrid->Append(new wxEnumProperty("Fit Plot X-Minimum", 
		wxPG_LABEL, PlotAreas));
	FitPAreaMin->SetValueToUnspecified();
	FitPAreaMin->Hide(true);
	FitPAreaMin->SetHelpString("Start of the plotted line of the fit.");

	FitPAreaMax = FitSettingsGrid->Append(new wxEnumProperty("Fit Plot X-Maximum", 
		wxPG_LABEL, PlotAreas));
	FitPAreaMax->SetValueToUnspecified();
	FitPAreaMax->Hide(true);
	FitPAreaMax->SetHelpString("End of the plotted line of the fit.");

	wxArrayString Linestyles;
	Linestyles.Add("solid");
	Linestyles.Add("dotted");
	Linestyles.Add("dashed");
	Linestyles.Add("dashdot");
	Linestyles.Add("dashdotdot");
	Linestyles.Add("densely dashed");
	FitLine = FitSettingsGrid->Append(new wxEnumProperty("Fit Line", wxPG_LABEL, Linestyles));
	FitLine->SetValueToUnspecified();
	FitLine->Hide(true);
	FitLine->SetHelpString("Style of the line of the fit.");

	FitPRes = FitSettingsGrid->Append(new wxBoolProperty("Print Residuals?", wxPG_LABEL));
	FitPRes->SetValueToUnspecified();
	FitPRes->Hide(true);
	FitPRes->SetHelpString("Print the residuals in the output?");
	FitPRes->SetAttribute(L"Hint", "False");

	FitBoundsMin = FitSettingsGrid->Append(new wxStringProperty("Fit Bounds Minimum", wxPG_LABEL));
	FitBoundsMin->SetValueFromString("<composed>");
	FitBoundsMin->Hide(true);
	FitBoundsMin->SetHelpString("Minimum limits (bounds) of the fit parameters.");

	FitBoundsMax = FitSettingsGrid->Append(new wxStringProperty("Fit Bounds Maximum", wxPG_LABEL));
	FitBoundsMax->SetValueFromString("<composed>");
	FitBoundsMax->Hide(true);
	FitBoundsMax->SetHelpString("Maximum limits (bounds) of the fit parameters.");

	wxArrayString FitMethods;
	FitMethods.Add("odr");
	FitMethods.Add("lm");
	FitMethods.Add("trf");
	FitMethods.Add("dogbox");
	FitMethod = FitSettingsGrid->Append(new wxEnumProperty("Fit Method", wxPG_LABEL, FitMethods));
	FitMethod->SetValueToUnspecified();
	FitMethod->Hide(true);
	FitMethod->SetHelpString("Fitting method to use. ODR is orthogonal distance regression.");
	FitMethod->SetAttribute(L"Hint", "lm");

	FitLogFit = FitSettingsGrid->Append(new wxBoolProperty("Use Logarithmus on Fit and Data?",
		wxPG_LABEL));
	FitLogFit->SetValueToUnspecified();
	FitLogFit->Hide(true);
	FitLogFit->SetHelpString("Use logarithmus on the fit function and the data while fitting?");
	FitLogFit->SetAttribute(L"Hint", "False");

	FitLogBase = FitSettingsGrid->Append(new wxFloatProperty("Fit Logarithmic Base", wxPG_LABEL));
	FitLogBase->SetValidator(*eFloatValidator);
	FitLogBase->SetValueToUnspecified();
	FitLogBase->Hide(true);
	FitLogBase->SetHelpString("Base of the logarithmus when used.");
	FitLogBase->SetAttribute(L"Hint", "e");

	wxArrayString LossFuncs;
	LossFuncs.Add("linear");
	LossFuncs.Add("soft_l1");
	LossFuncs.Add("huber");
	LossFuncs.Add("cauchy");
	LossFuncs.Add("arctan");
	FitLoss = FitSettingsGrid->Append(new wxEnumProperty("Fit Loss-Function", wxPG_LABEL, LossFuncs));
	FitLoss->SetValueToUnspecified();
	FitLoss->Hide(true);
	FitLoss->SetHelpString("Function to calculate the loss. Using non-linear can limit the loss.");

	FitLossScale = FitSettingsGrid->Append(new wxFloatProperty("Fit Loss-Scale", wxPG_LABEL));
	FitLossScale->SetAttribute(L"Min", 0);
	FitLossScale->SetValidator(*eFloatValidator);
	FitLossScale->SetValueToUnspecified();
	FitLossScale->Hide(true);
	FitLossScale->SetHelpString("Scale of the calculated loss.");
	FitLossScale->SetAttribute(L"Hint", 1.0);

	/*
	FitodrType = FitSettingsGrid->Append(new wxStringProperty("Fit ODR-Type", wxPG_LABEL));
	FitodrType->SetValueToUnspecified();
	FitodrType->Hide(true);

	FitCV = FitSettingsGrid->Append(new wxStringProperty("Fit Cross Validation", wxPG_LABEL));
	FitCV->SetValueToUnspecified();
	FitCV->Hide(true);
	*/
	FitLinewidth = FitSettingsGrid->Append(new wxFloatProperty("Fit Linewidth", wxPG_LABEL));
	FitLinewidth->SetAttribute(L"Min", 0);
	FitLinewidth->SetValidator(*eFloatValidator);
	FitLinewidth->SetValueToUnspecified();
	FitLinewidth->Hide(true);
	FitLinewidth->SetHelpString("Width of the line of the fit.");
	FitLinewidth->SetAttribute(L"Hint", 3.0);

	FitOrder = FitSettingsGrid->Append(new wxIntProperty("Fit Order", wxPG_LABEL));
	FitOrder->SetEditor(wxPGEditor_SpinCtrl);
	FitOrder->SetValueToUnspecified();
	FitOrder->Hide(true);
	FitOrder->SetHelpString("Order for the line of the fit. Higher orders overlap smaller ones.");
	FitOrder->SetAttribute(L"Hint", 3);

	FitOrdersZoom = FitSettingsGrid->Append(new wxIntProperty("Fit Order of Zoom", wxPG_LABEL));
	FitOrdersZoom->SetEditor(wxPGEditor_SpinCtrl);
	FitOrdersZoom->SetValueToUnspecified();
	FitOrdersZoom->Hide(true);
	FitOrdersZoom->SetHelpString("Order for the line of the fit in the zoom.");
	FitOrdersZoom->SetAttribute(L"Hint", 3);

	// Checkboxes instead of true/false
	//FitSettingsGrid->SetPropertyAttributeAll(wxPG_BOOL_USE_CHECKBOX, true);
	FitSettingsGrid->SetPropertyAttributeAll(wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING, false);


	wxStaticText* PlotSettingsHeader = new wxStaticText(SettingsTab, wxID_ANY, L"Plot Settings:");
	wxStaticText* FitSettingsHeader = new wxStaticText(SettingsTab, wxID_ANY, L"Fit Settings:");

	PlotSettingsHeader->SetMinSize(wxSize(PlotSettingsGrid->GetMinWidth(),
		PlotSettingsHeader->GetMinHeight()));
	FitSettingsHeader->SetMinSize(wxSize(FitSettingsGrid->GetMinWidth(),
		FitSettingsHeader->GetMinHeight()));

	// Set up the sizer for the contents on SettingsTab:
	SettingsSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer* HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	HeaderSizer->Add(PlotSettingsHeader, 1);
	HeaderSizer->Add(FitSettingsHeader, 1);
	SettingsSizer->Add(HeaderSizer, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);

	wxBoxSizer* GridSizer = new wxBoxSizer(wxHORIZONTAL);
	GridSizer->Add(PlotSettingsGrid, 1, wxEXPAND);
	GridSizer->Add(FitSettingsGrid, 1, wxEXPAND);
	SettingsSizer->Add(GridSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	SettingsTab->SetSizer(SettingsSizer);


}

void MainFrame::ChildsToParent(wxPGProperty* Parent) {
	wxPropertyGridIterator it;
	wxPGProperty* FirstChild = Parent->GetGrid()->GetFirstChild(Parent);
	for (it = Parent->GetGrid()->GetIterator(wxPG_ITERATE_DEFAULT, FirstChild); !it.AtEnd(); it++)
	{
		wxPGProperty* CurrentProp = *it;
		if (CurrentProp->GetParent() != Parent) {
			break;
		}
		if (CurrentProp->IsValueUnspecified() or (Parent == FitMethod and WithAnyBounds)) {
			if (CurrentProp->IsEnabled()) { CurrentProp->SetValue(Parent->GetValue()); }
		}
	}
	Parent->SetValueToUnspecified();
	if (Parent->GetParent()->IsRoot() or Parent->GetParent() == PrZoom
		or Parent->GetParent() == Markers) {
		Parent->GetGrid()->SetPropertyCell(Parent, 1, Parent->GenerateComposedValue(),
			wxBitmapBundle(), *wxBLACK);
	}
}

void MainFrame::OnPropertyGridChanged(wxPropertyGridEvent& event) {
	wxPGProperty* Property = event.GetProperty();
	if (!Property) { return; }

	UpdateSettingsGrid(Property);

}

void MainFrame::UpdateSettingsGrid(wxPGProperty* Property) {


	wxPGProperty* Parent = Property->GetParent();
	wxPGProperty* MainParent = Property->GetMainParent();

	if (MainParent == FitBoundsMin or MainParent == FitBoundsMax) {

		int FitsWithoutBounds = 0;
		for (unsigned int i = 1; i <= FitBoundsMin->GetChildCount(); i++) {
			std::string Prefix = "Fit Bounds Minimum.Fit ";
			wxPGProperty* MinProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			Prefix = "Fit Bounds Maximum.Fit ";
			wxPGProperty* MaxProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));

			bool AllValuesEmpty = true;
			wxPGProperty* FirstChild = FitSettingsGrid->GetFirstChild(MinProp);
			for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
				FirstChild); !it.AtEnd(); it++) // Iterate over params
			{
				wxPGProperty* CurrentChild = *it;
				if (CurrentChild->GetParent() != MinProp) {
					break;
				}
				if (not CurrentChild->IsValueUnspecified()) {
					AllValuesEmpty = false;
					wxPGProperty* MethodProp = FitSettingsGrid->GetProperty("Fit Method.Fit "
						+ std::to_string(i));
					if (MethodProp->GetValueAsString() != "trf") {
						MethodProp->SetValueFromString("trf");
						if (not FitMethod->IsValueUnspecified()) {
							ChildsToParent(FitMethod);
						}
						MethodProp->Enable(false);
					}
					FitsWithoutBounds--;
					goto AfterLoops;
				};
			}
			FirstChild = FitSettingsGrid->GetFirstChild(MaxProp);
			for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
				FirstChild); !it.AtEnd(); it++) // Iterate over params
			{
				wxPGProperty* CurrentChild = *it;
				if (CurrentChild->GetParent() != MaxProp) {
					break;
				}
				if (not CurrentChild->IsValueUnspecified()) {
					AllValuesEmpty = false;
					wxPGProperty* MethodProp = FitSettingsGrid->GetProperty("Fit Method.Fit "
						+ std::to_string(i));
					if (MethodProp->GetValueAsString() != "trf") {
						MethodProp->SetValueFromString("trf");
						if (not FitMethod->IsValueUnspecified()) {
							ChildsToParent(FitMethod);
						}
						MethodProp->Enable(false);
					}
					FitsWithoutBounds--;
					goto AfterLoops;
				}
			}
		AfterLoops:
			if (AllValuesEmpty) {
				Prefix = "Fit Method.Fit ";
				wxPGProperty* Prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
				Prop->Enable(true);
				FitsWithoutBounds++;
			}
		}
		if (FitsWithoutBounds == FitMethod->GetChildCount()) { WithAnyBounds = false; }
		else { WithAnyBounds = true; }
		FitSettingsGrid->Refresh();
		return;
	}

	if (MainParent == FitSVs) { return; }

	if (Property->GetGrid() == FuncSettingsGrid) {
		if (Property == FuncPython) { // if FuncPython set, create FuncLatex and FuncParams
			if (FuncPython->IsValueUnspecified() or FuncPython->GetValueAsString() == "") {
				FuncLatex->SetValueToUnspecified();
				ParamsCategory->DeleteChildren();
				FuncParams.Clear();
			}
			else {
				std::vector<std::wstring> pars = GetPythonFuncParams(
					Property->GetValueAsString().ToStdWstring());
				if (pars.size() < FuncParams.GetCount()) {
					wxPropertyGridPageState* state = FuncSettingsGrid->GetState();
					for (unsigned int i = FuncParams.GetCount() - 1; i >= pars.size(); i--) {
						state->DoDelete(FuncParams.Item(i), true);
					}
					FuncParams.RemoveAt(pars.size(), FuncParams.GetCount() - pars.size());
				}
				std::wstring OldPythonFunc = PythonFuncs[FuncName->GetValueAsString().ToStdWstring()];

				if (FuncLatex->IsValueUnspecified()
					or FuncLatex->GetValueAsString().ToStdString() == OldPythonFunc) {
					FuncLatex->SetValueFromString(Property->GetValueAsString());
				}
				for (unsigned int i = 0; i < pars.size(); i++) {
					if (i >= FuncParams.GetCount()) {
						wxPGProperty* ParamProp = FuncSettingsGrid->Append(
							new wxStringProperty(pars[i], wxPG_LABEL, pars[i]));
						ParamProp->SetHelpString("LaTex code of the param for the LaTeX table.");
						FuncParams.Add(ParamProp);
					}
					else {
						wxPGProperty* Prop = FuncParams.Item(i);
						if (Prop->GetLabel() != pars[i]) {
							Prop->SetLabel(pars[i]);
							Prop->SetValueFromString(pars[i]);
						}
					}
				}
			}
		}
		else if (Parent == ParamsCategory) { // if param value set, change FuncLatex param
			FuncPython->GetValueAsString();
			// Get Params and Latex with changed latex as standard param:
			std::vector<std::wstring> Params;
			std::vector<std::wstring> Latex;
			int ParamNumber = 0;
			for (unsigned int i = 0; i < FuncParams.GetCount(); i++) {
				wxPGProperty* Prop = FuncParams.Item(i);
				Params.push_back(Prop->GetLabel().ToStdWstring());
				if (Prop == Property) {
					Latex.push_back(Prop->GetLabel().ToStdWstring());
					ParamNumber = i;
				}
				else {
					Latex.push_back(Prop->GetValueAsString().ToStdWstring());
				}
			}
			std::wstring Function = FuncPython->GetValueAsString().ToStdWstring();
			std::wstring OldReplacedFunction = ReplaceParamsWithLatex(Function, Params, Latex);
			std::wstring CurrentLatexFunction = FuncLatex->GetValueAsString().ToStdWstring();
			if (FuncLatex->IsValueUnspecified() or CurrentLatexFunction == OldReplacedFunction) {
				Latex[ParamNumber] = Property->GetValueAsString().ToStdWstring();
				FuncLatex->SetValueFromString(ReplaceParamsWithLatex(Function, Params, Latex));
			}
		}
		StoreFunctionVariables();
		FuncSettingsGrid->Refresh();
		return;
	}
	if (!(Parent->IsCategory()) and (Parent->GetChildCount() > 0) and !(Parent->IsRoot())
		and (MainParent != FitSVs) and (MainParent != FitBoundsMin) 
		and (MainParent != FitBoundsMax)) {
		// if childs value is set, set unspecified childs to parents value and parent to unspecific
		ChildsToParent(Parent);
	}
	else if (!(Property->IsCategory()) && (Property->GetChildCount() > 0)
		and (MainParent != FitSVs) and (MainParent != FitBoundsMin)
		and (MainParent != FitBoundsMax)) {
		// When Parent is changed to not unspecific set all childrens to unspecificied
		if (Property->IsValueUnspecified() == false) {

			if (WithAnyBounds == true and Property == FitMethod) {
				ChildsToParent(Property);
			}
			else {
				wxPropertyGridIterator it;
				wxPGProperty* FirstChild = Property->GetGrid()->GetFirstChild(Property);
				for (it = Property->GetGrid()->GetIterator(wxPG_ITERATE_DEFAULT, FirstChild); !it.AtEnd(); it++)
				{
					wxPGProperty* CurrentProp = *it;
					if (CurrentProp->GetParent() != Property) {
						break;
					}
					CurrentProp->SetValueToUnspecified();
				}
				Property->GetGrid()->Collapse(Property);
			}
		}
	}
	else if (Property == ZoomCount || Property == FitCount) {

		unsigned int Count = Property->GetValue().GetLong();

		// Hide or show and delete all previous created children of Zoom or Fits:
		bool ShouldHide = true;
		if (Count > 0) { ShouldHide = false; }
		wxPropertyGridIterator it;
		for (it = Property->GetGrid()->GetIterator(wxPG_ITERATE_DEFAULT, wxBOTTOM);
			!it.AtEnd();
			it--)
		{
			wxPGProperty* p = *it;
			wxPGProperty* Parent = p->GetParent();

			if (not Parent->IsRoot() and Parent->HasFlag(wxPG_PROP_COMPOSED_VALUE)
				and (Parent->GetParent()->HasFlag(wxPG_PROP_COMPOSED_VALUE) or 
					(FitName->GetChildCount() < 2 and Property == FitCount))) {
				continue; // if p is fit of FitSVs or FitBounds continue
			}

			if (((Parent == PrZoom && Property == ZoomCount) // if p is parent of zooms or fits:
				|| (Parent->IsRoot() && Property == FitCount))
				&& p != Property) {
				p->Hide(ShouldHide);
				//p->DeleteChildren();
				if (p->IsValueUnspecified() and p->GetChildCount() <= 1) {
					p->GetCell(1).SetText("");
				}
			}
			else if ((Parent->GetParent() == PrZoom or Property == FitCount) and p!= Property) { 
				// if p is zoom or fit:
				std::string pLabel = p->GetLabel().ToStdString();
				int LetterNum = 4;
				if (Property == ZoomCount) { LetterNum = 5; }
				unsigned int pNum = std::stoi(pLabel.erase(0, LetterNum));
				if (pNum == 1 and Count == 1 and not p->IsValueUnspecified()
					and not (Property == FitCount and Parent->HasFlag(wxPG_PROP_COMPOSED_VALUE))) {
					Parent->SetValue(p->GetValue());
				}
				if (pNum > Count or Count == 1 or (Property == FitCount 
					and FitName->GetChildCount() <= 1 
					and not Parent->HasFlag(wxPG_PROP_COMPOSED_VALUE))) {
					Property->GetGrid()->DeleteProperty(p);
					if (Parent->IsValueUnspecified() and Parent->GetChildCount() > 1
						and Parent->GetCell(1).GetText() != "") {
						// Remove last component of composed text:
						std::string ComposedText = Parent->GetCell(1).GetText().ToStdString();
						ComposedText = ComposedText.substr(0,ComposedText.find_last_of(";"));
						if (Count <= 1) { ComposedText = ""; }
						Parent->GetCell(1).SetText(ComposedText);
					}
				}
			}
		}

		// Create new children of Zoom or Fits
		unsigned int ChildCount = ZoomXMin->GetChildCount();
		if (Property == FitCount) { ChildCount = FitName->GetChildCount(); }
		for (unsigned int z = ChildCount + 1; z <= Count; z++) {
			if (Count <= 1) { break; }

			wxPropertyGridIterator i;
			for (i = Property->GetGrid()->GetIterator(); !i.AtEnd(); i++)
			{
				wxPGProperty* p = *i;
				wxPGProperty* Parent = p->GetParent();

				if (((Parent == PrZoom && Property == ZoomCount) ||
					(Parent->IsRoot() && Property == FitCount))
					&& p != Property && p != ZoomHelpLines
					and (Property == ZoomCount or (Property == FitCount 
						and not p->HasFlag(wxPG_PROP_COMPOSED_VALUE)))) {

					std::string LabelPrefix = "";
					if (Property == ZoomCount) { LabelPrefix = "Zoom "; }
					else if (Property == FitCount) { LabelPrefix = "Fit "; }
					wxString ChildLabel = LabelPrefix + std::to_string(z);
					//wxString ChildName = p->GetName() + ChildLabel;

					wxPGProperty* Child = AppendInChildSameType(Property->GetGrid(), p, ChildLabel,
						wxPG_LABEL);

				}
			}
		}

		// if FitCount update legend labels:
		if (Property == FitCount) {

			wxPGProperty* Prop = PlotSettingsGrid->GetProperty("Legend Labels");
			if (not Prop) {
				PGCLegendLabels = new wxPropertyCategory("Legend Labels");
				PlotSettingsGrid->Insert("Legend relative X-Position", PGCLegendLabels);
			}

			// Delete fit legend labels:
			for (unsigned int i = Count; i <= PGCLegendLabels->GetChildCount(); i++) {
				std::string ChildName = "Legend Labels.Fit " + std::to_string(i);
				wxPGProperty* Child = PlotSettingsGrid->GetProperty(ChildName);
				if (Child) { PlotSettingsGrid->DeleteProperty(Child); }
			}

			// Add fit legend labels:
			wxPGEditor* wxPGSpinOrderEditor = wxPropertyGrid::RegisterEditorClass(
				new wxSpinOrderCtrlEditor());
			for (unsigned int i = 1; i <= Count; i++) {

				wxString ChildLabel = "Fit " + std::to_string(i);
				wxPGProperty* Child = PlotSettingsGrid->GetProperty("Legend Labels." + ChildLabel);
				if (not Child) {
					wxPGProperty* UndergroundChild = PlotSettingsGrid->GetProperty(
						"Legend Labels.Underground");
					if (UndergroundChild) {
						PlotSettingsGrid->Insert(UndergroundChild, new wxStringProperty(ChildLabel,
							wxPG_LABEL));
						PlotSettingsGrid->SetPropertyEditor(ChildLabel, wxPGSpinOrderEditor);
					}
					else {
						PlotSettingsGrid->AppendIn(PGCLegendLabels, new wxStringProperty(ChildLabel,
							wxPG_LABEL));
						PlotSettingsGrid->SetPropertyEditor(ChildLabel, wxPGSpinOrderEditor);
					}
				}
			}
			if (Count > 0) {
				if (not PlotSettingsGrid->GetProperty("Legend Labels.Underground")) {
					PlotSettingsGrid->AppendIn(PGCLegendLabels, new wxStringProperty("Underground",
						wxPG_LABEL));
					PlotSettingsGrid->SetPropertyEditor("Underground", wxPGSpinOrderEditor);
					PlotSettingsGrid->AppendIn(PGCLegendLabels, new wxStringProperty("MeanLine",
						wxPG_LABEL));
					PlotSettingsGrid->SetPropertyEditor("MeanLine", wxPGSpinOrderEditor);
				}
			}
			else {
				wxPGProperty* Child = PlotSettingsGrid->GetProperty("Legend Labels.Underground");
				if (Child) { PlotSettingsGrid->DeleteProperty(Child); }
				Child = PlotSettingsGrid->GetProperty("Legend Labels.MeanLine");
				if (Child) { PlotSettingsGrid->DeleteProperty(Child); }
				if (PGCLegendLabels->GetChildCount() == 0) {
					PlotSettingsGrid->DeleteProperty(PGCLegendLabels);
				}
			}

			// Recreate Fit-Settings with params:
			if (Count > 0) { RecreatePropsWithParams(true); }
		}

		// Collapse the parents of childrens:
		wxPropertyGridIterator col;
		for (col = Property->GetGrid()->GetIterator(); !col.AtEnd(); col++)
		{
			wxPGProperty* p = *col;
			wxPGProperty* Parent = p->GetParent();

			if ((Parent == PrZoom && Property == ZoomCount)
				|| (Parent->IsRoot() && Property == FitCount)) {

				Property->GetGrid()->Collapse(p);
			}
		}
		FitCount->RecreateEditor();
		FitCount->RefreshEditor();
	}
	if (Parent == Markers) {
		if (Property->IsValueUnspecified()) {
			Property->GetCell(1).SetText("");
		}
	}
	if (Property == FitFuncs or Parent == FitFuncs) {
		if (Property->GetValueAsString() == L"[new]") {

			Property->SetValueToUnspecified();
			OpenFunctionsWindow();
			return;
		}
		else {
			// Update propertys with function parameters:
			RecreatePropsWithParams(false);
			FitSettingsGrid->CollapseAll();
		}
	}
	PlotSettingsGrid->Refresh();
	FitSettingsGrid->Refresh();
}

void MainFrame::OnPropertyGridChanging(wxPropertyGridEvent& event) {

	wxPGProperty* property = event.GetProperty();
	if (!property) { return; }


	// if Function Name changed, delete variables with old name-key
	if (property->GetGrid() == FuncSettingsGrid and property == FuncName) {
		PythonFuncs.erase(property->GetValueAsString().ToStdWstring());
		LatexFuncs.erase(property->GetValueAsString().ToStdWstring());
		LatexParams.erase(property->GetValueAsString().ToStdWstring());
	}

	// event.GetValue() is the pending value
	// if user cleared input:
	if (event.GetValue().IsNull() || event.GetValue().GetString() == "")
	{
		property->SetValueToUnspecified();
		return;
	}
}

void MainFrame::RecreatePropsWithParams(bool KeepVals) {
	// Recreate properties with params to update the composed value (because its buggy if not)
	unsigned int NumberOfFits = FitCount->GetValue().GetLong();

	std::vector<std::map<std::wstring, wxString>> MinVals;
	std::vector<std::map<std::wstring, wxString>> MaxVals;
	std::vector<std::map<std::wstring, wxString>> SVVals;

	if (KeepVals) {
		std::cout << "start with " << NumberOfFits << std::endl;
		for (unsigned int i = 1; i <= NumberOfFits; i++)
		{
			std::string PrefixMin = "Fit Bounds Minimum.Fit " + std::to_string(i);
			std::string PrefixMax = "Fit Bounds Maximum.Fit " + std::to_string(i);
			std::string PrefixSV = "Fit Starting Values.Fit " + std::to_string(i);
			wxPGProperty* propMin = FitSettingsGrid->GetProperty(PrefixMin);
			wxPGProperty* propMax = FitSettingsGrid->GetProperty(PrefixMax);
			wxPGProperty* propSV = FitSettingsGrid->GetProperty(PrefixSV);

			if (NumberOfFits == 1) {
				propMin = FitSettingsGrid->GetFirstChild(FitBoundsMin);
				propMax = FitSettingsGrid->GetFirstChild(FitBoundsMax);
				propSV = FitSettingsGrid->GetFirstChild(FitSVs);
			}
			std::map<std::wstring, wxString> MinMap;
			std::map<std::wstring, wxString> MaxMap;
			std::map<std::wstring, wxString> SVMap;

			if (not propSV) {
				i = NumberOfFits;
				propMin = FitBoundsMin;
				propMax = FitBoundsMax;
				propSV = FitSVs;
			}

			std::cout << propSV->GetLabel().ToStdWstring() << std::endl;
			std::cout << propSV->GetValueAsString() << std::endl;

			for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT);
				!it.AtEnd(); it++)
			{
				wxPGProperty* p = *it;
				wxPGProperty* parent = p->GetParent();
				if (parent == propMin) { 
					MinMap[p->GetLabel().ToStdWstring()] = p->GetValueAsString();
				}
				else if (parent == propMax) { 
					MaxMap[p->GetLabel().ToStdWstring()] = p->GetValueAsString();
				}
				else if (parent == propSV) {
					std::cout << parent->GetLabel().ToStdWstring() << std::endl;
					std::cout << p->GetLabel().ToStdWstring() << std::endl;
					std::cout << p->GetValueAsString() << std::endl;
					SVMap[p->GetLabel().ToStdWstring()] = p->GetValueAsString();
				}
			}
			MinVals.push_back(MinMap);
			MaxVals.push_back(MaxMap);
			SVVals.push_back(SVMap);
		}
	}
	
	FitSettingsGrid->DeleteProperty(FitBoundsMin);
	FitSettingsGrid->DeleteProperty(FitBoundsMax);
	FitSettingsGrid->DeleteProperty(FitSVs);

	FitBoundsMin = FitSettingsGrid->Insert(FitMethod,
		new wxStringProperty("Fit Bounds Minimum", wxPG_LABEL));
	FitBoundsMin->SetValueFromString("<composed>");
	FitBoundsMin->SetHelpString("Minimum limits (bounds) of the fit parameters.");

	FitBoundsMax = FitSettingsGrid->Insert(FitMethod,
		new wxStringProperty("Fit Bounds Maximum", wxPG_LABEL));
	FitBoundsMax->SetValueFromString("<composed>");
	FitBoundsMax->SetHelpString("Maximum limits (bounds) of the fit parameters.");

	FitSVs = FitSettingsGrid->Insert(FitData,
		new wxStringProperty("Fit Starting Values", wxPG_LABEL));
	FitSVs->SetValueFromString("<composed>");
	FitSVs->SetHelpString("The start parameters for the values in the fitfunction.");

	for (unsigned int i = 1; i <= NumberOfFits; i++)
	{
		std::wstring FunctionName = L"";
		if (not FitFuncs->IsValueUnspecified()) {
			FunctionName = FitFuncs->GetValueAsString().ToStdWstring();
		}
		else {
			wxString Prefix = "Fitfunction.Fit ";
			wxPGProperty* prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) { FunctionName = prop->GetValueAsString().ToStdWstring(); }
		}

		wxPGProperty* propMin = FitBoundsMin;
		wxPGProperty* propMax = FitBoundsMax;
		wxPGProperty* propSV = FitSVs;


		if (NumberOfFits > 1) {
			propMin = AppendInChildSameType(FitSettingsGrid, FitBoundsMin,
				"Fit " + std::to_string(i), wxPG_LABEL);
			propMax = AppendInChildSameType(FitSettingsGrid, FitBoundsMax,
				"Fit " + std::to_string(i), wxPG_LABEL);
			propSV = AppendInChildSameType(FitSettingsGrid, FitSVs,
				"Fit " + std::to_string(i), wxPG_LABEL);
		}

		if (FunctionName != "") {
			std::vector<std::wstring> params = GetPythonFuncParams(PythonFuncs[FunctionName]);
			for (std::size_t p = 0; p < params.size(); p++) {
				wxPGProperty* pMin = FitSettingsGrid->AppendIn(propMin,
					new wxFloatProperty(params[p], wxPG_LABEL));
				pMin->SetValidator(*eFloatValidator);
				pMin->SetValueToUnspecified();
				wxPGProperty* pMax = FitSettingsGrid->AppendIn(propMax,
					new wxFloatProperty(params[p], wxPG_LABEL));
				pMax->SetValidator(*eFloatValidator);
				pMax->SetValueToUnspecified();
				wxPGProperty* pSV = FitSettingsGrid->AppendIn(propSV,
					new wxFloatProperty(params[p], wxPG_LABEL));
				pSV->SetValidator(*eFloatValidator);
				pSV->SetValueToUnspecified();

				if (KeepVals) {
					if (i - 1 < MinVals.size()) {
						pMin->SetValueFromString(MinVals[i - 1][params[p]]);
					}
					if (i - 1 < MaxVals.size()) {
						pMax->SetValueFromString(MaxVals[i - 1][params[p]]);
					}
					if (i - 1 < SVVals.size()) {
						pSV->SetValueFromString(SVVals[i - 1][params[p]]);
					}
				}
			}
		}
	}
}

void MainFrame::CreateOutputTab() {
	wxStaticText* OutputHeader = new wxStaticText(OutputTab, wxID_ANY, L"Output:");

	OutputHeader->SetMinSize(wxSize(OutputTab->GetMinWidth(),
		OutputHeader->GetMinHeight()));

	OutputText = new wxTextCtrl(OutputTab, wxID_ANY, wxEmptyString, wxDefaultPosition,
		wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

	// Set up the sizer for the contents on FitsTab:
	wxBoxSizer* FitsSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer* HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	HeaderSizer->Add(OutputHeader, 1);
	FitsSizer->Add(HeaderSizer, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);

	wxBoxSizer* GridSizer = new wxBoxSizer(wxHORIZONTAL);
	GridSizer->Add(OutputText, 1, wxEXPAND);
	FitsSizer->Add(GridSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	OutputTab->SetSizer(FitsSizer);

}

void MainFrame::OnFilePicked(wxFileDirPickerEvent& event) {

	ProcessPickedFile();
}

void MainFrame::ProcessPickedFile() {

	// Stop if is not a table file:
	wxString wxPath = FilePicker->GetPath();

	if (wxPath == "") {
		if (ListsCreated) {
			XChoice->Clear();
			XErrChoice->Clear();
			YChoice->Clear();
			YErrChoice->Clear();
			XList->Clear();
			XErrList->Clear();
			YList->Clear();
			YErrList->Clear();
		}
		return;
	}

	const char* Path = wxPath.mb_str();
	if (std::strstr(Path, ".xlsx") == nullptr && std::strstr(Path, ".csv") == nullptr) {
		wxMessageBox("File is not a \".xslx\" or \".csv\" file.");
		return;
	}

	// Stop if table file does not exist:
	if (not std::filesystem::exists(Path)) {
		wxMessageBox("File does not exist.");
		return;
	}

	// If no plot name set, change plot name to file name:
	FileName = FilePicker->GetFileName().GetName();

	// Get column names of picked file:
	const char* Seperator = CSVSettings["Seperator"].c_str();
	const char* Decimal = CSVSettings["Decimal"].c_str();
	PyObject* Args = Py_BuildValue("(sss)", Path, Seperator, Decimal);
	PyObject* ColNames = PyObject_CallObject(GetColNames, Args);
	vector<const char*> cRes = listTupleToVector_String(ColNames);

	Py_DECREF(ColNames);

	// Add DataNames
	for (int i = 0; i < cRes.size(); i++) {
		DataNames.Add(cRes[i]);
	}

	if (DataPanelHidden) {
		DataPanel->Show();
		DataPanelHidden = false;
	}
	// if List already created only update content
	if (ListsCreated) {
		XChoice->Clear();
		XErrChoice->Clear();
		YChoice->Clear();
		YErrChoice->Clear();
		XList->Clear();
		XErrList->Clear();
		YList->Clear();
		YErrList->Clear();

		DataNames.Insert("", 0); // empty field at beginning
		XChoice->Append(DataNames);
		XErrChoice->Append(DataNames);
		YChoice->Append(DataNames);
		YErrChoice->Append(DataNames);
		DataNames.RemoveAt(0); // remove empty field again

		return;
	}
	else {
		// Text widgets:
		XStaticText = new wxStaticText(DataPanel, wxID_ANY, L"x-Data:");
		XErrStaticText = new wxStaticText(DataPanel, wxID_ANY, L"x-Data Error:");
		YStaticText = new wxStaticText(DataPanel, wxID_ANY, L"y-Data:");
		YErrStaticText = new wxStaticText(DataPanel, wxID_ANY, L"y-Data Error:");

		// Buttons:
		AddButton = new wxButton(DataPanel, wxID_ANY, "Add");
		AddButton->SetMinSize(wxSize(900, -1));

		RemoveButton = new wxButton(DataPanel, wxID_ANY, "Remove");
		RemoveButton->SetMinSize(wxSize(900, -1));

		PlotButton = new wxButton(DataPanel, wxID_ANY, "Plot Data");
		PlotButton->SetMinSize(wxSize(100, 75));

		AddButton->Bind(wxEVT_BUTTON, &MainFrame::OnAddClicked, this, wxID_ANY, wxID_ANY);
		RemoveButton->Bind(wxEVT_BUTTON, &MainFrame::OnRemoveClicked, this);
		PlotButton->Bind(wxEVT_BUTTON, &MainFrame::OnPlotClicked, this);

		ListsCreated = true;
	}

	// Choice and list widgets:
	DataNames.Insert("", 0);

	XChoice = new wxChoice(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), DataNames);
	XChoice->SetMinSize(wxSize(200, -1));

	XErrChoice = new wxChoice(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), DataNames);
	XErrChoice->SetMinSize(wxSize(200, -1));

	XList = new wxListBox(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), 0, NULL, wxLB_MULTIPLE);
	XList->SetMinSize(wxSize(200, 150));

	XErrList = new wxListBox(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), 0, NULL, wxLB_MULTIPLE);
	XErrList->SetMinSize(wxSize(200, 150));

	YChoice = new wxChoice(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), DataNames);
	YChoice->SetMinSize(wxSize(200, -1));

	YErrChoice = new wxChoice(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), DataNames);
	YErrChoice->SetMinSize(wxSize(200, -1));

	YList = new wxListBox(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), 0, NULL, wxLB_MULTIPLE);
	YList->SetMinSize(wxSize(200, 150));

	YErrList = new wxListBox(DataPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), 0, NULL, wxLB_MULTIPLE);
	YErrList->SetMinSize(wxSize(200, 150));

	DataNames.RemoveAt(0); // remove empty field again

	// Set Textboxes
	XStaticText->SetMinSize(wxSize(XChoice->GetMinWidth(), XStaticText->GetMinHeight()));
	XErrStaticText->SetMinSize(wxSize(XErrChoice->GetMinWidth(), XErrStaticText->GetMinHeight()));
	YStaticText->SetMinSize(wxSize(YChoice->GetMinWidth(), YStaticText->GetMinHeight()));
	YStaticText->SetMinSize(wxSize(YErrChoice->GetMinWidth(), YErrStaticText->GetMinHeight()));

	// Add Sizers
	DataPanelSizer = new wxBoxSizer(wxVERTICAL);

	DataHeadSizer = new wxBoxSizer(wxHORIZONTAL);
	DataHeadSizer->Add(XStaticText);
	DataHeadSizer->Add(XErrStaticText);
	DataHeadSizer->Add(PlotButton->GetMinWidth(), 0);
	DataHeadSizer->Add(YStaticText);
	DataHeadSizer->Add(YErrStaticText);
	DataPanelSizer->Add(DataHeadSizer);

	DataChoiceSizer = new wxBoxSizer(wxHORIZONTAL);
	DataChoiceSizer->Add(XChoice);
	DataChoiceSizer->Add(XErrChoice);
	DataChoiceSizer->Add(PlotButton->GetMinWidth(), 0);
	DataChoiceSizer->Add(YChoice);
	DataChoiceSizer->Add(YErrChoice);
	DataPanelSizer->Add(DataChoiceSizer);

	DataAddSizer = new wxBoxSizer(wxHORIZONTAL);
	DataAddSizer->Add(AddButton);
	DataPanelSizer->Add(DataAddSizer);

	DataRemoveSizer = new wxBoxSizer(wxHORIZONTAL);
	DataRemoveSizer->Add(RemoveButton);
	DataPanelSizer->Add(DataRemoveSizer);

	DataListSizer = new wxBoxSizer(wxHORIZONTAL);
	DataListSizer->Add(XList);
	DataListSizer->Add(XErrList);
	DataListSizer->Add(PlotButton->GetMinWidth(), 0);
	DataListSizer->Add(YList);
	DataListSizer->Add(YErrList);
	DataPanelSizer->Add(DataListSizer);

	DataPlotSizer = new wxBoxSizer(wxHORIZONTAL);
	DataPlotSizer->Add(XList->GetMinWidth() + XErrList->GetMinWidth(), 0);
	DataPlotSizer->Add(PlotButton);
	DataPanelSizer->Add(DataPlotSizer);

	DataPanel->SetSizer(DataPanelSizer);

	DataSizer->Layout();

	SetSizerAndFit(FrameSizer);

	/*
	// Print client size:
	wxSize ClientSize = GetClientSize();
	int width = ClientSize.GetWidth();
	int height = ClientSize.GetHeight();
	std::cout << width << "," << height << std::endl;
	*/

}

void MainFrame::OnNew(wxCommandEvent& event) {
	ClearAll();
}

void MainFrame::ClearAll() {
	if (ListsCreated) {
		DataPanel->Hide();
		FilePicker->SetPath("Data table file");
		FileName = "";
		DataPanelHidden = true;
		XList->Clear();
		XErrList->Clear();
		YList->Clear();
		YErrList->Clear();
	}

	OutputText->SetValue("");


	for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT); !it.AtEnd(); it++)
	{
		wxPGProperty* prop = *it;
		if (prop != FitCount) {
			prop->GetCell(1).SetText("");
			prop->SetValueToUnspecified();
			prop->DeleteChildren();
			prop->Hide(true);
		}
	}
	FitCount->SetValueFromInt(0);

	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT); !it.AtEnd(); it++)
	{
		wxPGProperty* prop = *it;
		if (prop != ZoomCount) {
			prop->GetCell(1).SetText("");
			prop->SetValueToUnspecified();
			prop->DeleteChildren();
			if (prop->GetParent() == PrZoom and prop != ZoomHelpLines) {
				prop->Hide(true);
			}
		}
	}
	ZoomCount->SetValueFromInt(0);
	if (PGCLegendLabels != NULL and PGCLegendLabels->GetParentState() != NULL) {
		PlotSettingsGrid->DeleteProperty(PGCLegendLabels); 
	}

	PlotSettingsGrid->Refresh();
	FitSettingsGrid->Refresh();
}

void MainFrame::OnOpen(wxCommandEvent& event) {

	wxFileDialog
		openFileDialog(this, _("Open EzPlot file"), "", "",
			"EzPlot file(*.ezp) | *.ezp", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (openFileDialog.ShowModal() == wxID_CANCEL)
		return;     // the user changed idea...

	ClearAll();

	// Read Lines from file:
	std::vector<std::wstring> Lines;
	std::wstring line;
	std::wifstream myfile(openFileDialog.GetPath().ToStdWstring());
	if (myfile.is_open())
	{
		while (std::getline(myfile, line))
		{
			Lines.push_back(line);
		}
		myfile.close();
	}

	FilePicker->SetPath(Lines[0]);
	ProcessPickedFile();
	if (Lines[0] == "") { FilePicker->SetPath("Data table file"); }

	unsigned int XSize = std::stoi(Lines[1]);
	unsigned int XErrSize = std::stoi(Lines[2]);
	unsigned int YSize = std::stoi(Lines[3]);
	unsigned int YErrSize = std::stoi(Lines[4]);

	if (ListsCreated and Lines[0] != "" and std::filesystem::exists(Lines[0])) {
		for (unsigned int i = 0; i < XSize; i++) {
			XList->Append(Lines[5 + i]);
		}
		for (unsigned int i = 0; i < XErrSize; i++) {
			XErrList->Append(Lines[5 + i + XSize]);
		}
		for (unsigned int i = 0; i < YSize; i++) {
			YList->Append(Lines[5 + i + XSize + XErrSize]);
		}
		for (unsigned int i = 0; i < YErrSize; i++) {
			YErrList->Append(Lines[5 + i + XSize + XErrSize + YSize]);
		}
	}

	UpdateSettingsData();

	unsigned int CurrentLineNumber = 5 + XSize + XErrSize + YErrSize + YSize;

	for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT); !it.AtEnd(); it++)
	{
		wxPGProperty* prop = *it;
		if (prop->GetLabel().ToStdString() == Lines[CurrentLineNumber]) {

			std::wstring Line = Lines[CurrentLineNumber + 1];
			if (Line != "") {
				prop->SetValueFromString(Line);
				UpdateSettingsGrid(prop);
			}
			CurrentLineNumber = CurrentLineNumber + 2;
		}
	}
	
	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT); !it.AtEnd(); it++)
	{
		wxPGProperty* prop = *it;
		if (prop->GetLabel().ToStdString() == Lines[CurrentLineNumber]) {

			std::wstring Line = Lines[CurrentLineNumber + 1];
			if (Line != "") {
				prop->SetValueFromString(Line);
				UpdateSettingsGrid(prop);
			}
			CurrentLineNumber = CurrentLineNumber + 2;
		}
	}

}

void MainFrame::OnSave(wxCommandEvent& event) {

	wxFileDialog
		saveFileDialog(this, _("Save this EzPlot Plot"), "", "",
			"EzPlot file (*.ezp)|*.ezp|PNG file (*.png)|*.png|Project folder|",
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

	if (FileName != "") { saveFileDialog.SetFilename(FileName); }

	if (saveFileDialog.ShowModal() == wxID_CANCEL)
		return;     // the user changed idea...

	std::wstring Path = saveFileDialog.GetPath().ToStdWstring();
	char* appdata = getenv("LOCALAPPDATA");
	std::wstring PathToAppData = std::wstring(appdata, appdata+strlen(appdata)) + LR"(\Ezodox\EzPlot\)";
	if (not std::filesystem::exists(PathToAppData)) { 
		std::filesystem::create_directories(PathToAppData);
	}

	if (saveFileDialog.GetFilterIndex() == 1) { // png file
		std::wstring PlotFilePath = PathToAppData + L"plot.png";
		if (std::filesystem::exists(PlotFilePath)) {
			std::filesystem::copy_file(PlotFilePath, Path);
		}
		return;
	}
	else if (saveFileDialog.GetFilterIndex() == 2) { // Project folder

		// Create Project folder and copy plot files into it:
		std::filesystem::create_directories(Path);
		Path = Path + LR"(\)" + saveFileDialog.GetFilename().ToStdWstring();
		std::wstring TableFilePath = FilePicker->GetPath().ToStdWstring();
		if (std::filesystem::exists(TableFilePath)) {
			std::wstring FileType = TableFilePath.substr(TableFilePath.size() - 4);
			std::filesystem::copy_file(TableFilePath, Path + FileType);
		}
		std::wstring PlotFilePath = PathToAppData + L"plot.png";
		if (std::filesystem::exists(PlotFilePath)) {
			std::filesystem::copy_file(PlotFilePath, Path + L".png");
		}
		std::wstring TexFilePath = PathToAppData + L"plot.tex";
		if (std::filesystem::exists(TexFilePath)) {
			std::filesystem::copy_file(TexFilePath, Path + L".tex");
		}
		Path = Path + L".ezp";
	}

	ofstream SaveFile;
	SaveFile.open(Path);

	if (saveFileDialog.GetFilterIndex() == 2) {
		std::wstring TableFilePath = FilePicker->GetPath().ToStdWstring();
		if (std::filesystem::exists(TableFilePath)) {
			std::wstring FileType = TableFilePath.substr(TableFilePath.size() - 4);
			SaveFile << Path.erase(Path.length() - 4) + FileType << "\n";
		}
		else {
			SaveFile << "" << "\n";
		}
	}
	else {
		if (std::filesystem::exists(FilePicker->GetPath().ToStdWstring())) {
			SaveFile << FilePicker->GetPath() << "\n";
		}
		else {
			SaveFile << "" << "\n";
		}
	}

	if (ListsCreated) {
		SaveFile << XList->GetCount() << "\n";
		SaveFile << XErrList->GetCount() << "\n";
		SaveFile << YList->GetCount() << "\n";
		SaveFile << YErrList->GetCount() << "\n";
		for (unsigned int i = 0; i < XList->GetCount(); i++) {
			SaveFile << XList->GetString(i) << "\n";
		}
		for (unsigned int i = 0; i < XErrList->GetCount(); i++) {
			SaveFile << XErrList->GetString(i) << "\n";
		}
		for (unsigned int i = 0; i < YList->GetCount(); i++) {
			SaveFile << YList->GetString(i) << "\n";
		}
		for (unsigned int i = 0; i < YErrList->GetCount(); i++) {
			SaveFile << YErrList->GetString(i) << "\n";
		}
	}
	else {
		SaveFile << 0 << "\n";
		SaveFile << 0 << "\n";
		SaveFile << 0 << "\n";
		SaveFile << 0 << "\n";
	}

	for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT); !it.AtEnd(); it++)
	{
		wxPGProperty* prop = *it;
		SaveFile << prop->GetLabel() << "\n";
		SaveFile << prop->GetValueAsString() << "\n";
	}

	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT); !it.AtEnd(); it++)
	{
		wxPGProperty* prop = *it;
		SaveFile << prop->GetLabel() << "\n";
		SaveFile << prop->GetValueAsString() << "\n";
	}

	SaveFile.close();
	
}

void MainFrame::OnExit(wxCommandEvent& event) {
	Close();
}

void MainFrame::OnEditCSVSettings(wxCommandEvent& event) {

	MainFrame::OpenCSVSettings();

}

void MainFrame::OpenCSVSettings() {

	// if frame already opened, only bring it to the front:
	if (CSVSettingsFrame != NULL) {
		CSVSettingsFrame->Raise();
		return;
	}

	wxPoint MPos = GetPosition();
	CSVSettingsFrame = new wxFrame(MainPanel, wxID_ANY, "CSV Settings", wxPoint(MPos.x+350, MPos.y+250));
	CSVSettingsFrame->SetIcon(wxICON(MainIcon));
	CSVSettingsFrame->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnCSVSettingsWindowClose, this);

	wxPanel* CSVSettingsPanel = new wxPanel(CSVSettingsFrame);

	wxStaticText* SeperatorLabel = new wxStaticText(CSVSettingsPanel, wxID_ANY, L"Seperator:");
	wxStaticText* DecimalLabel = new wxStaticText(CSVSettingsPanel, wxID_ANY, L"Decimal:");

	SeperatorLabel->SetMinSize(wxSize(50, 50));
	DecimalLabel->SetMinSize(wxSize(50, 50));

	wxArrayString Seperators;
	Seperators.Add(",");
	Seperators.Add(";");

	wxArrayString Decimals;
	Decimals.Add(".");
	Decimals.Add(",");

	SeperatorChoice = new wxChoice(CSVSettingsPanel, wxID_ANY, wxPoint(-1, -1),
		wxSize(-1, -1), Seperators);
	SeperatorChoice->SetStringSelection(CSVSettings["Seperator"]);
	SeperatorChoice->Bind(wxEVT_CHOICE, &MainFrame::OnSeperatorSelected, this);

	DecimalChoice = new wxChoice(CSVSettingsPanel, wxID_ANY, wxPoint(-1, -1),
		wxSize(-1, -1), Decimals);
	DecimalChoice->SetStringSelection(CSVSettings["Decimal"]);
	DecimalChoice->Bind(wxEVT_CHOICE, &MainFrame::OnDecimalSelected, this);

	wxBoxSizer* SeperatorSizer = new wxBoxSizer(wxHORIZONTAL);
	SeperatorSizer->Add(SeperatorLabel, 0, wxRIGHT, 20);
	SeperatorSizer->Add(SeperatorChoice, 0);

	wxBoxSizer* DecimalSizer = new wxBoxSizer(wxHORIZONTAL);
	DecimalSizer->Add(DecimalLabel, 0, wxRIGHT, 20);
	DecimalSizer->Add(DecimalChoice, 0);

	// Set up the sizer for the contents on CSVSettingsPanel:
	wxBoxSizer* CSVPanelSizer = new wxBoxSizer(wxVERTICAL);
	CSVPanelSizer->Add(SeperatorSizer, 0, wxEXPAND | wxALL, 10);
	CSVPanelSizer->Add(DecimalSizer, 0, wxEXPAND | wxALL, 10);
	CSVSettingsPanel->SetSizer(CSVPanelSizer);

	// Set up the sizer for the contents on CSVSettingsFrame:
	wxBoxSizer* CSVSettingsFrameSizer = new wxBoxSizer(wxHORIZONTAL);
	CSVSettingsFrameSizer->Add(CSVSettingsPanel, 1, wxEXPAND);
	CSVSettingsFrame->SetSizerAndFit(CSVSettingsFrameSizer);

	CSVSettingsFrame->SetMinSize(wxSize(250, 150));

	CSVSettingsFrame->Show();

}

void MainFrame::OnEditFunctions(wxCommandEvent& event) {

	MainFrame::OpenFunctionsWindow();

}

void MainFrame::OpenFunctionsWindow() {

	// if frame already opened, only bring it to the front:
	if (FuncsFrame != NULL) {
		FuncsFrame->Raise();
		return;
	}

	FuncParams.Clear();

	wxPoint MPos = GetPosition();
	FuncsFrame = new wxFrame(MainPanel, wxID_ANY, "Fitfunctions", wxPoint(MPos.x+50,MPos.y+50));
	FuncsFrame->SetIcon(wxICON(MainIcon));
	FuncsFrame->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnFunctionsWindowClose, this);

	wxPanel* FuncsPanel = new wxPanel(FuncsFrame);

	wxStaticText* FuncChoiceHeader = new wxStaticText(FuncsPanel, wxID_ANY, L"Fitfunction:");

	wxArrayString FitFunctions;

	for (auto element : PythonFuncs) {
		FitFunctions.Add(element.first);
	}

	FitFunctions.Add(L"[new]");

	FuncChoice = new wxChoice(FuncsPanel, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1), FitFunctions);
	FuncChoice->SetStringSelection(L"[new]");
	FuncChoice->Bind(wxEVT_CHOICE, &MainFrame::OnFuncSelected, this);

	FuncSettingsGrid = new wxPropertyGrid(FuncsPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxPG_SPLITTER_AUTO_CENTER | wxPG_DEFAULT_STYLE);
	FuncSettingsGrid->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
	FuncSettingsGrid->ClearActionTriggers(wxPG_ACTION_PREV_PROPERTY);
	FuncSettingsGrid->ClearActionTriggers(wxPG_ACTION_NEXT_PROPERTY);
	FuncSettingsGrid->DedicateKey(WXK_RETURN);
	FuncSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_RETURN);
	FuncSettingsGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY, WXK_RETURN);
	FuncSettingsGrid->DedicateKey(WXK_UP);
	FuncSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_UP);
	FuncSettingsGrid->AddActionTrigger(wxPG_ACTION_PREV_PROPERTY, WXK_UP);
	FuncSettingsGrid->DedicateKey(WXK_DOWN);
	FuncSettingsGrid->AddActionTrigger(wxPG_ACTION_EDIT, WXK_DOWN);
	FuncSettingsGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY, WXK_DOWN);

	FuncSettingsGrid->Append(new wxPropertyCategory("Function"));

	FuncName = FuncSettingsGrid->Append(new wxStringProperty("Name"));
	FuncName->SetValueToUnspecified();
	FuncName->SetHelpString("Name of the function. Remove the name and save to delete the function.");
	FuncName->SetAttribute(L"Hint", "Example");

	FuncPython = FuncSettingsGrid->Append(new wxStringProperty("Python Code"));
	FuncPython->SetValueToUnspecified();
	FuncPython->SetHelpString("Python code of the function.\n"
		"Just the calculating expression without \"return\" or \"def ...\"");
	FuncPython->SetAttribute(L"Hint", "A*x**2 + B*x + C/numpy.sqrt(2) + D*numpy.pi - numpy.exp(x)");

	FuncLatex = FuncSettingsGrid->Append(new wxStringProperty("LaTeX Code"));
	FuncLatex->SetValueToUnspecified();
	FuncLatex->SetHelpString("LaTeX math code of the function for the LaTeX table.");
	FuncLatex->SetAttribute(L"Hint", R"(\alpha \cdot x^2 + B\,x + \frac{C}{\sqrt{2}} ...)");

	ParamsCategory = FuncSettingsGrid->Append(new wxPropertyCategory("Params"));

	wxPGCell LatexHeader = ParamsCategory->GetCell(1);
	LatexHeader.SetText("Latex");

	ParamsCategory->SetCell(1, LatexHeader);

	wxButton* SaveFuncButton = new wxButton(FuncsPanel, wxID_ANY, "Save");
	SaveFuncButton->SetMinSize(wxSize(100, 75));
	SaveFuncButton->Bind(wxEVT_BUTTON, &MainFrame::OnSaveFuncClicked, this, -1);

	FuncsFrame->SetMinSize(wxSize(800, 500));

	// sizer for the save button:
	wxBoxSizer* SaveFuncButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	SaveFuncButtonSizer->Add(100, 0, 1);
	SaveFuncButtonSizer->Add(SaveFuncButton);
	SaveFuncButtonSizer->Add(100, 0, 1);

	// Set up the sizer for the contents on FuncsPanel:
	wxBoxSizer* FuncsPanelSizer = new wxBoxSizer(wxVERTICAL);
	FuncsPanelSizer->Add(FuncChoiceHeader, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 20);
	FuncsPanelSizer->Add(FuncChoice, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
	FuncsPanelSizer->Add(FuncSettingsGrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
	FuncsPanelSizer->Add(SaveFuncButtonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
	FuncsPanelSizer->Add(0, 20);
	FuncsPanel->SetSizer(FuncsPanelSizer);

	// Set up the sizer for the contents on FuncsFrame:
	wxBoxSizer* FuncsFrameSizer = new wxBoxSizer(wxHORIZONTAL);
	FuncsFrameSizer->Add(FuncsPanel, 1, wxEXPAND);
	FuncsFrame->SetSizer(FuncsFrameSizer);

	FuncsFrame->Show();

}

void MainFrame::OnCSVSettingsWindowClose(wxCloseEvent& event) {
	CSVSettingsFrame = NULL;
	event.Skip();
}

void MainFrame::OnFunctionsWindowClose(wxCloseEvent& event) {
	FuncsFrame = NULL;
	event.Skip();
}

void MainFrame::OnPlotWindowClose(wxCloseEvent& event) {
	PlotFrame = NULL;
	event.Skip();
}

void MainFrame::OnSeperatorSelected(wxCommandEvent& event) {
	CSVSettings["Seperator"] = SeperatorChoice->GetStringSelection().ToStdString();
	SaveCSVSettings();
}

void MainFrame::OnDecimalSelected(wxCommandEvent& event) {
	CSVSettings["Decimal"] = DecimalChoice->GetStringSelection().ToStdString();
	SaveCSVSettings();
}

void MainFrame::SaveCSVSettings() {
	// Write in CSVSettings data file:
	char* appdata = getenv("LOCALAPPDATA");
	std::wstring PathToAppData = std::wstring(appdata, appdata + strlen(appdata)) + LR"(\Ezodox\EzPlot\)";
	if (not std::filesystem::exists(PathToAppData)) {
		std::filesystem::create_directories(PathToAppData);
	}
	ofstream CSVSettingsFile;
	CSVSettingsFile.open(PathToAppData + L"CSV Settings.dat");
	CSVSettingsFile << "Seperator" << "\n";
	CSVSettingsFile << CSVSettings["Seperator"] << "\n";
	CSVSettingsFile << "Decimal" << "\n";
	CSVSettingsFile << CSVSettings["Decimal"] << "\n";
}

void MainFrame::OnFuncSelected(wxCommandEvent& event) {

	ParamsCategory->DeleteChildren();
	FuncParams.Clear();
	if (FuncChoice->GetStringSelection() == L"[new]") {
		FuncName->SetValueToUnspecified();
		FuncPython->SetValueToUnspecified();
		FuncLatex->SetValueToUnspecified();
	}
	else {
		FuncName->SetValueFromString(FuncChoice->GetStringSelection());
		FuncPython->SetValueFromString(PythonFuncs[FuncChoice->GetStringSelection().ToStdWstring()]);
		FuncLatex->SetValueFromString(LatexFuncs[FuncChoice->GetStringSelection().ToStdWstring()]);

		std::vector<std::wstring> Params = GetPythonFuncParams(FuncPython->GetValueAsString().ToStdWstring());
		std::vector<std::wstring> Latexs = LatexParams[FuncChoice->GetStringSelection().ToStdWstring()];
		for (std::size_t p = 0; p < Params.size(); p++) {

			wxPGProperty* ParamProp = FuncSettingsGrid->Append(new wxStringProperty(Params[p],
				wxPG_LABEL, wxString(Latexs[p])));
			ParamProp->SetHelpString("LaTex math code of the param for the LaTeX table.");
			FuncParams.Add(ParamProp);
		}
	}
	FuncSettingsGrid->Refresh();
}

void MainFrame::StoreFunctionVariables() {
	PythonFuncs[FuncName->GetValueAsString().ToStdWstring()]
		= FuncPython->GetValueAsString().ToStdWstring();
	LatexFuncs[FuncName->GetValueAsString().ToStdWstring()]
		= FuncLatex->GetValueAsString().ToStdWstring();
	std::vector<std::wstring> Latexs;
	for (unsigned int i = 0; i < FuncParams.GetCount(); i++) {
		wxPGProperty* Prop = FuncParams.Item(i);
		Latexs.push_back(Prop->GetValueAsString().ToStdWstring());
	}
	LatexParams[FuncName->GetValueAsString().ToStdWstring()] = Latexs;
}

void MainFrame::OnSaveFuncClicked(wxCommandEvent& event) {

	if (FuncChoice->GetStringSelection() == L"[new]") {
		if (FuncName->IsValueUnspecified() or FuncName->GetValueAsString() == "") {
			wxMessageBox("Please type in a Function name.");
			return;
		}
		else {
			for (unsigned int i = 0; i < FuncChoice->GetCount(); i++) {
				if (FuncChoice->GetString(i) == FuncName->GetValueAsString()) {
					wxMessageBox("Function Name already defined.");
					return;
				}
			}
		}
	}

	// Write in Fitfunctions data and python file:
	char* appdata = getenv("LOCALAPPDATA");
	std::wstring PathToAppData = std::wstring(appdata, appdata + strlen(appdata)) + LR"(\Ezodox\EzPlot\)";
	if (not std::filesystem::exists(PathToAppData)) {
		std::filesystem::create_directories(PathToAppData);
	}
	ofstream FunctionDatFile;
	FunctionDatFile.open(PathToAppData + L"Fitfunctions.dat");
	ofstream FunctionsPyFile;
	FunctionsPyFile.open(PathToAppData + L"PyFitfunctions.py");
	FunctionsPyFile << "import numpy" << "\n";
	FunctionsPyFile << "import scipy" << "\n";

	wxArrayString FitFunctions;
	for (auto element : PythonFuncs) {
		if (element.first == "") { continue; }

		// Write in Fitfunctions.dat file:
		FunctionDatFile << element.first << "\n"; // FunctionName
		FunctionDatFile << element.second << "\n"; // FunctionPython
		FunctionDatFile << LatexFuncs[element.first] << "\n"; // FunctionLatex
		std::vector<std::wstring> Latexs = LatexParams[element.first];
		for (std::size_t i = 0; i < Latexs.size(); i++) {
			FunctionDatFile << Latexs[i] << "\n"; // FunctionLatexParams
		}

		// Write in PyFitfunctions.py file:
		std::wstring Val = L"def ";
		std::wstring PythonFunc = L"def " + element.first + L"(x, ";
		std::vector<std::wstring> PythonParams = GetPythonFuncParams(element.second);
		for (std::size_t i = 0; i < PythonParams.size(); i++) {
			PythonFunc = PythonFunc + PythonParams[i] + ", ";
		}
		PythonFunc.resize(PythonFunc.size() - 2);
		PythonFunc = PythonFunc + "): return " + element.second;
		FunctionsPyFile << PythonFunc << "\n";

		FitFunctions.Add(element.first);
	}

	FunctionDatFile.close();
	FunctionsPyFile.close();

	// Reload python functions:
	fitfunctions_module = PyImport_ReloadModule(fitfunctions_module);

	// Update Function Choice Widgets:
	FitFunctions.Add(L"[new]");
	FuncChoice->Set(FitFunctions);
	FuncChoice->SetStringSelection(L"[new]");

	wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT, FitFuncs);
	for (it; !it.AtEnd(); it++)
	{
		wxPGProperty* CurrentProp = *it;
		if (CurrentProp != FitFuncs and CurrentProp->GetParent() != FitFuncs) {
			break;
		}
		wxString SelectedFunc = CurrentProp->GetValueAsString();
		CurrentProp->SetChoices(FitFunctions);
		if (CurrentProp->IsVisible() and FitSettingsGrid->GetSelectedProperty() == CurrentProp
			and FuncName->GetValueAsString() != "") {
			FitSettingsGrid->ChangePropertyValue(CurrentProp, FuncName->GetValueAsString());
			//CurrentProp->SetValueFromString(FuncName->GetValueAsString());
			CurrentProp->RefreshEditor();
		}
		else {
			CurrentProp->SetValueFromString(SelectedFunc);
		}
	}

	FitSettingsGrid->Refresh();

	FuncName->SetValueToUnspecified();
	FuncPython->SetValueToUnspecified();
	FuncLatex->SetValueToUnspecified();
	ParamsCategory->DeleteChildren();
	FuncParams.Clear();
	FuncSettingsGrid->Refresh();

}

// Stuff to remove duplicates from vector:
struct target_less
{
	template<class It>
	bool operator()(It const& a, It const& b) const { return *a < *b; }
};
struct target_equal
{
	template<class It>
	bool operator()(It const& a, It const& b) const { return *a == *b; }
};
template<class It> It uniquify(It begin, It const end)
{
	std::vector<It> v;
	v.reserve(static_cast<size_t>(std::distance(begin, end)));
	for (It i = begin; i != end; ++i)
	{
		v.push_back(i);
	}
	std::sort(v.begin(), v.end(), target_less());
	v.erase(std::unique(v.begin(), v.end(), target_equal()), v.end());
	std::sort(v.begin(), v.end());
	size_t j = 0;
	for (It i = begin; i != end && j != v.size(); ++i)
	{
		if (i == v[j])
		{
			using std::iter_swap; iter_swap(i, begin);
			++j;
			++begin;
		}
	}
	return begin;
}
template <typename T>
void RemoveDuplicates(std::vector<T>& v)
{
	v.erase(uniquify(v.begin(), v.end()), v.end());
}

std::vector<std::wstring> MainFrame::GetPythonFuncParams(std::wstring PythonFunction) {
	std::smatch match;
	// match all parameters excluding x, exponential factors and functions:
	// No lookbehind support for, so matching character before:
	// (?!((?<=\\d)[eE][-\\+]?\\d|x[^\\w\\.\\_\\(]|$))[a-zA-Z]\\w*(?=[^\\w\\.\\_\\(]|$)
	
	std::wregex exp(LR"((?!(\d[eE][-\+]?\d|(.|^)x([^\w\.\_\(]|$))))"
		LR"(([^\.a-zA-Z]|^)[a-zA-Z]\w*(?=[^\w\.\_\(]|$))");
	std::vector<std::wstring> PythonParams;

	for (auto it = std::wsregex_iterator(PythonFunction.begin(), PythonFunction.end(), exp);
		it != std::wsregex_iterator(); ++it) {
		std::wstring matchstr = it->str(0);
		if (not isalpha(matchstr.front())) {
			matchstr.erase(0, 1); // if first character is not alphabetic remove it
		}
		PythonParams.push_back(matchstr);
	}

	RemoveDuplicates(PythonParams);

	return PythonParams;
}

std::wstring MainFrame::ReplaceParamsWithLatex(std::wstring Function, std::vector<std::wstring> Params, 
	std::vector<std::wstring> Latex) {
	std::smatch match;
	// match all parameters excluding x and functions:
	// No lookbehind support for, so matching character before:
	// (?!((?<=\\d)[eE][-\\+]?\\d|x[^\\w\\.\\_\\(]|$))[a-zA-Z]\\w*(?=[^\\w\\.\\_\\(]|$)
	std::wregex exp(LR"((?!(\d[eE][-\+]?\d|(.|^)x([^\w\.\_\(]|$))))"
		LR"(([^\.a-zA-Z]|^)[a-zA-Z]\w*(?=[^\w\.\_\(]|$))");
	std::wstring ReplacedFunction = Function;
	int StrOffset = 0;
	for (auto it = std::wsregex_iterator(Function.begin(), Function.end(), exp); 
		it != std::wsregex_iterator(); ++it) {
		std::wstring matchstr = it->str(0);
		int pos = it->position(0);
		int length = it->length(0);
		if (not isalpha(matchstr.front())) {
			matchstr.erase(0, 1); // if first character is not alphabetic remove it
			pos++;
			length--;
		}
		for (std::size_t i = 0; i < Params.size(); i++) {
			if (Params[i] == matchstr) {	
				ReplacedFunction.replace(pos + StrOffset, length, Latex[i]);
				StrOffset = StrOffset + (Latex[i].length() - length);
			}
		}
	}

	return ReplacedFunction;
}

void MainFrame::UpdateSettingsData() {

	// Update Fitted Data Properties:
	wxPropertyGridIterator itf = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT, FitData);
	for (itf; !itf.AtEnd(); itf++)
	{
		wxPGProperty* CurrentProp = *itf;
		if (CurrentProp != FitData and CurrentProp->GetParent() != FitData) {
			break;
		}
		wxPGChoices Datas;
		if (YList != NULL) {
			Datas = YList->GetStrings();
		}
		CurrentProp->SetChoices(Datas);
	}


	wxPGEditor* wxPGSpinOrderEditor = wxPropertyGrid::RegisterEditorClass(
		new wxSpinOrderCtrlEditor());


	// Delete previous created children of data:
	wxArrayString TrashLabels;
	wxArrayString FitLabels;
	for (long i = 1; i <= FitCount->GetValue().GetLong(); i++) {
		FitLabels.Add("Fit " + std::to_string(i));
	}
	FitLabels.Add("Underground");
	FitLabels.Add("MeanLine");
	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(); !it.AtEnd(); it++)
	{
		wxPGProperty* Property = *it;
		wxPGProperty* Parent = Property->GetParent();

		if (Parent == Markers) {
			Property->DeleteChildren();
			Property->GetCell(1).SetText("");
		}
		else if (Parent == PGCLegendLabels) {
			wxString Label = Property->GetLabel();
			int DataIndex = YList->FindString(Label);
			if (DataIndex == wxNOT_FOUND and FitLabels.Index(Label) == wxNOT_FOUND) {
				TrashLabels.Add(Label);
			}
		}
	}
	for (std::size_t i = 0; i < TrashLabels.GetCount(); i++) {
		PlotSettingsGrid->DeleteProperty("Legend Labels." + TrashLabels.Item(i));
	}

	wxPGProperty* Prop = PlotSettingsGrid->GetPropertyByName("Legend Labels");
	if (not Prop) {
		wxString PropNameAfter = "Legend relative X-Position";
		PGCLegendLabels = new wxPropertyCategory("Legend Labels");
		PlotSettingsGrid->Insert(PropNameAfter, PGCLegendLabels);
	}


	unsigned int DataNum = 0;
	if (YList != NULL) { DataNum = YList->GetCount(); }

	// Create new children of data:
	for (unsigned int i = 0; i < DataNum; i++) {

		// Create new children of data for legend labels:
		wxString ChildLabel = YList->GetString(i);
		wxPGProperty* Child = PlotSettingsGrid->GetProperty("Legend Labels." + ChildLabel);
		if (not Child) {
			wxPGProperty* FirstFit = PlotSettingsGrid->GetProperty("Legend Labels.Fit 1");
			if (FirstFit) {
				PlotSettingsGrid->Insert(FirstFit, new wxStringProperty(ChildLabel, wxPG_LABEL));
			}
			else {
				PlotSettingsGrid->AppendIn(PGCLegendLabels, new wxStringProperty(ChildLabel, wxPG_LABEL));
			}
			PlotSettingsGrid->SetPropertyEditor(ChildLabel, wxPGSpinOrderEditor);
		}

		// Create new children of data for marker settings:
		if (DataNum > 1) {
			wxPGProperty* FirstChild = PlotSettingsGrid->GetFirstChild(Markers);
			for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
				FirstChild); !it.AtEnd(); it++)
			{
				wxPGProperty* CurrentProp = *it;
				wxPGProperty* CurrentParent = CurrentProp->GetParent();
				if (CurrentParent == Markers) {
					AppendInChildSameType(PlotSettingsGrid, CurrentProp, ChildLabel, wxPG_LABEL);
				}
				else if (CurrentProp == PlotXLabel) {
					break;
				}
			}
		}
	}
	if (PGCLegendLabels->GetChildCount() == 0) {
		PlotSettingsGrid->DeleteProperty(PGCLegendLabels);
	}

	// Collapse the parents of childrens:
	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(); !it.AtEnd(); it++)
	{
		wxPGProperty* Property = *it;
		wxPGProperty* Parent = Property->GetParent();

		if (Parent == Markers) {

			PlotSettingsGrid->Collapse(Property);
		}
	}

}


void MainFrame::OnAddClicked(wxCommandEvent& event) {

	wxString XSelected = XChoice->GetStringSelection();
	wxString YSelected = YChoice->GetStringSelection();
	if (XSelected == "" || YSelected == "") {
		return; // if no data selected stop
	}
	wxString XErrSelected = XErrChoice->GetStringSelection();
	wxString YErrSelected = YErrChoice->GetStringSelection();
	if (XErrSelected == "") {
		XErrSelected = "[NULL]";
	}
	if (YErrSelected == "") {
		YErrSelected = "[NULL]";
	}

	int SameY = YList->FindString(YSelected);
	if (SameY == wxNOT_FOUND) {
		XList->Append(XSelected);
		XErrList->Append(XErrSelected);
		YList->Append(YSelected);
		YErrList->Append(YErrSelected);
	}
	else { // if Y-Data already added only replace Error
		XErrList->SetString(SameY, XErrSelected);
		YErrList->SetString(SameY, YErrSelected);
	}

	UpdateSettingsData();

}

std::string MainFrame::GetPythonOutput() {
	PyErr_Print(); 
	PyObject* output = PyObject_GetAttrString(catcher, "value"); 
	return _PyUnicode_AsString(output);
}

void MainFrame::PrintPythonOutput() {

	printf("Here's the output:\n %s", GetPythonOutput().c_str());

}

void MainFrame::ClearPythonOutput() {
	PyObject_SetAttrString(catcher, "value", Py_BuildValue("s",""));
}

int compare_int(int* a, int* b)
{
	if (*a > *b) return 1;
	else if (*a < *b) return -1;
	else return 0;
}

void MainFrame::OnRemoveClicked(wxCommandEvent& event) {

	wxArrayInt ErrSelections;
	wxArrayInt xSelections;
	wxArrayInt ySelections;
	XErrList->GetSelections(ErrSelections);
	XList->GetSelections(xSelections);
	YList->GetSelections(ySelections);

	// if nothing selected, delete the selected data in choice widgets
	if (ErrSelections == NULL && xSelections == NULL && ySelections == NULL) {
		wxString XSelected = XChoice->GetStringSelection();
		wxString XSelectedErr = XErrChoice->GetStringSelection();
		wxString YSelected = YChoice->GetStringSelection();
		wxString YSelectedErr = YErrChoice->GetStringSelection();
		int Count = YList->GetCount();
		wxString XItem;
		wxString XItemErr;
		wxString YItem;
		wxString YItemErr;
		for (int i = Count - 1; i >= 0; i--) {
			XItem = XList->GetString(i);
			XItemErr = XErrList->GetString(i);
			YItem = YList->GetString(i);
			YItemErr = YErrList->GetString(i);
			if ((XSelected != "" && XItem == XSelected) && (YSelected != "" && YItem == YSelected)) {
				XErrList->Delete(i);
				XList->Delete(i);
				YErrList->Delete(i);
				YList->Delete(i);
			}
			else {
				if ((XSelected == "") && (XSelectedErr != "" && XItemErr == XSelectedErr)) {
					XErrList->SetString(i, "[NULL]");
				}
				if ((YSelected == "") && (YSelectedErr != "" && YItemErr == YSelectedErr)) {
					YErrList->SetString(i, "[NULL]");
				}
			}
		}
	
	}
	else {

		// Empty selected items in error list:
		int sCount = ErrSelections.GetCount();
		for (int i = 0; i < sCount; i++) {
			XErrList->SetString(ErrSelections[i], "[NULL]");
		}

		YErrList->GetSelections(ErrSelections);
		sCount = ErrSelections.GetCount();
		for (int i = 0; i < sCount; i++) {
			YErrList->SetString(ErrSelections[i], "[NULL]");
		}

		// Delete selected items in data list, for that union selection arrays and delete backwards
		int sxCount = xSelections.GetCount();
		int syCount = ySelections.GetCount();
		for (int i = 0; i < syCount; i++) {
			bool Duplicate = false;
			for (int k = 0; k < sxCount; k++) {
				if (xSelections[k] == ySelections[i]) {
					Duplicate = true;
					break;
				}
			}
			if (Duplicate) { continue; }
			xSelections.Add(ySelections[i]);
		}

		xSelections.Sort(compare_int);
		sxCount = xSelections.GetCount();

		for (int i = sxCount - 1; i >= 0; i--) {

			XErrList->Delete(xSelections[i]);
			XList->Delete(xSelections[i]);
			YErrList->Delete(xSelections[i]);
			YList->Delete(xSelections[i]);
		}
	}

	UpdateSettingsData();

}

/*
bool MainFrame::SettingsChanged(std::unordered_map<std::string, std::any> Settings1,
	std::unordered_map<std::string, std::any> Settings2) {

	bool SettingsChanged = false;
	for (auto element : Settings1) {
		if (element.second.type() == typeid(int)) {
			if (std::any_cast<int>(element.second) != std::any_cast<int>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(long)) {
			if (std::any_cast<long>(element.second) != std::any_cast<long>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(float)) {
			if (std::any_cast<float>(element.second) != std::any_cast<float>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(double)) {
			if (std::any_cast<double>(element.second) != std::any_cast<double>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(bool)) {
			if (std::any_cast<bool>(element.second) != std::any_cast<bool>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(const char*)) {
			if (std::any_cast<const char*>(element.second) 
				!= std::any_cast<const char*>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(const wchar_t*)) {
			if (std::any_cast<const wchar_t*>(element.second) 
				!= std::any_cast<const wchar_t*>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::string)) {
			if (std::any_cast<std::string>(element.second) 
				!= std::any_cast<std::string>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::wstring)) {
			if (std::any_cast<wstring>(element.second) 
				!= std::any_cast<wstring>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}

		else if (element.second.type() == typeid(std::vector<int>)) {
			if (std::any_cast<std::vector<int>>(element.second) 
				!= std::any_cast<std::vector<int>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<long>)) {
			if (std::any_cast<std::vector<long>>(element.second) 
				!= std::any_cast<std::vector<long>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<double>)) {
			if (std::any_cast<std::vector<double>>(element.second) 
				!= std::any_cast<std::vector<double>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<bool>)) {
			if (std::any_cast<std::vector<bool>>(element.second) 
				!= std::any_cast<std::vector<bool>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<const char*>)) {
			if (std::any_cast<std::vector<const char*>>(element.second) 
				!= std::any_cast<std::vector<const char*>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<std::string>)) {
			if (std::any_cast<std::vector<std::string>>(element.second) 
				!= std::any_cast<std::vector<std::string>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<std::wstring>)) {
			if (std::any_cast<std::vector<std::wstring>>(element.second) 
				!= std::any_cast<std::vector<std::wstring>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<std::tuple<double, double>>)) {
			if (std::any_cast<std::vector<std::tuple<double, double>>>(element.second) 
				!= std::any_cast<std::vector<std::tuple<double, double>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<std::tuple<std::string, std::string>>)) {
			if (std::any_cast<std::vector<std::tuple<std::string, std::string>>>(element.second) 
				!= std::any_cast<std::vector<std::tuple<std::string,
				std::string>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<std::vector<double>>)) {
			if (std::any_cast<std::vector<std::vector<double>>>(element.second) 
				!= std::any_cast<std::vector<std::vector<double>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector <std::vector<std::tuple<double, double>>>)) {
			if (std::any_cast<std::vector <std::vector<std::tuple<double, double>>>>(element.second)
				!= std::any_cast<std::vector <std::vector<std::tuple<double,
				double>>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::vector<std::vector<std::vector<double>>>)) {
			if (std::any_cast<std::vector<std::vector<std::vector<double>>>>(element.second) 
				!= std::any_cast<std::vector<std::vector<std::vector<double>>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::tuple<double, double>)) {
			if (std::any_cast<std::tuple<double, double>>(element.second) 
				!= std::any_cast<std::tuple<double, double>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::tuple<std::string, std::string>)) {
			if (std::any_cast<std::tuple<std::string, std::string>>(element.second) 
				!= std::any_cast<std::tuple<std::string, std::string>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::unordered_map<std::string, std::string>)) {
			if (std::any_cast<std::unordered_map<std::string, std::string>>(element.second) 
				!= std::any_cast<std::unordered_map<std::string, std::string>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::unordered_map<std::string, std::wstring>)) {
			if (std::any_cast<std::unordered_map<std::string, std::wstring>>(element.second) 
				!= std::any_cast<std::unordered_map<std::string, std::wstring>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::unordered_map<std::wstring, std::wstring>)) {
			if (std::any_cast<std::unordered_map<std::wstring, std::wstring>>(element.second) 
				!= std::any_cast<std::unordered_map<std::wstring, std::wstring>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::unordered_map<std::string,
			std::vector<std::string>>)) {
			if (std::any_cast<std::unordered_map<std::string,
				std::vector<std::string>>>(element.second) 
				!= std::any_cast<std::unordered_map<std::string,
				std::vector<std::string>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::unordered_map<std::string,
			std::vector<std::wstring>>)) {
			if (std::any_cast<std::unordered_map<std::string,
				std::vector<std::wstring>>>(element.second) != std::any_cast<std::unordered_map<std::string,
				std::vector<std::wstring>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::unordered_map<std::wstring,
			std::vector<std::wstring>>)) {
			if (std::any_cast<std::unordered_map<std::wstring,
				std::vector<std::wstring>>>(element.second) != std::any_cast<std::unordered_map<std::wstring,
				std::vector<std::wstring>>>(Settings2[element.first])) {
				SettingsChanged = true;
				break;
			}
		}
		else if (element.second.type() == typeid(std::nullopt_t)) {
			if (Settings2[element.first].type() != typeid(std::nullopt_t)) {
				SettingsChanged = true;
				break;
			}
		}
	}

	return SettingsChanged;
}

*/

void MainFrame::OnPlotClicked(wxCommandEvent& event) {
	
	/*
	if (PlotFrame != NULL) {
		PlotFrame->Close();
	}
	*/

	// If plot window already exist close previous plot window:
	HWND PlotWindow = FindWindowA(NULL, "Figure 1");
	if (PlotWindow != NULL) {
		PostMessage(PlotWindow, WM_CLOSE, 0, 0);
		return;
	}
	CreatePlot();
}

void MainFrame::CreatePlot() {

	ClearPythonOutput();

	PyObject* PyDataInfos = ToPyObject(GetDataInfos());
	PyObject* PyPlotSettings = ToPyObject(GetPlotSettings());
	PyObject* PyFitFunctions = GetFitFunctions();
	PyObject* PyFitSettings = ToPyObject(GetFitSettings());
	PyObject* FitArgs = PyTuple_Pack(4, PyDataInfos, PyPlotSettings, PyFitFunctions, PyFitSettings);
	PyObject* OutErr = PyObject_CallObject(CPlot, FitArgs);

	//PyObject* args = PyTuple_Pack(4, PyDataInfos, PyPlotSettings, PyFitFunctions, PyFitSettings);
	//PyObject* result = PyObject_CallObject(CPlot, args);
	long cRes = PyLong_AsLong(OutErr);

	OutputText->SetValue(GetPythonOutput());
	PrintPythonOutput();

	PyObject_CallObject(ShowPlot, NULL);

	//Py_DECREF(CPlot);

	if (cRes != 1) {
		wxMessageBox(
			"Something went wrong. \n"
			"Possibly an error with latex coding in labels. \n"
			"Try to change the axis or legend labels. \n"
			"Also check if CSV seperator and decimal signs are correct.");
		return;
	}
}

template <typename T>
PyObject* VectorToPyList(const std::vector<T>& cpp_vec, PyObject* (*ConvertToPy)(const T&)) {
	assert(cpython_asserts());
	PyObject* r = PyList_New(cpp_vec.size());
	if (!r) {
		goto except;
	}
	for (std::size_t i = 0; i < cpp_vec.size(); ++i) {
		PyObject* item = (*ConvertToPy)(cpp_vec[i]);
		if (!item || PyErr_Occurred() || PyList_SetItem(r, i, item)) {
			goto except;
		}
	}
	assert(!PyErr_Occurred());
	assert(r);
	goto finally;
except:
	assert(PyErr_Occurred());
	// Clean up list
	if (r) {
		// No PyList_Clear().
		for (Py_ssize_t i = 0; i < PyList_GET_SIZE(r); ++i) {
			Py_XDECREF(PyList_GET_ITEM(r, i));
		}
		Py_DECREF(r);
		r = NULL;
	}
	finally:
	return r;
}

PyObject* IntToPy(const int& Val) {
	return PyLong_FromLong(Val);
}

PyObject* LongToPy(const long& Val) {
	return PyLong_FromLong(Val);
}

PyObject* DoubleToPy(const double& Val) {
	return PyFloat_FromDouble(Val);
}

PyObject* StringToPy(const std::string& str) {
	return PyUnicode_DecodeRawUnicodeEscape(str.c_str(), str.size(), "error");
}

PyObject* WStringToPy(const std::wstring& str) {
	return PyUnicode_FromWideChar(str.c_str(), wcslen(str.c_str()));
}

PyObject* StringToPyByte(const std::string& str) {
	return PyBytes_FromStringAndSize(str.c_str(), str.size());
}

PyObject* WCharToPy(const wchar_t* str) {
	return PyUnicode_FromWideChar(str, wcslen(str));
}

PyObject* BoolToPy(const bool& Val) {
	return PyBool_FromLong(Val);
}

PyObject* SingleValToPy(const std::any& Val) {
	PyObject* PyVal = Py_None;
	if (Val.type() == typeid(int)) {
		PyVal = PyLong_FromLong(std::any_cast<int>(Val));
	}
	else if (Val.type() == typeid(long)) {
		PyVal = PyLong_FromLong(std::any_cast<long>(Val));
	}
	else if (Val.type() == typeid(double)) {
		PyVal = PyFloat_FromDouble(std::any_cast<double>(Val));
	}
	else if (Val.type() == typeid(bool)) {
		PyVal = PyBool_FromLong(std::any_cast<bool>(Val));
	}
	else if (Val.type() == typeid(std::string)) {
		PyVal = StringToPy(std::any_cast<std::string>(Val));
	}
	else if (Val.type() == typeid(std::wstring)) {
		PyVal = WStringToPy(std::any_cast<std::wstring>(Val));
	}
	else if (Val.type() == typeid(const char*)) {
		PyVal = StringToPy(std::any_cast<const char*>(Val));
	}
	else if (Val.type() == typeid(const wchar_t*)) {
		PyVal = WCharToPy(std::any_cast<const wchar_t*>(Val));
	}
	return PyVal;
}

PyObject* TwoDoublesTupleToPy(const std::tuple<double, double>&Val) {
	PyObject* PyTup = PyTuple_New(2);
	PyObject* Double1 = PyFloat_FromDouble(std::get<0>(Val));
	PyObject* Double2 = PyFloat_FromDouble(std::get<1>(Val));
	PyTuple_SetItem(PyTup, 0, Double1);
	PyTuple_SetItem(PyTup, 1, Double2);
	return PyTup;
}

PyObject* TwoStringsTupleToPy(const std::tuple<std::string, std::string>& Val) {
	PyObject* PyTup = PyTuple_New(2);
	PyObject* Str1 = PyUnicode_FromString(std::get<0>(Val).c_str());
	PyObject* Str2 = PyUnicode_FromString(std::get<1>(Val).c_str());
	PyTuple_SetItem(PyTup, 0, Str1);
	PyTuple_SetItem(PyTup, 1, Str2);
	return PyTup;
}

PyObject* TwoAnysTupleToPy(const std::tuple<std::any, std::any>& Val) {
	PyObject* PyTup = PyTuple_New(2);
	PyObject* Val1 = SingleValToPy(std::get<0>(Val));
	PyObject* Val2 = SingleValToPy(std::get<1>(Val));
	PyTuple_SetItem(PyTup, 0, Val1);
	PyTuple_SetItem(PyTup, 1, Val2);
	return PyTup;
}

PyObject* VectorOfDoublesToPy(const std::vector<double>& Val) {
	return VectorToPyList(Val, DoubleToPy);
}

PyObject* VectorOfVectorOfDoublesToPy(const std::vector<std::vector<double>>& Val) {
	return VectorToPyList(Val, VectorOfDoublesToPy);
}

PyObject* VectorOfTwoDoublesTuplesToPy(const std::vector<std::tuple<double, double>>& Val) {
	return VectorToPyList(Val, TwoDoublesTupleToPy);
}

PyObject* VectorOfStringsToPy(const std::vector<std::string>& Val) {
	return VectorToPyList(Val, StringToPy);
}

PyObject* VectorOfWStringsToPy(const std::vector<std::wstring>& Val) {
	return VectorToPyList(Val, WStringToPy);
}

template <typename K, typename V>
PyObject* MapToPyDict(const std::unordered_map<K, V>& cpp_map,
	PyObject* (*KeyConvertToPy)(const K&), PyObject* (*ValConvertToPy)(const V&)) {
	PyObject* key = NULL;
	PyObject* val = NULL;
	PyObject* r = PyDict_New();

	if (!r) {
		goto except;
	}
	for (auto& iter : cpp_map) {
		key = (*KeyConvertToPy)(iter.first);
		if (!key || PyErr_Occurred()) {
			goto except;
		}
		val = (*ValConvertToPy)(iter.second);
		if (!val || PyErr_Occurred()) {
			goto except;
		}
		if (PyDict_SetItem(r, key, val)) {
			goto except;
		}
	}
	assert(!PyErr_Occurred());
	assert(r);
	goto finally;
except:
	assert(PyErr_Occurred());
	// Clean up dict
	if (r) {
		PyDict_Clear(r);
		Py_DECREF(r);
	}
	r = NULL;
	finally:
	return r;
}

PyObject* AnyExceptDicToPy(const std::any& Val) {
	// Any except dictionary to Python object

	PyObject* PyVal = Py_None;
	PyVal = SingleValToPy(Val);
	if (PyVal == Py_None) {
		if (Val.type() == typeid(std::vector<int>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<int>>(Val), &IntToPy);
		}
		else if (Val.type() == typeid(std::vector<double>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<double>>(Val), &DoubleToPy);
		}
		else if (Val.type() == typeid(std::vector<std::string>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::string>>(Val), &StringToPy);
		}
		else if (Val.type() == typeid(std::vector<std::wstring>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::wstring>>(Val), &WStringToPy);
		}
		else if (Val.type() == typeid(std::vector<bool>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<bool>>(Val), &BoolToPy);
		}
		else if (Val.type() == typeid(std::vector<std::tuple<double, double>>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::tuple<double, double>>>(Val),
				&TwoDoublesTupleToPy);
		}
		else if (Val.type() == typeid(std::vector<std::tuple<std::any, std::any>>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::tuple<std::any, std::any>>>(Val),
				&TwoAnysTupleToPy);
		}
		else if (Val.type() == typeid(std::vector<std::tuple<std::string, std::string>>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::tuple<std::string, std::string>>>(Val),
				&TwoStringsTupleToPy);
		}
		else if (Val.type() == typeid(std::vector<std::vector<double>>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::vector<double>>>(Val),
				&VectorOfDoublesToPy);
		}
		else if (Val.type() == typeid(std::vector <std::vector<std::tuple<double, double>>>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector <std::vector<std::tuple<double, double>>>>(Val),
				&VectorOfTwoDoublesTuplesToPy);
		}
		else if (Val.type() == typeid(std::vector<std::vector<std::vector<double>>>)) {
			PyVal = VectorToPyList(std::any_cast<std::vector<std::vector<std::vector<double>>>>(Val),
				&VectorOfVectorOfDoublesToPy);
		}
		else if (Val.type() == typeid(std::tuple<double, double>)) {
			PyVal = TwoDoublesTupleToPy(std::any_cast<std::tuple<double, double>>(Val));
		}
		else if (Val.type() == typeid(std::tuple<std::string, std::string>)) {
			PyVal = TwoStringsTupleToPy(std::any_cast<std::tuple<std::string, std::string>>(Val));
		}
		else if (Val.type() == typeid(std::tuple<std::any, std::any>)) {
			PyVal = TwoAnysTupleToPy(std::any_cast<std::tuple<std::any, std::any>>(Val));
		}
		else if (Val.type() == typeid(std::unordered_map<std::string, std::string>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::string, std::string>>(Val),
				&StringToPy, &StringToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::string, std::wstring>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::string, std::wstring>>(Val),
				&StringToPy, &WStringToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::wstring, std::string>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::wstring, std::string>>(Val),
				&WStringToPy, &StringToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::wstring, std::wstring>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::wstring, std::wstring>>(Val),
				&WStringToPy, &WStringToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::string, std::vector<std::string>>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::string, std::vector<std::string>>>(Val),
				&StringToPy, &VectorOfStringsToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::string, std::vector<std::wstring>>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::string, std::vector<std::wstring>>>(Val),
				&StringToPy, &VectorOfWStringsToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::wstring, std::vector<std::string>>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::wstring, std::vector<std::string>>>(Val),
				&WStringToPy, &VectorOfStringsToPy);
		}
		else if (Val.type() == typeid(std::unordered_map<std::wstring, std::vector<std::wstring>>)) {
			PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::wstring, std::vector<std::wstring>>>(Val),
				&WStringToPy, &VectorOfWStringsToPy);
		}
		else if (Val.type() == typeid(std::nullopt_t)) {
			PyVal = Py_None;
		}
	}
	return PyVal;
}

PyObject* MainFrame::ToPyObject(std::any Val) {

	PyObject* PyVal;
	if (Val.type() == typeid(std::unordered_map<std::string, std::any>)) {
		PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::string, std::any>>(Val),
			&StringToPy, &AnyExceptDicToPy);
	}
	else if (Val.type() == typeid(std::unordered_map<std::wstring, std::any>)) {
		PyVal = MapToPyDict(std::any_cast<std::unordered_map<std::wstring, std::any>>(Val),
			&WStringToPy, &AnyExceptDicToPy);
	}
	else {
		PyVal = AnyExceptDicToPy(Val);
	}

	return PyVal;
}

std::string GetPlotAreaString(std::string Val) {
	std::string PlotStr;
	if (Val == "Fit-Area") { PlotStr = "fit"; }
	else if (Val == "X-Interceptions") { PlotStr = "cross"; }
	else { PlotStr = ""; }

	return PlotStr;
}

std::string GetMarkerStyle(std::string Val) {
	std::string Marker = "o";
	if (Val == "circle") { Marker = "o"; }
	else if (Val == "point") { Marker = "."; }
	else if (Val == "triangle_down") { Marker = "v"; }
	else if (Val == "triangle_up") { Marker = "^"; }
	else if (Val == "triangle_left") { Marker = "<"; }
	else if (Val == "triangle_right") { Marker = ">"; }
	else if (Val == "octagon") { Marker = "8"; }
	else if (Val == "square") { Marker = "s"; }
	else if (Val == "pentagon") { Marker = "p"; }
	else if (Val == "plus") { Marker = "P"; }
	else if (Val == "star") { Marker = "*"; }
	else if (Val == "hexagon") { Marker = "h"; }
	else if (Val == "X") { Marker = "X"; }
	else if (Val == "diamond") { Marker = "D"; }

	return Marker;
}

std::unordered_map<std::string, std::any> MainFrame::GetDataInfos() {
	std::unordered_map<std::string, std::any> DataInfos;
	std::wstring Path = FilePicker->GetPath().ToStdWstring();
	DataInfos["Path"] = Path;

	std::vector<std::wstring> xColumns;
	for (unsigned int i = 0; i < XList->GetCount(); i++) {
		xColumns.push_back(XList->GetString(i).ToStdWstring());
	}
	std::vector<std::wstring> xErrorColumns;
	for (unsigned int i = 0; i < XErrList->GetCount(); i++) {
		xErrorColumns.push_back(XErrList->GetString(i).ToStdWstring());
	}
	std::vector<std::wstring> yColumns;
	for (unsigned int i = 0; i < YList->GetCount(); i++) {
		yColumns.push_back(YList->GetString(i).ToStdWstring());
	}
	std::vector<std::wstring> yErrorColumns;
	for (unsigned int i = 0; i < YErrList->GetCount(); i++) {
		yErrorColumns.push_back(YErrList->GetString(i).ToStdWstring());
	}
	DataInfos["xColumns"] = xColumns;
	DataInfos["yColumns"] = yColumns;
	DataInfos["xErrorColumns"] = xErrorColumns;
	DataInfos["yErrorColumns"] = yErrorColumns;
	DataInfos["Seperator"] = CSVSettings["Seperator"];
	DataInfos["Decimal"] = CSVSettings["Decimal"];

	return DataInfos;
}

std::unordered_map<std::string, std::any> MainFrame::GetPlotSettings() {

	std::unordered_map<std::string, std::any> PlotSettings;

	std::vector <std::vector<std::tuple<double, double>>> Zooms;
	for (unsigned int i = 1; i <= ZoomXMin->GetChildCount(); i++) {
		wxPGProperty* XMinProp = PlotSettingsGrid->GetProperty("Zoom X-Minimum.Zoom "
			+ std::to_string(i));
		wxPGProperty* XMaxProp = PlotSettingsGrid->GetProperty("Zoom X-Maximum.Zoom "
			+ std::to_string(i));
		wxPGProperty* YMinProp = PlotSettingsGrid->GetProperty("Zoom Y-Minimum.Zoom "
			+ std::to_string(i));
		wxPGProperty* YMaxProp = PlotSettingsGrid->GetProperty("Zoom Y-Maximum.Zoom "
			+ std::to_string(i));
		std::vector <std::tuple<double, double>> Zoom;

		Zoom.push_back(std::make_tuple<double, double>(XMinProp->GetValue().GetDouble(),
			XMaxProp->GetValue().GetDouble()));
		Zoom.push_back(std::make_tuple<double, double>(YMinProp->GetValue().GetDouble(),
			YMaxProp->GetValue().GetDouble()));

		Zooms.push_back(Zoom);
	}
	PlotSettings["Zoom"] = Zooms;

	std::vector <std::string> LegendOrder;
	std::vector <std::wstring> LegendLabels;
	std::vector <int> DataIndexes;
	wxPGProperty* FirstChild = PlotSettingsGrid->GetFirstChild(PGCLegendLabels);
	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
		FirstChild); !it.AtEnd(); it++)
	{
		wxPGProperty* CurrentProp = *it;
		if (CurrentProp->GetParent() != PGCLegendLabels) { break; }

		std::wstring LegendLabel = CurrentProp->GetValueAsString().ToStdWstring();
		if (CurrentProp->IsValueUnspecified() 
			or LegendLabel.find_first_not_of(' ') == std::string::npos) {
			// if Label is empty or contains only whitespace ignore
			continue; 
		}

		int DataIndex = YList->FindString(CurrentProp->GetLabel(), true);

		if (DataIndex == wxNOT_FOUND) {
			// if label is not data label it is a fit label:
			LegendOrder.push_back(CurrentProp->GetLabel().ToStdString());
		}
		else {
			// If DataIndex not already used append Data Label to LegendOrder:
			if (std::find(DataIndexes.begin(), DataIndexes.end(), DataIndex) == DataIndexes.end()) {
				LegendOrder.push_back("Data " + std::to_string(DataIndex));
				DataIndexes.push_back(DataIndex);
			}
			else {
				// If DataIndex already used append label as fit label (if Data Label equals Fit Label)
				LegendOrder.push_back(CurrentProp->GetLabel().ToStdString());
			}
		}
		LegendLabels.push_back(LegendLabel);
	}

	PlotSettings["LegendOrder"] = LegendOrder;
	PlotSettings["LegendLabels"] = LegendLabels;

	std::vector <std::string> MarkerColors;
	std::vector <double> MarkerSizes;
	std::vector <std::string> MarkerStyles;
	std::vector <long> MarkerOrders;
	std::vector <double> MarkerAlphas;
	std::vector <bool> MarkerConnects;
	std::vector <double> ErrorWidths;
	std::vector <double> ErrorCapsizes;
	FirstChild = PlotSettingsGrid->GetFirstChild(Markers);
	for (wxPropertyGridIterator it = PlotSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
		FirstChild); !it.AtEnd(); it++)
	{
		wxPGProperty* CurrentProp = *it;
		wxPGProperty* CurrentParent = CurrentProp->GetParent();
		if (CurrentProp == PlotXLabel) {
			break;
		}
		else if (not CurrentProp->IsValueUnspecified()) {
			if (CurrentParent == Markers) {
				if (CurrentProp == MarkerColor) {
					PlotSettings["mColors"] = CurrentProp->GetValueAsString().ToStdString();
				}
				else if (CurrentProp == MarkerSize) {
					PlotSettings["mSizes"] = CurrentProp->GetValue().GetDouble();
				}
				else if (CurrentProp == MarkerStyle) {
					PlotSettings["mStyles"] = GetMarkerStyle(CurrentProp->GetValueAsString().ToStdString());
				}
				else if (CurrentProp == MarkerOrder) {
					PlotSettings["mOrders"] = CurrentProp->GetValue().GetLong();
				}
				else if (CurrentProp == MarkerAlpha) {
					PlotSettings["mAlphas"] = CurrentProp->GetValue().GetDouble();
				}
				else if (CurrentProp == MarkerConnect) {
					PlotSettings["mConnects"] = CurrentProp->GetValue().GetBool();
				}
				else if (CurrentProp == ErrorWidth) {
					PlotSettings["ErrWidths"] = CurrentProp->GetValue().GetDouble();
				}
				else if (CurrentProp == ErrorCapsize) {
					PlotSettings["ErrCapsizes"] = CurrentProp->GetValue().GetDouble();
				}
			}
			else if (CurrentParent == MarkerColor) {
				MarkerColors.push_back(CurrentProp->GetValueAsString().ToStdString());
			}
			else if (CurrentParent == MarkerSize) {
				MarkerSizes.push_back(CurrentProp->GetValue().GetDouble());
			}
			else if (CurrentParent == MarkerStyle) {
				MarkerStyles.push_back(GetMarkerStyle(CurrentProp->GetValueAsString().ToStdString()));
			}
			else if (CurrentParent == MarkerOrder) {
				MarkerOrders.push_back(CurrentProp->GetValue().GetLong());
			}
			else if (CurrentParent == MarkerAlpha) {
				MarkerAlphas.push_back(CurrentProp->GetValue().GetDouble());
			}
			else if (CurrentParent == MarkerConnect) {
				MarkerConnects.push_back(CurrentProp->GetValue().GetBool());
			}
			else if (CurrentParent == ErrorWidth) {
				ErrorWidths.push_back(CurrentProp->GetValue().GetDouble());
			}
			else if (CurrentParent == ErrorCapsize) {
				ErrorCapsizes.push_back(CurrentProp->GetValue().GetDouble());
			}
		}
	}

	if (not MarkerColors.empty()) { PlotSettings["mColors"] = MarkerColors; }
	else { PlotSettings["mColors"] = std::nullopt; }
	if (not MarkerSizes.empty()) { PlotSettings["mSizes"] = MarkerSizes; }
	else { PlotSettings["mSizes"] = std::nullopt; }
	if (not MarkerStyles.empty()) { PlotSettings["mStyles"] = MarkerStyles; }
	else { PlotSettings["mStyles"] = std::nullopt; }
	if (not MarkerOrders.empty()) { PlotSettings["mOrders"] = MarkerOrders; }
	else { PlotSettings["mOrders"] = std::nullopt; }
	if (not MarkerAlphas.empty()) { PlotSettings["mAlphas"] = MarkerAlphas; }
	else { PlotSettings["mAlphas"] = std::nullopt; }
	if (not MarkerConnects.empty()) { PlotSettings["mConnects"] = MarkerConnects; }
	else { PlotSettings["mConnects"] = std::nullopt; }
	if (not ErrorWidths.empty()) { PlotSettings["ErrWidths"] = ErrorWidths; }
	else { PlotSettings["ErrWidths"] = std::nullopt; }
	if (not ErrorCapsizes.empty()) { PlotSettings["ErrCapsizes"] = ErrorCapsizes; }
	else { PlotSettings["ErrCapsizes"] = std::nullopt; }

	if (PlotXLabel->IsValueUnspecified()) { PlotSettings["LabelX"] = std::nullopt; }
	else { PlotSettings["LabelX"] = PlotXLabel->GetValueAsString().ToStdWstring(); }
	if (PlotXLabel->IsValueUnspecified()) { PlotSettings["LabelY"] = std::nullopt; }
	else { PlotSettings["LabelY"] = PlotYLabel->GetValueAsString().ToStdWstring(); }

	if (PlotLabelsize->IsValueUnspecified()) { PlotSettings["LabelSize"] = std::nullopt; }
	else { PlotSettings["LabelSize"] = PlotLabelsize->GetValue().GetDouble(); }

	if (PlotXMin->IsValueUnspecified() or PlotXMax->IsValueUnspecified()) {
		PlotSettings["xLimit"] = std::nullopt;
	}
	else {
		PlotSettings["xLimit"] = std::make_tuple<double, double>(PlotXMin->GetValue().GetDouble(),
			PlotXMax->GetValue().GetDouble());
	}

	if (PlotYMin->IsValueUnspecified() or PlotYMax->IsValueUnspecified()) {
		PlotSettings["yLimit"] = std::nullopt;
	}
	else {
		PlotSettings["yLimit"] = std::make_tuple<double, double>(PlotYMin->GetValue().GetDouble(),
			PlotYMax->GetValue().GetDouble());
	}

	if (PlotWidth->IsValueUnspecified()) { PlotSettings["FigWidth"] = std::nullopt; }
	else { PlotSettings["FigWidth"] = PlotWidth->GetValue().GetDouble(); }

	if (PlotHeight->IsValueUnspecified()) { PlotSettings["FigHeight"] = std::nullopt; }
	else { PlotSettings["FigHeight"] = PlotHeight->GetValue().GetDouble(); }

	if (ZoomHelpLines->IsValueUnspecified()) { PlotSettings["ZoomHelpLines"] = true; }
	else { PlotSettings["ZoomHelpLines"] = ZoomHelpLines->GetValue().GetBool(); }

	if (PlotProjection->IsValueUnspecified()) { PlotSettings["Projection"] = false; }
	else { PlotSettings["Projection"] = PlotProjection->GetValue().GetBool(); }

	if (PlotLegendRelX->IsValueUnspecified()) { PlotSettings["LegendRelX"] = std::nullopt; }
	else { PlotSettings["LegendRelX"] = PlotLegendRelX->GetValue().GetDouble(); }

	if (PlotLegendRelY->IsValueUnspecified()) { PlotSettings["LegendRelY"] = std::nullopt; }
	else { PlotSettings["LegendRelY"] = PlotLegendRelY->GetValue().GetDouble(); }

	if (PlotLegendFontsize->IsValueUnspecified()) { PlotSettings["LegendFontsize"] = std::nullopt; }
	else { PlotSettings["LegendFontsize"] = PlotLegendFontsize->GetValue().GetDouble(); }

	if (PlotLegendHandlelength->IsValueUnspecified()) { PlotSettings["LegendHandlelength"] = std::nullopt; }
	else { PlotSettings["LegendHandlelength"] = PlotLegendHandlelength->GetValue().GetDouble(); }

	if (PlotLegendMarkerscale->IsValueUnspecified()) { PlotSettings["LegendMarkerscale"] = std::nullopt; }
	else { PlotSettings["LegendMarkerscale"] = PlotLegendMarkerscale->GetValue().GetDouble(); }

	if (PlotLogScaleX->IsValueUnspecified()) { PlotSettings["LogScaleX"] = std::nullopt; }
	else { PlotSettings["LogScaleX"] = PlotLogScaleX->GetValue().GetBool(); }

	if (PlotLogScaleY->IsValueUnspecified()) { PlotSettings["LogScaleY"] = std::nullopt; }
	else { PlotSettings["LogScaleY"] = PlotLogScaleY->GetValue().GetBool(); }

	if (PlotLogBaseX->IsValueUnspecified()) { PlotSettings["LogScaleBaseX"] = std::exp(1); }
	else { PlotSettings["LogScaleBaseX"] = PlotLogBaseX->GetValue().GetDouble(); }

	if (PlotLogBaseY->IsValueUnspecified()) { PlotSettings["LogScaleBaseY"] = std::exp(1); }
	else { PlotSettings["LogScaleBaseY"] = PlotLogBaseY->GetValue().GetDouble(); }

	if (PlotXSci->IsValueUnspecified()) { PlotSettings["SciStyleX"] = std::nullopt; }
	else { PlotSettings["SciStyleX"] = PlotXSci->GetValue().GetBool(); }

	if (PlotYSci->IsValueUnspecified()) { PlotSettings["SciStyleY"] = std::nullopt; }
	else { PlotSettings["SciStyleY"] = PlotYSci->GetValue().GetBool(); }

	if (ZoomXSci->IsValueUnspecified()) { PlotSettings["SciStyleXZoom"] = std::nullopt; }
	else { PlotSettings["SciStyleXZoom"] = ZoomXSci->GetValue().GetBool(); }

	if (ZoomYSci->IsValueUnspecified()) { PlotSettings["SciStyleYZoom"] = std::nullopt; }
	else { PlotSettings["SciStyleYZoom"] = ZoomYSci->GetValue().GetBool(); }

	if (PlotTickLabelsize->IsValueUnspecified()) { PlotSettings["TickLabelSize"] = std::nullopt; }
	else { PlotSettings["TickLabelSize"] = PlotTickLabelsize->GetValue().GetDouble(); }

	if (PlotTickDirection->IsValueUnspecified()) { PlotSettings["TickDirection"] = std::nullopt; }
	else { PlotSettings["TickDirection"] = PlotTickDirection->GetValueAsString(); }

	if (PlotMajorTickLength->IsValueUnspecified()) { PlotSettings["MajorTickLength"] = std::nullopt; }
	else { PlotSettings["MajorTickLength"] = PlotMajorTickLength->GetValue().GetDouble(); }

	if (PlotMajorTickWidth->IsValueUnspecified()) { PlotSettings["MajorTickWidth"] = std::nullopt; }
	else { PlotSettings["MajorTickWidth"] = PlotMajorTickWidth->GetValue().GetDouble(); }

	if (PlotMajorTicksPeriodX->IsValueUnspecified()) { PlotSettings["MajorTicksPeriodX"] = std::nullopt; }
	else { PlotSettings["MajorTicksPeriodX"] = PlotMajorTicksPeriodX->GetValue().GetDouble(); }

	if (PlotMajorTicksPeriodY->IsValueUnspecified()) { PlotSettings["MajorTicksPeriodY"] = std::nullopt; }
	else { PlotSettings["MajorTicksPeriodY"] = PlotMajorTicksPeriodY->GetValue().GetDouble(); }

	if (ZoomXPeriod->IsValueUnspecified()) { PlotSettings["MajorTicksPeriodXZoom"] = std::nullopt; }
	else { PlotSettings["MajorTicksPeriodXZoom"] = ZoomXPeriod->GetValue().GetDouble(); }

	if (ZoomYPeriod->IsValueUnspecified()) { PlotSettings["MajorTicksPeriodYZoom"] = std::nullopt; }
	else { PlotSettings["MajorTicksPeriodYZoom"] = ZoomYPeriod->GetValue().GetDouble(); }

	if (PlotMinorTicksX->IsValueUnspecified()) { PlotSettings["MinorTicksX"] = std::nullopt; }
	else { PlotSettings["MinorTicksX"] = PlotMinorTicksX->GetValue().GetBool(); }

	if (PlotMinorTicksY->IsValueUnspecified()) { PlotSettings["MinorTicksY"] = std::nullopt; }
	else { PlotSettings["MinorTicksY"] = PlotMinorTicksY->GetValue().GetBool(); }

	if (PlotMinorTicksPeriodX->IsValueUnspecified()) { PlotSettings["MinorTicksPeriodX"] = std::nullopt; }
	else { PlotSettings["MinorTicksPeriodX"] = PlotMinorTicksPeriodX->GetValue().GetDouble(); }

	if (PlotMinorTicksPeriodY->IsValueUnspecified()) { PlotSettings["MinorTicksPeriodY"] = std::nullopt; }
	else { PlotSettings["MinorTicksPeriodY"] = PlotMinorTicksPeriodY->GetValue().GetDouble(); }

	if (PlotMinorTickLength->IsValueUnspecified()) { PlotSettings["MinorTickLength"] = std::nullopt; }
	else { PlotSettings["MinorTickLength"] = PlotMinorTickLength->GetValue().GetDouble(); }

	if (PlotMinorTickWidth->IsValueUnspecified()) { PlotSettings["MinorTickWidth"] = std::nullopt; }
	else { PlotSettings["MinorTickWidth"] = PlotMinorTickWidth->GetValue().GetDouble(); }

	std::vector<double> EmptyVec = {};
	PlotSettings["RemoveMajorXTicks"] = EmptyVec;
	PlotSettings["RemoveMajorYTicks"] = EmptyVec;
	PlotSettings["RemoveMinorXTicks"] = EmptyVec;
	PlotSettings["RemoveMinorYTicks"] = EmptyVec;

	return PlotSettings;
}

PyObject* MainFrame::GetFitFunctions() {
	long NumberOfFits = FitCount->GetValue().GetLong();
	PyObject* PyFitFunctions = PyList_New(NumberOfFits);
	for (long i = 0; i < NumberOfFits; i++) {
		wxPGProperty* FuncProp = FitSettingsGrid->GetProperty("Fitfunction.Fit " + std::to_string(i + 1));
		if (NumberOfFits == 1 or not FitFuncs->IsValueUnspecified()) { FuncProp = FitFuncs; }
		std::string FitFunc = FuncProp->GetValueAsString().ToStdString();
		PyObject* PyFitFunction = PyObject_GetAttrString(fitfunctions_module, FitFunc.c_str());
		if (NumberOfFits == 1 or not FitFuncs->IsValueUnspecified()) { 
			return PyFitFunction; 
		}
		PyList_SetItem(PyFitFunctions, i, PyFitFunction);
		//Py_DECREF(PyFitFunction);
	}

	return PyFitFunctions;
}

std::unordered_map<std::string, std::any> MainFrame::GetFitSettings() {
	std::unordered_map<std::string, std::any> FitSettings;

	FitSettings["LatexFuncs"] = LatexFuncs;
	FitSettings["LatexParams"] = LatexParams;

	std::vector<std::vector<double>> sParamsVecVec;
	std::vector<long> DataNos;
	std::vector<std::tuple<std::any, std::any>> AreaVec;
	std::vector<std::string> FitColors;
	std::vector<std::string> FitNames;
	std::vector<std::tuple<double, double>> ExAreaVec;
	std::vector<std::tuple<std::string, std::string>> pAreas;
	std::vector<std::string> FitLines;
	std::vector<bool> PResVec;
	std::vector<std::vector<std::vector<double>>> FitBoundsVecVecVec;
	std::vector<std::string> FitMethods;
	std::vector<bool> FitLogFits;
	std::vector<double> FitLogBases;
	std::vector<std::string> LossVec;
	std::vector<double> LossScaleVec;
	std::vector<double> FitLinewidths;
	std::vector<long> FitOrders;
	std::vector<long> FitOrdersZooms;
	long NumberOfFits = FitCount->GetValue().GetLong();
	for (long i = 1; i <= NumberOfFits; i++) // Iterate over Fits
	{
		wxString Prefix = "Fit Starting Values.Fit ";
		wxPGProperty* prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
		if (NumberOfFits == 1) { prop = FitSVs; }

		std::vector<double> sParamsVec;
		wxPGProperty* FirstChild = FitSettingsGrid->GetFirstChild(prop);
		for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
			FirstChild); !it.AtEnd(); it++) // Iterate over params
		{
			wxPGProperty* CurrentChild = *it;
			if (CurrentChild->GetParent() != prop) {
				break;
			}
			double Val = 1;
			if (not CurrentChild->IsValueUnspecified()) { Val = CurrentChild->GetValue().GetDouble(); }
			sParamsVec.push_back(Val);
		}
		sParamsVecVec.push_back(sParamsVec);

		if (not FitData->IsValueUnspecified()) {
			FitSettings["DataNo"] = YList->FindString(FitData->GetValueAsString());
		}
		else {
			Prefix = "Fitted Data.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				long Val = 0;
				if (not prop->IsValueUnspecified()) { Val = YList->FindString(prop->GetValueAsString()); }
				DataNos.push_back(Val);
			}
		}

		Prefix = "Fit X-Minimum.Fit ";
		wxPGProperty* MinProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
		Prefix = "Fit X-Maximum.Fit ";
		wxPGProperty* MaxProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));

		if (not FitXMin->IsValueUnspecified()) { MinProp = FitXMin; }
		if (not FitXMax->IsValueUnspecified()) { MaxProp = FitXMax; }

		if (MinProp and MaxProp) {
			if (MinProp->IsValueUnspecified() and not MaxProp->IsValueUnspecified()) {
				AreaVec.push_back(std::make_tuple<std::any, std::any>(std::nullopt,
					MaxProp->GetValue().GetDouble()));
			}
			else if (not MinProp->IsValueUnspecified() and MaxProp->IsValueUnspecified()) {
				AreaVec.push_back(std::make_tuple<std::any, std::any>(MinProp->GetValue().GetDouble(),
					std::nullopt));
			}
			else if (not MinProp->IsValueUnspecified() and not MaxProp->IsValueUnspecified()) {
				AreaVec.push_back(std::make_tuple<std::any, std::any>(MinProp->GetValue().GetDouble(),
					MaxProp->GetValue().GetDouble()));
			}
			else if (MinProp->IsValueUnspecified() and MaxProp->IsValueUnspecified()) {
				AreaVec.push_back(std::make_tuple<std::any, std::any>(std::nullopt, std::nullopt));
			}
		}

		if (not FitColor->IsValueUnspecified()) {
			FitSettings["Color"] = FitColor->GetValueAsString().ToStdString();
		}
		else {
			Prefix = "Fit Color.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				std::string Val = "blue";
				if (not prop->IsValueUnspecified()) { Val = prop->GetValueAsString().ToStdString(); }
				FitColors.push_back(Val);
			}
		}

		if (not FitName->IsValueUnspecified()) {
			FitSettings["Name"] = FitName->GetValueAsString().ToStdString();
		}
		else {
			Prefix = "Fit Name.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) { 
				std::string Val = "Fit " + std::to_string(i);
				if (not prop->IsValueUnspecified()) { Val = prop->GetValueAsString().ToStdString(); }
				FitNames.push_back(Val); 
			}
		}

		Prefix = "Fit excluded area X-Minimum.Fit ";
		MinProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
		Prefix = "Fit excluded area X-Maximum.Fit ";
		MaxProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));

		if (not FitExAreaMin->IsValueUnspecified()) { MinProp = FitExAreaMin; }
		if (not FitExAreaMax->IsValueUnspecified()) { MaxProp = FitExAreaMax; }

		if (MinProp and MaxProp) {
			ExAreaVec.push_back(std::make_tuple<double, double>(MinProp->GetValue().GetDouble(),
					MaxProp->GetValue().GetDouble()));
		}

		Prefix = "Fit Plot X-Minimum.Fit ";
		MinProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
		Prefix = "Fit Plot X-Maximum.Fit ";
		MaxProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));

		if (not FitPAreaMin->IsValueUnspecified()) { MinProp = FitPAreaMin; }
		if (not FitPAreaMin->IsValueUnspecified()) { MaxProp = FitPAreaMin; }

		if (MinProp and MaxProp) {
			std::string ValMin = MinProp->GetValueAsString().ToStdString();
			std::string ValMax = MaxProp->GetValueAsString().ToStdString();
			pAreas.push_back(std::make_tuple<std::string, std::string>(
				GetPlotAreaString(ValMin), GetPlotAreaString(ValMax)));
		}

		if (not FitLine->IsValueUnspecified()) {
			FitSettings["Line"] = FitLine->GetValueAsString().ToStdString();
		}
		else {
			Prefix = "Fit Line.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				std::string Val = "-";
				if (not prop->IsValueUnspecified()) { Val = prop->GetValueAsString().ToStdString(); }
				FitLines.push_back(Val);
			}
		}

		if (not FitPRes->IsValueUnspecified()) {
			FitSettings["pRes"] = FitPRes->GetValue().GetBool();
		}
		else {
			Prefix = "Print Residuals?.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				bool Val = false;
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetBool(); }
				PResVec.push_back(Val);
			}
		}

		Prefix = "Fit Bounds Minimum.Fit ";
		MinProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
		Prefix = "Fit Bounds Maximum.Fit ";
		MaxProp = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));

		if (NumberOfFits == 1) {
			MinProp = FitBoundsMin;
			MaxProp = FitBoundsMax;
		}

		std::vector<double> BoundsMinVec;
		FirstChild = FitSettingsGrid->GetFirstChild(MinProp);
		for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
			FirstChild); !it.AtEnd(); it++) // Iterate over params
		{
			wxPGProperty* CurrentChild = *it;
			if (CurrentChild->GetParent() != MinProp) {
				break;
			}

			double Val = -INFINITY;
			if (not CurrentChild->IsValueUnspecified()) { Val = CurrentChild->GetValue().GetDouble(); }
			BoundsMinVec.push_back(Val);
		}
		std::vector<double> BoundsMaxVec;
		FirstChild = FitSettingsGrid->GetFirstChild(MaxProp);
		for (wxPropertyGridIterator it = FitSettingsGrid->GetIterator(wxPG_ITERATE_DEFAULT,
			FirstChild); !it.AtEnd(); it++) // Iterate over params
		{
			wxPGProperty* CurrentChild = *it;
			if (CurrentChild->GetParent() != MaxProp) {
				break;
			}

			double Val = INFINITY;
			if (not CurrentChild->IsValueUnspecified()) { Val = CurrentChild->GetValue().GetDouble(); }
			BoundsMaxVec.push_back(Val);
		}
		std::vector<std::vector<double>> FitBoundsVecVec;
		FitBoundsVecVec.push_back(BoundsMinVec);
		FitBoundsVecVec.push_back(BoundsMaxVec);
		FitBoundsVecVecVec.push_back(FitBoundsVecVec);

		if (not FitMethod->IsValueUnspecified()) {
			FitSettings["Method"] = FitMethod->GetValueAsString().ToStdString();
		}
		else {
			Prefix = "Fit Method.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				std::string Val = "lm";
				if (not prop->IsValueUnspecified()) { Val = prop->GetValueAsString().ToStdString(); }
				FitMethods.push_back(Val);
			}
		}

		if (not FitLogFit->IsValueUnspecified()) {
			FitSettings["LogFit"] = FitLogFit->GetValue().GetBool();
		}
		else {
			Prefix = "Use Logarithmus on Fit and Data?.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				bool Val = false;
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetBool(); }
				FitLogFits.push_back(Val);
			}
		}

		if (not FitLogBase->IsValueUnspecified()) {
			FitSettings["LogBase"] = FitLogBase->GetValue().GetDouble();
		}
		else {
			Prefix = "Fit Logarithmic Base.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				double Val = std::exp(1);
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetDouble(); }
				FitLogBases.push_back(Val);
			}
		}

		if (not FitLoss->IsValueUnspecified()) {
			FitSettings["Loss"] = FitLoss->GetValueAsString().ToStdString();
		}
		else {
			Prefix = "Fit Loss-Function.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				std::string Val = "linear";
				if (not prop->IsValueUnspecified()) { Val = prop->GetValueAsString().ToStdString(); }
				LossVec.push_back(Val);
			}
		}

		if (not FitLossScale->IsValueUnspecified()) {
			FitSettings["LossScale"] = FitLossScale->GetValue().GetDouble();
		}
		else {
			Prefix = "Fit Loss-Scale.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				double Val = 1;
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetDouble(); }
				LossScaleVec.push_back(Val);
			}
		}

		if (not FitLinewidth->IsValueUnspecified()) {
			FitSettings["FitLinewidth"] = FitLinewidth->GetValue().GetDouble();
		}
		else {
			Prefix = "Fit Linewidth.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				double Val = 3;
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetDouble(); }
				FitLinewidths.push_back(Val);
			}
		}

		if (not FitOrder->IsValueUnspecified()) {
			FitSettings["FitOrder"] = FitOrder->GetValue().GetLong();
		}
		else {
			Prefix = "Fit Order.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				long Val = 3;
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetLong(); }
				FitOrders.push_back(Val);
			}
		}

		if (not FitOrdersZoom->IsValueUnspecified()) {
			FitSettings["FitOrdersZoom"] = FitOrdersZoom->GetValue().GetLong();
		}
		else {
			Prefix = "Fit Order of Zoom.Fit ";
			prop = FitSettingsGrid->GetProperty(Prefix + std::to_string(i));
			if (prop) {
				long Val = 3;
				if (not prop->IsValueUnspecified()) { Val = prop->GetValue().GetLong(); }
				FitOrdersZooms.push_back(Val);
			}
		}
	}

	FitSettings["NumFits"] = NumberOfFits;

	if (not sParamsVecVec.empty()) { FitSettings["sParams"] = sParamsVecVec; }
	else { FitSettings["sParams"] = std::nullopt; }

	if (not DataNos.empty()) { FitSettings["DataNo"] = DataNos; }
	else if (FitData->IsValueUnspecified()) { FitSettings["DataNo"] = std::nullopt; }

	if (not AreaVec.empty()) { FitSettings["Area"] = AreaVec; }
	else { FitSettings["Area"] = std::nullopt; }

	if (not FitColors.empty()) { FitSettings["Color"] = FitColors; }
	else if (FitColor->IsValueUnspecified()) { FitSettings["Color"] = std::nullopt; }

	if (not FitNames.empty()) { FitSettings["Name"] = FitNames; }
	else if (FitName->IsValueUnspecified()) { FitSettings["Name"] = std::nullopt; }

	if (not ExAreaVec.empty()) { FitSettings["ExArea"] = ExAreaVec; }
	else { FitSettings["ExArea"] = std::nullopt; }

	if (not FitNames.empty()) { FitSettings["Name"] = FitNames; }
	else if (FitName->IsValueUnspecified()) { FitSettings["Name"] = std::nullopt; }

	if (not pAreas.empty()) { FitSettings["pArea"] = pAreas; }
	else { FitSettings["pArea"] = std::nullopt; }
	
	if (not FitLines.empty()) { FitSettings["Line"] = FitLines; }
	else if (FitLine->IsValueUnspecified()) { FitSettings["Line"] = std::nullopt; }

	if (not PResVec.empty()) { FitSettings["pRes"] = PResVec; }
	else if (FitPRes->IsValueUnspecified()) { FitSettings["pRes"] = std::nullopt; }

	if (not FitBoundsVecVecVec.empty()) {
		for (size_t i = 0; i < FitBoundsVecVecVec.size(); i++) {

		}
		FitSettings["Bounds"] = FitBoundsVecVecVec;
	}
	else { 
		FitSettings["Bounds"] = std::nullopt; 
	}

	if (not FitMethods.empty()) {
		FitSettings["Method"] = FitMethods; 
	}
	else if (FitMethod->IsValueUnspecified()) { 
		FitSettings["Method"] = std::nullopt; 
	}

	if (not FitLogFits.empty()) { FitSettings["LogFit"] = FitLogFits; }
	else if (FitLogFit->IsValueUnspecified()) { FitSettings["LogFit"] = std::nullopt; }

	if (not FitLogBases.empty()) { FitSettings["LogBase"] = FitLogBases; }
	else if (FitLogBase->IsValueUnspecified()) { FitSettings["LogBase"] = std::nullopt; }

	if (not LossVec.empty()) { FitSettings["Loss"] = LossVec; }
	else if (FitLoss->IsValueUnspecified()) { FitSettings["Loss"] = std::nullopt; }

	if (not LossScaleVec.empty()) { FitSettings["LossScale"] = LossScaleVec; }
	else if (FitLossScale->IsValueUnspecified()) { FitSettings["LossScale"] = std::nullopt; }

	if (not FitLinewidths.empty()) { FitSettings["FitLinewidth"] = FitLinewidths; }
	else if (FitLinewidth->IsValueUnspecified()) { FitSettings["FitLinewidth"] = std::nullopt; }

	if (not FitOrders.empty()) { FitSettings["FitOrder"] = FitOrders; }
	else if (FitOrder->IsValueUnspecified()) { FitSettings["FitOrder"] = std::nullopt; }

	if (not FitOrdersZooms.empty()) { FitSettings["FitOrdersZoom"] = FitOrdersZooms; }
	else if (FitOrdersZoom->IsValueUnspecified()) { FitSettings["FitOrdersZoom"] = std::nullopt; }

	FitSettings["ExEr"] = std::nullopt;
	FitSettings["odrType"] = std::nullopt;
	FitSettings["CV"] = std::nullopt;

	return FitSettings;
}

void MainFrame::CreateAdditionalValidators() {
	// Write Validators for arrays:
	aFloatValidator = new wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST | wxFILTER_NUMERIC);
	wxArrayString aFloatList;
	wxString cEmptyComma = wxT(" ,");
	for (size_t i = 0; i < cEmptyComma.Length(); i++) {
		aFloatList.Add(wxString(cEmptyComma.GetChar(i)));
	}
	aFloatValidator->SetIncludes(aFloatList);

	aIntValidator = new wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST | wxFILTER_DIGITS);
	aIntValidator->SetIncludes(aFloatList);

	aTextValidator = new wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST | wxFILTER_ALPHA);
	aTextValidator->SetIncludes(aFloatList);

	// Validator for Floats including empty input
	wxArrayString eFloatList;
	wxString cEmpty = wxT("");
	for (size_t i = 0; i < cEmpty.Length(); i++) {
		eFloatList.Add(wxString(cEmpty.GetChar(i)));
	}
	eFloatValidator = new wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST | wxFILTER_NUMERIC);
	eFloatValidator->SetIncludes(eFloatList);

	// Validator for digits including "+","-" (ints) and empty input
	wxArrayString eIntList;
	wxString cPlusMinus = wxT("+-");
	for (size_t i = 0; i < cPlusMinus.Length(); i++) {
		eIntList.Add(wxString(cPlusMinus.GetChar(i)));
	}
	eIntValidator = new wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST | wxFILTER_DIGITS);
	eIntValidator->SetIncludes(eIntList);
}

// Function to append in a child with same type as parent:
wxPGProperty* MainFrame::AppendInChildSameType(wxPropertyGrid* PropertyGrid,
	wxPGProperty* Property, const wxString& Label, const wxString& Name) {

	wxClassInfo* PrClassInfo = Property->GetClassInfo();
	wxPGProperty* Child = nullptr;

	if (wxString(PrClassInfo->GetClassName()) == "wxArrayStringProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxArrayStringProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxBoolProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxBoolProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxColourProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxColourProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxCursorProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxCursorProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxDateProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxDateProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxDirProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxDirProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxEditEnumProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxEditEnumProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxEnumProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxEnumProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxFileProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxFileProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxFlagsProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxFlagsProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxFloatProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxFloatProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxFontProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxFontProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxImageFileProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxImageFileProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxIntProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxIntProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxLongStringProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxLongStringProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxMultiChoiceProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxMultiChoiceProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxPropertyCategory") {
		Child = PropertyGrid->AppendIn(Property, new wxPropertyCategory(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxStringProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxStringProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxSystemColourProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxSystemColourProperty(Label, Name));
	}
	else if (wxString(PrClassInfo->GetClassName()) == "wxUIntProperty") {
		Child = PropertyGrid->AppendIn(Property, new wxUIntProperty(Label, Name));
	}

	Child->SetChoices(Property->GetChoices());
	Child->SetAttributes(Property->GetAttributes());
	wxVariant Val;
	Val.MakeNull();
	Child->SetAttribute(L"Hint", Val);
	//Child->SetAttribute(L"Min", Property->DoGetAttribute(L"Min"));
	Child->SetEditor(Property->GetEditorClass());
	Child->GetCell(1).SetText("");
	if (Property->GetValidator() != NULL) {
		Child->SetValidator(*(Property->GetValidator()));
	}
	if (Property == FitBoundsMin or Property == FitBoundsMax or Property == FitSVs) {
		Child->SetValueFromString("<composed>");
	}
	else {
		Child->SetValueToUnspecified();
	}

	return Child;
}