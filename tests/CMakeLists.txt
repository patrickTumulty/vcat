
# RTSP test server: test-only helper exercised by scripts/send_test_video.sh
# --transport rtsp. Excluded from the default build and never shipped with
# vcat; build explicitly with:
#
#     cmake --build <build-dir> --target rtsp-test-server

add_executable(rtsp-test-server
    EXCLUDE_FROM_ALL
    rtsp_test_server.c
)

# Land in <build>/tests, outside the shipping <build>/bin tree.
set_target_properties(rtsp-test-server
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(rtsp-test-server
    PRIVATE
        ${GSTRTSPSERVER_LIBRARIES}
)

target_include_directories(rtsp-test-server
    PRIVATE
        ${GSTRTSPSERVER_INCLUDE_DIRS}
)
