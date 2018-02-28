#include <gtest/gtest.h>
extern "C" {
#include "server/server.c"
#include "server/inqueue.c"
}

class inqueueTest : public ::testing::Test
{
public:
    void SetUp()
    {
        inq = inq_create();
        ASSERT_TRUE(inq != 0);
    }

    void TearDown()
    {
        inq_destroy(inq);
    }

    struct inqueue *inq;
};

TEST_F(inqueueTest, DefaultEmpty)
{
    ASSERT_EQ(0, inq_size(inq));
}

TEST_F(inqueueTest, PushOne)
{
    char s[] = "abcd";
    ASSERT_EQ(0, inq_push(inq, 123, s, strlen(s)));
    ASSERT_EQ(0, inq->head);
    ASSERT_EQ(1, inq->tail);
    ASSERT_EQ(123, inq->cmd[0].type);
    ASSERT_EQ(strlen(s), inq->cmd[0].size);
    ASSERT_EQ(s, (char *)inq->cmd[0].data);
}

TEST_F(inqueueTest, PushToFull)
{
    int i;
    char s[] = "abcd";
    for (i = 0; i < SVP_CMDQUEUE_SIZE; i++)
        ASSERT_EQ(0, inq_push(inq, 123, s, strlen(s))) << "error insert " << i;
    ASSERT_EQ(-1, inq_push(inq, 123, s, strlen(s)));
}

TEST_F(inqueueTest, Peek)
{
    char s[] = "abcd";
    struct in_cmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    inq_push(inq, 123, s, strlen(s));
    ASSERT_EQ(0, inq_peek(inq, &cmd));
    ASSERT_EQ(123, cmd.type);
    ASSERT_EQ(s, (char *)cmd.data);
    ASSERT_EQ(strlen(s), cmd.size);
    ASSERT_EQ(1, inq_size(inq));
}

TEST_F(inqueueTest, PopOne)
{
    char s[] = "abcd";
    struct in_cmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    inq_push(inq, 123, s, strlen(s));
    ASSERT_EQ(1, inq_pop(inq, &cmd));
    ASSERT_EQ(123, cmd.type);
    ASSERT_EQ(s, (char *)cmd.data);
    ASSERT_EQ(strlen(s), cmd.size);
}

TEST_F(inqueueTest, PopAll)
{
    char s[] = "abcd";
    struct in_cmd cmd[4];
    memset(&cmd, 0, sizeof(cmd));
    inq_push(inq, 10, s, strlen(s));
    inq_push(inq, 20, s, strlen(s));
    ASSERT_EQ(2, inq_popn(inq, cmd, 4));
    ASSERT_EQ(10, cmd[0].type);
    ASSERT_EQ(20, cmd[1].type);
}

TEST_F(inqueueTest, PopEmpty)
{
    char s[] = "abcd";
    struct in_cmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    ASSERT_EQ(0, inq_pop(inq, &cmd));
}

TEST_F(inqueueTest, PopAllNonPositive)
{
    char s[] = "abcd";
    struct in_cmd cmd[4];
    memset(&cmd, 0, sizeof(cmd));
    inq_push(inq, 10, s, strlen(s));
    inq_push(inq, 20, s, strlen(s));
    ASSERT_EQ(0, inq_popn(inq, cmd, 0));
    ASSERT_EQ(0, inq_popn(inq, cmd, -3));
    ASSERT_EQ(2, inq_size(inq));
}

TEST_F(inqueueTest, Clear)
{
    char s[] = "abcd";
    inq_push(inq, 123, s, strlen(s));
    inq_push(inq, 123, s, strlen(s));
    inq_clear(inq, 0);
    ASSERT_EQ(0, inq->head);
    ASSERT_EQ(0, inq->tail);
}

TEST_F(inqueueTest, PopFullQueue)
{
    int i;
    char s[] = "abcd";
    struct in_cmd cmd;
    for (i = 0; i < SVP_CMDQUEUE_SIZE; i++)
        inq_push(inq, 123, s, strlen(s));
    ASSERT_EQ(1, inq_pop(inq, &cmd));
    ASSERT_EQ(SVP_CMDQUEUE_SIZE - 1, inq_size(inq));
}
