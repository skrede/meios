#ifndef HPP_GUARD_MEIOS_XACRO_SCOPED_EXCHANGE_H
#define HPP_GUARD_MEIOS_XACRO_SCOPED_EXCHANGE_H

#include <utility>

namespace meios
{

namespace detail
{

// Resolve bookkeeping is undone by destructors because an evaluator may catch an exception raised
// beneath a scope read and keep expanding.
template<typename T>
class scoped_exchange
{
public:
    scoped_exchange(T &slot, T replacement)
            : m_kept(std::exchange(slot, std::move(replacement)))
            , m_slot(slot)
    {
    }

    scoped_exchange(const scoped_exchange &) = delete;
    scoped_exchange &operator=(const scoped_exchange &) = delete;

    ~scoped_exchange()
    {
        m_slot = std::move(m_kept);
    }

private:
    T m_kept;
    T &m_slot;
};

template<typename Sequence>
class scoped_push
{
public:
    scoped_push(Sequence &sequence, typename Sequence::value_type element)
            : m_sequence(sequence)
    {
        m_sequence.push_back(std::move(element));
    }

    scoped_push(const scoped_push &) = delete;
    scoped_push &operator=(const scoped_push &) = delete;

    ~scoped_push()
    {
        m_sequence.pop_back();
    }

private:
    Sequence &m_sequence;
};

}

}

#endif
