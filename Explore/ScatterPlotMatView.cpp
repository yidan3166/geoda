/**
 * GeoDa TM, Copyright (C) 2011-2015 by Luc Anselin - all rights reserved
 *
 * This file is part of GeoDa.
 * 
 * GeoDa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GeoDa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <utility> // std::pair
#include <boost/foreach.hpp>
#include <wx/xrc/xmlres.h>
#include <wx/dcclient.h>
#include "../HighlightState.h"
#include "../GeneralWxUtils.h"
#include "../GeoDa.h"
#include "../logger.h"
#include "../Project.h"
#include "SimpleAxisCanvas.h"
#include "SimpleScatterPlotCanvas.h"
#include "SimpleHistCanvas.h"
#include "ScatterPlotMatView.h"


BEGIN_EVENT_TABLE(ScatterPlotMatFrame, TemplateFrame)
	EVT_MOUSE_EVENTS(ScatterPlotMatFrame::OnMouseEvent)
	EVT_ACTIVATE(ScatterPlotMatFrame::OnActivate)
END_EVENT_TABLE()

ScatterPlotMatFrame::ScatterPlotMatFrame(wxFrame *parent, Project* project,
																				 const wxString& title,
																				 const wxPoint& pos,
																				 const wxSize& size)
: TemplateFrame(parent, project, title, pos, size, wxDEFAULT_FRAME_STYLE),
lowess_param_frame(0), vars_chooser_frame(0), panel(0),
panel_v_szr(0), bag_szr(0), top_h_sizer(0),
show_regimes(true), show_outside_titles(true), show_linear_smoother(true),
show_lowess_smoother(false), show_slope_values(true)
{
	LOG_MSG("Entering ScatterPlotMatFrame::ScatterPlotMatFrame");
	supports_timeline_changes = true;
	{
		std::vector<wxString> tm_strs;
		project->GetTableInt()->GetTimeStrings(tm_strs);
		var_man.ClearAndInit(tm_strs);
	}
	
	int width, height;
	GetClientSize(&width, &height);
	
	panel = new wxPanel(this);
	panel->SetBackgroundColour(*wxWHITE);
	SetBackgroundColour(*wxWHITE);
	panel->Bind(wxEVT_MOTION, &ScatterPlotMatFrame::OnMouseEvent, this);
	
	//message_win = wxWebView::New(panel, wxID_ANY, wxWebViewDefaultURLStr,
	//														 wxDefaultPosition,
	//														 wxSize(-1, 200));
	message_win = new wxHtmlWindow(panel, wxID_ANY, wxDefaultPosition,
																 wxSize(200,-1));
	
	message_win->Bind(wxEVT_MOTION, &ScatterPlotMatFrame::OnMouseEvent, this);
	
	bag_szr = new wxGridBagSizer(0, 0); // 0 vgap, 0 hgap
	
	bag_szr->Add(message_win, wxGBPosition(0,0), wxGBSpan(1,1), wxEXPAND);
	bag_szr->SetFlexibleDirection(wxBOTH);
	bag_szr->AddGrowableCol(0, 1);
	bag_szr->AddGrowableRow(0, 1);	
	
	panel_v_szr = new wxBoxSizer(wxVERTICAL);
	panel_v_szr->Add(bag_szr, 1, wxEXPAND);
	
	wxBoxSizer* panel_h_szr = new wxBoxSizer(wxHORIZONTAL);
	panel_h_szr->Add(panel_v_szr, 1, wxEXPAND);
	
	panel->SetSizer(panel_h_szr);
	
	//wxBoxSizer* right_v_szr = new wxBoxSizer(wxVERTICAL);
	//conn_hist_canvas = new ConnectivityHistCanvas(this, this, project,
	//																							boost::uuids::nil_uuid());
	
	//right_v_szr->Add(conn_hist_canvas, 1, wxEXPAND);
	
	UpdateMessageWin();
	
	// Top Sizer for Frame
	top_h_sizer = new wxBoxSizer(wxHORIZONTAL);
	top_h_sizer->Add(panel, 1, wxEXPAND|wxALL, 8);
	
	SetSizer(top_h_sizer);
	DisplayStatusBar(false);
	
	Show(true);
	
	wxCommandEvent ev;
	OnShowVarsChooser(ev);
	LOG_MSG("Exiting ScatterPlotMatFrame::ScatterPlotMatFrame");
}

ScatterPlotMatFrame::~ScatterPlotMatFrame()
{
	LOG_MSG("In ScatterPlotMatFrame::~ScatterPlotMatFrame");
	if (lowess_param_frame) {
		lowess_param_frame->removeObserver(this);
		lowess_param_frame->closeAndDeleteWhenEmpty();
	}
	if (vars_chooser_frame) {
		vars_chooser_frame->removeObserver(this);
		vars_chooser_frame->closeAndDeleteWhenEmpty();
	}
	if (HasCapture()) ReleaseMouse();
	DeregisterAsActive();
}

void ScatterPlotMatFrame::OnMouseEvent(wxMouseEvent& event)
{
	LOG_MSG(wxString::Format("In ScatterPlotMatFrame::OnMouseEvent: (%d,%d)",
													 (int) event.GetX(), (int) event.GetY()));
	if (event.RightDown()) {
		LOG_MSG("Right Down");
	}
}

void ScatterPlotMatFrame::OnActivate(wxActivateEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnActivate");
	if (event.GetActive()) {
		RegisterAsActive("ScatterPlotMatFrame", GetTitle());
	}
	//if ( event.GetActive() && template_canvas ) template_canvas->SetFocus();
}

void ScatterPlotMatFrame::MapMenus()
{
	LOG_MSG("In ScatterPlotMatFrame::MapMenus");
	wxMenuBar* mb = GdaFrame::GetGdaFrame()->GetMenuBar();
	// Map Options Menus
	wxMenu* optMenu;
	optMenu = wxXmlResource::Get()->
		LoadMenu("ID_SCATTER_PLOT_MAT_MENU_OPTIONS");	
	ScatterPlotMatFrame::UpdateContextMenuItems(optMenu);

	GeneralWxUtils::ReplaceMenu(mb, "Options", optMenu);	
	UpdateOptionMenuItems();
}

void ScatterPlotMatFrame::UpdateOptionMenuItems()
{
	//TemplateFrame::UpdateOptionMenuItems(); // set common items first
	wxMenuBar* mb = GdaFrame::GetGdaFrame()->GetMenuBar();
	int menu = mb->FindMenu("Options");
	if (menu == wxNOT_FOUND) {
		LOG_MSG("ScatterPlotMatFrame::UpdateOptionMenuItems: Options "
						"menu not found");
	} else {
		ScatterPlotMatFrame::UpdateContextMenuItems(mb->GetMenu(menu));
	}
}

void ScatterPlotMatFrame::UpdateContextMenuItems(wxMenu* menu)
{
	// Update the checkmarks and enable/disable state for the
	// following menu items if they were specified for this particular
	// view in the xrc file.  Items that cannot be enable/disabled,
	// or are not checkable do not appear.
	GeneralWxUtils::CheckMenuItem(menu, XRCID("ID_VIEW_LINEAR_SMOOTHER"),
																show_linear_smoother);
	GeneralWxUtils::CheckMenuItem(menu, XRCID("ID_VIEW_LOWESS_SMOOTHER"),
																show_lowess_smoother);
	GeneralWxUtils::CheckMenuItem(menu, XRCID("ID_VIEW_REGIMES_REGRESSION"),
																show_regimes);
	GeneralWxUtils::CheckMenuItem(menu, XRCID("ID_DISPLAY_SLOPE_VALUES"),
																show_slope_values);
	
	TemplateFrame::UpdateContextMenuItems(menu); // set common items
}

void ScatterPlotMatFrame::OnViewLinearSmoother(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnViewLinearSmoother");
	show_linear_smoother = !show_linear_smoother;
	for (size_t i=0, sz=scatt_plots.size(); i<sz; ++i) {
		scatt_plots[i]->ShowLinearSmoother(show_linear_smoother);
	}
	UpdateOptionMenuItems();
}

void ScatterPlotMatFrame::OnViewLowessSmoother(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnViewLowessSmoother");
	show_lowess_smoother = !show_lowess_smoother;
	for (size_t i=0, sz=scatt_plots.size(); i<sz; ++i) {
		scatt_plots[i]->ShowLowessSmoother(show_lowess_smoother);
	}
	UpdateOptionMenuItems();
}

void ScatterPlotMatFrame::OnEditLowessParams(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnEditLowessParams");
	if (lowess_param_frame) {
		lowess_param_frame->Iconize(false);
		lowess_param_frame->Raise();
		lowess_param_frame->SetFocus();
	} else {
		Lowess l; // = t->GetLowess();  // should be shared by all cells
		lowess_param_frame = new LowessParamFrame(l.GetF(), l.GetIter(),
																							l.GetDeltaFactor(),
																							project);
		lowess_param_frame->registerObserver(this);
	}
}

void ScatterPlotMatFrame::OnShowVarsChooser(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnShowVarsChooser");
	if (vars_chooser_frame) {
		vars_chooser_frame->Iconize(false);
		vars_chooser_frame->Raise();
		vars_chooser_frame->SetFocus();
	} else {
		wxString title("Scatter Plot Matrix Variables Add/Remove");
		vars_chooser_frame = new VarsChooserFrame(var_man, project, true, true,
																							GetHelpHtml(),
																							"Scatter Plot Matrix Help",
																							title);
		vars_chooser_frame->registerObserver(this);
		vars_chooser_frame->SetSize(-1, -1, -1, 400);
	}
}

void ScatterPlotMatFrame::OnViewRegimesRegression(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnViewRegimesRegression");
	show_regimes = !show_regimes;
	for (size_t i=0, sz=scatt_plots.size(); i<sz; ++i) {
		scatt_plots[i]->ShowRegimes(show_regimes);
	}
	UpdateOptionMenuItems();
}

void ScatterPlotMatFrame::OnDisplayStatistics(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnDisplayStatistics");
	// should be managed here or by shared manager
	//ScatterPlotMatCanvas* t = (ScatterPlotMatCanvas*) template_canvas;
	//t->DisplayStatistics(!t->IsDisplayStats());
	UpdateOptionMenuItems();
}

void ScatterPlotMatFrame::OnDisplaySlopeValues(wxCommandEvent& event)
{
	LOG_MSG("In ScatterPlotMatFrame::OnDisplaySlopeValues");
	show_slope_values = !show_slope_values;
	for (size_t i=0, sz=scatt_plots.size(); i<sz; ++i) {
		scatt_plots[i]->ShowSlopeValues(show_slope_values);
	}
	UpdateOptionMenuItems();
}

/** Implementation of TableStateObserver interface */
void ScatterPlotMatFrame::update(TableState* o)
{
	LOG_MSG("In ScatterPlotMatFrame::update(TableState*)");
	if (vars_chooser_frame) vars_chooser_frame->UpdateFromTable();
}

