#include "root_path.hpp"

#include <pxp-agent/results_storage.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/request_type.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <boost/filesystem/operations.hpp>

#include <catch.hpp>

#include <string>
#include <utility>  // std::move

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;

TEST_CASE("ResultsStorage ctor", "[module]") {
    SECTION("can instantiate") {
        REQUIRE_NOTHROW(ResultsStorage("/some/spool/dir"));
    }
}

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static void configureTest(const std::string& p = SPOOL_DIR) {
    if (!fs::exists(p) && !fs::create_directories(p))
        FAIL("Failed to create the spool directory");
}

static void resetTest(const std::string& p = SPOOL_DIR) {
    if (fs::exists(p))
        fs::remove_all(p);
}

TEST_CASE("ResultsStorage::find", "[module][results]") {
    configureTest();

    SECTION("returns false when the spool directory does not exist") {
        ResultsStorage storage { SPOOL_DIR };

        REQUIRE_FALSE(storage.find("some_transaction_id"));
    }

    SECTION("returns true when the spool directory exists") {
        ResultsStorage storage { SPOOL_DIR };
        auto dir = SPOOL_DIR + "/some_transaction_id";

        if (!fs::exists(dir) && !fs::create_directories(dir))
            FAIL("Failed to create the results directory");

        REQUIRE(storage.find("some_transaction_id"));

        fs::remove_all(dir);
    }

    resetTest();
}

TEST_CASE("ResultsStorage::initializeMetadataFile", "[module][results]") {
    configureTest();

    SECTION("it does create the results dir for the given transaction") {
        ResultsStorage storage { SPOOL_DIR };
        lth_jc::JsonContainer metadata {};
        metadata.set<std::string>("foo", "bar");
        storage.initializeMetadataFile("1234", metadata);

        REQUIRE(fs::exists(SPOOL_DIR + "/1234"));
    }

    resetTest();
}

static const std::string TESTING_RESULTS { std::string { PXP_AGENT_ROOT_PATH}
                                           + "/lib/tests/resources/action_results" };

static const std::string VALID_TRANSACTION { "valid" };
static const std::string BROKEN_TRANSACTION { "broken" };

TEST_CASE("ResultsStorage::getActionMetadata", "[module][results]") {
    configureTest(TESTING_RESULTS);
    ResultsStorage st { TESTING_RESULTS };

    SECTION("Throws an Error if the metadata file does not exist") {
        REQUIRE_THROWS_AS(st.getActionMetadata("does_not_exist"),
                          ResultsStorage::Error);
    }

    SECTION("Throws an Error if the metadata is invalid") {
        REQUIRE_THROWS_AS(st.getActionMetadata(BROKEN_TRANSACTION),
                          ResultsStorage::Error);
    }

    SECTION("Returns a JSON object if the metadata is valid") {
        REQUIRE_NOTHROW(st.getActionMetadata(VALID_TRANSACTION));
    }
}

TEST_CASE("ResultsStorage::updateMetadataFile", "[module][results]") {
    std::string valid_transaction_id { "1234" };
    lth_jc::JsonContainer some_valid_metadata {};
    some_valid_metadata.set<std::string>("requester", "me");
    some_valid_metadata.set<std::string>("module", "good_stuff");
    some_valid_metadata.set<std::string>("action", "do_stuff");
    some_valid_metadata.set<std::string>("request_params", "abc");
    some_valid_metadata.set<std::string>("transaction_id", valid_transaction_id);
    some_valid_metadata.set<std::string>("request_id", "45");
    some_valid_metadata.set<bool>("notify_outcome", false);
    some_valid_metadata.set<std::string>("start", "5:60");
    some_valid_metadata.set<std::string>("status", "running");

    configureTest();
    ResultsStorage st { SPOOL_DIR };

    SECTION("Throws an Error if the results directory does not exist") {
        REQUIRE_THROWS_AS(st.updateMetadataFile(valid_transaction_id,
                                                some_valid_metadata),
                          ResultsStorage::Error);
    }

    SECTION("Correctly updates the metadata file") {
        st.initializeMetadataFile(valid_transaction_id, some_valid_metadata);
        some_valid_metadata.set<std::string>("status", "success");
        st.updateMetadataFile(valid_transaction_id, some_valid_metadata);
        auto read_metadata = st.getActionMetadata(valid_transaction_id);

        REQUIRE(read_metadata.get<std::string>("status") == "success");
    }

    resetTest();
}

TEST_CASE("ResultsStorage::pidFileExists", "[module][results]") {
    configureTest(TESTING_RESULTS);
    ResultsStorage st { TESTING_RESULTS };

    SECTION("returns true if exists") {
        REQUIRE(st.pidFileExists(VALID_TRANSACTION));
    }

    SECTION("returns false if it does not exist") {
        REQUIRE_FALSE(st.pidFileExists("does_not_exist"));
    }
}

TEST_CASE("ResultsStorage::getPID", "[module][results]") {
    configureTest(TESTING_RESULTS);
    ResultsStorage st { TESTING_RESULTS };

    SECTION("Throws an Error if the PID file does not exist") {
        REQUIRE_THROWS_AS(st.getPID("does_not_exist"),
                          ResultsStorage::Error);
    }

    SECTION("Throws an Error if the PID is invalid") {
        REQUIRE_THROWS_AS(st.getPID(BROKEN_TRANSACTION),
                          ResultsStorage::Error);
    }

    SECTION("Returns an integer if the PID is valid") {
        REQUIRE_NOTHROW(st.getPID(VALID_TRANSACTION));
    }
}

TEST_CASE("ResultsStorage::getOutput", "[module][results]") {
    ResultsStorage st { TESTING_RESULTS };

    SECTION("Throws an Error if the exitcode is invalid") {
        REQUIRE_THROWS_AS(st.getOutput(BROKEN_TRANSACTION),
                          ResultsStorage::Error);
    }

    SECTION("Retrieves correctly valid output") {
        auto output = st.getOutput(VALID_TRANSACTION);

        REQUIRE(output.exitcode == 0);
        REQUIRE(output.std_err == "Hey, all good here!");
        REQUIRE(output.std_out == "{\"spam\":\"eggs\"}");
    }
}

}  // namespace PXPAgent
