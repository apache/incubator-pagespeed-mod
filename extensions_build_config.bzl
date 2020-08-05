# See bazel/README.md for details on how this system works.
EXTENSIONS = {
    "envoy.filters.http.router": "//source/extensions/filters/http/router:config",
    "envoy.filters.network.direct_response": "//source/extensions/filters/network/direct_response:config",
    "envoy.filters.network.http_connection_manager": "//source/extensions/filters/network/http_connection_manager:config",
    "envoy.transport_sockets.raw_buffer": "//source/extensions/transport_sockets/raw_buffer:config",
}

# This can be used to extend the visibility rules for Envoy extensions
# (//:extension_config and //:extension_library in //BUILD)
# if downstream Envoy builds need to directly reference envoy extensions.
ADDITIONAL_VISIBILITY = []
