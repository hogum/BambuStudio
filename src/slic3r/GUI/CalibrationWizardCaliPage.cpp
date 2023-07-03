#include "CalibrationWizardCaliPage.hpp"
#include "I18N.hpp"
#include "Widgets/Label.hpp"

namespace Slic3r { namespace GUI {

static const wxString NA_STR = _L("N/A");


CalibrationCaliPage::CalibrationCaliPage(wxWindow* parent, CalibMode cali_mode, CaliPageType cali_type,
    wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : CalibrationWizardPage(parent, id, pos, size, style)
{
    m_cali_mode = cali_mode;

    m_page_type = cali_type;

    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_page(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

CalibrationCaliPage::~CalibrationCaliPage()
{
    m_printing_panel->get_pause_resume_button()->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CalibrationCaliPage::on_subtask_pause_resume), NULL, this);
    m_printing_panel->get_abort_button()->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CalibrationCaliPage::on_subtask_abort), NULL, this);
}

void CalibrationCaliPage::create_page(wxWindow* parent)
{
    m_page_caption = new CaliPageCaption(parent, m_cali_mode);
    m_page_caption->show_prev_btn(true);
    m_top_sizer->Add(m_page_caption, 0, wxEXPAND, 0);

    wxArrayString steps;
    steps.Add(_L("Preset"));
    steps.Add(_L("Calibration"));
    steps.Add(_L("Record"));
    m_step_panel = new CaliPageStepGuide(parent, steps);
    m_step_panel->set_steps(1);
    m_top_sizer->Add(m_step_panel, 0, wxEXPAND, 0);

    m_printing_picture = new wxStaticBitmap(parent, wxID_ANY, wxNullBitmap);
    m_top_sizer->Add(m_printing_picture, 0, wxALIGN_CENTER, 0);
    m_top_sizer->AddSpacer(FromDIP(20));

    set_cali_img();

    m_printing_panel = new PrintingTaskPanel(parent, PrintingTaskType::CALIBRATION);
    m_printing_panel->SetDoubleBuffered(true);
    m_printing_panel->SetSize({ CALIBRATION_PROGRESSBAR_LENGTH, -1 });
    m_printing_panel->SetMinSize({ CALIBRATION_PROGRESSBAR_LENGTH, -1 });
    m_printing_panel->enable_pause_resume_button(false, "resume_disable");
    m_printing_panel->enable_abort_button(false);

    m_top_sizer->Add(m_printing_panel, 0, wxALIGN_CENTER, 0);
    m_action_panel = new CaliPageActionPanel(parent, m_cali_mode, CaliPageType::CALI_PAGE_CALI);
    m_top_sizer->Add(m_action_panel, 0, wxEXPAND, 0);

    m_printing_panel->get_pause_resume_button()->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CalibrationCaliPage::on_subtask_pause_resume), NULL, this);
    m_printing_panel->get_abort_button()->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(CalibrationCaliPage::on_subtask_abort), NULL, this);

    Layout();
}

void CalibrationCaliPage::on_subtask_pause_resume(wxCommandEvent& event)
{
    DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject* obj = dev->get_selected_machine();
    if (!obj) return;

    if (obj->can_resume())
        obj->command_task_resume();
    else
        obj->command_task_pause();
}

void CalibrationCaliPage::on_subtask_abort(wxCommandEvent& event)
{
    DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject* obj = dev->get_selected_machine();
    if (!obj) return;

    if (abort_dlg == nullptr) {
        abort_dlg = new SecondaryCheckDialog(this->GetParent(), wxID_ANY, _L("Cancel print"));
        abort_dlg->Bind(EVT_SECONDARY_CHECK_CONFIRM, [this, obj](wxCommandEvent& e) {
            if (obj) obj->command_task_abort();
            });
    }
    abort_dlg->update_text(_L("Are you sure you want to cancel this print?"));
    abort_dlg->on_show();
}


