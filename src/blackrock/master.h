// Sandstorm Blackrock
// Copyright (c) 2015 Sandstorm Development Group, Inc.
// All Rights Reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BLACKROCK_MASTER_H_
#define BLACKROCK_MASTER_H_

#include "common.h"
#include "cluster-rpc.h"
#include <kj/async-io.h>
#include <blackrock/master.capnp.h>
#include <map>
#include "logs.h"

namespace sandstorm {
  class SubprocessSet;
}

namespace blackrock {

class ComputeDriver {
public:
  enum class MachineType {
    STORAGE,
    WORKER,
    COORDINATOR,
    FRONTEND,
    MONGO,
    GATEWAY
  };

  struct MachineId {
    MachineType type;
    uint index;

    inline bool operator==(const MachineId& other) const {
      return type == other.type && index == other.index;
    }
    inline bool operator<(const MachineId& other) const {
      return type < other.type ? true :
             type > other.type ? false :
             index < other.index;
    }

    kj::String toString() const;
    // Makes reasonable hostnames. E.g. { STORAGE, 123 } becomes "storage123".

    MachineId() = default;
    inline MachineId(MachineType type, uint index): type(type), index(index) {}
    MachineId(kj::StringPtr name);
    // Parses results of toString().
  };

  struct MachineStatus {
    MachineId id;
    kj::Maybe<VatPath::Reader> path;
    // Current path, or null if not powered up. Path remains valid until halt() or destroy() is
    // called on the machine.
  };

  virtual SimpleAddress getMasterBindAddress() = 0;
  // Get the address at which other machines in the cluster will see the master (i.e. this)
  // machine.

  virtual kj::Promise<kj::Array<MachineId>> listMachines() KJ_WARN_UNUSED_RESULT = 0;
  // List all machines currently running in the cluster.

  virtual kj::Promise<void> boot(MachineId id) = 0;
  // Boot the given machine.

  virtual kj::Promise<VatPath::Reader> run(MachineId id,
      VatId::Reader masterVatId, bool requireRestartProcess) KJ_WARN_UNUSED_RESULT = 0;
  // Run the Blackrock process on the given machine. If `requireRestartProcess` is true,
  // then all blackrock processes on the machine should be immediately terminated and restarted.
  // Depending on the driver, this may or may not have the effect of updating the binary to the
  // latest version. Note that `requireRestartProcess` is often much faster than stop() followed
  // by boot() and run(), but not as reliable.

  virtual kj::Promise<void> stop(MachineId id) KJ_WARN_UNUSED_RESULT = 0;
  // Shut down the given machine.
};

void runMaster(kj::AsyncIoContext& ioContext, ComputeDriver& driver, MasterConfig::Reader config,
               bool shouldRestart, kj::ArrayPtr<kj::StringPtr> machinesToRestart);

class VagrantDriver: public ComputeDriver {
public:
  VagrantDriver(sandstorm::SubprocessSet& subprocessSet, kj::LowLevelAsyncIoProvider& ioProvider);
  ~VagrantDriver() noexcept(false);

  SimpleAddress getMasterBindAddress() override;
  kj::Promise<kj::Array<MachineId>> listMachines() override;
  kj::Promise<void> boot(MachineId id) override;
  kj::Promise<VatPath::Reader> run(MachineId id, VatId::Reader masterVatId,
                                   bool requireRestartProcess) override;
  kj::Promise<void> stop(MachineId id) override;

private:
  sandstorm::SubprocessSet& subprocessSet;
  kj::LowLevelAsyncIoProvider& ioProvider;
  std::map<ComputeDriver::MachineId, kj::Own<capnp::MessageReader>> vatPaths;
  SimpleAddress masterBindAddress;

  LogSink logSink;
  kj::Promise<void> logTask;
  SimpleAddress logSinkAddress;

  kj::Promise<void> bootQueue = kj::READY_NOW;
};

} // namespace blackrock

#endif // BLACKROCK_MASTER_H_
