# gyp file for grpc, originally based on binding.gyp from the git repo.
{
  'variables': {
   'grpc_gcov%': 'false',
  },
  'target_defaults': {
    'configurations': {
      'Release': {
        'cflags': [
          '-O2',
          '-Wframe-larger-than=16384',
        ],
        'defines': [
          'NDEBUG',
        ],
      },
      'Debug': {
        'cflags': [
          '-O0',
        ],
        'defines': [
          '_DEBUG',
          'DEBUG',
        ],
      },
    },
    'cflags': [
      '-g',
      '-Wall',
      '-Wextra',
      '-Werror',
      '-Wno-long-long',
      '-Wno-unused-parameter',
      '-DOSATOMIC_USE_INLINED=1',
      '-Wno-deprecated-declarations',
      '-Ithird_party/nanopb',
      '-DPB_FIELD_32BIT',
    ],
    'ldflags': [
      '-g',
    ],
    'cflags_c': [
      '-Werror',
      '-std=c99',
    ],
    'cflags_cc': [
      '-Werror',
      '-std=c++11',
    ],
    'include_dirs': [
      'src',
      'src/include',
      'src/third_party/nanopb',
    ],
    'all_dependent_settings': {
      'include_dirs': [ 'src/include', ],
    },
    'defines': [
      'GRPC_ARES=0',
    ],
    'conditions': [
      ['grpc_gcov=="true"', {
        'cflags': [
          '-O0',
          '-fprofile-arcs',
          '-ftest-coverage',
          '-Wno-return-type',
        ],
        'defines': [
          '_DEBUG',
          'DEBUG',
          'GPR_GCOV',
        ],
        'ldflags': [
          '-fprofile-arcs',
          '-ftest-coverage',
          '-rdynamic',
          '-lstdc++',
        ],
      }],
      ['OS == "win"', {
        'defines': [
          '_WIN32_WINNT=0x0600',
          'WIN32_LEAN_AND_MEAN',
          '_HAS_EXCEPTIONS=0',
          'UNICODE',
          '_UNICODE',
          'NOMINMAX',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
          }
        },
        "libraries": [
          "ws2_32"
        ]
      }],
      ['OS == "mac"', {
        'xcode_settings': {
          'OTHER_CFLAGS': [
            '-g',
            '-Wall',
            '-Wextra',
            '-Werror',
            '-Wno-long-long',
            '-Wno-unused-parameter',
            '-DOSATOMIC_USE_INLINED=1',
            '-Wno-deprecated-declarations',
            '-Ithird_party/nanopb',
            '-DPB_FIELD_32BIT',
          ],
          'OTHER_CPLUSPLUSFLAGS': [
            '-g',
            '-Wall',
            '-Wextra',
            '-Werror',
            '-Wno-long-long',
            '-Wno-unused-parameter',
            '-DOSATOMIC_USE_INLINED=1',
            '-Wno-deprecated-declarations',
            '-Ithird_party/nanopb',
            '-DPB_FIELD_32BIT',
            '-stdlib=libc++',
            '-std=c++11',
            '-Wno-error=deprecated-declarations',
          ],
        },
      }]
    ]
  },
  'targets': [
      {
      'target_name': 'gpr',
      'type': 'static_library',
      'dependencies': [
      ],
      'sources': [
        'src/src/core/lib/gpr/alloc.cc',
        'src/src/core/lib/gpr/arena.cc',
        'src/src/core/lib/gpr/atm.cc',
        'src/src/core/lib/gpr/cpu_iphone.cc',
        'src/src/core/lib/gpr/cpu_linux.cc',
        'src/src/core/lib/gpr/cpu_posix.cc',
        'src/src/core/lib/gpr/cpu_windows.cc',
        'src/src/core/lib/gpr/env_linux.cc',
        'src/src/core/lib/gpr/env_posix.cc',
        'src/src/core/lib/gpr/env_windows.cc',
        'src/src/core/lib/gpr/host_port.cc',
        'src/src/core/lib/gpr/log.cc',
        'src/src/core/lib/gpr/log_android.cc',
        'src/src/core/lib/gpr/log_linux.cc',
        'src/src/core/lib/gpr/log_posix.cc',
        'src/src/core/lib/gpr/log_windows.cc',
        'src/src/core/lib/gpr/mpscq.cc',
        'src/src/core/lib/gpr/murmur_hash.cc',
        'src/src/core/lib/gpr/string.cc',
        'src/src/core/lib/gpr/string_posix.cc',
        'src/src/core/lib/gpr/string_util_windows.cc',
        'src/src/core/lib/gpr/string_windows.cc',
        'src/src/core/lib/gpr/sync.cc',
        'src/src/core/lib/gpr/sync_posix.cc',
        'src/src/core/lib/gpr/sync_windows.cc',
        'src/src/core/lib/gpr/time.cc',
        'src/src/core/lib/gpr/time_posix.cc',
        'src/src/core/lib/gpr/time_precise.cc',
        'src/src/core/lib/gpr/time_windows.cc',
        'src/src/core/lib/gpr/tls_pthread.cc',
        'src/src/core/lib/gpr/tmpfile_msys.cc',
        'src/src/core/lib/gpr/tmpfile_posix.cc',
        'src/src/core/lib/gpr/tmpfile_windows.cc',
        'src/src/core/lib/gpr/wrap_memcpy.cc',
        'src/src/core/lib/gprpp/fork.cc',
        'src/src/core/lib/gprpp/thd_posix.cc',
        'src/src/core/lib/gprpp/thd_windows.cc',
        'src/src/core/lib/profiling/basic_timers.cc',
        'src/src/core/lib/profiling/stap_timers.cc',
      ],
    },
    {
      'target_name': 'grpc_core',
      'type': 'static_library',
      'dependencies': [
       '<(DEPTH)/third_party/serf/select_openssl.gyp:select_openssl',
        ':gpr',
      ],
      'sources': [
        'src/src/core/lib/surface/init.cc',
        'src/src/core/lib/avl/avl.cc',
        'src/src/core/lib/backoff/backoff.cc',
        'src/src/core/lib/channel/channel_args.cc',
        'src/src/core/lib/channel/channel_stack.cc',
        'src/src/core/lib/channel/channel_stack_builder.cc',
        'src/src/core/lib/channel/channel_trace.cc',
        'src/src/core/lib/channel/channelz.cc',
        'src/src/core/lib/channel/channelz_registry.cc',
        'src/src/core/lib/channel/connected_channel.cc',
        'src/src/core/lib/channel/handshaker.cc',
        'src/src/core/lib/channel/handshaker_factory.cc',
        'src/src/core/lib/channel/handshaker_registry.cc',
        'src/src/core/lib/channel/status_util.cc',
        'src/src/core/lib/compression/compression.cc',
        'src/src/core/lib/compression/compression_internal.cc',
        'src/src/core/lib/compression/message_compress.cc',
        'src/src/core/lib/compression/stream_compression.cc',
        'src/src/core/lib/compression/stream_compression_gzip.cc',
        'src/src/core/lib/compression/stream_compression_identity.cc',
        'src/src/core/lib/debug/stats.cc',
        'src/src/core/lib/debug/stats_data.cc',
        'src/src/core/lib/http/format_request.cc',
        'src/src/core/lib/http/httpcli.cc',
        'src/src/core/lib/http/parser.cc',
        'src/src/core/lib/iomgr/buffer_list.cc',
        'src/src/core/lib/iomgr/call_combiner.cc',
        'src/src/core/lib/iomgr/combiner.cc',
        'src/src/core/lib/iomgr/endpoint.cc',
        'src/src/core/lib/iomgr/endpoint_pair_posix.cc',
        'src/src/core/lib/iomgr/endpoint_pair_uv.cc',
        'src/src/core/lib/iomgr/endpoint_pair_windows.cc',
        'src/src/core/lib/iomgr/error.cc',
        'src/src/core/lib/iomgr/ev_epoll1_linux.cc',
        'src/src/core/lib/iomgr/ev_epollex_linux.cc',
        'src/src/core/lib/iomgr/ev_poll_posix.cc',
        'src/src/core/lib/iomgr/ev_posix.cc',
        'src/src/core/lib/iomgr/ev_windows.cc',
        'src/src/core/lib/iomgr/exec_ctx.cc',
        'src/src/core/lib/iomgr/executor.cc',
        'src/src/core/lib/iomgr/fork_posix.cc',
        'src/src/core/lib/iomgr/fork_windows.cc',
        'src/src/core/lib/iomgr/gethostname_fallback.cc',
        'src/src/core/lib/iomgr/gethostname_host_name_max.cc',
        'src/src/core/lib/iomgr/gethostname_sysconf.cc',
        'src/src/core/lib/iomgr/internal_errqueue.cc',
        'src/src/core/lib/iomgr/iocp_windows.cc',
        'src/src/core/lib/iomgr/iomgr.cc',
        'src/src/core/lib/iomgr/iomgr_custom.cc',
        'src/src/core/lib/iomgr/iomgr_internal.cc',
        'src/src/core/lib/iomgr/iomgr_posix.cc',
        'src/src/core/lib/iomgr/iomgr_uv.cc',
        'src/src/core/lib/iomgr/iomgr_windows.cc',
        'src/src/core/lib/iomgr/is_epollexclusive_available.cc',
        'src/src/core/lib/iomgr/load_file.cc',
        'src/src/core/lib/iomgr/lockfree_event.cc',
        'src/src/core/lib/iomgr/network_status_tracker.cc',
        'src/src/core/lib/iomgr/polling_entity.cc',
        'src/src/core/lib/iomgr/pollset.cc',
        'src/src/core/lib/iomgr/pollset_custom.cc',
        'src/src/core/lib/iomgr/pollset_set.cc',
        'src/src/core/lib/iomgr/pollset_set_custom.cc',
        'src/src/core/lib/iomgr/pollset_set_windows.cc',
        'src/src/core/lib/iomgr/pollset_uv.cc',
        'src/src/core/lib/iomgr/pollset_windows.cc',
        'src/src/core/lib/iomgr/resolve_address.cc',
        'src/src/core/lib/iomgr/resolve_address_custom.cc',
        'src/src/core/lib/iomgr/resolve_address_posix.cc',
        'src/src/core/lib/iomgr/resolve_address_windows.cc',
        'src/src/core/lib/iomgr/resource_quota.cc',
        'src/src/core/lib/iomgr/sockaddr_utils.cc',
        'src/src/core/lib/iomgr/socket_factory_posix.cc',
        'src/src/core/lib/iomgr/socket_mutator.cc',
        'src/src/core/lib/iomgr/socket_utils_common_posix.cc',
        'src/src/core/lib/iomgr/socket_utils_linux.cc',
        'src/src/core/lib/iomgr/socket_utils_posix.cc',
        'src/src/core/lib/iomgr/socket_utils_uv.cc',
        'src/src/core/lib/iomgr/socket_utils_windows.cc',
        'src/src/core/lib/iomgr/socket_windows.cc',
        'src/src/core/lib/iomgr/tcp_client.cc',
        'src/src/core/lib/iomgr/tcp_client_custom.cc',
        'src/src/core/lib/iomgr/tcp_client_posix.cc',
        'src/src/core/lib/iomgr/tcp_client_windows.cc',
        'src/src/core/lib/iomgr/tcp_custom.cc',
        'src/src/core/lib/iomgr/tcp_posix.cc',
        'src/src/core/lib/iomgr/tcp_server.cc',
        'src/src/core/lib/iomgr/tcp_server_custom.cc',
        'src/src/core/lib/iomgr/tcp_server_posix.cc',
        'src/src/core/lib/iomgr/tcp_server_utils_posix_common.cc',
        'src/src/core/lib/iomgr/tcp_server_utils_posix_ifaddrs.cc',
        'src/src/core/lib/iomgr/tcp_server_utils_posix_noifaddrs.cc',
        'src/src/core/lib/iomgr/tcp_server_windows.cc',
        'src/src/core/lib/iomgr/tcp_uv.cc',
        'src/src/core/lib/iomgr/tcp_windows.cc',
        'src/src/core/lib/iomgr/time_averaged_stats.cc',
        'src/src/core/lib/iomgr/timer.cc',
        'src/src/core/lib/iomgr/timer_custom.cc',
        'src/src/core/lib/iomgr/timer_generic.cc',
        'src/src/core/lib/iomgr/timer_heap.cc',
        'src/src/core/lib/iomgr/timer_manager.cc',
        'src/src/core/lib/iomgr/timer_uv.cc',
        'src/src/core/lib/iomgr/udp_server.cc',
        'src/src/core/lib/iomgr/unix_sockets_posix.cc',
        'src/src/core/lib/iomgr/unix_sockets_posix_noop.cc',
        'src/src/core/lib/iomgr/wakeup_fd_cv.cc',
        'src/src/core/lib/iomgr/wakeup_fd_eventfd.cc',
        'src/src/core/lib/iomgr/wakeup_fd_nospecial.cc',
        'src/src/core/lib/iomgr/wakeup_fd_pipe.cc',
        'src/src/core/lib/iomgr/wakeup_fd_posix.cc',
        'src/src/core/lib/json/json.cc',
        'src/src/core/lib/json/json_reader.cc',
        'src/src/core/lib/json/json_string.cc',
        'src/src/core/lib/json/json_writer.cc',
        'src/src/core/lib/slice/b64.cc',
        'src/src/core/lib/slice/percent_encoding.cc',
        'src/src/core/lib/slice/slice.cc',
        'src/src/core/lib/slice/slice_buffer.cc',
        'src/src/core/lib/slice/slice_intern.cc',
        'src/src/core/lib/slice/slice_string_helpers.cc',
        'src/src/core/lib/surface/api_trace.cc',
        'src/src/core/lib/surface/byte_buffer.cc',
        'src/src/core/lib/surface/byte_buffer_reader.cc',
        'src/src/core/lib/surface/call.cc',
        'src/src/core/lib/surface/call_details.cc',
        'src/src/core/lib/surface/call_log_batch.cc',
        'src/src/core/lib/surface/channel.cc',
        'src/src/core/lib/surface/channel_init.cc',
        'src/src/core/lib/surface/channel_ping.cc',
        'src/src/core/lib/surface/channel_stack_type.cc',
        'src/src/core/lib/surface/completion_queue.cc',
        'src/src/core/lib/surface/completion_queue_factory.cc',
        'src/src/core/lib/surface/event_string.cc',
        'src/src/core/lib/surface/lame_client.cc',
        'src/src/core/lib/surface/metadata_array.cc',
        'src/src/core/lib/surface/server.cc',
        'src/src/core/lib/surface/validate_metadata.cc',
        'src/src/core/lib/surface/version.cc',
        'src/src/core/lib/transport/bdp_estimator.cc',
        'src/src/core/lib/transport/byte_stream.cc',
        'src/src/core/lib/transport/connectivity_state.cc',
        'src/src/core/lib/transport/error_utils.cc',
        'src/src/core/lib/transport/metadata.cc',
        'src/src/core/lib/transport/metadata_batch.cc',
        'src/src/core/lib/transport/pid_controller.cc',
        'src/src/core/lib/transport/service_config.cc',
        'src/src/core/lib/transport/static_metadata.cc',
        'src/src/core/lib/transport/status_conversion.cc',
        'src/src/core/lib/transport/status_metadata.cc',
        'src/src/core/lib/transport/timeout_encoding.cc',
        'src/src/core/lib/transport/transport.cc',
        'src/src/core/lib/transport/transport_op_string.cc',
        'src/src/core/lib/uri/uri_parser.cc',
        'src/src/core/lib/debug/trace.cc',
        'src/src/core/ext/transport/chttp2/server/secure/server_secure_chttp2.cc',
        'src/src/core/ext/transport/chttp2/transport/bin_decoder.cc',
        'src/src/core/ext/transport/chttp2/transport/bin_encoder.cc',
        'src/src/core/ext/transport/chttp2/transport/chttp2_plugin.cc',
        'src/src/core/ext/transport/chttp2/transport/chttp2_transport.cc',
        'src/src/core/ext/transport/chttp2/transport/flow_control.cc',
        'src/src/core/ext/transport/chttp2/transport/frame_data.cc',
        'src/src/core/ext/transport/chttp2/transport/frame_goaway.cc',
        'src/src/core/ext/transport/chttp2/transport/frame_ping.cc',
        'src/src/core/ext/transport/chttp2/transport/frame_rst_stream.cc',
        'src/src/core/ext/transport/chttp2/transport/frame_settings.cc',
        'src/src/core/ext/transport/chttp2/transport/frame_window_update.cc',
        'src/src/core/ext/transport/chttp2/transport/hpack_encoder.cc',
        'src/src/core/ext/transport/chttp2/transport/hpack_parser.cc',
        'src/src/core/ext/transport/chttp2/transport/hpack_table.cc',
        'src/src/core/ext/transport/chttp2/transport/http2_settings.cc',
        'src/src/core/ext/transport/chttp2/transport/huffsyms.cc',
        'src/src/core/ext/transport/chttp2/transport/incoming_metadata.cc',
        'src/src/core/ext/transport/chttp2/transport/parsing.cc',
        'src/src/core/ext/transport/chttp2/transport/stream_lists.cc',
        'src/src/core/ext/transport/chttp2/transport/stream_map.cc',
        'src/src/core/ext/transport/chttp2/transport/varint.cc',
        'src/src/core/ext/transport/chttp2/transport/writing.cc',
        'src/src/core/ext/transport/chttp2/alpn/alpn.cc',
        'src/src/core/ext/filters/http/client/http_client_filter.cc',
        'src/src/core/ext/filters/http/http_filters_plugin.cc',
        'src/src/core/ext/filters/http/message_compress/message_compress_filter.cc',
        'src/src/core/ext/filters/http/server/http_server_filter.cc',
        'src/src/core/lib/http/httpcli_security_connector.cc',
        'src/src/core/lib/security/context/security_context.cc',
        'src/src/core/lib/security/credentials/alts/alts_credentials.cc',
        'src/src/core/lib/security/credentials/composite/composite_credentials.cc',
        'src/src/core/lib/security/credentials/credentials.cc',
        'src/src/core/lib/security/credentials/credentials_metadata.cc',
        'src/src/core/lib/security/credentials/fake/fake_credentials.cc',
        'src/src/core/lib/security/credentials/google_default/credentials_generic.cc',
        'src/src/core/lib/security/credentials/google_default/google_default_credentials.cc',
        'src/src/core/lib/security/credentials/iam/iam_credentials.cc',
        'src/src/core/lib/security/credentials/jwt/json_token.cc',
        'src/src/core/lib/security/credentials/jwt/jwt_credentials.cc',
        'src/src/core/lib/security/credentials/jwt/jwt_verifier.cc',
        'src/src/core/lib/security/credentials/local/local_credentials.cc',
        'src/src/core/lib/security/credentials/oauth2/oauth2_credentials.cc',
        'src/src/core/lib/security/credentials/plugin/plugin_credentials.cc',
        'src/src/core/lib/security/credentials/ssl/ssl_credentials.cc',
        'src/src/core/lib/security/security_connector/alts/alts_security_connector.cc',
        'src/src/core/lib/security/security_connector/fake/fake_security_connector.cc',
        'src/src/core/lib/security/security_connector/load_system_roots_fallback.cc',
        'src/src/core/lib/security/security_connector/load_system_roots_linux.cc',
        'src/src/core/lib/security/security_connector/local/local_security_connector.cc',
        'src/src/core/lib/security/security_connector/security_connector.cc',
        'src/src/core/lib/security/security_connector/ssl/ssl_security_connector.cc',
        'src/src/core/lib/security/security_connector/ssl_utils.cc',
        'src/src/core/lib/security/transport/client_auth_filter.cc',
        'src/src/core/lib/security/transport/secure_endpoint.cc',
        'src/src/core/lib/security/transport/security_handshaker.cc',
        'src/src/core/lib/security/transport/server_auth_filter.cc',
        'src/src/core/lib/security/transport/target_authority_table.cc',
        'src/src/core/lib/security/transport/tsi_error.cc',
        'src/src/core/lib/security/util/json_util.cc',
        'src/src/core/lib/surface/init_secure.cc',
        'src/src/core/tsi/alts/crypt/aes_gcm.cc',
        'src/src/core/tsi/alts/crypt/gsec.cc',
        'src/src/core/tsi/alts/frame_protector/alts_counter.cc',
        'src/src/core/tsi/alts/frame_protector/alts_crypter.cc',
        'src/src/core/tsi/alts/frame_protector/alts_frame_protector.cc',
        'src/src/core/tsi/alts/frame_protector/alts_record_protocol_crypter_common.cc',
        'src/src/core/tsi/alts/frame_protector/alts_seal_privacy_integrity_crypter.cc',
        'src/src/core/tsi/alts/frame_protector/alts_unseal_privacy_integrity_crypter.cc',
        'src/src/core/tsi/alts/frame_protector/frame_handler.cc',
        'src/src/core/tsi/alts/handshaker/alts_handshaker_client.cc',
        'src/src/core/tsi/alts/handshaker/alts_shared_resource.cc',
        'src/src/core/tsi/alts/handshaker/alts_tsi_handshaker.cc',
        'src/src/core/tsi/alts/zero_copy_frame_protector/alts_grpc_integrity_only_record_protocol.cc',
        'src/src/core/tsi/alts/zero_copy_frame_protector/alts_grpc_privacy_integrity_record_protocol.cc',
        'src/src/core/tsi/alts/zero_copy_frame_protector/alts_grpc_record_protocol_common.cc',
        'src/src/core/tsi/alts/zero_copy_frame_protector/alts_iovec_record_protocol.cc',
        'src/src/core/tsi/alts/zero_copy_frame_protector/alts_zero_copy_grpc_protector.cc',
        'src/src/core/lib/security/credentials/alts/check_gcp_environment.cc',
        'src/src/core/lib/security/credentials/alts/check_gcp_environment_linux.cc',
        'src/src/core/lib/security/credentials/alts/check_gcp_environment_no_op.cc',
        'src/src/core/lib/security/credentials/alts/check_gcp_environment_windows.cc',
        'src/src/core/lib/security/credentials/alts/grpc_alts_credentials_client_options.cc',
        'src/src/core/lib/security/credentials/alts/grpc_alts_credentials_options.cc',
        'src/src/core/lib/security/credentials/alts/grpc_alts_credentials_server_options.cc',
        'src/src/core/tsi/alts/handshaker/alts_handshaker_service_api.cc',
        'src/src/core/tsi/alts/handshaker/alts_handshaker_service_api_util.cc',
        'src/src/core/tsi/alts/handshaker/alts_tsi_utils.cc',
        'src/src/core/tsi/alts/handshaker/transport_security_common_api.cc',
        'src/src/core/tsi/alts/handshaker/altscontext.pb.c',
        'src/src/core/tsi/alts/handshaker/handshaker.pb.c',
        'src/src/core/tsi/alts/handshaker/transport_security_common.pb.c',
        'src/third_party/nanopb/pb_common.c',
        'src/third_party/nanopb/pb_decode.c',
        'src/third_party/nanopb/pb_encode.c',
        'src/src/core/tsi/transport_security.cc',
        'src/src/core/ext/transport/chttp2/client/insecure/channel_create.cc',
        'src/src/core/ext/transport/chttp2/client/insecure/channel_create_posix.cc',
        'src/src/core/ext/transport/chttp2/client/authority.cc',
        'src/src/core/ext/transport/chttp2/client/chttp2_connector.cc',
        'src/src/core/ext/filters/client_channel/backup_poller.cc',
        'src/src/core/ext/filters/client_channel/channel_connectivity.cc',
        'src/src/core/ext/filters/client_channel/client_channel.cc',
        'src/src/core/ext/filters/client_channel/client_channel_channelz.cc',
        'src/src/core/ext/filters/client_channel/client_channel_factory.cc',
        'src/src/core/ext/filters/client_channel/client_channel_plugin.cc',
        'src/src/core/ext/filters/client_channel/connector.cc',
        'src/src/core/ext/filters/client_channel/health/health_check_client.cc',
        'src/src/core/ext/filters/client_channel/http_connect_handshaker.cc',
        'src/src/core/ext/filters/client_channel/http_proxy.cc',
        'src/src/core/ext/filters/client_channel/lb_policy.cc',
        'src/src/core/ext/filters/client_channel/lb_policy_factory.cc',
        'src/src/core/ext/filters/client_channel/lb_policy_registry.cc',
        'src/src/core/ext/filters/client_channel/parse_address.cc',
        'src/src/core/ext/filters/client_channel/proxy_mapper.cc',
        'src/src/core/ext/filters/client_channel/proxy_mapper_registry.cc',
        'src/src/core/ext/filters/client_channel/resolver.cc',
        'src/src/core/ext/filters/client_channel/resolver_registry.cc',
        'src/src/core/ext/filters/client_channel/resolver_result_parsing.cc',
        'src/src/core/ext/filters/client_channel/retry_throttle.cc',
        'src/src/core/ext/filters/client_channel/subchannel.cc',
        'src/src/core/ext/filters/client_channel/subchannel_index.cc',
        'src/src/core/ext/filters/deadline/deadline_filter.cc',
        'src/src/core/ext/filters/client_channel/health/health.pb.c',
        'src/src/core/tsi/fake_transport_security.cc',
        'src/src/core/tsi/local_transport_security.cc',
        'src/src/core/tsi/ssl/session_cache/ssl_session_boringssl.cc',
        'src/src/core/tsi/ssl/session_cache/ssl_session_cache.cc',
        'src/src/core/tsi/ssl/session_cache/ssl_session_openssl.cc',
        'src/src/core/tsi/ssl_transport_security.cc',
        'src/src/core/tsi/transport_security_grpc.cc',
        'src/src/core/ext/transport/chttp2/server/chttp2_server.cc',
        'src/src/core/ext/transport/chttp2/client/secure/secure_channel_create.cc',
        'src/src/core/ext/transport/chttp2/server/insecure/server_chttp2.cc',
        'src/src/core/ext/transport/chttp2/server/insecure/server_chttp2_posix.cc',
        'src/src/core/ext/transport/inproc/inproc_plugin.cc',
        'src/src/core/ext/transport/inproc/inproc_transport.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/client_load_reporting_filter.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/grpclb.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/grpclb_channel_secure.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/grpclb_client_stats.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/load_balancer_api.cc',
        'src/src/core/ext/filters/client_channel/resolver/fake/fake_resolver.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/proto/grpc/lb/v1/google/protobuf/duration.pb.c',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/proto/grpc/lb/v1/google/protobuf/timestamp.pb.c',
        'src/src/core/ext/filters/client_channel/lb_policy/grpclb/proto/grpc/lb/v1/load_balancer.pb.c',
        'src/src/core/ext/filters/client_channel/lb_policy/xds/xds.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/xds/xds_channel_secure.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/xds/xds_client_stats.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/xds/xds_load_balancer_api.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/pick_first/pick_first.cc',
        'src/src/core/ext/filters/client_channel/lb_policy/round_robin/round_robin.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/dns_resolver_ares.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_ev_driver.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_ev_driver_posix.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_ev_driver_windows.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_wrapper.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_wrapper_fallback.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_wrapper_posix.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/c_ares/grpc_ares_wrapper_windows.cc',
        'src/src/core/ext/filters/client_channel/resolver/dns/native/dns_resolver.cc',
        'src/src/core/ext/filters/client_channel/resolver/sockaddr/sockaddr_resolver.cc',
        'src/src/core/ext/filters/census/grpc_context.cc',
        'src/src/core/ext/filters/max_age/max_age_filter.cc',
        'src/src/core/ext/filters/message_size/message_size_filter.cc',
        'src/src/core/ext/filters/http/client_authority_filter.cc',
        'src/src/core/ext/filters/workarounds/workaround_cronet_compression_filter.cc',
        'src/src/core/ext/filters/workarounds/workaround_utils.cc',
        'src/src/core/plugin_registry/grpc_plugin_registry.cc',
      ],
      'defines': [
        'GPR_BACKWARDS_COMPATIBILITY_MODE',
      ],
    },
    {
      'target_name': 'grpc_cpp',
      'type': 'static_library',
      'dependencies': [
        ':grpc_core',
        ':gpr',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'sources': [
        'src/src/cpp/client/insecure_credentials.cc',
        'src/src/cpp/client/secure_credentials.cc',
        'src/src/cpp/common/auth_property_iterator.cc',
        'src/src/cpp/common/secure_auth_context.cc',
        'src/src/cpp/common/secure_channel_arguments.cc',
        'src/src/cpp/common/secure_create_auth_context.cc',
        'src/src/cpp/server/insecure_server_credentials.cc',
        'src/src/cpp/server/secure_server_credentials.cc',
        'src/src/cpp/client/channel_cc.cc',
        'src/src/cpp/client/client_context.cc',
        'src/src/cpp/client/client_interceptor.cc',
        'src/src/cpp/client/create_channel.cc',
        'src/src/cpp/client/create_channel_internal.cc',
        'src/src/cpp/client/create_channel_posix.cc',
        'src/src/cpp/client/credentials_cc.cc',
        'src/src/cpp/client/generic_stub.cc',
        'src/src/cpp/common/alarm.cc',
        'src/src/cpp/common/channel_arguments.cc',
        'src/src/cpp/common/channel_filter.cc',
        'src/src/cpp/common/completion_queue_cc.cc',
        'src/src/cpp/common/core_codegen.cc',
        'src/src/cpp/common/resource_quota_cc.cc',
        'src/src/cpp/common/rpc_method.cc',
        'src/src/cpp/common/version_cc.cc',
        'src/src/cpp/server/async_generic_service.cc',
        'src/src/cpp/server/channel_argument_option.cc',
        'src/src/cpp/server/create_default_thread_pool.cc',
        'src/src/cpp/server/dynamic_thread_pool.cc',
        'src/src/cpp/server/health/default_health_check_service.cc',
        'src/src/cpp/server/health/health_check_service.cc',
        'src/src/cpp/server/health/health_check_service_server_builder_option.cc',
        'src/src/cpp/server/server_builder.cc',
        'src/src/cpp/server/server_cc.cc',
        'src/src/cpp/server/server_context.cc',
        'src/src/cpp/server/server_credentials.cc',
        'src/src/cpp/server/server_posix.cc',
        'src/src/cpp/thread_manager/thread_manager.cc',
        'src/src/cpp/util/byte_buffer_cc.cc',
        'src/src/cpp/util/status.cc',
        'src/src/cpp/util/string_ref.cc',
        'src/src/cpp/util/time_cc.cc',
        'src/src/core/ext/filters/client_channel/health/health.pb.c',
        'src/third_party/nanopb/pb_common.c',
        'src/third_party/nanopb/pb_decode.c',
        'src/third_party/nanopb/pb_encode.c',
        'src/src/cpp/codegen/codegen_init.cc',
      ],
    },
    {
      'target_name': 'grpc_cpp_plugin',
      'type': 'executable',
      'toolsets': [ 'host' ],
      'sources': [
        'src/src/compiler/cpp_generator.cc',
        'src/src/compiler/cpp_plugin.cc',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc_lib',
      ],
    },
  ]
}
