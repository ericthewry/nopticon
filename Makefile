CXX_FLAGS += --std=c++11 -Wall -I./src

BUILD_DIR = build

SRC = src/analysis.cc                  \
      src/flow_graph.cc                \
      # Empty line

SRC_HEADER = src/analysis.hh           \
             src/flow_graph.hh         \
             src/ip_prefix_tree.hh     \
             src/ipv4.hh               \
             src/nopticon.hh           \
              # Empty line

CMD = cmd/gobgp_analysis.cc            \
      # Empty line

TEST = test/analysis_test.cc           \
       test/flow_graph_test.cc         \
       test/ipv4_test.cc               \
       test/ipv4_test_data.cc          \
       test/run_tests.cc               \
       # Empty line

TEST_HEADER = test/analysis_test.hh    \
              test/flow_graph_test.hh  \
              test/ipv4_test.hh        \
              test/ipv4_test_data.hh   \
              # Empty line

default: ${BUILD_DIR}/gobgp-analysis

${BUILD_DIR}/gobgp-analysis: ${SRC} ${SRC_HEADER} ${CMD} | mk_build_dir
	${CXX} ${CXX_FLAGS} -o $@ -I./deps/rapidjson/include ${SRC} ${CMD}

gobgp-analysis-test: ${BUILD_DIR}/gobgp-analysis
	./test/gobgp-analysis-test.sh

${BUILD_DIR}/run-test: ${SRC} ${SRC_HEADER} ${TEST} ${TEST_HEADER} | mk_build_dir
	${CXX} ${CXX_FLAGS} -o $@ ${SRC} ${TEST}

run-test: ${BUILD_DIR}/run-test
	${BUILD_DIR}/run-test

mk_build_dir:
	mkdir -p ${BUILD_DIR}

clean:
	rm -rf ${BUILD_DIR}

format:
	clang-format -i ${SRC} ${SRC_HEADER} ${TEST} ${TEST_HEADER}
