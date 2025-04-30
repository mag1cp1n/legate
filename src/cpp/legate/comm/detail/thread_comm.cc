/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <legate/comm/detail/thread_comm.h>

#include <legate/comm/detail/pthread_barrier.h>
#include <legate/utilities/assert.h>

namespace legate::detail::comm::coll {

void ThreadComm::init(std::int32_t global_comm_size)
{
  LEGATE_CHECK(global_comm_size > 0);
  CHECK_PTHREAD_CALL_V(
    pthread_barrier_init(&barrier_, nullptr, static_cast<unsigned int>(global_comm_size)));
  buffers_ = std::make_unique<atomic_buffer_type[]>(static_cast<std::size_t>(global_comm_size));
  displs_  = std::make_unique<atomic_displ_type[]>(static_cast<std::size_t>(global_comm_size));
  entered_finalize_ = 0;
  ready_flag_       = true;
}

void ThreadComm::finalize(std::int32_t global_comm_size, bool is_finalizer)
{
  ++entered_finalize_;
  if (is_finalizer) {
    // Need to ensure that all other threads have left the barrier before we can destroy the
    // thread_comm.
    while (entered_finalize_ != global_comm_size) {}
    entered_finalize_ = 0;
    clear();
  } else {
    // The remaining threads are not allowed to leave until the finalizer thread has finished
    // its work.
    while (ready()) {}
  }
}

void ThreadComm::clear() noexcept
{
  CHECK_PTHREAD_CALL_V(pthread_barrier_destroy(&barrier_));
  buffers_.reset();
  displs_.reset();
  ready_flag_ = false;
}

void ThreadComm::barrier_local()
{
  if (const auto ret = pthread_barrier_wait(&barrier_)) {
    if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
      return;
    }
    CHECK_PTHREAD_CALL_V(ret);
  }
}

}  // namespace legate::detail::comm::coll
