/**
 * @file main.cpp
 */

#include <iostream>
#include <string>
#include <vector>
#include <atomic>



class Sequence
{
public:
    Sequence(uint64_t a_start, uint64_t a_step):
        m_counter(a_start), m_step(a_step)
    {
    }
    Sequence(Sequence&& a_seq):
        m_counter(a_seq.m_counter.load()), m_step(a_seq.m_step)
    {
    }
    inline uint64_t next() {
        return m_counter.fetch_add(m_step, std::memory_order_relaxed);
    }
private:
    std::atomic_uint64_t m_counter;
    const uint64_t       m_step;
};


std::vector<Sequence> g_sequences;


int main(int argc, const char *argv[])
{
    g_sequences.emplace_back(1, 2);
    g_sequences.emplace_back(2, 3);
    g_sequences.emplace_back(3, 4);
    
    for(;;) {
        std::cout << g_sequences.at(0).next() << '\t'
                  << g_sequences.at(1).next() << '\t'
                  << g_sequences.at(2).next() << std::endl;
        std::cin.get();
    }
    
    return 0;
}
