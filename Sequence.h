/**
 * @file Sequence.h
 */

#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <map>


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
    uint64_t             next(); ///< получить след. значение последовательности
private:
    std::atomic_uint64_t m_counter;
    const uint64_t       m_step;
};

typedef std::shared_ptr<Sequence>  PSequence;
typedef std::map<int, PSequence>   MapPSequence;


/**
 * @short Фабрика последовательности, парсинг строки последовательности.
 *        seqN xxxx yyyy
 *        где хxxx - начальное значение, yyyy - шаг подпоследовательности
 */
class SequenceFactory
{
public:
    void                createSeq(const std::string& a_line);
    std::string         getOneRowSeq();
private:
    static MapPSequence s_sequences;
};
