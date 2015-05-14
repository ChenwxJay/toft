cc_library(
    name = 'distribute_lock',
    srcs = [
        'distribute_lock.cc',
        'error_code.cc',
        'pub_func.cc'
    ],
    deps = [
        '//thirdparty/zookeeper:zookeeper_mt',
        '//thirdparty/glog:glog',
        '//toft/system/time:time',
        '//toft/system/threading:threading',
    ]
)

cc_test(
    name = 'client_test',
    srcs = [
        'client_test.cc'
    ],
    deps = [
        ':distribute_lock',
        '//thirdparty/gtest:gtest',
        '//thirdparty/gtest:gtest_main'
    ]
)
