/*
 * (C) 2014 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>

// This simple implementation of readers-writer lock starves the writers,
// but we don't call it often enough to care.
//
// TODO: replace this class with std::shared_lock when it's supported in visual studio (or use boost)
class SharedMutex
{
public:
    SharedMutex() = default;
    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;

    void LockShared() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_bExclusive) {
            m_unlocked.wait(lock, [&] { return !m_bExclusive; });
        }
        ASSERT(m_nSharedCount >= 0);
        m_nSharedCount++;
    }

    void UnlockShared() {
        bool bNotify;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ASSERT(m_nSharedCount > 0);
            m_nSharedCount--;
            bNotify = (m_nSharedCount == 0);
        }
        if (bNotify) {
            m_unlocked.notify_all();
        }
    }

    void LockExclusive() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_bExclusive || m_nSharedCount != 0) {
            m_unlocked.wait(lock, [&] { return !m_bExclusive && m_nSharedCount == 0; });
        }
        m_bExclusive = true;
    }

    void UnlockExclusive() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            ASSERT(m_bExclusive);
            ASSERT(m_nSharedCount == 0);
            m_bExclusive = false;
        }
        m_unlocked.notify_all();
    }

    class SharedLock final
    {
    public:
        SharedLock() = delete;
        SharedLock(const SharedLock&) = delete;
        SharedLock& operator=(const SharedLock&) = delete;

        explicit SharedLock(SharedMutex* pMutex) : m_pMutex(pMutex) {
            m_pMutex->LockShared();
        }

        SharedLock(SharedLock&& other) : m_pMutex(other.m_pMutex) {
            other.m_pMutex = nullptr;
        }

        ~SharedLock() {
            if (m_pMutex) {
                m_pMutex->UnlockShared();
            }
        }

    private:
        SharedMutex* m_pMutex;
    };

    class ExclusiveLock final
    {
    public:
        ExclusiveLock() = delete;
        ExclusiveLock(const ExclusiveLock&) = delete;
        ExclusiveLock& operator=(const ExclusiveLock&) = delete;

        explicit ExclusiveLock(SharedMutex* pMutex) : m_pMutex(pMutex) {
            m_pMutex->LockExclusive();
        }

        ExclusiveLock(ExclusiveLock&& other) : m_pMutex(other.m_pMutex) {
            other.m_pMutex = nullptr;
        }

        ~ExclusiveLock() {
            if (m_pMutex) {
                m_pMutex->UnlockExclusive();
            }
        }

    private:
        SharedMutex* m_pMutex;
    };

    inline SharedLock GetSharedLock() {
        return SharedLock(this);
    }

    inline ExclusiveLock GetExclusiveLock() {
        return ExclusiveLock(this);
    }

protected:
    int m_nSharedCount = 0;
    bool m_bExclusive = false;
    std::mutex m_mutex;
    std::condition_variable m_unlocked;
};
