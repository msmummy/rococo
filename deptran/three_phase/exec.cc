
#include "../config.h"
#include "../multi_value.h"
#include "../rcc/dep_graph.h"
#include "../rcc/graph_marshaler.h"
#include "exec.h"
#include "sched.h"

namespace rococo {

int ThreePhaseExecutor::start_launch(
    const RequestHeader &header,
    const std::vector<mdb::Value> &input,
    const rrr::i32 &output_size,
    rrr::i32 *res,
    std::vector<mdb::Value> *output,
    rrr::DeferredReply *defer) {
  verify(0);
}


int ThreePhaseExecutor::prepare_launch(
    const std::vector <i32> &sids,
    rrr::i32 *res,
    rrr::DeferredReply *defer
) {
  if (Config::GetConfig()->do_logging()) {
    string log_s;
    sched_->get_prepare_log(cmd_id_, sids, &log_s);
    *res = this->prepare();

    if (*res == SUCCESS)
      recorder_->submit(log_s, [defer]() { defer->reply(); });
    else
      defer->reply();
  } else {
    *res = this->prepare();
    defer->reply();
  }
  return 0;
}

int ThreePhaseExecutor::prepare() {
  verify(0);
}

int ThreePhaseExecutor::abort_launch(
    rrr::i32 *res,
    rrr::DeferredReply *defer
) {
  *res = this->abort();
  if (Config::GetConfig()->do_logging()) {
    const char abort_tag = 'a';
    std::string log_s;
    log_s.resize(sizeof(cmd_id_) + sizeof(abort_tag));
    memcpy((void *) log_s.data(), (void *) &cmd_id_, sizeof(cmd_id_));
    memcpy((void *) log_s.data(), (void *) &abort_tag, sizeof(abort_tag));
    recorder_->submit(log_s);
  }
  // TODO optimize
//  sched_->Destroy(cmd_id_);
  defer->reply();
  Log::debug("abort finish");
  return 0;
}

int ThreePhaseExecutor::abort() {
  verify(mdb_txn_ != NULL);
//  verify(mdb_txn_ == sched_->del_mdb_txn(cmd_id_));
  // TODO fix, might have double delete here.
  mdb_txn_->abort();
//  delete mdb_txn_;
  return SUCCESS;
}

int ThreePhaseExecutor::commit_launch(
    rrr::i32 *res,
    rrr::DeferredReply *defer
) {
  *res = this->commit();
  if (Config::GetConfig()->do_logging()) {
    const char commit_tag = 'c';
    std::string log_s;
    log_s.resize(sizeof(cmd_id_) + sizeof(commit_tag));
    memcpy((void *) log_s.data(), (void *) &cmd_id_, sizeof(cmd_id_));
    memcpy((void *) log_s.data(), (void *) &commit_tag, sizeof(commit_tag));
    recorder_->submit(log_s);
  }
//  sched_->Destroy(cmd_id_);
  defer->reply();
  return 0;
}

int ThreePhaseExecutor::commit() {
  verify(0);
}

void ThreePhaseExecutor::execute(
    const RequestHeader &header,
    const Value *input,
    rrr::i32 input_size,
    rrr::i32 *res,
    mdb::Value *output,
    rrr::i32 *output_size
) {
  txn_reg_->get(header).txn_handler(
      this, dtxn_, header, input, input_size,
      res, output, output_size, NULL);
}

void ThreePhaseExecutor::execute(
    const RequestHeader &header,
    const std::vector <mdb::Value> &input,
    rrr::i32 *res,
    std::vector <mdb::Value> *output
) {
  rrr::i32 output_size = output->size();
  txn_reg_->get(header).txn_handler(
      this, dtxn_, header, input.data(), input.size(),
      res, output->data(), &output_size, NULL);
  output->resize(output_size);
}

} // namespace rococo;