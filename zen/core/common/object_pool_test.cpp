/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <memory>

#include <catch2/catch.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/common/misc.hpp>
#include <zen/core/common/object_pool.hpp>

namespace zen {

class PooledObject {
  public:
    explicit PooledObject(std::string&& data) : data_{data} {}
    std::string& data() { return data_; }

  private:
    std::string data_;
};

class PooledObjectConsumer {
  public:
    PooledObjectConsumer() {
        handle_.reset(pool_.acquire());
        if (!handle_) {
            std::string random_string{get_random_alpha_string(10)};
            handle_ = std::make_unique<PooledObject>(std::string(random_string));
        }
    }

    ~PooledObjectConsumer() {
        if (handle_) {
            pool_.add(handle_.release());
        }
    }

    PooledObject* object() { return handle_.get(); }

    void reset() { handle_.reset(); }

    //! \brief Exposes pool
    static const ObjectPool<PooledObject>& pool() { return pool_; }

  private:
    std::unique_ptr<PooledObject> handle_;
    static thread_local ObjectPool<PooledObject> pool_;
};

thread_local ObjectPool<PooledObject> PooledObjectConsumer::pool_{};

TEST_CASE("Object pool", "[memory]") {
    std::string id1;
    std::string id2;

    {
        // Create two objects that return their handles to the pool on destruction
        PooledObjectConsumer obj1{};
        REQUIRE(obj1.object() != nullptr);
        id1 = obj1.object()->data();
        PooledObjectConsumer obj2{};
        REQUIRE(obj2.object() != nullptr);
        id2 = obj2.object()->data();
        REQUIRE(obj1.pool().empty());
    }
    {
        // Create one object which pulls one element from the pool and does not return it
        PooledObjectConsumer obj1{};
        REQUIRE(obj1.object() != nullptr);
        REQUIRE(id1 == obj1.object()->data());  // Pool is a stack hence LIFO
        REQUIRE_FALSE(obj1.pool().empty());     // Still one object to pull
        REQUIRE(obj1.pool().size() == 1);
        obj1.reset();  // Release the pointer (not returned to the pool
    }
    {
        // Create another object which pulls the remaining element from the pool and does not return it
        PooledObjectConsumer obj1{};
        REQUIRE(obj1.object() != nullptr);
        REQUIRE(id2 == obj1.object()->data());
        REQUIRE(obj1.pool().empty());  // Nothing left in the pool
        obj1.reset();                  // Release the pointer (not returned to the pool)
    }
    {
        // Create the third instance which has nothing to pull from pool
        PooledObjectConsumer obj1{};
        REQUIRE(obj1.object() != nullptr);
        REQUIRE_FALSE(id1 == obj1.object()->data());
        REQUIRE_FALSE(id2 == obj1.object()->data());
    }
}
}  // namespace zen
