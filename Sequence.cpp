/**
 * @file Sequence.cpp
 */

#include "Sequence.h"


VectorSequence g_sequences;


Sequence::Sequence(uint64_t a_start, uint64_t a_step):
    m_counter(a_start),
    m_step(a_step)
{
}

Sequence::Sequence(Sequence&& a_seq):
    m_counter(a_seq.m_counter.load()),
    m_step(a_seq.m_step)
{
}

uint64_t Sequence::next()
{
    return m_counter.fetch_add(m_step, std::memory_order_relaxed);
}
