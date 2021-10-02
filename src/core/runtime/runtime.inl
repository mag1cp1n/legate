/* Copyright 2021 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

namespace legate {

template <typename T>
T Runtime::get_tunable(const LibraryContext* context, int64_t tunable_id, int64_t mapper_id /*= 0*/)
{
  Legion::TunableLauncher launcher(tunable_id, context->get_mapper_id(mapper_id), 0, sizeof(T));
  auto future = legion_runtime_->select_tunable_value(legion_context_, launcher);
  return future.get_result<T>();
}

}  // namespace legate