/** Implementation of TimeStateObserver interface */
void ScatterPlotMatFrame::update(TimeState* o)
{
	LOG_MSG("In ScatterPlotMatFrame::update(TimeState* o)");
	
    
    bool has_time_var = false;
    for (data_map_type::iterator i=data_map.begin(); i!=data_map.end(); ++i) {
        if (i->second.size() > 1) {
            has_time_var = true;
            break;
        }
    }
    
    if (has_time_var) {
        var_man.UpdateGlobalTime(o->GetCurrTime());
        if (var_man.GetVarsCount() > 1) {
            SetupPanelForNumVariables(var_man.GetVarsCount());
        }
        UpdateTitle();
        Refresh();
    }
    
}

void ScatterPlotMatFrame::update(LowessParamObservable* o)
{
	for (size_t i=0, sz=scatt_plots.size(); i<sz; ++i) {
		scatt_plots[i]->ChangeLoessParams(o->GetF(), o->GetIter(), o->GetDeltaFactor());
	}
	// Is Refresh() needed?
}

void ScatterPlotMatFrame::notifyOfClosing(LowessParamObservable* o)
{
	lowess_param_frame = 0;
}

void ScatterPlotMatFrame::update(VarsChooserObservable* o)
{
	LOG_MSG("In ScatterPlotMatFrame::update(VarsChooserObservable*)");
	UpdateDataMapFromVarMan();
	SetupPanelForNumVariables(var_man.GetVarsCount());
	Refresh();
}

