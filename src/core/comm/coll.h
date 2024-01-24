/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "core/utilities/typedefs.h"

#include "legate_defines.h"

#include <cstdbool>
#include <cstddef>
#include <vector>

#if LegateDefined(LEGATE_USE_NETWORK)
#include <mpi.h>
#endif

// If we aren't building with networking, we'll use pthread_barrier to
// construct a communicator for thread-local communication. Mac OS
// does not implement pthread barriers, so we need to include an
// implementation in case they are not defined. We also need to
// include unistd.h since that defines _POSIX_BARRIERS.
#include <pthread.h>
#include <unistd.h>
#if !defined(_POSIX_BARRIERS) || (_POSIX_BARRIERS < 0)
#include "core/comm/pthread_barrier.h"
#endif

namespace legate::comm::coll {

namespace detail {

[[nodiscard]] Logger& log_coll();

}  // namespace detail

#if LegateDefined(LEGATE_USE_NETWORK)
struct RankMappingTable {
  int* mpi_rank{};
  int* global_rank{};
};
#endif

struct ThreadComm {
  pthread_barrier_t barrier;
  bool ready_flag{};
  const void** buffers{};
  const int** displs{};
};

enum class CollDataType : int {
  CollInt8   = 0,
  CollChar   = 1,
  CollUint8  = 2,
  CollInt    = 3,
  CollUint32 = 4,
  CollInt64  = 5,
  CollUint64 = 6,
  CollFloat  = 7,
  CollDouble = 8,
};

enum CollStatus : int {
  CollSuccess = 0,
  CollError   = 1,
};

enum CollCommType : int {
  CollMPI   = 0,
  CollLocal = 1,
};

struct Coll_Comm {
#if LegateDefined(LEGATE_USE_NETWORK)
  MPI_Comm mpi_comm{};
  RankMappingTable mapping_table{};
#endif
  volatile ThreadComm* local_comm{};
  int mpi_rank{};
  int mpi_comm_size{};
  int mpi_comm_size_actual{};
  int global_rank{};
  int global_comm_size{};
  int nb_threads{};
  int unique_id{};
  bool status{};
};

using CollComm = Coll_Comm*;

class BackendNetwork {
 public:
  BackendNetwork()          = default;
  virtual ~BackendNetwork() = default;

  [[nodiscard]] virtual int init_comm() = 0;

  virtual void abort();

  [[nodiscard]] virtual int comm_create(CollComm global_comm,
                                        int global_comm_size,
                                        int global_rank,
                                        int unique_id,
                                        const int* mapping_table) = 0;

  [[nodiscard]] virtual int comm_destroy(CollComm global_comm) = 0;

  [[nodiscard]] virtual int alltoallv(const void* sendbuf,
                                      const int sendcounts[],
                                      const int sdispls[],
                                      void* recvbuf,
                                      const int recvcounts[],
                                      const int rdispls[],
                                      CollDataType type,
                                      CollComm global_comm) = 0;

  [[nodiscard]] virtual int alltoall(
    const void* sendbuf, void* recvbuf, int count, CollDataType type, CollComm global_comm) = 0;

  [[nodiscard]] virtual int allgather(
    const void* sendbuf, void* recvbuf, int count, CollDataType type, CollComm global_comm) = 0;

 protected:
  [[nodiscard]] int collGetUniqueId(int* id);

  [[nodiscard]] static void* allocateInplaceBuffer(const void* recvbuf, size_t size);

 public:
  CollCommType comm_type;

 protected:
  bool coll_inited{};
  int current_unique_id{};
};

#if LegateDefined(LEGATE_USE_NETWORK)
class MPINetwork : public BackendNetwork {
 public:
  MPINetwork(int argc, char* argv[]);

  ~MPINetwork() override;

  [[nodiscard]] int init_comm() override;

  void abort() override;

  [[nodiscard]] int comm_create(CollComm global_comm,
                                int global_comm_size,
                                int global_rank,
                                int unique_id,
                                const int* mapping_table) override;

  [[nodiscard]] int comm_destroy(CollComm global_comm) override;