void CalibrationCaliPage::set_cali_img()
{
    if (m_cali_mode == CalibMode::Calib_PA_Line) {
        m_printing_picture->SetBitmap(create_scaled_bitmap("extrusion_calibration_tips_en", nullptr, 400));
    }
    else if (m_cali_mode == CalibMode::Calib_Flow_Rate) {
        if (m_page_type == CaliPageType::CALI_PAGE_CALI)
            m_printing_picture->SetBitmap(create_scaled_bitmap("flow_rate_calibration", nullptr, 400));
        if (m_page_type == CaliPageType::CALI_PAGE_FINE_CALI)
            m_printing_picture->SetBitmap(create_scaled_bitmap("flow_rate_calibration_fine", nullptr, 400));
        else
            m_printing_picture->SetBitmap(create_scaled_bitmap("flow_rate_calibration", nullptr, 400));
    }
    else if (m_cali_mode == CalibMode::Calib_Vol_speed_Tower) {
        m_printing_picture->SetBitmap(create_scaled_bitmap("max_volumetric_speed_calibration", nullptr, 400));
    }
}

void CalibrationCaliPage::update(MachineObject* obj)
{
    static int get_result_count = 0;
    // enable calibration when finished
    bool enable_cali = false;
    if (obj) {
        if (m_cali_mode == CalibMode::Calib_PA_Line) {
            if (m_cali_method == CalibrationMethod::CALI_METHOD_AUTO) {
                if (obj->get_pa_calib_result) {
                    enable_cali = true;
                } else {
                    if (get_obj_calibration_mode(obj) == m_cali_mode && obj->is_printing_finished()) {
                        // use selected diameter, add a counter to timeout, add a warning tips when get result failed
                        CalibUtils::emit_get_PA_calib_results(get_selected_calibration_nozzle_dia(obj));
                        BOOST_LOG_TRIVIAL(trace) << "CalibUtils::emit_get_PA_calib_results, auto count = " << get_result_count++;
                    } else {
                        ;
                    }
                }
            } else if (m_cali_method == CalibrationMethod::CALI_METHOD_MANUAL) {
                if (get_obj_calibration_mode(obj) == m_cali_mode && obj->is_printing_finished()) {
                    enable_cali = true;
                } else {
                    enable_cali = false;
                }
            } else {
                assert(false);
            }
            m_action_panel->enable_button(CaliPageActionType::CALI_ACTION_CALI_NEXT, enable_cali);
        } else if (m_cali_mode == CalibMode::Calib_Flow_Rate) {
            if (m_cali_method == CalibrationMethod::CALI_METHOD_AUTO) {
                if (obj->get_flow_calib_result) {
                    enable_cali = true;
                } else {
                    if (get_obj_calibration_mode(obj) == m_cali_mode && obj->is_printing_finished()) {
                        // use selected diameter, add a counter to timeout, add a warning tips when get result failed
                        CalibUtils::emit_get_flow_ratio_calib_results(get_selected_calibration_nozzle_dia(obj));
                    } else {
                        ;
                    }
                }
            } else if (m_cali_method == CalibrationMethod::CALI_METHOD_MANUAL) {
                if (get_obj_calibration_mode(obj) == m_cali_mode && obj->is_printing_finished()) {
                    // use selected diameter, add a counter to timeout, add a warning tips when get result failed
                    CalibUtils::emit_get_flow_ratio_calib_results(get_selected_calibration_nozzle_dia(obj));
                    enable_cali = true;
                }
                else {
                    enable_cali = false;
                }
            } else {
                assert(false);
            }
            m_action_panel->enable_button(CaliPageActionType::CALI_ACTION_NEXT, enable_cali);
        } 
        else if (m_cali_mode == CalibMode::Calib_Vol_speed_Tower) {
            if (get_obj_calibration_mode(obj) == m_cali_mode && obj->is_printing_finished()) {
                enable_cali = true;
            } else {
                ;
            }
        }
        else {
            assert(false);
        }
        m_action_panel->enable_button(CaliPageActionType::CALI_ACTION_NEXT, enable_cali);
    }

    // only display calibration printing status
    if (get_obj_calibration_mode(obj) == m_cali_mode) {
        update_subtask(obj);
    } else {
        update_subtask(nullptr);
    }
}

