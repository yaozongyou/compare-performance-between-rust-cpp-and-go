load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "greeting",
    srcs = [
       "main.cpp",
        "acceptor.h",
        "acceptor.cpp",
        "session.h",
        "session.cpp",
        "network_thread.h",
        "network_thread.cpp",
    ],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_google_absl//absl/strings",
        "@boost//:asio",
        "@boost//:beast",
        "@com_github_google_glog//:glog",
    ],
    features = [
#        "fully_static_link",
    ],
)
