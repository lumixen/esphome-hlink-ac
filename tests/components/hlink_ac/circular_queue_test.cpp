#include "common.h"

namespace esphome::hlink_ac::testing {

TEST(CircularRequestsQueueTest, InitiallyEmpty) {
  CircularRequestsQueue queue;
  EXPECT_TRUE(queue.is_empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_FALSE(queue.is_full());
}

TEST(CircularRequestsQueueTest, DequeueFromEmptyReturnsNullptr) {
  CircularRequestsQueue queue;
  EXPECT_EQ(queue.dequeue(), nullptr);
}

TEST(CircularRequestsQueueTest, EnqueueAndDequeueSingle) {
  CircularRequestsQueue queue;
  auto req = make_unique<HlinkRequest>(
      HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
  EXPECT_EQ(queue.enqueue(std::move(req)), 1);
  EXPECT_FALSE(queue.is_empty());
  EXPECT_EQ(queue.size(), 1);

  auto result = queue.dequeue();
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result->request_frame.type, HlinkRequestFrame::Type::MT);
  EXPECT_EQ(result->request_frame.p.address, 0x0000);
  EXPECT_TRUE(queue.is_empty());
  EXPECT_EQ(queue.size(), 0);
}

TEST(CircularRequestsQueueTest, FifoOrder) {
  CircularRequestsQueue queue;
  for (int i = 0; i < 5; i++) {
    auto req = make_unique<HlinkRequest>(
        HlinkRequestFrame{HlinkRequestFrame::Type::ST, {static_cast<uint16_t>(i)}});
    queue.enqueue(std::move(req));
  }
  EXPECT_EQ(queue.size(), 5);

  for (int i = 0; i < 5; i++) {
    auto result = queue.dequeue();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->request_frame.p.address, i);
  }
  EXPECT_TRUE(queue.is_empty());
}

TEST(CircularRequestsQueueTest, WrapAround) {
  CircularRequestsQueue queue;
  // Fill the queue except one slot
  for (int i = 0; i < REQUESTS_QUEUE_SIZE - 1; i++) {
    auto req = make_unique<HlinkRequest>(
        HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
    queue.enqueue(std::move(req));
  }
  EXPECT_FALSE(queue.is_full());

  // Add one more to fill it
  auto req = make_unique<HlinkRequest>(
      HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
  queue.enqueue(std::move(req));
  EXPECT_TRUE(queue.is_full());

  // Remove half
  for (int i = 0; i < REQUESTS_QUEUE_SIZE / 2; i++) {
    queue.dequeue();
  }
  EXPECT_FALSE(queue.is_full());
  EXPECT_EQ(queue.size(), REQUESTS_QUEUE_SIZE / 2);

  // Add more to trigger wrap-around
  for (int i = 0; i < REQUESTS_QUEUE_SIZE / 2; i++) {
    auto r = make_unique<HlinkRequest>(
        HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
    queue.enqueue(std::move(r));
  }
  EXPECT_TRUE(queue.is_full());

  // Dequeue all
  for (int i = 0; i < REQUESTS_QUEUE_SIZE; i++) {
    auto result = queue.dequeue();
    EXPECT_NE(result, nullptr);
  }
  EXPECT_TRUE(queue.is_empty());
}

TEST(CircularRequestsQueueTest, EnqueueWhenFullReturnsMinusOne) {
  CircularRequestsQueue queue;
  for (int i = 0; i < REQUESTS_QUEUE_SIZE; i++) {
    auto req = make_unique<HlinkRequest>(
        HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
    queue.enqueue(std::move(req));
  }
  EXPECT_TRUE(queue.is_full());

  auto req = make_unique<HlinkRequest>(
      HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
  EXPECT_EQ(queue.enqueue(std::move(req)), -1);
}

TEST(CircularRequestsQueueTest, SizeTracking) {
  CircularRequestsQueue queue;
  EXPECT_EQ(queue.size(), 0);

  auto req1 = make_unique<HlinkRequest>(
      HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0000}});
  queue.enqueue(std::move(req1));
  EXPECT_EQ(queue.size(), 1);

  auto req2 = make_unique<HlinkRequest>(
      HlinkRequestFrame{HlinkRequestFrame::Type::MT, {0x0001}});
  queue.enqueue(std::move(req2));
  EXPECT_EQ(queue.size(), 2);

  queue.dequeue();
  EXPECT_EQ(queue.size(), 1);

  queue.dequeue();
  EXPECT_EQ(queue.size(), 0);
}

}  // namespace esphome::hlink_ac::testing