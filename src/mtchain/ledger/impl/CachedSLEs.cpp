//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/ledger/CachedSLEs.h>
#include <vector>

namespace mtchain {

void
CachedSLEs::expire()
{
    std::vector<
        std::shared_ptr<void const>> trash;
    {
        auto const expireTime =
            map_.clock().now() - timeToLive_;
        std::lock_guard<
            std::mutex> lock(mutex_);
        for (auto iter = map_.chronological.begin();
            iter != map_.chronological.end(); ++iter)
        {
            if (iter.when() > expireTime)
                break;
            if (iter->second.unique())
            {
                trash.emplace_back(
                    std::move(iter->second));
                iter = map_.erase(iter);
            }
        }
    }
}

double
CachedSLEs::rate() const
{
    std::lock_guard<
        std::mutex> lock(mutex_);
    auto const tot = hit_ + miss_;
    if (tot == 0)
        return 0;
    return double(hit_) / tot;
}

} //
