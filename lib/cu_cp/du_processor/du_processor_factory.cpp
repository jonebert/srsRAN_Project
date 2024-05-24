/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "du_processor_factory.h"
#include "../cu_cp_controller/common_task_scheduler.h"
#include "du_processor_impl.h"

/// Notice this would be the only place were we include concrete class implementation files.

using namespace srsran;
using namespace srs_cu_cp;

std::unique_ptr<du_processor>
srsran::srs_cu_cp::create_du_processor(const du_processor_config_t&        du_processor_config_,
                                       du_processor_cu_cp_notifier&        cu_cp_notifier_,
                                       f1ap_message_notifier&              f1ap_notifier_,
                                       rrc_ue_nas_notifier&                rrc_ue_nas_pdu_notifier_,
                                       rrc_ue_control_notifier&            rrc_ue_ngap_ctrl_notifier_,
                                       rrc_du_measurement_config_notifier& rrc_du_cu_cp_notifier,
                                       common_task_scheduler&              common_task_sched_,
                                       du_processor_ue_manager&            ue_manager_,
                                       timer_manager&                      timers_,
                                       task_executor&                      ctrl_exec_)
{
  auto du_processor = std::make_unique<du_processor_impl>(du_processor_config_,
                                                          cu_cp_notifier_,
                                                          f1ap_notifier_,
                                                          rrc_ue_nas_pdu_notifier_,
                                                          rrc_ue_ngap_ctrl_notifier_,
                                                          rrc_du_cu_cp_notifier,
                                                          common_task_sched_,
                                                          ue_manager_,
                                                          timers_,
                                                          ctrl_exec_);
  return du_processor;
}
