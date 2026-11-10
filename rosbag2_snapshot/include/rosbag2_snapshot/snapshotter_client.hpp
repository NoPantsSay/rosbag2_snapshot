// Copyright (c) 2018-2021, Open Source Robotics Foundation, Inc., GAIA Platform, Inc., All rights reserved.  // NOLINT
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the {copyright_holder} nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef ROSBAG2_SNAPSHOT__SNAPSHOTTER_CLIENT_HPP_
#define ROSBAG2_SNAPSHOT__SNAPSHOTTER_CLIENT_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rosbag2_snapshot/snapshotter.hpp>

#include <string>
#include <vector>

namespace rosbag2_snapshot
{

// Configuration for SnapshotterClient
struct SnapshotterClientOptions
{
  SnapshotterClientOptions();
  enum Action
  {
    TRIGGER_WRITE,
    PAUSE,
    RESUME
  };
  // What to do when SnapshotterClient.run is called
  Action action_;
  // List of topics to write when action_ == TRIGGER_WRITE.
  // If empty, write all buffered topics.
  std::vector<TopicDetails> topics_;
  // Name of file to write to when action_ == TRIGGER_WRITE, relative to snapshot node.
  std::string filename_;
};

// Node used to call services which interface with the snapshotter node to trigger
// write, pause, and resume
class SnapshotterClient : public rclcpp::Node
{
public:
  explicit SnapshotterClient(const rclcpp::NodeOptions & options);

private:
  void setSnapshotterClientOptions(SnapshotterClientOptions const & opts);
};

}  // namespace rosbag2_snapshot

#endif  // ROSBAG2_SNAPSHOT__SNAPSHOTTER_CLIENT_HPP_