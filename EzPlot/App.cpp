#include "App.h"
#include "MainFrame.h"
#include "GUIConsole.h"
#include <wx/wx.h>
#include <Python.h>
#include <windows.h>
#include <stdio.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdlib.h>

IMPLEMENT_APP(App);

bool App::OnInit() {

	// If EzPlot window already exist only push it to the foreground:
	HANDLE m_singleInstanceMutex = CreateMutex(NULL, TRUE, L"d0ff0aed-315b-4992-a740-2117eaacb6a0");
	if (m_singleInstanceMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND existingApp = FindWindow(0, L"EzPlot");
		if (existingApp) SetForegroundWindow(existingApp);
		return FALSE; // Exit the app. For MFC, return false from InitInstance.
	}

	// Activate python virtual environment:
	std::wofstream batch;
	batch.open(LR"(.\python\Scripts\activate.bat)", std::ios::out);
	batch << ":START\n";
	batch.close();

	// NumPy module is not clearing static variables (bug) so it cant be run twice or with debug
	Py_Initialize();
	//Py_SetPath(PythonPath.c_str());

	MainFrame* mainFrame = new MainFrame("EzPlot");
	mainFrame->SetClientSize(929, 500);
	mainFrame->Center();
	mainFrame->Show();

	//CreateNewConsole(1024);

	return true;
}