  [[nodiscard]] int alltoallv(const void* sendbuf,
                              const int sendcounts[],
                              const int sdispls[],
                              void* recvbuf,
                              const int recvcounts[],
                              const int rdispls[],
                              CollDataType type,
                              CollComm global_comm) override;

  [[nodiscard]] int alltoall(const void* sendbuf,
                             void* recvbuf,
                             int count,
                             CollDataType type,
                             CollComm global_comm) override;

  [[nodiscard]] int allgather(const void* sendbuf,
                              void* recvbuf,
                              int count,
                              CollDataType type,
                              CollComm global_comm) override;

 protected:
  [[nodiscard]] int gather(const void* sendbuf,
                           void* recvbuf,
                           int count,
                           CollDataType type,
                           int root,
                           CollComm global_comm);

  [[nodiscard]] int bcast(void* buf, int count, CollDataType type, int root, CollComm global_comm);

  [[nodiscard]] static MPI_Datatype dtypeToMPIDtype(CollDataType dtype);

  [[nodiscard]] int generateAlltoallTag(int rank1, int rank2, CollComm global_comm) const;

  [[nodiscard]] int generateAlltoallvTag(int rank1, int rank2, CollComm global_comm) const;

  [[nodiscard]] int generateBcastTag(int rank, CollComm global_comm) const;

  [[nodiscard]] int generateGatherTag(int rank, CollComm global_comm) const;

 private:
  int mpi_tag_ub{};
  bool self_init_mpi{};
  std::vector<MPI_Comm> mpi_comms{};
};
#endif

class LocalNetwork : public BackendNetwork {
 public:
  LocalNetwork(int argc, char* argv[]);

  ~LocalNetwork() override;

  [[nodiscard]] int init_comm() override;

  [[nodiscard]] int comm_create(CollComm global_comm,
                                int global_comm_size,
                                int global_rank,
                                int unique_id,
                                const int* mapping_table) override;

  [[nodiscard]] int comm_destroy(CollComm global_comm) override;

  [[nodiscard]] int alltoallv(const void* sendbuf,
                              const int sendcounts[],
                              const int sdispls[],
                              void* recvbuf,
                              const int recvcounts[],
                              const int rdispls[],
                              CollDataType type,
                              CollComm global_comm) override;

  [[nodiscard]] int alltoall(const void* sendbuf,
                             void* recvbuf,
                             int count,
                             CollDataType type,
                             CollComm global_comm) override;

  [[nodiscard]] int allgather(const void* sendbuf,
                              void* recvbuf,
                              int count,
                              CollDataType type,
                              CollComm global_comm) override;

 protected:
  [[nodiscard]] static size_t getDtypeSize(CollDataType dtype);

  void resetLocalBuffer(CollComm global_comm);

  void barrierLocal(CollComm global_comm);

 private:
  std::vector<ThreadComm*> thread_comms{};
};

extern BackendNetwork* backend_network;

[[nodiscard]] int collCommCreate(CollComm global_comm,
                                 int global_comm_size,
                                 int global_rank,
                                 int unique_id,
                                 const int* mapping_table);

[[nodiscard]] int collCommDestroy(CollComm global_comm);

[[nodiscard]] int collAlltoallv(const void* sendbuf,
                                const int sendcounts[],
                                const int sdispls[],
                                void* recvbuf,
                                const int recvcounts[],
                                const int rdispls[],
                                CollDataType type,
                                CollComm global_comm);

[[nodiscard]] int collAlltoall(
  const void* sendbuf, void* recvbuf, int count, CollDataType type, CollComm global_comm);

[[nodiscard]] int collAllgather(
  const void* sendbuf, void* recvbuf, int count, CollDataType type, CollComm global_comm);

[[nodiscard]] int collInit(int argc, char* argv[]);

[[nodiscard]] int collFinalize();

// this is forward declared in legate_defines.h (for LEGATE_ABORT()), because we don't want to
// include this entire header
void collAbort() noexcept;  // NOLINT(readability-redundant-declaration)

[[nodiscard]] int collInitComm();

}  // namespace legate::comm::coll
