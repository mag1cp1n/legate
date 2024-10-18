/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <highfive/H5File.hpp>

#include <cstddef>
#include <mutex>
#include <string>

namespace legate::experimental::io::hdf5::detail {

// HDF5 isn't thread-safe so we use a global lock, which means the reads are serialized within
// each Legate rank. See
// <https://github.com/nv-legate/legate.io/pull/16#issuecomment-1740837700> Notice, in order to
// avoid deadlock, we only lock access to the HDF5 file API. This is because any task that
// blocks on the runtime will get removed from the processor (still holding the mutex), then
// while the runtime is servicing the call, another task can start running on the processor.
class HDF5GlobalLock {
  static inline std::mutex mut_{};

  std::lock_guard<std::mutex> guard_{mut_};
};

[[nodiscard]] HighFive::File open_hdf5_file(const HDF5GlobalLock&,
                                            const std::string& filepath,
                                            bool gds_on);

}  // namespace legate::experimental::io::hdf5::detail
