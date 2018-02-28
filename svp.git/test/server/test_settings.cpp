extern "C" {
#include "settings.h"
}
#include <gtest/gtest.h>
#include <stdlib.h>
#include <stdio.h>

static const int VALUE_INT = 456;
static const int DEFAULT_INT = 123;
static const char VALUE_STR[] = "hello";
static char DEFAULT_STR[] = "default";
static const char PROP[] = "abc";
static const char SECTION_PROP[] = "general.abc";

class settingsTest : public ::testing::Test
{
public:
    void SetUp()
    {
    }

    void TearDown()
    {
        settings_clear(GS);
    }
};

TEST_F(settingsTest, ReadNonExistInt)
{
    int i;
    settings_read_int(GS, PROP, &i, DEFAULT_INT);
    ASSERT_EQ(DEFAULT_INT, i);
}

TEST_F(settingsTest, WriteReadInt)
{
    int i;
    settings_write_int(GS, PROP, VALUE_INT);
    settings_read_int(GS, PROP, &i, DEFAULT_INT);
    ASSERT_EQ(VALUE_INT, i);
}

TEST_F(settingsTest, ReadNonExistString)
{
    char s[20] = {0};
    settings_read_str(GS, PROP, s, sizeof(s), DEFAULT_STR);
    ASSERT_STREQ(DEFAULT_STR, s);
}

TEST_F(settingsTest, WriteReadString)
{
    char s[20] = {0};
    settings_write_str(GS, PROP, VALUE_STR);
    settings_read_str(GS, PROP, s, sizeof(s), DEFAULT_STR);
    ASSERT_STREQ(VALUE_STR, s);
}

TEST_F(settingsTest, Exist)
{
    ASSERT_FALSE(settings_exist(GS, PROP));
    settings_write_int(GS, PROP, VALUE_INT);
    ASSERT_TRUE(settings_exist(GS, PROP));
}

TEST_F(settingsTest, Remove)
{
    int i;
    settings_write_int(GS, PROP, VALUE_INT);
    settings_remove(GS, PROP);

    ASSERT_FALSE(settings_exist(GS, PROP));

    settings_read_int(GS, PROP, &i, DEFAULT_INT);
    ASSERT_EQ(123, i);
}

TEST_F(settingsTest, ExistWithSection)
{
    ASSERT_FALSE(settings_exist(GS, SECTION_PROP));
    settings_write_int(GS, SECTION_PROP, VALUE_INT);
    ASSERT_TRUE(settings_exist(GS, SECTION_PROP));
}

TEST_F(settingsTest, WriteReadIntWithSection)
{
    int i;
    settings_write_int(GS, SECTION_PROP, VALUE_INT);
    settings_read_int(GS, SECTION_PROP, &i, DEFAULT_INT);
    ASSERT_EQ(VALUE_INT, i);
}

TEST_F(settingsTest, WriteReadStringWithSection)
{
    char s[20] = {0};
    settings_write_str(GS, SECTION_PROP, VALUE_STR);
    settings_read_str(GS, SECTION_PROP, s, sizeof(s), DEFAULT_STR);
    ASSERT_STREQ(VALUE_STR, s);
}

TEST_F(settingsTest, UpdateInsertOnly)
{
    char iniData[] = "[group1]\n"
                     "key1=\"bla\"\n"
                     "key2=-3\n";
    char s[20] = {0};
    int i;
    settings_update(GS, iniData, sizeof(iniData));
    settings_read_str(GS, "group1.key1", s, sizeof(s), DEFAULT_STR);
    settings_read_int(GS, "group1.key2", &i, DEFAULT_INT);
    ASSERT_STREQ("bla", s);
    ASSERT_EQ(-3, i);
}

TEST_F(settingsTest, UpdateWithOverwrite)
{
    char iniData[] = "[group1]\n"
                     "key1=\"bla\"\n"
                     "key2=-3\n";
    char s[20] = {0};
    int i;
    settings_write_str(GS, "group1.key1", "value1");
    settings_write_str(GS, "group1.key2", "value2");
    settings_update(GS, iniData, sizeof(iniData));
    settings_read_str(GS, "group1.key1", s, sizeof(s), DEFAULT_STR);
    settings_read_int(GS, "group1.key2", &i, DEFAULT_INT);
    ASSERT_STREQ("bla", s);
    ASSERT_EQ(-3, i);
}

TEST_F(settingsTest, Dump)
{
    char expected[] = "\n"
                      "[group1]\n"
                      "key1                           = value1\n"
                      "key2                           = value2\n"
                      "\n\n";
    char iniData[1024];
    char fname[20] = "/tmp/svp.XXXXXX";
    mkstemp(fname);

    settings_write_str(GS, "group1.key1", "value1");
    settings_write_str(GS, "group1.key2", "value2");
    settings_dump(GS, iniData, sizeof(iniData));
    ASSERT_STREQ(expected, iniData);
}

TEST_F(settingsTest, SaveRestore)
{
    char fname[20] = "/tmp/svp.XXXXXX";
    int i;
    char s[100];
    mkstemp(fname);

    settings_write_int(GS, PROP, VALUE_INT);
    settings_write_str(GS, SECTION_PROP, VALUE_STR);

    settings_save(GS, fname);
    settings_clear(GS);
    settings_restore(GS, fname);

    settings_read_int(GS, PROP, &i, DEFAULT_INT);
    ASSERT_EQ(VALUE_INT, i);
    settings_read_str(GS, SECTION_PROP, s, sizeof(s), DEFAULT_STR);
    ASSERT_STREQ(VALUE_STR, s);
}
