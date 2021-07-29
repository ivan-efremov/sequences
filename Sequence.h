/**
 * @file Sequence.h
 */

#pragma once

#include <cstdint>
#include <vector>
#include <atomic>


/**
 * @short Числовая последовательность.
 *        a_start - начальное значение последовательности
 *        a_step - шаг последовательности
 */
class Sequence
{
public:
             Sequence(uint64_t a_start, uint64_t a_step);
             Sequence(Sequence&& a_seq);
    uint64_t next(); ///< получить след. значение последовательности
private:
    std::atomic_uint64_t m_counter;
    const uint64_t       m_step;
};

/// Вектор последовательности
typedef std::vector<Sequence> VectorSequence;


extern VectorSequence g_sequences;
