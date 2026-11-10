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

#include <rclcpp/rclcpp.hpp>
#include <rosbag2_snapshot/snapshotter_client.hpp>
#include <rosbag2_snapshot_msgs/srv/trigger_snapshot.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include <filesystem>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace rosbag2_snapshot
{

using rosbag2_snapshot_msgs::srv::TriggerSnapshot;
using std::shared_ptr;
using std::string;
using std_srvs::srv::SetBool;

SnapshotterClientOptions::SnapshotterClientOptions()
: action_(SnapshotterClientOptions::TRIGGER_WRITE)
{
}

SnapshotterClient::SnapshotterClient(const rclcpp::NodeOptions & options)
: rclcpp::Node("snapshotter_client", options)
{
  std::string action_str{};

  SnapshotterClientOptions opts{};

  try {
    action_str = declare_parameter<std::string>("action_type");
  } catch (const rclcpp::ParameterTypeException & ex) {
    RCLCPP_ERROR(get_logger(), "action_type parameter is missing or of incorrect type.");
    throw ex;
  }

  if (action_str == "trigger_write") {
    opts.action_ = SnapshotterClientOptions::TRIGGER_WRITE;
  } else if (action_str == "resume") {
    opts.action_ = SnapshotterClientOptions::RESUME;
  } else if (action_str == "pause") {
    opts.action_ = SnapshotterClientOptions::PAUSE;
  } else {
    RCLCPP_ERROR(get_logger(), "action_type must be one of: trigger_write, resume, or pause");
    throw std::invalid_argument{"Invalid value for action_type parameter."};
  }

  std::vector<std::string> topic_names{};

  try {
    topic_names = declare_parameter<std::vector<std::string>>("topics", std::vector<std::string>{});
  } catch (const rclcpp::ParameterTypeException & ex) {
    if (std::string{ex.what()}.find("not set") == std::string::npos) {
      RCLCPP_ERROR(get_logger(), "topics must be an array of strings.");
      throw ex;
    }
  }

  if (topic_names.size() > 0) {
    for (const auto & topic : topic_names) {
      std::string prefix = "topic_details." + topic;
      std::string topic_type{};

      try {
        topic_type = declare_parameter<std::string>(prefix + ".type");
      } catch (const rclcpp::ParameterTypeException & ex) {
        if (std::string{ex.what()}.find("not set") == std::string::npos) {
          RCLCPP_ERROR(get_logger(), "Topic type must be a string.");
        } else {
          RCLCPP_ERROR(get_logger(), "Topic %s is missing a type.", topic.c_str());
        }

        throw ex;
      }

      TopicDetails details{};
      details.name = topic;
      details.type = topic_type;
      opts.topics_.push_back(details);
    }
  }

  try {
    opts.filename_ = declare_parameter<std::string>("filename", "");
  } catch (const rclcpp::ParameterTypeException & ex) {
    if (opts.action_ == SnapshotterClientOptions::TRIGGER_WRITE &&
      std::string{ex.what()}.find("not set") == std::string::npos)
    {
      RCLCPP_ERROR(get_logger(), "filename must be a string.");
      throw ex;
    }
  }

  if (opts.action_ == SnapshotterClientOptions::TRIGGER_WRITE && opts.topics_.size() == 0) {
    RCLCPP_INFO(get_logger(), "No topics provided - logging all topics.");
    RCLCPP_WARN(get_logger(), "Logging all topics is very memory-intensive.");
  }

  setSnapshotterClientOptions(opts);
  rclcpp::shutdown();
}

void SnapshotterClient::setSnapshotterClientOptions(const SnapshotterClientOptions & opts)
{
  if (opts.action_ == SnapshotterClientOptions::TRIGGER_WRITE) {
    auto client = create_client<TriggerSnapshot>("trigger_snapshot");
    if (!client->wait_for_service(std::chrono::seconds(5))) {
      throw std::runtime_error{
              "Service trigger_snapshot is not ready. "
              "Is snapshot running in this namespace?"
      };
    }

    auto req = std::make_shared<TriggerSnapshot::Request>();

    for (const auto & topic : opts.topics_) {
      req->topics.push_back(topic.asMessage());
    }

    // Use filename as-is; if empty, server will use timestamp
    req->filename = opts.filename_;

    // Resolve filename relative to clients working directory to avoid confusion
    if (!req->filename.empty()) {
      std::filesystem::path p(std::filesystem::absolute(req->filename));
      req->filename = p.string();
    }

    auto result_future = client->async_send_request(req);
    auto future_result =
      rclcpp::spin_until_future_complete(this->get_node_base_interface(), result_future);

    if (future_result != rclcpp::FutureReturnCode::SUCCESS) {
      RCLCPP_ERROR(get_logger(), "Calling the service failed.");
    } else {
      auto result = result_future.get();
      RCLCPP_INFO(
        get_logger(),
        "Service returned: [%s] %s",
        (result->success ? "SUCCESS" : "FAILURE"),
        result->message.c_str()
      );
    }

    return;
  } else if (  // NOLINT
    opts.action_ == SnapshotterClientOptions::PAUSE ||
    opts.action_ == SnapshotterClientOptions::RESUME)
  {
    auto client = create_client<SetBool>("enable_snapshot");
    if (!client->wait_for_service(std::chrono::seconds(5))) {
      throw std::runtime_error{
              "Service enable_snapshot does not exist. "
              "Is snapshot running in this namespace?"
      };
    }

    SetBool::Request::SharedPtr req;
    req->data = (opts.action_ == SnapshotterClientOptions::RESUME);

    auto result_future = client->async_send_request(req);
    auto future_result =
      rclcpp::spin_until_future_complete(this->get_node_base_interface(), result_future);

    if (future_result != rclcpp::FutureReturnCode::SUCCESS) {
      RCLCPP_ERROR(get_logger(), "Calling the service failed.");
    } else {
      auto result = result_future.get();
      RCLCPP_INFO(
        get_logger(),
        "Service returned: [%s] %s",
        (result->success ? "SUCCESS" : "FAILURE"),
        result->message.c_str()
      );
    }

    return;
  } else {
    throw std::runtime_error{"Invalid options received."};
  }
}

}  // namespace rosbag2_snapshot

#include <rclcpp_components/register_node_macro.hpp>  // NOLINT
RCLCPP_COMPONENTS_REGISTER_NODE(rosbag2_snapshot::SnapshotterClient)