void CalibrationCaliPage::update_subtask(MachineObject* obj)
{
    if (!obj) return;

    if (obj->is_support_layer_num) {
        m_printing_panel->update_layers_num(true);
    }
    else {
        m_printing_panel->update_layers_num(false);
    }

    if (obj->is_system_printing()
        || obj->is_in_calibration()) {
        reset_printing_values();
    }
    else if (obj->is_in_printing() || obj->print_status == "FINISH") {
        if (obj->is_in_prepare() || obj->print_status == "SLICING") {
            m_printing_panel->get_market_scoring_button()->Hide();
            m_printing_panel->enable_abort_button(false);
            m_printing_panel->enable_pause_resume_button(false, "pause_disable");
            wxString prepare_text;
            bool show_percent = true;

            if (obj->is_in_prepare()) {
                prepare_text = wxString::Format(_L("Downloading..."));
            }
            else if (obj->print_status == "SLICING") {
                if (obj->queue_number <= 0) {
                    prepare_text = wxString::Format(_L("Cloud Slicing..."));
                }
                else {
                    prepare_text = wxString::Format(_L("In Cloud Slicing Queue, there are %s tasks ahead."), std::to_string(obj->queue_number));
                    show_percent = false;
                }
            }
            else
                prepare_text = wxString::Format(_L("Downloading..."));

            if (obj->gcode_file_prepare_percent >= 0 && obj->gcode_file_prepare_percent <= 100 && show_percent)
                prepare_text += wxString::Format("(%d%%)", obj->gcode_file_prepare_percent);

            m_printing_panel->update_stage_value(prepare_text, 0);
            m_printing_panel->update_progress_percent(NA_STR, wxEmptyString);
            m_printing_panel->update_left_time(NA_STR);
            m_printing_panel->update_layers_num(true, wxString::Format(_L("Layer: %s"), NA_STR));
            m_printing_panel->update_subtask_name(wxString::Format("%s", GUI::from_u8(obj->subtask_name)));


            if (obj->get_modeltask() && obj->get_modeltask()->design_id > 0) {
                m_printing_panel->show_profile_info(true, wxString::FromUTF8(obj->get_modeltask()->profile_name));
            }
            else {
                m_printing_panel->show_profile_info(false);
            }

            update_basic_print_data(false, obj->slice_info->weight, obj->slice_info->prediction);
        }
        else {
            if (obj->can_resume()) {
                m_printing_panel->enable_pause_resume_button(true, "resume");

            }
            else {
                m_printing_panel->enable_pause_resume_button(true, "pause");
            }
            if (obj->print_status == "FINISH") {

                m_printing_panel->enable_abort_button(false);
                m_printing_panel->enable_pause_resume_button(false, "resume_disable");

                bool is_market_task = obj->get_modeltask() && obj->get_modeltask()->design_id > 0;
                if (is_market_task) {
                    m_printing_panel->get_market_scoring_button()->Show();
                    BOOST_LOG_TRIVIAL(info) << "SHOW_SCORE_BTU: design_id [" << obj->get_modeltask()->design_id << "] print_finish [" << m_print_finish << "]";
                    if (!m_print_finish && IsShownOnScreen()) {
                        m_print_finish = true;
                    }
                }
                else {
                    m_printing_panel->get_market_scoring_button()->Hide();
                }
            }
            else {
                m_printing_panel->enable_abort_button(true);
                m_printing_panel->get_market_scoring_button()->Hide();
                if (m_print_finish) {
                    m_print_finish = false;
                }
            }
            // update printing stage

            m_printing_panel->update_left_time(obj->mc_left_time);
            if (obj->subtask_) {
                m_printing_panel->update_stage_value(obj->get_curr_stage(), obj->subtask_->task_progress);
                m_printing_panel->update_progress_percent(wxString::Format("%d", obj->subtask_->task_progress), "%");
                m_printing_panel->update_layers_num(true, wxString::Format(_L("Layer: %d/%d"), obj->curr_layer, obj->total_layers));

            }
            else {
                m_printing_panel->update_stage_value(obj->get_curr_stage(), 0);
                m_printing_panel->update_progress_percent(NA_STR, wxEmptyString);
                m_printing_panel->update_layers_num(true, wxString::Format(_L("Layer: %s"), NA_STR));
            }
        }

        m_printing_panel->update_subtask_name(wxString::Format("%s", GUI::from_u8(obj->subtask_name)));

        if (obj->get_modeltask() && obj->get_modeltask()->design_id > 0) {
            m_printing_panel->show_profile_info(wxString::FromUTF8(obj->get_modeltask()->profile_name));
        }
        else {
            m_printing_panel->show_profile_info(false);
        }

    }
    else {
        reset_printing_values();
    }

    this->Layout();
}

