#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "conf_search.hpp"

namespace osprey::kstar {

// Port of Java: edu.duke.cs.osprey.astar.conf.ConfSearchCache
//
// Notes:
// - Java uses SoftReference + GC behavior; C++ has no GC.
//   We mimic semantics with a weak_ptr "soft" reference and a shared_ptr "strong" reference.
// - Tests call Entry::clearRefs() to force re-instantiation; that works the same here.
class ConfSearchCache final {
public:
    using Factory = std::function<std::unique_ptr<ConfSearch>()>;

    class EntryImpl; // forward declaration (defined below)

    class Entry final : public ConfSearch {
    public:
        Entry() = default;
        explicit Entry(std::shared_ptr<EntryImpl> impl);

        void clearRefs();
        [[nodiscard]] bool isProtected() const noexcept;

        std::uint64_t getNumConformations() const override;
        std::optional<ScoredConf> nextConf() override;

    private:
        std::shared_ptr<EntryImpl> impl_;
    };

    class EntryImpl final : public ConfSearch {
    public:
        EntryImpl(ConfSearchCache& cache, Factory factory)
            : cache_(cache)
            , factory_(std::move(factory)) {
            (void)getOrMakeTree();
        }

        void clearRefs() {
            soft_ref_.reset();
            strong_ref_.reset();
        }

        [[nodiscard]] bool isProtected() const noexcept {
            return static_cast<bool>(strong_ref_);
        }

        std::uint64_t getNumConformations() const override {
            return getOrMakeTree()->getNumConformations();
        }

        std::optional<ScoredConf> nextConf() override {

            // no more confs? don't bother with the tree
            if (is_exhausted_) {
                return std::nullopt;
            }

            // get the next conf
            auto conf = getOrMakeTree()->nextConf();

            // and keep track of which conf we're on
            if (!conf) {
                is_exhausted_ = true;

                // and let GC take the tree (in C++: drop references)
                clearRefs();

            } else {
                num_confs_++;
            }

            return conf;
        }

    private:
        ConfSearchCache& cache_;
        Factory factory_;

        std::uint64_t num_confs_ = 0;
        bool is_exhausted_ = false;

        std::shared_ptr<ConfSearch> strong_ref_;
        std::weak_ptr<ConfSearch> soft_ref_;

        std::shared_ptr<ConfSearch> getOrMakeTree() const {
            // Java mutates cache state even in logically-const methods; allow that here too.
            return const_cast<EntryImpl*>(this)->getOrMakeTreeMutable();
        }

        std::shared_ptr<ConfSearch> getOrMakeTreeMutable() {

            // check the "soft" ref to see if we still have a tree
            if (!soft_ref_.expired()) {
                if (auto tree = soft_ref_.lock()) {
                    markUsed(tree);
                    return tree;
                }
            }

            // don't have a tree, make a new one
            std::unique_ptr<ConfSearch> made = factory_();
            if (!made) {
                throw std::runtime_error("ConfSearchCache factory returned null");
            }
            auto tree = std::shared_ptr<ConfSearch>(std::move(made));

            // and put it back to where it was
            for (std::uint64_t i = 0; i < num_confs_; i++) {
                (void)tree->nextConf();
            }

            // recently-used entries are always protected from collection
            soft_ref_ = tree;
            markUsed(tree);

            return tree;
        }

        void markUsed(const std::shared_ptr<ConfSearch>& tree) {

            // protect by holding a strong reference
            strong_ref_ = tree;

            // if capacity restrictions are turned on, manage recency and protections
            if (cache_.min_capacity_.has_value()) {
                cache_.touch(*this);
            }
        }

        friend class ConfSearchCache;
    };

    explicit ConfSearchCache(std::optional<std::size_t> minCapacity)
        : min_capacity_(minCapacity) {}

    Entry make(Factory factory) {
        auto impl = std::make_shared<EntryImpl>(*this, std::move(factory));
        return Entry(std::move(impl));
    }

private:
    std::optional<std::size_t> min_capacity_;

    // LRU tracking of EntryImpl pointers; Java uses LinkedHashSet of Entry.
    // Here we keep insertion order in a list + a map for O(1) remove/touch.
    std::list<EntryImpl*> recent_;
    std::unordered_map<EntryImpl*, std::list<EntryImpl*>::iterator> recent_pos_;

    void touch(EntryImpl& entry) {
        // remove if present
        if (auto it = recent_pos_.find(&entry); it != recent_pos_.end()) {
            recent_.erase(it->second);
            recent_pos_.erase(it);
        }

        // push to back as most-recent
        recent_.push_back(&entry);
        recent_pos_[&entry] = std::prev(recent_.end());

        // if over capacity, expose least recently used to "collection" (drop strong ref)
        if (recent_.size() > min_capacity_.value()) {
            EntryImpl* lru = recent_.front();
            recent_.pop_front();
            recent_pos_.erase(lru);
            lru->strong_ref_.reset();
        }
    }
};

// ---- Entry inline definitions (after EntryImpl is complete) ----

inline ConfSearchCache::Entry::Entry(std::shared_ptr<EntryImpl> impl)
    : impl_(std::move(impl)) {}

inline void ConfSearchCache::Entry::clearRefs() {
    impl_->clearRefs();
}

inline bool ConfSearchCache::Entry::isProtected() const noexcept {
    return impl_->isProtected();
}

inline std::uint64_t ConfSearchCache::Entry::getNumConformations() const {
    return impl_->getNumConformations();
}

inline std::optional<ScoredConf> ConfSearchCache::Entry::nextConf() {
    return impl_->nextConf();
}

} // namespace osprey::kstar


