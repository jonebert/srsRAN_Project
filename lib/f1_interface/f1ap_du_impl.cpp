/*
 *
 * copyright 2013-2022 software radio systems limited
 *
 * by using this file, you agree to the terms and conditions set
 * forth in the license file which can be found at the top level of
 * the distribution.
 *
 */

#include "f1ap_du_impl.h"
#include "../ran/gnb_format.h"
#include "procedures/f1ap_du_setup_procedure.h"
#include "srsgnb/asn1/f1ap.h"
#include "srsgnb/support/async/event_signal.h"

using namespace srsgnb;
using namespace asn1::f1ap;
using namespace srs_du;

f1ap_du_impl::f1ap_du_impl(timer_manager& timers_, f1c_message_notifier& message_notifier_) :
  logger(srslog::fetch_basic_logger("DU-F1AP")),
  timers(timers_),
  f1c_notifier(message_notifier_),
  events(std::make_unique<f1ap_event_manager>())
{
  f1c_setup_timer = timers.create_unique_timer();
  f1c_setup_timer.set(1000, [](uint32_t tid) {
    // TODO
  });
}

// Note: For fwd declaration of member types, dtor cannot be trivial.
f1ap_du_impl::~f1ap_du_impl() {}

async_task<f1_setup_response_message> f1ap_du_impl::handle_f1ap_setup_request(const f1_setup_request_message& request)
{
  return launch_async<f1ap_du_setup_procedure>(request, f1c_notifier, *events, logger);
}

async_task<f1ap_ue_create_response> f1ap_du_impl::handle_ue_creation_request(const f1ap_ue_create_request& msg)
{
  // TODO: add UE create procedure
  // TODO: Change execution context.
  return launch_async(
      [this, msg, resp = f1ap_ue_create_response{}](coro_context<async_task<f1ap_ue_create_response> >& ctx) mutable {
        CORO_BEGIN(ctx);
        resp.result = false;
        if (not ctxt.ue_ctxt_manager.contains(msg.ue_index)) {
          ctxt.ue_ctxt_manager.emplace(msg.ue_index);
          ctxt.ue_ctxt_manager[msg.ue_index].crnti             = INVALID_RNTI; // Needed?
          ctxt.ue_ctxt_manager[msg.ue_index].gnb_du_f1ap_ue_id = msg.ue_index;
          ctxt.ue_ctxt_manager[msg.ue_index].gnb_cu_f1ap_ue_id = (du_ue_index_t)-1; // TODO

          resp.result = true;
        }
        CORO_RETURN(resp);
      });
}

void f1ap_du_impl::handle_dl_rrc_message_transfer(const asn1::f1ap::dlrrc_msg_transfer_s& msg)
{
  logger.info("Received F1AP DL RRC msg.");
  // TODO: handle dl rrc message
}

async_task<f1ap_ue_context_modification_response_message>
f1ap_du_impl::handle_ue_context_modification(const f1ap_ue_context_modification_required_message& msg)
{
  // TODO: add procedure implementation

  f1ap_event_manager::f1ap_ue_context_modification_outcome_t ue_ctxt_mod_resp;

  return launch_async([this, ue_ctxt_mod_resp, res = f1ap_ue_context_modification_response_message{}, msg](
                          coro_context<async_task<f1ap_ue_context_modification_response_message> >& ctx) mutable {
    CORO_BEGIN(ctx);

    CORO_AWAIT_VALUE(ue_ctxt_mod_resp, events->f1ap_ue_context_modification_response);

    if (ue_ctxt_mod_resp.has_value()) {
      logger.info("Received F1AP PDU with successful outcome.");
      res.confirm = *ue_ctxt_mod_resp.value();
      res.success = true;
    } else {
      logger.info("Received F1AP PDU with unsuccessful outcome.");
      res.refuse  = *ue_ctxt_mod_resp.error();
      res.success = false;
    }

    CORO_RETURN(res);
  });
}

void f1ap_du_impl::handle_pdu(f1_rx_pdu pdu)
{
  log_ue_event(logger, ue_event_prefix{"UL", pdu.ue_index}.set_channel("SRB0"), "Received PDU.");

  // FIXME: fill message
  asn1::f1ap::f1_ap_pdu_c f1ap_pdu;

  // Make it an initial message to let test pass.
  f1ap_pdu.set_init_msg();

  // send to handler
  f1c_notifier.on_new_message(f1ap_pdu);
}

void f1ap_du_impl::handle_message(const asn1::f1ap::f1_ap_pdu_c& pdu)
{
  logger.info("Handling F1AP PDU of type \"{}\"", pdu.type().to_string());

  switch (pdu.type().value) {
    case asn1::f1ap::f1_ap_pdu_c::types_opts::init_msg:
      handle_initiating_message(pdu.init_msg());
      break;
    case asn1::f1ap::f1_ap_pdu_c::types_opts::successful_outcome:
      handle_successful_outcome(pdu.successful_outcome());
      break;
    case asn1::f1ap::f1_ap_pdu_c::types_opts::unsuccessful_outcome:
      handle_unsuccessful_outcome(pdu.unsuccessful_outcome());
      break;
    default:
      logger.error("Invalid PDU type");
      break;
  }
}

void f1ap_du_impl::handle_initiating_message(const asn1::f1ap::init_msg_s& msg)
{
  switch (msg.value.type().value) {
    case asn1::f1ap::f1_ap_elem_procs_o::init_msg_c::types_opts::dlrrc_msg_transfer:
      handle_dl_rrc_message_transfer(msg.value.dlrrc_msg_transfer());
      break;
    default:
      logger.error("Initiating message of type {} is not supported", msg.value.type().to_string());
  }
}

void f1ap_du_impl::handle_successful_outcome(const asn1::f1ap::successful_outcome_s& outcome)
{
  switch (outcome.value.type().value) {
    case asn1::f1ap::f1_ap_elem_procs_o::successful_outcome_c::types_opts::f1_setup_resp:
      events->f1ap_setup_response.set(&outcome.value.f1_setup_resp());
      break;
    default:
      logger.error("Successful outcome of type {} is not supported", outcome.value.type().to_string());
  }
}

void f1ap_du_impl::handle_unsuccessful_outcome(const asn1::f1ap::unsuccessful_outcome_s& outcome)
{
  switch (outcome.value.type().value) {
    case asn1::f1ap::f1_ap_elem_procs_o::unsuccessful_outcome_c::types_opts::f1_setup_fail:
      events->f1ap_setup_response.set(&outcome.value.f1_setup_fail());
      break;
    default:
      logger.error("Successful outcome of type {} is not supported", outcome.value.type().to_string());
  }
}
