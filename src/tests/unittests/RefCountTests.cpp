// Copyright 2021 The GPGMM Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "gpgmm/common/RefCount.h"

using namespace gpgmm;

class DummyObject : public RefCounted {
  public:
    DummyObject() : RefCounted(0) {
    }
};

TEST(RefCountTests, IncrementDecrement) {
    RefCounted refcount(2);
    EXPECT_FALSE(refcount.Unref());
    EXPECT_EQ(refcount.GetRefCount(), 1);

    EXPECT_TRUE(refcount.HasOneRef());

    EXPECT_TRUE(refcount.Unref());
    EXPECT_EQ(refcount.GetRefCount(), 0);

    EXPECT_FALSE(refcount.HasOneRef());
}

// Verify semantics of attach, detach, and aquire.
TEST(RefCountTests, ScopedRefAttachDetach) {
    ScopedRef<DummyObject> firstRef(new DummyObject());
    EXPECT_EQ(firstRef->GetRefCount(), 1);

    ScopedRef<DummyObject> secondRef = firstRef;
    EXPECT_EQ(secondRef->GetRefCount(), 2);

    DummyObject* ptr = firstRef.Detach();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->GetRefCount(), 2);

    ScopedRef<DummyObject> firstRefAgain;
    firstRefAgain.Attach(ptr);
    EXPECT_EQ(ptr->GetRefCount(), 2);

    EXPECT_TRUE(firstRefAgain == secondRef);
    EXPECT_FALSE(firstRef == firstRefAgain);

    ScopedRef<DummyObject> firstRefAgainAquired =
        ScopedRef<DummyObject>::Acquire(firstRefAgain.Detach());
    EXPECT_EQ(firstRefAgainAquired->GetRefCount(), 2);

    EXPECT_TRUE(firstRefAgainAquired == secondRef);
    EXPECT_FALSE(firstRef == firstRefAgainAquired);

    firstRefAgainAquired = nullptr;
    EXPECT_EQ(secondRef->GetRefCount(), 1);

    secondRef = nullptr;
    EXPECT_TRUE(secondRef == nullptr);
}

TEST(RefCountTests, ScopedRefSafeRelease) {
    ScopedRef<DummyObject> ref(new DummyObject());
    ref.~ScopedRef();
    EXPECT_EQ(ref.Get(), nullptr);

    ref.~ScopedRef();
    EXPECT_EQ(ref.Get(), nullptr);
}

// Verify move semantics only transfers ownership.
TEST(RefCountTests, ScopedRefMove) {
    DummyObject* obj = new DummyObject();
    ScopedRef<DummyObject> firstRef(obj);
    EXPECT_EQ(firstRef->GetRefCount(), 1);

    firstRef->Ref();

    ScopedRef<DummyObject> secondRef(std::move(firstRef));
    EXPECT_EQ(firstRef, nullptr);
    EXPECT_EQ(secondRef->GetRefCount(), 2);
    EXPECT_EQ(secondRef.Get(), obj);
}
