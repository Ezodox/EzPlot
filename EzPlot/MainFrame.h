#pragma once
#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/propgrid/propgrid.h>
#include <wx/activityindicator.h>
#include <Python.h>
#include <vector>
#include <string>
#include <map>
#include <any>
#include <tuple>
#include <optional>

class MainFrame : public wxFrame
{
public:
	MainFrame(const wxString& title);

private:
	void CreateSettingsTab();
	void CreateOutputTab();
	void UpdateSettingsData();
	void OpenCSVSettings();
	void OpenFunctionsWindow();
	std::vector<std::wstring> GetPythonFuncParams(std::wstring PythonFunction);
	std::wstring ReplaceParamsWithLatex(std::wstring Function, std::vector<std::wstring> Params,
		std::vector<std::wstring> Latex);
	wxPGProperty* AppendInChildSameType(wxPropertyGrid* PropertyGrid,
		wxPGProperty* Property, const wxString& Label = *wxPGProperty::sm_wxPG_LABEL,
		const wxString& Name = *wxPGProperty::sm_wxPG_LABEL);
	std::unordered_map<std::string, std::any> GetDataInfos();
	std::unordered_map<std::string, std::any> GetPlotSettings();
	PyObject* GetFitFunctions();
	std::unordered_map<std::string, std::any> GetFitSettings();
	std::string GetPythonOutput();
	//bool SettingsChanged(std::unordered_map<std::string, std::any> Settings1,
	//	std::unordered_map<std::string, std::any> Settings2);
	void PrintPythonOutput();
	void ClearPythonOutput();
	void StoreFunctionVariables();
	PyObject* ToPyObject(std::any Val);
	void OnNew(wxCommandEvent& event);
	void OnOpen(wxCommandEvent& event);
	void OnSave(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnEditCSVSettings(wxCommandEvent& event);
	void OnEditFunctions(wxCommandEvent& event);
	void ClearAll();
	void CreatePlot();
	void CreateAdditionalValidators();
	void ChildsToParent(wxPGProperty* Parent);
	void OnPropertyGridChanged(wxPropertyGridEvent& event);
	void UpdateSettingsGrid(wxPGProperty* Property);
	void OnPropertyGridChanging(wxPropertyGridEvent& event);
	void RecreatePropsWithParams(bool KeepVals);
	void OnAddClicked(wxCommandEvent& event);
	void OnRemoveClicked(wxCommandEvent& event);
	void OnPlotClicked(wxCommandEvent& event);
	void OnFilePicked(wxFileDirPickerEvent& event);
	void ProcessPickedFile();
	void OnSaveFuncClicked(wxCommandEvent& event);
	void OnFuncSelected(wxCommandEvent& event);
	void OnSeperatorSelected(wxCommandEvent& event);
	void OnDecimalSelected(wxCommandEvent& event);
	void SaveCSVSettings();
	void OnCSVSettingsWindowClose(wxCloseEvent& event);
	void OnFunctionsWindowClose(wxCloseEvent& event);
	void CreatePlotFrame();
	void OnPlotWindowClose(wxCloseEvent& event);

	wxPanel* MainPanel;
	wxFrame* CSVSettingsFrame;
	wxFrame* FuncsFrame;
	wxFrame* PlotFrame;
	wxNotebook* Tabs;
	wxPanel* DataTab;
	wxPanel* DataPanel;
	wxPanel* SettingsTab;
	wxPanel* OutputTab;
	wxPropertyGrid* PlotSettingsGrid;
	wxPropertyGrid* FitSettingsGrid;
	wxPropertyGrid* FuncSettingsGrid;
	wxBoxSizer* FrameSizer;
	wxBoxSizer* PanelSizer;
	wxBoxSizer* FilePickerSizer;
	wxBoxSizer* LoadingSizer;
	wxBoxSizer* DataSizer;
	wxBoxSizer* DataPanelSizer;
	wxBoxSizer* SettingsSizer;
	wxBoxSizer* DataHeadSizer;
	wxBoxSizer* DataChoiceSizer;
	wxBoxSizer* DataAddSizer;
	wxBoxSizer* DataRemoveSizer;
	wxBoxSizer* DataListSizer;
	wxBoxSizer* DataPlotSizer;
	wxFilePickerCtrl* FilePicker;
	wxStaticText* XStaticText;
	wxStaticText* XErrStaticText;
	wxStaticText* YStaticText;
	wxStaticText* YErrStaticText;
	wxChoice* XChoice;
	wxChoice* XErrChoice;
	wxChoice* YChoice;
	wxChoice* YErrChoice;
	wxButton* AddButton;
	wxButton* RemoveButton;
	wxListBox* XList;
	wxListBox* XErrList;
	wxListBox* YList;
	wxListBox* YErrList;
	wxButton* PlotButton;
	wxPGProperty* Markers;
	wxPGProperty* PrZoom;
	wxPropertyCategory* PGCLegendLabels;
	wxPGProperty* PlotWidth;
	wxPGProperty* PlotHeight;
	wxPGProperty* PlotProjection;
	wxPGProperty* PlotLegendRelX;
	wxPGProperty* PlotLegendRelY;
	wxPGProperty* PlotLegendFontsize;
	wxPGProperty* PlotLegendHandlelength;
	wxPGProperty* PlotLegendMarkerscale;
	wxPGProperty* ZoomCount;
	wxPGProperty* ZoomXMin;
	wxPGProperty* ZoomYMin;
	wxPGProperty* ZoomXMax;
	wxPGProperty* ZoomYMax;
	wxPGProperty* ZoomXSci;
	wxPGProperty* ZoomYSci;
	wxPGProperty* ZoomXPeriod;
	wxPGProperty* ZoomYPeriod;
	wxPGProperty* ZoomHelpLines;
	wxPGProperty* MarkerColor;
	wxPGProperty* MarkerSize;
	wxPGProperty* MarkerStyle;
	wxPGProperty* MarkerOrder;
	wxPGProperty* MarkerAlpha;
	wxPGProperty* MarkerConnect;
	wxPGProperty* ErrorWidth;
	wxPGProperty* ErrorCapsize;
	wxPGProperty* PlotXLabel;
	wxPGProperty* PlotYLabel;
	wxPGProperty* PlotLabelsize;
	wxPGProperty* PlotXMin;
	wxPGProperty* PlotXMax;
	wxPGProperty* PlotYMin;
	wxPGProperty* PlotYMax;
	wxPGProperty* PlotLogScaleX;
	wxPGProperty* PlotLogScaleY;
	wxPGProperty* PlotLogBaseX;
	wxPGProperty* PlotLogBaseY;
	wxPGProperty* PlotXSci;
	wxPGProperty* PlotYSci;
	wxPGProperty* PlotTickLabelsize;
	wxPGProperty* PlotTickDirection;
	wxPGProperty* PlotMajorTickLength;
	wxPGProperty* PlotMajorTickWidth;
	wxPGProperty* PlotMajorTicksPeriodX;
	wxPGProperty* PlotMajorTicksPeriodY;
	wxPGProperty* PlotMinorTicksX;
	wxPGProperty* PlotMinorTicksY;
	wxPGProperty* PlotMinorTicksPeriodX;
	wxPGProperty* PlotMinorTicksPeriodY;
	wxPGProperty* PlotMinorTickLength;
	wxPGProperty* PlotMinorTickWidth;
	wxPGProperty* FitCount;
	wxPGProperty* FitFuncs;
	wxPGProperty* FitSVs;
	wxPGProperty* FitData;
	wxPGProperty* FitXMin;
	wxPGProperty* FitXMax;
	wxPGProperty* FitColor;
	wxPGProperty* FitName;
	wxPGProperty* FitExAreaMin;
	wxPGProperty* FitExAreaMax;
	wxPGProperty* FitPAreaMin;
	wxPGProperty* FitPAreaMax;
	wxPGProperty* FitLine;
	wxPGProperty* FitPRes;
	wxPGProperty* FitBoundsMin;
	wxPGProperty* FitBoundsMax;
	wxPGProperty* FitMethod;
	wxPGProperty* FitLogFit;
	wxPGProperty* FitLogBase;
	wxPGProperty* FitLoss;
	wxPGProperty* FitLossScale;
	wxPGProperty* FitLinewidth;
	wxPGProperty* FitOrder;
	wxPGProperty* FitOrdersZoom;
	wxChoice* SeperatorChoice;
	wxChoice* DecimalChoice;
	wxChoice* FuncChoice;
	wxPGProperty* FuncName;
	wxPGProperty* FuncPython;
	wxPGProperty* FuncLatex;
	wxPGProperty* ParamsCategory;
	wxArrayPGProperty FuncParams;

	wxActivityIndicator* LoadingIcon;

	wxTextCtrl* OutputText;

	wxArrayString DataNames;
	wxArrayString Colors;
	wxArrayString MarkerStyles;
	bool ListsCreated;
	bool DataPanelHidden;
	bool WithAnyBounds;

	wxSize ClientSize;

	wxTextValidator* aFloatValidator;
	wxTextValidator* aIntValidator;
	wxTextValidator* aTextValidator;
	wxTextValidator* eFloatValidator;
	wxTextValidator* eIntValidator;

	PyObject* fp_module;
	PyObject* plot_module;
	PyObject* fitfunctions_module;
	PyObject* print_module;
	PyObject* CPlot;
	PyObject* GetColNames;
	PyObject* catcher;
	PyObject* ShowPlot;

	wxString FileName;

	//std::unordered_map<std::string, std::any> LastDataInfos;
	//std::unordered_map<std::string, std::any> LastPlotSettings;
	//PyObject* LastScatterIDs;

	std::unordered_map<std::string, std::string> CSVSettings;
	std::unordered_map<std::wstring, std::wstring> PythonFuncs;
	std::unordered_map<std::wstring, std::wstring> LatexFuncs;
	std::unordered_map<std::wstring, std::vector<std::wstring>> LatexParams;

	DECLARE_EVENT_TABLE()
};