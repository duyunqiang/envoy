licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_package",
)

envoy_package()

envoy_cc_extension(
    name = "cluster",
    srcs = ["cluster.cc"],
    hdrs = ["cluster.h"],
    security_posture = "robust_to_untrusted_downstream",
    deps = [
        "//source/common/network:transport_socket_options_lib",
        "//source/common/upstream:cluster_factory_lib",
        "//source/common/upstream:logical_host_lib",
        "//source/extensions/clusters:well_known_names",
        "//source/extensions/common/dynamic_forward_proxy:dns_cache_interface",
        "//source/extensions/common/dynamic_forward_proxy:dns_cache_manager_impl",
        "@envoy_api//envoy/api/v2:pkg_cc_proto",
        "@envoy_api//envoy/api/v2/endpoint:pkg_cc_proto",
        "@envoy_api//envoy/config/cluster/dynamic_forward_proxy/v2alpha:pkg_cc_proto",
    ],
)
