#include <gtest/gtest.h>
#include <stdlib.h>
#include <iostream>
#include <http_data.h>
#include "auth_manager.h"
#include "core_api.h"

static const size_t FUTURE_TS = 64723363199;

class AuthManagerTest : public ::testing::Test {
protected:
    Store *store;
    AuthManager auth_manager;

    void setupCollection() {
        std::string state_dir_path = "/tmp/typesense_test/auth_manager_test_db";
        system(("rm -rf "+state_dir_path+" && mkdir -p "+state_dir_path).c_str());

        store = new Store(state_dir_path);
        auth_manager.init(store);
    }

    virtual void SetUp() {
        setupCollection();
    }

    virtual void TearDown() {
        delete store;
    }
};

TEST_F(AuthManagerTest, CreateListDeleteAPIKeys) {
    auto list_op = auth_manager.list_keys();
    ASSERT_TRUE(list_op.ok());
    ASSERT_EQ(0, list_op.get().size());

    auto get_op = auth_manager.get_key(0);
    ASSERT_FALSE(get_op.ok());
    ASSERT_EQ(404, get_op.code());

    // test inserts

    api_key_t api_key1("abcd1", "test key 1", {"read", "write"}, {"collection1", "collection2"}, FUTURE_TS);
    api_key_t api_key2("abcd2", "test key 2", {"admin"}, {"*"}, FUTURE_TS);

    ASSERT_EQ("abcd1", api_key1.value);
    ASSERT_EQ("abcd2", api_key2.value);

    auto insert_op = auth_manager.create_key(api_key1);
    ASSERT_TRUE(insert_op.ok());
    ASSERT_EQ(5, insert_op.get().value.size());

    insert_op = auth_manager.create_key(api_key2);
    ASSERT_TRUE(insert_op.ok());
    ASSERT_EQ(5, insert_op.get().value.size());

    // get an individual key

    get_op = auth_manager.get_key(0);
    ASSERT_TRUE(get_op.ok());
    const api_key_t &key1 = get_op.get();
    ASSERT_EQ(4, key1.value.size());
    ASSERT_EQ("test key 1", key1.description);
    ASSERT_EQ(2, key1.actions.size());
    EXPECT_STREQ("read", key1.actions[0].c_str());
    EXPECT_STREQ("write", key1.actions[1].c_str());
    ASSERT_EQ(2, key1.collections.size());
    EXPECT_STREQ("collection1", key1.collections[0].c_str());
    EXPECT_STREQ("collection2", key1.collections[1].c_str());

    get_op = auth_manager.get_key(1);
    ASSERT_TRUE(get_op.ok());
    ASSERT_EQ(4, get_op.get().value.size());
    ASSERT_EQ("test key 2", get_op.get().description);

    get_op = auth_manager.get_key(1, false);
    ASSERT_TRUE(get_op.ok());
    ASSERT_NE(4, get_op.get().value.size());

    get_op = auth_manager.get_key(2, false);
    ASSERT_FALSE(get_op.ok());

    // listing keys
    list_op = auth_manager.list_keys();
    ASSERT_TRUE(list_op.ok());
    ASSERT_EQ(2, list_op.get().size());
    ASSERT_EQ("test key 1", list_op.get()[0].description);
    ASSERT_EQ("abcd", list_op.get()[0].value);
    ASSERT_EQ("test key 2", list_op.get()[1].description);
    ASSERT_EQ("abcd", list_op.get()[1].value);
}

TEST_F(AuthManagerTest, CheckRestoreOfAPIKeys) {
    api_key_t api_key1("abcd1", "test key 1", {"read", "write"}, {"collection1", "collection2"}, FUTURE_TS);
    api_key_t api_key2("abcd2", "test key 2", {"admin"}, {"*"}, FUTURE_TS);

    std::string key_value1 = auth_manager.create_key(api_key1).get().value;
    std::string key_value2 = auth_manager.create_key(api_key2).get().value;

    AuthManager auth_manager2;
    auth_manager2.init(store);

    // list keys

    auto list_op = auth_manager.list_keys();
    ASSERT_TRUE(list_op.ok());
    ASSERT_EQ(2, list_op.get().size());
    ASSERT_EQ("test key 1", list_op.get()[0].description);
    ASSERT_EQ("abcd", list_op.get()[0].value);
    ASSERT_STREQ(key_value1.substr(0, 4).c_str(), list_op.get()[0].value.c_str());
    ASSERT_EQ(FUTURE_TS, list_op.get()[0].expires_at);

    ASSERT_EQ("test key 2", list_op.get()[1].description);
    ASSERT_EQ("abcd", list_op.get()[1].value);
    ASSERT_STREQ(key_value2.substr(0, 4).c_str(), list_op.get()[1].value.c_str());
    ASSERT_EQ(FUTURE_TS, list_op.get()[1].expires_at);
}