void ScatterPlotMatFrame::notifyOfClosing(VarsChooserObservable* o)
{
	vars_chooser_frame = 0;
}

void ScatterPlotMatFrame::SetupPanelForNumVariables(int num_vars)
{
	LOG_MSG("Entering ScatterPlotMatFrame::SetupPanelForNumVariables");
	if (!panel || !bag_szr) return;
	LOG(num_vars);
	LOG(bag_szr->GetItemCount());
	LOG(bag_szr->GetRows());
	LOG(bag_szr->GetCols());
	if (message_win) {
		message_win->Unbind(wxEVT_MOTION, &ScatterPlotMatFrame::OnMouseEvent, this);
		bool detatch_success = bag_szr->Detach(0);
		LOG(detatch_success);
		message_win->Destroy();
		message_win = 0;
	}
	bag_szr->Clear();
	LOG(bag_szr->GetItemCount());
	LOG(bag_szr->GetRows());
	LOG(bag_szr->GetCols());
	LOG(bag_szr->GetEffectiveRowsCount());
	LOG(bag_szr->GetEffectiveColsCount());
	panel_v_szr->Remove(bag_szr); // bag_szr is deleted automatically
	bag_szr = new wxGridBagSizer(0, 0); // 0 vgap, 0 hgap
	for (size_t i=0, sz=scatt_plots.size(); i<sz; ++i) {
		if (scatt_plots[i]) {
			scatt_plots[i]->Unbind(wxEVT_MOTION, &ScatterPlotMatFrame::OnMouseEvent,
														 this);
			scatt_plots[i]->Destroy();
		}
	}
	scatt_plots.clear();
	for (size_t i=0, sz=hist_plots.size(); i<sz; ++i) {
		if (hist_plots[i]) {
			hist_plots[i]->Unbind(wxEVT_MOTION, &ScatterPlotMatFrame::OnMouseEvent,
														 this);
			hist_plots[i]->Destroy();
		}
	}
	hist_plots.clear();
	for (size_t i=0, sz=vert_labels.size(); i<sz; ++i) {
		if (vert_labels[i]) vert_labels[i]->Destroy();
	}
	vert_labels.clear();
	for (size_t i=0, sz=horiz_labels.size(); i<sz; ++i) {
		if (horiz_labels[i]) horiz_labels[i]->Destroy();
	}
	horiz_labels.clear();
	if (num_vars < 2) {
		message_win = new wxHtmlWindow(panel, wxID_ANY, wxDefaultPosition, wxSize(200,-1));
		message_win->Bind(wxEVT_MOTION, &ScatterPlotMatFrame::OnMouseEvent, this);
		UpdateMessageWin();
		bag_szr->Add(message_win, wxGBPosition(0,0), wxGBSpan(1,1), wxEXPAND);
		
		bag_szr->SetFlexibleDirection(wxBOTH);
		if (bag_szr->IsColGrowable(0))
            bag_szr->RemoveGrowableCol(0);
		bag_szr->AddGrowableCol(0, 1);
		if (bag_szr->IsRowGrowable(0))
            bag_szr->RemoveGrowableRow(0);
		bag_szr->AddGrowableRow(0, 1);
		
	} else {
		for (int row=0; row<num_vars; ++row) {
			wxString row_nm(var_man.GetName(row));
			int row_tm(var_man.GetTime(row));
            
            if (data_map[row_nm].size() == 1)
                row_tm = 0;
            
            // We don't support time variable here
            //int row_tm = 0;
            wxString row_title(var_man.GetNameWithTime(row));
			const std::vector<double>& Y(data_map[row_nm][row_tm]);
			double row_min = var_man.GetMinOverAllTms(row);
			double row_max = var_man.GetMaxOverAllTms(row);
			SimpleAxisCanvas* sa_can = 0;
			{
                sa_can = new SimpleAxisCanvas(panel, this, project,
                                              project->GetHighlightState(),
                                              Y, row_title,
                                              row_min, row_max, false,
                                              show_outside_titles, false,
                                              true, true, -1, false, false, 0,
                                              wxDefaultPosition, wxSize(50, -1));
				bag_szr->Add(sa_can, wxGBPosition(row, 0), wxGBSpan(1,1), wxEXPAND);
				vert_labels.push_back(sa_can);
			}
			{
                sa_can = new SimpleAxisCanvas(panel, this, project,
                                              project->GetHighlightState(),
                                              Y, row_title,
                                              row_min, row_max, true,
                                              show_outside_titles, false,
                                              true, true, -1, false, false, 0,
                                              wxDefaultPosition, wxSize(-1, 50));
				bag_szr->Add(sa_can, wxGBPosition(num_vars, row+1), wxGBSpan(1,1),
										 wxEXPAND);
				horiz_labels.push_back(sa_can);
			}
			SimpleHistCanvas* sh_can = 0;
            sh_can = new SimpleHistCanvas(panel, this, project,
                                          project->GetHighlightState(), Y, row_title,
                                          row_min, row_max,
                                          !show_outside_titles);
			bag_szr->Add(sh_can, wxGBPosition(row, row+1), wxGBSpan(1,1), wxEXPAND);
			hist_plots.push_back(sh_can);
			for (int col=0; col<num_vars; ++col) {
				if (col == row) continue;
				wxString col_nm(var_man.GetName(col));
				int col_tm(var_man.GetTime(col));
                
                if (data_map[row_nm].size() == 1)
                    col_tm = 0;
                
				wxString col_title(var_man.GetNameWithTime(col));
				LOG_MSG("Creating Canvas Cell: ");
				const std::vector<double>& X(data_map[col_nm][col_tm]);
				double col_min = var_man.GetMinOverAllTms(col);
				double col_max = var_man.GetMaxOverAllTms(col);
				SimpleScatterPlotCanvas* sp_can = 0;
                sp_can = new SimpleScatterPlotCanvas(panel, this, project,
                                                     project->GetHighlightState(), 0,
                                                     X, Y,
                                                     col_title, row_title,
                                                     col_min, col_max,
                                                     row_min, row_max,
                                                     true, true, false,
                                                     "ID_SCATTER_PLOT_MAT_MENU_OPTIONS",
                                                     !show_outside_titles,
                                                     false, false, //show axes thru org
                                                     show_regimes,
                                                     show_linear_smoother,
                                                     show_lowess_smoother,
                                                     show_slope_values);
				bag_szr->Add(sp_can, wxGBPosition(row, col+1), wxGBSpan(1,1), wxEXPAND);
				scatt_plots.push_back(sp_can);
			}
		}
		bag_szr->Add(50, 50, wxGBPosition(num_vars, 0), wxGBSpan(1,1));
		
		bag_szr->SetFlexibleDirection(wxBOTH);
		if (bag_szr->IsColGrowable(0)) bag_szr->RemoveGrowableCol(0);
		if (bag_szr->IsRowGrowable(num_vars)) {
			bag_szr->RemoveGrowableRow(num_vars);
		}
		for (int i=0; i<num_vars; ++i) {
			if (bag_szr->IsColGrowable(i+1)) bag_szr->RemoveGrowableCol(i+1);
			bag_szr->AddGrowableCol(i+1, 1);
		}
		for (int i=0; i<num_vars; ++i) {
			if (bag_szr->IsRowGrowable(i)) bag_szr->RemoveGrowableRow(i);
			bag_szr->AddGrowableRow(i, 1);
		}
	}
	panel_v_szr->Add(bag_szr, 1, wxEXPAND);
	LOG(bag_szr->GetItemCount());
	top_h_sizer->RecalcSizes();
	//Refresh();
	LOG_MSG("Exiting ScatterPlotMatFrame::SetupPanelForNumVariables");
}

