/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "lib/cu_cp/cu_cp_impl.h"
#include "test_helpers.h"
#include "tests/unittests/e1ap/common/test_helpers.h"
#include "tests/unittests/e1ap/cu_cp/e1ap_cu_cp_test_helpers.h"
#include "tests/unittests/f1ap/common/test_helpers.h"
#include "tests/unittests/f1ap/cu_cp/f1ap_cu_test_helpers.h"
#include "tests/unittests/ngap/test_helpers.h"
#include "srsran/cu_cp/cu_cp_types.h"
#include "srsran/support/executors/manual_task_worker.h"
#include <gtest/gtest.h>

namespace srsran {
namespace srs_cu_cp {

/// Fixture class for CU-CP test
class cu_cp_test : public ::testing::Test
{
protected:
  cu_cp_test();
  ~cu_cp_test() override;

  void test_amf_connection();

  void test_e1ap_attach();
  void test_du_attach(du_index_t du_index, unsigned gnb_du_id, unsigned nrcell_id, pci_t pci);

  void attach_ue(gnb_du_ue_f1ap_id_t du_ue_id, gnb_cu_ue_f1ap_id_t cu_ue_id, rnti_t crnti, du_index_t du_index);
  void authenticate_ue(amf_ue_id_t         amf_ue_id,
                       ran_ue_id_t         ran_ue_id,
                       du_index_t          du_index,
                       gnb_du_ue_f1ap_id_t du_ue_id,
                       gnb_cu_ue_f1ap_id_t cu_ue_id);
  void setup_security(amf_ue_id_t         amf_ue_id,
                      ran_ue_id_t         ran_ue_id,
                      du_index_t          du_index,
                      gnb_du_ue_f1ap_id_t du_ue_id,
                      gnb_cu_ue_f1ap_id_t cu_ue_id);
  void test_preamble_all_connected(du_index_t du_index, pci_t pci);
  void test_preamble_ue_creation(du_index_t          du_index,
                                 gnb_du_ue_f1ap_id_t du_ue_id,
                                 gnb_cu_ue_f1ap_id_t cu_ue_id,
                                 pci_t               pci,
                                 rnti_t              crnti,
                                 amf_ue_id_t         amf_ue_id,
                                 ran_ue_id_t         ran_ue_id);
  void test_preamble_ue_full_attach(du_index_t             du_index,
                                    gnb_du_ue_f1ap_id_t    du_ue_id,
                                    gnb_cu_ue_f1ap_id_t    cu_ue_id,
                                    pci_t                  pci,
                                    rnti_t                 crnti,
                                    amf_ue_id_t            amf_ue_id,
                                    ran_ue_id_t            ran_ue_id,
                                    gnb_cu_cp_ue_e1ap_id_t cu_cp_ue_e1ap_id,
                                    gnb_cu_up_ue_e1ap_id_t cu_up_ue_e1ap_id);
  bool check_minimal_paging_result();
  bool check_paging_result();

  srslog::basic_logger& test_logger  = srslog::fetch_basic_logger("TEST");
  srslog::basic_logger& cu_cp_logger = srslog::fetch_basic_logger("CU-CP");

  dummy_ngap_amf_notifier        ngap_amf_notifier;
  std::unique_ptr<timer_manager> timers = std::make_unique<timer_manager>(256);

  manual_task_worker ctrl_worker{128};

  std::unique_ptr<cu_cp_impl> cu_cp_obj;

  dummy_cu_cp_f1c_gateway  f1c_gw;
  dummy_cu_cp_e1ap_gateway e1ap_gw;

  std::unique_ptr<ngap_message_handler> dummy_amf;
};

} // namespace srs_cu_cp
} // namespace srsran
