/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "libperfmgr"

#include "perfmgr/EventNode.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <utils/Trace.h>

namespace android {
namespace perfmgr {

EventNode::EventNode(
        std::string name, std::string node_path, std::vector<RequestGroup> req_sorted,
        std::size_t default_val_index, bool reset_on_init,
        std::function<void(const std::string &, const std::string &, const std::string &)>
                update_callback)
    : Node(std::move(name), std::move(node_path), std::move(req_sorted), default_val_index,
           reset_on_init),
      update_callback_(update_callback) {}

std::chrono::milliseconds EventNode::Update(bool) {
    std::size_t value_index = default_val_index_;
    std::chrono::milliseconds expire_time = std::chrono::milliseconds::max();

    // Find the highest outstanding request's expire time
    for (std::size_t i = 0; i < req_sorted_.size(); i++) {
        if (req_sorted_[i].GetExpireTime(&expire_time)) {
            value_index = i;
            break;
        }
    }

    // Update node only if request index changes
    if (value_index != current_val_index_ || reset_on_init_) {
        const std::string &req_value = req_sorted_[value_index].GetRequestValue();
        if (ATRACE_ENABLED()) {
            ATRACE_INT(("N:" + GetName()).c_str(), value_index);
            const std::string tag =
                    GetName() + ":" + req_value + ":" + std::to_string(expire_time.count());
            ATRACE_BEGIN(tag.c_str());
        }
        update_callback_(name_, node_path_, req_value);
        current_val_index_ = value_index;
        reset_on_init_ = false;
        if (ATRACE_ENABLED()) {
            ATRACE_END();
        }
    }
    return expire_time;
}

void EventNode::DumpToFd(int fd) const {
    const std::string &node_value = req_sorted_[current_val_index_].GetRequestValue();
    std::string buf(android::base::StringPrintf(
            "Node Name\t"
            "Event Path\t"
            "Current Index\t"
            "Current Value\n"
            "%s\t%s\t%zu\t%s\n",
            name_.c_str(), node_path_.c_str(), current_val_index_, node_value.c_str()));
    if (!android::base::WriteStringToFd(buf, fd)) {
        LOG(ERROR) << "Failed to dump fd: " << fd;
    }
    for (std::size_t i = 0; i < req_sorted_.size(); i++) {
        req_sorted_[i].DumpToFd(fd, android::base::StringPrintf("\t\tReq%zu:\t", i));
    }
}

}  // namespace perfmgr
}  // namespace android