void ScatterPlotMatFrame::UpdateMessageWin()
{
	if (!message_win) return;
	wxString s;
	s << "<!DOCTYPE html>";
	s << "<html>";
	s << "<head>";
	s << "  <style type=\"text/css\">";	
	s << "  body {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    font-size: large;";
	s << "    color: blue;";
	s << "  }";
	
	s << "  h1 {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    color: blue;";
	s << "  }";
	s <<   "</style>";
 
	s << "</head>";
	s << "<body>";
	s << "<br /><br /><br />";
	//s << "  <h1 align=\"center\">Message</h1>";
	s << "<center><p>";
	s << "<font face=\"verdana,arial,sans-serif\" color=\"black\" size=\"5\">";
	
	int count = var_man.GetVarsCount();
	if (count == 0) {
		s << "Please use<br />";
		s << "<font color=\"blue\">Options > Add/Remove Variables<br /></font>";
		s << "to specify two or more variables.";
	} if (count == 1) {
		s << "Variable <font color=\"blue\">" << var_man.GetName(0);
		s << "</font> is specified. ";
		s << "Please add at least one additional variable.";
	} if (count > 1) {
		s << "Variables specified: <br />";
		for (int i=0; i<count; ++i) {
			s << var_man.GetName(i);
			if (i+1 < count) s << "<br />";
		}
	}
	
	s << "  </font></p></center>";
	s << "</body>";
	s << "</html>";
	message_win->SetPage(s );
}

