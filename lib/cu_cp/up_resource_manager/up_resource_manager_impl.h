/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/cu_cp/up_resource_manager.h"
#include <map>

namespace srsran {

namespace srs_cu_cp {

/// UP resource manager implementation
class up_resource_manager_impl final : public up_resource_manager
{
public:
  up_resource_manager_impl(const up_resource_manager_cfg& cfg);
  ~up_resource_manager_impl() = default;

  bool
  validate_request(const slotted_id_vector<pdu_session_id_t, cu_cp_pdu_session_res_setup_item>& setup_items) override;
  bool validate_request(const cu_cp_pdu_session_resource_modify_request& pdu) override;
  bool validate_request(const cu_cp_pdu_session_resource_release_command& pdu) override;

  up_config_update
  calculate_update(const slotted_id_vector<pdu_session_id_t, cu_cp_pdu_session_res_setup_item>& setup_items) override;
  up_config_update calculate_update(const cu_cp_pdu_session_resource_modify_request& pdu) override;
  up_config_update calculate_update(const cu_cp_pdu_session_resource_release_command& pdu) override;

  bool                          apply_config_update(const up_config_update_result& config) override;
  const up_pdu_session_context& get_pdu_session_context(pdu_session_id_t psi) override;
  const up_drb_context&         get_drb_context(drb_id_t drb_id) override;
  bool                          has_pdu_session(pdu_session_id_t pdu_session_id) override;
  size_t                        get_nof_drbs() override;
  size_t                        get_nof_pdu_sessions() override;
  size_t                        get_nof_qos_flows(pdu_session_id_t psi) override;
  size_t                        get_total_nof_qos_flows() override;
  std::vector<pdu_session_id_t> get_pdu_sessions() override;
  std::vector<drb_id_t>         get_drbs() override;
  up_context                    get_up_context() override;
  void                          set_up_context(const up_context& ctx) override;

private:
  bool valid_5qi(const five_qi_t five_qi);

  up_resource_manager_cfg cfg;

  up_context context; // The currently active state.

  srslog::basic_logger& logger;
};

} // namespace srs_cu_cp

} // namespace srsran
