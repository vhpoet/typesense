#include <gtest/gtest.h>
#include "index.h"
#include <vector>

TEST(IndexTest, ScrubReindexDoc) {
    std::unordered_map<std::string, field> search_schema;
    search_schema.emplace("title", field("title", field_types::STRING, false));
    search_schema.emplace("points", field("points", field_types::INT32, false));
    search_schema.emplace("cast", field("cast", field_types::STRING_ARRAY, false));
    search_schema.emplace("movie", field("movie", field_types::BOOL, false));

    Index index("index", search_schema, {}, {});
    nlohmann::json old_doc;
    old_doc["id"] = "1";
    old_doc["title"] = "One more thing.";
    old_doc["points"] = 100;
    old_doc["cast"] = {"John Wick", "Jeremy Renner"};
    old_doc["movie"] = true;

    // all fields remain same

    nlohmann::json update_doc1, del_doc1;
    update_doc1 = old_doc;
    del_doc1 = old_doc;

    index.scrub_reindex_doc(update_doc1, del_doc1, old_doc);
    ASSERT_EQ(1, del_doc1.size());
    ASSERT_STREQ("1", del_doc1["id"].get<std::string>().c_str());

    // when only some fields are updated

    nlohmann::json update_doc2, del_doc2;
    update_doc2["id"] = "1";
    update_doc2["points"] = 100;
    update_doc2["cast"] = {"Jack"};

    del_doc2 = update_doc2;

    index.scrub_reindex_doc(update_doc2, del_doc2, old_doc);
    ASSERT_EQ(2, del_doc2.size());
    ASSERT_STREQ("1", del_doc2["id"].get<std::string>().c_str());
    std::vector<std::string> cast = del_doc2["cast"].get<std::vector<std::string>>();
    ASSERT_EQ(1, cast.size());
    ASSERT_STREQ("Jack", cast[0].c_str());

    // containing fields not part of search schema

    nlohmann::json update_doc3, del_doc3;
    update_doc3["id"] = "1";
    update_doc3["title"] = "The Lawyer";
    update_doc3["foo"] = "Bar";

    del_doc3 = update_doc3;
    index.scrub_reindex_doc(update_doc3, del_doc3, old_doc);
    ASSERT_EQ(3, del_doc3.size());
    ASSERT_STREQ("1", del_doc3["id"].get<std::string>().c_str());
    ASSERT_STREQ("The Lawyer", del_doc3["title"].get<std::string>().c_str());
    ASSERT_STREQ("Bar", del_doc3["foo"].get<std::string>().c_str());
}

TEST(IndexTest, PointInPolygon180thMeridian) {
    // somewhere in far eastern russia
    GeoCoord verts[3] = {
        {67.63378886620751, 179.87924212491276},
        {67.6276069384328, -179.8364939577639},
        {67.5749950728145, 179.94421673458666}
    };

    Geofence poly1{3, verts};
    double offset = Index::transform_for_180th_meridian(poly1);

    GeoCoord point1 = {67.61896440098865, 179.9998420463554};
    GeoCoord point2 = {67.6332378896519, 179.88828622883355};
    GeoCoord point3 = {67.62717271243574, -179.85954137693625};

    GeoCoord point4 = {67.65842784263879, -179.79268650445243};
    GeoCoord point5 = {67.62016647245217, 179.83764198608083};

    Index::transform_for_180th_meridian(point1, offset);
    Index::transform_for_180th_meridian(point2, offset);
    Index::transform_for_180th_meridian(point3, offset);
    Index::transform_for_180th_meridian(point4, offset);
    Index::transform_for_180th_meridian(point5, offset);

    ASSERT_TRUE(Index::is_point_in_polygon(poly1, point1));
    ASSERT_TRUE(Index::is_point_in_polygon(poly1, point2));
    ASSERT_TRUE(Index::is_point_in_polygon(poly1, point3));

    ASSERT_FALSE(Index::is_point_in_polygon(poly1, point4));
    ASSERT_FALSE(Index::is_point_in_polygon(poly1, point5));
}