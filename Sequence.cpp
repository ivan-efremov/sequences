/**
 * @file Sequence.cpp
 */

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "Sequence.h"

/**
 * class Sequence.
 */
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


/**
 * class SequenceFactory
 */
void SequenceFactory::createSeq(const std::string& a_line)
{
    int seqId = 0;
    uint64_t start = 0UL, step = 0UL;
    if(sscanf(a_line.c_str(), "seq%d %lu %lu", &seqId, &start, &step) != 3) {
        throw std::runtime_error("Bad request");
    }
    if(!(seqId >= 1 && seqId <= 3)) {
        throw std::runtime_error("Sequence number must be in range [1;3]");
    }
    if(start == 0UL) {
        throw std::runtime_error("Start parameter 'xxxx' not a valid");
    }
    if(step == 0ULL) {
        throw std::runtime_error("Step parameter 'yyyy' not a valid");
    }
#ifdef DEBUG
    std::cout << "createSeq: " << seqId << ' ' << start << ' ' << step << std::endl;
#endif
    auto res = m_sequences.emplace(
        seqId, std::make_shared<Sequence>(start, step)
    );
    if(!res.second) {
        throw std::runtime_error("Sequence already is exists");
    }
}

std::string SequenceFactory::getOneRowSeq()
{
    std::string rowseq;
    rowseq.reserve(128);
    for(auto &vt : m_sequences) {
        PSequence seq(vt.second);
        rowseq += std::to_string(seq->next());
        rowseq += '\t';
    }
    if(!rowseq.empty()) {
        rowseq.pop_back();
    }
#ifdef DEBUG
    std::cout << "getOneRowSeq: " << rowseq << std::endl;
    sleep(1);
#endif
    return rowseq;
}