void CalibrationCaliPage::update_basic_print_data(bool def, float weight, int prediction)
{
    if (def) {
        wxString str_prediction = wxString::Format("%s", get_bbl_time_dhms(prediction));
        wxString str_weight = wxString::Format("%.2fg", weight);

        m_printing_panel->show_priting_use_info(true, str_prediction, str_weight);
    }
    else {
        m_printing_panel->show_priting_use_info(false, "0m", "0g");
    }
}

void CalibrationCaliPage::reset_printing_values()
{
    m_printing_panel->enable_pause_resume_button(false, "pause_disable");
    m_printing_panel->enable_abort_button(false);
    m_printing_panel->reset_printing_value();
    m_printing_panel->update_subtask_name(NA_STR);
    m_printing_panel->show_profile_info(false);
    m_printing_panel->update_stage_value(wxEmptyString, 0);
    m_printing_panel->update_progress_percent(NA_STR, wxEmptyString);
    m_printing_panel->get_market_scoring_button()->Hide();
    m_printing_panel->update_left_time(NA_STR);
    m_printing_panel->update_layers_num(true, wxString::Format(_L("Layer: %s"), NA_STR));
    update_basic_print_data(false);
    this->Layout();
}

void CalibrationCaliPage::on_device_connected(MachineObject* obj)
{
    ;
}

void CalibrationCaliPage::set_cali_method(CalibrationMethod method)
{
    m_cali_method = method;

    wxArrayString auto_steps;
    auto_steps.Add(_L("Preset"));
    auto_steps.Add(_L("Calibration"));
    auto_steps.Add(_L("Record"));

    wxArrayString manual_steps;
    manual_steps.Add(_L("Preset"));
    manual_steps.Add(_L("Calibration1"));
    manual_steps.Add(_L("Calibration2"));
    manual_steps.Add(_L("Record"));

    if (method == CalibrationMethod::CALI_METHOD_AUTO) {
        m_step_panel->set_steps_string(auto_steps);
        m_step_panel->set_steps(1);
    }
    else if (method == CalibrationMethod::CALI_METHOD_MANUAL) {
        if (m_cali_mode == CalibMode::Calib_PA_Line) {
            m_step_panel->set_steps_string(auto_steps);
            m_step_panel->set_steps(1);
        } else {
            m_step_panel->set_steps_string(manual_steps);
            if (m_page_type == CaliPageType::CALI_PAGE_CALI)
                m_step_panel->set_steps(1);
            else if (m_page_type == CaliPageType::CALI_PAGE_FINE_CALI) {
                m_step_panel->set_steps(2);
            }
            else {
                m_step_panel->set_steps(1);
            }
        }
    }
    else {
        assert(false);
    }
}

float CalibrationCaliPage::get_selected_calibration_nozzle_dia(MachineObject* obj)
{
    // return selected if this is set
    if (obj->cali_selected_nozzle_dia > 1e-3 && obj->cali_selected_nozzle_dia < 10.0f)
        return obj->cali_selected_nozzle_dia;

    // return default nozzle if nozzle diameter is set
    if (obj->nozzle_diameter > 1e-3 && obj->nozzle_diameter < 10.0f)
        return obj->nozzle_diameter;

    // return 0.4 by default
    return 0.4;
}

}}