/** Adds/removes variables from data_map according to variables present
 in var_man. */
void ScatterPlotMatFrame::UpdateDataMapFromVarMan()
{
	LOG_MSG("Entering ScatterPlotMatFrame::UpdateDataMapFromVarMan");
	using namespace std;
	// get set of var_man names
	set<wxString> vm_nms;
	for (int i=0; i<var_man.GetVarsCount(); ++i) {
		vm_nms.insert(var_man.GetName(i));
	}
	
	// remove items from data_map not in vm_nms
	set<wxString> to_remove;
	LOG_MSG("to_remove from data_map:");
	for (data_map_type::iterator i=data_map.begin(); i!=data_map.end(); ++i) {
		wxString nm(i->first);
		if (vm_nms.find(nm) != vm_nms.end()) continue;
		to_remove.insert(nm);
		LOG_MSG("  " + nm);
	}
	
	for (set<wxString>::iterator i=to_remove.begin(); i!=to_remove.end(); ++i) {
		LOG_MSG("Being removed from data_map: " + (*i));
		data_map.erase(*i);
	}
	
	// add items to data_map that are in vm_nms, but not currently in data_map
	set<wxString> to_add;
	for (set<wxString>::iterator i=vm_nms.begin(); i!=vm_nms.end(); ++i) {
		wxString nm(*i);
		if (data_map.find(nm) != data_map.end()) continue;
		to_add.insert(nm);
		LOG_MSG("Must add to data_map: " + nm);
	}
	
	TableInterface* table_int = project->GetTableInt();
	for (set<wxString>::iterator i=to_add.begin(); i!=to_add.end(); ++i) {
		wxString nm = (*i);
		LOG_MSG(nm);
		int c_id = table_int->FindColId(nm);
		if (c_id < 0) {
			LOG_MSG("Error, variable not found in table: " + nm);
			continue;
		}
		int tms = table_int->GetColTimeSteps(c_id);
		LOG(tms);
		pair<wxString, vec_vec_dbl_type> p(nm, vec_vec_dbl_type(tms));
		data_map.insert(p);
		data_map_type::iterator e = data_map.find(nm);
		if (e == data_map.end()) {
			LOG_MSG("Could not find element just inserted! " + nm);
			continue;
		}
		for (int t=0; t<tms; ++t) {
			table_int->GetColData(c_id, t, e->second[t]);
		}
	}
	
	LOG_MSG("Exiting ScatterPlotMatFrame::UpdateDataMapFromVarMan");
}

wxString ScatterPlotMatFrame::GetHelpHtml()
{
	wxString s;
	s << "<!DOCTYPE html>";
	s << "<html>";
	s << "<head>";
	s << "  <style type=\"text/css\">";
	s << "  body {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    font-size: small;";
	s << "  }";
	s << "  h1 h2 {";
	s << "    font-family: \"Trebuchet MS\", Arial, Helvetica, sans-serif;";
	s << "    color: blue;";
	s << "  }";
	s <<   "</style>";
	s << "</head>";
	s << "<body>";
	
	s << "<h2>Scatter Plot Matrix Help</h2>";
	s << "<p>";
	s << "Scatter Plot Matrix displays an array of scatter plots for ";
	s << "every combination of pairs of variables.  For n variables, ";
	s << "n(n-1) scatter plots are generated.";
	
	s << "</body>";
	s << "</html>";
	return s;	
}

void ScatterPlotMatFrame::GetVizInfo(vector<wxString>& vars)
{
	for (int i=0; i<var_man.GetVarsCount(); ++i) {
		vars.push_back(var_man.GetName(i));
	}
}
