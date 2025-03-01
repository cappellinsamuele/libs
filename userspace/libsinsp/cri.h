/*
Copyright (C) 2021 The Falco Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#pragma once

#include <memory>
#include <string>

#ifndef MINIMAL_BUILD
#include "cri.pb.h"
#include "cri.grpc.pb.h"
#endif // MINIMAL_BUILD

#include "container_info.h"

#ifdef GRPC_INCLUDE_IS_GRPCPP
#	include <grpcpp/grpcpp.h>
#else
#	include <grpc++/grpc++.h>
#endif

namespace libsinsp {
namespace cri {

// these shouldn't be globals but we still need references to *the* CRI runtime
extern std::vector<std::string> s_cri_unix_socket_paths;
extern int64_t s_cri_timeout;
// TODO: drop these 2 below
extern std::string s_cri_unix_socket_path;
extern sinsp_container_type s_cri_runtime_type;
extern bool s_cri_extra_queries;

class cri_interface
{
public:
	cri_interface(const std::string& cri_path);

	/**
	 * @brief did we manage to connect to CRI and get the runtime name/version?
	 * @return true if successfully connected to CRI
	 */
	bool is_ok() const
	{
		return m_cri != nullptr;
	}

	/**
	 * @brief get the detected CRI runtime type
	 * @return one of CT_CRIO, CT_CONTAINERD, CT_CRI (for other CRI runtimes)
	 * 	corresponding to the CRI runtime type detected
	 */
	sinsp_container_type get_cri_runtime_type() const;

	/**
	 * @brief thin wrapper around CRI gRPC ContainerStatus call
	 * @param container_id container ID
	 * @param resp reference to the response (if the RPC is successful, it will be filled out)
	 * @return status of the gRPC call
	 */
	grpc::Status get_container_status(const std::string& container_id, runtime::v1alpha2::ContainerStatusResponse& resp);

	/**
	 * @brief thin wrapper around CRI gRPC ContainerStats call
	 * @param container_id container ID
	 * @param resp reference to the response (if the RPC is successful, it will be filled out)
	 * @return status of the gRPC call
	 */
	grpc::Status get_container_stats(const std::string& container_id, runtime::v1alpha2::ContainerStatsResponse& resp);

	/**
	 * @brief fill out container image information based on CRI response
	 * @param status `status` field of the ContainerStatusResponse
	 * @param info `info` field of the ContainerStatusResponse
	 * @param container the container info to fill out
	 * @return true if successful
	 */
	bool parse_cri_image(const runtime::v1alpha2::ContainerStatus &status, const google::protobuf::Map<std::string, std::string> &info, sinsp_container_info &container);

	/**
	 * @brief fill out container mount information based on CRI response
	 * @param status `status` field of the ContainerStatusResponse
	 * @param container the container info to fill out
	 * @return true if successful
	 */
	bool parse_cri_mounts(const runtime::v1alpha2::ContainerStatus &status, sinsp_container_info &container);

	/**
	 * @brief fill out container environment variables based on CRI response
	 * @param info the `info` key of the `info` field of the ContainerStatusResponse
	 * @param container the container info to fill out
	 * @return true if successful
	 *
	 * Note: only containerd exposes this data
	 */
	bool parse_cri_env(const Json::Value &info, sinsp_container_info &container);

	/**
	 * @brief fill out extra image info based on CRI response
	 * @param info the `info` key of the `info` field of the ContainerStatusResponse
	 * @param container the container info to fill out
	 * @return true if successful
	 *
	 * Note: only containerd exposes this data
	 */
	bool parse_cri_json_image(const Json::Value &info, sinsp_container_info &container);

	/**
	 * @brief fill out extra container info (e.g. resource limits) based on CRI response
	 * @param info the `info` key of the `info` field of the ContainerStatusResponse
	 * @param container the container info to fill out
	 * @return true if successful
	 */
	bool parse_cri_ext_container_info(const Json::Value &info, sinsp_container_info &container);

	/**
	 * @brief fill out extra container user info (e.g. configured uid) based on CRI response
	 * @param info the `info` key of the `info` field of the ContainerStatusResponse
	 * @param container the container info to fill out
	 * @return true if successful
	 *
	 * Note: only containerd exposes this data
	 */
	bool parse_cri_user_info(const Json::Value &info, sinsp_container_info &container);

	/**
	 * @brief check if the passed container ID is a pod sandbox (pause container)
	 * @param container_id the container ID to check
	 * @return true if it's a pod sandbox
	 */
	bool is_pod_sandbox(const std::string &container_id);

	/**
	 * @brief get pod IP address
	 * @param resp initialized runtime::v1alpha2::PodSandboxStatusResponse of the pod sandbox
	 * @return the IP address if possible, 0 otherwise (e.g. when the pod uses host netns)
	 */
	uint32_t get_pod_sandbox_ip(runtime::v1alpha2::PodSandboxStatusResponse &resp);

	/**
	 * @brief get unparsed JSON string with cni result of the pod sandbox from PodSandboxStatusResponse info() field.
	 * @param resp initialized runtime::v1alpha2::PodSandboxStatusResponse of the pod sandbox
	 * @param cniresult initialized cniresult
	 */
	void get_pod_info_cniresult(runtime::v1alpha2::PodSandboxStatusResponse &resp, std::string &cniresult);

	/**
	 * @brief make request and get PodSandboxStatusResponse and grpc::Status.
	 * @param pod_sandbox_id ID of the pod sandbox
	 * @param resp initialized runtime::v1alpha2::PodSandboxStatusResponse of the pod sandbox
	 * @param status initialized grpc::Status
	 */
	void get_pod_sandbox_resp(const std::string &pod_sandbox_id, runtime::v1alpha2::PodSandboxStatusResponse &resp, grpc::Status &status);

	/**
	 * @brief get container IP address if possible, 0 otherwise (e.g. when the pod uses host netns),
	 *  get unparsed JSON string with cni result of the pod sandbox from PodSandboxStatusResponse info() field.
	 * @param container_id the container ID
	 * @param container_ip initialized container_ip
	 * @param cniresult initialized cniresult
	 *
	 * This method first finds the pod ID, then gets the IP address and also checks for cni result
	 * of the pod sandbox container
	 */
	void get_container_ip(const std::string &container_id, uint32_t &container_ip, std::string &cniresult);

	/**
	 * @brief get image id info from CRI
	 * @param image_ref the image ref from container metadata
	 * @return image id if found, empty string otherwise
	 */
	std::string get_container_image_id(const std::string &image_ref);

private:

	std::unique_ptr<runtime::v1alpha2::RuntimeService::Stub> m_cri;
	std::unique_ptr<runtime::v1alpha2::ImageService::Stub> m_cri_image;
	sinsp_container_type m_cri_runtime_type;
};

}
}
