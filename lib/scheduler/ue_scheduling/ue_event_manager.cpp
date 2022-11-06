/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "ue_event_manager.h"
#include "../../../lib/ran/gnb_format.h"

using namespace srsgnb;

/// Class used to enqueue all processed events in a slot and log them in a single batch.
class srsgnb::event_logger
{
public:
  event_logger(du_cell_index_t cell_index_, srslog::basic_logger& logger_) :
    cell_index(cell_index_), enabled(logger_.info.enabled()), logger(logger_)
  {
  }
  event_logger(const event_logger&)            = delete;
  event_logger(event_logger&&)                 = delete;
  event_logger& operator=(const event_logger&) = delete;
  event_logger& operator=(event_logger&&)      = delete;
  ~event_logger()
  {
    if (enabled and fmtbuf.size() > 0) {
      if (cell_index < MAX_NOF_DU_CELLS) {
        logger.info("SCHED: Processed events, cell_index={}: [{}]", cell_index, to_c_str(fmtbuf));
      } else {
        logger.info("SCHED: Processed events: [{}]", to_c_str(fmtbuf));
      }
    }
  }

  template <typename... Args>
  void enqueue(const char* fmtstr, Args&&... args)
  {
    if (enabled) {
      if (fmtbuf.size() > 0) {
        fmt::format_to(fmtbuf, ", ");
      }
      fmt::format_to(fmtbuf, fmtstr, std::forward<Args>(args)...);
    }
  }

private:
  du_cell_index_t       cell_index;
  bool                  enabled;
  srslog::basic_logger& logger;
  fmt::memory_buffer    fmtbuf;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ue_event_manager::ue_event_manager(ue_list& ue_db, sched_configuration_notifier& mac_notifier) :
  ue_db(ue_db), mac_notifier(mac_notifier), logger(srslog::fetch_basic_logger("MAC"))
{
}

void ue_event_manager::handle_add_ue_request(const sched_ue_creation_request_message& ue_request)
{
  // Create UE object outside the scheduler slot indication handler to minimize latency.
  std::unique_ptr<ue> u = std::make_unique<ue>(*du_cells[ue_request.cells[0].cell_index].cfg, ue_request);

  // Defer UE object addition to ue list to the slot indication handler.
  common_events.emplace(MAX_NOF_DU_UES, [this, u = std::move(u)](event_logger& ev_logger) mutable {
    ev_logger.enqueue("ue_add(ueId={})", u->ue_index);
    log_ue_proc_event(logger.info, ue_event_prefix{} | u->ue_index, "Sched UE Configuration", "started.");

    // Insert UE in UE repository.
    du_ue_index_t ueidx = u->ue_index;
    ue_db.insert(ueidx, std::move(u));

    log_ue_proc_event(logger.info, ue_event_prefix{} | ueidx, "Sched UE Configuration", "completed.");
    // Notify Scheduler UE configuration is complete.
    mac_notifier.on_ue_config_complete(ueidx);
  });
}

void ue_event_manager::handle_ue_reconfiguration_request(const sched_ue_reconfiguration_message& ue_request)
{
  common_events.emplace(ue_request.ue_index, [this, ue_request = std::move(ue_request)](event_logger& ev_logger) {
    ev_logger.enqueue("ue_cfg(ueId={})", ue_request.ue_index);
    log_ue_proc_event(logger.info, ue_event_prefix{} | ue_request.ue_index, "Sched UE Reconfiguration", "started.");

    // Configure existing UE.
    ue_db[ue_request.ue_index].handle_reconfiguration_request(ue_request);

    log_ue_proc_event(logger.info, ue_event_prefix{} | ue_request.ue_index, "Sched UE Reconfiguration", "completed.");

    // Notify Scheduler UE configuration is complete.
    mac_notifier.on_ue_config_complete(ue_request.ue_index);
  });
}

void ue_event_manager::handle_ue_delete_request(du_ue_index_t ue_index)
{
  common_events.emplace(ue_index, [this, ue_index](event_logger& ev_logger) {
    ev_logger.enqueue("ue_rem(ueId={})", ue_index);
    log_ue_proc_event(logger.info, ue_event_prefix{} | ue_index, "Sched UE Deletion", "started.");

    // Remove UE from repository.
    ue_db.erase(ue_index);

    log_ue_proc_event(logger.info, ue_event_prefix{} | ue_index, "Sched UE Deletion", "completed.");

    // Notify Scheduler UE configuration is complete.
    mac_notifier.on_ue_delete_response(ue_index);
  });
}

void ue_event_manager::handle_ul_bsr_indication(const ul_bsr_indication_message& bsr_ind)
{
  srsgnb_sanity_check(cell_exists(bsr_ind.cell_index), "Invalid cell index");

  common_events.emplace(bsr_ind.ue_index, [this, bsr_ind](event_logger& ev_logger) {
    ev_logger.enqueue("ul_bsr(ueId={})", bsr_ind.ue_index);
    ue_db[bsr_ind.ue_index].handle_bsr_indication(bsr_ind);
  });
}

void ue_event_manager::handle_crc_indication(const ul_crc_indication& crc_ind)
{
  srsgnb_sanity_check(cell_exists(crc_ind.cell_index), "Invalid cell index");

  for (unsigned i = 0; i != crc_ind.crcs.size(); ++i) {
    cell_specific_events[crc_ind.cell_index].emplace(
        crc_ind.crcs[i].ue_index, [this, crc = crc_ind.crcs[i]](ue_cell& ue_cc, event_logger& ev_logger) {
          ev_logger.enqueue("crc(ueId={},h_id={},value={})", crc.ue_index, crc.harq_id, crc.tb_crc_success);
          // TODO: Derive TB.
          int tbs = ue_cc.harqs.ul_harq(crc.harq_id).crc_info(crc.tb_crc_success);
          if (tbs < 0) {
            logger.warning("SCHED: ACK for h_id={} that is inactive", crc.harq_id);
          }
        });
  }
}

void ue_event_manager::handle_harq_ind(ue_cell& ue_cc, slot_point uci_sl, span<const bool> harq_bits)
{
  for (unsigned bit_idx = 0; bit_idx != harq_bits.size(); ++bit_idx) {
    int tbs = -1;
    for (unsigned h_id = 0; h_id != ue_cc.harqs.nof_dl_harqs(); ++h_id) {
      if (ue_cc.harqs.dl_harq(h_id).slot_ack(0) == uci_sl) {
        // TODO: Fetch the right HARQ id, TB, CBG.
        tbs = ue_cc.harqs.dl_harq(h_id).ack_info(0, harq_bits[bit_idx]);
        if (tbs > 0) {
          logger.debug("SCHED: ueid={}, dl_h_id={} with TB size={} bytes ACKed.", ue_cc.ue_index, h_id, tbs);
        }
        break;
      }
    }
    if (tbs < 0) {
      logger.warning("SCHED: DL HARQ for ueId={}, uci slot={} not found.", ue_cc.ue_index, uci_sl);
    }
  }
}

void ue_event_manager::handle_uci_indication(const uci_indication& ind)
{
  srsgnb_sanity_check(cell_exists(ind.cell_index), "Invalid cell index");

  // Process DL HARQ ACKs.
  for (unsigned i = 0; i != ind.ucis.size(); ++i) {
    if (not ind.ucis[i].harqs.empty()) {
      cell_specific_events[ind.cell_index].emplace(
          ind.ucis[i].ue_index,
          [this, uci_sl = ind.slot_rx, harq_bits = ind.ucis[i].harqs](ue_cell& ue_cc, event_logger& ev_logger) {
            ev_logger.enqueue("uci_harq(ueId={},{} harqs)", ue_cc.ue_index, harq_bits.size());
            handle_harq_ind(ue_cc, uci_sl, harq_bits);
          });
    }
  }

  // Process SRs.
  for (unsigned i = 0; i != ind.ucis.size(); ++i) {
    if (ind.ucis[i].sr_detected) {
      common_events.emplace(ind.ucis[i].ue_index, [ue_index = ind.ucis[i].ue_index, this](event_logger& ev_logger) {
        ev_logger.enqueue("sr_ind(ueId={})", ue_index);
        ue_db[ue_index].handle_sr_indication();
      });
    }
  }
}

void ue_event_manager::handle_dl_mac_ce_indication(const dl_mac_ce_indication& ce)
{
  common_events.emplace(ce.ue_index, [this, ce](event_logger& ev_logger) {
    ev_logger.enqueue("mac_ce(ueId={},ce={})", ce.ue_index, ce.ce_lcid);
    ue_db[ce.ue_index].handle_dl_mac_ce_indication(ce);
  });
}

void ue_event_manager::handle_dl_buffer_state_indication(const dl_buffer_state_indication_message& bs)
{
  common_events.emplace(bs.ue_index, [this, bs](event_logger& ev_logger) {
    ev_logger.enqueue("mac_bs(ueId={},lcid={},bs={})", bs.ue_index, bs.lcid, bs.bs);
    ue& u = ue_db[bs.ue_index];
    u.handle_dl_buffer_state_indication(bs);
    if (bs.lcid == LCID_SRB0) {
      // Signal SRB0 scheduler with the new SRB0 buffer state.
      du_cells[u.get_pcell().cell_index].srb0_sched->handle_dl_buffer_state_indication(bs.ue_index);
    }
  });
}

void ue_event_manager::process_common(slot_point sl, du_cell_index_t cell_index)
{
  event_logger ev_logger{MAX_NOF_DU_CELLS, logger};

  if (last_sl != sl) {
    // Pop pending common events.
    common_events.slot_indication();
    last_sl = sl;
  }

  // Process events for UEs whose PCell matches cell_index argument.
  span<common_event_t> events_to_process = common_events.get_events();
  for (common_event_t& ev : events_to_process) {
    if (ev.callback.is_empty()) {
      // Event already processed.
      continue;
    }
    if (ev.ue_index == MAX_NOF_DU_UES) {
      // The UE is being created.
      ev.callback(ev_logger);
      ev.callback = {};
    } else {
      if (not ue_db.contains(ev.ue_index)) {
        // Can't find UE. Log error.
        log_invalid_ue_index(ev.ue_index);
        ev.callback = {};
        continue;
      }
      if (ue_db[ev.ue_index].get_pcell().cell_index == cell_index) {
        // If we are currently processing PCell.
        ev.callback(ev_logger);
        ev.callback = {};
      }
    }
  }
}

void ue_event_manager::process_cell_specific(du_cell_index_t cell_index)
{
  event_logger ev_logger{cell_index, logger};

  // Pop and process pending cell-specific events.
  cell_specific_events[cell_index].slot_indication();
  auto events = cell_specific_events[cell_index].get_events();
  for (cell_event_t& ev : events) {
    if (not ue_db.contains(ev.ue_index)) {
      log_invalid_ue_index(ev.ue_index);
      continue;
    }
    ue&      ue    = ue_db[ev.ue_index];
    ue_cell* ue_cc = ue.find_cell(cell_index);
    if (ue_cc == nullptr) {
      log_invalid_cc(ev.ue_index, cell_index);
      continue;
    }
    ev.callback(*ue_cc, ev_logger);
  }
}

void ue_event_manager::run(slot_point sl, du_cell_index_t cell_index)
{
  srsgnb_sanity_check(cell_exists(cell_index), "Invalid cell index {}", cell_index);

  // Process common events.
  process_common(sl, cell_index);

  // Process carrier specific events.
  process_cell_specific(cell_index);
}

void ue_event_manager::add_cell(const cell_configuration& cell_cfg_, ue_srb0_scheduler& srb0_sched)
{
  srsgnb_assert(not cell_exists(cell_cfg_.cell_index), "Overwriting cell configurations not supported");

  du_cells[cell_cfg_.cell_index].cfg        = &cell_cfg_;
  du_cells[cell_cfg_.cell_index].srb0_sched = &srb0_sched;
}

bool ue_event_manager::cell_exists(du_cell_index_t cell_index) const
{
  return cell_index < MAX_NOF_DU_CELLS and du_cells[cell_index].cfg != nullptr;
}

void ue_event_manager::log_invalid_ue_index(du_ue_index_t ue_index) const
{
  logger.warning("SCHED: Event for ueId={} ignored. Cause: UE with provided ueId does not exist", ue_index);
}

void ue_event_manager::log_invalid_cc(du_ue_index_t ue_index, du_cell_index_t cell_index) const
{
  logger.warning("SCHED: Event for ueId={} ignored. Cause: Cell {} is not configured.", ue_index, cell_index);
}