TEST_F(AuthManagerTest, VerifyAuthentication) {
    std::map<std::string, std::string> sparams;
    // when no keys are present at all
    ASSERT_FALSE(auth_manager.authenticate("jdlaslasdasd", "", {}, sparams));

    // wildcard permission
    api_key_t wildcard_all_key = api_key_t("abcd1", "wildcard all key", {"*"}, {"*"}, FUTURE_TS);
    auth_manager.create_key(wildcard_all_key);

    ASSERT_FALSE(auth_manager.authenticate("jdlaslasdasd", "documents:create", {"collection1"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(wildcard_all_key.value, "metrics:get", {""}, sparams));

    // long API key
    std::string long_api_key_str = StringUtils::randstring(50);
    api_key_t long_api_key = api_key_t(long_api_key_str, "long api key", {"*"}, {"*"}, FUTURE_TS);
    auth_manager.create_key(long_api_key);

    ASSERT_TRUE(auth_manager.authenticate(long_api_key_str, "metrics:get", {""}, sparams));

    // wildcard on a collection
    api_key_t wildcard_coll_key = api_key_t("abcd2", "wildcard coll key", {"*"}, {"collection1"}, FUTURE_TS);
    auth_manager.create_key(wildcard_coll_key);

    ASSERT_FALSE(auth_manager.authenticate("adasda", "documents:create", {"collection1"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(wildcard_coll_key.value, "documents:get", {"collection1"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(wildcard_coll_key.value, "documents:get", {"collection2"}, sparams));

    // wildcard on multiple collections
    api_key_t wildcard_colls_key = api_key_t("abcd3", "wildcard coll key", {"*"}, {"collection1", "collection2", "collection3"}, FUTURE_TS);
    auth_manager.create_key(wildcard_colls_key);

    ASSERT_TRUE(auth_manager.authenticate(wildcard_colls_key.value, "documents:get", {"collection1"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(wildcard_colls_key.value, "documents:search", {"collection2"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(wildcard_colls_key.value, "documents:create", {"collection3"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(wildcard_colls_key.value, "documents:get", {"collection4"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(wildcard_colls_key.value, "documents:get", {"*"}, sparams));

    // only 1 action on multiple collections
    api_key_t one_action_key = api_key_t("abcd4", "one action key", {"documents:search"}, {"collection1", "collection2"}, FUTURE_TS);
    auth_manager.create_key(one_action_key);

    ASSERT_TRUE(auth_manager.authenticate(one_action_key.value, "documents:search", {"collection1"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(one_action_key.value, "documents:get", {"collection2"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(one_action_key.value, "documents:search", {{"collection5"}}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(one_action_key.value, "*", {"collection2"}, sparams));

    // multiple actions on multiple collections
    api_key_t mul_acoll_key = api_key_t("abcd5", "multiple action/collection key",
                                        {"documents:get", "collections:list"}, {"metacollection", "collection2"}, FUTURE_TS);
    auth_manager.create_key(mul_acoll_key);

    ASSERT_TRUE(auth_manager.authenticate(mul_acoll_key.value, "documents:get", {"metacollection"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(mul_acoll_key.value, "collections:list", {"collection2"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(mul_acoll_key.value, "collections:list", {"metacollection"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(mul_acoll_key.value, "documents:search", {"collection2"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(mul_acoll_key.value, "documents:get", {"collection5"}, sparams));
    ASSERT_FALSE(auth_manager.authenticate(mul_acoll_key.value, "*", {"*"}, sparams));

    // regexp match

    api_key_t regexp_colls_key1 = api_key_t("abcd6", "regexp coll key", {"*"}, {"coll.*"}, FUTURE_TS);
    auth_manager.create_key(regexp_colls_key1);
    ASSERT_TRUE(auth_manager.authenticate(regexp_colls_key1.value, "collections:list", {{"collection2"}}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(regexp_colls_key1.value, "documents:get", {"collection5"}, sparams));

    api_key_t regexp_colls_key2 = api_key_t("abcd7", "regexp coll key", {"*"}, {".*meta.*"}, FUTURE_TS);
    auth_manager.create_key(regexp_colls_key2);
    ASSERT_TRUE(auth_manager.authenticate(regexp_colls_key2.value, "collections:list", {"metacollection"}, sparams));
    ASSERT_TRUE(auth_manager.authenticate(regexp_colls_key2.value, "collections:list", {"ametacollection"}, sparams));

    // check for expiry

    api_key_t expired_key1 = api_key_t("abcd8", "expiry key", {"*"}, {"*"}, 1606542716);
    auth_manager.create_key(expired_key1);
    ASSERT_FALSE(auth_manager.authenticate(expired_key1.value, "collections:list", {"collection"}, sparams));

    api_key_t unexpired_key1 = api_key_t("abcd9", "expiry key", {"*"}, {"*"}, 2237712220);
    auth_manager.create_key(unexpired_key1);
    ASSERT_TRUE(auth_manager.authenticate(unexpired_key1.value, "collections:list", {"collection"}, sparams));
}

TEST_F(AuthManagerTest, HandleAuthentication) {
    route_path rpath_multi_search = route_path("POST", {"multi_search"}, post_multi_search, false, false);
    std::map<std::string, std::string> req_params;

    std::vector<std::string> collections;
    get_collections_for_auth(req_params, "{]", rpath_multi_search, collections);

    ASSERT_EQ(1, collections.size());
    ASSERT_STREQ("", collections[0].c_str());

    nlohmann::json sample_search_body;
    sample_search_body["searches"] = nlohmann::json::array();
    nlohmann::json search_query;
    search_query["q"] = "aaa";
    search_query["collection"] = "company1";

    sample_search_body["searches"].push_back(search_query);

    search_query["collection"] = "company2";
    sample_search_body["searches"].push_back(search_query);

    collections.clear();
    get_collections_for_auth(req_params, sample_search_body.dump(), rpath_multi_search, collections);

    ASSERT_EQ(2, collections.size());
    ASSERT_STREQ("company1", collections[0].c_str());
    ASSERT_STREQ("company2", collections[1].c_str());

    collections.clear();
    req_params["collection"] = "foo";

    get_collections_for_auth(req_params, sample_search_body.dump(), rpath_multi_search, collections);

    ASSERT_EQ(3, collections.size());
    ASSERT_STREQ("foo", collections[0].c_str());
    ASSERT_STREQ("company1", collections[1].c_str());
    ASSERT_STREQ("company2", collections[2].c_str());

    collections.clear();
    req_params.clear();

    route_path rpath_search = route_path("GET", {"collections", ":collection", "documents", "search"}, get_search, false, false);
    get_collections_for_auth(req_params, sample_search_body.dump(), rpath_search, collections);

    ASSERT_EQ(1, collections.size());
    ASSERT_STREQ("", collections[0].c_str());
}

TEST_F(AuthManagerTest, GenerationOfAPIAction) {
    route_path rpath_search = route_path("GET", {"collections", ":collection", "documents", "search"}, nullptr, false, false);
    route_path rpath_multi_search = route_path("POST", {"multi_search"}, nullptr, false, false);
    route_path rpath_coll_create = route_path("POST", {"collections"}, nullptr, false, false);
    route_path rpath_coll_get = route_path("GET", {"collections", ":collection"}, nullptr, false, false);
    route_path rpath_coll_list = route_path("GET", {"collections"}, nullptr, false, false);
    route_path rpath_keys_post = route_path("POST", {"keys"}, nullptr, false, false);
    route_path rpath_doc_delete = route_path("DELETE", {"collections", ":collection", "documents", ":id"}, nullptr, false, false);
    route_path rpath_override_upsert = route_path("PUT", {"collections", ":collection", "overrides", ":id"}, nullptr, false, false);
    route_path rpath_doc_patch = route_path("PATCH", {"collections", ":collection", "documents", ":id"}, nullptr, false, false);

    ASSERT_STREQ("documents:search", rpath_search._get_action().c_str());
    ASSERT_STREQ("documents:search", rpath_multi_search._get_action().c_str());
    ASSERT_STREQ("collections:create", rpath_coll_create._get_action().c_str());
    ASSERT_STREQ("collections:get", rpath_coll_get._get_action().c_str());
    ASSERT_STREQ("collections:list", rpath_coll_list._get_action().c_str());
    ASSERT_STREQ("keys:create", rpath_keys_post._get_action().c_str());
    ASSERT_STREQ("documents:delete", rpath_doc_delete._get_action().c_str());
    ASSERT_STREQ("overrides:upsert", rpath_override_upsert._get_action().c_str());
    ASSERT_STREQ("documents:update", rpath_doc_patch._get_action().c_str());
}

TEST_F(AuthManagerTest, ScopedAPIKeys) {
    std::map<std::string, std::string> params;
    params["filter_by"] = "country:USA";

    // create a API key bound to search scope and a given collection
    api_key_t key_search_coll1("KeyVal", "test key", {"documents:search"}, {"coll1"}, FUTURE_TS);
    auth_manager.create_key(key_search_coll1);

    std::string scoped_key = StringUtils::base64_encode(
      "IvjqWNZ5M5ElcvbMoXj45BxkQrZG4ZKEaNQoRioCx2s=KeyV{\"filter_by\": \"user_id:1080\"}"
    );

    ASSERT_TRUE(auth_manager.authenticate(scoped_key, "documents:search", {"coll1"}, params));
    ASSERT_STREQ("country:USA&&user_id:1080", params["filter_by"].c_str());

    // should scope to collection bound by the parent key
    ASSERT_FALSE(auth_manager.authenticate(scoped_key, "documents:search", {"coll2"}, params));

    // should scope to search action only
    ASSERT_FALSE(auth_manager.authenticate(scoped_key, "documents:create", {"coll1"}, params));

    // check with corrupted key
    ASSERT_FALSE(auth_manager.authenticate("asdasasd", "documents:search", {"coll1"}, params));

    // when params is empty, embedded param should be set
    std::map<std::string, std::string> empty_params;
    ASSERT_TRUE(auth_manager.authenticate(scoped_key, "documents:search", {"coll1"}, empty_params));
    ASSERT_STREQ("user_id:1080", empty_params["filter_by"].c_str());

    // when more than a single key prefix matches, must pick the correct underlying key
    api_key_t key_search_coll2("KeyVal2", "test key", {"documents:search"}, {"coll2"}, FUTURE_TS);
    auth_manager.create_key(key_search_coll2);
    ASSERT_FALSE(auth_manager.authenticate(scoped_key, "documents:search", {"coll2"}, empty_params));

    // should only allow scoped API keys derived from parent key with documents:search action
    api_key_t key_search_admin("AdminKey", "admin key", {"*"}, {"*"}, FUTURE_TS);
    auth_manager.create_key(key_search_admin);
    std::string scoped_key2 = StringUtils::base64_encode(
      "BXbsk+xLT1gxOjDyip6+PE4MtOzOm/H7kbkN1d/j/s4=Admi{\"filter_by\": \"user_id:1080\"}"
    );
    ASSERT_FALSE(auth_manager.authenticate(scoped_key2, "documents:search", {"coll2"}, empty_params));

    // expiration of scoped api key

    // {"filter_by": "user_id:1080", "expires_at": 2237712220} (NOT expired)
    api_key_t key_expiry("ExpireKey", "expire key", {"documents:search"}, {"*"}, FUTURE_TS);
    auth_manager.create_key(key_expiry);

    empty_params.clear();

    std::string scoped_key3 = "K1M2STRDelZYNHpxNGVWUTlBTGpOWUl4dk8wNU8xdnVEZi9aSUcvZE5tcz1FeHBpeyJmaWx0ZXJfYnkiOi"
                              "AidXNlcl9pZDoxMDgwIiwgImV4cGlyZXNfYXQiOiAyMjM3NzEyMjIwfQ==";

    ASSERT_TRUE(auth_manager.authenticate(scoped_key3, "documents:search", {"coll1"}, empty_params));
    ASSERT_STREQ("user_id:1080", empty_params["filter_by"].c_str());
    ASSERT_EQ(1, empty_params.size());

    // {"filter_by": "user_id:1080", "expires_at": 1606563316} (expired)

    api_key_t key_expiry2("ExpireKey2", "expire key", {"documents:search"}, {"*"}, FUTURE_TS);
    auth_manager.create_key(key_expiry2);

    empty_params.clear();

    std::string scoped_key4 = "SXFKNldZZWRiWkVKVmI2RCt3OTlKNHpBZ24yWlRUbEdJdERtTy9IZ2REZz1FeHBpeyJmaWx0ZXJfYnkiOiAidXN"
                              "lcl9pZDoxMDgwIiwgImV4cGlyZXNfYXQiOiAxNjA2NTYzMzE2fQ==";

    ASSERT_FALSE(auth_manager.authenticate(scoped_key4, "documents:search", {"coll1"}, empty_params));

    // {"filter_by": "user_id:1080", "expires_at": 64723363200} (greater than parent key expiry)
    // embedded key's param cannot exceed parent's expiry

    api_key_t key_expiry3("ExpireKey3", "expire key", {"documents:search"}, {"*"}, 1606563841);
    auth_manager.create_key(key_expiry3);

    empty_params.clear();

    std::string scoped_key5 = "V3JMNFJlZHRMVStrZHphNFVGZDh4MWltSmx6Yzk2R3QvS2ZwSE8weGRWQT1FeHBpeyJmaWx0ZXJfYnkiOiAidX"
                              "Nlcl9pZDoxMDgwIiwgImV4cGlyZXNfYXQiOiA2NDcyMzM2MzIwMH0=";

    ASSERT_FALSE(auth_manager.authenticate(scoped_key5, "documents:search", {"coll1"}, empty_params));

    // bad scoped API key
    ASSERT_FALSE(auth_manager.authenticate(" XhsdBdhehdDheruyhvbdhwjhHdhgyeHbfheR", "documents:search", {"coll1"}, empty_params));
    ASSERT_FALSE(auth_manager.authenticate("cXYPvkNKRlQrBzVTEgY4a3FrZfZ2MEs4kFJ6all3eldwM GhKZnRId3Y3TT1RZmxZeYJmaWx0ZXJfYnkiOkJ1aWQ6OElVm1lUVm15SG9ZOHM4NUx2VFk4S2drNHJIMiJ9", "documents:search", {"coll1"}, empty_params));
    ASSERT_FALSE(auth_manager.authenticate("SXZqcVdOWjVNNUVsY3ZiTW9YajQ1QnhrUXJaRzRaS0VhTlFvUmlvQ3gycz1LZXlWeyJmaWx0ZXJfYnkiOiAidXNlcl9pZDoxMDgw In0=", "documents:search", {"coll1"}, empty_params));
}

TEST_F(AuthManagerTest, ValidateBadKeyProperties) {
    nlohmann::json key_obj1;
    key_obj1["description"] = "desc";
    key_obj1["actions"].push_back("*");
    key_obj1["collections"].push_back(1);

    Option<uint32_t> validate_op = api_key_t::validate(key_obj1);
    ASSERT_FALSE(validate_op.ok());
    ASSERT_STREQ("Wrong format for `collections`. It should be an array of string.", validate_op.error().c_str());

    key_obj1["actions"].push_back(1);
    key_obj1["collections"].push_back("*");
    validate_op = api_key_t::validate(key_obj1);
    ASSERT_FALSE(validate_op.ok());
    ASSERT_STREQ("Wrong format for `actions`. It should be an array of string.", validate_op.error().c_str());

    key_obj1["actions"] = 1;
    key_obj1["collections"] = {"*"};
    validate_op = api_key_t::validate(key_obj1);
    ASSERT_FALSE(validate_op.ok());
    ASSERT_STREQ("Wrong format for `actions`. It should be an array of string.", validate_op.error().c_str());

    nlohmann::json key_obj2;
    key_obj2["description"] = "desc";
    key_obj2["actions"] = {"*"};
    key_obj2["collections"] = {"foobar"};
    key_obj2["expires_at"] = -100;

    validate_op = api_key_t::validate(key_obj2);
    ASSERT_FALSE(validate_op.ok());
    ASSERT_STREQ("Wrong format for `expires_at`. It should be an unsigned integer.", validate_op.error().c_str());

    key_obj2["expires_at"] = "expiry_ts";

    validate_op = api_key_t::validate(key_obj2);
    ASSERT_FALSE(validate_op.ok());
    ASSERT_STREQ("Wrong format for `expires_at`. It should be an unsigned integer.", validate_op.error().c_str());

    key_obj2["expires_at"] = 1606539880;

    validate_op = api_key_t::validate(key_obj2);
    ASSERT_TRUE(validate_op.ok());

    // check for valid value
    nlohmann::json key_obj3;
    key_obj3["description"] = "desc";
    key_obj3["actions"] = {"*"};
    key_obj3["collections"] = {"foobar"};
    key_obj3["value"] = 100;

    validate_op = api_key_t::validate(key_obj3);
    ASSERT_FALSE(validate_op.ok());
    ASSERT_STREQ("Key value must be a string.", validate_op.error().c_str());
}